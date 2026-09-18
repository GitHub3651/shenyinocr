#ifndef DETECTION_OCR_OCR_DETECTION_PIPELINE_H
#define DETECTION_OCR_OCR_DETECTION_PIPELINE_H

#include "detection/common/detection_pose.h"
#include "engines/ocr/ocr_engine.h"

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
            IOcrEngine &ocrEngine) const;
    DetectionResult detect(
            const DetectionWorkItem &item,
            const std::string &targetText,
            IOcrEngine &ocrEngine) const;
    static DetectionResult toDetectionResult(
            const OcrDetectionResult &ocrResult,
            const DetectionPose &pose);
};

#endif // DETECTION_OCR_OCR_DETECTION_PIPELINE_H
