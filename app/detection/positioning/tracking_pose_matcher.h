// 文件作用：本文件用于通过模板匹配计算跟踪区域的位置、角度和变换矩阵。
// 主要职责：通过模板匹配计算跟踪区域的位置、角度和变换矩阵。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"

#include <opencv2/core.hpp>

#include <vector>

// 组件说明：TrackingPoseMatcher 组件提供对应设备或检测能力的统一实现。
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
