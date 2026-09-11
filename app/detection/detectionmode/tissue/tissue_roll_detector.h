#ifndef DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H
#define DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H

#include <QMetaType>
#include <opencv2/opencv.hpp>
#include <string>

struct TissueRollItem
{
    bool isOk = true;
    double roughnessScore = 0.0;
    bool roughnessNg = false;
    int ringPixelCount = 0;
    cv::Point2f center;
    cv::Size2f outerAxes;
    cv::Rect outerBbox;
    cv::Point2f innerCenter;
    cv::Size2f innerAxes;
    bool innerHoleFound = false;
    std::string rejectReason;
};

struct TissueRollResult
{
    bool isOk = true;
    bool rollFound = false;
    int imageWidth = 0;
    int imageHeight = 0;
    TissueRollItem roll;
    std::string message;
};

Q_DECLARE_METATYPE(TissueRollResult)

class TissueRollDetector
{
public:
    explicit TissueRollDetector(double roughnessThreshold);

    TissueRollResult processImage(const cv::Mat& image) const;

private:
    double m_roughnessThreshold = 6.0;
};

#endif // DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H
