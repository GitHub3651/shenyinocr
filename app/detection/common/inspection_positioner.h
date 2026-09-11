#pragma once

#include "detection/common/detection_pose.h"
#include "detection/common/tracking_pose_matcher.h"
#include "contracts/detection_mode.h"

#include <opencv2/core.hpp>

#include <vector>

class InspectionPositioner
{
public:
    bool configure(
        DetectionTrackingKind trackingKind,
        const std::vector<WordTrackingTemplate> &templates,
        const std::vector<cv::Point2f> &singleDatePolygon,
        const cv::Mat &singleTrackingTemplate);
    DetectionPose locate(const cv::Mat &image) const;

private:
    // 每个状态对应一个已经严格加载的外部模板。
    struct TemplateState
    {
        QString name;
        int templateIndex = -1;
        std::vector<cv::Point2f> barcodePolygon;
        std::vector<cv::Point2f> datePolygon;
        TrackingPoseMatcher matcher;
    };

    DetectionTrackingKind m_trackingKind =
            DetectionTrackingKind::WholeFrame;
    std::vector<cv::Point2f> m_singleDatePolygon;
    TrackingPoseMatcher m_singleMatcher;
    std::vector<TemplateState> m_templates;
};
