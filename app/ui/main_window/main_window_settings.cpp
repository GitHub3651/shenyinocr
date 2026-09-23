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
    m_detectionSettingsUi->groupBox_templateManagement->setVisible(usesTemplate);
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
    m_detectionSettingsUi->pushButton_editCharacterTemplates->setVisible(
                showCharacterSettings);
    m_detectionSettingsUi->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    m_detectionSettingsUi->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    m_detectionSettingsUi->pushButton_applyTissueRoughnessThreshold->setVisible(showTissueThreshold);
}

void MainWindow::on_pushButton_clearSoftwareData_clicked()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "删除已保存的软件设置",
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
        qCWarning(logUi).noquote()
                << QStringLiteral(
                    "event=settings.delete_failed path=%1 diagnostic=%2")
                   .arg(settingsPath, settingsFile.errorString());
        showParameterCritical(
                    "删除失败",
                    QStringLiteral("无法删除软件设置，请检查文件权限。"));
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
                QStringLiteral("软件设置保存失败"),
                QStringLiteral(
                    "参数已应用到设备，但未保存到软件设置。"));
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
    if (!result.success && errorMessage) {
        *errorMessage = result.maximumValue > result.minimumValue
                && (exposureValue < result.minimumValue
                    || exposureValue > result.maximumValue)
                ? QStringLiteral("相机曝光值超出允许范围，请输入 %1 ~ %2。")
                  .arg(result.minimumValue)
                  .arg(result.maximumValue)
                : QStringLiteral("相机曝光设置失败，请检查输入值和相机状态。");
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
        if (errors) errors->append(error);
        if (showSuccessMessage) showParameterWarning("提示", error);
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
        const QString message = result.maximumValue > result.minimumValue
                && (gainValue < result.minimumValue
                    || gainValue > result.maximumValue)
                ? QStringLiteral("相机增益值超出允许范围，请输入 %1 ~ %2。")
                  .arg(result.minimumValue)
                  .arg(result.maximumValue)
                : QStringLiteral("相机增益设置失败，请检查输入值和相机状态。");
        if (errors) errors->append(message);
        if (showSuccessMessage) {
            showParameterWarning("提示", message);
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
        showParameterInfo("提示", "PLC 运行参数已应用。");
    }
    return true;
}

void MainWindow::on_pushButton_applyCameraExposure_clicked()
{
    QStringList errors;
    applyCameraExposureFromUi(&errors, true);
}

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

void MainWindow::on_pushButton_applyPlcProcessParameters_clicked()
{
    QStringList errors;
    applyPlcRunSettingsFromUi(&errors, true);
}



void MainWindow::setupNonPersistentDefaults()
{
    m_detectionSettingsUi->lineEdit_imageThreshold->setText(QString::number(
        TemplateSettings::DefaultImageThresholdPercent));
    m_detectionSettingsUi->textEdit_targetText->setPlainText("");
}

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
    if (!result.isSuccess())
    {
        m_machineSettingsPage->restoreAppliedValue("plc.photo_distance");
        showParameterWarning("错误", result.error.userMessage);
    }
    else
    {
        if (saveAppliedHardwareSettings(
                    QStringList() << "plc.photo_distance")) {
            showParameterInfo("提示", "拍照距离设置成功");
        }
    }
}
