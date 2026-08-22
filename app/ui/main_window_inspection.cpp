// 文件作用：本文件用于实现主窗口中相机、模板、启动停止和检测结果相关的交互槽。
// 主要职责：实现主窗口中相机、模板、启动停止和检测结果相关的交互槽。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
/**
 * @file ui/main_window_inspection.cpp
 * @brief 主窗口检测、运行状态与窗口生命周期薄协调。
 */

#include "ui/main_window.h"
#include "ui_main_window.h"
#include "contracts/detection_mode.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"


#include <QTimer>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QCloseEvent>
#include <QApplication>
#include <QDebug>

#include <limits>

void MainWindow::presentInspectionFault()
{
    if (!m_inspectionPage) {
        return;
    }
    updateOperationUiState();
    m_inspectionPage->presentFault(
                m_runtime->faultSnapshot(),
                &m_faultAlarmPresented);
}

bool MainWindow::confirmInspectionFaultRecovery()
{
    return m_inspectionPage
            && m_inspectionPage->confirmFaultRecovery(
                m_runtime->faultSnapshot());
}

void MainWindow::checkInspectionPlcHealth()
{
    m_inspectionApplicationService->checkPlcHealth();
}

void MainWindow::restoreNormalFaultUi()
{
    m_faultAlarmPresented = false;
    if (m_inspectionPage) {
        m_inspectionPage->clearTransientView();
        m_inspectionPage->restoreNormalFaultStyle();
    }
}

void MainWindow::presentStartFailure(
    const StartInspectionResult &result)
{
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
                    QStringLiteral("启动资源预检失败"),
                    result.error.diagnostic.isEmpty()
                    ? result.error.userMessage
                    : result.error.userMessage
                      + QStringLiteral("\n\n")
                      + result.error.diagnostic);
        break;
    case InspectionStartIssue::TemplatesMissing:
        QMessageBox::warning(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::TemplateResourcesInvalid:
        QMessageBox::warning(
                    this,
                    "二维码+三期模板预检失败",
                    QString("以下问题必须处理后才能启动检测：\n\n%1")
                    .arg(result.details.join("\n")));
        break;
    case InspectionStartIssue::TemplateIncomplete:
        QMessageBox::warning(
                    this,
                    "操作规范",
                    QString("当前模板无法启动检测。\n\n"
                            "具体原因：\n%1\n\n"
                            "请编辑该模板，或重新选择一个完整模板。")
                    .arg(result.details.join("\n")));
        break;
    case InspectionStartIssue::TemplatesIncomplete:
        QMessageBox::warning(
                    this,
                    "提示",
                    QString("当前选择中没有可用模板：\n%1")
                    .arg(result.details.join("\n")));
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
    const StopInspectionResult &result)
{
    if (result.issue
            == StopInspectionIssue::AcquisitionStillStopping) {
        ui->label_runtimeStatus->setText("停止中，请稍后再关闭相机");
        updateOperationUiState();
        return;
    }

    if (result.cameraRecovery.issue
            == CameraRecoveryIssueDto::ExposureRejected) {
        {
            QSignalBlocker blocker(ui->spinBox_cameraExposure);
            ui->spinBox_cameraExposure->setRange(
                        0, (std::numeric_limits<int>::max)());
            ui->spinBox_cameraExposure->setValue(
                        machineSettings().cameraExposure);
        }
        m_machineSettingsPage->refreshDirty("camera.exposure");
        QMessageBox::warning(
                    this,
                    "警告",
                    QString("停止识别后恢复相机曝光失败：\n%1")
                    .arg(result.cameraRecovery.errorMessage));
    } else if (result.cameraRecovery.isRecovered()
               && result.cameraRecovery.recoveryAttempted) {
        ui->label_runtimeStatus->setText("相机已打开");
        if (!result.cameraRecovery.adjustmentMessage.isEmpty()) {
            QMessageBox::information(
                        this,
                        "提示",
                        result.cameraRecovery.adjustmentMessage);
        }
    }

    if (m_templateEditorPage) {
        m_templateEditorPage->cancelTemplateDrawing();
    }
    m_inspectionPage->clearResultView();
    if (result.issue == StopInspectionIssue::RuntimeFault
            || result.issue
               == StopInspectionIssue::FaultReconciliationFailed) {
        presentInspectionFault();
        if (!result.error.diagnostic.isEmpty()) {
            QMessageBox::critical(
                        this,
                        QStringLiteral("故障产品收口失败"),
                        result.error.diagnostic);
        }
        return;
    }

    if (result.recoveredFault) {
        restoreNormalFaultUi();
    }
    if (!result.reconciliationSummary.isEmpty()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("故障产品收口结果"),
                    result.reconciliationSummary);
    }
    ui->label_runtimeStatus->setText("已停止");
    updateOperationUiState();
}

