// 文件作用：实现多模板定位和字符检测快照的构造。
#include "detection/detection_template_snapshot.h"

namespace {

std::vector<cv::Mat> cloneImages(const std::vector<cv::Mat> &images)
{
    std::vector<cv::Mat> clones;
    clones.reserve(images.size());
    for (const cv::Mat &image : images) {
        clones.push_back(image.clone());
    }
    return clones;
}

}

DetectionTemplateSnapshot DetectionTemplateSnapshotBuilder::create(
    const QVector<PreparedTemplateSnapshot> &templates)
{
    DetectionTemplateSnapshot snapshot;
    snapshot.trackingTemplates.reserve(
                static_cast<std::size_t>(templates.size()));
    snapshot.detectionTemplates.reserve(
                static_cast<std::size_t>(templates.size()));

    for (int sourceIndex = 0; sourceIndex < templates.size(); ++sourceIndex) {
        const PreparedTemplateSnapshot &source = templates.at(sourceIndex);
        if (!source) {
            continue;
        }
        const int runtimeIndex = static_cast<int>(
                    snapshot.detectionTemplates.size());
        WordTrackingTemplate tracking;
        tracking.name = source->displayName;
        tracking.templateIndex = runtimeIndex;
        tracking.trackingTemplate = source->trackingTemplate.clone();
        tracking.barcodePoly = source->barcodePolygon;
        tracking.datePoly = source->datePolygon;
        snapshot.trackingTemplates.push_back(tracking);

        DetectionModeWorkerTemplate detection;
        detection.templateName = source->displayName;
        detection.targetText = source->settings.targetText;
        detection.preparedTemplates = CharacterGlyphMatcher::prepare(
                    cloneImages(source->characterTemplates));
        detection.templateTargetIndexes =
                source->characterTemplateTargetIndexes;
        detection.thresholdPercent =
                source->settings.imageThresholdPercent;
        detection.barcodeOptions.formatMask =
                source->settings.barcodeParameters.formatMask;
        detection.barcodeOptions.roiPaddingPercent =
                source->settings.barcodeParameters.roiPaddingPercent;
        detection.barcodeOptions.maxDecodeTimeMs =
                source->settings.barcodeParameters.maxDecodeTimeMs;
        detection.barcodeOptions.enableFallback =
                source->settings.barcodeParameters.enableFallback;
        snapshot.detectionTemplates.push_back(detection);
    }
    return snapshot;
}
