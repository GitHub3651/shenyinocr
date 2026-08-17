#include "application/inspection_application_service.h"

#include "contracts/detection_mode.h"
#include "application/settings_application_service.h"
#include "recipes/recipe_store.h"
#include "runtime/camera_session.h"
#include "runtime/inspection_runtime.h"

#include <QDateTime>
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

InspectionStartModeKind modeKind(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Tissue:
        return InspectionStartModeKind::Tissue;
    case DetectionMode::Word:
        return InspectionStartModeKind::WordProfiles;
    case DetectionMode::BarcodeWord:
        return InspectionStartModeKind::BarcodeWordProfiles;
    case DetectionMode::Stamp:
    case DetectionMode::Ocr:
    default:
        return InspectionStartModeKind::SingleTemplate;
    }
}

InspectionTrackingKind trackingKind(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Tissue:
        return InspectionTrackingKind::WholeFrame;
    case DetectionMode::Word:
    case DetectionMode::BarcodeWord:
        return InspectionTrackingKind::WordProfiles;
    case DetectionMode::Stamp:
    case DetectionMode::Ocr:
    default:
        return InspectionTrackingKind::SingleTemplate;
    }
}

bool hasCompleteCharacterTemplates(
    const PreparedRecipeProfile &profile)
{
    const int targetCount = preparedRecipeTargetUnits(
                profile.definition.targetText).size();
    if (targetCount <= 0
            || profile.characterTemplates.empty()
            || profile.characterTemplates.size()
               != profile.characterTemplateTargetIndexes.size()) {
        return false;
    }
    QVector<bool> found(targetCount, false);
    for (const int index : profile.characterTemplateTargetIndexes) {
        if (index >= 0 && index < targetCount) {
            found[index] = true;
        }
    }
    for (const bool value : found) {
        if (!value) {
            return false;
        }
    }
    return true;
}

InspectionProfileSnapshot profileSnapshotFromPreparedRecipe(
    const PreparedRecipe &prepared)
{
    std::vector<InspectionProfileSource> sources;
    sources.reserve(static_cast<std::size_t>(prepared.profiles.size()));
    for (const PreparedRecipeProfile &profile : prepared.profiles) {
        InspectionProfileSource source;
        source.name = profile.definition.name;
        source.trackingTemplate = profile.trackingTemplate;
        source.barcodePoly = profile.barcodePolygon;
        source.datePoly = profile.datePolygon;
        source.targetText = profile.definition.targetText;
        source.imageThreshold =
                profile.definition.imageThresholdPercent;
        source.digitTemplates = profile.characterTemplates;
        source.digitTemplateTargetIndexes =
                profile.characterTemplateTargetIndexes;
        source.barcodeOptions.formatMask =
                profile.definition.barcodeParameters.formatMask;
        source.barcodeOptions.roiPaddingPercent =
                profile.definition.barcodeParameters.roiPaddingPercent;
        source.barcodeOptions.maxDecodeTimeMs =
                profile.definition.barcodeParameters.maxDecodeTimeMs;
        source.barcodeOptions.enableFallback =
                profile.definition.barcodeParameters.enableFallback;
        sources.push_back(source);
    }
    return InspectionProfileSnapshotBuilder::create(sources);
}

InspectionStartResourceInput resourceInputFor(
    const PreparedRecipe &prepared,
    DetectionMode mode,
    const BarcodeRuntimeReadiness &barcode)
{
    InspectionStartResourceInput input;
    input.modeKind = modeKind(mode);
    input.preparedRecipeReady = static_cast<bool>(prepared.recipe);
    if (prepared.profiles.isEmpty()) {
        return input;
    }

    const PreparedRecipeProfile &first = prepared.profiles.first();
    input.trackingTemplateReady = !first.trackingTemplate.empty();
    input.dateRegionReady = first.datePolygon.size() >= 3;
    input.targetTextRequired = mode == DetectionMode::Stamp
            || mode == DetectionMode::Ocr;
    input.targetTextReady =
            !first.definition.targetText.trimmed().isEmpty();
    input.characterTemplatesRequired =
            mode == DetectionMode::Stamp;
    input.characterTemplatesReady =
            hasCompleteCharacterTemplates(first);
    input.barcodeDecoderReady = barcode.ready;
    input.barcodeDecoderError = barcode.errorMessage;

    if (mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord) {
        for (const PreparedRecipeProfile &profile : prepared.profiles) {
            InspectionStartProfileReadiness readiness;
            readiness.displayName = profile.definition.name;
            readiness.trackingTemplateReady =
                    !profile.trackingTemplate.empty();
            readiness.calibrationReady =
                    profile.datePolygon.size() >= 3;
            readiness.barcodeRegionReady =
                    mode != DetectionMode::BarcodeWord
                    || profile.barcodePolygon.size() == 4;
            readiness.dateRegionReady =
                    profile.datePolygon.size() >= 3;
            readiness.targetTextReady =
                    !profile.definition.targetText.trimmed().isEmpty();
            readiness.characterTemplatesReady =
                    hasCompleteCharacterTemplates(profile);
            input.profiles.push_back(readiness);
        }
    }
    return input;
}

