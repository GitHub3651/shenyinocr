#pragma once

#include "runtime/inspection_start_preflight.h"

enum class InspectionAcquisitionKind {
    SoftwareTrigger,
    HardwareTrigger
};

enum class InspectionTrackingKind {
    WholeFrame,
    SingleTemplate,
    WordProfiles
};

struct InspectionRunPlan
{
    InspectionAcquisitionKind acquisitionKind =
            InspectionAcquisitionKind::SoftwareTrigger;
    InspectionTrackingKind trackingKind =
            InspectionTrackingKind::SingleTemplate;
    bool barcodeWordHardTriggerMode = false;
};

class InspectionRunConfiguration
{
public:
    static InspectionRunPlan createPlan(
        InspectionStartModeKind modeKind,
        bool hardwareTriggerEnabled);
};
