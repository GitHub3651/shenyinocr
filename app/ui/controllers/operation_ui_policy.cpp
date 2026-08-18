// 文件作用：本文件用于根据运行状态计算按钮文字、启用状态和可执行操作。
// 主要职责：根据运行状态计算按钮文字、启用状态和可执行操作。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "operation_ui_policy.h"

// 函数说明：create 函数创建、准备或启动对应流程。
namespace {

OperationUiSnapshot::Access access(
    bool enabled,
    const QString &disabledReason = QString())
{
    OperationUiSnapshot::Access result;
    result.enabled = enabled;
    if (!enabled) {
        result.disabledReason = disabledReason;
    }
    return result;
}

QString operationBusyReason(OperationUiState state)
{
    switch (state) {
    case OperationUiState::Detecting:
        return QStringLiteral("当前正在识别，请先停止识别。");
    case OperationUiState::Stopping:
        return QStringLiteral("系统正在停止，请稍候。");
    case OperationUiState::Fault:
        return QStringLiteral("请先确认故障并恢复。");
    case OperationUiState::TemplatePreviewing:
    case OperationUiState::TemplateFrozen:
        return QStringLiteral("请先退出模板制作。");
    case OperationUiState::CameraClosed:
    case OperationUiState::CameraReady:
    default:
        return QString();
    }
}

} // namespace

OperationUiSnapshot OperationUiPolicy::create(
    const OperationUiContext &context)
{
    OperationUiSnapshot snapshot;
    const OperationUiState state = context.state;
    const bool idle = state == OperationUiState::CameraClosed
            || state == OperationUiState::CameraReady;
    const QString busyReason = operationBusyReason(state);

    snapshot.cameraOpen = context.cameraOpen;
    snapshot.plcConnected = context.plcConnected;
    snapshot.openCamera = access(
                state == OperationUiState::CameraClosed
                && !context.cameraOpen,
                context.cameraOpen
                ? QStringLiteral("相机已打开。") : busyReason);
    snapshot.startDetection = access(
                state == OperationUiState::CameraReady
                && context.cameraOpen,
                context.cameraOpen
                ? busyReason
                : QStringLiteral("请先打开相机。"));
    snapshot.closeCamera = access(
                state == OperationUiState::CameraReady
                && context.cameraOpen,
                context.cameraOpen
                ? busyReason : QStringLiteral("相机未打开。"));
    snapshot.templateCapture = access(
                context.cameraOpen
                && (state == OperationUiState::CameraReady
                    || state == OperationUiState::TemplatePreviewing
                    || state == OperationUiState::TemplateFrozen),
                context.cameraOpen
                ? busyReason
                : QStringLiteral("请先打开相机。"));
    snapshot.saveTemplate = access(
                state == OperationUiState::TemplateFrozen,
                QStringLiteral("请先获取并冻结模板画面。"));
    snapshot.stop = access(
                state == OperationUiState::Detecting
                || state == OperationUiState::Fault
                || state == OperationUiState::TemplatePreviewing
                || state == OperationUiState::TemplateFrozen,
                state == OperationUiState::Stopping
                ? QStringLiteral("系统正在停止，请稍候。")
                : QStringLiteral("当前没有需要停止的任务。"));
    snapshot.generalSettings = access(idle, busyReason);
    snapshot.cameraSettings = access(
                state == OperationUiState::CameraReady
                && context.cameraOpen,
                context.cameraOpen
                ? busyReason
                : QStringLiteral("请先打开相机。"));
    snapshot.plcConnection = access(
                idle && !context.plcConnected,
                !idle ? busyReason : QStringLiteral(
                    "PLC 已连接。如需修改连接参数，请先断开 PLC。"));
    snapshot.plcRuntime = access(
                idle && context.plcConnected,
                !idle ? busyReason : QStringLiteral("请先连接 PLC。"));
    snapshot.recipeSelection = access(idle, busyReason);
    snapshot.recipeEditing = access(idle, busyReason);
    snapshot.statisticsReset = access(idle, busyReason);
    snapshot.rejectQueueReset = access(idle, busyReason);
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
        break;
    case OperationUiState::CameraReady:
        break;
    case OperationUiState::Detecting:
    case OperationUiState::Stopping:
        break;
    case OperationUiState::Fault:
        snapshot.statusText = QStringLiteral(
                    "系统故障：检测已暂停");
        break;
    case OperationUiState::TemplatePreviewing:
        snapshot.statusText = QStringLiteral(
                    "模板制作中：实时取景");
        break;
    case OperationUiState::TemplateFrozen:
        snapshot.statusText = QStringLiteral(
                    "模板制作中：请完成框选并保存");
        break;
    }
    return snapshot;
}
