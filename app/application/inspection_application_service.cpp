#include "application/inspection_application_service.h"

#include "contracts/detection_mode.h"
#include "application/settings_application_service.h"
#include "templates/template_store.h"
#include "runtime/camera_session.h"
#include "runtime/inspection_runtime.h"
#include "system_support/logging/log_categories.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QVector>

namespace {

constexpr float kMicrosecondsPerMillisecond = 1000.0f;

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

ResultServiceRunConfiguration resultConfiguration(
    const AppSettings &settings,
    DetectionMode detectionMode)
{
    ResultServiceRunConfiguration configuration;
    configuration.imageSaveModeIndex =
            appSettingsImageSaveModeIds().indexOf(
                settings.imageSaveModeId);
    configuration.plcOutputEnabled = settings.triggerEnabled;
    configuration.delayedNgOffset = settings.rejectPosition;
    configuration.saveOptions.rootDirectory = settings.imageSavePath;
    configuration.saveOptions.format = QStringLiteral("jpg");
    configuration.saveOptions.imageContentModeIndex =
            appSettingsImageSaveTypeIds().indexOf(
                settings.imageSaveTypeId);
    configuration.barcodeCsvEnabled = settings.barcodeCsvEnabled
            && detectionMode == DetectionMode::BarcodeWord;
    configuration.barcodeCsvOutputDirectory =
            configuration.barcodeCsvEnabled
            ? settings.barcodeCsvOutputDirectory
            : QString();
    return configuration;
}

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
        return QStringLiteral("二维码识别组件无法使用。");
    case InspectionStartIssue::TemplateIncomplete:
        return QStringLiteral("模板尚未制作完整，无法启动检测。");
    case InspectionStartIssue::TemplatesIncomplete:
        return QStringLiteral("当前选择中没有制作完整的模板。");
    case InspectionStartIssue::None:
    default:
        return QString();
    }
}

QString faultReasonText(InspectionFaultReason reason)
{
    switch (reason) {
    case InspectionFaultReason::CameraDisconnected:
        return QStringLiteral("相机连接或图像采集异常");
    case InspectionFaultReason::PlcDisconnected:
        return QStringLiteral("PLC 连接或检测结果发送异常");
    case InspectionFaultReason::HardTriggerQueueOverflow:
        return QStringLiteral("待检测图像过多，系统已暂停");
    case InspectionFaultReason::ProductIdentityAmbiguous:
        return QStringLiteral("无法确定当前图像对应的产品");
    case InspectionFaultReason::RuntimeInvariantViolation:
        return QStringLiteral("系统状态异常，检测已暂停");
    case InspectionFaultReason::BarcodeCsvUnavailable:
        return QStringLiteral("二维码结果无法保存到本机");
    case InspectionFaultReason::None:
        break;
    }
    return QString();
}

InspectionAcquisitionDto acquisitionDto(
        bool hardwareTriggerEnabled)
{
    return hardwareTriggerEnabled
            ? InspectionAcquisitionDto::HardwareTrigger
            : InspectionAcquisitionDto::SoftwareTrigger;
}

} // namespace

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
    callbacks.previewFrameReady = [this](const cv::Mat &image) {
        emit templatePreviewFrameReady(image);
    };
    callbacks.previewFailed = [this](const QString &reason) {
        emit templatePreviewFailed(reason);
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

InspectionApplicationService::~InspectionApplicationService()
{
    m_cameraSession->setCallbacks(CameraSessionCallbacks());
}

StartInspectionResult InspectionApplicationService::start(
    const StartInspectionCommand &command)
{
    const AppSettings settings = m_settings->current();
    InspectionStartAccessInput access;
    access.runtimeBusy = m_runtime->isBusy();
    access.templateOperationActive = !access.runtimeBusy
            && m_cameraSession->isCapturing();
    access.cameraOpen = m_cameraSession->isOpen();
    access.dirtySettings = !command.unappliedChanges.isEmpty();
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
                    QStringLiteral("当前选择的检测模式无效。"));
    }
    if (detectionMode == DetectionMode::BarcodeWord
            && settings.barcodeCsvEnabled
            && !QDir().mkpath(settings.barcodeCsvOutputDirectory)) {
        return rejectStart(
                    InspectionStartIssue::None,
                    QStringLiteral("BARCODE_CSV_DIRECTORY_UNAVAILABLE"),
                    QStringLiteral(
                        "二维码结果保存文件夹不可用，请重新选择。"),
                    QStringList(),
                    settings.barcodeCsvOutputDirectory);
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
                ? QStringLiteral("模板无法使用。")
                : templateError.userMessage;
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=run.template_load_failed code=%1 path=%2 diagnostic=%3")
                   .arg(templateError.code, path,
                        templateError.diagnostic);
        const QString warning = QStringLiteral(
                    "模板“%1”无法使用：%2")
                .arg(QFileInfo(path).fileName(),
                     reason);
        if (detectionMode == DetectionMode::Stamp
                || detectionMode == DetectionMode::Ocr) {
            return rejectStart(
                        InspectionStartIssue::TemplateIncomplete,
                        templateError.code.isEmpty()
                        ? QStringLiteral("TEMPLATE_LOAD_FAILED")
                        : templateError.code,
                        warning,
                        QStringList() << warning,
                        QStringLiteral("path=%1; diagnostic=%2")
                        .arg(path, templateError.diagnostic));
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
    MultiTemplateRuntimeSnapshot multiTemplateSnapshot;
    if (detectionModeDescriptor(detectionMode).trackingKind
            == DetectionTrackingKind::MultipleTemplates) {
        multiTemplateSnapshot = MultiTemplateRuntimeSnapshotBuilder::create(
                    preparedTemplates);
        if (!multiTemplateSnapshot.isValid()) {
            return rejectStart(
                        InspectionStartIssue::TemplatesMissing,
                        QStringLiteral("INSPECTION_TEMPLATE_SNAPSHOT_INVALID"),
                        QStringLiteral("当前模板未准备好。"));
        }
    }

    const QString runId = m_runtime->beginStart(
                settings, detectionMode, preparedTemplates,
                multiTemplateSnapshot,
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
        const OperationResult saved =
                m_settings->saveConfiguration(adjusted);
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
                    QStringLiteral("相机尚未准备好，无法开始检测。"),
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
                        QStringLiteral("PLC 参数应用失败，无法开始检测。"),
                        QStringList(),
                        error.diagnostic);
        }
    }

    QString executionError;
    if (!m_runtime->startDetection(
                resultConfiguration(settings, detectionMode),
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
                    ? QStringLiteral("无法开始检测，请重试。")
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
                    QStringLiteral("图像采集启动失败。"),
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
                    QStringLiteral("无法开始检测，请重试。"));
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
        const OperationResult saved =
                m_settings->saveConfiguration(adjusted);
        if (!saved.isSuccess() && errorMessage) {
            *errorMessage = saved.error.userMessage;
        }
        return saved.isSuccess();
    };
    if (acquisition.shouldRestoreCamera()) {
        result.cameraRecovery = m_cameraSession->restorePreviewReady(
                    settings.cameraExposure,
                    persistExposure);
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
                    "停止检测时发生系统故障。");
        result.snapshot = runtimeSnapshot();
        publishSnapshot();
        return result;
    }

    if (recoveringFault) {
        const int unconfirmed = m_runtime->reconcileFaultProducts();
        result.reconciliationSummary = QStringLiteral(
                    "视觉检测已暂停，输送线状态未知。\n"
                    "本次未完成产品记为未确认：%1件。\n"
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
                    "停止检测后，相机曝光恢复失败，请检查相机状态。");
        result.error.diagnostic =
                result.cameraRecovery.errorMessage;
    }
    result.snapshot = runtimeSnapshot();
    publishSnapshot();
    return result;
}

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
        const OperationResult saved =
                m_settings->saveConfiguration(adjusted);
        if (!saved.isSuccess() && errorMessage) {
            *errorMessage = saved.error.userMessage;
        }
        return saved.isSuccess();
    };
    result.camera = m_cameraSession->openFirst(
                settings.cameraExposure, persistExposure);
    if (!result.camera.isSuccess()) {
        result.operation = OperationResult::rejected(
                    QStringLiteral("CAMERA_OPEN_FAILED"),
                    QStringLiteral("相机打开失败，请检查相机连接和参数。"),
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
    QString message = QStringLiteral("PLC 运行参数应用失败");
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
    return m_cameraSession->queryGainRange();
}

CameraParameterResultDto
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
    return m_cameraSession->applyExposure(exposure);
}

