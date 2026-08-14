#pragma once

#include "TrackingTypes.h"

#include <QImage>

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

class DetectionResultPresenter
{
public:
    void clear();

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
};
