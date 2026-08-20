// 文件作用：在检测启动前检查运行状态和已经集中准备好的模板资源。
#pragma once

#include "contracts/detection_mode.h"

#include <QString>
#include <QStringList>

enum class InspectionStartIssue {
    None,
    TemplateOperationActive,
    RuntimeBusy,
    CameraClosed,
    DirtySettingsConfirmationRequired,
    PlcDisconnected,
    TemplateMissing,
    TemplatesMissing,
    TemplateResourcesInvalid,
    TemplateIncomplete,
    TemplatesIncomplete
};

struct InspectionStartAccessInput
{
    bool templateOperationActive = false;
    bool runtimeBusy = false;
    bool cameraOpen = false;
    bool dirtySettings = false;
    bool plcTriggerEnabled = false;
    bool plcConnected = false;
};

struct InspectionStartResourceInput
{
    DetectionMode mode = DetectionMode::Stamp;
    int preparedTemplateCount = 0;
    bool barcodeDecoderReady = true;
    QString barcodeDecoderError;
};

struct InspectionStartPreflightResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    QStringList details;

    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None;
    }
};

class InspectionStartPreflight
{
public:
    static InspectionStartPreflightResult evaluateAccess(
        const InspectionStartAccessInput &input);
    static InspectionStartPreflightResult evaluateResources(
        const InspectionStartResourceInput &input);
};
