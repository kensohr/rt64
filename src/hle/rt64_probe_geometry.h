//
// RT64
//

#pragma once

// Dependency-free so it can be unit-tested on its own.
//
// The renderer probe's measure of one perspective triangle, for the host's suspect-geometry flags: what the cartridge
// draws of it, worked out in clip space from RSP's posTransformed alone. It repeats none of RT64's screen positions or
// draw rect code, so it can disagree with them: a draw rect that stops short of what a triangle covers is the defect it
// is there to catch (wipeout64-bringup/48, where vertices behind the camera cut the rect short).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace RT64 {
    // Clip space, as RSP's posTransformed holds it.
    struct ProbeClipVertex {
        float x;
        float y;
        float z;
        float w;
    };

    // In pixels, as RSP's viewport maps clip space to the screen: x / w * scaleX + translateX, and y / -w * scaleY +
    // translateY (the N64's y axis points down).
    struct ProbeViewport {
        float scaleX;
        float scaleY;
        float translateX;
        float translateY;
    };

    // In quarter pixels, like FixedRect.
    struct ProbeScissor {
        int32_t ulx;
        int32_t uly;
        int32_t lrx;
        int32_t lry;
    };

    // The view volume's w >= ProbeMinClipW stands in for w > 0: it keeps vertices at the camera out of the projection.
    // Same value as 48's detector.
    constexpr float ProbeMinClipW = 1e-4f;

    // A triangle covering less than this many pixels draws nothing worth counting. Same value as 48's detector.
    constexpr float ProbeMinAreaPixels = 1.0f;

    constexpr float ProbeQuarterPixels = 4.0f;

    struct ProbeTriangleGeometry {
        // The part inside the cartridge's view volume (|x| <= w, |y| <= w, -w <= z <= w, w >= ProbeMinClipW) covers at
        // least ProbeMinAreaPixels and survives the cartridge's back-face cull.
        bool counts = false;

        // A vertex at or behind the camera (w <= 0). Only set when the triangle counts.
        bool behindCamera = false;

        // A vertex beyond the near plane (z < -w). Only set when the triangle counts.
        bool nearClip = false;

        // The lowest edge, in quarter pixels rounded down, of the counted part that is also inside the scissor. Unset
        // when that part covers less than ProbeMinAreaPixels.
        bool hasReach = false;
        int32_t reachQuarterPixels = 0;

        // Over the three vertices, whether or not the triangle counts.
        float maxAbsW = 0.0f;   // largest finite |w|
        bool nonFiniteW = false;
    };

    namespace probe_geometry_detail {
        // Each plane can add one vertex: 3 + 7 view volume planes + 4 scissor sides.
        constexpr int MaxPolygonVertices = 16;

        struct Polygon {
            int count = 0;
            ProbeClipVertex v[MaxPolygonVertices];
        };

        // Keeps the half space a * x + b * y + c * z + d * w + e >= 0 (Sutherland-Hodgman).
        struct HalfSpace {
            float a;
            float b;
            float c;
            float d;
            float e;
        };

        inline float distance(const HalfSpace &h, const ProbeClipVertex &p) {
            return h.a * p.x + h.b * p.y + h.c * p.z + h.d * p.w + h.e;
        }

        inline Polygon clip(const Polygon &in, const HalfSpace &h) {
            Polygon out;
            for (int i = 0; i < in.count; i++) {
                const ProbeClipVertex &p0 = in.v[i];
                const ProbeClipVertex &p1 = in.v[(i + 1) % in.count];
                const float d0 = distance(h, p0);
                const float d1 = distance(h, p1);
                if (d0 >= 0.0f) {
                    out.v[out.count++] = p0;
                }

                if ((d0 >= 0.0f) != (d1 >= 0.0f)) {
                    const float t = d0 / (d0 - d1);
                    out.v[out.count++] = { p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t, p0.z + (p1.z - p0.z) * t, p0.w + (p1.w - p0.w) * t };
                }
            }

            return out;
        }

        inline Polygon clipToViewVolume(Polygon p) {
            p = clip(p, { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f });
            p = clip(p, { -1.0f, 0.0f, 0.0f, 1.0f, 0.0f });
            p = clip(p, { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f });
            p = clip(p, { 0.0f, -1.0f, 0.0f, 1.0f, 0.0f });
            p = clip(p, { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f });
            p = clip(p, { 0.0f, 0.0f, -1.0f, 1.0f, 0.0f });
            return clip(p, { 0.0f, 0.0f, 0.0f, 1.0f, -ProbeMinClipW });
        }

        // The rect's sides as half spaces in clip space, from the viewport mapping: x / w * scaleX + translateX >= ulx
        // is scaleX * x + (translateX - ulx) * w >= 0 for w > 0, and likewise for the other sides.
        inline Polygon clipToRect(Polygon p, const ProbeViewport &vp, float ulx, float uly, float lrx, float lry) {
            p = clip(p, { vp.scaleX, 0.0f, 0.0f, vp.translateX - ulx, 0.0f });
            p = clip(p, { -vp.scaleX, 0.0f, 0.0f, lrx - vp.translateX, 0.0f });
            p = clip(p, { 0.0f, -vp.scaleY, 0.0f, vp.translateY - uly, 0.0f });
            return clip(p, { 0.0f, vp.scaleY, 0.0f, lry - vp.translateY, 0.0f });
        }

        inline float screenX(const ProbeClipVertex &p, const ProbeViewport &vp) {
            return p.x / p.w * vp.scaleX + vp.translateX;
        }

        inline float screenY(const ProbeClipVertex &p, const ProbeViewport &vp) {
            return p.y / -p.w * vp.scaleY + vp.translateY;
        }

        // In pixels, with the sign of RSP::drawIndexedTri's cull test: cross(V, U).z of the screen positions, U and V
        // the edges from the first vertex to the second and third. Positive passes the test.
        inline float signedArea(const Polygon &polygon, const ProbeViewport &vp) {
            float sum = 0.0f;
            for (int i = 1; (i + 1) < polygon.count; i++) {
                const float ux = screenX(polygon.v[i], vp) - screenX(polygon.v[0], vp);
                const float uy = screenY(polygon.v[i], vp) - screenY(polygon.v[0], vp);
                const float vx = screenX(polygon.v[i + 1], vp) - screenX(polygon.v[0], vp);
                const float vy = screenY(polygon.v[i + 1], vp) - screenY(polygon.v[0], vp);
                sum += vx * uy - vy * ux;
            }

            return sum * 0.5f;
        }

        inline bool isFinite(const ProbeClipVertex &p) {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::isfinite(p.w);
        }
    };

    // `culls`: the cartridge's back-face cull is on, with the vertices in the order RSP::drawIndexedTri tests them (it
    // swaps the first and third for front-face culling, so one test covers both).
    inline ProbeTriangleGeometry measureProbeTriangle(const ProbeClipVertex (&vertices)[3], const ProbeViewport &viewport, const ProbeScissor &scissor, bool culls) {
        using namespace probe_geometry_detail;
        ProbeTriangleGeometry geometry;
        bool allFinite = true;
        for (const ProbeClipVertex &vertex : vertices) {
            if (std::isfinite(vertex.w)) {
                geometry.maxAbsW = std::max(geometry.maxAbsW, std::fabs(vertex.w));
            }
            else {
                geometry.nonFiniteW = true;
            }

            allFinite = allFinite && isFinite(vertex);
        }

        if (!allFinite) {
            return geometry;
        }

        Polygon triangle;
        triangle.count = 3;
        for (int i = 0; i < 3; i++) {
            triangle.v[i] = vertices[i];
        }

        const Polygon inView = clipToViewVolume(triangle);
        const float area = signedArea(inView, viewport);
        const bool isCulled = culls && (area < 0.0f);
        if ((std::fabs(area) < ProbeMinAreaPixels) || isCulled) {
            return geometry;
        }

        geometry.counts = true;
        for (const ProbeClipVertex &vertex : vertices) {
            geometry.behindCamera = geometry.behindCamera || (vertex.w <= 0.0f);
            geometry.nearClip = geometry.nearClip || (vertex.z < -vertex.w);
        }

        const Polygon inScissor = clipToRect(inView, viewport, scissor.ulx / ProbeQuarterPixels, scissor.uly / ProbeQuarterPixels,
            scissor.lrx / ProbeQuarterPixels, scissor.lry / ProbeQuarterPixels);
        if (std::fabs(signedArea(inScissor, viewport)) < ProbeMinAreaPixels) {
            return geometry;
        }

        float bottomPixels = screenY(inScissor.v[0], viewport);
        for (int i = 1; i < inScissor.count; i++) {
            bottomPixels = std::max(bottomPixels, screenY(inScissor.v[i], viewport));
        }

        geometry.hasReach = true;
        geometry.reachQuarterPixels = int32_t(std::floor(bottomPixels * ProbeQuarterPixels));
        return geometry;
    }

    // How far down one framebuffer pair's counted triangles reach.
    struct ProbePairReach {
        bool hasReach = false;
        int32_t reachQuarterPixels = 0;

        // A triangle with a vertex behind the camera reached that far.
        bool behindCamera = false;
    };

    // The measures of one workload's triangles, summed per workload and per framebuffer pair. Only a workload that
    // began with the probe connected is measured, so a report never covers part of one.
    struct ProbeGeometryAccumulator {
        bool isMeasuring = false;
        uint32_t behindCameraTris = 0;
        uint32_t nearClipTris = 0;
        float maxAbsW = 0.0f;
        bool nonFiniteW = false;
        std::vector<ProbePairReach> pairs;

        void begin(bool isProbeConnected) {
            isMeasuring = isProbeConnected;
            behindCameraTris = 0;
            nearClipTris = 0;
            maxAbsW = 0.0f;
            nonFiniteW = false;
            pairs.clear();
        }

        void add(uint32_t pairIndex, const ProbeTriangleGeometry &triangle) {
            maxAbsW = std::max(maxAbsW, triangle.maxAbsW);
            nonFiniteW = nonFiniteW || triangle.nonFiniteW;
            if (!triangle.counts) {
                return;
            }

            behindCameraTris += triangle.behindCamera ? 1 : 0;
            nearClipTris += triangle.nearClip ? 1 : 0;
            if (!triangle.hasReach) {
                return;
            }

            if (pairIndex >= pairs.size()) {
                pairs.resize(pairIndex + 1);
            }

            ProbePairReach &pair = pairs[pairIndex];
            if (!pair.hasReach || (triangle.reachQuarterPixels > pair.reachQuarterPixels)) {
                pair = { true, triangle.reachQuarterPixels, triangle.behindCamera };
            }
            else if (triangle.reachQuarterPixels == pair.reachQuarterPixels) {
                pair.behindCamera = pair.behindCamera || triangle.behindCamera;
            }
        }

        ProbePairReach pairReach(uint32_t pairIndex) const {
            return (pairIndex < pairs.size()) ? pairs[pairIndex] : ProbePairReach();
        }
    };
};
