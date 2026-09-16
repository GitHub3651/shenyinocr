#include "ui/main_window/main_window.h"
#include "ui_detection_settings_page.h"
#include "ui_image_settings_page.h"
#include "ui_inspection_info_page.h"
#include "ui_main_window.h"
#include "ui_plc_settings_page.h"
#include "ui_software_settings_page.h"
#include "contracts/detection_mode.h"
#include "ui/main_window/inspection/inspection_page.h"
#include "ui/main_window/settings/machine_settings_page.h"
#include "ui/main_window/template/template_editor_page.h"
#include "system_support/logging/log_categories.h"


#include <QTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QCloseEvent>

#include <limits>

void MainWindow::checkInspectionPlcHealth()
{
    m_inspectionApplicationService->checkPlcHealth();
}

void MainWindow::presentStartFailure(
    const StartInspectionResult &result)
{
    QString summary = QStringLiteral(
                "event=run.start_failed code=%1 reason=%2")
            .arg(result.error.code, result.error.userMessage);
    if (!result.error.diagnostic.trimmed().isEmpty()) {
        summary += QStringLiteral(" diagnostic=%1")
                .arg(result.error.diagnostic.trimmed());
    }
    if (!result.details.isEmpty()) {
        summary += QStringLiteral(" details=%1")
                .arg(result.details.join(QStringLiteral(";")));
    }
    qCWarning(logRuntime).noquote()
            << summary;
    if (result.error.code == QStringLiteral(
                "INSPECTION_START_EXECUTION_FAILED")
            || result.error.code == QStringLiteral(
                "INSPECTION_START_COMMIT_FAILED")) {
        QMessageBox::warning(
                    this, QStringLiteral("启动失败"),
                    result.error.userMessage);
        return;
    }
    switch (result.issue) {
    case InspectionStartIssue::TemplateOperationActive:
        QMessageBox::warning(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::RuntimeBusy:
        QMessageBox::information(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::CameraClosed:
        QMessageBox::warning(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::PlcDisconnected:
        showParameterWarning("提示", result.error.userMessage);
        break;
    case InspectionStartIssue::TemplateMissing:
        QMessageBox::warning(
                    this,
                    QStringLiteral("无法开始检测"),
                    result.error.userMessage);
        break;
    case InspectionStartIssue::TemplatesMissing:
        QMessageBox::warning(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::TemplateResourcesInvalid:
        QMessageBox::warning(
                    this,
                    QStringLiteral("无法开始检测"),
                    result.error.userMessage);
        break;
    case InspectionStartIssue::TemplateIncomplete:
        QMessageBox::warning(
                    this,
                    QStringLiteral("无法开始检测"),
                    result.error.userMessage);
        break;
    case InspectionStartIssue::TemplatesIncomplete:
        QMessageBox::warning(
                    this,
                    QStringLiteral("无法开始检测"),
                    result.error.userMessage);
        break;
    case InspectionStartIssue::DirtySettingsConfirmationRequired:
    case InspectionStartIssue::None:
    default:
        if (!result.error.userMessage.isEmpty()) {
            QMessageBox::warning(
                        this, "启动失败", result.error.userMessage);
        }
        break;
    }
}

void MainWindow::finishInspectionStopUi(
    const CameraRecoveryResultDto &cameraRecovery)
{
    if (cameraRecovery.issue
            == CameraRecoveryIssueDto::ExposureRejected) {
        qCWarning(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.exposure_recovery_failed diagnostic=%1")
                   .arg(cameraRecovery.errorMessage);
        {
            QSignalBlocker blocker(m_imageSettingsUi->spinBox_cameraExposure);
            m_imageSettingsUi->spinBox_cameraExposure->setRange(
                        0, (std::numeric_limits<int>::max)());
            m_imageSettingsUi->spinBox_cameraExposure->setValue(
                        machineSettings().cameraExposure);
        }
        m_machineSettingsPage->refreshDirty("camera.exposure");
    } else if (cameraRecovery.isRecovered()
               && cameraRecovery.recoveryAttempted) {
        m_inspectionInfoUi->label_runtimeStatus->setText("相机已打开");
        if (!cameraRecovery.adjustmentMessage.isEmpty()) {
            QMessageBox::information(
                        this,
                        "提示",
                        cameraRecovery.adjustmentMessage);
        }
    }

    m_templateEditorPage->cancelTemplateDrawing();
    m_inspectionPage->setStatistics(m_runtime->statistics());
    m_inspectionInfoUi->label_runtimeStatus->setText("已停止");
    updateOperationUiState();
}

void MainWindow::presentTemplatePreviewFrame(const cv::Mat &image)
{
    DetectionMode activeMode = DetectionMode::Word;
    const bool tissueMode = detectionModeFromUiId(
                machineSettings().detectModeId,
                &activeMode)
            && activeMode == DetectionMode::Tissue;
    const QImage preview =
            m_inspectionApplicationService->renderPreviewFrame(
                image, tissueMode, isInspectionBusy());
    m_inspectionPage->presentPreviewImage(preview);
}


OperationUiState MainWindow::operationUiState() const
{
    const RuntimeSnapshot snapshot =
            m_inspectionApplicationService->runtimeSnapshot();
    switch (snapshot.state) {
    case ApplicationRuntimeState::Starting:
    case ApplicationRuntimeState::Running:
        return OperationUiState::Detecting;
    case ApplicationRuntimeState::Stopping:
        return OperationUiState::Stopping;
    case ApplicationRuntimeState::Idle:
    default:
        if (m_templateEditorPage->captureState()
                   == TemplateEditorPage::CaptureState::Previewing) {
            return OperationUiState::TemplatePreviewing;
        }
        if (m_templateEditorPage->captureState()
                   == TemplateEditorPage::CaptureState::Frozen) {
            return OperationUiState::TemplateFrozen;
        }
        return snapshot.cameraOpen
                ? OperationUiState::CameraReady
                : OperationUiState::CameraClosed;
    }
}

bool MainWindow::isCameraOpen() const
{
    return m_inspectionApplicationService
            ->runtimeSnapshot().cameraOpen;
}

bool MainWindow::isInspectionBusy() const
{
    return m_inspectionApplicationService
            ->runtimeSnapshot().isInspectionBusy();
}

const AppSettings &MainWindow::machineSettings() const
{
    return m_settingsApplicationService->current();
}

void MainWindow::updateOperationUiState()
{
    const RuntimeSnapshot runtime =
            m_inspectionApplicationService->runtimeSnapshot();
    updateBarcodeCsvUi(runtime);
    OperationUiContext context;
    context.state = operationUiState();
    context.cameraOpen = runtime.cameraOpen;
    context.plcConnected = runtime.plcConnected;
    const OperationUiSnapshot snapshot =
            OperationUiPolicy::create(context);

    m_inspectionPage->applyOperationState(context.state, snapshot);
    m_machineSettingsPage->applyOperationState(snapshot);
    m_templateEditorPage->applyOperationState(context.state, snapshot);

    applyOperationUiAccess(
                ui->toolButton_selectTemplate,
                snapshot.templateSelection);
    applyOperationUiAccess(
                m_imageSettingsUi->pushButton_browseImageSavePath,
                snapshot.imageSettings);
    applyOperationUiAccess(
                m_imageSettingsUi->pushButton_applyImageRotation,
                snapshot.imageSettings);
    applyOperationUiAccess(
                m_imageSettingsUi->pushButton_applyColorChannel,
                snapshot.imageSettings);
    applyOperationUiAccess(
                m_softwareSettingsUi->pushButton_clearSoftwareData,
                snapshot.generalSettings);
    applyOperationUiAccess(
                m_inspectionInfoUi->pushButton_resetTotalCount,
                snapshot.statisticsReset);
    applyOperationUiAccess(
                m_inspectionInfoUi->pushButton_resetNgCount,
                snapshot.statisticsReset);
    applyOperationUiAccess(
                m_inspectionInfoUi->pushButton_resetRejectQueue,
                snapshot.rejectQueueReset);
}

void MainWindow::showParameterInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::showParameterInfoAsError(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::showParameterWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void MainWindow::showParameterCritical(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_applicationExitInProgress) {
        event->accept();
        return;
    }

    AppSettings settings = m_settingsApplicationService->current();
    settings.leftDrawerSplitterState =
            ui->splitter_leftDrawerMain->saveState();
    m_settingsApplicationService->saveConfiguration(settings);

    m_applicationExitInProgress = true;
    m_templateEditorPage->resetTemplateCaptureState();

    m_inspectionApplicationService->shutdown();

    event->accept();
}

void MainWindow::on_pushButton_browseImageSavePath_clicked()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
                this,
                "选择图像保存路径",
                m_imageSettingsUi->lineEdit_imageSavePath->text().trimmed().isEmpty()
                ? QString("C:/")
                : m_imageSettingsUi->lineEdit_imageSavePath->text().trimmed(),
                QFileDialog::ShowDirsOnly
                | QFileDialog::DontUseNativeDialog);
    if (dirPath.isEmpty()) {
        return;
    }

    m_imageSettingsUi->lineEdit_imageSavePath->setText(dirPath);
    qCInfo(logUi).noquote()
            << QStringLiteral("event=image_save.directory_selected path=%1")
               .arg(QDir::toNativeSeparators(dirPath));
}



void MainWindow::on_pushButton_resetTotalCount_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->resetStatistics();
    if (!result.isSuccess()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=statistics.reset_failed context=user code=%1 reason=%2")
                   .arg(result.error.code, result.error.userMessage);
        showParameterWarning(QStringLiteral("提示"), result.error.userMessage);
    } else if (m_runtime) {
        m_inspectionPage->setStatistics(m_runtime->statistics());
        qCInfo(logRuntime).noquote() << "event=statistics.reset context=user";
    }
}

void MainWindow::on_pushButton_resetNgCount_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->resetNgCount();
    if (!result.isSuccess()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=statistics.ng_reset_failed code=%1 reason=%2")
                   .arg(result.error.code, result.error.userMessage);
        showParameterWarning(QStringLiteral("提示"), result.error.userMessage);
    } else if (m_runtime) {
        m_inspectionPage->setStatistics(m_runtime->statistics());
        qCInfo(logRuntime).noquote() << "event=statistics.ng_reset";
    }
}

void MainWindow::on_pushButton_applyImageRotation_clicked()
{
    const QStringList keys = QStringList() << "image.rotation";
    AppSettings candidate = m_settingsApplicationService->current();
    m_machineSettingsPage->copyUiValuesTo(candidate, keys);
    const OperationResult saved =
            m_settingsApplicationService->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        qCCritical(logUi).noquote()
                << QStringLiteral(
                    "event=settings.save_failed key=image.rotation value=%1 code=%2 reason=%3")
                   .arg(candidate.imageRotationId)
                   .arg(saved.error.code, saved.error.userMessage);
        m_machineSettingsPage->restoreAppliedValues(keys);
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("图像旋转保存失败：\n%1")
                    .arg(saved.error.userMessage));
        return;
    }
    m_machineSettingsPage->refreshDirty(keys);
    qCInfo(logUi).noquote()
            << QStringLiteral(
                "event=settings.saved key=image.rotation value=%1")
               .arg(candidate.imageRotationId);
    showParameterInfo("提示", "旋转角度设置成功");
}
void MainWindow::on_toolButton_closeCamera_clicked()
{
    const OperationResult closeResult =
            m_inspectionApplicationService->closeCamera();
    if (!closeResult.isSuccess()) {
        qCWarning(logUi).noquote()
                << QStringLiteral(
                    "event=camera.close_rejected code=%1 reason=%2")
                   .arg(closeResult.error.code,
                        closeResult.error.userMessage);
        QMessageBox::warning(
                    this,
                    "警告",
                    closeResult.error.userMessage);
        return;
    }
    m_templateEditorPage->cancelTemplateDrawing();
    ui->inspectionImageCanvas->clear();
    m_inspectionPage->clearInspectionView(
                InspectionClearScope::AllDetectionData);
    m_templateEditorPage->resetTemplateCaptureState();
    m_inspectionInfoUi->label_runtimeStatus->setText("相机已关闭");
    updateOperationUiState();
}

