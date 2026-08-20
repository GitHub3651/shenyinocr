// 文件作用：本文件用于执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 主要职责：执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
#define DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H

#include "detection/tissue/tissue_roll_detector.h"
#include "detection/positioning/detection_pose.h"

// 组件说明：TissueDetectionPipeline 组件提供对应设备或检测能力的统一实现。
class TissueDetectionPipeline
{
public:
    explicit TissueDetectionPipeline(double roughnessThreshold);

    TissueRollResult detect(const cv::Mat &image) const;
    static DetectionResult toDetectionResult(
        const TissueRollResult &tissueResult);
    double roughnessThreshold() const;

private:
    TissueRollDetector m_detector;
};

#endif // DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
