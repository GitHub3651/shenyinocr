#include "runtime/inspection_run_configuration.h"

InspectionRunPlan InspectionRunConfiguration::createPlan(
    InspectionTrackingKind trackingKind,
    bool hardwareTriggerEnabled,
    bool barcodeWordMode)
{
    InspectionRunPlan plan;
    plan.acquisitionKind = hardwareTriggerEnabled
            ? InspectionAcquisitionKind::HardwareTrigger
            : InspectionAcquisitionKind::SoftwareTrigger;
    plan.trackingKind = trackingKind;
    plan.barcodeWordHardTriggerMode = barcodeWordMode;
    return plan;
}
