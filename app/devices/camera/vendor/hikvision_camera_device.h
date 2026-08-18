// 文件作用：本文件用于封装海康相机SDK，完成枚举、打开、触发、参数设置和帧回调转换。
// 主要职责：封装海康相机SDK，完成枚举、打开、触发、参数设置和帧回调转换。
// 模块位置：设备层；通过统一端口隔离相机和PLC供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "devices/camera/camera_device.h"

#include <memory>

// 组件说明：HikvisionCameraDevice 组件提供对应硬件设备能力的统一实现。
class HikvisionCameraDevice final : public ICameraDevice
{
public:
    HikvisionCameraDevice();
    ~HikvisionCameraDevice() override;

    CameraResult enumerate(int *deviceCount) override;
    CameraResult openFirst() override;
    CameraResult applySettings(
        const CameraSettings &settings) override;
    CameraResult setTriggerMode(CameraTriggerMode mode) override;
    CameraResult startGrabbing() override;
    CameraResult triggerSoftware() override;
    CameraFrameResult waitNextFrame(int timeoutMs) override;
    void interruptWait() override;
    CameraResult stopGrabbing() override;
    CameraResult close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
