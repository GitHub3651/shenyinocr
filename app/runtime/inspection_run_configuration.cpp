#include "runtime/inspection_run_configuration.h"

namespace {
int legacySelectionCode(int index)
{
    switch (index) {
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 3;
    default:
        return 0;
    }
}
}

InspectionRunPlan InspectionRunConfiguration::createPlan(
    InspectionStartModeKind modeKind,
    bool hardwareTriggerEnabled)
{
    InspectionRunPlan plan;
    plan.acquisitionKind = hardwareTriggerEnabled
            ? InspectionAcquisitionKind::HardwareTrigger
            : InspectionAcquisitionKind::SoftwareTrigger;

    switch (modeKind) {
    case InspectionStartModeKind::Tissue:
        plan.trackingKind = InspectionTrackingKind::WholeFrame;
        break;
    case InspectionStartModeKind::WordProfiles:
    case InspectionStartModeKind::BarcodeWordProfiles:
        plan.trackingKind = InspectionTrackingKind::WordProfiles;
        break;
    case InspectionStartModeKind::SingleTemplate:
    default:
        plan.trackingKind = InspectionTrackingKind::SingleTemplate;
        break;
    }

    plan.barcodeWordHardTriggerMode =
            modeKind == InspectionStartModeKind::BarcodeWordProfiles;
    return plan;
}

InspectionRuntimeSettingsResult InspectionRunConfiguration::parseSettings(
    const InspectionRuntimeSettingsInput &input)
{
    InspectionRuntimeSettingsResult result;

    bool imageThresholdOk = false;
    const int imageThreshold =
            input.imageThresholdText.trimmed().toInt(&imageThresholdOk);
    if (!imageThresholdOk
            || imageThreshold < 0
            || imageThreshold > 100) {
        result.issue =
                InspectionRuntimeSettingsIssue::InvalidImageThreshold;
        return result;
    }

    bool tissueThresholdOk = false;
    const double tissueThreshold =
            input.tissueThresholdText.trimmed().toDouble(
                &tissueThresholdOk);
    if (!tissueThresholdOk || tissueThreshold <= 0.0) {
        result.issue =
                InspectionRuntimeSettingsIssue::InvalidTissueThreshold;
        return result;
    }

    result.settings.imageThreshold = imageThreshold;
    result.settings.tissueThreshold = tissueThreshold;
    result.settings.rotationCode =
            legacySelectionCode(input.rotationIndex);
    result.settings.colorChannelCode =
            legacySelectionCode(input.colorChannelIndex);
    return result;
}
