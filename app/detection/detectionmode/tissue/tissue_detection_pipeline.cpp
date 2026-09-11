#include "tissue_detection_pipeline.h"
#include "contracts/detection_mode.h"

TissueDetectionPipeline::TissueDetectionPipeline(
        double roughnessThreshold)
    : m_detector(roughnessThreshold)
{
}
TissueRollResult TissueDetectionPipeline::detect(
        const cv::Mat &image) const
{
    return m_detector.processImage(image);
}

DetectionResult TissueDetectionPipeline::toDetectionResult(
    const TissueRollResult &tissueResult)
{
    DetectionResult result;
    result.modeId = detectionModeUiId(DetectionMode::Tissue);
    result.verdict = tissueResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.recognizedText = tissueResult.rollFound
            ? QStringLiteral("粗糙度：%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("粗糙度：--");
    result.presentationText = result.recognizedText;
    result.hasPresentationText = true;
    result.diagnostic = QString::fromStdString(tissueResult.message);

    if (tissueResult.rollFound) {
        DetectionOverlayEllipse outer;
        outer.role = QStringLiteral("tissue_outer");
        outer.center = tissueResult.roll.center;
        outer.axes = tissueResult.roll.outerAxes;
        result.overlay.ellipses.push_back(outer);

        DetectionOverlayEllipse inner;
        inner.role = QStringLiteral("tissue_inner");
        inner.center = tissueResult.roll.innerCenter;
        inner.axes = tissueResult.roll.innerAxes;
        result.overlay.ellipses.push_back(inner);
    }
    return result;
}
