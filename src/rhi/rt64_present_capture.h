//
// RT64
//

#pragma once

#include <cstdint>

namespace RT64 {
    // An image the present queue drew with the VI renderer, read back before the render hook's draw (so without any
    // UI the host draws), at the swap chain's size. Pixels are B8G8R8A8 rows top to bottom, rowPitch bytes apart,
    // valid only during the hook call.
    struct PresentCaptureImage {
        uint64_t presentId = 0;
        uint32_t fbAddress = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t rowPitch = 0;
        const uint8_t *bgra = nullptr;
    };

    // Called on the thread that queues presents (the VI update) for each present queued, with its id (increasing,
    // from 1) and the VI framebuffer address it shows.
    using PresentQueuedHook = void(uint64_t presentId, uint32_t fbAddress);

    // Called on the present thread for each drawn present (not interpolated frames, not skipped presents).
    using PresentCaptureHook = void(const PresentCaptureImage &image);

    PresentQueuedHook *GetPresentQueuedHook();
    PresentCaptureHook *GetPresentCaptureHook();

    // Both null by default: then nothing is read back and presenting is unchanged. Set them before the game starts.
    void SetPresentCaptureHooks(PresentQueuedHook *queued, PresentCaptureHook *capture);
};
