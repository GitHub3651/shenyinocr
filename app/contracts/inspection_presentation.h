// 文件作用：定义一次产品检测在界面上完整呈现所需的只读数据。
// 主要职责：提供运行时与界面之间稳定、纯数据的结果合同。
#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

enum class DetectionVerdictViewStyle
{
    Correct,
    Error
};

struct DetectionResultStatistics
{
    int totalCount = 0;
    int ngCount = 0;

    double passRatePercent() const
    {
        return totalCount > 0
                ? (1.0 - static_cast<double>(ngCount) / totalCount) * 100.0
                : 0.0;
    }
};

struct DetectionAbnormalStatistics
{
    quint64 systemFaultCount = 0;
    quint64 unconfirmedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
};

struct InspectionPresentation
{
    QImage image;
    DetectionVerdictViewStyle verdictStyle =
            DetectionVerdictViewStyle::Error;
    QString verdictText;
    QString recognitionText;
    bool updatesTemplateName = false;
    QString templateName;
    DetectionResultStatistics statistics;
    QString elapsedText;

    bool isValid() const
    {
        return !image.isNull();
    }
};

Q_DECLARE_METATYPE(InspectionPresentation)
