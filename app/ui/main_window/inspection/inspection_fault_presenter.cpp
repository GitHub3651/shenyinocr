#include "ui/main_window/inspection/inspection_fault_presenter.h"

#include <QStringList>

bool InspectionFaultPresentation::isValid() const
{
    return !statusText.isEmpty()
            && !resultText.isEmpty()
            && !operatorMessage.isEmpty();
}

InspectionFaultPresentation InspectionFaultPresenter::create(
    const InspectionFaultSnapshot &snapshot)
{
    InspectionFaultPresentation presentation;
    if (!snapshot.isActive()) {
        return presentation;
    }

    presentation.statusText = QStringLiteral("视觉检测已暂停");
    presentation.resultText = QStringLiteral("检测暂停");

    QStringList details;
    QString reasonText;
    switch (snapshot.reason) {
    case InspectionFaultReason::CameraDisconnected:
        reasonText = QStringLiteral("相机连接或图像采集异常");
        break;
    case InspectionFaultReason::PlcDisconnected:
        reasonText = QStringLiteral("PLC 连接或检测结果发送异常");
        break;
    case InspectionFaultReason::HardTriggerQueueOverflow:
        reasonText = QStringLiteral("待检测图像过多，系统已暂停");
        break;
    case InspectionFaultReason::ProductIdentityAmbiguous:
        reasonText = QStringLiteral("无法确定当前图像对应的产品");
        break;
    case InspectionFaultReason::RuntimeInvariantViolation:
        reasonText = QStringLiteral("系统状态异常，检测已暂停");
        break;
    case InspectionFaultReason::BarcodeCsvUnavailable:
        reasonText = QStringLiteral("二维码结果无法保存到本机");
        break;
    case InspectionFaultReason::None:
        break;
    }
    details << QStringLiteral("故障原因：%1").arg(reasonText);
    details << QStringLiteral(
                   "本次运行已接收 %1 件，已完成 %2 件。")
               .arg(snapshot.acceptedProductCount)
               .arg(snapshot.completedProductCount);
    details << QStringLiteral(
                   "输送线状态未知，请使用输送线自身控制确认停线，"
                   "并隔离故障期间的产品。");
    details << QStringLiteral(
                   "确认现场已安全处理后，点击【确认故障并恢复】恢复检测操作。");
    presentation.operatorMessage = details.join(QStringLiteral("\n\n"));
    return presentation;
}
