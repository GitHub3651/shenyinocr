#include "runtime/capture_worker.h"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

constexpr int kHardwareFrameWaitSliceMs = 1000;
constexpr int kSoftwareFrameWaitTimeoutMs = 500;
constexpr int kSoftwareTriggerIntervalMs = 180;

} // namespace

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
    const CaptureWorkerCallbacks &callbacks)
{
    if (m_running.load()) {
        return false;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_mode = mode;
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

    while (!m_stopRequested.load()) {
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
                    m_mode == CaptureMode::HardwareTrigger
                    ? kHardwareFrameWaitSliceMs
                    : kSoftwareFrameWaitTimeoutMs);
        if (m_stopRequested.load()
                || frame.status == CameraFrameStatus::Interrupted) {
            break;
        }
        if (frame.status == CameraFrameStatus::Timeout) {
            // 外部硬件在空闲时可能长期没有触发沿；单次等待到期只让
            // Worker重新取得控制权，不代表产品帧失败，也不限制下一帧。
            if (m_mode == CaptureMode::HardwareTrigger) {
                continue;
            }
            ++consecutiveFailures;
            if (consecutiveFailures >= 3) {
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
        if (m_callbacks.frameReady) {
            m_callbacks.frameReady(frame.frame);
        }
        if (m_mode == CaptureMode::SoftwareTrigger
                && !m_stopRequested.load()) {
            std::this_thread::sleep_for(
                        std::chrono::milliseconds(
                            kSoftwareTriggerIntervalMs));
        }
    }

    m_running.store(false);
    if (m_callbacks.stopped) {
        m_callbacks.stopped();
    }
}
