// 文件作用：本文件用于组织相机、PLC、检测运行和模板启动等用户用例，并向界面返回结构化结果。
// 主要职责：组织相机、PLC、检测运行和模板启动等用户用例，并向界面返回结构化结果。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/inspection_application_service.h"

#include "contracts/detection_mode.h"
#include "application/settings_application_service.h"
#include "templates/template_store.h"
#include "runtime/camera_session.h"
#include "runtime/inspection_runtime.h"

#include <QDateTime>
#include <QFileInfo>
#include <QVector>

namespace {

constexpr float kMicrosecondsPerMillisecond = 1000.0f;

// 函数说明：applicationState 函数实现名称所表示的处理步骤。
ApplicationRuntimeState applicationState(
    InspectionRuntimeState state)
{
    switch (state) {
    case InspectionRuntimeState::Starting:
        return ApplicationRuntimeState::Starting;
    case InspectionRuntimeState::Running:
        return ApplicationRuntimeState::Running;
    case InspectionRuntimeState::Stopping:
        return ApplicationRuntimeState::Stopping;
    case InspectionRuntimeState::Fault:
        return ApplicationRuntimeState::Fault;
    case InspectionRuntimeState::Idle:
    default:
        return ApplicationRuntimeState::Idle;
    }
}

// 函数说明：framePreprocessSettings 函数实现名称所表示的处理步骤。
FramePreprocessSettings framePreprocessSettings(
    const AppSettings &settings)
{
    FramePreprocessSettings output;
    const int rotation = appSettingsRotationIds().indexOf(
                settings.imageRotationId);
    const int channel = appSettingsColorChannelIds().indexOf(
                settings.colorChannelId);
    output.rotation = static_cast<FrameRotation>(rotation);
    output.colorChannel = static_cast<FrameColorChannel>(channel);
    return output;
}

// 函数说明：cameraConfiguration 函数实现名称所表示的处理步骤。
CameraSessionCaptureConfiguration cameraConfiguration(
    const AppSettings &settings,
    bool hardwareTriggerEnabled)
{
    CameraSessionCaptureConfiguration output;
    output.hardwareTriggerEnabled = hardwareTriggerEnabled;
    output.framePreprocess = framePreprocessSettings(settings);
    if (hardwareTriggerEnabled) {
        output.hardwareTriggerDelayMicroseconds =
                static_cast<float>(settings.cameraDelay)
                * kMicrosecondsPerMillisecond;
    }
    output.exposure = settings.cameraExposure;
    output.gain = settings.cameraGain;
    return output;
}

// 函数说明：resultConfiguration 函数实现名称所表示的处理步骤。
ResultServiceRunConfiguration resultConfiguration(
    const AppSettings &settings)
{
    ResultServiceRunConfiguration configuration;
    configuration.imageSaveModeIndex =
            appSettingsImageSaveModeIds().indexOf(
                settings.imageSaveModeId);
    configuration.plcOutputEnabled = settings.triggerEnabled;
    configuration.delayedNgOffset = settings.rejectPosition;
    configuration.saveOptions.rootDirectory = settings.imageSavePath;
    configuration.saveOptions.format = QStringLiteral("jpg");
    configuration.saveOptions.quality = settings.imageJpegQuality;
    configuration.saveOptions.imageContentModeIndex =
            appSettingsImageSaveTypeIds().indexOf(
                settings.imageSaveTypeId);
    return configuration;
}

// 函数说明：plcRunSettings 函数实现名称所表示的处理步骤。
PlcRunSettingsCommand plcRunSettings(const AppSettings &settings)
{
    PlcRunSettingsCommand output;
    output.rejectTime = static_cast<std::uint16_t>(settings.rejectTime);
    output.rejectDistance = static_cast<std::uint32_t>(
                settings.rejectDistance);
    output.photoTime = static_cast<std::uint16_t>(settings.photoTime);
    output.photoDistance = static_cast<std::uint32_t>(
                settings.photoDistance);
    return output;
}

// 函数说明：startIssueCode 函数创建、准备或启动对应流程。
QString startIssueCode(InspectionStartIssue issue)
{
    switch (issue) {
    case InspectionStartIssue::TemplateOperationActive:
        return QStringLiteral("INSPECTION_TEMPLATE_OPERATION_ACTIVE");
    case InspectionStartIssue::RuntimeBusy:
        return QStringLiteral("INSPECTION_RUNTIME_BUSY");
    case InspectionStartIssue::CameraClosed:
        return QStringLiteral("INSPECTION_CAMERA_CLOSED");
    case InspectionStartIssue::DirtySettingsConfirmationRequired:
        return QStringLiteral("INSPECTION_UNAPPLIED_CHANGES");
    case InspectionStartIssue::PlcDisconnected:
        return QStringLiteral("INSPECTION_PLC_DISCONNECTED");
    case InspectionStartIssue::TemplateMissing:
        return QStringLiteral("INSPECTION_TEMPLATE_MISSING");
    case InspectionStartIssue::TemplatesMissing:
        return QStringLiteral("INSPECTION_TEMPLATES_MISSING");
    case InspectionStartIssue::TemplateResourcesInvalid:
        return QStringLiteral("INSPECTION_TEMPLATE_RESOURCE_INVALID");
    case InspectionStartIssue::TemplateIncomplete:
        return QStringLiteral("INSPECTION_TEMPLATE_INCOMPLETE");
    case InspectionStartIssue::TemplatesIncomplete:
        return QStringLiteral("INSPECTION_TEMPLATES_INCOMPLETE");
    case InspectionStartIssue::None:
    default:
        return QString();
    }
}

// 函数说明：startIssueMessage 函数创建、准备或启动对应流程。
QString startIssueMessage(InspectionStartIssue issue)
{
    switch (issue) {
    case InspectionStartIssue::TemplateOperationActive:
        return QStringLiteral("当前正在制作模板，请先点击【退出模板制作】。");
    case InspectionStartIssue::RuntimeBusy:
        return QStringLiteral("当前正在识别或停止中，请勿重复启动。");
    case InspectionStartIssue::CameraClosed:
        return QStringLiteral("请先点击【打开相机】！");
    case InspectionStartIssue::DirtySettingsConfirmationRequired:
        return QStringLiteral("存在尚未应用的参数修改。");
    case InspectionStartIssue::PlcDisconnected:
        return QStringLiteral("已启用 PLC 触发，但 PLC 未连接，请先连接 PLC。");
    case InspectionStartIssue::TemplateMissing:
        return QStringLiteral("当前模式没有选择可用模板，请先选择模板。");
    case InspectionStartIssue::TemplatesMissing:
        return QStringLiteral("当前模式没有任何可用模板。");
    case InspectionStartIssue::TemplateResourcesInvalid:
        return QStringLiteral("二维码+三期模板资源预检失败。");
    case InspectionStartIssue::TemplateIncomplete:
        return QStringLiteral("模板尚未制作完整，无法启动检测。");
    case InspectionStartIssue::TemplatesIncomplete:
        return QStringLiteral("当前选择中没有制作完整的模板。");
    case InspectionStartIssue::None:
    default:
        return QString();
    }
}

// 函数说明：faultReasonText 函数实现名称所表示的处理步骤。
QString faultReasonText(InspectionFaultReason reason)
{
    switch (reason) {
    case InspectionFaultReason::CameraDisconnected:
        return QStringLiteral("相机断连或正式采集异常");
    case InspectionFaultReason::PlcDisconnected:
        return QStringLiteral("PLC连接或结果输出异常");
    case InspectionFaultReason::HardTriggerQueueOverflow:
        return QStringLiteral("硬触发检测队列已满");
    case InspectionFaultReason::ProductIdentityAmbiguous:
        return QStringLiteral("产品身份无法唯一确定");
    case InspectionFaultReason::RuntimeInvariantViolation:
        return QStringLiteral("检测运行约束被破坏");
    case InspectionFaultReason::None:
        break;
    }
    return QString();
}

// 函数说明：acquisitionDto 函数实现名称所表示的处理步骤。
InspectionAcquisitionDto acquisitionDto(
        bool hardwareTriggerEnabled)
{
    return hardwareTriggerEnabled
            ? InspectionAcquisitionDto::HardwareTrigger
            : InspectionAcquisitionDto::SoftwareTrigger;
}

// 函数说明：cameraParameterDto 函数实现名称所表示的处理步骤。
CameraParameterResultDto cameraParameterDto(
        const InspectionCameraParameterResult &source)
{
    CameraParameterResultDto result;
    result.success = source.success;
    result.minimumValue = source.minimumValue;
    result.maximumValue = source.maximumValue;
    result.actualValue = source.actualValue;
    result.nativeErrorCode = source.nativeErrorCode;
    result.diagnostic = source.diagnostic;
    return result;
}

// 函数说明：cameraOpenIssueDto 函数实现名称所表示的处理步骤。
CameraOpenIssueDto cameraOpenIssueDto(
        InspectionCameraOpenIssue issue)
{
    switch (issue) {
    case InspectionCameraOpenIssue::DeviceNotFound:
        return CameraOpenIssueDto::DeviceNotFound;
    case InspectionCameraOpenIssue::DeviceOpenFailed:
        return CameraOpenIssueDto::DeviceOpenFailed;
    case InspectionCameraOpenIssue::ExposureFailed:
        return CameraOpenIssueDto::ExposureFailed;
    case InspectionCameraOpenIssue::InitializationFailed:
        return CameraOpenIssueDto::InitializationFailed;
    case InspectionCameraOpenIssue::None:
    default:
        return CameraOpenIssueDto::None;
    }
}

// 函数说明：cameraOpenDto 函数实现名称所表示的处理步骤。
CameraOpenResultDto cameraOpenDto(
        const InspectionCameraOpenResult &source)
{
    CameraOpenResultDto result;
    result.issue = cameraOpenIssueDto(source.issue);
    result.deviceCount = source.deviceCount;
    result.appliedExposure = source.appliedExposure;
    result.exposureMinimum = source.exposureMinimum;
    result.exposureMaximum = source.exposureMaximum;
    result.exposureAdjusted = source.exposureAdjusted;
    result.adjustmentMessage = source.adjustmentMessage;
    result.diagnostic = source.diagnostic;
    return result;
}

// 函数说明：cameraRecoveryIssueDto 函数实现名称所表示的处理步骤。
CameraRecoveryIssueDto cameraRecoveryIssueDto(
        InspectionCameraRecoveryIssue issue)
{
    switch (issue) {
    case InspectionCameraRecoveryIssue::MissingCamera:
        return CameraRecoveryIssueDto::MissingCamera;
    case InspectionCameraRecoveryIssue::ExposureRejected:
        return CameraRecoveryIssueDto::ExposureRejected;
    case InspectionCameraRecoveryIssue::InitializationFailed:
        return CameraRecoveryIssueDto::InitializationFailed;
    case InspectionCameraRecoveryIssue::None:
    default:
        return CameraRecoveryIssueDto::None;
    }
}

// 函数说明：cameraRecoveryDto 函数实现名称所表示的处理步骤。
CameraRecoveryResultDto cameraRecoveryDto(
        const InspectionCameraRecoveryResult &source)
{
    CameraRecoveryResultDto result;
    result.issue = cameraRecoveryIssueDto(source.issue);
    result.recoveryAttempted = source.recoveryAttempted;
    result.cameraOpen = source.cameraOpen;
    result.adjustmentMessage = source.adjustmentMessage;
    result.errorMessage = source.errorMessage;
    return result;
}

} // namespace

