//
// RT64
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "rt64_present_capture.h"

namespace RT64 {
    // The device Application::setup ended with, after any graphics API fallback, as the host's diagnostic recorder
    // reports it. Includes only the standard library, so the host reads it without RT64's other headers.
    struct RendererDeviceInfo {
        std::string name;
        uint32_t vendorId = 0;       // PCI vendor id (plume's RenderDeviceVendor), 0 when unknown
        uint64_t driverVersion = 0;  // as the graphics API reports it (D3D12: the UMD version's four 16-bit parts)
        std::string graphicsApi;     // "D3D12", "Vulkan" or "Metal"
    };

    // Any thread. Empty until Application::setup has finished.
    std::optional<RendererDeviceInfo> GetRendererDeviceInfo();

    // Called by Application::setup once the device is final.
    void SetRendererDeviceInfo(RendererDeviceInfo info);

    // One workload (one full sync), reported when its processing ends.
    struct ProbeWorkload {
        uint32_t gameCallCount = 0;
    };

    // Mirrors FramebufferPair::FlushReason without including RT64's hle headers.
    enum class ProbeFlushReason : uint8_t {
        None,
        SamplingFromColorImage,
        SamplingFromDepthImage,
        ColorImageChanged,
        DepthImageChanged,
        ProcessDisplayListsEnd
    };

    // Quarter pixels, as FixedRect.
    struct ProbeRect {
        int32_t ulx = 0;
        int32_t uly = 0;
        int32_t lrx = 0;
        int32_t lry = 0;
    };

    // One framebuffer pair of a workload at the end of its full sync, the placeholder pair included.
    struct ProbePair {
        uint32_t colorAddress = 0;
        // False when drawColorRect is null or empty (nothing drawn to colour): then the rect and height are unset.
        bool hasDrawColorRect = false;
        ProbeRect drawColorRect;
        int32_t colorHeight = 0;     // drawColorRect.bottom(true): the pixel rows RT64 sizes the framebuffer with
        ProbeFlushReason flushReason = ProbeFlushReason::None;
    };

    // The renderer probe's hooks, switched on and off as one set: the host points the probe at a whole table, so RT64
    // never sees half a set, and a later hook is a new member rather than a new setter. A null member is not called.
    // Every member is called on the thread that processes display lists and updates the screen.
    struct RendererProbeHooks {
        // State::fullSync, just before the workload advances. Draw calls are counted per workload, not per task:
        // a dummy workload has a full sync without a task, and a task without a full sync continues the workload.
        void (*workloadProcessed)(const ProbeWorkload &workload) = nullptr;

        // Application::processDisplayLists, after the interpreter processed the task (not while the inspector pauses).
        void (*taskProcessed)() = nullptr;

        // State::updateScreen, for each present queued, beside the present queued hook (rt64_present_capture.h).
        void (*presentQueued)(uint64_t presentId, uint32_t fbAddress) = nullptr;

        // State::fullSync, once per framebuffer pair in order, just before workloadProcessed for the same workload.
        void (*pairProcessed)(const ProbePair &pair) = nullptr;
    };

    // Any thread, at any time. Null (the default) disconnects: then each call site costs one atomic load. The table
    // must stay valid for the rest of the process, and a call that loaded the previous table may still run after
    // the store returns.
    void SetRendererProbeHooks(const RendererProbeHooks *hooks);
    const RendererProbeHooks *GetRendererProbeHooks();

    // The host's frame ring: a second capture of each drawn present (not interpolated frames, not skipped presents),
    // independent of the present capture hooks the shot recorder owns; both can capture the same present. While set,
    // the present queue draws the VI renderer's output once more into a width x height target (the GPU scales it),
    // reads it back, and calls `captured` on the present thread after the present's command list ran and before the
    // workload thread is let go, so the callback must be short (a copy). The image's size is the one recorded.
    struct RingCaptureListener {
        uint32_t width = 0;
        uint32_t height = 0;
        PresentCaptureHook *captured = nullptr;
    };

    // Any thread, at any time, with the same lifetime rule as the probe hooks. Null (the default): no extra draw and
    // no readback, one atomic load per drawn present.
    void SetRingCaptureListener(const RingCaptureListener *listener);
    const RingCaptureListener *GetRingCaptureListener();
};
