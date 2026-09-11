#pragma once

#include "detection/common/detection_pose.h"

#include <QImage>

class InspectionPresentationRenderer
{
public:
    static QImage renderRawFrame(const cv::Mat &image);
    static QImage renderDetectionFrame(
        const cv::Mat &image,
        const DetectionOverlay &overlay);
};