FramePreprocessSettings framePreprocessSettings(
    const MachineSettings &settings)
{
    FramePreprocessSettings output;
    const int rotation = machineSettingsRotationIds().indexOf(
                settings.imageRotationId);
    const int channel = machineSettingsColorChannelIds().indexOf(
                settings.colorChannelId);
    output.rotation = static_cast<FrameRotation>(rotation);
    output.colorChannel = static_cast<FrameColorChannel>(channel);
    return output;
}

CameraSessionCaptureConfiguration cameraConfiguration(
    const MachineSettings &settings,
    const PreparedRecipeSnapshot &prepared,
    const InspectionProfileSnapshot &profileSnapshot,
    const InspectionRunPlan &runPlan)
{
    CameraSessionCaptureConfiguration output;
    output.runPlan = runPlan;
    output.framePreprocess = framePreprocessSettings(settings);
    if (runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger) {
        output.hardwareTriggerDelayMicroseconds =
                static_cast<float>(settings.cameraDelay)
                * kMicrosecondsPerMillisecond;
    }
    output.exposure = settings.cameraExposure;
    output.gain = settings.cameraGain;
    output.trackingProfiles = profileSnapshot.trackingProfiles;
    if (prepared && !prepared->profiles.isEmpty()) {
        const PreparedRecipeProfile &profile = prepared->profiles.first();
        output.singleDatePolygon = profile.datePolygon;
        output.singleTrackingTemplate =
                profile.trackingTemplate.clone();
    }
    return output;
}

ResultServiceRunConfiguration resultConfiguration(
    const MachineSettings &settings)
{
    ResultServiceRunConfiguration configuration;
    configuration.imageSaveModeIndex =
            machineSettingsImageSaveModeIds().indexOf(
                settings.imageSaveModeId);
    configuration.plcOutputEnabled = settings.triggerEnabled;
    configuration.delayedNgOffset = settings.rejectPosition;
    configuration.saveOptions.rootDirectory = settings.imageSavePath;
    configuration.saveOptions.format = QStringLiteral("jpg");
    configuration.saveOptions.quality = settings.imageJpegQuality;
    configuration.saveOptions.imageContentModeIndex =
            machineSettingsImageSaveTypeIds().indexOf(
                settings.imageSaveTypeId);
    return configuration;
}

PlcRunSettingsCommand plcRunSettings(const MachineSettings &settings)
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
    case InspectionStartIssue::PreparedRecipeMissing:
        return QStringLiteral("INSPECTION_RECIPE_NOT_PREPARED");
    case InspectionStartIssue::WordProfilesMissing:
        return QStringLiteral("INSPECTION_PROFILE_MISSING");
    case InspectionStartIssue::BarcodeResourcesInvalid:
        return QStringLiteral("INSPECTION_BARCODE_RESOURCE_INVALID");
    case InspectionStartIssue::ProductTemplateIncomplete:
        return QStringLiteral("INSPECTION_PRODUCT_TEMPLATE_INCOMPLETE");
    case InspectionStartIssue::WordProfilesIncomplete:
        return QStringLiteral("INSPECTION_PROFILE_INCOMPLETE");
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
    case InspectionStartIssue::PreparedRecipeMissing:
        return QStringLiteral("当前模式没有已完整准备的产品配方，请先创建或加载新格式配方。");
    case InspectionStartIssue::WordProfilesMissing:
        return QStringLiteral("当前配方没有可用的Profile，请重新创建或加载产品配方。");
    case InspectionStartIssue::BarcodeResourcesInvalid:
        return QStringLiteral("二维码+三期模板资源预检失败。");
    case InspectionStartIssue::ProductTemplateIncomplete:
        return QStringLiteral("缺少可用产品模板，无法启动检测。");
    case InspectionStartIssue::WordProfilesIncomplete:
        return QStringLiteral("部分产品模板还没有确认目标字符，不能启动检测。");
    case InspectionStartIssue::None:
    default:
        return QString();
    }
}

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

