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

DetectionResult TissueDetectionPipeline::toDetectionResult(
    const TissueRollResult &tissueResult)
{
    DetectionResult result;
    result.modeId = QStringLiteral("tissue_detection");
    result.verdict = tissueResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = tissueResult.rollFound
            ? QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a--");
    result.diagnostic = QString::fromStdString(tissueResult.message);
    result.elapsedMs = static_cast<double>(
                tissueResult.processingTimeMs);

    if (tissueResult.rollFound) {
        const cv::Rect &box = tissueResult.roll.outerBbox;
        DetectionOverlayPolygon polygon;
        polygon.role = QStringLiteral("tissue_roll");
        polygon.points = {
            cv::Point(box.x, box.y),
            cv::Point(box.x + box.width, box.y),
            cv::Point(box.x + box.width, box.y + box.height),
            cv::Point(box.x, box.y + box.height)
        };
        polygon.score = tissueResult.roll.roughnessScore;
        result.overlay.polygons.push_back(polygon);
    }
    return result;
}

double TissueDetectionPipeline::roughnessThreshold() const
{
    return m_detector.roughnessThreshold();
}
