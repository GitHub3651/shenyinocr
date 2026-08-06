#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "TrackingTypes.h"

class TrackingPoseMatcher
{
public:
    bool init(const cv::Mat& trackingTemplateBgr);
    void clear();
    bool isReady() const;
    bool prepareFrame(const cv::Mat& frameBgr,
                      cv::Mat* gray,
                      cv::Mat* smallGray) const;
    DetectionPose matchPrepared(const cv::Mat& gray,
                                const cv::Mat& smallGray,
                                const std::vector<cv::Point2f>& relDatePoly) const;
    DetectionPose match(const cv::Mat& frameBgr,
                        const std::vector<cv::Point2f>& relDatePoly) const;

private:
    cv::Mat templateGray;
    std::vector<cv::Mat> preRotatedTemplates;
    std::vector<int> preRotatedAngles;
    std::vector<cv::Mat> preRotatedTemplatesSmall;
    double pyramidScale = 0.2;
};