InspectionAcquisitionDto acquisitionDto(
        InspectionAcquisitionKind kind)
{
    return kind == InspectionAcquisitionKind::HardwareTrigger
            ? InspectionAcquisitionDto::HardwareTrigger
            : InspectionAcquisitionDto::SoftwareTrigger;
}

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

InspectionApplicationService::InspectionApplicationService(
    const std::shared_ptr<InspectionRuntime> &runtime,
    const std::shared_ptr<CameraSession> &cameraSession,
    const std::shared_ptr<SettingsApplicationService> &settings,
    const std::shared_ptr<RecipeStore> &recipes,
    QObject *parent)
    : QObject(parent),
      m_runtime(runtime),
      m_cameraSession(cameraSession),
      m_settings(settings),
      m_recipes(recipes)
{
    qRegisterMetaType<RuntimeSnapshot>("RuntimeSnapshot");
    qRegisterMetaType<InspectionFaultReason>(
                "InspectionFaultReason");
    CameraSessionCallbacks callbacks;
    callbacks.streamingFrameReady = [this](const cv::Mat &image) {
        emit streamingFrameReady(image.clone());
    };
    callbacks.trackingPoseReady = [this](const DetectionPose &pose) {
        m_runtime->resultService().updatePose(pose);
    };
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

InspectionApplicationService::~InspectionApplicationService()
{
    m_cameraSession->setCallbacks(CameraSessionCallbacks());
}

StartInspectionResult InspectionApplicationService::start(
    const StartInspectionCommand &command)
{
    const MachineSettings settings = m_settings->current();
    InspectionStartAccessInput access;
    access.templateOperationActive = command.templateOperationActive;
    access.runtimeBusy = m_runtime->isBusy();
    access.cameraOpen = m_cameraOpen;
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
                    InspectionStartIssue::PreparedRecipeMissing,
                    QStringLiteral("INSPECTION_DETECTION_MODE_INVALID"),
                    QStringLiteral("机器设置中的检测模式无效。"));
    }
    const QString recipeId = settings.publishedRecipeIdsByMode
            .value(settings.detectModeId).trimmed();
    if (recipeId.isEmpty()) {
        return rejectStart(
                    InspectionStartIssue::PreparedRecipeMissing,
                    startIssueCode(
                        InspectionStartIssue::PreparedRecipeMissing),
                    startIssueMessage(
                        InspectionStartIssue::PreparedRecipeMissing));
    }

    PreparedRecipeSnapshot prepared;
    QString recipeError;
    if (!m_recipes
            || !m_recipes->loadPreparedRecipe(
                recipeId, &prepared, &recipeError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode != detectionMode) {
        return rejectStart(
                    InspectionStartIssue::PreparedRecipeMissing,
                    QStringLiteral("INSPECTION_RECIPE_PREPARE_FAILED"),
                    QStringLiteral("当前产品配方无效或与检测模式不匹配。"),
                    QStringList(),
                    recipeError);
    }

    BarcodeRuntimeReadiness barcode;
    if (detectionMode == DetectionMode::BarcodeWord) {
        barcode = m_runtime->preparePipeline(detectionMode);
    }
    const InspectionStartResourceInput resourceInput =
            resourceInputFor(*prepared, detectionMode, barcode);
    const InspectionStartPreflightResult resourceResult =
            InspectionStartPreflight::evaluateResources(resourceInput);
    if (!resourceResult.isAccepted()) {
        return rejectStart(
                    resourceResult.issue,
                    startIssueCode(resourceResult.issue),
                    startIssueMessage(resourceResult.issue),
                    resourceResult.details);
    }

    const InspectionRunPlan runPlan = InspectionRunConfiguration::createPlan(
                trackingKind(detectionMode),
                settings.triggerEnabled,
                detectionMode == DetectionMode::BarcodeWord);
    if (runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger) {
        m_runtime->resetStatistics();
    }
    InspectionProfileSnapshot profileSnapshot;
    if (detectionMode == DetectionMode::Word
            || detectionMode == DetectionMode::BarcodeWord) {
        profileSnapshot = profileSnapshotFromPreparedRecipe(*prepared);
        if (!profileSnapshot.isValid()) {
            return rejectStart(
                        InspectionStartIssue::WordProfilesMissing,
                        QStringLiteral("INSPECTION_PROFILE_SNAPSHOT_INVALID"),
                        QStringLiteral("没有可用的字库定位配置。"));
        }
    }

    const QString runId = m_runtime->beginStart(
                settings, prepared, profileSnapshot);
    if (runId.isEmpty()) {
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    startIssueCode(InspectionStartIssue::RuntimeBusy),
                    startIssueMessage(InspectionStartIssue::RuntimeBusy));
    }
    publishSnapshot();

    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        MachineSettings adjusted = m_settings->current();
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
                    prepared,
                    profileSnapshot,
                    runPlan),
                &cameraError)) {
        const InspectionCameraRecoveryResult recovery =
                m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_cameraOpen = recovery.cameraOpen;
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
        const OperationResult trigger = applyPlcTriggerMode(
                    settings.triggerModeId);
        const OperationResult parameters = trigger.isSuccess()
                ? applyPlcRunSettings(plcRunSettings(settings))
                : trigger;
        if (!trigger.isSuccess() || !parameters.isSuccess()) {
            m_cameraSession->stopInspection();
            const InspectionCameraRecoveryResult recovery =
                    m_cameraSession->restorePreviewReady(
                        settings.cameraExposure, persistExposure);
            m_cameraOpen = recovery.cameraOpen;
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
    if (!m_runtime->startPipeline(
                resultConfiguration(settings),
                &executionError)) {
        m_cameraSession->stopInspection();
        const InspectionCameraRecoveryResult recovery =
                m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_cameraOpen = recovery.cameraOpen;
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
        const InspectionCameraRecoveryResult recovery =
                m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_cameraOpen = recovery.cameraOpen;
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
        const InspectionCameraRecoveryResult recovery =
                m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_cameraOpen = recovery.cameraOpen;
        m_runtime->rollbackStart();
        publishSnapshot();
        return rejectStart(
                    InspectionStartIssue::RuntimeBusy,
                    QStringLiteral("INSPECTION_START_COMMIT_FAILED"),
                    QStringLiteral("运行状态提交失败。"));
    }

    m_activeRecipeId = recipeId;
    publishSnapshot();
    StartInspectionResult result;
    result.acquisitionKind = acquisitionDto(runPlan.acquisitionKind);
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

    const MachineSettings settings = m_settings->current();
    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        MachineSettings adjusted = m_settings->current();
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
        result.cameraRecovery.cameraOpen = m_cameraOpen;
    }
    m_cameraOpen = result.cameraRecovery.cameraOpen;
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

    m_activeRecipeId.clear();
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
    if (m_cameraOpen) {
        result.operation = OperationResult::rejected(
                    QStringLiteral("CAMERA_ALREADY_OPEN"),
                    QStringLiteral("相机已连接！"));
        result.snapshot = runtimeSnapshot();
        return result;
    }

    const MachineSettings settings = m_settings->current();
    const PlcOperationResult plc = m_runtime->connectPlc(
                plcCommand.address,
                plcCommand.rack,
                plcCommand.slot);
    result.plcConnectionFailed = !plc.isSuccess();
    result.plcNativeErrorCode = plc.nativeErrorCode;

    const CameraSession::PersistAdjustedExposure persistExposure =
            [this](int adjustedExposure, QString *errorMessage) {
        MachineSettings adjusted = m_settings->current();
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
    m_cameraOpen = true;
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
    if (m_cameraOpen) {
        m_cameraSession->close();
    }
    m_cameraOpen = false;
    m_runtime->resetStatistics();
    publishSnapshot();
    return OperationResult::accepted();
}

OperationResult InspectionApplicationService::connectPlc(
    const PlcConnectionCommand &command)
{
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
    if (!m_runtime->isPlcConnected()) {
        return OperationResult::rejected(
                    QStringLiteral("PLC_NOT_CONNECTED"),
                    QStringLiteral("PLC未连接！"));
    }
    const int modeIndex = machineSettingsTriggerModeIds().indexOf(modeId);
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

OperationResult InspectionApplicationService::writePlcPhotoDistance(
    std::uint32_t value)
{
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
InspectionApplicationService::queryCameraExposureRange()
{
    return cameraParameterDto(m_cameraSession->queryExposureRange());
}

CameraParameterResultDto
InspectionApplicationService::queryCameraGainRange()
{
    return cameraParameterDto(m_cameraSession->queryGainRange());
}

CameraParameterResultDto
InspectionApplicationService::applyCameraExposure(int exposure)
{
    return cameraParameterDto(m_cameraSession->applyExposure(exposure));
}

CameraParameterResultDto
InspectionApplicationService::applyCameraGain(int gain)
{
    return cameraParameterDto(m_cameraSession->applyGain(gain));
}

bool InspectionApplicationService::startTemplatePreview(
    quint64 sessionId,
    int rotationCode,
    int colorChannelCode,
    QString *errorMessage)
{
    FramePreprocessSettings settings;
    settings.rotation = static_cast<FrameRotation>(rotationCode);
    settings.colorChannel =
            static_cast<FrameColorChannel>(colorChannelCode);
    return m_cameraSession->startPreview(
                sessionId, settings, errorMessage);
}

bool InspectionApplicationService::stopTemplatePreview()
{
    return m_cameraSession->stopPreview();
}

void InspectionApplicationService::acknowledgeTemplatePreviewFrame(
    quint64 sessionId)
{
    m_cameraSession->acknowledgePreviewFrame(sessionId);
}

bool InspectionApplicationService::hasCurrentCameraImage() const
{
    return m_cameraSession->hasCurrentImage();
}

cv::Mat InspectionApplicationService::currentCameraImageClone() const
{
    return m_cameraSession->currentImageClone();
}

void InspectionApplicationService::replaceCurrentCameraImage(
    const cv::Mat &image)
{
    m_cameraSession->replaceCurrentImage(image);
}

bool InspectionApplicationService::isCameraOpen() const
{
    return m_cameraOpen && m_cameraSession->isOpen();
}

bool InspectionApplicationService::isCapturing() const
{
    return m_cameraSession->isCapturing();
}

void InspectionApplicationService::shutdown()
{
    m_runtime->beginStop();
    m_cameraSession->stopInspection();
    m_runtime->waitForStop();
    if (m_cameraOpen) {
        m_cameraSession->close();
        m_cameraOpen = false;
    }
    if (m_runtime->isPlcConnected()) {
        m_runtime->disconnectPlc();
    }
    m_runtime->finishStop();
    m_activeRecipeId.clear();
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
    if (m_cameraOpen) {
        const MachineSettings settings = m_settings->current();
        const CameraSession::PersistAdjustedExposure persistExposure =
                [this](int adjustedExposure, QString *errorMessage) {
            MachineSettings adjusted = m_settings->current();
            adjusted.cameraExposure = adjustedExposure;
            m_settings->updateDraft(adjusted);
            const OperationResult saved = m_settings->applyDraft();
            if (!saved.isSuccess() && errorMessage) {
                *errorMessage = saved.error.userMessage;
            }
            return saved.isSuccess();
        };
        const InspectionCameraRecoveryResult recovery =
                m_cameraSession->restorePreviewReady(
                    settings.cameraExposure, persistExposure);
        m_cameraOpen = recovery.cameraOpen;
    }
    m_runtime->finishStop();
    m_activeRecipeId.clear();
    publishSnapshot();
}

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

void InspectionApplicationService::clearUiBindings()
{
    m_runtime->resultService().setCallbacks(ResultServiceCallbacks());
    m_runtime->resultService().bindView(
                InspectionPresentationViewBindings());
}

void InspectionApplicationService::clearResultView()
{
    m_runtime->resultService().clear();
}

void InspectionApplicationService::clearTransientView()
{
    m_runtime->resultService().clearTransientView();
}

void InspectionApplicationService::presentPreviewFrame(
    const cv::Mat &image,
    bool tissueMode,
    bool productionRunning)
{
    m_runtime->resultService().presentPreviewFrame(
                image, tissueMode, productionRunning);
}

void InspectionApplicationService::resetStatistics()
{
    m_runtime->resetStatistics();
    m_runtime->resultService().presentTotalAndNgCounts(
                m_runtime->totalCount(), m_runtime->ngCount());
}

void InspectionApplicationService::resetNgCount()
{
    m_runtime->resetNgCount();
    m_runtime->resultService().presentNgCount(m_runtime->ngCount());
}

void InspectionApplicationService::clearPendingDelayedNgRequests()
{
    m_runtime->clearPendingDelayedNgRequests();
}

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

RuntimeSnapshot InspectionApplicationService::runtimeSnapshot() const
{
    RuntimeSnapshot snapshot;
    snapshot.state = applicationState(m_runtime->state());
    snapshot.cameraOpen = m_cameraOpen;
    snapshot.plcConnected = m_runtime->isPlcConnected();
    snapshot.runId = m_runtime->runId();
    snapshot.recipeId = m_activeRecipeId;
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
