// 文件作用：本文件用于实现主窗口中机器设置、PLC参数和软件数据相关的交互槽。
// 主要职责：实现主窗口中机器设置、PLC参数和软件数据相关的交互槽。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
/**
 * @file ui/main_window_settings.cpp
 * @brief 主窗口设置、模板页面命令与硬件按钮薄协调。
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

} // namespace

void MainWindow::updateSaveDirButtonText()
{
    if (!ui || !ui->lineEdit_imageSavePath || !ui->pushButton_browseImageSavePath) {
        return;
    }

    const QString saveDir = ui->lineEdit_imageSavePath->text().trimmed();
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

    DetectionMode mode = DetectionMode::Word;
    const bool validMode = detectionModeFromUiId(
                m_templateEditorPage->detectModeIdForIndex(
                    ui->comboBox_detectionMode->currentIndex()),
                &mode);
    const DetectionModeDescriptor descriptor =
            detectionModeDescriptor(mode);
    const bool usesTemplate = validMode
            && descriptor.trackingKind != DetectionTrackingKind::WholeFrame;
    const bool showTissueThreshold = validMode
            && mode == DetectionMode::Tissue;
    const bool showCharacterSettings = usesTemplate
            && descriptor.requiresCharacterTemplates;
    const bool showImageThreshold = usesTemplate
            && mode != DetectionMode::Ocr;
    const bool showBatch = usesTemplate
            && descriptor.trackingKind
               == DetectionTrackingKind::MultipleTemplates;

    ui->toolButton_selectTemplate->setVisible(usesTemplate);
    ui->toolButton_createTemplate->setVisible(usesTemplate);
    ui->pushButton_saveTemplate->setVisible(usesTemplate);
    ui->groupBox_currentTemplate->setVisible(usesTemplate);
    ui->groupBox_currentTemplateSettings->setVisible(usesTemplate);
    ui->groupBox_templateCreation->setVisible(usesTemplate);
    ui->label_targetText->setVisible(usesTemplate
                                     && descriptor.requiresTargetText);
    ui->textEdit_targetText->setVisible(usesTemplate
                                        && descriptor.requiresTargetText);
    ui->pushButton_applyTargetText->setVisible(
                usesTemplate && descriptor.requiresTargetText);
    ui->pushButton_applyBatchTargetText->setVisible(
                showBatch && descriptor.requiresTargetText);
    ui->label_imageThreshold->setVisible(showImageThreshold);
    ui->lineEdit_imageThreshold->setVisible(showImageThreshold);
    ui->label_imageThresholdUnit->setVisible(showImageThreshold);
    ui->pushButton_applyImageThreshold->setVisible(showImageThreshold);
    ui->pushButton_applyBatchImageThreshold->setVisible(
                showBatch && showImageThreshold);
    ui->pushButton_editCharacterTemplates->setVisible(
                showCharacterSettings);
    ui->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->pushButton_applyTissueRoughnessThreshold->setVisible(showTissueThreshold);
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

    ui->pushButton_clearSoftwareData->setToolTip(
                "只清除当前 Windows 用户的软件公共界面设置，不删除产品模板、识别图片、授权文件或日志。");
    connect(ui->pushButton_clearSoftwareData,
            &QPushButton::clicked,
            this,
            &MainWindow::clearCurrentSoftwareData);

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

    m_templateEditorPage->clearTemplateState();
    m_templateEditorPage->setCurrentTemplateNameVisible(false);
    m_templateEditorPage->updateCurrentTemplateName();
    if (m_templateEditorPage) {
        m_templateEditorPage->cancelTemplateDrawing();
    }
    m_machineSettingsPage->clearAllDirty();
    m_templateEditorPage->clearTemplateDirty();
    updateOperationUiState();
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
    const AppSettings editableDefaults =
            m_machineSettingsPage->defaultsForHardwareState(
                cameraOpen, plcConnected);

    AppSettings candidate = m_settingsApplicationService->current();
    candidate.detectModeId = editableDefaults.detectModeId;
    candidate.imageSaveModeId = editableDefaults.imageSaveModeId;
    candidate.imageSaveTypeId = editableDefaults.imageSaveTypeId;
    candidate.imageSavePath = editableDefaults.imageSavePath;
    candidate.triggerEnabled = editableDefaults.triggerEnabled;
    const OperationResult saved =
            m_settingsApplicationService->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        applyMachineSettingsToUi(m_settingsApplicationService->current());
        showParameterCritical(
                    "严重警告",
                    QString("恢复默认设置失败：\n%1")
                    .arg(saved.error.userMessage));
        return;
    }

    applyMachineSettingsToUi(editableDefaults);
    m_templateEditorPage->clearTemplateDirty();
    updateOperationUiState();
    m_machineSettingsPage->refreshAllDirty();

    showParameterInfo(
        "提示",
        "当前可设置参数已恢复为默认值。带 * 的参数需要点击对应【设置】后才会生效。");
}

bool MainWindow::hasDirtySettings() const
{
    return m_settingsEditState.hasDirtySettings();
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

    m_templateEditorPage->restoreTemplatesForMode(
                m_templateEditorPage->currentDetectModeId(), false);
    m_templateEditorPage->refreshTemplateDirty();
}

void MainWindow::setupDetectModeChangeTracking()
{
    connect(ui->comboBox_detectionMode,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                const QString nextModeId =
                        m_templateEditorPage->detectModeIdForIndex(index);
                const QString previousModeId =
                        m_settingsApplicationService->current().detectModeId;
                if (nextModeId == previousModeId) {
                    return;
                }
                AppSettings candidate =
                        m_settingsApplicationService->current();
                candidate.detectModeId = nextModeId;
                const OperationResult saved =
                        m_settingsApplicationService
                        ->saveConfiguration(candidate);
                if (!saved.isSuccess()) {
                    m_machineSettingsPage->restoreAppliedValue(
                                QStringLiteral("detect.mode"));
                    m_currentDetectModeId = previousModeId;
                    updateTissueRoughnessUiVisibility();
                    showParameterCritical(
                                QStringLiteral("严重警告"),
                                QStringLiteral("检测模式保存失败：\n%1")
                                .arg(saved.error.userMessage));
                    return;
                }
                resetTemplateCaptureState();
                m_currentDetectModeId = nextModeId;
                updateTissueRoughnessUiVisibility();
                m_templateEditorPage->cancelTemplateDrawing();
                if (previousModeId != nextModeId) {
                    m_templateEditorPage->clearTemplateState();
                }
                m_templateEditorPage->refreshCurrentTemplateEditor();
                m_templateEditorPage->restoreTemplatesForMode(
                            m_currentDetectModeId, false);
            });
}

bool MainWindow::saveAppliedHardwareSettings(
    const QStringList &keys)
{
    AppSettings candidate = m_settingsApplicationService->current();
    m_machineSettingsPage->copyUiValuesTo(candidate, keys);
    const OperationResult saved =
            m_settingsApplicationService
            ->commitAppliedHardwareSettings(candidate);
    m_machineSettingsPage->refreshDirty(keys);
    if (saved.isSuccess()) {
        return true;
    }
    showParameterWarning(
                QStringLiteral("配置保存失败"),
                QStringLiteral(
                    "参数已下发，但保存配置失败，重启后可能不会保留。\n\n%1")
                .arg(saved.error.userMessage));
    return false;
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
    QString error;
    if (!applyCameraExposureValue(ui->spinBox_cameraExposure->value(), &error)) {
        const QString message = error.isEmpty()
                ? QString("相机曝光设置失败") : error;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        m_machineSettingsPage->restoreAppliedValue("camera.exposure");
        return false;
    }
    const bool persisted = saveAppliedHardwareSettings(
                QStringList() << "camera.exposure");
    if (showSuccessMessage && persisted) {
        showParameterInfo("提示", "相机曝光设置成功！");
    }
    return true;
}

bool MainWindow::applyCameraGainFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
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
        m_machineSettingsPage->restoreAppliedValue("camera.gain");
        return false;
    }
    const CameraParameterResultDto result =
            m_inspectionApplicationService->applyCameraGain(gainValue);
    if (!result.success) {
        if (errors) errors->append(result.diagnostic);
        if (showSuccessMessage) {
            showParameterWarning("提示", result.diagnostic);
        }
        m_machineSettingsPage->restoreAppliedValue("camera.gain");
        return false;
    }
    const bool persisted = saveAppliedHardwareSettings(
                QStringList() << "camera.gain");
    if (showSuccessMessage && persisted) {
        showParameterInfo("提示", "相机增益设置成功！");
    }
    return true;
}

bool MainWindow::applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage)
{
    const int plcMode = ui->comboBox_plcTriggerMode->currentIndex();

    if (plcMode != 0 && plcMode != 1) {
        const QString message = "PLC触发模式无效";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        m_machineSettingsPage->restoreAppliedValue("plc.trigger_mode");
        return false;
    }

    const QString modeId = appSettingsTriggerModeIds()
            .value(plcMode);
    const OperationResult result =
            m_inspectionApplicationService
            ->applyPlcTriggerMode(modeId);
    if (!result.isSuccess()) {
        const QString message = result.error.userMessage;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        m_machineSettingsPage->restoreAppliedValue("plc.trigger_mode");
        return false;
    }

    const bool persisted = saveAppliedHardwareSettings(
                QStringList() << "plc.trigger_mode");
    if (showSuccessMessage && persisted) {
        showParameterInfo("提示", plcMode == 0
                          ? "连续模式设置成功"
                          : "间歇模式设置成功");
    }
    return true;
}

bool MainWindow::applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    const QStringList keys = QStringList()
            << "plc.photo_distance"
            << "plc.photo_time"
            << "plc.camera_delay"
            << "plc.reject_distance"
            << "plc.reject_time"
            << "plc.reject_position";
    if (!ui->lineEdit_photoDistance->hasAcceptableInput()
            || !ui->lineEdit_photoTime->hasAcceptableInput()
            || !ui->lineEdit_hardwareTriggerDelay->hasAcceptableInput()
            || !ui->lineEdit_rejectDistance->hasAcceptableInput()
            || !ui->lineEdit_rejectTime->hasAcceptableInput()
            || !ui->lineEdit_rejectPosition->hasAcceptableInput()) {
        const QString message = QStringLiteral("PLC 过程参数必须是有效整数。");
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        m_machineSettingsPage->restoreAppliedValues(keys);
        return false;
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
        m_machineSettingsPage->restoreAppliedValues(keys);
        return false;
    }

    const bool persisted = saveAppliedHardwareSettings(keys);
    if (showSuccessMessage && persisted) {
        showParameterInfo("提示", "所有设置已经完成！");
    }
    return true;
}

/**
 * @brief 曝光确定按钮点击槽函数
 * @details 设置相机曝光值
 */
