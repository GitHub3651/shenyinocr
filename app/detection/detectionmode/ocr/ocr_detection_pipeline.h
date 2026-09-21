#ifndef DETECTION_OCR_OCR_DETECTION_PIPELINE_H
#define DETECTION_OCR_OCR_DETECTION_PIPELINE_H

#include "detection/common/detection_pose.h"
#include "detection/detectionmode/ocr/deep_ocr_engine.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

struct OcrDetectionResult
{
    std::string recognizedText;
    std::vector<OcrRecognitionItem> items;
    bool isOk = false;
};

class OcrDetectionPipeline
{
public:
    OcrDetectionResult detect(
            cv::Mat &croppedImage,
            const std::string &targetText,
            DeepOcrEngine &ocrEngine) const;
    DetectionResult detect(
            const DetectionWorkItem &item,
            const std::string &targetText,
            DeepOcrEngine &ocrEngine) const;
    static DetectionResult toDetectionResult(
            const OcrDetectionResult &ocrResult,
            const DetectionPose &pose);
};

#endif // DETECTION_OCR_OCR_DETECTION_PIPELINE_H
