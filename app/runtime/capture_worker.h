#pragma once

#include "devices/camera/camera_device.h"

#include <atomic>
#include <functional>
#include <memory>
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
        const CaptureWorkerCallbacks &callbacks);
    void stop();
    bool isRunning() const;

private:
    void run();

    std::shared_ptr<ICameraDevice> m_cameraDevice;
    CaptureMode m_mode = CaptureMode::SoftwareTrigger;
    CaptureWorkerCallbacks m_callbacks;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};
