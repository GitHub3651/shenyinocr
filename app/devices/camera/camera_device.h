// 文件作用：本文件用于定义相机设备端口、触发模式、帧数据和设备错误状态。
// 主要职责：定义相机设备端口、触发模式、帧数据和设备错误状态。
// 模块位置：设备层；通过统一端口隔离相机、PLC、OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QDateTime>

#include <opencv2/core.hpp>

#include <cstdint>

// 组件说明：CameraResultCode 枚举列出该组件允许使用的稳定状态和选项。
enum class CameraResultCode
{
    Success,
    InvalidState,
    InvalidSettings,
    DeviceError
};

// 组件说明：CameraSettings 组件集中描述相关配置、规则和运行参数。
struct CameraSettings
{
    bool updateExposure = false;
    float exposure = 0.0f;
    bool updateGain = false;
    float gain = 0.0f;
    bool updateTriggerDelay = false;
    float triggerDelayMicroseconds = 0.0f;
    bool updateLineDebouncerTime = false;
    unsigned int lineDebouncerTime = 5000U;
};

// 组件说明：CameraSettingRange 数据结构集中保存该流程需要的一组相关数据。
struct CameraSettingRange
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    float current = 0.0f;
};

// 组件说明：CameraResult 数据结构保存一次操作的结果、状态和错误信息。
struct CameraResult
{
    CameraResultCode code = CameraResultCode::Success;
    int nativeErrorCode = 0;
    CameraSettingRange exposureRange;
    CameraSettingRange gainRange;

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return code == CameraResultCode::Success;
    }

    // 函数说明：deviceError 函数实现名称所表示的处理步骤。
    static CameraResult deviceError(int nativeErrorCode)
    {
        CameraResult result;
        result.code = CameraResultCode::DeviceError;
        result.nativeErrorCode = nativeErrorCode;
        return result;
    }
};

// 组件说明：CameraTriggerMode 枚举列出该组件允许使用的稳定状态和选项。
enum class CameraTriggerMode
{
    Software,
    HardwareLine0
};

// 组件说明：CameraFrame 数据结构集中保存该流程需要的一组相关数据。
struct CameraFrame
{
    std::uint64_t sequence = 0;
    QDateTime timestampUtc;
    cv::Mat image;
};

// 组件说明：CameraFrameStatus 枚举列出该组件允许使用的稳定状态和选项。
enum class CameraFrameStatus
{
    FrameReady,
    Timeout,
    Interrupted,
    DeviceError
};

// 组件说明：CameraFrameResult 数据结构保存一次操作的结果、状态和错误信息。
struct CameraFrameResult
{
    CameraFrameStatus status = CameraFrameStatus::Timeout;
    CameraFrame frame;
    int nativeErrorCode = 0;
};

// 组件说明：ICameraDevice 组件提供对应设备或检测能力的统一实现。
class ICameraDevice
{
public:
    // 函数说明：~ICameraDevice 函数实现名称所表示的处理步骤。
    virtual ~ICameraDevice() {}

    virtual CameraResult enumerate(int *deviceCount) = 0;
    virtual CameraResult openFirst() = 0;
    virtual CameraResult applySettings(
        const CameraSettings &settings) = 0;
    virtual CameraResult setTriggerMode(CameraTriggerMode mode) = 0;
    virtual CameraResult startGrabbing() = 0;
    virtual CameraResult triggerSoftware() = 0;
    virtual CameraFrameResult waitNextFrame(int timeoutMs) = 0;
    virtual void interruptWait() = 0;
    virtual CameraResult stopGrabbing() = 0;
    virtual CameraResult close() = 0;
};
