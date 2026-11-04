//
// RT64
//

#pragma once

// Dependency-free so it can be unit-tested on its own.

namespace RT64 {
    // Draws with no viewport (raw triangles) keep the cut measured with a test ROM against an LLE implementation.
    constexpr float RawTriangleRasterDepthCut = 1022.0f / 1024.0f;

    // The screen depth of the ucode's z = w clip for a viewport, where RasterPS must cut a draw so that it ends at the
    // same place. It's 1022/1024 for a depth scale and offset of 511, and 1023/1024 for a scale of 1023 and an offset of 0.
    inline float viewportRasterDepthCut(float depthScale, float depthOffset) {
        const float depthAtFarPlane = depthScale + depthOffset;
        return (depthAtFarPlane < 1.0f) ? depthAtFarPlane : 1.0f;
    }
};
