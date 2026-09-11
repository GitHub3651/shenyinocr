#ifndef DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
#define DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H

#include "detection/detectionmode/tissue/tissue_roll_detector.h"
#include "detection/common/detection_pose.h"

class TissueDetectionPipeline
{
public:
    explicit TissueDetectionPipeline(double roughnessThreshold);

    TissueRollResult detect(const cv::Mat &image) const;
    static DetectionResult toDetectionResult(
        const TissueRollResult &tissueResult);

private:
    TissueRollDetector m_detector;
};

#endif // DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