// 函数说明：InspectionApplicationService 构造函数创建组件并初始化其依赖和初始状态。
InspectionApplicationService::InspectionApplicationService(
    const std::shared_ptr<InspectionRuntime> &runtime,
    const std::shared_ptr<CameraSession> &cameraSession,
    const std::shared_ptr<SettingsApplicationService> &settings,
    const std::shared_ptr<TemplateStore> &templates,
    QObject *parent)
    : QObject(parent),
      m_runtime(runtime),
      m_cameraSession(cameraSession),
      m_settings(settings),
      m_templates(templates)
{
    qRegisterMetaType<RuntimeSnapshot>("RuntimeSnapshot");
    qRegisterMetaType<InspectionFaultReason>(
                "InspectionFaultReason");
    CameraSessionCallbacks callbacks;
    callbacks.previewFrameReady = [this](
            quint64 sessionId,
            const cv::Mat &image) {
        emit templatePreviewFrameReady(sessionId, image.clone());
    };
    callbacks.previewFailed = [this](
            quint64 sessionId,
            const QString &reason) {
        emit templatePreviewFailed(sessionId, reason);
    };
    callbacks.captureStopped = [this](bool preview) {
        emit captureStopped(preview);
    };
    callbacks.enterFault = [this](
            InspectionFaultReason reason,
            const QString &diagnostic) {
        enterFault(reason, diagnostic);
    };
    m_cameraSession->setCallbacks(callbacks);
}

