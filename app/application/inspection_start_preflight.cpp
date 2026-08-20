// 文件作用：实现检测启动前的轻量状态检查；模板字段校验由 TemplateStore 负责。
#include "application/inspection_start_preflight.h"

namespace {

InspectionStartPreflightResult rejected(
    InspectionStartIssue issue,
    const QStringList &details = QStringList())
{
    InspectionStartPreflightResult result;
    result.issue = issue;
    result.details = details;
    return result;
}

}

InspectionStartPreflightResult InspectionStartPreflight::evaluateAccess(
    const InspectionStartAccessInput &input)
{
    if (input.templateOperationActive) {
        return rejected(InspectionStartIssue::TemplateOperationActive);
    }
    if (input.runtimeBusy) {
        return rejected(InspectionStartIssue::RuntimeBusy);
    }
    if (!input.cameraOpen) {
        return rejected(InspectionStartIssue::CameraClosed);
    }
    if (input.dirtySettings) {
        return rejected(
                    InspectionStartIssue::DirtySettingsConfirmationRequired);
    }
    if (input.plcTriggerEnabled && !input.plcConnected) {
        return rejected(InspectionStartIssue::PlcDisconnected);
    }
    return InspectionStartPreflightResult();
}

InspectionStartPreflightResult InspectionStartPreflight::evaluateResources(
    const InspectionStartResourceInput &input)
{
    if (input.mode != DetectionMode::Tissue
            && input.preparedTemplateCount <= 0) {
        return rejected(InspectionStartIssue::TemplateMissing);
    }
    if (detectionModeDescriptor(input.mode).requiresBarcodeDecoder
            && !input.barcodeDecoderReady) {
        return rejected(
                    InspectionStartIssue::TemplateResourcesInvalid,
                    QStringList()
                    << QStringLiteral("读码组件不可用：%1")
                       .arg(input.barcodeDecoderError));
    }
    return InspectionStartPreflightResult();
}
