#include "ui/controllers/inspection_start_controller.h"

#include "widget.h"
#include "ui_widget.h"

#include "DetectionModes.h"
#include "runtime/inspection_acquisition_controller.h"
#include "runtime/inspection_profile_snapshot.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_runtime_start_transaction.h"
#include "runtime/inspection_start_preflight.h"
#include "ui/controllers/inspection_runtime_ui_coordinator.h"
#include "ui/controllers/machine_settings_page_controller.h"
#include "ui/controllers/template_editor_controller.h"

#include <QDebug>
#include <QMessageBox>
#include <QPushButton>

#pragma execution_character_set("utf-8")

namespace {

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

} // namespace

InspectionStartController::InspectionStartController(Widget *host)
    : m_host(host)
{
}

void InspectionStartController::startInspection()
{
    if (!m_host) {
        return;
    }

    qDebug() << "=== on_plcbtn_clicked() START ===";

    InspectionStartAccessInput startAccess;
    startAccess.templateOperationActive =
            m_host->m_templateCaptureState != Widget::TemplateCaptureState::Idle
            || m_host->m_operationState == Widget::OperationState::TemplatePreviewing
            || m_host->m_operationState == Widget::OperationState::TemplateFrozen;
    startAccess.runtimeBusy =
            m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
            || m_host->isCollecting
            || m_host->hasRunningInspectionThread()
            || m_host->m_runtimeController.isBusy();
    startAccess.cameraOpen = m_host->m_bOpenDevice;

    InspectionStartPreflightResult accessResult =
            InspectionStartPreflight::evaluateAccess(startAccess);
    if (accessResult.issue
            == InspectionStartIssue::TemplateOperationActive) {
        QMessageBox::warning(
                    m_host,
                    "提示",
                    "当前正在制作模板，请先点击【退出模板制作】。");
        return;
    }
    if (accessResult.issue == InspectionStartIssue::RuntimeBusy) {
        QMessageBox::information(
                    m_host,
                    "提示",
                    "当前正在识别或停止中，请勿重复启动。");
        return;
    }
    if (accessResult.issue == InspectionStartIssue::CameraClosed) {
        QMessageBox::warning(m_host, "提示", "请先点击【打开相机】！");
        return;
    }

    m_host->updateHardwareParameterUiEnabled();
    m_host->m_settingsPageController->refreshAllDirty();
    m_host->refreshRecipeProfileDirty();

    startAccess = InspectionStartAccessInput();
    startAccess.cameraOpen = true;
    startAccess.dirtySettings = m_host->hasDirtySettings();
    startAccess.plcTriggerEnabled =
            m_host->m_appliedMachineSettings.triggerEnabled;
    startAccess.plcConnected =
            m_host->m_runtimeController.isPlcConnected();
    accessResult = InspectionStartPreflight::evaluateAccess(startAccess);

    if (accessResult.issue
            == InspectionStartIssue::DirtySettingsConfirmationRequired) {
        QMessageBox confirmBox(m_host);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("提示");
        confirmBox.setText(m_host->dirtySettingsMessage());
        QPushButton *continueButton =
                confirmBox.addButton("继续运行", QMessageBox::AcceptRole);
        QPushButton *cancelButton =
                confirmBox.addButton("取消", QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();

        if (confirmBox.clickedButton() != continueButton) {
            return;
        }

        m_host->restoreUnappliedSettingsFromApplied();
        startAccess.dirtySettings = false;
        startAccess.plcTriggerEnabled =
                m_host->m_appliedMachineSettings.triggerEnabled;
        startAccess.plcConnected =
                m_host->m_runtimeController.isPlcConnected();
        accessResult =
                InspectionStartPreflight::evaluateAccess(startAccess);
    }

    if (accessResult.issue == InspectionStartIssue::PlcDisconnected) {
        m_host->showParameterWarning("提示", "已启用 PLC 触发，但 PLC 未连接，请先连接 PLC。");
        return;
    }

    m_host->updateCurrentTemplateName();
    DetectionMode detectionMode;
    const QString modeId =
            m_host->m_appliedMachineSettings.detectModeId;
    const bool modeValid =
            detectionModeFromUiId(modeId, &detectionMode);
    const int modeIndex =
            machineSettingsDetectionModeIds().indexOf(modeId);
    const PreparedRecipeSnapshot prepared =
            m_host->m_templateEditorController
            ->activePreparedRecipe();
    const bool preparedForMode =
            modeValid
            && prepared
            && prepared->recipe
            && prepared->recipe->detectionMode
               == detectionMode;

    const bool isWordMode =
            modeValid
            && (detectionMode == DetectionMode::Word
                || detectionMode
                   == DetectionMode::BarcodeWord);
    const bool isBarcodeWordMode =
            modeValid
            && detectionMode == DetectionMode::BarcodeWord;
    const bool isTissueMode =
            modeValid
            && detectionMode == DetectionMode::Tissue;
    const bool isWordProfileMode =
            isWordMode
            && preparedForMode
            && !prepared->profiles.isEmpty();

    InspectionStartResourceInput resourceInput;
    if (isTissueMode) {
        resourceInput.modeKind =
                InspectionStartModeKind::Tissue;
    } else if (isBarcodeWordMode) {
        resourceInput.modeKind =
                InspectionStartModeKind::BarcodeWordProfiles;
    } else if (isWordMode) {
        resourceInput.modeKind =
                InspectionStartModeKind::WordProfiles;
    } else {
        resourceInput.modeKind =
                InspectionStartModeKind::SingleTemplate;
    }
    resourceInput.preparedRecipeReady = preparedForMode;

    if (preparedForMode
            && !prepared->profiles.isEmpty()) {
        const PreparedRecipeProfile &firstProfile =
                prepared->profiles.first();
        resourceInput.trackingTemplateReady =
                !firstProfile.trackingTemplate.empty();
        resourceInput.dateRegionReady =
                firstProfile.datePolygon.size() >= 3;
        resourceInput.targetTextRequired =
                detectionMode == DetectionMode::Stamp
                || detectionMode == DetectionMode::Ocr;
        resourceInput.targetTextReady =
                !firstProfile.definition.targetText
                .trimmed().isEmpty();
        resourceInput.characterTemplatesRequired =
                detectionMode == DetectionMode::Stamp;
        resourceInput.characterTemplatesReady =
                hasCompleteCharacterTemplates(firstProfile);
    }

    if (isWordMode && preparedForMode) {
        resourceInput.barcodeDecoderReady =
                !isBarcodeWordMode;
        if (isBarcodeWordMode) {
            resourceInput.barcodeDecoderReady =
                    m_host->m_barcodeDecoder->ensureLoaded();
            if (!resourceInput.barcodeDecoderReady) {
                resourceInput.barcodeDecoderError =
                        m_host->m_barcodeDecoder->lastError();
            }
        }

        for (const PreparedRecipeProfile &profile
             : prepared->profiles) {
            InspectionStartProfileReadiness readiness;
            readiness.displayName =
                    profile.definition.name;
            readiness.trackingTemplateReady =
                    !profile.trackingTemplate.empty();
            readiness.calibrationReady =
                    profile.datePolygon.size() >= 3;
            readiness.barcodeRegionReady =
                    !isBarcodeWordMode
                    || profile.barcodePolygon.size() == 4;
            readiness.dateRegionReady =
                    profile.datePolygon.size() >= 3;
            readiness.targetTextReady =
                    !profile.definition.targetText
                    .trimmed().isEmpty();
            readiness.characterTemplatesReady =
                    hasCompleteCharacterTemplates(profile);
            resourceInput.profiles.push_back(readiness);
        }
    }

    const InspectionStartPreflightResult resourceResult =
            InspectionStartPreflight::evaluateResources(resourceInput);
    if (resourceResult.issue
            == InspectionStartIssue::PreparedRecipeMissing) {
        QMessageBox::warning(
                    m_host,
                    QStringLiteral("启动资源预检失败"),
                    QStringLiteral(
                        "当前模式没有已完整准备的产品配方，请先创建或加载新格式配方。"));
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::WordProfilesMissing) {
        QMessageBox::warning(
                    m_host,
                    "提示",
                    "当前配方没有可用的Profile，请重新创建或加载产品配方。");
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::BarcodeResourcesInvalid) {
        QMessageBox::warning(
                    m_host,
                    "二维码+三期模板预检失败",
                    QString("以下问题必须处理后才能启动检测：\n\n%1")
                    .arg(resourceResult.details.join("\n")));
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::ProductTemplateIncomplete) {
        QMessageBox::warning(m_host, "操作规范",
                             QString("缺少可用产品模板，无法启动检测。\n\n"
                                     "具体原因：\n%1\n\n"
                             "请重新创建配方，或从【已发布配方】加载完整的新格式配方。")
                             .arg(resourceResult.details.join("\n")));
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::WordProfilesIncomplete) {
        QMessageBox::warning(
                    m_host,
                    "提示",
                    QString("以下产品模板还没有确认目标字符，不能启动检测：\n%1")
                    .arg(resourceResult.details.join("\n")));
        return;
    }

    const InspectionRunPlan runPlan =
            InspectionRunConfiguration::createPlan(
                resourceInput.modeKind,
                m_host->m_appliedMachineSettings.triggerEnabled);

    InspectionProfileSnapshot profileSnapshotForRun;
    if (isWordProfileMode) {
        profileSnapshotForRun =
                profileSnapshotFromPreparedRecipe(*prepared);
        if (!profileSnapshotForRun.isValid()) {
            QMessageBox::warning(m_host, "提示", "没有可用的字库定位配置。");
            return;
        }
    }

    std::vector<cv::Point2f> singleDatePolygon;
    cv::Rect2d singleTrackingBox;
    cv::Mat singleTrackingTemplate;
    if (preparedForMode && !prepared->profiles.isEmpty()) {
        const PreparedRecipeProfile &profile =
                prepared->profiles.first();
        singleDatePolygon = profile.datePolygon;
        singleTrackingBox = cv::Rect2d(
                    profile.definition.trackingRoi.x(),
                    profile.definition.trackingRoi.y(),
                    profile.definition.trackingRoi.width(),
                    profile.definition.trackingRoi.height());
        singleTrackingTemplate =
                profile.trackingTemplate.clone();
    }

    m_host->m_barcodeWordRunActive = false;

    if (m_host->imageLabel) {
        m_host->imageLabel->setTemplateDrawingEnabled(false);
    }
    m_host->hideTemplateGuide();
    if (m_host->m_runtimeUiCoordinator) {
        m_host->m_runtimeUiCoordinator->clearDetectionRoiWarning(QString());
    }

    // ==========================================================
    // 以下为原有启动线程逻辑，完全保留你所有的 PLC/相机 流程
    // ==========================================================
    if (runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger)
    {
        // 外部触发/硬触发模式逻辑
        if (m_host->isCollecting) {
            QMessageBox::information(m_host, "提示", "已在采集中，若要停止请点击【停止识别】按钮");
            return;
        }

        QStringList applyErrors;
        if (!m_host->applyCameraHardwareSettingsForRun(&applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = static_cast<float>(
                    m_host->m_appliedMachineSettings.cameraGain);

        m_host->ui->image_undetected->clear();
        m_host->ui->imagenum->clear();
        m_host->ui->ngnum->clear();
        m_host->ui->resultlabel_7->clear();
        m_host->ui->speedLabel->clear();
        m_host->m_runtimeController.resetStatistics();

        // 重置相机状态。具体SDK调用顺序由运行层统一维护。
        if (m_host->m_acquisitionController
                && m_host->m_acquisitionController->hasCamera()
                && m_host->m_bOpenDevice) {
            const InspectionCameraStartResult cameraStartResult =
                    m_host->m_acquisitionController->applyCameraStart(
                InspectionAcquisitionKind::HardwareTrigger,
                gainValue,
                [this](QString *errorMessage) {
                return m_host->applyCameraExposureValue(
                            m_host->m_appliedMachineSettings.cameraExposure,
                            errorMessage);
            });
            if (!cameraStartResult.isAccepted()) {
                if (cameraStartResult.issue
                        == InspectionCameraStartIssue::ExposureRejected) {
                    QMessageBox::warning(
                                m_host,
                                "启动失败",
                                QString("切换硬触发模式后恢复相机曝光失败：\n%1")
                                .arg(cameraStartResult.errorMessage));
                } else {
                    QMessageBox::critical(m_host, "错误", "相机初始化失败！");
                }
                return;
            }
        }

        m_host->m_acquisitionController->configureHardwareWorker(
                    runPlan,
                    profileSnapshotForRun.trackingProfiles,
                    singleDatePolygon,
                    singleTrackingBox,
                    singleTrackingTemplate);

        applyErrors.clear();
        if (!m_host->applyRuntimeThreadSettingsForRun(
                    prepared, &applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!m_host->applyPlcTriggerModeForRun(&applyErrors)
                || !m_host->applyPlcRunSettingsForRun(&applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前 PLC 参数下发失败：\n") + applyErrors.join("\n"));
            return;
        }

        if (isWordProfileMode) {
            qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                     << static_cast<int>(
                            profileSnapshotForRun.detectionProfiles.size())
                     << "mode:" << modeId;
        }
        m_host->m_barcodeWordRunActive = isBarcodeWordMode;
        m_host->m_resultBoundDisplayActive.store(true);
        InspectionRuntimeStartTransaction startTransaction(
                    m_host->m_runtimeController);
        if (!startTransaction.begin()) {
            m_host->m_resultBoundDisplayActive.store(false);
            m_host->m_barcodeWordRunActive = false;
            QMessageBox::warning(
                        m_host,
                        QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                        QStringLiteral("\u5f53\u524d\u8fd0\u884c\u72b6\u6001\u4e0d\u5141\u8bb8\u91cd\u590d\u542f\u52a8\u3002"));
            return;
        }
        qDebug() << "[RUNTIME_CONTROLLER] starting"
                 << startTransaction.runId();
        QString workerError;
        if (!m_host->startDetectionWorkerForMode(
                    startTransaction,
                    modeIndex,
                    prepared,
                    profileSnapshotForRun,
                    &workerError)) {
            startTransaction.rollback();
            m_host->m_resultBoundDisplayActive.store(false);
            m_host->m_barcodeWordRunActive = false;
            QMessageBox::warning(
                        m_host,
                        QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                        workerError);
            return;
        }
        qDebug() << "[DETECTION_WORKER] hard-trigger ingress enabled"
                 << "modeIndex=" << modeIndex;
        if (m_host->m_acquisitionController->startHardwareWorker()) {
            if (!startTransaction.commit()) {
                m_host->m_acquisitionController->requestHardwareStop();
                m_host->m_acquisitionController->waitForHardware(1500);
                m_host->m_resultBoundDisplayActive.store(false);
                m_host->m_barcodeWordRunActive = false;
                m_host->isCollecting = false;
                m_host->m_operationState = m_host->m_bOpenDevice
                        ? Widget::OperationState::CameraReady
                        : Widget::OperationState::CameraClosed;
                m_host->updateOperationUiState();
                QMessageBox::warning(
                            m_host,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8fd0\u884c\u72b6\u6001\u63d0\u4ea4\u5931\u8d25\u3002"));
                return;
            }
            m_host->isCollecting = true;
            m_host->m_operationState =
                    Widget::OperationState::Detecting;
            m_host->ui->statusLabel->setText("触发模式运行中");
            m_host->updateOperationUiState();
        } else {
            startTransaction.rollback();
            m_host->m_resultBoundDisplayActive = false;
            m_host->m_barcodeWordRunActive = false;
            m_host->isCollecting = false;
            m_host->m_operationState = m_host->m_bOpenDevice
                    ? Widget::OperationState::CameraReady
                    : Widget::OperationState::CameraClosed;
            m_host->updateOperationUiState();
        }
    }
    else
    {
        // 软触发/连续模式逻辑
        QStringList applyErrors;
        if (!m_host->applyCameraHardwareSettingsForRun(&applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = static_cast<float>(
                    m_host->m_appliedMachineSettings.cameraGain);

        m_host->m_acquisitionController->ensureWorkersReady();
        m_host->m_acquisitionController->configureSoftwareWorker(
                    runPlan,
                    profileSnapshotForRun.trackingProfiles,
                    singleDatePolygon,
                    singleTrackingBox,
                    singleTrackingTemplate);

        applyErrors.clear();
        if (!m_host->applyRuntimeThreadSettingsForRun(
                    prepared, &applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!m_host->applyPlcTriggerModeForRun(&applyErrors)
                || !m_host->applyPlcRunSettingsForRun(&applyErrors)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前 PLC 参数下发失败：\n") + applyErrors.join("\n"));
            return;
        }

        const InspectionCameraStartResult cameraStartResult =
                m_host->m_acquisitionController->applyCameraStart(
            InspectionAcquisitionKind::SoftwareTrigger,
            gainValue,
            [this](QString *errorMessage) {
            return m_host->applyCameraExposureValue(
                        m_host->m_appliedMachineSettings.cameraExposure,
                        errorMessage);
        });
        if (!cameraStartResult.isAccepted()) {
            if (cameraStartResult.issue
                    == InspectionCameraStartIssue::ExposureRejected) {
                QMessageBox::warning(
                            m_host,
                            "启动失败",
                            QString("切换软触发模式后恢复相机曝光失败：\n%1")
                            .arg(cameraStartResult.errorMessage));
            } else {
                QMessageBox::critical(m_host, "错误", "相机初始化失败！");
            }
            return;
        }
        if (!m_host->m_acquisitionController->isSoftwareRunning()) {
            if (isWordProfileMode) {
                qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                         << static_cast<int>(
                                profileSnapshotForRun.detectionProfiles.size())
                         << "mode:" << modeId;
            }
            m_host->m_resultBoundDisplayActive.store(true);
            // Publish the selected orchestration mode before acquisition can
            // emit its first frame.
            m_host->m_barcodeWordRunActive = isBarcodeWordMode;
            InspectionRuntimeStartTransaction startTransaction(
                        m_host->m_runtimeController);
            if (!startTransaction.begin()) {
                m_host->m_resultBoundDisplayActive.store(false);
                m_host->m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            m_host,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u5f53\u524d\u8fd0\u884c\u72b6\u6001\u4e0d\u5141\u8bb8\u91cd\u590d\u542f\u52a8\u3002"));
                return;
            }
            qDebug() << "[RUNTIME_CONTROLLER] starting"
                     << startTransaction.runId();
            QString workerError;
            if (!m_host->startDetectionWorkerForMode(
                        startTransaction,
                        modeIndex,
                        prepared,
                        profileSnapshotForRun,
                        &workerError)) {
                startTransaction.rollback();
                m_host->m_resultBoundDisplayActive.store(false);
                m_host->m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            m_host,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            workerError);
                return;
            }
            if (!m_host->m_acquisitionController->startSoftwareWorker()) {
                startTransaction.rollback();
                m_host->m_resultBoundDisplayActive.store(false);
                m_host->m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            m_host,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8f6f\u89e6\u53d1\u91c7\u96c6\u7ebf\u7a0b\u542f\u52a8\u5931\u8d25\u3002"));
                return;
            }
            if (!startTransaction.commit()) {
                m_host->m_acquisitionController->requestSoftwareStop();
                m_host->m_acquisitionController->waitForSoftware(1500);
                m_host->m_resultBoundDisplayActive.store(false);
                m_host->m_barcodeWordRunActive = false;
                m_host->isCollecting = false;
                m_host->m_operationState = m_host->m_bOpenDevice
                        ? Widget::OperationState::CameraReady
                        : Widget::OperationState::CameraClosed;
                m_host->updateOperationUiState();
                QMessageBox::warning(
                            m_host,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8fd0\u884c\u72b6\u6001\u63d0\u4ea4\u5931\u8d25\u3002"));
                return;
            }
            m_host->isCollecting = true;
            m_host->m_operationState =
                    Widget::OperationState::Detecting;
            m_host->ui->statusLabel->setText("软触发模式运行中");
            m_host->updateOperationUiState();
        }
    }


    qDebug() << "=== on_plcbtn_clicked() COMPLETED ===";
}