// 函数说明：~InspectionApplicationService 析构函数按生命周期要求释放组件持有的资源。
InspectionApplicationService::~InspectionApplicationService()
{
    m_cameraSession->setCallbacks(CameraSessionCallbacks());
}

// 函数说明：start 函数创建、准备或启动对应流程。
StartInspectionResult InspectionApplicationService::start(
    const StartInspectionCommand &command)
{
    const AppSettings settings = m_settings->current();
    InspectionStartAccessInput access;
    access.runtimeBusy = m_runtime->isBusy();
    access.templateOperationActive = !access.runtimeBusy
            && m_cameraSession->isCapturing();
    access.cameraOpen = m_cameraSession->isOpen();
    access.dirtySettings = m_settings->hasUnappliedChanges()
            || !command.unappliedChanges.isEmpty();
    access.plcTriggerEnabled = settings.triggerEnabled;
    access.plcConnected = m_runtime->isPlcConnected();

    const InspectionStartPreflightResult accessResult =
            InspectionStartPreflight::evaluateAccess(access);
    if (!accessResult.isAccepted()) {
        return rejectStart(
                    accessResult.issue,
                    startIssueCode(accessResult.issue),
                    startIssueMessage(accessResult.issue),
                    command.unappliedChanges);
    }

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(
                settings.detectModeId, &detectionMode)) {
        return rejectStart(
                    InspectionStartIssue::TemplateMissing,
                    QStringLiteral("INSPECTION_DETECTION_MODE_INVALID"),
                    QStringLiteral("应用设置中的检测模式无效。"));
    }
    const QStringList selectedPaths =
            settings.detectionSchemes.templatePaths(detectionMode);
    if (detectionMode != DetectionMode::Tissue
            && selectedPaths.isEmpty()) {
        return rejectStart(
                    InspectionStartIssue::TemplateMissing,
                    startIssueCode(
                        InspectionStartIssue::TemplateMissing),
                    startIssueMessage(
                        InspectionStartIssue::TemplateMissing));
    }

    QVector<PreparedTemplateSnapshot> preparedTemplates;
    QStringList templateWarnings;
    for (const QString &path : selectedPaths) {
        PreparedTemplateSnapshot prepared;
        TemplateStoreError templateError;
        if (m_templates
                && m_templates->loadPrepared(
                    path, detectionMode, &prepared, &templateError)
                && prepared) {
            preparedTemplates.append(prepared);
            continue;
        }
        const QString reason = templateError.userMessage.isEmpty()
                ? QStringLiteral("未知模板错误")
                : templateError.userMessage;
        const QString warning = QStringLiteral(
                    "模板“%1”状态异常：\n%2\n%3")
                .arg(QFileInfo(path).fileName(),
                     path, reason);
        if (detectionMode == DetectionMode::Stamp
                || detectionMode == DetectionMode::Ocr) {
            return rejectStart(
                        InspectionStartIssue::TemplateIncomplete,
                        templateError.code.isEmpty()
                        ? QStringLiteral("TEMPLATE_LOAD_FAILED")
                        : templateError.code,
                        warning,
                        QStringList() << warning,
                        templateError.diagnostic);
        }
        templateWarnings.append(
                    QStringLiteral("%1\n已跳过加载该模板。")
                    .arg(warning));
    }
    if (detectionMode != DetectionMode::Tissue
            && preparedTemplates.isEmpty()) {
        return rejectStart(
                    InspectionStartIssue::TemplatesIncomplete,
                    QStringLiteral("INSPECTION_NO_VALID_TEMPLATE"),
                    QStringLiteral("当前选择中没有可用模板。"),
                    templateWarnings);
    }

    DetectionRuntimeReadiness detectionReadiness;
    if (detectionModeDescriptor(detectionMode).requiresBarcodeDecoder) {
        detectionReadiness = m_runtime->prepareDetection(detectionMode);
    }
    InspectionStartResourceInput resourceInput;
    resourceInput.mode = detectionMode;
    resourceInput.preparedTemplateCount = preparedTemplates.size();
    resourceInput.barcodeDecoderReady = detectionReadiness.ready;
    resourceInput.barcodeDecoderError = detectionReadiness.errorMessage;
    const InspectionStartPreflightResult resourceResult =
            InspectionStartPreflight::evaluateResources(resourceInput);
    if (!resourceResult.isAccepted()) {
        return rejectStart(
                    resourceResult.issue,
                    startIssueCode(resourceResult.issue),
                    startIssueMessage(resourceResult.issue),
                    resourceResult.details);
    }

    const bool hardwareTriggerEnabled = settings.triggerEnabled;
    if (hardwareTriggerEnabled) {
        m_runtime->resetStatistics();
    }
    DetectionTemplateSnapshot templateSnapshot;
    if (detectionModeDescriptor(detectionMode).trackingKind
            == DetectionTrackingKind::MultipleTemplates) {
        templateSnapshot = DetectionTemplateSnapshotBuilder::create(
                    preparedTemplates);
        if (!templateSnapshot.isValid()) {
            return rejectStart(
                        InspectionStartIssue::TemplatesMissing,
                        QStringLiteral("INSPECTION_TEMPLATE_SNAPSHOT_INVALID"),
                        QStringLiteral("没有可用的多模板定位配置。"));
        }
    }

    const QString runId = m_runtime->beginStart(
                settings, detectionMode, preparedTemplates,
                templateSnapshot,
                settings.detectionSchemes.tissueRoughnessThreshold,
                framePreprocessSettings(settings));
    if (runId.isEmpty()) {
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    startIssueCode(InspectionStartIssue::RuntimeBusy),
                    startIssueMessage(InspectionStartIssue::RuntimeBusy));
    }
    publishSnapshot();

    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        AppSettings adjusted = m_settings->current();
        adjusted.cameraExposure = adjustedExposure;
        m_settings->updateDraft(adjusted);
        const OperationResult saved = m_settings->applyDraft();
        if (!saved.isSuccess() && errorMessage) {
            *errorMessage = saved.error.userMessage;
        }
        return saved.isSuccess();
    };
    QString cameraError;
    if (!m_cameraSession->prepareInspection(
                cameraConfiguration(
                    settings,
                    hardwareTriggerEnabled),
                &cameraError)) {
        m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_runtime->rollbackStart();
        publishSnapshot();
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    QStringLiteral("INSPECTION_CAMERA_PREPARE_FAILED"),
                    cameraError.isEmpty()
                    ? QStringLiteral("启动识别前相机准备失败。")
                    : cameraError,
                    QStringList(),
                    cameraError);
    }

    if (m_runtime->isPlcConnected()) {
        const OperationResult trigger = applyPlcTriggerModeToDevice(
                    settings.triggerModeId);
        const OperationResult parameters = trigger.isSuccess()
                ? applyPlcRunSettingsToDevice(plcRunSettings(settings))
                : trigger;
        if (!trigger.isSuccess() || !parameters.isSuccess()) {
            m_cameraSession->stopInspection();
            m_cameraSession->restorePreviewReady(
                        settings.cameraExposure, persistExposure);
            m_runtime->rollbackStart();
            publishSnapshot();
            const ApplicationError error = !trigger.isSuccess()
                    ? trigger.error
                    : parameters.error;
            return rejectStart(
                        InspectionStartIssue::RuntimeBusy,
                        QStringLiteral("INSPECTION_PLC_START_SETTINGS_FAILED"),
                        QStringLiteral("启动识别前PLC参数下发失败：\n")
                            + error.userMessage,
                        QStringList(),
                        error.diagnostic);
        }
    }

    QString executionError;
    if (!m_runtime->startDetection(
                resultConfiguration(settings),
                &executionError)) {
        m_cameraSession->stopInspection();
        m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_runtime->rollbackStart();
        publishSnapshot();
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    QStringLiteral("INSPECTION_START_EXECUTION_FAILED"),
                    executionError.isEmpty()
                    ? QStringLiteral("启动识别失败。")
                    : executionError,
                    QStringList(),
                    executionError);
    }
    if (!m_cameraSession->startInspection(&cameraError)) {
        m_cameraSession->stopInspection();
        m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_runtime->rollbackStart();
        publishSnapshot();
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    QStringLiteral("INSPECTION_CAPTURE_START_FAILED"),
                    cameraError.isEmpty()
                    ? QStringLiteral("相机采集线程启动失败。")
                    : cameraError,
                    QStringList(),
                    cameraError);
    }
    if (!m_runtime->commitStart()) {
        m_cameraSession->stopInspection();
        m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_runtime->rollbackStart();
        publishSnapshot();
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    QStringLiteral("INSPECTION_START_COMMIT_FAILED"),
                    QStringLiteral("运行状态提交失败。"));
    }

    m_activeTemplatePaths.clear();
    for (const PreparedTemplateSnapshot &prepared : preparedTemplates) {
        if (prepared) {
            m_activeTemplatePaths.append(prepared->directoryPath);
        }
    }
    publishSnapshot();
    StartInspectionResult result;
    result.details = templateWarnings;
    result.acquisitionKind = acquisitionDto(hardwareTriggerEnabled);
    result.snapshot = runtimeSnapshot();
    return result;
}

