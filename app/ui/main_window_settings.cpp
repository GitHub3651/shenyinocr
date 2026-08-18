/**
 * @file ui/main_window_settings.cpp
 * @brief 主窗口设置、配方页面命令与硬件按钮薄协调。
 */

#include "ui/main_window.h"
#include "ui_main_window.h"
#include "contracts/detection_mode.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"


#include <QString>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>
#include <QDebug>

#include <cstdint>

#pragma execution_character_set("utf-8")

namespace {
bool parseIntValue(const QString &text, int *value)
{
    bool ok = false;
    const int parsed = text.trimmed().toInt(&ok);
    if (!ok) {
        return false;
    }
    if (value) {
        *value = parsed;
    }
    return true;
}

bool isSingleTemplateRecipeMode(const QString &modeId)
{
    DetectionMode mode;
    return detectionModeFromUiId(modeId, &mode)
            && (mode == DetectionMode::Stamp
                || mode == DetectionMode::Ocr);
}

} // namespace

void MainWindow::updateCurrentTemplateName()
{
    m_templateEditorPage->updateCurrentTemplateName();
}

void MainWindow::updateSaveDirButtonText()
{
    if (!ui || !ui->lineEdit_imageSavePath || !ui->pushButton_browseImageSavePath) {
        return;
    }

    const QString saveDir = selectedDir.trimmed();
    ui->pushButton_browseImageSavePath->setText("浏览");
    ui->pushButton_browseImageSavePath->setToolTip("点击选择图像保存路径");

    if (saveDir.isEmpty()) {
        ui->lineEdit_imageSavePath->clear();
        ui->lineEdit_imageSavePath->setToolTip("");
        return;
    }

    ui->lineEdit_imageSavePath->setText(saveDir);
    ui->lineEdit_imageSavePath->setToolTip(saveDir);
}

void MainWindow::updateImageSaveOptionsVisibility()
{
    if (!ui || !ui->comboBox_imageSaveRange) {
        return;
    }

    const bool saveImages =
            ui->comboBox_imageSaveRange->currentIndex() != 0;
    if (ui->label_imageSaveContent) {
        ui->label_imageSaveContent->setVisible(saveImages);
    }
    if (ui->comboBox_imageSaveContent) {
        ui->comboBox_imageSaveContent->setVisible(saveImages);
    }
    if (ui->label_imageSavePath) {
        ui->label_imageSavePath->setVisible(saveImages);
    }
    if (ui->lineEdit_imageSavePath) {
        ui->lineEdit_imageSavePath->setVisible(saveImages);
    }
    if (ui->pushButton_browseImageSavePath) {
        ui->pushButton_browseImageSavePath->setVisible(saveImages);
    }
    if (ui->groupBox_imageSaving) {
        ui->groupBox_imageSaving->updateGeometry();
    }
}

void MainWindow::updateTissueRoughnessUiVisibility()
{
    if (!ui) {
        return;
    }

    const bool showTissueThreshold = (ui->comboBox_detectionMode->currentIndex() == 3);
    ui->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->pushButton_applyTissueRoughnessThreshold->setVisible(showTissueThreshold);
}

void MainWindow::setupTemplateGuide()
{
    m_templateEditorPage->setupTemplateGuide();
}

void MainWindow::adjustTemplateGuideHeight()
{
    m_templateEditorPage->adjustTemplateGuideHeight();
}

void MainWindow::hideTemplateGuide()
{
    m_templateEditorPage->hideTemplateGuide();
}

void MainWindow::updateImageDisplayStatusText(const QString &body)
{
    m_templateEditorPage->updateImageDisplayStatusText(body);
}

void MainWindow::handleTemplateGuideEvent(
        const QString &eventName,
        int pointCount)
{
    m_templateEditorPage->handleTemplateGuideEvent(
                eventName,
                pointCount);
}

void MainWindow::setupManualCharacterCropUi()
{
    m_templateEditorPage->setupManualCharacterCropUi();
}

