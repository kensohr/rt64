//
// RT64
//

#include "rt64_renderer_probe.h"

#include <atomic>
#include <mutex>

namespace RT64 {
    static std::mutex deviceInfoMutex;
    static std::optional<RendererDeviceInfo> deviceInfo;

    std::optional<RendererDeviceInfo> GetRendererDeviceInfo() {
        std::lock_guard<std::mutex> lock(deviceInfoMutex);
        return deviceInfo;
    }

    void SetRendererDeviceInfo(RendererDeviceInfo info) {
        std::lock_guard<std::mutex> lock(deviceInfoMutex);
        deviceInfo = std::move(info);
    }

    static std::atomic<const RendererProbeHooks *> probeHooks = nullptr;

    void SetRendererProbeHooks(const RendererProbeHooks *hooks) {
        probeHooks = hooks;
    }

    const RendererProbeHooks *GetRendererProbeHooks() {
        return probeHooks.load();
    }

    static std::atomic<const RingCaptureListener *> ringCaptureListener = nullptr;

    void SetRingCaptureListener(const RingCaptureListener *listener) {
        ringCaptureListener = listener;
    }

    const RingCaptureListener *GetRingCaptureListener() {
        return ringCaptureListener.load();
    }

    static std::atomic<const RendererCaptureHooks *> captureHooks = nullptr;

    void SetRendererCaptureHooks(const RendererCaptureHooks *hooks) {
        captureHooks = hooks;
    }

    const RendererCaptureHooks *GetRendererCaptureHooks() {
        return captureHooks.load();
    }
};
