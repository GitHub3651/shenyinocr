#include "ui/controllers/inspection_start_controller.h"

#include "widget.h"
#include "ui_widget.h"

#include "DetectionModes.h"
#include "runtime/inspection_acquisition_controller.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_runtime_start_transaction.h"
#include "runtime/inspection_start_preflight.h"
#include "ui/controllers/inspection_runtime_ui_coordinator.h"
#include "ui/controllers/machine_settings_page_controller.h"
#include "ui/controllers/template_editor_controller.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

#pragma execution_character_set("utf-8")

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
    m_host->refreshTemplatePrivateSettingDirty();

    startAccess = InspectionStartAccessInput();
    startAccess.cameraOpen = true;
    startAccess.dirtySettings = m_host->hasDirtySettings();
    startAccess.plcTriggerEnabled = m_host->ui->checkBox->isChecked();
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
        startAccess.plcTriggerEnabled = m_host->ui->checkBox->isChecked();
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
    const bool isWordMode = isWordFamilyMode(m_host->currentDetectModeId());
    const bool isBarcodeWordMode =
            m_host->currentDetectModeId() == BarcodeWordDetectionMode;
    const bool isTissueMode = (m_host->ui->comboBox_4->currentIndex() == 3);
    const bool isWordProfileMode =
            isWordMode && !m_host->m_templateEditorController->wordTemplateProfiles().empty();

    InspectionStartResourceInput resourceInput;
    if (isTissueMode) {
        resourceInput.modeKind = InspectionStartModeKind::Tissue;
    } else if (isBarcodeWordMode) {
        resourceInput.modeKind =
                InspectionStartModeKind::BarcodeWordProfiles;
    } else if (isWordMode) {
        resourceInput.modeKind = InspectionStartModeKind::WordProfiles;
    } else {
        resourceInput.modeKind =
                InspectionStartModeKind::SingleTemplate;
    }
    resourceInput.productTemplateDirectorySelected =
            !m_host->currentTemplateDirPath.trimmed().isEmpty();
    resourceInput.trackingTemplateReady =
            !m_host->m_loadedTrackingTemplate.empty();
    resourceInput.dateRegionReady = !m_host->savedDatePoly.empty();

    if (isWordMode) {
        resourceInput.barcodeDecoderReady = !isBarcodeWordMode;
        if (isBarcodeWordMode
                && !m_host->m_templateEditorController->wordTemplateProfiles().empty()) {
            resourceInput.barcodeDecoderReady =
                    m_host->m_barcodeDecoder->ensureLoaded();
            if (!resourceInput.barcodeDecoderReady) {
                resourceInput.barcodeDecoderError =
                        m_host->m_barcodeDecoder->lastError();
            }
        }

        for (const WordTemplateProfile &profile
             : m_host->m_templateEditorController->wordTemplateProfiles()) {
            const QString profileName = profile.name.isEmpty()
                    ? QDir(profile.dirPath).dirName()
                    : profile.name;
            InspectionStartProfileReadiness readiness;
            readiness.displayName = profileName;
            readiness.targetTextReady =
                    !profile.settings.targetText.trimmed().isEmpty();
            readiness.characterTemplatesReady =
                    !profile.digitTemplates.empty();

            if (isBarcodeWordMode) {
                const QString trackingPath =
                        m_host->wordTemplateProfileAssetPath(
                            profile,
                            QStringLiteral("trackingTemplate"),
                            QStringLiteral("tracking_template.bmp"));
                QFile trackingFile(trackingPath);
                cv::Mat diskTrackingTemplate;
                if (trackingFile.open(QIODevice::ReadOnly)) {
                    const QByteArray bytes = trackingFile.readAll();
                    if (!bytes.isEmpty()) {
                        try {
                            const std::vector<uchar> buffer(
                                        bytes.begin(),
                                        bytes.end());
                            diskTrackingTemplate =
                                    cv::imdecode(
                                        buffer,
                                        cv::IMREAD_COLOR);
                        } catch (...) {
                            diskTrackingTemplate.release();
                        }
                    }
                }
                readiness.trackingTemplateReady =
                        !profile.trackingTemplate.empty()
                        && !diskTrackingTemplate.empty();

                CalibrationData diskCalibration;
                const QString calibrationPath =
                        m_host->wordTemplateProfileAssetPath(
                            profile,
                            QStringLiteral("calibration"),
                            QStringLiteral("calibrate_config.yaml"));
                readiness.calibrationReady =
                        QFileInfo::exists(calibrationPath)
                        && diskCalibration.load(
                            calibrationPath
                            .toLocal8Bit()
                            .toStdString());
                if (readiness.calibrationReady) {
                    readiness.barcodeRegionReady =
                            diskCalibration.barcode_poly.size() == 4
                            && profile.barcodePoly.size() == 4;
                    readiness.dateRegionReady =
                            diskCalibration.date_poly.size() >= 3
                            && profile.datePoly.size() >= 3;
                }

                readiness.targetTextReady =
                        readiness.targetTextReady
                        && profile.targetCount > 0;
                readiness.characterTemplatesReady =
                        readiness.characterTemplatesReady
                        && profile.digitTemplates.size()
                           == profile.digitTemplateTargetIndexes.size();
            }
            resourceInput.profiles.push_back(readiness);
        }
    }

    const InspectionStartPreflightResult resourceResult =
            InspectionStartPreflight::evaluateResources(resourceInput);
    if (resourceResult.issue
            == InspectionStartIssue::WordProfilesMissing) {
        QMessageBox::warning(
                    m_host,
                    "提示",
                    "当前没有加载产品模板，请重新选择产品模板文件夹。");
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
                                     "如果是新产品：\n"
                                     "请先【拍照】，框选定位区域和喷码检测区域，然后点击【保存模板】。\n\n"
                                     "如果是已有产品：\n"
                                     "请点击【选择模板】，选择对应产品模板文件夹。")
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
                m_host->ui->checkBox->isChecked());

    InspectionProfileSnapshot profileSnapshotForRun;
    if (isWordProfileMode) {
        profileSnapshotForRun = m_host->createWordTemplateRunSnapshot();
        if (!profileSnapshotForRun.isValid()) {
            QMessageBox::warning(m_host, "提示", "没有可用的字库定位配置。");
            return;
        }
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
        if (!m_host->applyCameraHardwareSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = m_host->ui->lineEdit_14->text().toFloat();

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
                            m_host->m_appliedGlobalSettings.cameraExposure,
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
                    m_host->savedDatePoly,
                    m_host->savedTrackingBox,
                    m_host->m_loadedTrackingTemplate);

        applyErrors.clear();
        if (!m_host->applyRuntimeThreadSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!m_host->applyPlcTriggerModeFromUi(&applyErrors, false)
                || !m_host->applyPlcRunSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前 PLC 参数下发失败：\n") + applyErrors.join("\n"));
            return;
        }

        // 发送模板匹配相关参数
        emit m_host->jiancestring(m_host->ui->dateEdit->toPlainText().toStdString());

        if (isWordProfileMode) {
            qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                     << static_cast<int>(
                            profileSnapshotForRun.detectionProfiles.size())
                     << "mode:" << m_host->currentDetectModeId();
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
                    m_host->ui->comboBox_4->currentIndex(),
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
                 << "modeIndex=" << m_host->ui->comboBox_4->currentIndex();
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
        if (!m_host->applyCameraHardwareSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = m_host->ui->lineEdit_14->text().toFloat();

        m_host->m_acquisitionController->ensureWorkersReady();
        m_host->m_acquisitionController->configureSoftwareWorker(
                    runPlan,
                    profileSnapshotForRun.trackingProfiles,
                    m_host->savedDatePoly,
                    m_host->savedTrackingBox,
                    m_host->m_loadedTrackingTemplate);

        applyErrors.clear();
        if (!m_host->applyRuntimeThreadSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(m_host, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!m_host->applyPlcTriggerModeFromUi(&applyErrors, false)
                || !m_host->applyPlcRunSettingsFromUi(&applyErrors, false)) {
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
                        m_host->m_appliedGlobalSettings.cameraExposure,
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
                         << "mode:" << m_host->currentDetectModeId();
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
                        m_host->ui->comboBox_4->currentIndex(),
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