// 函数说明：stop 函数停止流程、清理状态或释放对应资源。
StopInspectionResult InspectionApplicationService::stop(
    const StopInspectionCommand &command)
{
    StopInspectionResult result;
    const InspectionRuntimeState initialState = m_runtime->state();
    const bool recoveringFault =
            initialState == InspectionRuntimeState::Fault;
    result.recoveredFault = recoveringFault;
    if (recoveringFault && !command.acknowledgeFault) {
        result.issue = StopInspectionIssue::FaultConfirmationRequired;
        result.error.code = QStringLiteral(
                    "INSPECTION_FAULT_CONFIRMATION_REQUIRED");
        result.error.userMessage = QStringLiteral(
                    "需要操作员确认后才能解除故障锁定。");
        result.snapshot = runtimeSnapshot();
        return result;
    }
    if (initialState == InspectionRuntimeState::Idle) {
        result.snapshot = runtimeSnapshot();
        return result;
    }

    m_runtime->beginStop();
    publishSnapshot();
    const CameraCaptureStopResult acquisition =
            m_cameraSession->stopInspection();
    m_runtime->waitForStop();
    if (!acquisition.allStopped()) {
        result.issue = StopInspectionIssue::AcquisitionStillStopping;
        result.error.code = QStringLiteral(
                    "INSPECTION_ACQUISITION_STILL_STOPPING");
        result.error.userMessage = QStringLiteral(
                    "停止中，请稍后再关闭相机。");
        result.snapshot = runtimeSnapshot();
        return result;
    }

    const AppSettings settings = m_settings->current();
    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        AppSettings adjusted = m_settings->current();
        adjusted.cameraExposure = adjustedExposure;
        m_settings->updateDraft(adjusted);
        const OperationResult saved = m_settings->applyDraft();
        if (!saved.isSuccess() && errorMessage) {
            *errorMessage = saved.error.userMessage;
        }
        return saved.isSuccess();
    };
    if (acquisition.shouldRestoreCamera()) {
        result.cameraRecovery = cameraRecoveryDto(
                    m_cameraSession->restorePreviewReady(
                        settings.cameraExposure,
                        persistExposure));
    } else {
        result.cameraRecovery.cameraOpen = m_cameraSession->isOpen();
    }
    m_runtime->finishStop();

    if (!recoveringFault
            && m_runtime->state() == InspectionRuntimeState::Fault) {
        result.issue = StopInspectionIssue::RuntimeFault;
        result.error.code = QStringLiteral(
                    "INSPECTION_RUNTIME_FAULT_DURING_STOP");
        result.error.userMessage = QStringLiteral(
                    "停止过程中检测运行时进入故障状态。");
        result.snapshot = runtimeSnapshot();
        publishSnapshot();
        return result;
    }

    if (recoveringFault) {
        const int unconfirmed = m_runtime->reconcileFaultProducts();
        result.reconciliationSummary = QStringLiteral(
                    "视觉检测已暂停，输送线状态未知。\n"
                    "本次未完成产品记为Unconfirmed：%1件。\n"
                    "请通过输送线自身控制确认停线并隔离相关产品。")
                .arg(unconfirmed);
        if (!m_runtime->acknowledgeFault()) {
            result.issue = StopInspectionIssue::FaultReconciliationFailed;
            result.error.code = QStringLiteral(
                        "INSPECTION_FAULT_ACKNOWLEDGE_REJECTED");
            result.error.userMessage = QStringLiteral(
                        "故障仍有未完成产品，不能解除锁定。");
            result.snapshot = runtimeSnapshot();
            publishSnapshot();
            return result;
        }
    }

    m_activeTemplatePaths.clear();
    if (result.cameraRecovery.issue
            == CameraRecoveryIssueDto::ExposureRejected) {
        result.issue = StopInspectionIssue::CameraRecoveryFailed;
        result.error.code = QStringLiteral(
                    "INSPECTION_CAMERA_EXPOSURE_RECOVERY_FAILED");
        result.error.userMessage = QStringLiteral(
                    "停止识别后恢复相机曝光失败。");
        result.error.diagnostic =
                result.cameraRecovery.errorMessage;
    }
    result.snapshot = runtimeSnapshot();
    publishSnapshot();
    return result;
}

