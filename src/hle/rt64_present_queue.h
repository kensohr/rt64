//
// RT64
//

#pragma once

#include "common/rt64_profiling_timer.h"
#include "gui/rt64_inspector.h"
#include "render/rt64_vi_renderer.h"
#include "rhi/rt64_present_capture.h"

#include "rt64_application_window.h"
#include "rt64_present.h"
#include "rt64_shared_queue_resources.h"

#define PRESENT_QUEUE_SIZE 4

namespace RT64 {
    struct WorkloadQueue;

    struct PresentQueue {
        struct External {
            ApplicationWindow *appWindow = nullptr;
            RenderDevice *device = nullptr;
            RenderSwapChain *swapChain = nullptr;
            RenderWorker *presentGraphicsWorker = nullptr;
            WorkloadQueue *workloadQueue = nullptr;
            SharedQueueResources *sharedResources = nullptr;
            const ShaderLibrary *shaderLibrary = nullptr;
            UserConfiguration::GraphicsAPI createdGraphicsAPI = UserConfiguration::GraphicsAPI::OptionCount;
            bool swapChainPresentWaitEnabled = false;
        };

        External ext;
        std::array<Present, PRESENT_QUEUE_SIZE> presents;
        int threadCursor;
        int writeCursor;
        int barrierCursor;
        std::mutex cursorMutex;
        std::condition_variable cursorCondition;
        uint64_t presentId;
        std::mutex presentIdMutex;
        std::condition_variable presentIdCondition;
        std::thread *presentThread = nullptr;
        std::mutex threadMutex;
        std::atomic<bool> presentThreadRunning = false;
        std::recursive_mutex inspectorMutex;
        std::mutex screenFbChangePoolMutex;
        Framebuffer scratchFb;
        FramebufferChangePool scratchFbChangePool;
        FramebufferChangePool screenFbChangePool;
        std::atomic<bool> viewRDRAM = false;
        std::vector<std::unique_ptr<RenderFramebuffer>> swapChainFramebuffers;
        std::unique_ptr<RenderCommandSemaphore> acquiredSemaphore;
        std::vector<std::unique_ptr<RenderCommandSemaphore>> drawSemaphores;
        std::unique_ptr<VIRenderer> viRenderer;
        std::unique_ptr<Inspector> inspector;
        ProfilingTimer presentProfiler = ProfilingTimer(120);
        Timestamp presentTimestamp;
        VIHistory viHistory;

        // An offscreen copy of the VI renderer's image, read back for the present capture hook (rt64_present_capture.h)
        // or the ring capture listener (rt64_renderer_probe.h).
        struct Capture {
            std::unique_ptr<RenderTexture> texture;
            std::unique_ptr<RenderFramebuffer> framebuffer;
            std::unique_ptr<RenderBuffer> readback;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t rowPitch = 0;
            bool recorded = false;
        };

        Capture capture;
        Capture ringCapture;

        PresentQueue();
        ~PresentQueue();
        void reset();
        void advanceToNextPresent();
        void repeatLastPresent();
        uint32_t previousWriteCursor() const;
        void waitForIdle();
        void waitForPresentId(uint64_t waitId);
        void setup(const External &ext);
        void threadPresent(const Present &present, bool &swapChainValid);
        void recordCapture(RenderCommandList *commandList, const VIRenderer::RenderParams &renderParams, Capture &target, uint32_t width, uint32_t height, VIRenderer::Layout layout);
        void deliverCapture(const Present &present, Capture &target, PresentCaptureHook *captureHook);
        void skipInterpolation();
        void notifyPresentId(const Present &present);
        void threadAdvanceBarrier();
        void threadLoop();
    };
};