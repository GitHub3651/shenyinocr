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
    case OperationUiState::Detecting:
        return QStringLiteral("当前正在识别，请先停止识别。");
    case OperationUiState::Stopping:
        return QStringLiteral("系统正在停止，请稍候。");
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
                || state == OperationUiState::TemplatePreviewing
                || state == OperationUiState::TemplateFrozen,
                state == OperationUiState::Stopping
                ? QStringLiteral("系统正在停止，请稍候。")
                : QStringLiteral("当前没有需要停止的任务。"));
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
            state == OperationUiState::Stopping
            ? QStringLiteral("停止中...")
            : (templateOperation
               ? QStringLiteral("退出模板制作")
               : QStringLiteral("停止识别"));
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
