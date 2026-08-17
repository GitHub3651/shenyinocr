#ifndef DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
#define DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H

#include "detection/tissue/tissue_roll_detector.h"
#include "detection/positioning/detection_pose.h"
#include "recipes/product_recipe.h"

class TissueDetectionPipeline
{
public:
    explicit TissueDetectionPipeline(
            const TissueRecipeParameters &parameters);

    TissueRollResult detect(const cv::Mat &image) const;
    static DetectionResult toDetectionResult(
        const TissueRollResult &tissueResult);
    double roughnessThreshold() const;

private:
    TissueRollDetector m_detector;
};

#endif // DETECTION_TISSUE_TISSUE_DETECTION_PIPELINE_H
