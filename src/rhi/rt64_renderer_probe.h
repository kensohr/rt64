//
// RT64
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

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
};
