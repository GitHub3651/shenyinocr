#pragma once

#include "TrackingTypes.h"
#include "runtime/result_handler.h"

#include <QImage>

#include <functional>

struct TissueRollPresentation
{
    cv::Point2f center;
    cv::Size2f outerAxes;
    cv::Point2f innerCenter;
    cv::Size2f innerAxes;
};

struct DetectionPresentationState
{
    DetectionPose pose;
    std::vector<DetectionOverlayPolygon> detailPolygons;
    bool stampIsOverlap = false;
    TissueRollPresentation tissueRoll;
    bool hasTissueRoll = false;
};

enum class DetectionVerdictViewStyle
{
    Correct,
    Error
};

struct DetectionResultViewSnapshot
{
    ProductKey productKey;
    QImage image;
    DetectionVerdictViewStyle verdictStyle =
            DetectionVerdictViewStyle::Error;
    QString recognitionText;
    bool updatesTemplateName = false;
    QString templateName;
    DetectionResultStatistics statistics;
    QString elapsedText;

    bool isValid() const;
};

struct DetectionResultViewBindings
{
    std::function<void(const QImage &)> showImage;
    std::function<void(DetectionVerdictViewStyle)> showVerdictStyle;
    std::function<void(const QString &)> showVerdictText;
    std::function<void(const QString &)> showRecognitionText;
    std::function<void(const QString &)> showTemplateName;
    std::function<void(int)> showTotalCount;
    std::function<void(int)> showNgCount;
    std::function<void(double)> showPassRate;
    std::function<void(const QString &)> showElapsedText;

    bool isValid() const;
};

class DetectionResultPresenter
{
public:
    void bindView(const DetectionResultViewBindings &bindings);
    bool hasViewBindings() const;

    void clear();
    void clearTransientView();
    bool present(const DetectionResultViewSnapshot &snapshot);
    bool presentFrame(const QImage &image);
    void presentTotalAndNgCounts(int totalCount, int ngCount);
    void presentNgCount(int ngCount);
    const ProductKey &lastPresentedProductKey() const;

    void installDetectionResult(
        const DetectionResult &result,
        const DetectionPose &pose,
        bool stampIsOverlap = false);
    void installTissueRoll(
        const TissueRollPresentation &roll,
        bool hasTissueRoll);
    void updatePose(const DetectionPose &pose);

    const DetectionPresentationState &state() const;
    QImage renderFrame(
        const cv::Mat &image,
        bool includeTissueOverlay) const;

private:
    DetectionPresentationState m_state;
    DetectionResultViewBindings m_viewBindings;
    ProductKey m_lastPresentedProductKey;
};
