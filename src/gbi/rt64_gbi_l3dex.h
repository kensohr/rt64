//
// RT64
//

#pragma once

#include "rt64_gbi.h"

// L3DEX 1.x shares F3DEX's opcode numbering and puts G_LINE3D where F3DEX has G_QUAD.
#define L3DEX_G_LINE3D 0xB5

namespace RT64 {
    namespace GBI_L3DEX {
        void line3D(State *state, DisplayList **dl);

        void setup(GBI *gbi);
    };
};
