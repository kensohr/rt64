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

        // Over the workload's perspective triangles, measured in clip space (hle/rt64_probe_geometry.h). All zero for
        // a workload that began before the probe was connected.
        uint32_t behindCameraTris = 0;   // counted triangles with a vertex at or behind the camera
        uint32_t nearClipTris = 0;       // counted triangles with a vertex beyond the near plane
        float maxAbsW = 0.0f;            // largest finite |w| of any vertex
        bool nonFiniteW = false;         // some vertex had a NaN or infinite w
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
        uint8_t colorSiz = 0;        // G_IM_SIZ_* of the colour image
        uint16_t colorWidth = 0;     // the colour image's width in pixels
        // False when drawColorRect is null or empty (nothing drawn to colour): then the rect and height are unset.
        bool hasDrawColorRect = false;
        ProbeRect drawColorRect;
        int32_t colorHeight = 0;     // drawColorRect.bottom(true): the pixel rows RT64 sizes the framebuffer with
        ProbeFlushReason flushReason = ProbeFlushReason::None;

        // How far down the pair's counted perspective triangles reach within the viewport and scissor, measured in clip
        // space apart from drawColorRect (hle/rt64_probe_geometry.h). Unset when none reached into the scissor, or the
        // workload began before the probe was connected.
        bool hasReach = false;
        int32_t reachQuarterPixels = 0;
        bool reachBehindCamera = false;  // a triangle with a vertex at or behind the camera reached that far
    };

    // A perspective view-projection registered for geometry: the top of the cartridge's projection stack as loaded
    // (not the extended GBI's adjusted transform), with the RSP viewport in effect.
    struct ProbeProjection {
        float viewProj[4][4] = {};       // row-major, as RT64's float4x4 holds it
        float viewportScale[2] = {};     // x, y in pixels, as RSPViewport holds them
        float viewportTranslate[2] = {};
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

        // RSP::addCurrentProjection, each time it registers a new view-projection or viewport of perspective type
        // for the vertices that follow, while a task's display lists are processed (so before its taskProcessed).
        void (*perspectiveProjectionRegistered)(const ProbeProjection &projection) = nullptr;
    };

    // Any thread, at any time. Null (the default) disconnects: then each call site costs one atomic load. The table
    // must stay valid for the rest of the process, and a call that loaded the previous table may still run after
    // the store returns.
    void SetRendererProbeHooks(const RendererProbeHooks *hooks);
    const RendererProbeHooks *GetRendererProbeHooks();

    // The host's frame ring: a second capture of each drawn present (not interpolated frames, not skipped presents),
    // independent of the present capture hooks the shot recorder owns; both can capture the same present. While set,
    // the present queue draws the VI renderer's output once more into a width x height target (the GPU scales it) in
    // VIRenderer::Layout::ConsoleArea, the console's 4:3 picture filling the target, reads it back, and calls
    // `captured` on the present thread after the present's command list ran and before the workload thread is let
    // go, so the callback must be short (a copy). The image's size is the one recorded.
    struct RingCaptureListener {
        uint32_t width = 0;
        uint32_t height = 0;
        PresentCaptureHook *captured = nullptr;
    };

    // Any thread, at any time, with the same lifetime rule as the probe hooks. Null (the default): no extra draw and
    // no readback, one atomic load per drawn present.
    void SetRingCaptureListener(const RingCaptureListener *listener);
    const RingCaptureListener *GetRingCaptureListener();

    // VI registers as Application::Core::decodeVI masked them, in register order: status, origin, width, intr,
    // v_current_line, timing (burst), v_sync, h_sync, leap, h_start, v_start, v_burst, x_scale, y_scale.
    constexpr uint32_t ProbeViRegisterCount = 14;

    // One tile descriptor as RDP::tiles holds it.
    struct ProbeRdpTile {
        uint8_t fmt = 0;
        uint8_t siz = 0;
        uint16_t line = 0;
        uint16_t tmem = 0;
        uint8_t palette = 0;
        uint8_t cms = 0;
        uint8_t cmt = 0;
        uint8_t masks = 0;
        uint8_t maskt = 0;
        uint8_t shifts = 0;
        uint8_t shiftt = 0;
        uint16_t uls = 0;
        uint16_t ult = 0;
        uint16_t lrs = 0;
        uint16_t lrt = 0;
    };

    // The RDP state that carries across tasks and frames, as RDP holds it: the current (top of stack) value of each
    // stacked field, colours as the floats RDP keeps. Left out: the extended GBI's stacks below the top and its
    // alignment state, tile replacement hashes, the Rice load history and a partial LLE command.
    struct ProbeRdpState {
        uint64_t tmem[512] = {};             // RDP::TMEM, RDP_TMEM_WORDS words as RT64 lays them out
        uint32_t textureAddress = 0;         // RDP::texture (the last texture image)
        uint8_t textureFmt = 0;
        uint8_t textureSiz = 0;
        uint16_t textureWidth = 0;
        ProbeRdpTile tiles[8];
        uint32_t colorImageAddress = 0;
        uint8_t colorImageFmt = 0;
        uint8_t colorImageSiz = 0;
        uint16_t colorImageWidth = 0;
        uint32_t depthImageAddress = 0;
        uint32_t otherModeL = 0;
        uint32_t otherModeH = 0;
        uint32_t combinerL = 0;
        uint32_t combinerH = 0;
        float envColor[4] = {};
        float primColor[4] = {};
        float primLod[2] = {};
        float primDepth[2] = {};
        float blendColor[4] = {};
        float fogColor[4] = {};
        uint32_t fillColor = 0;
        int32_t scissorRect[4] = {};         // ulx, uly, lrx, lry in quarter pixels
        uint8_t scissorMode = 0;
        int32_t convertK[6] = {};
        float keyCenter[3] = {};
        float keyScale[3] = {};
    };

    // The renderer probe's capture hooks (the host's frame captures), switched as one set like RendererProbeHooks and
    // independent of it. A null member is not called. Every member is called on the thread that processes display
    // lists and updates the screen.
    struct RendererCaptureHooks {
        // An RDRAM range RT64 reads while processing a workload or updating the screen: display list commands,
        // matrices, vertices, viewports, lights, moved memory, ucode bytes for GBI detection, texture and TLUT loads,
        // framebuffer RAM checks and the screen framebuffer. A physical address, possibly past RDRAM's end.
        void (*rdramRead)(uint32_t address, uint32_t size) = nullptr;

        // Interpreter::loadUCodeGBI for a task, after the GBI was chosen (cached or detected): RT64's GBIUCode value,
        // 0 when no GBI matched.
        void (*gbiChosen)(uint32_t ucode) = nullptr;

        // State::updateScreen, for a present it queues, just before the present queued hooks.
        void (*presentQueued)(uint64_t presentId, const uint32_t (&viRegisters)[ProbeViRegisterCount]) = nullptr;
    };

    // Any thread, at any time, with the same lifetime rule as the probe hooks. Null (the default): each read site
    // costs one atomic load.
    void SetRendererCaptureHooks(const RendererCaptureHooks *hooks);
    const RendererCaptureHooks *GetRendererCaptureHooks();

    // Reports a read to the capture hooks when they are set.
    inline void ReportRdramRead(uint32_t address, uint32_t size) {
        const RendererCaptureHooks *hooks = GetRendererCaptureHooks();
        if ((hooks != nullptr) && (hooks->rdramRead != nullptr)) {
            hooks->rdramRead(address, size);
        }
    }

    // The display list thread only, between workloads. Fills `state` from the RDP of the State that last ran setup.
    // False before any State has.
    bool ReadProbeRdpState(ProbeRdpState &state);

    // The exact inverse of ReadProbeRdpState, for the host's frame replay: sets the same fields of that RDP from `state`.
    // The display list thread only, between workloads. False before any State has run setup.
    bool WriteProbeRdpState(const ProbeRdpState &state);
};
