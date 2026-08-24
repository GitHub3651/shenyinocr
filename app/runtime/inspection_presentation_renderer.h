// 文件作用：本文件用于把检测结果、原图和覆盖图形组合成统一的界面呈现数据。
// 主要职责：把检测结果、原图和覆盖图形组合成统一的界面呈现数据。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/common/detection_pose.h"

#include <QImage>

// 组件说明：InspectionPresentationRenderer 组件封装本文件中与其名称对应的单一职责。
class InspectionPresentationRenderer
{
public:
    static QImage renderRawFrame(const cv::Mat &image);
    static QImage renderDetectionFrame(
        const cv::Mat &image,
        const DetectionOverlay &overlay);
};