// 函数说明：openCamera 函数创建、准备或启动对应流程。
OpenCameraResult InspectionApplicationService::openCamera(
    const PlcConnectionCommand &plcCommand)
{
    OpenCameraResult result;
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        result.operation = OperationResult::rejected(
                    QStringLiteral("CAMERA_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能重新打开相机。"));
        result.snapshot = runtimeSnapshot();
        return result;
    }
    if (m_cameraSession->isOpen()) {
        result.operation = OperationResult::rejected(
                    QStringLiteral("CAMERA_ALREADY_OPEN"),
                    QStringLiteral("相机已连接！"));
        result.snapshot = runtimeSnapshot();
        return result;
    }

    const AppSettings settings = m_settings->current();
    const PlcOperationResult plc = m_runtime->connectPlc(
                plcCommand.address,
                plcCommand.rack,
                plcCommand.slot);
    result.plcConnectionFailed = !plc.isSuccess();
    result.plcNativeErrorCode = plc.nativeErrorCode;

    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        AppSettings adjusted = m_settings->current();
        adjusted.cameraExposure = adjustedExposure;
        m_settings->updateDraft(adjusted);
        const OperationResult saved = m_settings->applyDraft();
        if (!saved.isSuccess() && errorMessage) {
            *errorMessage = saved.error.userMessage;
        }
        return saved.isSuccess();
    };
    result.camera = cameraOpenDto(
                m_cameraSession->openFirst(
                    settings.cameraExposure, persistExposure));
    if (!result.camera.isSuccess()) {
        result.operation = OperationResult::rejected(
                    QStringLiteral("CAMERA_OPEN_FAILED"),
                    QStringLiteral("打开相机失败。"),
                    result.camera.diagnostic);
        result.snapshot = runtimeSnapshot();
        publishSnapshot();
        return result;
    }
    result.operation = OperationResult::accepted();
    result.snapshot = runtimeSnapshot();
    publishSnapshot();
    return result;
}

// 函数说明：closeCamera 函数停止流程、清理状态或释放对应资源。
OperationResult InspectionApplicationService::closeCamera()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("CAMERA_RUNTIME_BUSY"),
                    QStringLiteral("相机正在检测采图中，请先停止识别。"));
    }
    if (m_cameraSession->isCapturing()) {
        return OperationResult::rejected(
                    QStringLiteral("CAMERA_CAPTURE_BUSY"),
                    QStringLiteral("相机正在取景，请先退出模板制作。"));
    }
    if (m_cameraSession->isOpen()) {
        m_cameraSession->close();
    }
    m_runtime->resetStatistics();
    publishSnapshot();
    return OperationResult::accepted();
}

// 函数说明：connectPlc 函数建立或断开对应外部连接。
OperationResult InspectionApplicationService::connectPlc(
    const PlcConnectionCommand &command)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能连接 PLC。"));
    }
    if (m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_ALREADY_CONNECTED"),
                    QStringLiteral("PLC 已连接。"));
    }
    const PlcOperationResult result = m_runtime->connectPlc(
                command.address, command.rack, command.slot);
    publishSnapshot();
    return result.isSuccess()
            ? OperationResult::accepted()
            : plcFailure(
                QStringLiteral("PLC_CONNECT_FAILED"),
                QStringLiteral("PLC连接失败"),
                result.nativeErrorCode);
}

// 函数说明：disconnectPlc 函数建立或断开对应外部连接。
OperationResult InspectionApplicationService::disconnectPlc()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能断开 PLC。"));
    }
    if (!m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_NOT_CONNECTED"),
                    QStringLiteral("PLC 未连接。"));
    }
    const PlcOperationResult result = m_runtime->disconnectPlc();
    publishSnapshot();
    return result.isSuccess()
            ? OperationResult::accepted()
            : plcFailure(
                QStringLiteral("PLC_DISCONNECT_FAILED"),
                QStringLiteral("PLC断开失败"),
                result.nativeErrorCode);
}