void MainWindow::setupSoftwareSettingsPage()
{
    if (!ui || !ui->lineEdit_softwareDataDirectory || !ui->pushButton_clearSoftwareData
            || !ui->pushButton_restoreDefaultSettings) {
        return;
    }

    m_softwareDataDirLineEdit = ui->lineEdit_softwareDataDirectory;
    m_softwareDataDirLineEdit->setReadOnly(true);
    m_softwareDataDirLineEdit->setCursor(Qt::PointingHandCursor);
    m_softwareDataDirLineEdit->setText(
                m_settingsApplicationService
                ? m_settingsApplicationService->applicationDataRoot()
                : QString());
    m_softwareDataDirLineEdit->setToolTip("软件公共设置保存在此文件夹。双击可打开目录；产品模板、识别图片、授权和日志不在清空范围内。");
    if (m_machineSettingsPage) {
        m_machineSettingsPage->setSoftwareDataDirectoryEditor(
                    m_softwareDataDirLineEdit);
    }

    ui->pushButton_clearSoftwareData->setStyleSheet(
                "QPushButton {"
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #d93025;"
                "padding: 5px 10px;"
                "}"
                "QPushButton:hover { background-color: #fff2f0; }"
                "QPushButton:pressed { background-color: #fde2e0; }");
    ui->pushButton_clearSoftwareData->setToolTip(
                "只清除当前 Windows 用户的软件公共界面设置，不删除产品模板、识别图片、授权文件或日志。");
    connect(ui->pushButton_clearSoftwareData,
            &QPushButton::clicked,
            this,
            &MainWindow::clearCurrentSoftwareData);

    ui->pushButton_restoreDefaultSettings->setStyleSheet(
                "QPushButton {"
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #333333;"
                "padding: 5px 10px;"
                "}"
                "QPushButton:hover { background-color: #f2f6fc; }"
                "QPushButton:pressed { background-color: #ebeef5; }");
    ui->pushButton_restoreDefaultSettings->setToolTip(
                "将软件公共界面设置恢复为默认值，不删除产品模板、识别图片、授权文件或日志。");
    connect(ui->pushButton_restoreDefaultSettings,
            &QPushButton::clicked,
            this,
            &MainWindow::restoreDefaultMachineSettings);
}

void MainWindow::clearCurrentSoftwareData()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "清空当前软件数据",
                "将清空当前 Windows 用户保存的软件界面设置和路径记录，并恢复默认设置。\n\n"
                "产品模板、识别图片、授权文件和日志不会被删除。\n\n"
                "是否继续？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_machineSettingsPage
            || !m_machineSettingsPage->clear(&errorMessage)) {
        showParameterCritical("严重警告", QString("清空软件公共数据失败：\n%1").arg(errorMessage));
        return;
    }

    m_templateApplicationService->replacePublishedRecipeIdsByMode(
                m_appliedMachineSettings.publishedRecipeIdsByMode);
    clearWordMultiTemplateState();
    m_templateEditorPage->setCurrentTemplateNameVisible(false);
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    m_machineSettingsPage->clearAllDirty();
    clearRecipeProfileDirty();
    updateHardwareParameterUiEnabled();
    showParameterInfo("提示", "当前软件公共数据已清空，界面已恢复默认设置。");
}

void MainWindow::restoreDefaultMachineSettings()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "恢复默认设置",
                "将恢复软件公共设置为默认值。\n"
                "不会删除产品模板、识别图片、授权文件或日志。\n\n"
                "是否继续？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const bool cameraOpen = isCameraOpen();
    const bool plcConnected =
            m_inspectionApplicationService
            ->runtimeSnapshot().plcConnected;
    const MachineSettings editableDefaults =
            m_machineSettingsPage->defaultsForHardwareState(
                cameraOpen, plcConnected);

    applyMachineSettingsToUi(editableDefaults);
    clearWordMultiTemplateState();
    m_templateEditorPage->setCurrentTemplateNameVisible(false);
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    clearRecipeProfileDirty();
    updateHardwareParameterUiEnabled();
    m_machineSettingsPage->refreshAllDirty();

    if (!saveSettings(false)) {
        showParameterCritical("严重警告", "恢复默认设置失败：公共配置保存失败。");
        return;
    }

    showParameterInfo(
        "提示",
        "当前可设置参数已恢复为默认值。带 * 的参数需要点击对应【设置】后才会生效。");
}

bool MainWindow::hasDirtySettings() const
{
    return m_settingsEditState.hasDirtySettings();
}

