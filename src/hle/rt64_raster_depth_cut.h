//
// RT64
//

#pragma once

// Dependency-free so it can be unit-tested on its own.

#include <limits>
#include <optional>

namespace RT64 {
    // Draws with no viewport (raw triangles) keep the cut measured with a test ROM against an LLE implementation.
    constexpr float RawTriangleRasterFarCut = 1022.0f / 1024.0f;

    // Below any depth a draw can reach, for draws that RasterPS keeps however near they are.
    constexpr float NoRasterNearCut = -std::numeric_limits<float>::max();

    // The screen depth of the ucode's z = w clip for a viewport, where RasterPS must cut a draw so that it ends at the
    // same place. It's 1022/1024 for a depth scale and offset of 511, and 1023/1024 for a scale of 1023 and an offset of 0.
    inline float viewportRasterFarCut(float depthScale, float depthOffset) {
        const float depthAtFarPlane = depthScale + depthOffset;
        return (depthAtFarPlane < 1.0f) ? depthAtFarPlane : 1.0f;
    }

    // The screen depth of the ucode's z = -w clip, when it lies below depth 0. The ucode draws what is between that clip
    // and depth 0, with its vertices clamped to depth 0, so RasterPS must keep those pixels. It's -1023/1024 for a depth
    // scale of 1023 and an offset of 0. A depth scale and offset of 511 put the clip on depth 0, so there's no such band.
    inline std::optional<float> viewportRasterNearCut(float depthScale, float depthOffset) {
        const float depthAtNearPlane = depthOffset - depthScale;
        if (depthAtNearPlane >= 0.0f) {
            return std::nullopt;
        }

        return depthAtNearPlane;
    }
};
