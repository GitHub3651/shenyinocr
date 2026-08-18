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
            ? QStringLiteral("采集中...")
            : (state == OperationUiState::Stopping
               ? QStringLiteral("停止中...")
               : QStringLiteral("启动识别"));

    const bool templateOperation =
            state == OperationUiState::TemplatePreviewing
            || state == OperationUiState::TemplateFrozen;
    snapshot.stopText =
            state == OperationUiState::Fault
            ? QStringLiteral("确认故障并恢复")
            : (state == OperationUiState::Stopping
            ? QStringLiteral("停止中...")
            : (templateOperation
               ? QStringLiteral("退出模板制作")
               : QStringLiteral("停止识别")));
    snapshot.templateCaptureText =
            state == OperationUiState::TemplatePreviewing
            ? QStringLiteral("拍照并开始框选")
            : (state == OperationUiState::TemplateFrozen
               ? QStringLiteral("重新取景")
               : QStringLiteral("制作模板"));

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
                    "系统故障：检测已暂停");
        break;
    case OperationUiState::TemplatePreviewing:
        snapshot.stopEnabled = true;
        snapshot.templateCaptureEnabled = true;
        snapshot.statusText = QStringLiteral(
                    "模板制作中：实时取景");
        break;
    case OperationUiState::TemplateFrozen:
        snapshot.stopEnabled = true;
        snapshot.templateCaptureEnabled = true;
        snapshot.saveTemplateEnabled = true;
        snapshot.statusText = QStringLiteral(
                    "模板制作中：请完成框选并保存");
        break;
    }
    return snapshot;
}