bool MainWindow::saveSettings(bool showErrorMessage)
{
    QString errorMessage;
    if (m_machineSettingsPage
            && m_machineSettingsPage->save(
                showErrorMessage, &errorMessage)) {
        return true;
    }
    if (showErrorMessage) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("当前界面设置保存失败：\n%1")
                    .arg(errorMessage));
    } else {
        qDebug() << "[MACHINE_SETTINGS] silent save failed:"
                 << errorMessage;
    }
    return false;
}

void MainWindow::updateHardwareParameterUiEnabled()
{
    if (!m_machineSettingsPage) {
        return;
    }
    const bool cameraOpen = isCameraOpen();
    const bool operationBusy =
            isInspectionBusy()
            || (m_templateEditorPage
                && m_templateEditorPage->templateOperationActive());
    m_machineSettingsPage->updateHardwareEnabled(
                cameraOpen,
                m_inspectionApplicationService
                ->runtimeSnapshot().plcConnected,
                operationBusy);
}

QString MainWindow::dirtySettingsMessage() const
{
    return m_settingsEditState.dirtySettingsMessage();
}

void MainWindow::restoreUnappliedSettingsFromApplied()
{
    if (m_machineSettingsPage) {
        m_machineSettingsPage->restoreUnappliedMachineSettings();
    }

    const bool oldUpdating = m_updatingMachineSettingsUi;
    m_updatingMachineSettingsUi = true;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(
                m_templateEditorPage->wordTemplateProfiles().size())) {
        const RecipeProfile &settings =
                m_templateEditorPage->wordTemplateProfiles()[
                    static_cast<size_t>(profileIndex)].settings;
        QSignalBlocker targetTextBlocker(ui->textEdit_targetText);
        QSignalBlocker thresholdBlocker(ui->lineEdit_imageThreshold);
        ui->textEdit_targetText->setPlainText(settings.targetText);
        ui->lineEdit_imageThreshold->setText(QString::number(
            static_cast<int>(settings.imageThresholdPercent)));
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateEditorPage->activePreparedRecipe()
               && m_templateEditorPage->activePreparedRecipe()->recipe
               && m_templateEditorPage->activePreparedRecipe()
                  ->recipe->profiles.size() == 1) {
        const RecipeProfile settings =
                m_templateEditorPage->activePreparedRecipe()
                ->recipe->profiles.first();
        QSignalBlocker targetTextBlocker(ui->textEdit_targetText);
        QSignalBlocker thresholdBlocker(ui->lineEdit_imageThreshold);
        ui->textEdit_targetText->setPlainText(settings.targetText);
        ui->lineEdit_imageThreshold->setText(QString::number(
            static_cast<int>(settings.imageThresholdPercent)));
    }
    m_updatingMachineSettingsUi = oldUpdating;
    refreshRecipeProfileDirty();
}

void MainWindow::setupRecipeProfileDirtyTracking()
{
    m_templateEditorPage->setupRecipeProfileDirtyTracking();
}

void MainWindow::refreshRecipeProfileDirty()
{
    m_templateEditorPage->refreshRecipeProfileDirty();
}

void MainWindow::clearRecipeProfileDirty()
{
    m_templateEditorPage->clearRecipeProfileDirty();
}

void MainWindow::setupWordTemplateEditorCombo()
{
    m_templateEditorPage->setupWordTemplateEditorCombo();
}

void MainWindow::setupDetectModeChangeTracking()
{
    connect(ui->comboBox_detectionMode,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (m_applyingMachineSettings) {
                    resetTemplateCaptureState();
                    updateTissueRoughnessUiVisibility();
                    refreshWordTemplateEditorCombo();
                    return;
                }

                const QString previousModeId = m_currentDetectModeId;
                const QString nextModeId = detectModeIdForIndex(index);
                resetTemplateCaptureState();
                m_resultBoundDisplayActive = false;
                m_currentDetectModeId = nextModeId;
                updateTissueRoughnessUiVisibility();
                if (imageLabel) {
                    imageLabel->setTemplateDrawingEnabled(false);
                }
                hideTemplateGuide();
                if (previousModeId != nextModeId) {
                    if (isWordFamilyMode(previousModeId)) {
                        clearWordMultiTemplateState();
                    } else if (isSingleTemplateRecipeMode(previousModeId)) {
                        clearSingleTemplateRecipeState();
                    } else {
                        refreshWordTemplateEditorCombo();
                    }
                } else {
                    refreshWordTemplateEditorCombo();
                }
                restoreTemplatesForMode(m_currentDetectModeId, false);
                saveSettings();
            });
}

