#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

enum class InspectionStartModeKind {
    Tissue,
    SingleTemplate,
    WordProfiles,
    BarcodeWordProfiles
};

enum class InspectionStartIssue {
    None,
    TemplateOperationActive,
    RuntimeBusy,
    CameraClosed,
    DirtySettingsConfirmationRequired,
    PlcDisconnected,
    WordProfilesMissing,
    BarcodeResourcesInvalid,
    ProductTemplateIncomplete,
    WordProfilesIncomplete
};

struct InspectionStartAccessInput
{
    bool templateOperationActive = false;
    bool runtimeBusy = false;
    bool cameraOpen = false;
    bool dirtySettings = false;
    bool plcTriggerEnabled = false;
    bool plcConnected = false;
};

struct InspectionStartProfileReadiness
{
    QString displayName;
    bool trackingTemplateReady = false;
    bool calibrationReady = false;
    bool barcodeRegionReady = false;
    bool dateRegionReady = false;
    bool targetTextReady = false;
    bool characterTemplatesReady = false;
};

struct InspectionStartResourceInput
{
    InspectionStartModeKind modeKind =
            InspectionStartModeKind::SingleTemplate;
    bool productTemplateDirectorySelected = false;
    bool trackingTemplateReady = false;
    bool dateRegionReady = false;
    bool barcodeDecoderReady = true;
    QString barcodeDecoderError;
    QVector<InspectionStartProfileReadiness> profiles;
};

struct InspectionStartPreflightResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    QStringList details;

    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None;
    }
};

class InspectionStartPreflight
{
public:
    static InspectionStartPreflightResult evaluateAccess(
        const InspectionStartAccessInput &input);
    static InspectionStartPreflightResult evaluateResources(
        const InspectionStartResourceInput &input);
};
