#include "ui/presenters/inspection_fault_presenter.h"

#include <QStringList>

namespace {

QString faultReasonText(InspectionFaultReason reason)
{
    switch (reason) {
    case InspectionFaultReason::PlcDisconnected:
        return QStringLiteral("PLC\u8fde\u63a5\u6216\u7ed3\u679c\u8f93\u51fa\u5f02\u5e38");
    case InspectionFaultReason::HardTriggerQueueOverflow:
        return QStringLiteral("\u786c\u89e6\u53d1\u68c0\u6d4b\u961f\u5217\u5df2\u6ee1");
    case InspectionFaultReason::ProductIdentityAmbiguous:
        return QStringLiteral("\u4ea7\u54c1\u8eab\u4efd\u65e0\u6cd5\u552f\u4e00\u786e\u5b9a");
    case InspectionFaultReason::RuntimeInvariantViolation:
        return QStringLiteral("\u68c0\u6d4b\u8fd0\u884c\u7ea6\u675f\u88ab\u7834\u574f");
    case InspectionFaultReason::None:
        break;
    }
    return QString();
}

}

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

    const QString reasonText = faultReasonText(snapshot.reason);
    presentation.statusText = QStringLiteral(
                "\u7cfb\u7edf\u6545\u969c\uff1a\u68c0\u6d4b\u5df2\u6682\u505c");
    presentation.resultText = QStringLiteral("\u7cfb\u7edf\u6545\u969c");
    presentation.statusStyleSheet = QStringLiteral(
                "QLabel{color:#d32f2f; background:#ffebee;"
                " font-weight:bold; border:2px solid #d32f2f;"
                " padding:6px;}");
    presentation.resultStyleSheet = QStringLiteral(
                "QLabel{color:#d32f2f; background:#ffebee;"
                " font-weight:bold; font-size:32px;"
                " border:2px solid #d32f2f;}");

    QStringList details;
    details << QStringLiteral("\u6545\u969c\u539f\u56e0\uff1a%1").arg(reasonText);
    if (!snapshot.diagnostic.isEmpty()) {
        details << QStringLiteral("\u8bca\u65ad\u4fe1\u606f\uff1a%1")
                   .arg(snapshot.diagnostic);
    }
    details << QStringLiteral(
                   "\u672c\u6b21\u8fd0\u884c\u5df2\u63a5\u6536 %1 \u4ef6\uff0c\u5df2\u5b8c\u6210 %2 \u4ef6\u3002")
               .arg(snapshot.acceptedProductCount)
               .arg(snapshot.completedProductCount);
    details << QStringLiteral(
                   "\u8f93\u9001\u7ebf\u72b6\u6001\u672a\u77e5\uff0c\u8bf7\u7acb\u5373\u4f7f\u7528\u8f93\u9001\u7ebf\u81ea\u8eab\u63a7\u5236\u505c\u673a\uff0c"
                   "\u5e76\u9694\u79bb\u6545\u969c\u671f\u95f4\u7684\u4ea7\u54c1\u3002");
    details << QStringLiteral(
                   "\u786e\u8ba4\u73b0\u573a\u5df2\u5b89\u5168\u5904\u7406\u540e\uff0c\u70b9\u51fb\u3010\u786e\u8ba4\u6545\u969c\u5e76\u6062\u590d\u3011\u89e3\u9664\u8f6f\u4ef6\u9501\u5b9a\u3002");
    presentation.operatorMessage = details.join(QStringLiteral("\n\n"));
    return presentation;
}
