//
// RT64
//

#pragma once

#include "rt64_gbi.h"

// gspSprite2D opcodes. The ucode shares the F3D opcode space: 0xBE is G_CULLDL and 0xBD is G_POPMTX in F3D.
#define S2D_G_SPRITE2D_BASE 0x09
#define S2D_G_SPRITE2D_SCALEFLIP 0xBE
#define S2D_G_SPRITE2D_DRAW 0xBD

namespace RT64 {
    // HLE command map for gspSprite2D (the "S2D" family: S2D, S2D.fifo and their hash variants).
    //
    // The ucode draws textured rectangles from uSprite structures: G_SPRITE2D_BASE loads the uSprite,
    // G_SPRITE2D_SCALEFLIP sets a scale and two flip flags that persist, and G_SPRITE2D_DRAW places one copy
    // at a screen position. It has no vertex, matrix, lighting or fog support: the F3D opcodes for those are
    // accepted and ignored, as the real ucode does. RDP commands pass through GBI_RDP's map.
    //
    // First cut (WipeOut 64 bring-up): CI4 and CI8 sprites with a TLUT, loaded through the lower half of TMEM
    // in row bands. Anything else logs a warning once per shape and is not drawn.
    namespace GBI_S2D {
        void sprite2DBase(State *state, DisplayList **dl);
        void sprite2DScaleFlip(State *state, DisplayList **dl);
        void sprite2DDraw(State *state, DisplayList **dl);
        void moveWord(State *state, DisplayList **dl);
        void ignore(State *state, DisplayList **dl);
        void reset(State *state);

        void setup(GBI *gbi);
    };
};
