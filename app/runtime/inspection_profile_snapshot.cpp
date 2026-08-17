#include "runtime/inspection_profile_snapshot.h"

namespace {

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
