// 文件作用：本文件用于组织相机、PLC、检测运行和模板启动等用户用例，并向界面返回结构化结果。
// 主要职责：组织相机、PLC、检测运行和模板启动等用户用例，并向界面返回结构化结果。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
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
class TemplateStore;
class SettingsApplicationService;
// 组件说明：InspectionFaultReason 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionFaultReason;

// 组件说明：StartInspectionCommand 数据结构集中传递该流程需要的只读数据或回调。
struct StartInspectionCommand
{
    QStringList unappliedChanges;
};

// 组件说明：StartInspectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct StartInspectionResult
{
    InspectionStartIssue issue = InspectionStartIssue::None;
    ApplicationError error;
    QStringList details;
    RuntimeSnapshot snapshot;
    InspectionAcquisitionDto acquisitionKind =
            InspectionAcquisitionDto::SoftwareTrigger;

    // 函数说明：isAccepted 函数检查相关状态并返回判断结果。
    bool isAccepted() const
    {
        return issue == InspectionStartIssue::None
                && error.isEmpty();
    }
};

// 组件说明：StopInspectionIssue 枚举列出该组件允许使用的稳定状态和选项。
enum class StopInspectionIssue
{
    None,
    FaultConfirmationRequired,
    AcquisitionStillStopping,
    CameraRecoveryFailed,
    FaultReconciliationFailed,
    RuntimeFault
};

// 组件说明：StopInspectionCommand 数据结构集中传递该流程需要的只读数据或回调。
struct StopInspectionCommand
{
    bool acknowledgeFault = false;
};

// 组件说明：StopInspectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct StopInspectionResult
{
    StopInspectionIssue issue = StopInspectionIssue::None;
    ApplicationError error;
    RuntimeSnapshot snapshot;
    CameraRecoveryResultDto cameraRecovery;
    QString reconciliationSummary;
    bool recoveredFault = false;

    // 函数说明：isAccepted 函数检查相关状态并返回判断结果。
    bool isAccepted() const
    {
        return issue == StopInspectionIssue::None;
    }
};

// 组件说明：OpenCameraResult 数据结构保存一次操作的结果、状态和错误信息。
struct OpenCameraResult
{
    OperationResult operation;
    CameraOpenResultDto camera;
    bool plcConnectionFailed = false;
    int plcNativeErrorCode = 0;
    RuntimeSnapshot snapshot;
};

// 组件说明：PlcConnectionCommand 数据结构集中传递该流程需要的只读数据或回调。
struct PlcConnectionCommand
{
    QString address;
    int rack = 0;
    int slot = 0;
};

// 组件说明：PlcRunSettingsCommand 数据结构集中传递该流程需要的只读数据或回调。
struct PlcRunSettingsCommand
{
    std::uint16_t rejectTime = 0;
    std::uint32_t rejectDistance = 0;
    std::uint16_t photoTime = 0;
    std::uint32_t photoDistance = 0;
};

// 组件说明：InspectionApplicationService 组件封装对应业务职责和生命周期边界。
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
    CameraParameterResultDto queryCameraGainRange();
    CameraParameterResultDto applyCameraExposure(int exposure);
    CameraParameterResultDto applyCameraGain(int gain);
    OperationResult startTemplatePreview(
        quint64 sessionId,
        int rotationCode,
        int colorChannelCode);
    OperationResult stopTemplatePreview();
    void acknowledgeTemplatePreviewFrame(quint64 sessionId);
    bool hasCurrentCameraImage() const;
    cv::Mat currentCameraImageClone() const;
    void replaceCurrentCameraImage(const cv::Mat &image);
    bool isCameraOpen() const;
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
    OperationResult resetStatistics();
    OperationResult resetNgCount();
    OperationResult clearPendingDelayedNgRequests();
    void checkPlcHealth();
    ApplicationFaultSnapshot faultSnapshot() const;

    RuntimeSnapshot runtimeSnapshot() const;

signals:
    void runtimeSnapshotChanged(RuntimeSnapshot snapshot);
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
    QStringList m_activeTemplatePaths;
};