// 函数说明：applyPlcTriggerMode 函数更新或应用对应的配置和状态。
OperationResult InspectionApplicationService::applyPlcTriggerMode(
    const QString &modeId)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能修改 PLC 参数。"));
    }
    return applyPlcTriggerModeToDevice(modeId);
}

OperationResult
InspectionApplicationService::applyPlcTriggerModeToDevice(
    const QString &modeId)
{
    if (!m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_NOT_CONNECTED"),
                    QStringLiteral("PLC未连接！"));
    }
    const int modeIndex = appSettingsTriggerModeIds().indexOf(modeId);
    if (modeIndex < 0) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_TRIGGER_MODE_INVALID"),
                    QStringLiteral("PLC触发模式无效"));
    }
    const PlcOperationResult result =
            m_runtime->writePlcTriggerMode(modeIndex);
    return result.isSuccess()
            ? OperationResult::accepted()
            : plcFailure(
                QStringLiteral("PLC_TRIGGER_MODE_WRITE_FAILED"),
                modeIndex == 0
                ? QStringLiteral("设置连续模式失败")
                : QStringLiteral("设置间歇模式失败"),
                result.nativeErrorCode);
}

// 函数说明：applyPlcRunSettings 函数更新或应用对应的配置和状态。
OperationResult InspectionApplicationService::applyPlcRunSettings(
    const PlcRunSettingsCommand &command)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能修改 PLC 参数。"));
    }
    return applyPlcRunSettingsToDevice(command);
}

OperationResult
InspectionApplicationService::applyPlcRunSettingsToDevice(
    const PlcRunSettingsCommand &command)
{
    if (!m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_NOT_CONNECTED"),
                    QStringLiteral("PLC未连接！"));
    }
    InspectionPlcRunSettings plcSettings;
    plcSettings.rejectTime = command.rejectTime;
    plcSettings.rejectDistance = command.rejectDistance;
    plcSettings.photoTime = command.photoTime;
    plcSettings.photoDistance = command.photoDistance;
    const InspectionPlcRunSettingsResult result =
            m_runtime->applyPlcRunSettings(plcSettings);
    if (result.isSuccess()) {
        return OperationResult::accepted();
    }
    QString code = QStringLiteral("PLC_RUN_SETTINGS_WRITE_FAILED");
    QString message = QStringLiteral("PLC运行参数下发失败");
    switch (result.failedField) {
    case InspectionPlcRunSettingField::RejectTime:
        code = QStringLiteral("PLC_REJECT_TIME_WRITE_FAILED");
        message = QStringLiteral("设置剔除时间失败");
        break;
    case InspectionPlcRunSettingField::RejectDistance:
        code = QStringLiteral("PLC_REJECT_DISTANCE_WRITE_FAILED");
        message = QStringLiteral("设置剔除距离失败");
        break;
    case InspectionPlcRunSettingField::PhotoTime:
        code = QStringLiteral("PLC_PHOTO_TIME_WRITE_FAILED");
        message = QStringLiteral("设置拍照时间失败");
        break;
    case InspectionPlcRunSettingField::PhotoDistance:
        code = QStringLiteral("PLC_PHOTO_DISTANCE_WRITE_FAILED");
        message = QStringLiteral("设置拍照距离失败");
        break;
    case InspectionPlcRunSettingField::None:
    default:
        break;
    }
    return plcFailure(
                code, message, result.operation.nativeErrorCode);
}

// 函数说明：writePlcPhotoDistance 函数保存或发布对应的数据和资源。
OperationResult InspectionApplicationService::writePlcPhotoDistance(
    std::uint32_t value)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_RUNTIME_BUSY"),
                    QStringLiteral("当前有任务正在运行，不能修改 PLC 参数。"));
    }
    if (!m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_NOT_CONNECTED"),
                    QStringLiteral("PLC未连接！"));
    }
    const PlcOperationResult result =
            m_runtime->writePlcPhotoDistance(value);
    return result.isSuccess()
            ? OperationResult::accepted()
            : plcFailure(
                QStringLiteral("PLC_PHOTO_DISTANCE_WRITE_FAILED"),
                QStringLiteral("设置拍照距离失败"),
                result.nativeErrorCode);
}

CameraParameterResultDto
// 函数说明：queryCameraGainRange 函数读取、等待或计算对应的数据。
InspectionApplicationService::queryCameraGainRange()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle
            || !m_cameraSession->isOpen()
            || m_cameraSession->isCapturing()) {
        CameraParameterResultDto result;
        result.diagnostic = QStringLiteral(
                    "当前相机状态不能查询增益范围。");
        return result;
    }
    return cameraParameterDto(m_cameraSession->queryGainRange());
}

CameraParameterResultDto
// 函数说明：applyCameraExposure 函数更新或应用对应的配置和状态。
InspectionApplicationService::applyCameraExposure(int exposure)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle
            || !m_cameraSession->isOpen()
            || m_cameraSession->isCapturing()) {
        CameraParameterResultDto result;
        result.diagnostic = QStringLiteral(
                    "请在相机已打开且没有检测或取景时设置曝光。");
        return result;
    }
    return cameraParameterDto(m_cameraSession->applyExposure(exposure));
}

CameraParameterResultDto
// 函数说明：applyCameraGain 函数更新或应用对应的配置和状态。
InspectionApplicationService::applyCameraGain(int gain)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle
            || !m_cameraSession->isOpen()
            || m_cameraSession->isCapturing()) {
        CameraParameterResultDto result;
        result.diagnostic = QStringLiteral(
                    "请在相机已打开且没有检测或取景时设置增益。");
        return result;
    }
    return cameraParameterDto(m_cameraSession->applyGain(gain));
}

