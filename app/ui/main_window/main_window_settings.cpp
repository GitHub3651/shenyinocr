// 文件作用：本文件用于实现主窗口中机器设置、PLC参数和软件数据相关的交互槽。
// 主要职责：实现主窗口中机器设置、PLC参数和软件数据相关的交互槽。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
/**
 * @file ui/main_window/main_window_settings.cpp
 * @brief 主窗口设置、模板页面命令与硬件按钮薄协调。
 */

#include "ui/main_window/main_window.h"
#include "ui_detection_settings_page.h"
#include "ui_image_settings_page.h"
#include "ui_inspection_info_page.h"
#include "ui_main_window.h"
#include "ui_plc_settings_page.h"
#include "contracts/detection_mode.h"
#include "system_support/logging/log_categories.h"
#include "ui/main_window/inspection/inspection_page.h"
#include "ui/main_window/settings/machine_settings_page.h"
#include "ui/main_window/template/template_editor_page.h"


#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QMessageBox>
#include <QComboBox>
#include <QSignalBlocker>

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

void MainWindow::updateTissueRoughnessUiVisibility()
{
    DetectionMode mode = DetectionMode::Word;
    const bool validMode = detectionModeFromUiId(
                m_templateEditorPage->detectModeIdForIndex(
                    m_detectionSettingsUi->comboBox_detectionMode->currentIndex()),
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
    ui->widget_templateControls->setVisible(usesTemplate);
    m_inspectionInfoUi->groupBox_currentTemplate->setVisible(usesTemplate);
    m_detectionSettingsUi->groupBox_currentTemplateSettings->setVisible(usesTemplate);
    m_detectionSettingsUi->label_targetText->setVisible(usesTemplate
                                     && descriptor.requiresTargetText);
    m_detectionSettingsUi->textEdit_targetText->setVisible(usesTemplate
                                        && descriptor.requiresTargetText);
    m_detectionSettingsUi->pushButton_applyTargetText->setVisible(
                usesTemplate && descriptor.requiresTargetText);
    m_detectionSettingsUi->label_imageThreshold->setVisible(showImageThreshold);
    m_detectionSettingsUi->lineEdit_imageThreshold->setVisible(showImageThreshold);
    m_detectionSettingsUi->label_imageThresholdUnit->setVisible(showImageThreshold);
    m_detectionSettingsUi->pushButton_applyImageThreshold->setVisible(showImageThreshold);
    ui->toolButton_editCharacterTemplates->setVisible(showCharacterSettings);
    m_detectionSettingsUi->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    m_detectionSettingsUi->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    m_detectionSettingsUi->pushButton_applyTissueRoughnessThreshold->setVisible(showTissueThreshold);
}

void MainWindow::on_pushButton_clearSoftwareData_clicked()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "清空当前软件数据",
                "将删除当前 Windows 用户保存的软件设置。\n\n"
                "下次启动将使用默认设置。\n"
                "产品模板、识别图片、授权文件和日志不会被删除。\n\n"
                "是否继续？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const QString settingsPath = QDir(
                m_settingsApplicationService->applicationDataRoot())
            .filePath(QStringLiteral("settings/app_settings.json"));
    QFile settingsFile(settingsPath);
    if (settingsFile.exists() && !settingsFile.remove()) {
        showParameterCritical(
                    "删除失败",
                    QString("无法删除软件设置：\n%1")
                    .arg(settingsFile.errorString()));
        return;
    }

    QMessageBox::information(
                this,
                "软件即将关闭",
                "当前软件设置已经删除。\n\n"
                "软件将立即关闭，下次启动时将使用默认设置。");
    QCoreApplication::quit();
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
    m_machineSettingsPage->restoreUnappliedMachineSettings();

    m_templateEditorPage->restoreTemplatesForMode(
                m_templateEditorPage->currentDetectModeId(), false);
    m_templateEditorPage->refreshTemplateDirty();
}

