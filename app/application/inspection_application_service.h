#pragma once

#include "application/application_result.h"
#include "application/inspection_start_preflight.h"
#include "application/runtime_snapshot.h"
#include "contracts/camera_operation_result.h"

#include <QObject>
#include <QImage>
#include <QStringList>

#include <opencv2/core.hpp>

#include <cstdint>
#include <memory>

class InspectionRuntime;
class CameraSession;
class TemplateStore;
class SettingsApplicationService;
enum class InspectionFaultReason;

struct StartInspectionCommand
{
    QStringList unappliedChanges;
};

struct StartInspectionResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    ApplicationError error;
    QStringList details;

    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None
                && error.isEmpty();
    }
};

struct OpenCameraResult
{
    OperationResult operation;
    CameraOpenResultDto camera;
    bool plcConnectionFailed = false;
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
        const std::shared_ptr<TemplateStore> &templates,
        QObject *parent = nullptr);
    ~InspectionApplicationService() override;

    StartInspectionResult start(const StartInspectionCommand &command);
    CameraRecoveryResultDto stop(InspectionFaultReason reason);
    OpenCameraResult openCamera(const PlcConnectionCommand &plc);
    OperationResult closeCamera();
    OperationResult connectPlc(const PlcConnectionCommand &command);
    OperationResult disconnectPlc();
    OperationResult applyPlcTriggerMode(const QString &modeId);
    OperationResult applyPlcRunSettings(
        const PlcRunSettingsCommand &command);
    OperationResult writePlcPhotoDistance(std::uint32_t value);
    CameraParameterResultDto queryCameraGainRange();
    CameraParameterResultDto applyCameraExposure(int exposure);
    CameraParameterResultDto applyCameraGain(int gain);
    OperationResult startPreview(
        int rotationCode,
        int colorChannelCode);
    OperationResult stopPreview();
    bool hasCurrentCameraImage() const;
    cv::Mat currentCameraImageClone() const;
    bool isCameraOpen() const;
    void shutdown();
    QImage renderPreviewFrame(
        const cv::Mat &image,
        bool tissueMode,
        bool productionRunning);
    OperationResult resetStatistics();
    OperationResult resetNgCount();
    OperationResult clearPendingDelayedNgRequests();
    void checkPlcHealth();
    RuntimeSnapshot runtimeSnapshot() const;

signals:
    void runtimeSnapshotChanged(RuntimeSnapshot snapshot);
    void previewFrameReady(cv::Mat image);
    void previewFailed(QString reason);
    void previewStopped();

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
    OperationResult applyPlcTriggerModeToDevice(const QString &modeId);
    OperationResult applyPlcRunSettingsToDevice(
        const PlcRunSettingsCommand &command);
    void publishSnapshot();
    void enterFault(
        InspectionFaultReason reason,
        const QString &diagnostic);

    std::shared_ptr<InspectionRuntime> m_runtime;
    std::shared_ptr<CameraSession> m_cameraSession;
    std::shared_ptr<SettingsApplicationService> m_settings;
    std::shared_ptr<TemplateStore> m_templates;
};