/**
 * @brief 显示图像槽函数
 * @param image OpenCV Mat图像指针
 * @details 将OpenCV图像转换为QPixmap并显示在UI上
 */
void MainWindow::slot_displayAndDetect(cv::Mat *image)
{
    DetectionMode activeMode = DetectionMode::Word;
    const bool tissueMode = detectionModeFromUiId(
                m_appliedMachineSettings.detectModeId,
                &activeMode)
            && activeMode == DetectionMode::Tissue;
    const bool productionRunning =
            isInspectionBusy();
    if (image) {
        const QImage preview = m_inspectionApplicationService->renderPreviewFrame(
                    *image, tissueMode, productionRunning);
        m_inspectionPage->presentPreviewImage(preview);
    }
}


OperationUiState MainWindow::operationUiState() const
{
    const RuntimeSnapshot snapshot =
            m_inspectionApplicationService->runtimeSnapshot();
    switch (snapshot.state) {
    case ApplicationRuntimeState::Starting:
    case ApplicationRuntimeState::Running:
        return OperationState::Detecting;
    case ApplicationRuntimeState::Stopping:
        return OperationState::Stopping;
    case ApplicationRuntimeState::Fault:
        return OperationState::Fault;
    case ApplicationRuntimeState::Idle:
    default:
        if (m_templateEditorPage
                && m_templateEditorPage->captureState()
                   == TemplateEditorPage::CaptureState::Previewing) {
            return OperationState::TemplatePreviewing;
        }
        if (m_templateEditorPage
                && m_templateEditorPage->captureState()
                   == TemplateEditorPage::CaptureState::Frozen) {
            return OperationState::TemplateFrozen;
        }
        return snapshot.cameraOpen
                ? OperationState::CameraReady
                : OperationState::CameraClosed;
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
    if (!m_inspectionPage) {
        return;
    }
    const RuntimeSnapshot runtime =
            m_inspectionApplicationService->runtimeSnapshot();
    OperationUiContext context;
    context.state = operationUiState();
    context.cameraOpen = runtime.cameraOpen;
    context.plcConnected = runtime.plcConnected;
    const OperationUiSnapshot snapshot =
            OperationUiPolicy::create(context);

    m_inspectionPage->applyOperationState(context.state, snapshot);
    if (m_machineSettingsPage) {
        m_machineSettingsPage->applyOperationState(snapshot);
    }
    if (m_templateEditorPage) {
        m_templateEditorPage->applyOperationState(snapshot);
    }

    applyOperationUiAccess(
                ui->toolButton_selectTemplate,
                snapshot.templateSelection);
    applyOperationUiAccess(
                ui->pushButton_browseImageSavePath,
                snapshot.generalSettings);
    applyOperationUiAccess(
                ui->pushButton_applyImageRotation,
                snapshot.generalSettings);
    applyOperationUiAccess(
                ui->pushButton_applyColorChannel,
                snapshot.generalSettings);
    applyOperationUiAccess(
                ui->pushButton_clearSoftwareData,
                snapshot.generalSettings);
    applyOperationUiAccess(
                ui->pushButton_restoreDefaultSettings,
                snapshot.generalSettings);
    applyOperationUiAccess(
                ui->pushButton_resetTotalCount,
                snapshot.statisticsReset);
    applyOperationUiAccess(
                ui->pushButton_resetNgCount,
                snapshot.statisticsReset);
    applyOperationUiAccess(
                ui->pushButton_resetRejectQueue,
                snapshot.rejectQueueReset);
}

void MainWindow::resetTemplateCaptureState()
{
    if (m_templateEditorPage) {
        m_templateEditorPage->resetTemplateCaptureState();
    }
}

/**
 * @brief 制作模板按钮点击槽函数
 */
void MainWindow::on_toolButton_createTemplate_clicked()
{
    if (m_templateEditorPage) {
        m_templateEditorPage->handleTemplateCaptureButton();
    }
}

void MainWindow::showParameterInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::showParameterInfoWithRedWarning(const QString &title,
                                             const QString &message,
                                             const QString &warningMessage)
{
    QString infoHtml = message.toHtmlEscaped();
    infoHtml.replace("\r\n", "\n");
    infoHtml.replace('\r', '\n');
    infoHtml.replace("\n", "<br>");

    QString warningHtml = warningMessage.toHtmlEscaped();
    warningHtml.replace("\r\n", "\n");
    warningHtml.replace('\r', '\n');
    warningHtml.replace("\n", "<br>");

    QMessageBox messageBox(QMessageBox::Warning,
                           title,
                           QString(),
                           QMessageBox::Ok,
                           this);
    messageBox.setTextFormat(Qt::RichText);
    messageBox.setText(
                QString("<div>%1</div><p><strong>%2</strong></p>")
                .arg(infoHtml)
                .arg(warningHtml));
    messageBox.exec();
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

/**
 * @brief 窗口关闭事件
 * @param event 关闭事件对象
 * @details 关闭时停止运行服务并保存设置
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_applicationExitInProgress) {
        event->accept();
        return;
    }

    m_applicationExitInProgress = true;
    if (m_templateCaptureAttentionTimer) {
        m_templateCaptureAttentionTimer->stop();
    }

    resetTemplateCaptureState();

    m_inspectionApplicationService->shutdown();

    saveSettings(false);
    event->accept();
}

/**
 * @brief 选择保存文件夹按钮点击槽函数
 */
void MainWindow::on_pushButton_browseImageSavePath_clicked()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
                this,
                "选择图像保存路径",
                ui->lineEdit_imageSavePath->text().trimmed().isEmpty()
                ? QString("C:/")
                : ui->lineEdit_imageSavePath->text().trimmed(),
                QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) {
        return;
    }

    ui->lineEdit_imageSavePath->setText(dirPath);
    updateSaveDirButtonText();
    qDebug() << "save file path:" << dirPath;
}



