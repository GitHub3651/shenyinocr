#pragma once

#include "detection/common/detection_pose.h"

#include <opencv2/core.hpp>

#include <vector>

class TrackingPoseMatcher
{
public:
    bool init(const cv::Mat &trackingTemplateBgr);
    void clear();
    bool isReady() const;
    bool prepareFrame(
        const cv::Mat &frameBgr,
        cv::Mat *gray,
        cv::Mat *smallGray) const;
    DetectionPose matchPrepared(
        const cv::Mat &gray,
        const cv::Mat &smallGray,
        const std::vector<cv::Point2f> &relativeDatePolygon) const;
    DetectionPose match(
        const cv::Mat &frameBgr,
        const std::vector<cv::Point2f> &relativeDatePolygon) const;

private:
    cv::Mat m_templateGray;
    std::vector<cv::Mat> m_rotatedTemplates;
    std::vector<int> m_rotatedAngles;
    std::vector<cv::Mat> m_smallRotatedTemplates;
    double m_pyramidScale = 0.2;
};
