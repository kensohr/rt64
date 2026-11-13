//
// RT64
//

#pragma once

#include "common/rt64_user_configuration.h"
#include "hle/rt64_vi.h"

#include "rt64_descriptor_sets.h"
#include "rt64_shader_library.h"

namespace RT64 {
    struct VIRenderer {
        // How the VI's output is placed on the target. Window: RT64's HD space fitted into the target with its aspect
        // kept, as the swap chain shows it. ConsoleArea: the 4:3 scanline area the console outputs (320x240 SD,
        // with the VI's own borders) stretched over the whole target, whatever the window, aspect ratio or
        // resolution settings are; what a wider aspect setting adds beside it is cut off.
        enum class Layout {
            Window,
            ConsoleArea
        };

        std::unique_ptr<VideoInterfaceDescriptorSet> descriptorSet;
        const RenderSampler *descriptorSetSampler = nullptr;

        struct RenderParams {
            RenderDevice *device = nullptr;
            RenderCommandList *commandList = nullptr;
            RenderTexture *texture = nullptr;
            const RenderSwapChain *swapChain = nullptr;
            const ShaderLibrary *shaderLibrary = nullptr;
            RenderFormat textureFormat = RenderFormat::UNKNOWN;
            hlslpp::float2 resolutionScale;
            uint32_t downsamplingScale = 0;
            uint32_t textureWidth = 0;
            uint32_t textureHeight = 0;
            UserConfiguration::Filtering filtering = UserConfiguration::Filtering::Linear;
            const VI *vi = nullptr;
            bool removeBlackBorders = false;
            // The size of the target drawn into, when it isn't the swap chain's (0 x 0: the swap chain's).
            uint32_t targetWidth = 0;
            uint32_t targetHeight = 0;
            Layout layout = Layout::Window;
        };

        VIRenderer();
        ~VIRenderer();
        void render(const RenderParams &p);
        static void getViewportAndScissor(const RenderSwapChain *swapChain, const VI &vi, hlslpp::float2 resolutionScale, uint32_t downsamplingScale, bool removeBlackBorders, RenderViewport &viewport, RenderRect &scissor);
        static void getViewportAndScissor(uint32_t windowWidth, uint32_t windowHeight, const VI &vi, hlslpp::float2 resolutionScale, uint32_t downsamplingScale, bool removeBlackBorders, RenderViewport &viewport, RenderRect &scissor);
        static void getConsoleAreaViewportAndScissor(uint32_t targetWidth, uint32_t targetHeight, const VI &vi, hlslpp::float2 resolutionScale, RenderViewport &viewport, RenderRect &scissor);
    };
};