/**
 * @brief 清空总数统计按钮点击槽函数
 */
void MainWindow::on_pushButton_resetTotalCount_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->resetStatistics();
    if (!result.isSuccess()) {
        showParameterWarning(QStringLiteral("提示"), result.error.userMessage);
    } else if (m_runtime) {
        m_inspectionPage->setStatistics(m_runtime->statistics());
    }
}

/**
 * @brief 清空NG数统计按钮点击槽函数
 */
void MainWindow::on_pushButton_resetNgCount_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->resetNgCount();
    if (!result.isSuccess()) {
        showParameterWarning(QStringLiteral("提示"), result.error.userMessage);
    } else if (m_runtime) {
        m_inspectionPage->setStatistics(m_runtime->statistics());
    }
}

/**
 * @brief 旋转角度确定按钮点击槽函数
 * @details 设置图像旋转角度（0°、90°、180°、270°）
 */
void MainWindow::on_pushButton_applyImageRotation_clicked()
{
    m_machineSettingsPage->updateAppliedFromUi("image.rotation");
    m_machineSettingsPage->refreshDirty("image.rotation");
    saveSettings(false);
    showParameterInfo("提示", "旋转角度设置成功");
}



//关闭相机按钮
void MainWindow::on_toolButton_closeCamera_clicked()
{
    const OperationResult closeResult =
            m_inspectionApplicationService->closeCamera();
    if (!closeResult.isSuccess()) {
        QMessageBox::warning(
                    this,
                    "警告",
                    closeResult.error.userMessage);
        return;
    }
    // 清空文本并将文本置0
    ui->label_verdictResult->clear();
    m_templateEditorPage->cancelTemplateDrawing();
    imageLabel->clear();
    ui->imageLabel_inspection->clear();
    ui->lineEdit_totalCount->clear();
    ui->lineEdit_ngCount->clear();
    //    ui->ocrResult->clear();
    ui->label_recognitionText->clear();
    ui->lineEdit_detectionDuration->clear();
    resetTemplateCaptureState();
    if (m_inspectionPage) {
        m_inspectionPage->clearDetectionRoiWarning(QString());
    }
    ui->label_runtimeStatus->setText("相机已关闭");
    updateOperationUiState();
}

