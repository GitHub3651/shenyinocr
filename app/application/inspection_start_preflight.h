// 文件作用：本文件用于在启动检测前检查相机、PLC、配方和当前运行状态是否满足条件。
// 主要职责：在启动检测前检查相机、PLC、配方和当前运行状态是否满足条件。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "contracts/detection_mode.h"

#include <QString>
#include <QStringList>
#include <QVector>

enum class InspectionStartIssue {
    None,
    TemplateOperationActive,
    RuntimeBusy,
    CameraClosed,
    DirtySettingsConfirmationRequired,
    PlcDisconnected,
    PreparedRecipeMissing,
    WordProfilesMissing,
    BarcodeResourcesInvalid,
    ProductTemplateIncomplete,
    WordProfilesIncomplete
};

// 组件说明：InspectionStartAccessInput 数据结构集中保存该流程需要的一组相关数据。
struct InspectionStartAccessInput
{
    bool templateOperationActive = false;
    bool runtimeBusy = false;
    bool cameraOpen = false;
    bool dirtySettings = false;
    bool plcTriggerEnabled = false;
    bool plcConnected = false;
};

// 组件说明：InspectionStartProfileReadiness 数据结构集中保存该流程需要的一组相关数据。
struct InspectionStartProfileReadiness
{
    QString displayName;
    bool trackingTemplateReady = false;
    bool calibrationReady = false;
    bool barcodeRegionReady = false;
    bool dateRegionReady = false;
    bool targetTextReady = false;
    bool characterTemplatesReady = false;
};

// 组件说明：InspectionStartResourceInput 数据结构集中保存该流程需要的一组相关数据。
struct InspectionStartResourceInput
{
    DetectionMode mode = DetectionMode::Stamp;
    bool preparedRecipeReady = false;
    bool trackingTemplateReady = false;
    bool dateRegionReady = false;
    bool targetTextRequired = false;
    bool targetTextReady = false;
    bool characterTemplatesRequired = false;
    bool characterTemplatesReady = false;
    bool barcodeDecoderReady = true;
    QString barcodeDecoderError;
    QVector<InspectionStartProfileReadiness> profiles;
};

// 组件说明：InspectionStartPreflightResult 数据结构保存一次操作的结果、状态和错误信息。
struct InspectionStartPreflightResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    QStringList details;

    // 函数说明：isAccepted 函数检查相关状态并返回判断结果。
    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None;
    }
};

// 组件说明：InspectionStartPreflight 组件封装本文件中与其名称对应的单一职责。
class InspectionStartPreflight
{
public:
    static InspectionStartPreflightResult evaluateAccess(
        const InspectionStartAccessInput &input);
    static InspectionStartPreflightResult evaluateResources(
        const InspectionStartResourceInput &input);
};
