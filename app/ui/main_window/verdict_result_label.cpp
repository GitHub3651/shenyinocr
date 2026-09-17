#include "ui/main_window/verdict_result_label.h"

#include "contracts/inspection_presentation.h"

#include <QPainter>

VerdictResultLabel::VerdictResultLabel(QWidget *parent)
    : QLabel(parent),
      m_verdictStyle(DetectionVerdictViewStyle::Error)
{
}

void VerdictResultLabel::showVerdict(
    DetectionVerdictViewStyle verdictStyle)
{
    if (m_hasVerdict && m_verdictStyle == verdictStyle) {
        return;
    }

    m_verdictStyle = verdictStyle;
    m_svgRenderer.load(
                m_verdictStyle == DetectionVerdictViewStyle::Correct
                ? QStringLiteral(":/svg/verdict_correct.svg")
                : QStringLiteral(":/svg/verdict_wrong.svg"));
    m_hasVerdict = true;
    update();
}

void VerdictResultLabel::clearVerdict()
{
    if (!m_hasVerdict) {
        return;
    }
    m_hasVerdict = false;
    update();
}

void VerdictResultLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    if (!m_hasVerdict) {
        return;
    }

    const qreal iconSize = width() * 0.7;
    const QRectF iconRect(
                (width() - iconSize) * 0.5,
                (height() - iconSize) * 0.5,
                iconSize,
                iconSize);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    m_svgRenderer.render(&painter, iconRect);
}
