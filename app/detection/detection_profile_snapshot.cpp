// 文件作用：本文件用于把配方Profile准备为检测线程可直接读取的只读运行快照。
// 主要职责：把配方Profile准备为检测线程可直接读取的只读运行快照。
// 模块位置：检测层；把PreparedRecipe转换为检测线程只读快照。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "detection/detection_profile_snapshot.h"

namespace {

// 函数说明：cloneImages 函数实现名称所表示的处理步骤。
std::vector<cv::Mat> cloneImages(
        const std::vector<cv::Mat> &images)
{
    std::vector<cv::Mat> clones;
    clones.reserve(images.size());
    for (const cv::Mat &image : images) {
        clones.push_back(image.clone());
    }
    return clones;
}

} // namespace

// 函数说明：create 函数创建、准备或启动对应流程。
DetectionProfileSnapshot DetectionProfileSnapshotBuilder::create(
        const PreparedRecipe &preparedRecipe)
{
    DetectionProfileSnapshot snapshot;
    snapshot.trackingProfiles.reserve(preparedRecipe.profiles.size());
    snapshot.detectionProfiles.reserve(preparedRecipe.profiles.size());

    for (int index = 0;
         index < preparedRecipe.profiles.size();
         ++index) {
        const PreparedRecipeProfile &source =
                preparedRecipe.profiles.at(index);
        const QString templateName = source.definition.name;

        WordTrackingProfile trackingProfile;
        trackingProfile.name = templateName;
        trackingProfile.profileIndex = index;
        trackingProfile.trackingTemplate =
                source.trackingTemplate.clone();
        trackingProfile.barcodePoly = source.barcodePolygon;
        trackingProfile.datePoly = source.datePolygon;
        snapshot.trackingProfiles.push_back(trackingProfile);

        DetectionModeWorkerProfile detectionProfile;
        detectionProfile.templateName = templateName;
        detectionProfile.targetText = source.definition.targetText;
        detectionProfile.preparedTemplates =
                CharacterGlyphMatcher::prepare(
                    cloneImages(source.characterTemplates));
        detectionProfile.templateTargetIndexes =
                source.characterTemplateTargetIndexes;
        detectionProfile.thresholdPercent =
                source.definition.imageThresholdPercent;
        detectionProfile.barcodeOptions.formatMask =
                source.definition.barcodeParameters.formatMask;
        detectionProfile.barcodeOptions.roiPaddingPercent =
                source.definition.barcodeParameters.roiPaddingPercent;
        detectionProfile.barcodeOptions.maxDecodeTimeMs =
                source.definition.barcodeParameters.maxDecodeTimeMs;
        detectionProfile.barcodeOptions.enableFallback =
                source.definition.barcodeParameters.enableFallback;
        snapshot.detectionProfiles.push_back(detectionProfile);
    }

    return snapshot;
}
