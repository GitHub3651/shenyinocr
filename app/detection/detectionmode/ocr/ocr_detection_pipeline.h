// 文件作用：本文件用于执行深度OCR模式的预处理、文字识别、目标比较和结果生成。
// 主要职责：执行深度OCR模式的预处理、文字识别、目标比较和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_OCR_OCR_DETECTION_PIPELINE_H
#define DETECTION_OCR_OCR_DETECTION_PIPELINE_H

#include "detection/common/detection_pose.h"
#include "engines/ocr/ocr_engine.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

// 组件说明：OcrDetectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct OcrDetectionResult
{
    std::string recognizedText;
    bool isOk = false;
};

// 组件说明：OcrDetectionPipeline 组件提供对应设备或检测能力的统一实现。
class OcrDetectionPipeline
{
public:
    OcrDetectionResult detect(
            cv::Mat &croppedImage,
            const std::string &targetText,
            IOcrEngine &ocrEngine) const;
    DetectionResult detect(
            const DetectionWorkItem &item,
            const std::string &targetText,
            IOcrEngine &ocrEngine) const;
    static DetectionResult toDetectionResult(
            const OcrDetectionResult &ocrResult,
            const DetectionPose &pose,
            double elapsedMs);
};

#endif // DETECTION_OCR_OCR_DETECTION_PIPELINE_H
