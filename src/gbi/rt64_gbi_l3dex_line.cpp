//
// RT64
//

#include "rt64_gbi_l3dex_line.h"

#include <cmath>

namespace RT64 {
    namespace L3DEXLine {
        Line3D decodeLine3D(uint32_t w1) {
            Line3D line;
            line.v0 = uint8_t((w1 >> 17) & 0x7F);
            line.v1 = uint8_t((w1 >> 9) & 0x7F);
            line.wd = int8_t(w1 & 0xFF);
            return line;
        }

        NearClip clipToNearPlane(const ClipPoint &p0, const ClipPoint &p1) {
            // Signed distance to the near plane, positive in front of it.
            const float d0 = p0.z + p0.w;
            const float d1 = p1.z + p1.w;
            const bool p0InFront = (d0 >= 0.0f);
            const bool p1InFront = (d1 >= 0.0f);
            if (!p0InFront && !p1InFront) {
                return { false, 0.0f, 0.0f };
            }

            if (p0InFront && p1InFront) {
                return { true, 0.0f, 1.0f };
            }

            const float tPlane = d0 / (d0 - d1);
            if (p0InFront) {
                return { true, 0.0f, tPlane };
            }
            else {
                return { true, tPlane, 1.0f };
            }
        }

        ClipPoint lerp(const ClipPoint &p0, const ClipPoint &p1, float t) {
            return {
                p0.x + (p1.x - p0.x) * t,
                p0.y + (p1.y - p0.y) * t,
                p0.z + (p1.z - p0.z) * t,
                p0.w + (p1.w - p0.w) * t
            };
        }

        ScreenPoint toScreen(const ClipPoint &p, const Viewport &viewport) {
            return {
                (p.x / p.w) * viewport.scale[0] + viewport.translate[0],
                (p.y / -p.w) * viewport.scale[1] + viewport.translate[1],
                (p.z / p.w) * viewport.scale[2] + viewport.translate[2]
            };
        }

        // Total line width in N64 pixels. Width 0 is one pixel, as the RDP draws the thinnest line; each width
        // step is taken as half a pixel (the GBI's unit), unverified because WipeOut 64 only ever uses 0.
        // Negative widths draw as the thinnest line.
        static float lineWidthPixels(int8_t wd) {
            const float ThinnestWidthPixels = 1.0f;
            const float PixelsPerWidthStep = 0.5f;
            const float width = ThinnestWidthPixels + PixelsPerWidthStep * float(wd);
            return (width < ThinnestWidthPixels) ? ThinnestWidthPixels : width;
        }

        static void setPosition(float (&position)[4], float x, float y, float z) {
            position[0] = x;
            position[1] = y;
            position[2] = z;
            position[3] = 1.0f;
        }

        ThinQuad expandThinQuad(const ScreenPoint &p0, const ScreenPoint &p1, int8_t wd) {
            ThinQuad quad = {};
            const float dx = p1.x - p0.x;
            const float dy = p1.y - p0.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 0.0f) {
                return quad;
            }

            // Perpendicular to the line, scaled to half the width.
            const float halfWidth = lineWidthPixels(wd) * 0.5f;
            const float offsetX = -dy / length * halfWidth;
            const float offsetY = dx / length * halfWidth;

            // Vertex order matches ThinQuadVertexEndpoint: (p0+, p0-, p1+) and (p1+, p0-, p1-).
            setPosition(quad.positions[0], p0.x + offsetX, p0.y + offsetY, p0.z);
            setPosition(quad.positions[1], p0.x - offsetX, p0.y - offsetY, p0.z);
            setPosition(quad.positions[2], p1.x + offsetX, p1.y + offsetY, p1.z);
            setPosition(quad.positions[3], p1.x + offsetX, p1.y + offsetY, p1.z);
            setPosition(quad.positions[4], p0.x - offsetX, p0.y - offsetY, p0.z);
            setPosition(quad.positions[5], p1.x - offsetX, p1.y - offsetY, p1.z);
            quad.triangleCount = 2;
            return quad;
        }
    };
};
