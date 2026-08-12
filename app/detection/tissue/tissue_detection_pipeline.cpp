#include "tissue_detection_pipeline.h"

TissueDetectionPipeline::TissueDetectionPipeline(
        const TissueRecipeParameters &parameters)
    : m_detector(parameters)
{
}

TissueRollResult TissueDetectionPipeline::detect(
        const cv::Mat &image) const
{
    return m_detector.processImage(image);
}

double TissueDetectionPipeline::roughnessThreshold() const
{
    return m_detector.roughnessThreshold();
}