void MainWindow::on_toolButton_startInspection_clicked()
{
    updateOperationUiState();
    m_machineSettingsPage->refreshAllDirty();
    m_templateEditorPage->refreshTemplateDirty();
    m_templateEditorPage->updateCurrentTemplateName();

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
            return;
        }
        m_settingsApplicationService->discardDraft();
        restoreUnappliedSettingsFromApplied();
        command.unappliedChanges.clear();
        result = m_inspectionApplicationService->start(command);
    }

    if (!result.isAccepted()) {
        presentStartFailure(result);
        updateOperationUiState();
        return;
    }
    if (!result.details.isEmpty()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("部分模板已跳过"),
                    result.details.join(QStringLiteral("\n")));
    }
    m_templateEditorPage->cancelTemplateDrawing();
    if (m_inspectionPage) {
        m_inspectionPage->clearDetectionRoiWarning(QString());
    }
    if (result.acquisitionKind
            == InspectionAcquisitionDto::HardwareTrigger) {
        ui->imageLabel_inspection->clear();
        ui->lineEdit_totalCount->clear();
        ui->lineEdit_ngCount->clear();
        ui->label_recognitionText->clear();
        ui->lineEdit_detectionDuration->clear();
    }
    ui->label_runtimeStatus->setText(
                result.acquisitionKind
                == InspectionAcquisitionDto::HardwareTrigger
                ? "触发模式运行中"
                : "软触发模式运行中");
    updateOperationUiState();
}
// 检测相机
void MainWindow::on_toolButton_openCamera_clicked()
{
    PlcConnectionCommand plcCommand;
    plcCommand.address = ui->lineEdit_plcIpAddress->text();
    plcCommand.rack = ui->lineEdit_plcRack->text().toInt();
    plcCommand.slot = ui->lineEdit_plcSlot->text().toInt();
    const OpenCameraResult result =
            m_inspectionApplicationService->openCamera(plcCommand);

    if (!result.operation.isSuccess()
            && result.camera.isSuccess()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("提示"),
                    result.operation.error.userMessage);
        updateOperationUiState();
        return;
    }

    if (result.plcConnectionFailed)
    {
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
    else{
    m_machineSettingsPage->updateAppliedFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    m_machineSettingsPage->refreshDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    saveSettings(false);
    qDebug()<<"opencamera，plc connect success";
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
            QSignalBlocker blocker(ui->spinBox_cameraExposure);
            ui->spinBox_cameraExposure->setRange(
                0, (std::numeric_limits<int>::max)());
            ui->spinBox_cameraExposure->setValue(
                m_appliedMachineSettings.cameraExposure);
        }
        m_machineSettingsPage->refreshDirty("camera.exposure");
        updateOperationUiState();
        QMessageBox::warning(
            this,
            "警告",
            "相机异常！");
        return;
    }

    m_appliedMachineSettings =
            m_settingsApplicationService->current();
    {
        QSignalBlocker blocker(ui->spinBox_cameraExposure);
        ui->spinBox_cameraExposure->setRange(
            openResult.exposureMinimum,
            openResult.exposureMaximum);
        ui->spinBox_cameraExposure->setValue(openResult.appliedExposure);
    }
    m_machineSettingsPage->refreshDirty("camera.exposure");

    ui->label_runtimeStatus->setText("相机已打开");
    updateOperationUiState();
    const QString openMessage = openResult.adjustmentMessage.isEmpty()
            ? QString("相机打开成功！")
            : QString("相机打开成功！\n\n%1")
              .arg(openResult.adjustmentMessage);
    QMessageBox::information(this, "提示", openMessage);
}

// PLC模式选择
void MainWindow::on_pushButton_applyPlcTriggerMode_clicked()
{
    QStringList errors;
    applyPlcTriggerModeFromUi(&errors, true);
}

//剔除队列复位 清空还未发出的剔除信号
void MainWindow::on_pushButton_resetRejectQueue_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->clearPendingDelayedNgRequests();
    if (!result.isSuccess()) {
        QMessageBox::warning(
                    this, QStringLiteral("提示"),
                    result.error.userMessage);
        return;
    }
    QMessageBox::information(this, "提示", "剔除队列已清空！");
}

//加载UI样式表模板
void MainWindow::initStyle()
{
    QFile file(QStringLiteral(":/qss/app_theme.qss"));
    if (!file.open(QFile::ReadOnly)) {
        qWarning().noquote()
                << QStringLiteral("[UI_STYLE] 无法读取正式样式资源：:/qss/app_theme.qss");
        return;
    }

    const QString qss = QString::fromUtf8(file.readAll());
    qApp->setStyleSheet(qss);
}



//设置颜色通道
