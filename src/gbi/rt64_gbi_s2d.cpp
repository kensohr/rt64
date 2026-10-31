//
// RT64
//

#include "rt64_gbi_s2d.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

#include "rt64_f3d.h"
#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_rdp.h"

namespace RT64 {
    namespace GBI_S2D {
        // uSprite (SDK gbi.h), 24 bytes, 8-byte aligned, read as five 32-bit words:
        //   0: SourceImagePointer   1: TlutPointer
        //   2: Stride << 16 | SubImageWidth
        //   3: SubImageHeight << 16 | SourceImageType << 8 | SourceImageBitSize
        //   4: SourceImageOffsetS << 16 | SourceImageOffsetT
        constexpr uint32_t USPRITE_WORDS = 5;

        // Scale is 5.10 fixed point, texels per screen pixel: 1024 draws the sub-image at 1:1.
        constexpr int32_t SCALE_FRACTION_BITS = 10;
        constexpr uint16_t SCALE_ONE = 1 << SCALE_FRACTION_BITS;

        // The TLUT lives in the upper half of TMEM (word 256 onwards), so texels get the lower 2 KiB.
        constexpr uint16_t TLUT_TMEM_WORD = 256;
        constexpr uint32_t TEXEL_TMEM_BYTES = 2048;
        constexpr uint32_t TMEM_WORD_BYTES = 8;

        constexpr uint32_t CI4_TLUT_ENTRIES = 16;
        constexpr uint32_t CI8_TLUT_ENTRIES = 256;

        // Coordinates: screen positions in 10.2, tile bounds in 10.2, texrect s/t in s10.5.
        constexpr int32_t PIXEL_FIXED_SHIFT = 2;
        constexpr int32_t TEXEL_FIXED_SHIFT = 5;
        constexpr int32_t MAX_TEXEL_COORDINATE = 1023;

        // Copy mode steps four texels per dsdx unit, so the s5.10 step holds a quarter of the scale range.
        constexpr uint16_t MAX_COPY_MODE_SCALE = 8191;

        // Shapes already reported, so a sprite drawn every frame warns once.
        static std::unordered_set<std::string> warnedShapes;

        static void warnOnce(const RSP::Sprite2D &sprite, const char *reason) {
            char shape[160];
            snprintf(shape, sizeof(shape), "%s: fmt %u siz %u sub-image %dx%d stride %d tlut %s scale %u/%u",
                reason, sprite.fmt, sprite.siz, sprite.imageW, sprite.imageH, sprite.stride,
                (sprite.tlutAddress != 0) ? "yes" : "no", sprite.scaleX, sprite.scaleY);

            if (warnedShapes.insert(shape).second) {
                fprintf(stderr, "S2D warning: sprite outside the implemented envelope, not drawn (%s)\n", shape);
            }
        }

        // A 4-bit load starts on an even texel because TMEM holds whole bytes.
        static int32_t loadOriginS(const RSP::Sprite2D &sprite) {
            return (sprite.siz == G_IM_SIZ_4b) ? (sprite.imageX & ~1) : sprite.imageX;
        }

        // Texels per loaded row: the sub-image plus the odd 4-bit texel in front of it, if any.
        static uint32_t loadedRowTexels(const RSP::Sprite2D &sprite) {
            return uint32_t(sprite.imageX - loadOriginS(sprite)) + uint32_t(sprite.imageW);
        }

        static uint32_t bytesPerRow(const RSP::Sprite2D &sprite) {
            // Round a 4-bit row with an odd texel count up to whole bytes.
            return ((loadedRowTexels(sprite) << sprite.siz) + 1) >> 1;
        }

        static uint32_t tmemWordsPerRow(const RSP::Sprite2D &sprite) {
            return (bytesPerRow(sprite) + TMEM_WORD_BYTES - 1) / TMEM_WORD_BYTES;
        }

