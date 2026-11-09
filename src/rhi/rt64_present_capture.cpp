//
// RT64
//

#include "rt64_present_capture.h"

#include <atomic>

namespace RT64 {
    static std::atomic<PresentQueuedHook *> queuedHook = nullptr;
    static std::atomic<PresentCaptureHook *> captureHook = nullptr;

    PresentQueuedHook *GetPresentQueuedHook() {
        return queuedHook.load();
    }

    PresentCaptureHook *GetPresentCaptureHook() {
        return captureHook.load();
    }

    void SetPresentCaptureHooks(PresentQueuedHook *queued, PresentCaptureHook *capture) {
        queuedHook = queued;
        captureHook = capture;
    }
};
