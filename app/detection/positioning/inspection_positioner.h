#pragma once

#include "detection/positioning/detection_pose.h"
#include "detection/positioning/tracking_pose_matcher.h"

#include <opencv2/core.hpp>

#include <vector>

enum class InspectionTrackingKind {
    WholeFrame,
    SingleTemplate,
    WordProfiles
};

class InspectionPositioner
{
public:
    bool configure(
        InspectionTrackingKind trackingKind,
        const std::vector<WordTrackingProfile> &profiles,
        const std::vector<cv::Point2f> &singleDatePolygon,
        const cv::Mat &singleTrackingTemplate);
    DetectionPose locate(const cv::Mat &image) const;
    InspectionTrackingKind trackingKind() const;

private:
    struct ProfileState
    {
        QString name;
        int profileIndex = -1;
        std::vector<cv::Point2f> barcodePolygon;
        std::vector<cv::Point2f> datePolygon;
        TrackingPoseMatcher matcher;
    };

    InspectionTrackingKind m_trackingKind =
            InspectionTrackingKind::WholeFrame;
    std::vector<cv::Point2f> m_singleDatePolygon;
    TrackingPoseMatcher m_singleMatcher;
    std::vector<ProfileState> m_profiles;
};
