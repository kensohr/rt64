//
// RT64
//

#pragma once

#include "rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RDPParams {
        float4 primColor;
        float2 primLOD;
        float2 primDepth;
        float4 envColor;
        float4 fogColor;
        float4 blendColor;
        float3 keyCenter;
        float3 keyScale;
        int convertK[6];
        float rasterMaxDepth; // Per-draw depth above which RasterPS discards. Filled in with the call's projection.
        float rasterMinDepth; // Per-draw depth below which RasterPS discards, for draws it clamps to depth 0 instead of cutting at it.
    };
#ifdef HLSL_CPU
};
#endif