// 文件作用：实现多模板定位和字符检测快照的构造。
#include "detection/multi_template_runtime_snapshot.h"

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

MultiTemplateRuntimeSnapshot MultiTemplateRuntimeSnapshotBuilder::create(
    const QVector<PreparedTemplateSnapshot> &templates)
{
    MultiTemplateRuntimeSnapshot snapshot;
    snapshot.trackingTemplates.reserve(
                static_cast<std::size_t>(templates.size()));
    snapshot.runtimeConfigs.reserve(
                static_cast<std::size_t>(templates.size()));

    for (int sourceIndex = 0; sourceIndex < templates.size(); ++sourceIndex) {
        const PreparedTemplateSnapshot &source = templates.at(sourceIndex);
        if (!source) {
            continue;
        }
        const int runtimeIndex = static_cast<int>(
                    snapshot.runtimeConfigs.size());
        WordTrackingTemplate tracking;
        tracking.name = source->displayName;
        tracking.templateIndex = runtimeIndex;
        tracking.trackingTemplate = source->trackingTemplate.clone();
        tracking.barcodePoly = source->barcodePolygon;
        tracking.datePoly = source->datePolygon;
        snapshot.trackingTemplates.push_back(tracking);

        MultiTemplateRuntimeConfig runtimeConfig;
        runtimeConfig.templateName = source->displayName;
        runtimeConfig.targetUnits = source->targetUnits;
        runtimeConfig.preparedTemplates = CharacterGlyphMatcher::prepare(
                    cloneImages(source->characterTemplates));
        runtimeConfig.templateTargetIndexes =
                source->characterTemplateTargetIndexes;
        runtimeConfig.thresholdPercent =
                source->settings.imageThresholdPercent;
        runtimeConfig.barcodeOptions.formatMask =
                source->settings.barcodeParameters.formatMask;
        runtimeConfig.barcodeOptions.roiPaddingPercent =
                source->settings.barcodeParameters.roiPaddingPercent;
        runtimeConfig.barcodeOptions.maxDecodeTimeMs =
                source->settings.barcodeParameters.maxDecodeTimeMs;
        runtimeConfig.barcodeOptions.enableFallback =
                source->settings.barcodeParameters.enableFallback;
        snapshot.runtimeConfigs.push_back(runtimeConfig);
    }
    return snapshot;
}