// 函数说明：startTemplatePreview 函数创建、准备或启动对应流程。
OperationResult InspectionApplicationService::startTemplatePreview(
    quint64 sessionId,
    int rotationCode,
    int colorChannelCode)
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_RUNTIME_BUSY"),
                    QStringLiteral("当前正在进行正式检测，请先停止识别。"));
    }
    if (!m_cameraSession->isOpen()) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_CAMERA_CLOSED"),
                    QStringLiteral("请先点击【打开相机】！"));
    }
    if (m_cameraSession->isCapturing()) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_CAPTURE_BUSY"),
                    QStringLiteral("相机采集线程仍在运行，请先停止当前任务。"));
    }
    const InspectionCameraParameterResult exposure =
            m_cameraSession->applyExposure(
                m_settings->current().cameraExposure);
    if (!exposure.success) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_EXPOSURE_FAILED"),
                    QStringLiteral("制作模板前应用相机曝光失败。"),
                    exposure.diagnostic);
    }
    FramePreprocessSettings settings;
    settings.rotation = static_cast<FrameRotation>(rotationCode);
    settings.colorChannel =
            static_cast<FrameColorChannel>(colorChannelCode);
    QString errorMessage;
    if (!m_cameraSession->startPreview(
            sessionId, settings, &errorMessage)) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_PREVIEW_START_FAILED"),
                    errorMessage.isEmpty()
                    ? QStringLiteral("实时取景线程启动失败。")
                    : errorMessage,
                    errorMessage);
    }
    return OperationResult::accepted();
}

// 函数说明：stopTemplatePreview 函数停止流程、清理状态或释放对应资源。
OperationResult InspectionApplicationService::stopTemplatePreview()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_RUNTIME_BUSY"),
                    QStringLiteral("当前正在进行正式检测，不能停止模板取景。"));
    }
    return m_cameraSession->stopPreview()
            ? OperationResult::accepted()
            : OperationResult::rejected(
                QStringLiteral("TEMPLATE_PREVIEW_STOP_FAILED"),
                QStringLiteral("实时取景线程尚未停止，请稍后重试。"));
}

// 函数说明：acknowledgeTemplatePreviewFrame 函数停止流程、清理状态或释放对应资源。
void InspectionApplicationService::acknowledgeTemplatePreviewFrame(
    quint64 sessionId)
{
    m_cameraSession->acknowledgePreviewFrame(sessionId);
}

// 函数说明：hasCurrentCameraImage 函数检查相关状态并返回判断结果。
bool InspectionApplicationService::hasCurrentCameraImage() const
{
    return m_cameraSession->hasCurrentImage();
}

// 函数说明：currentCameraImageClone 函数读取、等待或计算对应的数据。
cv::Mat InspectionApplicationService::currentCameraImageClone() const
{
    return m_cameraSession->currentImageClone();
}

// 函数说明：replaceCurrentCameraImage 函数更新或应用对应的配置和状态。
void InspectionApplicationService::replaceCurrentCameraImage(
    const cv::Mat &image)
{
    m_cameraSession->replaceCurrentImage(image);
}

// 函数说明：isCameraOpen 函数检查相关状态并返回判断结果。
bool InspectionApplicationService::isCameraOpen() const
{
    return m_cameraSession->isOpen();
}

// 函数说明：shutdown 函数实现名称所表示的处理步骤。
void InspectionApplicationService::shutdown()
{
    m_runtime->beginStop();
    m_cameraSession->stopInspection();
    m_runtime->waitForStop();
    if (m_cameraSession->isOpen()) {
        m_cameraSession->close();
    }
    if (m_runtime->isPlcConnected()) {
        m_runtime->disconnectPlc();
    }
    m_runtime->finishStop();
    m_activeTemplatePaths.clear();
    publishSnapshot();
}

// 函数说明：completeUnexpectedAcquisitionStop 函数实现名称所表示的处理步骤。
void InspectionApplicationService::completeUnexpectedAcquisitionStop()
{
    const InspectionRuntimeState state = m_runtime->state();
    if (state != InspectionRuntimeState::Running
            && state != InspectionRuntimeState::Stopping) {
        return;
    }
    m_runtime->beginStop();
    m_cameraSession->stopInspection();
    m_runtime->waitForStop();
    if (m_cameraSession->isOpen()) {
        const AppSettings settings = m_settings->current();
        const CameraSession::PersistAdjustedExposure persistExposure =
                [this](int adjustedExposure, QString *errorMessage) {
            AppSettings adjusted = m_settings->current();
            adjusted.cameraExposure = adjustedExposure;
            m_settings->updateDraft(adjusted);
            const OperationResult saved = m_settings->applyDraft();
            if (!saved.isSuccess() && errorMessage) {
                *errorMessage = saved.error.userMessage;
            }
            return saved.isSuccess();
        };
        m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
    }
    m_runtime->finishStop();
    m_activeTemplatePaths.clear();
    publishSnapshot();
}

// 函数说明：setUiCallbacks 函数更新或应用对应的配置和状态。
void InspectionApplicationService::setUiCallbacks(
    const InspectionUiCallbacks &callbacks)
{
    ResultServiceCallbacks runtimeCallbacks;
    runtimeCallbacks.runtimeFaulted = [this]() {
        publishSnapshot();
        emit faultEntered();
    };
    runtimeCallbacks.warnMissingAnnotatedImage =
            callbacks.warnMissingAnnotatedImage;
    runtimeCallbacks.reportImageSaveFailure =
            callbacks.reportImageSaveFailure;
    runtimeCallbacks.clearPreviousOverlay =
            callbacks.clearPreviousOverlay;
    runtimeCallbacks.showDetectionRoiWarning =
            callbacks.showDetectionRoiWarning;
    runtimeCallbacks.clearDetectionRoiWarning =
            callbacks.clearDetectionRoiWarning;
    m_runtime->resultService().setCallbacks(runtimeCallbacks);
}