        // The first cut covers what the WipeOut 64 census saw: CI4/CI8 with a TLUT, rows no wider than TMEM,
        // texel coordinates and copy-mode steps inside the texrect's fixed-point range.
        static bool isWithinEnvelope(const RSP::Sprite2D &sprite, bool copyMode) {
            if (sprite.fmt != G_IM_FMT_CI || (sprite.siz != G_IM_SIZ_4b && sprite.siz != G_IM_SIZ_8b)) {
                warnOnce(sprite, "texel format other than CI4/CI8");
                return false;
            }

            if (sprite.tlutAddress == 0) {
                warnOnce(sprite, "CI sprite without a TLUT");
                return false;
            }

            if (sprite.scaleX == 0 || sprite.scaleY == 0) {
                warnOnce(sprite, "zero scale");
                return false;
            }

            if (tmemWordsPerRow(sprite) * TMEM_WORD_BYTES > TEXEL_TMEM_BYTES) {
                warnOnce(sprite, "sub-image row wider than TMEM");
                return false;
            }

            if (sprite.imageX + sprite.imageW > MAX_TEXEL_COORDINATE || sprite.imageY + sprite.imageH > MAX_TEXEL_COORDINATE) {
                warnOnce(sprite, "sub-image beyond texel coordinate 1023");
                return false;
            }

            if (copyMode && (sprite.scaleX > MAX_COPY_MODE_SCALE || sprite.scaleY > MAX_COPY_MODE_SCALE)) {
                warnOnce(sprite, "copy-mode scale above 8191");
                return false;
            }

            return true;
        }

        static void loadTLUT(State *state, const RSP::Sprite2D &sprite) {
            const uint32_t entries = (sprite.siz == G_IM_SIZ_4b) ? CI4_TLUT_ENTRIES : CI8_TLUT_ENTRIES;
            RDP *rdp = state->rdp.get();
            rdp->setTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, state->rsp->fromSegmented(sprite.tlutAddress));
            rdp->setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, TLUT_TMEM_WORD, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            rdp->loadTLUT(G_TX_LOADTILE, 0, 0, (entries - 1) << PIXEL_FIXED_SHIFT, 0);

            // The ucode owns the two TLUT mode bits: the game's own other-mode commands never set them.
            constexpr uint32_t TEXTLUT_BITS = 2;
            state->rsp->setOtherModeH(TEXTLUT_BITS, G_MDSFT_TEXTLUT, G_TT_RGBA16);
        }

