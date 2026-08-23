// 文件作用：本文件用于根据当前模板选择定位方式，并输出检测区域对应的位置姿态。
// 主要职责：根据当前模板选择定位方式，并输出检测区域对应的位置姿态。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/common/detection_pose.h"
#include "detection/common/tracking_pose_matcher.h"
#include "contracts/detection_mode.h"

#include <opencv2/core.hpp>

#include <vector>

// 组件说明：InspectionPositioner 组件提供对应设备或检测能力的统一实现。
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
