#pragma once

#include <QDateTime>
#include <QImage>
#include <QString>
#include <QtGlobal>

#include <functional>

enum class InspectionVerdictStyleDto
{
    Correct,
    Incorrect
};

struct InspectionViewBindingsDto
{
    std::function<void(const QImage &)> showImage;
    std::function<void(InspectionVerdictStyleDto)> showVerdictStyle;
    std::function<void(const QString &)> showVerdictText;
    std::function<void(const QString &)> showRecognitionText;
    std::function<void(const QString &)> showTemplateName;
    std::function<void(int)> showTotalCount;
    std::function<void(int)> showNgCount;
    std::function<void(double)> showPassRate;
    std::function<void(const QString &)> showElapsedText;

    bool isValid() const
    {
        return showImage && showVerdictStyle && showVerdictText
                && showRecognitionText && showTemplateName
                && showTotalCount && showNgCount
                && showPassRate && showElapsedText;
    }
};

struct InspectionUiCallbacks
{
    std::function<void()> warnMissingAnnotatedImage;
    std::function<void(quint64, const QString &)> reportImageSaveFailure;
    std::function<void(bool)> clearPreviousOverlay;
    std::function<void()> showDetectionRoiWarning;
    std::function<void()> clearDetectionRoiWarning;
};

struct ApplicationFaultSnapshot
{
    bool active = false;
    QString reasonText;
    QString diagnostic;
    QString runId;
    quint64 acceptedProductCount = 0;
    quint64 completedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
    QDateTime occurredAtUtc;

    bool isActive() const { return active; }
};
