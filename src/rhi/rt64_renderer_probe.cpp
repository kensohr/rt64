//
// RT64
//

#include "rt64_renderer_probe.h"

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
};