// 函数说明：bindView 函数实现名称所表示的处理步骤。
void InspectionApplicationService::bindView(
    const InspectionViewBindingsDto &bindings)
{
    InspectionPresentationViewBindings runtimeBindings;
    runtimeBindings.showImage = bindings.showImage;
    runtimeBindings.showVerdictText = bindings.showVerdictText;
    runtimeBindings.showRecognitionText = bindings.showRecognitionText;
    runtimeBindings.showTemplateName = bindings.showTemplateName;
    runtimeBindings.showTotalCount = bindings.showTotalCount;
    runtimeBindings.showNgCount = bindings.showNgCount;
    runtimeBindings.showPassRate = bindings.showPassRate;
    runtimeBindings.showElapsedText = bindings.showElapsedText;
    runtimeBindings.showVerdictStyle = [bindings](
            DetectionVerdictViewStyle style) {
        if (bindings.showVerdictStyle) {
            bindings.showVerdictStyle(
                        style == DetectionVerdictViewStyle::Correct
                        ? InspectionVerdictStyleDto::Correct
                        : InspectionVerdictStyleDto::Incorrect);
        }
    };
    m_runtime->resultService().bindView(runtimeBindings);
}

// 函数说明：clearUiBindings 函数停止流程、清理状态或释放对应资源。
void InspectionApplicationService::clearUiBindings()
{
    m_runtime->resultService().setCallbacks(ResultServiceCallbacks());
    m_runtime->resultService().bindView(
                InspectionPresentationViewBindings());
}

// 函数说明：clearResultView 函数停止流程、清理状态或释放对应资源。
void InspectionApplicationService::clearResultView()
{
    m_runtime->resultService().clear();
}

// 函数说明：clearTransientView 函数停止流程、清理状态或释放对应资源。
void InspectionApplicationService::clearTransientView()
{
    m_runtime->resultService().clearTransientView();
}

// 函数说明：presentPreviewFrame 函数执行对应事件或业务处理。
void InspectionApplicationService::presentPreviewFrame(
    const cv::Mat &image,
    bool tissueMode,
    bool productionRunning)
{
    m_runtime->resultService().presentPreviewFrame(
                image, tissueMode, productionRunning);
}

// 函数说明：resetStatistics 函数停止流程、清理状态或释放对应资源。
OperationResult InspectionApplicationService::resetStatistics()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("STATISTICS_RUNTIME_BUSY"),
                    QStringLiteral("请先停止当前任务再清零统计。"));
    }
    m_runtime->resetStatistics();
    m_runtime->resultService().presentTotalAndNgCounts(
                m_runtime->totalCount(), m_runtime->ngCount());
    return OperationResult::accepted();
}

// 函数说明：resetNgCount 函数停止流程、清理状态或释放对应资源。
OperationResult InspectionApplicationService::resetNgCount()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("STATISTICS_RUNTIME_BUSY"),
                    QStringLiteral("请先停止当前任务再清零统计。"));
    }
    m_runtime->resetNgCount();
    m_runtime->resultService().presentNgCount(m_runtime->ngCount());
    return OperationResult::accepted();
}

// 函数说明：clearPendingDelayedNgRequests 函数停止流程、清理状态或释放对应资源。
OperationResult
InspectionApplicationService::clearPendingDelayedNgRequests()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("REJECT_QUEUE_RUNTIME_BUSY"),
                    QStringLiteral("请先停止当前任务再清空剔除队列。"));
    }
    m_runtime->clearPendingDelayedNgRequests();
    return OperationResult::accepted();
}

// 函数说明：checkPlcHealth 函数校验、转换或恢复对应数据。
void InspectionApplicationService::checkPlcHealth()
{
    if (!m_runtime->isRunning()
            || !m_runtime->resultService().requiresPlcForRun()
            || m_runtime->isPlcConnected()) {
        return;
    }
    enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("运行中 PLC 连接状态已断开。"));
}

ApplicationFaultSnapshot
// 函数说明：faultSnapshot 函数实现名称所表示的处理步骤。
InspectionApplicationService::faultSnapshot() const
{
    const InspectionFaultSnapshot source = m_runtime->faultSnapshot();
    ApplicationFaultSnapshot snapshot;
    snapshot.active = source.isActive();
    snapshot.reasonText = faultReasonText(source.reason);
    snapshot.diagnostic = source.diagnostic;
    snapshot.runId = source.runId;
    snapshot.acceptedProductCount = source.acceptedProductCount;
    snapshot.completedProductCount = source.completedProductCount;
    snapshot.postFaultDroppedFrameCount =
            source.postFaultDroppedFrameCount;
    snapshot.occurredAtUtc = source.occurredAtUtc;
    return snapshot;
}

// 函数说明：enterFault 函数实现名称所表示的处理步骤。
void InspectionApplicationService::enterFault(
    InspectionFaultReason reason,
    const QString &diagnostic)
{
    if (!m_runtime->enterFault(reason, diagnostic)) {
        return;
    }
    publishSnapshot();
    emit faultEntered();
}

// 函数说明：runtimeSnapshot 函数执行对应事件或业务处理。
RuntimeSnapshot InspectionApplicationService::runtimeSnapshot() const
{
    RuntimeSnapshot snapshot;
    snapshot.state = applicationState(m_runtime->state());
    snapshot.cameraOpen = m_cameraSession->isOpen();
    snapshot.plcConnected = m_runtime->isPlcConnected();
    snapshot.runId = m_runtime->runId();
    snapshot.activeTemplatePaths = m_activeTemplatePaths;
    return snapshot;
}

// 函数说明：rejectStart 函数实现名称所表示的处理步骤。
StartInspectionResult InspectionApplicationService::rejectStart(
    InspectionStartIssue issue,
    const QString &code,
    const QString &userMessage,
    const QStringList &details,
    const QString &diagnostic) const
{
    StartInspectionResult result;
    result.issue = issue;
    result.error.code = code;
    result.error.userMessage = userMessage;
    result.error.diagnostic = diagnostic;
    result.details = details;
    result.snapshot = runtimeSnapshot();
    return result;
}

// 函数说明：plcFailure 函数实现名称所表示的处理步骤。
OperationResult InspectionApplicationService::plcFailure(
    const QString &code,
    const QString &userMessage,
    int nativeErrorCode) const
{
    return OperationResult::rejected(
                code,
                userMessage,
                QStringLiteral("nativeErrorCode=%1")
                .arg(nativeErrorCode));
}

// 函数说明：publishSnapshot 函数保存或发布对应的数据和资源。
void InspectionApplicationService::publishSnapshot()
{
    emit runtimeSnapshotChanged(runtimeSnapshot());
}
