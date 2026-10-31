//
// RT64
//

#include "rt64_gbi_l3dex.h"

#include "rt64_gbi_f3dex.h"
#include "rt64_gbi_l3dex_line.h"

namespace RT64 {
    namespace GBI_L3DEX {
        void line3D(State *state, DisplayList **dl) {
            const L3DEXLine::Line3D line = L3DEXLine::decodeLine3D((*dl)->w1);
            state->rsp->drawLine(line.v0, line.v1, line.wd);
        }

        // The F3DEX map with G_LINE3D in place of G_QUAD. The rest of F3DEX's map stays as it is: WipeOut 64's
        // captured L3DEX lists draw with G_LINE3D only.
        void setup(GBI *gbi) {
            GBI_F3DEX::setup(gbi);

            gbi->map[L3DEX_G_LINE3D] = &line3D;
        }
    }
};
