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
#include <QToolTip>
#include <QCursor>
#include <QTextEdit>
#include <QEvent>
#include <QCloseEvent>
#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QDebug>

#include <limits>

#include <opencv2/highgui.hpp>

#pragma execution_character_set("utf-8")

void MainWindow::presentInspectionFault()
{
    if (!m_inspectionPage) {
        return;
    }
    m_resultBoundDisplayActive.store(true);
    updateOperationUiState();
    m_inspectionPage->presentFault(
                m_inspectionApplicationService->faultSnapshot(),
                &m_faultAlarmPresented);
}

bool MainWindow::confirmInspectionFaultRecovery()
{
    return m_inspectionPage
            && m_inspectionPage->confirmFaultRecovery(
                m_inspectionApplicationService->faultSnapshot());
}

void MainWindow::checkInspectionPlcHealth()
{
    m_inspectionApplicationService->checkPlcHealth();
}

void MainWindow::restoreNormalFaultUi()
{
    m_faultAlarmPresented = false;
    m_inspectionApplicationService->clearTransientView();
    if (m_inspectionPage) {
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
    case InspectionStartIssue::PreparedRecipeMissing:
        QMessageBox::warning(
                    this,
                    QStringLiteral("启动资源预检失败"),
                    result.error.diagnostic.isEmpty()
                    ? result.error.userMessage
                    : result.error.userMessage
                      + QStringLiteral("\n\n")
                      + result.error.diagnostic);
        break;
    case InspectionStartIssue::WordProfilesMissing:
        QMessageBox::warning(
                    this, "提示", result.error.userMessage);
        break;
    case InspectionStartIssue::BarcodeResourcesInvalid:
        QMessageBox::warning(
                    this,
                    "二维码+三期模板预检失败",
                    QString("以下问题必须处理后才能启动检测：\n\n%1")
                    .arg(result.details.join("\n")));
        break;
    case InspectionStartIssue::ProductTemplateIncomplete:
        QMessageBox::warning(
                    this,
                    "操作规范",
                    QString("缺少可用产品模板，无法启动检测。\n\n"
                            "具体原因：\n%1\n\n"
                            "请重新创建配方，或从【已发布配方】加载完整的新格式配方。")
                    .arg(result.details.join("\n")));
        break;
    case InspectionStartIssue::WordProfilesIncomplete:
        QMessageBox::warning(
                    this,
                    "提示",
                    QString("以下产品模板还没有确认目标字符，不能启动检测：\n%1")
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

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    hideTemplateGuide();
    m_inspectionApplicationService->clearResultView();
    m_resultBoundDisplayActive.store(false);
    m_barcodeWordRunActive = false;
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
        m_inspectionApplicationService->presentPreviewFrame(
                    *image, tissueMode, productionRunning);
    }
}


void MainWindow::clearBarcodeTemplateValidation()
{
    m_templateEditorPage->clearBarcodeTemplateValidation();
}

OperationUiState MainWindow::operationUiState() const
{
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

const MachineSettings &MainWindow::machineSettings() const
{
    return m_settingsApplicationService->current();
}

void MainWindow::updateOperationUiState()
{
    if (m_inspectionPage) {
        m_inspectionPage->updateOperationState(
                    operationUiState(),
                    operationUiState() == OperationState::Fault);
    }
}

bool MainWindow::stopTemplatePreview(int waitTimeMs)
{
    Q_UNUSED(waitTimeMs)
    return m_templateEditorPage
            && m_templateEditorPage->stopTemplatePreview();
}

void MainWindow::resetTemplateCaptureState()
{
    if (m_templateEditorPage) {
        m_templateEditorPage->resetTemplateCaptureState();
    }
}

bool MainWindow::startTemplatePreview()
{
    return m_templateEditorPage
            && m_templateEditorPage->startTemplatePreview();
}

bool MainWindow::freezeTemplatePreview()
{
    return m_templateEditorPage
            && m_templateEditorPage->freezeTemplatePreview();
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
                QString("<div>%1</div>"
                        "<div style=\"margin-top:12px;color:#c00000;"
                        "font-weight:700;\">%2</div>")
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
 * @details 关闭时保存设置，销毁所有OpenCV窗口
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_applicationExitInProgress) {
        event->accept();
        return;
    }

    m_applicationExitInProgress = true;
    m_barcodeWordRunActive = false;
    if (m_templateCaptureAttentionTimer) {
        m_templateCaptureAttentionTimer->stop();
    }

    resetTemplateCaptureState();

    m_inspectionApplicationService->shutdown();

    try {
        cv::destroyAllWindows();
    } catch (...) {
    }
    saveSettings(false);
    event->accept();
}

/**
 * @brief 阈值确定按钮点击槽函数
 * @details 设置相似度判断阈值
 */
void MainWindow::on_pushButton_applyImageThreshold_clicked()
{
    m_templateEditorPage->applyCurrentImageThreshold();
}

/**
 * @brief 保存当前图像按钮点击槽函数
 * @details 打开文件保存对话框，保存当前显示的图像
 */
void MainWindow::on_pushButton_saveTemplate_clicked()
{
    m_templateEditorPage->saveCurrentTemplate();
}

// 先定义一个保存参数到指定文件夹的函数（可放在MainWindow类中）
void MainWindow::on_toolButton_selectRecipe_clicked()
{
    m_templateEditorPage->selectPublishedRecipeForCurrentMode();
}

/**
 * @brief 选择保存文件夹按钮点击槽函数
 */
void MainWindow::on_pushButton_browseImageSavePath_clicked()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
                this,
                "选择图像保存路径",
                selectedDir.isEmpty() ? QString("C:/") : selectedDir,
                QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) {
        return;
    }

    selectedDir = dirPath;
    updateSaveDirButtonText();
    qDebug() << "save file path:" << selectedDir;
}



