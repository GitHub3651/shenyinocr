// 文件作用：本文件用于在后台等待相机帧，并按软触发、硬触发或预览模式提交采集结果。
// 主要职责：在后台等待相机帧，并按软触发、硬触发或预览模式提交采集结果。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/capture_worker.h"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

constexpr int kHardwareFrameWaitSliceMs = 1000;
constexpr int kSoftwareFrameWaitTimeoutMs = 500;
constexpr int kSoftwareTriggerIntervalMs = 180;

} // namespace

// 函数说明：CaptureWorker 构造函数创建组件并初始化其依赖和初始状态。
CaptureWorker::CaptureWorker(
    const std::shared_ptr<ICameraDevice> &cameraDevice)
    : m_cameraDevice(cameraDevice)
{
    if (!m_cameraDevice) {
        throw std::invalid_argument(
                    "CaptureWorker requires a camera device");
    }
}

// 函数说明：~CaptureWorker 析构函数按生命周期要求释放组件持有的资源。
CaptureWorker::~CaptureWorker()
{
    stop();
}

// 函数说明：start 函数创建、准备或启动对应流程。
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

// 函数说明：stop 函数停止流程、清理状态或释放对应资源。
void CaptureWorker::stop()
{
    m_stopRequested.store(true);
    m_cameraDevice->interruptWait();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_running.store(false);
}

// 函数说明：isRunning 函数检查相关状态并返回判断结果。
bool CaptureWorker::isRunning() const
{
    return m_running.load();
}

// 函数说明：run 函数执行对应事件或业务处理。
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
