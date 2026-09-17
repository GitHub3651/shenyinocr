#pragma once

#include <QLabel>
#include <QSvgRenderer>

class QPaintEvent;

enum class DetectionVerdictViewStyle;

class VerdictResultLabel : public QLabel
{
public:
    explicit VerdictResultLabel(QWidget *parent = nullptr);

    void showVerdict(DetectionVerdictViewStyle verdictStyle);
    void clearVerdict();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QSvgRenderer m_svgRenderer;
    DetectionVerdictViewStyle m_verdictStyle;
    bool m_hasVerdict = false;
};