/**
 * @brief 清空总数统计按钮点击槽函数
 */
void MainWindow::on_pushButton_resetTotalCount_clicked()
{
    m_inspectionApplicationService->resetStatistics();
}

/**
 * @brief 清空NG数统计按钮点击槽函数
 */
void MainWindow::on_pushButton_resetNgCount_clicked()
{
    m_inspectionApplicationService->resetNgCount();
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



bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_templateEditorPage->guideFrame()
            && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this]() {
            adjustTemplateGuideHeight();
        });
        return false;
    }

        if (watched == ui->pushButton_applyTargetText
            || watched == ui->pushButton_applyBatchTargetText
            || watched == ui->pushButton_applyBatchImageThreshold
            || watched == ui->checkBox_hardwareTriggerEnabled
            || watched == ui->pushButton_browseImageSavePath
            || watched == ui->pushButton_applyColorChannel
            || watched == ui->pushButton_resetRejectQueue
            || watched == ui->label_imageThreshold
            || watched == ui->label_imageRotation
            || watched == ui->label_cameraGain
            || watched == ui->label_photoTime
            || watched == ui->label_hardwareTriggerDelay
            || watched == ui->label_rejectDistance
            || watched == ui->label_rejectTime
            || watched == ui->label_rejectPosition
            || watched == ui->label_photoDistance
            || watched == ui->comboBox_plcTriggerMode
            || watched == m_templateEditorPage->manualCharacterCropButton()
            || watched == ui->toolButton_createTemplate) {
        QWidget *button = qobject_cast<QWidget *>(watched);
        if (!button) {
            return QWidget::eventFilter(watched, event);
        }

        if (event->type() == QEvent::Enter) {
            QTimer::singleShot(500, this, [this, button, watched]() {
                if (!button->underMouse()) {
                    return;
                }

                QString tooltipText;
                if (watched == ui->toolButton_createTemplate) {
                    switch (ui->comboBox_detectionMode->currentIndex()) {
                    case 0:
                        tooltipText =
                                "制作模板匹配产品模板：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 在冻结图像上框选定位区域和检测区域。\n"
                                "4. 点击【保存模板】保存产品模板。";
                        break;
                    case 1:
                        tooltipText =
                                "制作字库产品模板步骤：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 按住鼠标左键框选定位区域。\n"
                                "4. 用鼠标左键点击喷码区域边缘，右键闭合。\n"
                                "5. 点击【保存模板】保存产品模板。";
                        break;
                    case 2:
                        tooltipText =
                                "点击后进入实时取景，再次点击可冻结当前画面。\n\n"
                                "深度模型模式通常不需要制作传统产品模板。";
                        break;
                    case 3:
                        tooltipText =
                                "点击后进入实时取景，再次点击可冻结当前画面。\n\n"
                                "纸巾检测通常不需要制作产品模板。";
                        break;
                    case 4:
                        tooltipText =
                                "制作二维码+三期产品模板步骤：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 框选稳定且不会变化的定位锚点。\n"
                                "4. 框选二维码区域并等待扫描验证。\n"
                                "5. 用鼠标左键点击日期区域边缘，右键闭合。\n"
                                "6. 点击【保存模板】保存产品模板。";
                        break;
                    default:
                        tooltipText = "点击后进入实时取景，再次点击冻结当前画面。";
                        break;
                    }
                } else {
                    tooltipText = button->toolTip();
                }

                if (!tooltipText.isEmpty()) {
                    QToolTip::showText(QCursor::pos(), tooltipText, button);
                }
            });
            return false;
        }

        if (event->type() == QEvent::Leave) {
            QToolTip::hideText();
            return false;
        }

        if (event->type() == QEvent::ToolTip) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

//关闭相机按钮
void MainWindow::on_toolButton_closeCamera_clicked()
{
    if (m_templateEditorPage
            && m_templateEditorPage->templateOperationActive()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在制作模板，请先点击【退出模板制作】。");
        return;
    }

    const OperationResult closeResult =
            m_inspectionApplicationService->closeCamera();
    if (!closeResult.isSuccess()) {
        QMessageBox::warning(
                    this,
                    "警告",
                    "相机正在检测采图中！\n"
                    "请先点击【停止识别】完全停止检测后，再关闭相机。");
        return;
    }
    // 清空文本并将文本置0
    ui->label_verdictResult->clear();
    imageLabel->setTemplateDrawingEnabled(false);
    hideTemplateGuide();
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
    ui->label_runtimeStatus->setStyleSheet("QLabel{color:#e74c3c; font-weight:bold;}");
    updateOperationUiState();
}

void MainWindow::on_toolButton_startInspection_clicked()
{
    updateHardwareParameterUiEnabled();
    m_machineSettingsPage->refreshAllDirty();
    refreshRecipeProfileDirty();
    updateCurrentTemplateName();

    StartInspectionCommand command;
    command.templateOperationActive =
            m_templateEditorPage
            && m_templateEditorPage->templateOperationActive();
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
    DetectionMode activeMode = DetectionMode::Stamp;
    detectionModeFromUiId(
                m_appliedMachineSettings.detectModeId,
                &activeMode);
    m_barcodeWordRunActive =
            activeMode == DetectionMode::BarcodeWord;
    m_resultBoundDisplayActive.store(true);
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();
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
    if (isInspectionBusy()
            || (m_templateEditorPage
                && m_templateEditorPage->templateOperationActive())) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前有任务正在运行，不能重新打开相机。");
        return;
    }
    if (isCameraOpen())
    {
        QMessageBox::warning(this, "警告", "相机已连接！");
        return;
    }

    PlcConnectionCommand plcCommand;
    plcCommand.address = ui->lineEdit_plcIpAddress->text();
    plcCommand.rack = ui->lineEdit_plcRack->text().toInt();
    plcCommand.slot = ui->lineEdit_plcSlot->text().toInt();
    const OpenCameraResult result =
            m_inspectionApplicationService->openCamera(plcCommand);

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
    updateHardwareParameterUiEnabled();

    const CameraOpenResultDto &openResult = result.camera;
    if (!openResult.isSuccess()) {
        if (openResult.issue
                == CameraOpenIssueDto::DeviceNotFound) {
            QMessageBox::warning(this, "警告", "未找到相机设备！");
            return;
        }
        if (openResult.issue
                == CameraOpenIssueDto::DeviceOpenFailed) {
            QMessageBox::warning(
                this, "警告", "打开设备失败！");
            return;
        }
        {
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
            QString("打开相机后应用曝光参数失败：\n%1")
            .arg(openResult.diagnostic));
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
    ui->label_runtimeStatus->setStyleSheet("QLabel{color:#2ecc71; font-weight:bold;}");
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
    m_inspectionApplicationService->clearPendingDelayedNgRequests();
    QMessageBox::information(this, "提示", "剔除队列已清空！");
}

//加载UI样式表模板
void MainWindow::initStyle()
    {
        QFile file(":/qss/1.css");// 淡蓝色风格
        if(file.open(QFile::ReadOnly)){
            QString qss = QLatin1String(file.readAll());
            qss +=
                    "\nQGroupBox#groupBox_mainControls QToolButton:disabled {"
                    "background-color: #f2f3f5;"
                    "color: #a8abb2;"
                    "border-color: #dcdfe6;"
                    "}"
                    "QPushButton:disabled {"
                    "background-color: #f2f3f5;"
                    "color: #a8abb2;"
                    "border-color: #dcdfe6;"
                    "}"
                    "QComboBox:disabled,"
                    "QLineEdit:disabled,"
                    "QTextEdit:disabled,"
                    "QPlainTextEdit:disabled,"
                    "QSpinBox:disabled,"
                    "QDoubleSpinBox:disabled,"
                    "QDateEdit:disabled,"
                    "QTimeEdit:disabled {"
                    "color: #a8abb2;"
                    "}";

            // 提取主色调用于设置系统调色板
            QString paletteColor = qss.mid(20,7);// 获取QSS中定义的主色
            qApp->setPalette(QPalette(QColor(paletteColor)));

            // 应用样式表（qApp 是全局应用程序对象，作用于所有控件）
            qApp->setStyleSheet(qss);

            file.close();
        }
    }



//设置颜色通道
