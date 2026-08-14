#pragma once

#include "runtime/inspection_start_preflight.h"

#include <QString>

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

enum class InspectionRuntimeSettingsIssue {
    None,
    InvalidImageThreshold,
    InvalidTissueThreshold
};

struct InspectionRuntimeSettingsInput
{
    QString imageThresholdText;
    QString tissueThresholdText;
    int rotationIndex = 0;
    int colorChannelIndex = 0;
};

struct InspectionRuntimeSettings
{
    int imageThreshold = 0;
    double tissueThreshold = 0.0;
    int rotationCode = 0;
    int colorChannelCode = 0;
};

struct InspectionRuntimeSettingsResult
{
    InspectionRuntimeSettingsIssue issue =
            InspectionRuntimeSettingsIssue::None;
    InspectionRuntimeSettings settings;

    bool isAccepted() const
    {
        return issue == InspectionRuntimeSettingsIssue::None;
    }
};

class InspectionRunConfiguration
{
public:
    static InspectionRunPlan createPlan(
        InspectionStartModeKind modeKind,
        bool hardwareTriggerEnabled);
    static InspectionRuntimeSettingsResult parseSettings(
        const InspectionRuntimeSettingsInput &input);
};
