// 文件作用：本文件用于根据运行状态计算按钮文字、启用状态和可执行操作。
// 主要职责：根据运行状态计算按钮文字、启用状态和可执行操作。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "operation_ui_policy.h"

// 函数说明：create 函数创建、准备或启动对应流程。
OperationUiSnapshot OperationUiPolicy::create(OperationUiState state)
{
    OperationUiSnapshot snapshot;
    snapshot.enableAllOperations =
            state == OperationUiState::CameraClosed
            || state == OperationUiState::CameraReady;
    snapshot.settingsEnabled = snapshot.enableAllOperations;
    snapshot.startDetectionText =
            state == OperationUiState::Detecting
            ? QStringLiteral("\u91c7\u96c6\u4e2d...")
            : (state == OperationUiState::Stopping
               ? QStringLiteral("\u505c\u6b62\u4e2d...")
               : QStringLiteral("\u542f\u52a8\u8bc6\u522b"));

    const bool templateOperation =
            state == OperationUiState::TemplatePreviewing
            || state == OperationUiState::TemplateFrozen;
    snapshot.stopText =
            state == OperationUiState::Fault
            ? QStringLiteral("\u786e\u8ba4\u6545\u969c\u5e76\u6062\u590d")
            : (state == OperationUiState::Stopping
            ? QStringLiteral("\u505c\u6b62\u4e2d...")
            : (templateOperation
               ? QStringLiteral("\u9000\u51fa\u6a21\u677f\u5236\u4f5c")
               : QStringLiteral("\u505c\u6b62\u8bc6\u522b")));
    snapshot.templateCaptureText =
            state == OperationUiState::TemplatePreviewing
            ? QStringLiteral("\u62cd\u7167\u5e76\u5f00\u59cb\u6846\u9009")
            : (state == OperationUiState::TemplateFrozen
               ? QStringLiteral("\u91cd\u65b0\u53d6\u666f")
               : QStringLiteral("\u5236\u4f5c\u6a21\u677f"));

    switch (state) {
    case OperationUiState::CameraClosed:
        snapshot.openCameraEnabled = true;
        snapshot.startDetectionEnabled = false;
        snapshot.stopEnabled = false;
        snapshot.closeCameraEnabled = false;
        snapshot.templateCaptureEnabled = false;
        snapshot.saveTemplateEnabled = false;
        break;
    case OperationUiState::CameraReady:
        snapshot.openCameraEnabled = false;
        snapshot.startDetectionEnabled = true;
        snapshot.stopEnabled = false;
        snapshot.closeCameraEnabled = true;
        snapshot.templateCaptureEnabled = true;
        snapshot.saveTemplateEnabled = false;
        break;
    case OperationUiState::Detecting:
    case OperationUiState::Stopping:
        snapshot.stopEnabled = true;
        break;
    case OperationUiState::Fault:
        snapshot.stopEnabled = true;
        snapshot.statusText = QStringLiteral(
                    "\u7cfb\u7edf\u6545\u969c\uff1a\u68c0\u6d4b\u5df2\u6682\u505c");
        break;
    case OperationUiState::TemplatePreviewing:
        snapshot.stopEnabled = true;
        snapshot.templateCaptureEnabled = true;
        snapshot.statusText = QStringLiteral(
                    "\u6a21\u677f\u5236\u4f5c\u4e2d\uff1a\u5b9e\u65f6\u53d6\u666f");
        break;
    case OperationUiState::TemplateFrozen:
        snapshot.stopEnabled = true;
        snapshot.templateCaptureEnabled = true;
        snapshot.saveTemplateEnabled = true;
        snapshot.statusText = QStringLiteral(
                    "\u6a21\u677f\u5236\u4f5c\u4e2d\uff1a\u8bf7\u5b8c\u6210\u6846\u9009\u5e76\u4fdd\u5b58");
        break;
    }
    return snapshot;
}
