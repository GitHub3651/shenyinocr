#pragma once

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
        InspectionTrackingKind trackingKind,
        bool hardwareTriggerEnabled,
        bool barcodeWordMode = false);
};
