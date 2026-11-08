//
// RT64
//

#pragma once

// Dependency-free so it can be unit-tested on its own.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace RT64 {
    struct TriangleScreenVertex {
        // N64 screen position, as RSP's posScreen holds it.
        float x;
        float y;

        // Clip-space w, as RSP's posTransformed holds it.
        float w;
    };

    // In quarter pixels, like FixedRect.
    struct TriangleScreenBounds {
        int32_t ulx;
        int32_t uly;
        int32_t lrx;
        int32_t lry;
    };

    constexpr float TriangleScreenBoundsQuarterPixels = 4.0f;

    // Far beyond any framebuffer, and well inside what int32 quarter pixels hold.
    constexpr float TriangleScreenBoundsLimitPixels = 1048576.0f;

    // The screen rect a triangle's vertices span, which the caller merges into the framebuffer pair's draw rect. None
    // when a vertex is at or behind the camera (w <= 0): its screen position is projected through a w of the wrong sign
    // (or a stand-in for 0), so it lands mirrored and the span says nothing about where the triangle is drawn.
    inline std::optional<TriangleScreenBounds> triangleScreenBounds(const TriangleScreenVertex (&vertices)[3]) {
        for (const TriangleScreenVertex &vertex : vertices) {
            // Written so that a NaN w is rejected too.
            if (!(vertex.w > 0.0f)) {
                return std::nullopt;
            }
        }

        // A vertex just in front of the camera can land millions of pixels away. Clamped first, so the conversion to
        // quarter pixels can't overflow; the caller intersects the result with the viewport anyway.
        const auto clampPixels = [](float pixels) {
            return std::clamp(pixels, -TriangleScreenBoundsLimitPixels, TriangleScreenBoundsLimitPixels);
        };

        TriangleScreenBounds bounds = { INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN };
        for (const TriangleScreenVertex &vertex : vertices) {
            const float x = clampPixels(vertex.x);
            const float y = clampPixels(vertex.y);
            bounds.ulx = std::min(bounds.ulx, int32_t(x * TriangleScreenBoundsQuarterPixels));
            bounds.uly = std::min(bounds.uly, int32_t(y * TriangleScreenBoundsQuarterPixels));
            bounds.lrx = std::max(bounds.lrx, int32_t(std::ceil(x) * TriangleScreenBoundsQuarterPixels));
            bounds.lry = std::max(bounds.lry, int32_t(std::ceil(y) * TriangleScreenBoundsQuarterPixels));
        }

        return bounds;
    }
};
