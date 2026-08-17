#include "runtime/capture_worker.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

CaptureWorker::CaptureWorker(
    const std::shared_ptr<ICameraDevice> &cameraDevice)
    : m_cameraDevice(cameraDevice)
{
    if (!m_cameraDevice) {
        throw std::invalid_argument(
                    "CaptureWorker requires a camera device");
    }
}

CaptureWorker::~CaptureWorker()
{
    stop();
}

bool CaptureWorker::start(
    CaptureMode mode,
    int minimumIntervalMs,
    const CaptureWorkerCallbacks &callbacks)
{
    if (m_running.load()) {
        return false;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_mode = mode;
    m_minimumIntervalMs = std::max(0, minimumIntervalMs);
    m_callbacks = callbacks;
    m_stopRequested.store(false);
    m_running.store(true);
    m_thread = std::thread(&CaptureWorker::run, this);
    return true;
}

void CaptureWorker::stop()
{
    m_stopRequested.store(true);
    m_cameraDevice->interruptWait();
    m_waitCondition.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_running.store(false);
}

bool CaptureWorker::isRunning() const
{
    return m_running.load();
}

void CaptureWorker::run()
{
    int consecutiveFailures = 0;
    std::chrono::steady_clock::time_point lastFrameTime;
    bool hasLastFrameTime = false;

    while (!m_stopRequested.load()) {
        if (m_mode != CaptureMode::HardwareTrigger
                && hasLastFrameTime
                && !waitMinimumInterval(lastFrameTime)) {
            break;
        }
        if (m_mode != CaptureMode::HardwareTrigger) {
            const CameraResult trigger = m_cameraDevice->triggerSoftware();
            if (!trigger.isSuccess()) {
                ++consecutiveFailures;
                if (consecutiveFailures >= 3) {
                    if (m_callbacks.captureError) {
                        m_callbacks.captureError(
                                    CameraFrameStatus::DeviceError,
                                    trigger.nativeErrorCode);
                    }
                    break;
                }
                continue;
            }
        }

        const CameraFrameResult frame =
                m_cameraDevice->waitNextFrame(
                    m_mode == CaptureMode::HardwareTrigger ? 1000 : 500);
        if (m_stopRequested.load()
                || frame.status == CameraFrameStatus::Interrupted) {
            break;
        }
        if (frame.status == CameraFrameStatus::Timeout) {
            ++consecutiveFailures;
            if (m_mode != CaptureMode::HardwareTrigger
                    && consecutiveFailures >= 3) {
                if (m_callbacks.captureError) {
                    m_callbacks.captureError(
                                CameraFrameStatus::Timeout, 0);
                }
                break;
            }
            continue;
        }
        if (frame.status == CameraFrameStatus::DeviceError) {
            if (m_callbacks.captureError) {
                m_callbacks.captureError(
                            frame.status, frame.nativeErrorCode);
            }
            break;
        }
        if (frame.frame.image.empty()) {
            continue;
        }

        consecutiveFailures = 0;
        lastFrameTime = std::chrono::steady_clock::now();
        hasLastFrameTime = true;
        if (m_callbacks.frameReady) {
            m_callbacks.frameReady(frame.frame);
        }
    }

    m_running.store(false);
    if (m_callbacks.stopped) {
        m_callbacks.stopped();
    }
}

bool CaptureWorker::waitMinimumInterval(
    const std::chrono::steady_clock::time_point &lastFrameTime) const
{
    if (m_minimumIntervalMs <= 0) {
        return !m_stopRequested.load();
    }
    const std::chrono::steady_clock::time_point deadline =
            lastFrameTime
            + std::chrono::milliseconds(m_minimumIntervalMs);
    std::unique_lock<std::mutex> lock(m_waitMutex);
    m_waitCondition.wait_until(
                lock,
                deadline,
                [this]() { return m_stopRequested.load(); });
    return !m_stopRequested.load();
}
