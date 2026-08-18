// 文件作用：本文件用于定义界面与应用服务之间使用的相机命令、结果和参数合同。
// 主要职责：定义界面与应用服务之间使用的相机命令、结果和参数合同。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>

// 组件说明：InspectionAcquisitionDto 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionAcquisitionDto
{
    SoftwareTrigger,
    HardwareTrigger
};

// 组件说明：CameraOpenIssueDto 枚举列出该组件允许使用的稳定状态和选项。
enum class CameraOpenIssueDto
{
    None,
    DeviceNotFound,
    DeviceOpenFailed,
    ExposureFailed,
    InitializationFailed
};

// 组件说明：CameraParameterResultDto 数据结构集中保存该流程需要的一组相关数据。
struct CameraParameterResultDto
{
    bool success = false;
    int minimumValue = 0;
    int maximumValue = 0;
    double actualValue = 0.0;
    int nativeErrorCode = 0;
    QString diagnostic;
};

// 组件说明：CameraOpenResultDto 数据结构集中保存该流程需要的一组相关数据。
struct CameraOpenResultDto
{
    CameraOpenIssueDto issue = CameraOpenIssueDto::None;
    int deviceCount = 0;
    int appliedExposure = 0;
    int exposureMinimum = 0;
    int exposureMaximum = 0;
    bool exposureAdjusted = false;
    QString adjustmentMessage;
    QString diagnostic;

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return issue == CameraOpenIssueDto::None;
    }
};

// 组件说明：CameraRecoveryIssueDto 枚举列出该组件允许使用的稳定状态和选项。
enum class CameraRecoveryIssueDto
{
    None,
    MissingCamera,
    ExposureRejected,
    InitializationFailed
};

// 组件说明：CameraRecoveryResultDto 数据结构集中保存该流程需要的一组相关数据。
struct CameraRecoveryResultDto
{
    CameraRecoveryIssueDto issue = CameraRecoveryIssueDto::None;
    bool recoveryAttempted = false;
    bool cameraOpen = false;
    QString adjustmentMessage;
    QString errorMessage;

    // 函数说明：isRecovered 函数检查相关状态并返回判断结果。
    bool isRecovered() const
    {
        return issue == CameraRecoveryIssueDto::None;
    }
};