void MainWindow::clearWordMultiTemplateState()
{
    m_templateEditorPage->clearWordMultiTemplateState();
}

void MainWindow::clearSingleTemplateRecipeState()
{
    m_templateEditorPage->clearSingleTemplateRecipeState();
}

QString MainWindow::detectModeIdForIndex(int index) const
{
    return m_templateEditorPage->detectModeIdForIndex(index);
}

QString MainWindow::currentDetectModeId() const
{
    return m_templateEditorPage->currentDetectModeId();
}

void MainWindow::restoreTemplatesForMode(
        const QString &modeId,
        bool showMessage)
{
    m_templateEditorPage->restoreTemplatesForMode(
                modeId,
                showMessage);
}

void MainWindow::refreshWordTemplateEditorCombo()
{
    m_templateEditorPage->refreshWordTemplateEditorCombo();
}

int MainWindow::currentWordTemplateProfileIndex() const
{
    return m_templateEditorPage
            ->currentWordTemplateProfileIndex();
}

bool MainWindow::applyCameraExposureValue(
    int exposureValue,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const CameraParameterResultDto result =
            m_inspectionApplicationService
            ->applyCameraExposure(exposureValue);
    if (result.minimumValue <= result.maximumValue) {
        QSignalBlocker blocker(ui->spinBox_cameraExposure);
        ui->spinBox_cameraExposure->setRange(
            result.minimumValue, result.maximumValue);
    }
    if (!result.success && errorMessage) {
        *errorMessage = result.diagnostic;
    }
    return result.success;
}

bool MainWindow::applyCameraExposureFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    if (!isCameraOpen()) {
        const QString message = "未打开相机，无法设置曝光！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("警告", message);
        return false;
    }
    QString error;
    if (!applyCameraExposureValue(ui->spinBox_cameraExposure->value(), &error)) {
        const QString message = error.isEmpty()
                ? QString("相机曝光设置失败") : error;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        m_machineSettingsPage->refreshDirty("camera.exposure");
        return false;
    }
    if (showSuccessMessage) {
        showParameterInfo("提示", "相机曝光设置成功！");
    }
    return true;
}

bool MainWindow::applyCameraGainFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    if (!isCameraOpen()) {
        const QString message =
                "相机未初始化或未打开，无法设置增益！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }
    int gainValue = 0;
    if (!parseIntValue(ui->lineEdit_cameraGain->text(), &gainValue)) {
        const CameraParameterResultDto range =
                m_inspectionApplicationService
                ->queryCameraGainRange();
        const QString message = QString(
            "请输入有效的整数增益！当前相机允许范围：%1 ~ %2")
            .arg(range.minimumValue)
            .arg(range.maximumValue);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }
    const CameraParameterResultDto result =
            m_inspectionApplicationService->applyCameraGain(gainValue);
    if (!result.success) {
        if (errors) errors->append(result.diagnostic);
        if (showSuccessMessage) {
            showParameterWarning("提示", result.diagnostic);
        }
        return false;
    }
    if (showSuccessMessage) {
        showParameterInfo("提示", "相机增益设置成功！");
    }
    return true;
}