CameraParameterResultDto
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
    return m_cameraSession->applyGain(gain);
}

OperationResult InspectionApplicationService::startTemplatePreview(
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
                    QStringLiteral("相机正在采集图像，请先停止检测。"));
    }
    const CameraParameterResultDto exposure =
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
            settings, &errorMessage)) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_PREVIEW_START_FAILED"),
                    QStringLiteral("无法开始实时取景，请重试。"),
                    errorMessage);
    }
    return OperationResult::accepted();
}

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
                QStringLiteral("实时取景尚未停止，请稍后重试。"));
}

bool InspectionApplicationService::hasCurrentCameraImage() const
{
    return m_cameraSession->hasCurrentImage();
}

cv::Mat InspectionApplicationService::currentCameraImageClone() const
{
    return m_cameraSession->currentImageClone();
}

bool InspectionApplicationService::isCameraOpen() const
{
    return m_cameraSession->isOpen();
}

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
            const OperationResult saved =
                    m_settings->saveConfiguration(adjusted);
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

QImage InspectionApplicationService::renderPreviewFrame(
    const cv::Mat &image,
    bool tissueMode,
    bool productionRunning)
{
    if (image.empty() || (tissueMode && productionRunning)) {
        return QImage();
    }
    return m_runtime->renderPreviewFrame(image);
}

OperationResult InspectionApplicationService::resetStatistics()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("STATISTICS_RUNTIME_BUSY"),
                    QStringLiteral("请先停止当前任务再清零统计。"));
    }
    m_runtime->resetStatistics();
    return OperationResult::accepted();
}

OperationResult InspectionApplicationService::resetNgCount()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("STATISTICS_RUNTIME_BUSY"),
                    QStringLiteral("请先停止当前任务再清零统计。"));
    }
    m_runtime->resetNgCount();
    return OperationResult::accepted();
}

OperationResult
InspectionApplicationService::clearPendingDelayedNgRequests()
{
    if (m_runtime->state() != InspectionRuntimeState::Idle) {
        return OperationResult::rejected(
                    QStringLiteral("REJECT_QUEUE_RUNTIME_BUSY"),
                    QStringLiteral("请先停止检测，再清除待执行的剔除动作。"));
    }
    m_runtime->clearPendingDelayedNgRequests();
    return OperationResult::accepted();
}

void InspectionApplicationService::checkPlcHealth()
{
    if (!m_runtime->isRunning()
            || !m_runtime->requiresPlcForRun()
            || m_runtime->isPlcConnected()) {
        return;
    }
    enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("运行中 PLC 连接状态已断开。"));
}

void InspectionApplicationService::enterFault(
    InspectionFaultReason reason,
    const QString &diagnostic)
{
    if (!m_runtime->enterFault(reason, diagnostic)) {
        return;
    }
    publishSnapshot();
}

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

void InspectionApplicationService::publishSnapshot()
{
    emit runtimeSnapshotChanged(runtimeSnapshot());
}
