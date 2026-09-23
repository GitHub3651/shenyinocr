#include "operation_ui_policy.h"

#include <QVariant>
#include <QWidget>

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
    case OperationUiState::Starting:
        return QStringLiteral("系统正在启动识别，请稍候。");
    case OperationUiState::Detecting:
        return QStringLiteral("当前正在识别，请先停止识别。");
    case OperationUiState::Stopping:
        return QStringLiteral("系统正在停止，请稍候。");
    case OperationUiState::CameraPreviewing:
        return QStringLiteral("请先停止实时预览。");
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

void applyOperationUiAccess(
    QWidget *widget,
    const OperationUiSnapshot::Access &access,
    bool showDisabledReason)
{
    if (!widget) {
        return;
    }
    static const char originalToolTipProperty[] =
            "_operationOriginalToolTip";
    if (!widget->property(originalToolTipProperty).isValid()) {
        widget->setProperty(originalToolTipProperty, widget->toolTip());
    }

    widget->setEnabled(access.enabled);
    const QString originalToolTip =
            widget->property(originalToolTipProperty).toString();
    const bool showReason = !access.enabled
            && showDisabledReason
            && !access.disabledReason.isEmpty();
    widget->setToolTip(showReason
                       ? access.disabledReason
                       : originalToolTip);
}

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
    snapshot.cameraAction = access(
                (state == OperationUiState::CameraClosed
                 && !context.cameraOpen)
                || (state == OperationUiState::CameraReady
                    && context.cameraOpen),
                context.cameraOpen
                ? busyReason : QStringLiteral("请先等待当前操作结束。"));
    snapshot.inspectionAction = access(
                (state == OperationUiState::CameraReady
                 && context.cameraOpen)
                || state == OperationUiState::Detecting,
                context.cameraOpen
                ? busyReason
                : QStringLiteral("请先打开相机。"));
    snapshot.previewAction = access(
                state == OperationUiState::CameraReady
                || state == OperationUiState::CameraPreviewing,
                !context.cameraOpen
                ? QStringLiteral("请先打开相机。")
                : busyReason);
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
    snapshot.templateExit = access(
                state == OperationUiState::TemplatePreviewing
                || state == OperationUiState::TemplateFrozen,
                QStringLiteral("当前未进入模板制作。"));
    snapshot.generalSettings = access(idle, busyReason);
    snapshot.imageSettings = access(
                idle || state == OperationUiState::TemplateFrozen,
                busyReason);
    snapshot.cameraSettings = access(
                (state == OperationUiState::CameraReady
                 || state == OperationUiState::TemplateFrozen)
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
    snapshot.templateSelection = access(idle, busyReason);
    snapshot.templateEditing = access(idle, busyReason);
    snapshot.statisticsReset = access(idle, busyReason);
    snapshot.rejectQueueReset = access(idle, busyReason);
    snapshot.cameraActionText = context.cameraOpen
            ? QStringLiteral("关闭相机")
            : QStringLiteral("打开相机");
    if (state == OperationUiState::Starting) {
        snapshot.inspectionActionText = QStringLiteral("启动中");
    } else if (state == OperationUiState::Stopping) {
        snapshot.inspectionActionText = QStringLiteral("停止中");
    } else if (state == OperationUiState::Detecting) {
        snapshot.inspectionActionText = QStringLiteral("停止识别");
    } else {
        snapshot.inspectionActionText = QStringLiteral("启动识别");
    }
    snapshot.previewActionText =
            state == OperationUiState::CameraPreviewing
            ? QStringLiteral("停止预览")
            : QStringLiteral("预览画面");
    snapshot.templateCaptureText =
            state == OperationUiState::TemplatePreviewing
            ? QStringLiteral("拍照框选")
            : (state == OperationUiState::TemplateFrozen
               ? QStringLiteral("重新取景")
               : QStringLiteral("制作模板"));

    switch (state) {
    case OperationUiState::CameraClosed:
        snapshot.statusText = QStringLiteral("相机已关闭");
        snapshot.statusUiState = QStringLiteral("idle");
        break;
    case OperationUiState::CameraReady:
        snapshot.statusText = QStringLiteral("相机已打开");
        snapshot.statusUiState = QStringLiteral("ready");
        break;
    case OperationUiState::CameraPreviewing:
        snapshot.statusText = QStringLiteral("实时预览中");
        snapshot.statusUiState = QStringLiteral("running");
        break;
    case OperationUiState::Starting:
        snapshot.statusText = QStringLiteral("正在启动识别");
        snapshot.statusUiState = QStringLiteral("warning");
        break;
    case OperationUiState::Detecting:
        snapshot.statusText = context.hardwareTriggerEnabled
                ? QStringLiteral("硬触发模式运行中")
                : QStringLiteral("软触发模式运行中");
        snapshot.statusUiState = QStringLiteral("running");
        break;
    case OperationUiState::Stopping:
        snapshot.statusText = QStringLiteral("正在停止识别");
        snapshot.statusUiState = QStringLiteral("stopping");
        break;
    case OperationUiState::TemplatePreviewing:
        snapshot.statusText = QStringLiteral(
                    "模板制作中：实时取景");
        snapshot.statusUiState = QStringLiteral("warning");
        break;
    case OperationUiState::TemplateFrozen:
        snapshot.statusText = QStringLiteral(
                    "模板制作中：请完成框选并保存");
        snapshot.statusUiState = QStringLiteral("warning");
        break;
    }
    return snapshot;
}
