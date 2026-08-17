#pragma once

#include "detection/positioning/inspection_positioner.h"

enum class InspectionAcquisitionKind {
    SoftwareTrigger,
    HardwareTrigger
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
        InspectionTrackingKind trackingKind,
        bool hardwareTriggerEnabled,
        bool barcodeWordMode = false);
};
