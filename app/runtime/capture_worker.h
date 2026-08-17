#pragma once

#include "devices/camera/camera_device.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

enum class CaptureMode
{
    SoftwareTrigger,
    HardwareTrigger,
    Preview
};

struct CaptureWorkerCallbacks
{
    std::function<void(const CameraFrame &)> frameReady;
    std::function<void(CameraFrameStatus, int)> captureError;
    std::function<void()> stopped;
};

class CaptureWorker
{
public:
    explicit CaptureWorker(
        const std::shared_ptr<ICameraDevice> &cameraDevice);
    ~CaptureWorker();

    bool start(
        CaptureMode mode,
        int minimumIntervalMs,
        const CaptureWorkerCallbacks &callbacks);
    void stop();
    bool isRunning() const;

private:
    void run();
    bool waitMinimumInterval(
        const std::chrono::steady_clock::time_point &lastFrameTime) const;

    std::shared_ptr<ICameraDevice> m_cameraDevice;
    CaptureMode m_mode = CaptureMode::SoftwareTrigger;
    int m_minimumIntervalMs = 0;
    CaptureWorkerCallbacks m_callbacks;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_running{false};
    mutable std::mutex m_waitMutex;
    mutable std::condition_variable m_waitCondition;
    std::thread m_thread;
};
