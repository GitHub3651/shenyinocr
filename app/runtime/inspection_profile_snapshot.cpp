#include "runtime/inspection_profile_snapshot.h"

#include <QDir>

namespace {

int resolvedThresholdPercent(
        double profileThreshold,
        const QString &fallbackThresholdText)
{
    int thresholdPercent = static_cast<int>(profileThreshold);
    bool thresholdValid = false;
    const int profileValue = QString::number(profileThreshold)
            .trimmed()
            .toInt(&thresholdValid);
    if (thresholdValid) {
        return profileValue;
    }

    const int fallbackValue = fallbackThresholdText
            .trimmed()
            .toInt(&thresholdValid);
    return thresholdValid ? fallbackValue : thresholdPercent;
}

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
        const std::vector<InspectionProfileSource> &sources,
        const QString &fallbackThresholdText)
{
    InspectionProfileSnapshot snapshot;
    snapshot.trackingProfiles.reserve(sources.size());
    snapshot.detectionProfiles.reserve(sources.size());

    for (int index = 0;
         index < static_cast<int>(sources.size());
         ++index) {
        const InspectionProfileSource &source =
                sources[static_cast<std::size_t>(index)];
        const QString templateName = source.name.isEmpty()
                ? QDir(source.directoryPath).dirName()
                : source.name;

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
                CharacterTemplateMatcher::prepare(
                    cloneImages(source.digitTemplates));
        detectionProfile.templateTargetIndexes =
                source.digitTemplateTargetIndexes;
        detectionProfile.thresholdPercent =
                resolvedThresholdPercent(
                    source.imageThreshold,
                    fallbackThresholdText);
        detectionProfile.barcodeOptions = source.barcodeOptions;
        detectionProfile.decodeStrategy = source.decodeStrategy;
        snapshot.detectionProfiles.push_back(detectionProfile);
    }

    return snapshot;
}