bool MainWindow::applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage)
{
    PLCmode = ui->comboBox_plcTriggerMode->currentIndex();

    if (!m_inspectionApplicationService
            ->runtimeSnapshot().plcConnected) {
        const QString message = "PLC未连接！";
        if (showSuccessMessage) {
            if (errors) errors->append(message);
            showParameterWarning("警告", message);
            return false;
        }
        return true;
    }

    if (PLCmode != 0 && PLCmode != 1) {
        const QString message = "PLC触发模式无效";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    const QString modeId = machineSettingsTriggerModeIds()
            .value(PLCmode);
    const OperationResult result =
            m_inspectionApplicationService
            ->applyPlcTriggerMode(modeId);
    if (!result.isSuccess()) {
        const QString message = result.error.userMessage;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", PLCmode == 0 ? "连续模式设置成功" : "间歇模式设置成功");
    }
    m_machineSettingsPage->updateAppliedFromUi("plc.trigger_mode");
    m_machineSettingsPage->refreshDirty("plc.trigger_mode");
    saveSettings(false);
    return true;
}

bool MainWindow::applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    if (!m_inspectionApplicationService
            ->runtimeSnapshot().plcConnected) {
        const QString message = "PLC未连接！";
        if (showSuccessMessage) {
            if (errors) errors->append(message);
            showParameterWarning("警告", message);
            return false;
        }
        return true;
    }


    PlcRunSettingsCommand plcSettings;
    plcSettings.rejectTime = static_cast<std::uint16_t>(
                ui->lineEdit_rejectTime->text().toUInt());
    plcSettings.rejectDistance =
            ui->lineEdit_rejectDistance->text().toUInt();
    plcSettings.photoTime = static_cast<std::uint16_t>(
                ui->lineEdit_photoTime->text().toUInt());
    plcSettings.photoDistance =
            ui->lineEdit_photoDistance->text().toUInt();
    const OperationResult result =
            m_inspectionApplicationService
            ->applyPlcRunSettings(plcSettings);
    if (!result.isSuccess()) {
        const QString message = result.error.userMessage;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", "所有设置已经完成！");
    }
    m_machineSettingsPage->updateAppliedFromUi(QStringList()
                                      << "plc.photo_distance"
                                      << "plc.photo_time"
                                      << "plc.camera_delay"
                                      << "plc.reject_distance"
                                      << "plc.reject_time"
                                      << "plc.reject_position");
    m_machineSettingsPage->refreshDirty(QStringList()
                               << "plc.photo_distance"
                               << "plc.photo_time"
                               << "plc.camera_delay"
                               << "plc.reject_distance"
                               << "plc.reject_time"
                               << "plc.reject_position");
    saveSettings(false);
    return true;
}

/**
 * @brief 曝光确定按钮点击槽函数
 * @details 设置相机曝光值
 */
void MainWindow::on_pushButton_applyCameraExposure_clicked()
{
    QStringList errors;
    if (applyCameraExposureFromUi(&errors, true)) {
        m_machineSettingsPage->updateAppliedFromUi("camera.exposure");
        m_machineSettingsPage->refreshDirty("camera.exposure");
        saveSettings(false);
    }
}

/**
 * @brief 获取目标字符串
 * @return QString 目标字符串
 */
/**
 * @brief PLC连接按钮点击槽函数
 * @details 连接到西门子PLC
 */
void MainWindow::on_pushButton_connectPlc_clicked()
{
    PlcConnectionCommand command;
    command.address = ui->lineEdit_plcIpAddress->text();
    command.rack = ui->lineEdit_plcRack->text().toInt();
    command.slot = ui->lineEdit_plcSlot->text().toInt();
    const OperationResult result =
            m_inspectionApplicationService->connectPlc(command);

    if (result.isSuccess())
    {
        m_machineSettingsPage->updateAppliedFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
        m_machineSettingsPage->refreshDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
        saveSettings(false);
        updateHardwareParameterUiEnabled();
        QMessageBox::information(this, "success", "PLC连接成功");
    }
    else
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
}

/**
 * @brief PLC断开按钮点击槽函数
 */
void MainWindow::on_pushButton_disconnectPlc_clicked()
{
    const OperationResult result =
            m_inspectionApplicationService->disconnectPlc();

    if (result.isSuccess())
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::information(this, "success", "PLC断开成功");
    }
    else
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::critical(this, "error", "PLC断开失败");
    }
}

/**
 * @brief 写入批次时间按钮点击槽函数
 * @details 向PLC DB1.982写入WORD值（批次时间）
 */
void MainWindow::on_pushButton_applyPlcProcessParameters_clicked()
{
    QStringList errors;
    applyPlcRunSettingsFromUi(&errors, true);
}



