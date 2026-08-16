#pragma once

#include "application/application_result.h"
#include "application/inspection_runtime_port.h"
#include "application/inspection_start_preflight.h"
#include "application/runtime_snapshot.h"

#include <QObject>
#include <QStringList>

#include <cstdint>
#include <memory>

class InspectionRuntimeController;
class RecipeStore;
class SettingsApplicationService;
struct InspectionPlcRunSettings;

struct StartInspectionCommand
{
    bool templateOperationActive = false;
    QStringList unappliedChanges;
};

struct StartInspectionResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    ApplicationError error;
    QStringList details;
    RuntimeSnapshot snapshot;
    InspectionAcquisitionKind acquisitionKind =
            InspectionAcquisitionKind::SoftwareTrigger;

    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None
                && error.isEmpty();
    }
};

enum class StopInspectionIssue
{
    None,
    FaultConfirmationRequired,
    AcquisitionStillStopping,
    CameraRecoveryFailed,
    FaultReconciliationFailed,
    RuntimeFault
};

struct StopInspectionCommand
{
    bool acknowledgeFault = false;
};

struct StopInspectionResult
{
    StopInspectionIssue issue = StopInspectionIssue::None;
    ApplicationError error;
    RuntimeSnapshot snapshot;
    InspectionCameraRecoveryResult cameraRecovery;
    QString reconciliationSummary;
    bool recoveredFault = false;

    bool isAccepted() const
    {
        return issue == StopInspectionIssue::None;
    }
};

struct OpenCameraResult
{
    OperationResult operation;
    InspectionCameraOpenResult camera;
    bool plcConnectionFailed = false;
    int plcNativeErrorCode = 0;
    RuntimeSnapshot snapshot;
};

struct PlcConnectionCommand
{
    QString address;
    int rack = 0;
    int slot = 0;
};

class InspectionApplicationService : public QObject
{
    Q_OBJECT
public:
    InspectionApplicationService(
        const std::shared_ptr<InspectionRuntimeController> &runtime,
        const std::shared_ptr<InspectionRuntimePort> &runtimePort,
        const std::shared_ptr<SettingsApplicationService> &settings,
        const std::shared_ptr<RecipeStore> &recipes,
        QObject *parent = nullptr);

    StartInspectionResult start(const StartInspectionCommand &command);
    StopInspectionResult stop(const StopInspectionCommand &command =
                              StopInspectionCommand());
    OpenCameraResult openCamera(const PlcConnectionCommand &plc);
    OperationResult closeCamera();
    OperationResult connectPlc(const PlcConnectionCommand &command);
    OperationResult disconnectPlc();
    OperationResult applyPlcTriggerMode(const QString &modeId);
    OperationResult applyPlcRunSettings(
        const InspectionPlcRunSettings &settings);
    OperationResult writePlcPhotoDistance(std::uint32_t value);
    void shutdown();
    void completeUnexpectedAcquisitionStop();

    RuntimeSnapshot runtimeSnapshot() const;

signals:
    void runtimeSnapshotChanged(RuntimeSnapshot snapshot);

private:
    StartInspectionResult rejectStart(
        InspectionStartIssue issue,
        const QString &code,
        const QString &userMessage,
        const QStringList &details = QStringList(),
        const QString &diagnostic = QString()) const;
    OperationResult plcFailure(
        const QString &code,
        const QString &userMessage,
        int nativeErrorCode) const;
    void publishSnapshot();

    std::shared_ptr<InspectionRuntimeController> m_runtime;
    std::shared_ptr<InspectionRuntimePort> m_runtimePort;
    std::shared_ptr<SettingsApplicationService> m_settings;
    std::shared_ptr<RecipeStore> m_recipes;
    bool m_cameraOpen = false;
    QString m_activeRecipeId;
};
