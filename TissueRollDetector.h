#ifndef TISSUEROLLDETECTOR_H
#define TISSUEROLLDETECTOR_H

#include <QMetaType>
#include <opencv2/opencv.hpp>
#include <string>

struct TissueRollConfig
{
    TissueRollConfig();
    explicit TissueRollConfig(double roughnessThresholdValue);

    double roughnessThreshold;
};

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
    int processingTimeMs = 0;
    TissueRollItem roll;
    std::string message;
};

Q_DECLARE_METATYPE(TissueRollResult)

class TissueRollDetector
{
public:
    explicit TissueRollDetector(const TissueRollConfig& config = TissueRollConfig());

    static void setDefaultRoughnessThreshold(double threshold);
    static double defaultRoughnessThreshold();

    TissueRollResult processImage(const cv::Mat& image) const;

private:
    TissueRollConfig m_config;
};

#endif // TISSUEROLLDETECTOR_H
