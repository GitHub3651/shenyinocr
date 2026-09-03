// 文件作用：本文件用于把运行时故障快照转换为界面可显示的故障文字。
// 主要职责：把运行时故障快照转换为状态、结果和操作员提示文字。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
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
        reasonText = QStringLiteral("相机断连或正式采集异常");
        break;
    case InspectionFaultReason::PlcDisconnected:
        reasonText = QStringLiteral("PLC连接或结果输出异常");
        break;
    case InspectionFaultReason::HardTriggerQueueOverflow:
        reasonText = QStringLiteral("硬触发检测队列已满");
        break;
    case InspectionFaultReason::ProductIdentityAmbiguous:
        reasonText = QStringLiteral("产品身份无法唯一确定");
        break;
    case InspectionFaultReason::RuntimeInvariantViolation:
        reasonText = QStringLiteral("检测运行约束被破坏");
        break;
    case InspectionFaultReason::BarcodeCsvUnavailable:
        reasonText = QStringLiteral("二维码 CSV 写入不可用");
        break;
    case InspectionFaultReason::None:
        break;
    }
    details << QStringLiteral("故障原因：%1").arg(reasonText);
    if (!snapshot.diagnostic.isEmpty()) {
        details << QStringLiteral("诊断信息：%1")
                   .arg(snapshot.diagnostic);
    }
    details << QStringLiteral(
                   "本次运行已接收 %1 件，已完成 %2 件。")
               .arg(snapshot.acceptedProductCount)
               .arg(snapshot.completedProductCount);
    details << QStringLiteral(
                   "输送线状态未知，请使用输送线自身控制确认停线，"
                   "并隔离故障期间的产品。");
    details << QStringLiteral(
                   "确认现场已安全处理后，点击【确认故障并恢复】解除软件锁定。");
    presentation.operatorMessage = details.join(QStringLiteral("\n\n"));
    return presentation;
}