void MainWindow::on_pushButton_applyCameraExposure_clicked()
{
    QStringList errors;
    applyCameraExposureFromUi(&errors, true);
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
    const QStringList connectionKeys =
            QStringList() << "plc.ip" << "plc.rack" << "plc.slot";
    if (ui->lineEdit_plcIpAddress->text().trimmed().isEmpty()
            || !ui->lineEdit_plcRack->hasAcceptableInput()
            || !ui->lineEdit_plcSlot->hasAcceptableInput()) {
        m_machineSettingsPage->restoreAppliedValues(connectionKeys);
        showParameterWarning("error", "PLC 连接参数无效");
        return;
    }
    PlcConnectionCommand command;
    command.address = ui->lineEdit_plcIpAddress->text();
    command.rack = ui->lineEdit_plcRack->text().toInt();
    command.slot = ui->lineEdit_plcSlot->text().toInt();
    const OperationResult result =
            m_inspectionApplicationService->connectPlc(command);

    if (result.isSuccess())
    {
        const bool persisted =
                saveAppliedHardwareSettings(connectionKeys);
        updateOperationUiState();
        if (persisted) {
            QMessageBox::information(this, "success", "PLC连接成功");
        }
    }
    else
    {
        m_machineSettingsPage->restoreAppliedValues(connectionKeys);
        updateOperationUiState();
        QMessageBox::critical(
                    this, "error", result.error.userMessage);
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
        updateOperationUiState();
        QMessageBox::information(this, "success", "PLC断开成功");
    }
    else
    {
        updateOperationUiState();
        QMessageBox::critical(
                    this, "error", result.error.userMessage);
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
        m_templateEditorPage->cancelTemplateDrawing();
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

    finishInspectionStopUi(
                m_inspectionApplicationService->stop(command));
}
void MainWindow::applyMachineSettingsToUi(
    const AppSettings &settings)
{
    if (m_machineSettingsPage) {
        m_machineSettingsPage->applyToUi(settings);
    }
    m_currentDetectModeId = m_templateEditorPage->currentDetectModeId();
    m_templateEditorPage->restoreTemplatesForMode(
                m_currentDetectModeId, false);
}

/**
 * @brief 设置非公共配置初始值
 * @details 公共配置统一由 AppSettings::defaults() 提供
 */
void MainWindow::setupNonPersistentDefaults()
{
    ui->lineEdit_imageThreshold->setText(QString::number(
        TemplateSettings::DefaultImageThresholdPercent));
    ui->textEdit_targetText->setPlainText("");
    ui->lineEdit_tissueRoughnessThreshold->setText(
        QString::number(
            6.0,
            'f', 3));
}

// ================= 拦截滚轮误操作事件 =================
void MainWindow::on_pushButton_applyColorChannel_clicked()
{
    const QStringList keys = QStringList() << "image.color_channel";
    AppSettings candidate = m_settingsApplicationService->current();
    m_machineSettingsPage->copyUiValuesTo(candidate, keys);
    const OperationResult saved =
            m_settingsApplicationService->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        m_machineSettingsPage->restoreAppliedValues(keys);
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("颜色通道保存失败：\n%1")
                    .arg(saved.error.userMessage));
        return;
    }
    m_machineSettingsPage->refreshDirty(keys);
    showParameterInfo("提示", "颜色通道设置成功");
}


//设置相机增益
void MainWindow::on_pushButton_applyCameraGain_clicked()
{
    QStringList errors;
    applyCameraGainFromUi(&errors, true);
}

void MainWindow::on_pushButton_applyPhotoDistance_clicked()
{
    if (!ui->lineEdit_photoDistance->hasAcceptableInput()) {
        m_machineSettingsPage->restoreAppliedValue("plc.photo_distance");
        showParameterWarning("error", "拍照距离必须是有效整数");
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
        m_machineSettingsPage->restoreAppliedValue("plc.photo_distance");
        showParameterWarning("error", result.error.userMessage);
    }
    else
    {
        // 写入成功
        if (saveAppliedHardwareSettings(
                    QStringList() << "plc.photo_distance")) {
            showParameterInfo("提示", "拍照距离设置成功");
        }
    }
}
