// 文件作用：本文件用于在后台等待相机帧，并按软触发、硬触发或预览模式提交采集结果。
// 主要职责：在后台等待相机帧，并按软触发、硬触发或预览模式提交采集结果。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "devices/camera/camera_device.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

// 组件说明：CaptureMode 枚举列出该组件允许使用的稳定状态和选项。
enum class CaptureMode
{
    SoftwareTrigger,
    HardwareTrigger,
    Preview
};

// 组件说明：CaptureWorkerCallbacks 数据结构集中传递该流程需要的只读数据或回调。
struct CaptureWorkerCallbacks
{
    std::function<void(const CameraFrame &)> frameReady;
    std::function<void(CameraFrameStatus, int)> captureError;
    std::function<void()> stopped;
};

// 组件说明：CaptureWorker 组件封装对应业务职责和生命周期边界。
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
