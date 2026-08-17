#pragma once

#include "detection/positioning/detection_pose.h"
#include "runtime/inspection_presentation.h"

#include <QImage>

#include <functional>

struct TissueRollPresentation
{
    cv::Point2f center;
    cv::Size2f outerAxes;
    cv::Point2f innerCenter;
    cv::Size2f innerAxes;
};

struct InspectionPresentationRenderState
{
    DetectionPose pose;
    std::vector<DetectionOverlayPolygon> detailPolygons;
    bool stampIsOverlap = false;
    TissueRollPresentation tissueRoll;
    bool hasTissueRoll = false;
};

struct InspectionPresentationViewBindings
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

class InspectionPresentationRenderer
{
public:
    void bindView(const InspectionPresentationViewBindings &bindings);
    bool hasViewBindings() const;

    void clear();
    void clearTransientView();
    bool present(const InspectionPresentation &snapshot);
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

    const InspectionPresentationRenderState &state() const;
    QImage renderFrame(
        const cv::Mat &image,
        bool includeTissueOverlay) const;

private:
    InspectionPresentationRenderState m_state;
    InspectionPresentationViewBindings m_viewBindings;
    ProductKey m_lastPresentedProductKey;
};
