#ifndef DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H
#define DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H

#include <QString>

#include <opencv2/core.hpp>

#include <functional>
#include <vector>

struct StampOverlapResult
{
    bool isOk = false;
    std::vector<cv::Point> finalStampPoly;
};

struct StampDetectionResult
{
    int targetCharacterCount = 0;
    int detectedCharacterCount = 0;
    bool characterIsOk = false;
    bool overlapIsOk = false;
    bool isOk = false;
    std::vector<cv::Point> finalStampPoly;
};

class StampDetectionPipeline
{
public:
    typedef std::function<int(cv::Mat &dateRoi)> CharacterMatchFunction;
    typedef std::function<StampOverlapResult(
            const cv::Mat &sourceImage,
            const std::vector<cv::Point> &datePoly)>
            OverlapDetectionFunction;

    StampDetectionResult detect(
            cv::Mat &dateRoi,
            const cv::Mat &sourceImage,
            const std::vector<cv::Point> &datePoly,
            const QString &targetText,
            const CharacterMatchFunction &matchCharacters,
            const OverlapDetectionFunction &detectOverlap) const;
};

#endif // DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H