        // Loads rows [firstRow, firstRow + rowCount) of the sub-image into the lower half of TMEM and describes
        // them with the render tile. The render tile's origin is the load origin, which keeps the texrect's s
        // coordinate equal to the sub-image offset even when a 4-bit load starts one texel early.
        static void loadSubImageRows(State *state, const RSP::Sprite2D &sprite, int32_t firstRow, int32_t rowCount) {
            const uint16_t uls = uint16_t(loadOriginS(sprite) << PIXEL_FIXED_SHIFT);
            const uint16_t ult = uint16_t((sprite.imageY + firstRow) << PIXEL_FIXED_SHIFT);
            const uint16_t lrs = uint16_t((sprite.imageX + sprite.imageW - 1) << PIXEL_FIXED_SHIFT);
            const uint16_t lrt = uint16_t((sprite.imageY + firstRow + rowCount - 1) << PIXEL_FIXED_SHIFT);
            const uint16_t line = uint16_t(tmemWordsPerRow(sprite));
            RDP *rdp = state->rdp.get();
            rdp->setTextureImage(sprite.fmt, sprite.siz, uint16_t(sprite.stride), state->rsp->fromSegmented(sprite.imageAddress));
            rdp->setTile(G_TX_LOADTILE, sprite.fmt, sprite.siz, line, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            rdp->loadTile(G_TX_LOADTILE, uls, ult, lrs, lrt);
            rdp->setTile(G_TX_RENDERTILE, sprite.fmt, sprite.siz, line, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            rdp->setTileSize(G_TX_RENDERTILE, uls, ult, lrs, lrt);
        }

        // Screen extent in 10.2 pixels of a run of texels at the sprite's scale.
        static int32_t scaledExtent(int32_t texels, uint16_t scale) {
            return (texels << (PIXEL_FIXED_SHIFT + SCALE_FRACTION_BITS)) / scale;
        }

        // One texrect for rows [firstRow, firstRow + rowCount) of the sub-image, the rows TMEM holds. A flip
        // samples the texture from its far edge with a negative step, which keeps the rectangle itself
        // well-formed for the RDP emulation. With a vertical flip the rows are stacked from the bottom up.
        static void drawSubImageRows(State *state, const RSP::Sprite2D &sprite, int32_t frameX, int32_t frameY, int32_t firstRow, int32_t rowCount) {
            const int32_t frameW = scaledExtent(sprite.imageW, sprite.scaleX);
            const int32_t frameH = scaledExtent(sprite.imageH, sprite.scaleY);
            const int32_t rowsTop = scaledExtent(firstRow, sprite.scaleY);
            const int32_t rowsBottom = scaledExtent(firstRow + rowCount, sprite.scaleY);
            const int32_t chunkY = sprite.flipY ? (frameY + frameH - rowsBottom) : (frameY + rowsTop);
            const int32_t chunkH = rowsBottom - rowsTop;
            const int32_t texelS = sprite.flipX ? (sprite.imageX + sprite.imageW) : sprite.imageX;
            const int32_t texelT = sprite.imageY + (sprite.flipY ? (firstRow + rowCount) : firstRow);
            int16_t dsdx = int16_t(sprite.flipX ? -int32_t(sprite.scaleX) : int32_t(sprite.scaleX));
            const int16_t dtdy = int16_t(sprite.flipY ? -int32_t(sprite.scaleY) : int32_t(sprite.scaleY));

            // In copy mode the RDP steps four texels per dsdx unit, and RDP::drawTexRect undoes that.
            if (state->rdp->otherMode.cycleType() == G_CYC_COPY) {
                dsdx = int16_t(dsdx * 4);
            }

            state->rdp->drawTexRect(frameX, chunkY, frameX + frameW, chunkY + chunkH, G_TX_RENDERTILE,
                int16_t(texelS << TEXEL_FIXED_SHIFT), int16_t(texelT << TEXEL_FIXED_SHIFT), dsdx, dtdy, false);
        }

        // A sub-image taller than the lower half of TMEM goes through it in row bands, each one load and one
        // texrect, as the real ucode tiles it.
        static void drawSubImage(State *state, const RSP::Sprite2D &sprite, int32_t frameX, int32_t frameY) {
            const int32_t rowsPerLoad = int32_t(TEXEL_TMEM_BYTES / (tmemWordsPerRow(sprite) * TMEM_WORD_BYTES));
            for (int32_t firstRow = 0; firstRow < sprite.imageH; firstRow += rowsPerLoad) {
                const int32_t rowCount = std::min(rowsPerLoad, sprite.imageH - firstRow);
                loadSubImageRows(state, sprite, firstRow, rowCount);
                drawSubImageRows(state, sprite, frameX, frameY, firstRow, rowCount);
            }
        }

        void sprite2DBase(State *state, DisplayList **dl) {
            // The RSP DMAs the structure, so the address is masked as the DMA hardware would.
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1) & RSP_DMA_MASK;
            uint32_t words[USPRITE_WORDS];
            memcpy(words, state->fromRDRAM(rdramAddress), sizeof(words));

            RSP::Sprite2D &sprite = state->rsp->sprite2D;
            sprite.imageAddress = words[0];
            sprite.tlutAddress = words[1];
            sprite.stride = int16_t(words[2] >> 16);
            sprite.imageW = int16_t(words[2] & 0xFFFF);
            sprite.imageH = int16_t(words[3] >> 16);
            sprite.fmt = uint8_t((words[3] >> 8) & 0xFF);
            sprite.siz = uint8_t(words[3] & 0xFF);
            sprite.imageX = int16_t(words[4] >> 16);
            sprite.imageY = int16_t(words[4] & 0xFFFF);
        }

        void sprite2DScaleFlip(State *state, DisplayList **dl) {
            RSP::Sprite2D &sprite = state->rsp->sprite2D;
            sprite.scaleX = uint16_t((*dl)->p1(16, 16));
            sprite.scaleY = uint16_t((*dl)->p1(0, 16));
            sprite.flipX = ((*dl)->p0(8, 8) != 0);
            sprite.flipY = ((*dl)->p0(0, 8) != 0);
        }

        void sprite2DDraw(State *state, DisplayList **dl) {
            const RSP::Sprite2D &sprite = state->rsp->sprite2D;

            // Glyphs clipped off the left edge arrive with a zero or negative width: nothing to draw.
            if (sprite.imageW <= 0 || sprite.imageH <= 0) {
                return;
            }

            const bool copyMode = (state->rdp->otherMode.cycleType() == G_CYC_COPY);
            if (!isWithinEnvelope(sprite, copyMode)) {
                return;
            }

            const int32_t frameX = int16_t((*dl)->p1(16, 16));
            const int32_t frameY = int16_t((*dl)->p1(0, 16));
            loadTLUT(state, sprite);
            drawSubImage(state, sprite, frameX, frameY);
        }

        void moveWord(State *state, DisplayList **dl) {
            // The ucode keeps the segment table. The other move-word types address vertex, light, fog and
            // matrix state it has none of.
            if ((*dl)->p0(0, 8) == G_MW_SEGMENT) {
                state->rsp->setSegment((*dl)->p0(10, 4), (*dl)->w1);
            }
        }

        void ignore(State *state, DisplayList **dl) {
            // Fast3D commands the game's shared frame set-up emits; gspSprite2D does nothing with them.
        }

        void reset(State *state) {
            state->rsp->sprite2D = {};
            state->rsp->sprite2D.scaleX = SCALE_ONE;
            state->rsp->sprite2D.scaleY = SCALE_ONE;
        }

        void setup(GBI *gbi) {
            gbi->constants = {
                { F3DENUM::G_MTX_MODELVIEW, 0x00 },
                { F3DENUM::G_MTX_PROJECTION, 0x01 },
                { F3DENUM::G_MTX_MUL, 0x00 },
                { F3DENUM::G_MTX_LOAD, 0x02 },
                { F3DENUM::G_MTX_NOPUSH, 0x00 },
                { F3DENUM::G_MTX_PUSH, 0x04 },
                { F3DENUM::G_TEXTURE_ENABLE, 0x00000002 },
                { F3DENUM::G_SHADING_SMOOTH, 0x00000200 },
                { F3DENUM::G_CULL_FRONT, 0x00001000 },
                { F3DENUM::G_CULL_BACK, 0x00002000 },
                { F3DENUM::G_CULL_BOTH, 0x00003000 }
            };

            gbi->map[F3D_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[S2D_G_SPRITE2D_BASE] = &sprite2DBase;
            gbi->map[S2D_G_SPRITE2D_SCALEFLIP] = &sprite2DScaleFlip;
            gbi->map[S2D_G_SPRITE2D_DRAW] = &sprite2DDraw;
            gbi->map[F3D_G_MTX] = &ignore;
            gbi->map[F3D_G_MOVEMEM] = &ignore;
            gbi->map[F3D_G_VTX] = &ignore;
            gbi->map[F3D_G_TRI1] = &ignore;
            gbi->map[F3D_G_QUAD] = &ignore;
            gbi->map[F3D_G_TEXTURE] = &ignore;
            gbi->map[F3D_G_SETGEOMETRYMODE] = &ignore;
            gbi->map[F3D_G_CLEARGEOMETRYMODE] = &ignore;
            gbi->map[F3D_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3D_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[F3D_G_MOVEWORD] = &moveWord;
            gbi->map[F3D_G_SETOTHERMODE_H] = &GBI_F3D::setOtherModeH;
            gbi->map[F3D_G_SETOTHERMODE_L] = &GBI_F3D::setOtherModeL;
            gbi->map[F3D_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3D_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;
            gbi->map[G_RDPNOOP] = &GBI_RDP::noOp;

            gbi->resetFromTask = &reset;
        }
    }
};
