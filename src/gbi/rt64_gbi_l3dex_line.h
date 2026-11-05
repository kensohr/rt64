//
// RT64
//

#pragma once

#include <cstdint>

// Line math of the L3DEX module (GBI_L3DEX): decoding G_LINE3D, clipping against the near, far, x and y planes,
// the colour and depth of the clipped ends, and the screen-space thin quad a line is drawn as. Pure and
// dependency-free (no RT64 headers) so it can be unit-tested on its own; GBI_L3DEX and RSP::drawLine do the RDRAM
// and renderer side.

namespace RT64 {
    namespace L3DEXLine {
        struct Line3D {
            uint8_t v0; // Vertex buffer indices of the two endpoints.
            uint8_t v1;
            int8_t wd; // Width, 0 for the thinnest line.
        };

        // Decodes the second word of an F3DEX-family G_LINE3D: flag 31-24, v0 * 2 23-16, v1 * 2 15-8, wd 7-0.
        Line3D decodeLine3D(uint32_t w1);

        // A transformed vertex, in clip space (row vector times modelview times projection).
        struct ClipPoint {
            float x, y, z, w;
        };

        // The part of a line left after clipping, as parameters along v0 -> v1.
        struct LineClip {
            bool visible;
            float t0;
            float t1;
        };

        // Clips a line against the near plane z = -w (guPerspective's convention). A vertex behind the
        // camera projects mirrored, so a line crossing the camera plane must be cut before it is drawn.
        LineClip clipToNearPlane(const ClipPoint &p0, const ClipPoint &p1);

        // Clips a line against the far plane z = w, as L3DEX 1.21 does: a point with z >= w is beyond it.
        LineClip clipToFarPlane(const ClipPoint &p0, const ClipPoint &p1);

        // The part of a line between the near and far planes.
        LineClip clipToDepthPlanes(const ClipPoint &p0, const ClipPoint &p1);

        // The part of a line inside all six planes, as L3DEX 1.21 clips it: the near and far planes, and |x| < w and
        // |y| < w, which put the cut on the current viewport's edges. A point on an x or y plane is outside it.
        LineClip clipToAllPlanes(const ClipPoint &p0, const ClipPoint &p1);

        // The point at parameter t along p0 -> p1.
        ClipPoint lerp(const ClipPoint &p0, const ClipPoint &p1, float t);

        // A position in N64 screen space: pixels, and depth as RT64's viewport maps it.
        struct ScreenPoint {
            float x, y, z;
        };

        struct Viewport {
            float scale[3];
            float translate[3];
        };

        // Projects a clip-space point the way RT64's RSP computes posScreen (y flipped, as the RSP does).
        ScreenPoint toScreen(const ClipPoint &p, const Viewport &viewport);

        // Keeps a line's screen depth below the depth where RasterPS discards raw triangles (1022/1024,
        // RawTriangleRasterFarCut), which is what a line is drawn as. That cut sits at z = w only for a viewport with
        // depth scale and offset 511. WipeOut 64 uses scale 1023 and offset 0, so z = w lands on 1023/1024 and the cut
        // would remove lines the L3DEX RSP still draws: from about 7,400 units instead of 9,600 in 4P, 10,900 instead
        // of 16,400 in 1P. Draws with a viewport cut at their own z = w, so a line flattened here can draw over geometry
        // past 1022/1024 that is really nearer.
        float clampToRasterDepth(float screenDepth);

        struct LineColor {
            float r, g, b, a;
        };

        // The colour at parameter t along a line, for an endpoint moved by a clip.
        LineColor lerpColor(const LineColor &c0, const LineColor &c1, float t);

        constexpr uint32_t ThinQuadVertexCount = 6;

        // Which line endpoint (0 or 1) each thin-quad vertex sits beside, for the caller's colours.
        constexpr uint8_t ThinQuadVertexEndpoint[ThinQuadVertexCount] = { 0, 0, 1, 1, 0, 1 };

        // Two triangles as RDP::drawTris takes them: x, y, z, w per vertex, w = 1.
        struct ThinQuad {
            uint32_t triangleCount; // 2, or 0 for a zero-length line.
            float positions[ThinQuadVertexCount][4];
        };

        // Expands a line into a quad offset by the perpendicular half-width on both sides. Width 0 is one
        // N64 pixel in total, as the RDP draws it.
        ThinQuad expandThinQuad(const ScreenPoint &p0, const ScreenPoint &p1, int8_t wd);
    };
};