void MainWindow::on_toolButton_startInspection_clicked()
{
    updateOperationUiState();
    m_machineSettingsPage->refreshAllDirty();
    m_templateEditorPage->refreshTemplateDirty();

    StartInspectionCommand command;
    command.unappliedChanges = m_settingsEditState.dirtyNames();
    StartInspectionResult result =
            m_inspectionApplicationService->start(command);
    if (result.issue
            == InspectionStartIssue::DirtySettingsConfirmationRequired) {
        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("提示");
        confirmBox.setText(dirtySettingsMessage());
        QPushButton *continueButton =
                confirmBox.addButton("继续运行", QMessageBox::AcceptRole);
        QPushButton *cancelButton =
                confirmBox.addButton("取消", QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();
        if (confirmBox.clickedButton() != continueButton) {
            qCInfo(logRuntime).noquote()
                    << "event=run.start_cancelled reason=unapplied_settings";
            return;
        }
        restoreUnappliedSettingsFromApplied();
        command.unappliedChanges.clear();
        result = m_inspectionApplicationService->start(command);
    }

    if (!result.isAccepted()) {
        presentStartFailure(result);
        updateOperationUiState();
        return;
    }
    m_templateEditorPage->cancelTemplateDrawing();
    ui->inspectionImageCanvas->clear();
    m_inspectionPage->clearInspectionView(
                InspectionClearScope::ImageMetadata);
    m_inspectionPage->setStatistics(m_runtime->statistics());
    if (!result.details.isEmpty()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("部分模板已跳过"),
                    result.details.join(QStringLiteral("\n")));
    }
    m_inspectionInfoUi->label_runtimeStatus->setText(
                result.acquisitionKind
                == InspectionAcquisitionDto::HardwareTrigger
                ? "硬触发模式运行中"
                : "软触发模式运行中");
    updateOperationUiState();
}
void MainWindow::on_toolButton_openCamera_clicked()
{
    PlcConnectionCommand plcCommand;
    plcCommand.address = m_plcSettingsUi->lineEdit_plcIpAddress->text();
    plcCommand.rack = m_plcSettingsUi->lineEdit_plcRack->text().toInt();
    plcCommand.slot = m_plcSettingsUi->lineEdit_plcSlot->text().toInt();
    const OpenCameraResult result =
            m_inspectionApplicationService->openCamera(plcCommand);

    if (!result.operation.isSuccess()
            && result.camera.isSuccess()) {
        qCWarning(logUi).noquote()
                << QStringLiteral(
                    "event=camera.open_rejected code=%1 reason=%2")
                   .arg(result.operation.error.code,
                        result.operation.error.userMessage);
        QMessageBox::warning(
                    this,
                    QStringLiteral("提示"),
                    result.operation.error.userMessage);
        updateOperationUiState();
        return;
    }

    if (result.plcConnectionFailed)
    {
        m_machineSettingsPage->restoreAppliedValues(
                    QStringList()
                    << "plc.ip" << "plc.rack" << "plc.slot");
        QMessageBox::critical(this, "错误", "PLC连接失败");
    }
    else{
    saveAppliedHardwareSettings(
                QStringList()
                << "plc.ip" << "plc.rack" << "plc.slot");
    }
    updateOperationUiState();

    const CameraOpenResultDto &openResult = result.camera;
    if (!openResult.isSuccess()) {
        if (openResult.issue
                == CameraOpenIssueDto::DeviceNotFound) {
            QMessageBox::warning(this, "警告", "相机未连接！");
            return;
        }
        if (openResult.issue
                == CameraOpenIssueDto::ExposureFailed
                || openResult.issue
                   == CameraOpenIssueDto::InitializationFailed) {
            QSignalBlocker blocker(m_imageSettingsUi->spinBox_cameraExposure);
            m_imageSettingsUi->spinBox_cameraExposure->setRange(
                0, (std::numeric_limits<int>::max)());
            m_imageSettingsUi->spinBox_cameraExposure->setValue(
                machineSettings().cameraExposure);
        }
        m_machineSettingsPage->refreshDirty("camera.exposure");
        updateOperationUiState();
        QMessageBox::warning(
            this,
            "警告",
            "相机打开失败，请检查相机连接和参数。");
        return;
    }

    {
        QSignalBlocker blocker(m_imageSettingsUi->spinBox_cameraExposure);
        m_imageSettingsUi->spinBox_cameraExposure->setRange(
            openResult.exposureMinimum,
            openResult.exposureMaximum);
        m_imageSettingsUi->spinBox_cameraExposure->setValue(openResult.appliedExposure);
    }
    m_machineSettingsPage->refreshDirty("camera.exposure");

    m_inspectionInfoUi->label_runtimeStatus->setText("相机已打开");
    updateOperationUiState();
    const QString openMessage = openResult.adjustmentMessage.isEmpty()
            ? QString("相机打开成功！")
            : QString("相机打开成功！\n\n%1")
               .arg(openResult.adjustmentMessage);
    QMessageBox::information(this, "提示", openMessage);
}

void MainWindow::on_pushButton_applyPlcTriggerMode_clicked()
{
    QStringList errors;
    applyPlcTriggerModeFromUi(&errors, true);
}

void MainWindow::on_pushButton_resetRejectQueue_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->clearPendingDelayedNgRequests();
    if (!result.isSuccess()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=plc.reject_queue_reset_failed code=%1 reason=%2")
                   .arg(result.error.code, result.error.userMessage);
        QMessageBox::warning(
                    this, QStringLiteral("提示"),
                    result.error.userMessage);
        return;
    }
    qCInfo(logRuntime).noquote() << "event=plc.reject_queue_reset";
    QMessageBox::information(
                this, "提示", "待执行的剔除动作已清除。");
}