void MainWindow::setupDetectModeChangeTracking()
{
    connect(m_detectionSettingsUi->comboBox_detectionMode,
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
                    qCCritical(logUi).noquote()
                            << QStringLiteral(
                                "event=settings.save_failed key=detect.mode value=%1 code=%2 reason=%3")
                               .arg(nextModeId, saved.error.code,
                                    saved.error.userMessage);
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
                DetectionMode savedMode;
                const QString savedModeId = detectionModeFromUiId(
                            nextModeId, &savedMode)
                        ? detectionModeId(savedMode)
                        : nextModeId;
                qCInfo(logUi).noquote()
                        << QStringLiteral(
                            "event=settings.saved key=detect.mode value=%1")
                           .arg(savedModeId);
                m_templateEditorPage->resetTemplateCaptureState();
                m_currentDetectModeId = nextModeId;
                updateTissueRoughnessUiVisibility();
                updateBarcodeCsvUi(
                            m_inspectionApplicationService->runtimeSnapshot());
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
    qCWarning(logUi).noquote()
            << QStringLiteral(
                "event=settings.save_failed keys=%1 code=%2 reason=%3")
               .arg(keys.join(QStringLiteral(",")),
                    saved.error.code, saved.error.userMessage);
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
        QSignalBlocker blocker(m_imageSettingsUi->spinBox_cameraExposure);
        m_imageSettingsUi->spinBox_cameraExposure->setRange(
            result.minimumValue, result.maximumValue);
    }
    if (!result.success && errorMessage) {
        *errorMessage = result.diagnostic;
    }
    if (!result.success) {
        qCWarning(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.exposure_rejected requested=%1 nativeCode=%2 reason=%3")
                   .arg(exposureValue)
                   .arg(result.nativeErrorCode)
                   .arg(result.diagnostic);
    } else {
        qCInfo(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.exposure_applied requested=%1 actual=%2 range=%3-%4")
                   .arg(exposureValue)
                   .arg(result.actualValue)
                   .arg(result.minimumValue)
                   .arg(result.maximumValue);
    }
    return result.success;
}

bool MainWindow::applyCameraExposureFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    QString error;
    if (!applyCameraExposureValue(m_imageSettingsUi->spinBox_cameraExposure->value(), &error)) {
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
    if (!parseIntValue(m_imageSettingsUi->lineEdit_cameraGain->text(), &gainValue)) {
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
        qCWarning(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.gain_rejected requested=%1 nativeCode=%2 reason=%3")
                   .arg(gainValue)
                   .arg(result.nativeErrorCode)
                   .arg(result.diagnostic);
        if (errors) errors->append(result.diagnostic);
        if (showSuccessMessage) {
            showParameterWarning("提示", result.diagnostic);
        }
        m_machineSettingsPage->restoreAppliedValue("camera.gain");
        return false;
    }
    qCInfo(logDevice).noquote()
            << QStringLiteral(
                "event=camera.gain_applied requested=%1 actual=%2 range=%3-%4")
               .arg(gainValue)
               .arg(result.actualValue)
               .arg(result.minimumValue)
               .arg(result.maximumValue);
    const bool persisted = saveAppliedHardwareSettings(
                QStringList() << "camera.gain");
    if (showSuccessMessage && persisted) {
        showParameterInfo("提示", "相机增益设置成功！");
    }
    return true;
}

bool MainWindow::applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage)
{
    const int plcMode = m_plcSettingsUi->comboBox_plcTriggerMode->currentIndex();

    if (plcMode != 0 && plcMode != 1) {
        const QString message = "PLC触发模式无效";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("错误", message);
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
        if (showSuccessMessage) showParameterWarning("错误", message);
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
    if (!m_detectionSettingsUi->lineEdit_photoDistance->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_photoTime->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_hardwareTriggerDelay->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_rejectDistance->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_rejectTime->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_rejectPosition->hasAcceptableInput()) {
        const QString message = QStringLiteral("PLC 过程参数必须是有效整数。");
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("错误", message);
        m_machineSettingsPage->restoreAppliedValues(keys);
        return false;
    }
    PlcRunSettingsCommand plcSettings;
    plcSettings.rejectTime = static_cast<std::uint16_t>(
                m_plcSettingsUi->lineEdit_rejectTime->text().toUInt());
    plcSettings.rejectDistance =
            m_plcSettingsUi->lineEdit_rejectDistance->text().toUInt();
    plcSettings.photoTime = static_cast<std::uint16_t>(
                m_plcSettingsUi->lineEdit_photoTime->text().toUInt());
    plcSettings.photoDistance =
            m_detectionSettingsUi->lineEdit_photoDistance->text().toUInt();
    const OperationResult result =
            m_inspectionApplicationService
            ->applyPlcRunSettings(plcSettings);
    if (!result.isSuccess()) {
        const QString message = result.error.userMessage;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("错误", message);
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
    if (m_plcSettingsUi->lineEdit_plcIpAddress->text().trimmed().isEmpty()
            || !m_plcSettingsUi->lineEdit_plcRack->hasAcceptableInput()
            || !m_plcSettingsUi->lineEdit_plcSlot->hasAcceptableInput()) {
        m_machineSettingsPage->restoreAppliedValues(connectionKeys);
        showParameterWarning("错误", "PLC 连接参数无效");
        return;
    }
    PlcConnectionCommand command;
    command.address = m_plcSettingsUi->lineEdit_plcIpAddress->text();
    command.rack = m_plcSettingsUi->lineEdit_plcRack->text().toInt();
    command.slot = m_plcSettingsUi->lineEdit_plcSlot->text().toInt();
    const OperationResult result =
            m_inspectionApplicationService->connectPlc(command);

    if (result.isSuccess())
    {
        const bool persisted =
                saveAppliedHardwareSettings(connectionKeys);
        updateOperationUiState();
        if (persisted) {
            QMessageBox::information(this, "成功", "PLC连接成功");
        }
    }
    else
    {
        m_machineSettingsPage->restoreAppliedValues(connectionKeys);
        updateOperationUiState();
        QMessageBox::critical(
                    this, "错误", result.error.userMessage);
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
        QMessageBox::information(this, "成功", "PLC断开成功");
    }
    else
    {
        updateOperationUiState();
        QMessageBox::critical(
                    this, "错误", result.error.userMessage);
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
    const RuntimeSnapshot before =
            m_inspectionApplicationService->runtimeSnapshot();
    StopInspectionCommand command;
    if (before.state == ApplicationRuntimeState::Fault) {
        if (!m_inspectionPage->confirmFaultRecovery(
                m_runtime->faultSnapshot())) {
            presentInspectionFault();
            return;
        }
        command.acknowledgeFault = true;
    }

    if (m_templateEditorPage->templateOperationActive()) {
        m_templateEditorPage->resetTemplateCaptureState();
        m_templateEditorPage->cancelTemplateDrawing();
        m_inspectionInfoUi->label_runtimeStatus->setText(
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
/**
 * @brief 设置非公共配置初始值
 * @details 公共配置统一由 AppSettings::defaults() 提供
 */
void MainWindow::setupNonPersistentDefaults()
{
    m_detectionSettingsUi->lineEdit_imageThreshold->setText(QString::number(
        TemplateSettings::DefaultImageThresholdPercent));
    m_detectionSettingsUi->textEdit_targetText->setPlainText("");
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
        qCCritical(logUi).noquote()
                << QStringLiteral(
                    "event=settings.save_failed key=image.color_channel value=%1 code=%2 reason=%3")
                   .arg(m_machineSettingsPage->settingValueText(
                            QStringLiteral("image.color_channel")),
                        saved.error.code, saved.error.userMessage);
        m_machineSettingsPage->restoreAppliedValues(keys);
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("颜色通道保存失败：\n%1")
                    .arg(saved.error.userMessage));
        return;
    }
    m_machineSettingsPage->refreshDirty(keys);
    qCInfo(logUi).noquote()
            << QStringLiteral(
                "event=settings.saved key=image.color_channel value=%1")
               .arg(m_machineSettingsPage->settingValueText(
                        QStringLiteral("image.color_channel")));
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
    if (!m_detectionSettingsUi->lineEdit_photoDistance->hasAcceptableInput()) {
        m_machineSettingsPage->restoreAppliedValue("plc.photo_distance");
        showParameterWarning("错误", "拍照距离必须是有效整数");
        return;
    }
    const std::uint32_t value =
            m_detectionSettingsUi->lineEdit_photoDistance->text().toUInt();
    const OperationResult result =
            m_inspectionApplicationService
            ->writePlcPhotoDistance(value);
// 判断写入结果
    if (!result.isSuccess())
    {
        // 写入失败
        m_machineSettingsPage->restoreAppliedValue("plc.photo_distance");
        showParameterWarning("错误", result.error.userMessage);
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
