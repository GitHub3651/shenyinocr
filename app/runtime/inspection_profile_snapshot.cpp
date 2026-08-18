// 文件作用：本文件用于把配方Profile准备为检测线程可直接读取的只读运行快照。
// 主要职责：把配方Profile准备为检测线程可直接读取的只读运行快照。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/inspection_profile_snapshot.h"

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
InspectionProfileSnapshot InspectionProfileSnapshotBuilder::create(
        const std::vector<InspectionProfileSource> &sources)
{
    InspectionProfileSnapshot snapshot;
    snapshot.trackingProfiles.reserve(sources.size());
    snapshot.detectionProfiles.reserve(sources.size());

    for (int index = 0;
         index < static_cast<int>(sources.size());
         ++index) {
        const InspectionProfileSource &source =
                sources[static_cast<std::size_t>(index)];
        const QString templateName = source.name;

        WordTrackingProfile trackingProfile;
        trackingProfile.name = templateName;
        trackingProfile.profileIndex = index;
        trackingProfile.trackingTemplate =
                source.trackingTemplate.clone();
        trackingProfile.barcodePoly = source.barcodePoly;
        trackingProfile.datePoly = source.datePoly;
        snapshot.trackingProfiles.push_back(trackingProfile);

        DetectionModeWorkerProfile detectionProfile;
        detectionProfile.templateName = templateName;
        detectionProfile.targetText = source.targetText;
        detectionProfile.preparedTemplates =
                CharacterGlyphMatcher::prepare(
                    cloneImages(source.digitTemplates));
        detectionProfile.templateTargetIndexes =
                source.digitTemplateTargetIndexes;
        detectionProfile.thresholdPercent = source.imageThreshold;
        detectionProfile.barcodeOptions = source.barcodeOptions;
        detectionProfile.decodeStrategy = source.decodeStrategy;
        snapshot.detectionProfiles.push_back(detectionProfile);
    }

    return snapshot;
}