void MainWindow::on_toolButton_stopInspection_clicked()
{
    if (m_inspectionPage) {
        m_inspectionPage->clearDetectionRoiWarning(QString());
    }

    const RuntimeSnapshot before =
            m_inspectionApplicationService->runtimeSnapshot();
    StopInspectionCommand command;
    if (before.state == ApplicationRuntimeState::Fault) {
        if (!confirmInspectionFaultRecovery()) {
            presentInspectionFault();
            return;
        }
        command.acknowledgeFault = true;
    }

    if (m_templateEditorPage
            && m_templateEditorPage->templateOperationActive()) {
        resetTemplateCaptureState();
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
            imageLabel->clearSelection();
        }
        clearBarcodeTemplateValidation();
        hideTemplateGuide();
        ui->label_runtimeStatus->setText(
                    isCameraOpen()
                    ? "已退出模板制作，相机已打开"
                    : "已退出模板制作，相机已关闭");
        updateOperationUiState();
        return;
    }

    if (before.state == ApplicationRuntimeState::Idle) {
        updateOperationUiState();
        return;
    }

    m_barcodeWordRunActive = false;
    finishInspectionStopUi(
                m_inspectionApplicationService->stop(command));
}



/**
 * @brief 目标字符确定按钮点击槽函数
 */

void MainWindow::on_pushButton_applyTargetText_clicked()
{
    m_templateEditorPage->applyCurrentTargetText();
}

void MainWindow::on_pushButton_applyBatchTargetText_clicked()
{
    m_templateEditorPage->applyBatchTargetText();
}

void MainWindow::on_pushButton_applyBatchImageThreshold_clicked()
{
    m_templateEditorPage->applyBatchImageThreshold();
}
void MainWindow::applyMachineSettingsToUi(
    const MachineSettings &settings)
{
    m_templateApplicationService->replacePublishedRecipeIdsByMode(
                settings.publishedRecipeIdsByMode);
    if (m_machineSettingsPage) {
        m_machineSettingsPage->applyToUi(settings);
    }
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
}

void MainWindow::applyRecipeProfileToUi(const RecipeProfile &settings)
{
    QSignalBlocker targetBlocker(ui->textEdit_targetText);
    QSignalBlocker thresholdBlocker(ui->lineEdit_imageThreshold);
    ui->textEdit_targetText->setPlainText(settings.targetText);
    ui->lineEdit_imageThreshold->setText(QString::number(static_cast<int>(settings.imageThresholdPercent)));
    refreshRecipeProfileDirty();
}

/**
 * @brief 设置非公共配置初始值
 * @details 公共配置统一由 MachineSettings::defaults() 提供
 */
void MainWindow::setupNonPersistentDefaults()
{
    ui->lineEdit_imageThreshold->setText(QString::number(
        RecipeProfile::DefaultImageThresholdPercent));
    ui->textEdit_targetText->setPlainText("");
    ui->lineEdit_tissueRoughnessThreshold->setText(
        QString::number(
            TissueRecipeParameters().roughnessThreshold,
            'f', 3));
}

// ================= 拦截滚轮误操作事件 =================
void MainWindow::on_pushButton_applyColorChannel_clicked()
{
    m_machineSettingsPage->updateAppliedFromUi("image.color_channel");
    m_machineSettingsPage->refreshDirty("image.color_channel");
    saveSettings(false);
    showParameterInfo("提示", "颜色通道设置成功");

}


//设置相机增益
void MainWindow::on_pushButton_applyCameraGain_clicked()
{
    QStringList errors;
    if (applyCameraGainFromUi(&errors, true)) {
        m_machineSettingsPage->updateAppliedFromUi("camera.gain");
        m_machineSettingsPage->refreshDirty("camera.gain");
        saveSettings(false);
    }
}

void MainWindow::on_pushButton_applyTissueRoughnessThreshold_clicked()
{
    m_templateEditorPage->applyCurrentTissueThreshold();
}


void MainWindow::on_pushButton_applyPhotoDistance_clicked()
{

    if (!m_inspectionApplicationService
            ->runtimeSnapshot().plcConnected)
    {
        showParameterWarning("警告", "PLC未连接！");
        return;
    }

    const std::uint32_t value =
            ui->lineEdit_photoDistance->text().toUInt();
    const OperationResult result =
            m_inspectionApplicationService
            ->writePlcPhotoDistance(value);
// 判断写入结果
    if (!result.isSuccess())
    {
        // 写入失败
        showParameterWarning("error", "设置拍照距离失败");
    }
    else
    {
        // 写入成功
        m_machineSettingsPage->updateAppliedFromUi("plc.photo_distance");
        m_machineSettingsPage->refreshDirty("plc.photo_distance");
        saveSettings(false);
        showParameterInfo("提示", "拍照距离设置成功");
    }
}
