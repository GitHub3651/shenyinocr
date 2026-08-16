#include "runtime/inspection_run_configuration.h"

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
