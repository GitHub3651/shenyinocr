#pragma once

#include "application/application_result.h"
#include "application/camera_application_contract.h"
#include "application/inspection_start_preflight.h"
#include "application/inspection_ui_contract.h"
#include "application/runtime_snapshot.h"

#include <QObject>
#include <QStringList>

#include <opencv2/core.hpp>

#include <cstdint>
#include <memory>

class InspectionRuntime;
class CameraSession;
class RecipeStore;
class SettingsApplicationService;
enum class InspectionFaultReason;

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
    InspectionAcquisitionDto acquisitionKind =
            InspectionAcquisitionDto::SoftwareTrigger;

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
    CameraRecoveryResultDto cameraRecovery;
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
    CameraOpenResultDto camera;
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

struct PlcRunSettingsCommand
{
    std::uint16_t rejectTime = 0;
    std::uint32_t rejectDistance = 0;
    std::uint16_t photoTime = 0;
    std::uint32_t photoDistance = 0;
};

class InspectionApplicationService : public QObject
{
    Q_OBJECT
public:
    InspectionApplicationService(
        const std::shared_ptr<InspectionRuntime> &runtime,
        const std::shared_ptr<CameraSession> &cameraSession,
        const std::shared_ptr<SettingsApplicationService> &settings,
        const std::shared_ptr<RecipeStore> &recipes,
        QObject *parent = nullptr);
    ~InspectionApplicationService() override;

    StartInspectionResult start(const StartInspectionCommand &command);
    StopInspectionResult stop(const StopInspectionCommand &command =
                              StopInspectionCommand());
    OpenCameraResult openCamera(const PlcConnectionCommand &plc);
    OperationResult closeCamera();
    OperationResult connectPlc(const PlcConnectionCommand &command);
    OperationResult disconnectPlc();
    OperationResult applyPlcTriggerMode(const QString &modeId);
    OperationResult applyPlcRunSettings(
        const PlcRunSettingsCommand &command);
    OperationResult writePlcPhotoDistance(std::uint32_t value);
    CameraParameterResultDto queryCameraExposureRange();
    CameraParameterResultDto queryCameraGainRange();
    CameraParameterResultDto applyCameraExposure(int exposure);
    CameraParameterResultDto applyCameraGain(int gain);
    bool startTemplatePreview(
        quint64 sessionId,
        int rotationCode,
        int colorChannelCode,
        QString *errorMessage);
    bool stopTemplatePreview();
    void acknowledgeTemplatePreviewFrame(quint64 sessionId);
    bool hasCurrentCameraImage() const;
    cv::Mat currentCameraImageClone() const;
    void replaceCurrentCameraImage(const cv::Mat &image);
    bool isCameraOpen() const;
    bool isCapturing() const;
    void shutdown();
    void completeUnexpectedAcquisitionStop();
    void setUiCallbacks(const InspectionUiCallbacks &callbacks);
    void bindView(const InspectionViewBindingsDto &bindings);
    void clearUiBindings();
    void clearResultView();
    void clearTransientView();
    void presentPreviewFrame(
        const cv::Mat &image,
        bool tissueMode,
        bool productionRunning);
    void resetStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();
    void checkPlcHealth();
    ApplicationFaultSnapshot faultSnapshot() const;

    RuntimeSnapshot runtimeSnapshot() const;

signals:
    void runtimeSnapshotChanged(RuntimeSnapshot snapshot);
    void streamingFrameReady(cv::Mat image);
    void templatePreviewFrameReady(quint64 sessionId, cv::Mat image);
    void templatePreviewFailed(quint64 sessionId, QString reason);
    void captureStopped(bool preview);
    void faultEntered();

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
    void enterFault(
        InspectionFaultReason reason,
        const QString &diagnostic);

    std::shared_ptr<InspectionRuntime> m_runtime;
    std::shared_ptr<CameraSession> m_cameraSession;
    std::shared_ptr<SettingsApplicationService> m_settings;
    std::shared_ptr<RecipeStore> m_recipes;
    bool m_cameraOpen = false;
    QString m_activeRecipeId;
};
