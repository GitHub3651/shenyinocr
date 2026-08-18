// 文件作用：本文件用于把运行时故障快照转换为界面可显示的状态文字和样式。
// 主要职责：把运行时故障快照转换为界面可显示的状态文字和样式。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/presenters/inspection_fault_presenter.h"

#include <QStringList>

// 函数说明：isValid 函数检查相关状态并返回判断结果。
bool InspectionFaultPresentation::isValid() const
{
    return !statusText.isEmpty()
            && !resultText.isEmpty()
            && !operatorMessage.isEmpty();
}

// 函数说明：create 函数创建、准备或启动对应流程。
InspectionFaultPresentation InspectionFaultPresenter::create(
    const ApplicationFaultSnapshot &snapshot)
{
    InspectionFaultPresentation presentation;
    if (!snapshot.isActive()) {
        return presentation;
    }

    presentation.statusText = QStringLiteral("视觉检测已暂停");
    presentation.resultText = QStringLiteral("检测暂停");
    presentation.statusStyleSheet = QStringLiteral(
                "QLabel{color:#d32f2f; background:#ffebee;"
                " font-weight:bold; border:2px solid #d32f2f;"
                " padding:6px;}");
    presentation.resultStyleSheet = QStringLiteral(
                "QLabel{color:#d32f2f; background:#ffebee;"
                " font-weight:bold; font-size:32px;"
                " border:2px solid #d32f2f;}");

    QStringList details;
    details << QStringLiteral("\u6545\u969c\u539f\u56e0\uff1a%1")
               .arg(snapshot.reasonText);
    if (!snapshot.diagnostic.isEmpty()) {
        details << QStringLiteral("\u8bca\u65ad\u4fe1\u606f\uff1a%1")
                   .arg(snapshot.diagnostic);
    }
    details << QStringLiteral(
                   "\u672c\u6b21\u8fd0\u884c\u5df2\u63a5\u6536 %1 \u4ef6\uff0c\u5df2\u5b8c\u6210 %2 \u4ef6\u3002")
               .arg(snapshot.acceptedProductCount)
               .arg(snapshot.completedProductCount);
    details << QStringLiteral(
                   "输送线状态未知，请使用输送线自身控制确认停线，"
                   "并隔离故障期间的产品。");
    details << QStringLiteral(
                   "\u786e\u8ba4\u73b0\u573a\u5df2\u5b89\u5168\u5904\u7406\u540e\uff0c\u70b9\u51fb\u3010\u786e\u8ba4\u6545\u969c\u5e76\u6062\u590d\u3011\u89e3\u9664\u8f6f\u4ef6\u9501\u5b9a\u3002");
    presentation.operatorMessage = details.join(QStringLiteral("\n\n"));
    return presentation;
}
