// 文件作用：本文件用于绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 主要职责：绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/pages/machine_settings_page.h"

#include "system_support/machine_settings_policy.h"
#include "contracts/detection_mode.h"
#include "ui/controllers/settings_edit_state.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QDoubleValidator>
#include <QEvent>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSplitterHandle>
#include <QUrl>
#include <QWidget>

#include <cmath>

namespace {

// 函数说明：detectModeIds 函数执行对应事件或业务处理。
const QStringList &detectModeIds()
{
    static const QStringList ids = appSettingsDetectionModeIds();
    return ids;
}

// 函数说明：imageSaveModeIds 函数实现名称所表示的处理步骤。
const QStringList &imageSaveModeIds()
{
    static const QStringList ids = appSettingsImageSaveModeIds();
    return ids;
}

// 函数说明：imageSaveTypeIds 函数实现名称所表示的处理步骤。
const QStringList &imageSaveTypeIds()
{
    static const QStringList ids = appSettingsImageSaveTypeIds();
    return ids;
}

// 函数说明：colorChannelIds 函数实现名称所表示的处理步骤。
const QStringList &colorChannelIds()
{
    static const QStringList ids = appSettingsColorChannelIds();
    return ids;
}

// 函数说明：rotationIds 函数实现名称所表示的处理步骤。
const QStringList &rotationIds()
{
    static const QStringList ids = appSettingsRotationIds();
    return ids;
}

// 函数说明：triggerModeIds 函数执行对应事件或业务处理。
const QStringList &triggerModeIds()
{
    static const QStringList ids = appSettingsTriggerModeIds();
    return ids;
}

// 函数说明：idAt 函数实现名称所表示的处理步骤。
QString idAt(
    const QStringList &ids,
    int index,
    const QString &fallback)
{
    return index >= 0 && index < ids.size()
            ? ids.at(index)
            : fallback;
}

// 函数说明：indexOf 函数实现名称所表示的处理步骤。
int indexOf(
    const QStringList &ids,
    const QString &id,
    int fallback)
{
    const int index = ids.indexOf(id);
    return index >= 0 ? index : fallback;
}

// 函数说明：parseInt 函数校验、转换或恢复对应数据。
bool parseInt(const QString &text, int *value)
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

// 函数说明：parseDouble 函数校验、转换或恢复对应数据。
bool parseDouble(const QString &text, double *value)
{
    bool ok = false;
    const double parsed = text.trimmed().toDouble(&ok);
    if (!ok) {
        return false;
    }
    if (value) {
        *value = parsed;
    }
    return true;
}

// 函数说明：text 函数实现名称所表示的处理步骤。
QString text(const wchar_t *value)
{
    return QString::fromWCharArray(value);
}

} // namespace

// 函数说明：MachineSettingsPage 构造函数创建组件并初始化其依赖和初始状态。
MachineSettingsPage::MachineSettingsPage(
    const MachineSettingsPageViewBindings &view,
    SettingsApplicationService *settingsService,
    SettingsEditState *editState,
    QString *selectedDirectory,
    bool *applyingSettings,
    bool *updatingSettingsUi,
    const Callbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_view(view),
      m_appliedSettings(settingsService
                        ? &settingsService->editableDraft()
                        : nullptr),
      m_settingsService(settingsService),
      m_editState(editState),
      m_selectedDirectory(selectedDirectory),
      m_applyingSettings(applyingSettings),
      m_updatingSettingsUi(updatingSettingsUi),
      m_callbacks(callbacks)
{
}

// 函数说明：setupBindings 函数更新或应用对应的配置和状态。
void MachineSettingsPage::setupBindings()
{
    if (!m_view.comboBox_detectionMode || !m_editState) {
        return;
    }

    m_bindings.clear();
    m_hardwareActions.clear();

    {
        QSignalBlocker blocker(m_view.comboBox_detectionMode);
        m_view.comboBox_detectionMode->clear();
        for (const DetectionModeDescriptor &descriptor :
             detectionModeDescriptors()) {
            m_view.comboBox_detectionMode->addItem(
                        QString::fromUtf8(descriptor.displayName),
                        QLatin1String(descriptor.uiId));
        }
    }

    registerGlobalSetting("camera.exposure", m_view.spinBox_cameraExposure,
                          m_view.label_cameraExposure, true,
                          HardwareDependency::Camera);
    registerGlobalSetting("camera.gain", m_view.lineEdit_cameraGain,
                          m_view.label_cameraGain, true,
                          HardwareDependency::Camera);
    registerGlobalSetting("image.color_channel", m_view.comboBox_colorChannel,
                          m_view.label_colorChannel, true);
    registerGlobalSetting("image.rotation", m_view.comboBox_imageRotation,
                          m_view.label_imageRotation, true);
    registerGlobalSetting("plc.trigger_mode", m_view.comboBox_plcTriggerMode,
                          m_view.label_plcTriggerMode, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_distance", m_view.lineEdit_photoDistance,
                          m_view.label_photoDistance, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_time", m_view.lineEdit_photoTime,
                          m_view.label_photoTime, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.camera_delay", m_view.lineEdit_hardwareTriggerDelay,
                          m_view.label_hardwareTriggerDelay, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_distance", m_view.lineEdit_rejectDistance,
                          m_view.label_rejectDistance, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_time", m_view.lineEdit_rejectTime,
                          m_view.label_rejectTime, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_position", m_view.lineEdit_rejectPosition,
                          m_view.label_rejectPosition, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.ip", m_view.lineEdit_plcIpAddress,
                          m_view.label_plcIpAddress, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.rack", m_view.lineEdit_plcRack,
                          m_view.label_plcRackSlot, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.slot", m_view.lineEdit_plcSlot,
                          m_view.label_plcRackSlot, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("detect.mode", m_view.comboBox_detectionMode,
                          m_view.label_detectionMode, false);
    registerGlobalSetting("image.save_mode", m_view.comboBox_imageSaveRange,
                          m_view.label_imageSaveRange, false);
    registerGlobalSetting("image.save_type", m_view.comboBox_imageSaveContent,
                          m_view.label_imageSaveContent, false);
    registerGlobalSetting("image.save_path", m_view.lineEdit_imageSavePath,
                          m_view.label_imageSavePath, false);
    registerGlobalSetting("trigger.enabled", m_view.checkBox_hardwareTriggerEnabled,
                          nullptr, false);

    registerHardwareAction(m_view.pushButton_applyCameraExposure, HardwareDependency::Camera);
    registerHardwareAction(m_view.pushButton_applyCameraGain, HardwareDependency::Camera);
    registerHardwareAction(m_view.pushButton_connectPlc,
                           HardwareDependency::PlcConnection);
    registerHardwareAction(m_view.pushButton_disconnectPlc,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_view.pushButton_applyPlcTriggerMode,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_view.pushButton_applyPhotoDistance,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_view.pushButton_applyPlcProcessParameters,
                           HardwareDependency::PlcRuntime);

    QObject::connect(
        m_view.comboBox_imageSaveRange,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged),
        this,
        [this](int) {
        if (m_callbacks.updateImageSaveOptionsVisibility) {
            m_callbacks.updateImageSaveOptionsVisibility();
        }
    });
    if (m_callbacks.updateImageSaveOptionsVisibility) {
        m_callbacks.updateImageSaveOptionsVisibility();
    }
}

// 函数说明：setupNumericInputValidators 函数更新或应用对应的配置和状态。
void MachineSettingsPage::setupNumericInputValidators()
{
    if (!m_view.comboBox_detectionMode) {
        return;
    }
    auto setIntValidator = [](QLineEdit *lineEdit) {
        if (lineEdit) {
            lineEdit->setValidator(
                new QIntValidator(0, 2147483647, lineEdit));
        }
    };
    setIntValidator(m_view.lineEdit_cameraGain);
    setIntValidator(m_view.lineEdit_plcRack);
    setIntValidator(m_view.lineEdit_plcSlot);
    setIntValidator(m_view.lineEdit_photoDistance);
    setIntValidator(m_view.lineEdit_hardwareTriggerDelay);
    setIntValidator(m_view.lineEdit_rejectDistance);
    setIntValidator(m_view.lineEdit_rejectPosition);
    m_view.lineEdit_photoTime->setValidator(
        new QIntValidator(0, 65535, m_view.lineEdit_photoTime));
    m_view.lineEdit_rejectTime->setValidator(
        new QIntValidator(0, 65535, m_view.lineEdit_rejectTime));

    if (m_view.lineEdit_imageThreshold) {
        m_view.lineEdit_imageThreshold->setValidator(
            new QIntValidator(0, 100, m_view.lineEdit_imageThreshold));
        m_view.lineEdit_imageThreshold->setMaxLength(3);
        m_view.lineEdit_imageThreshold->setToolTip(
            text(L"请输0到100之间的整数，单位：%"));
    }
    if (m_view.lineEdit_tissueRoughnessThreshold) {
        QDoubleValidator *validator = new QDoubleValidator(
            0.001, 1000000.0, 3,
            m_view.lineEdit_tissueRoughnessThreshold);
        validator->setNotation(QDoubleValidator::StandardNotation);
        m_view.lineEdit_tissueRoughnessThreshold->setValidator(validator);
    }
}

// 函数说明：installWheelProtection 函数实现名称所表示的处理步骤。
void MachineSettingsPage::installWheelProtection(
    QWidget *rootWidget)
{
    if (!rootWidget) {
        return;
    }
    const QList<QComboBox *> comboBoxes =
        rootWidget->findChildren<QComboBox *>();
    for (QComboBox *comboBox : comboBoxes) {
        comboBox->installEventFilter(this);
    }
    const QList<QAbstractSpinBox *> spinBoxes =
        rootWidget->findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *spinBox : spinBoxes) {
        spinBox->installEventFilter(this);
    }
}

// 函数说明：setSoftwareDataDirectoryEditor 函数更新或应用对应的配置和状态。
void MachineSettingsPage::setSoftwareDataDirectoryEditor(
    QLineEdit *editor)
{
    if (m_softwareDataDirectoryEditor == editor) {
        return;
    }
    if (m_softwareDataDirectoryEditor) {
        m_softwareDataDirectoryEditor->removeEventFilter(this);
    }
    m_softwareDataDirectoryEditor = editor;
    if (m_softwareDataDirectoryEditor) {
        m_softwareDataDirectoryEditor->installEventFilter(this);
    }
}

// 函数说明：initialize 函数创建、准备或启动对应流程。
void MachineSettingsPage::initialize(
    const AppSettings &settings)
{
    if (!m_appliedSettings) {
        return;
    }
    *m_appliedSettings = settings;
    m_loaded = true;
    applyToUi(settings);
}

// 函数说明：save 函数保存对应的数据和资源。
bool MachineSettingsPage::save(
    bool,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_loaded || !m_appliedSettings) {
        if (errorMessage) {
            *errorMessage = "Global settings have not been loaded.";
        }
        return false;
    }
    syncImmediateSettings();
    const OperationResult saved = m_settingsService
            ? m_settingsService->applyDraft()
            : OperationResult::rejected(
                QStringLiteral("MACHINE_SETTINGS_SERVICE_MISSING"),
                QStringLiteral("机器设置服务不可用。"));
    if (!saved.isSuccess() && errorMessage) {
        *errorMessage = saved.error.userMessage;
    }
    return saved.isSuccess();
}

// 函数说明：clear 函数停止流程、清理状态或释放对应资源。
bool MachineSettingsPage::clear(QString *errorMessage)
{
    const OperationResult cleared = m_settingsService
            ? m_settingsService->clearSettings()
            : OperationResult::rejected(
                QStringLiteral("MACHINE_SETTINGS_SERVICE_MISSING"),
                QStringLiteral("机器设置服务不可用。"));
    if (!cleared.isSuccess()) {
        if (errorMessage) {
            *errorMessage = cleared.error.userMessage;
        }
        return false;
    }
    if (!m_appliedSettings) {
        return false;
    }
    *m_appliedSettings = m_settingsService->current();
    applyToUi(*m_appliedSettings);
    return true;
}

// 函数说明：defaultsForHardwareState 函数实现名称所表示的处理步骤。
AppSettings MachineSettingsPage::defaultsForHardwareState(
    bool cameraOpen,
    bool plcConnected) const
{
    const AppSettings defaults = AppSettings::defaults();
    return MachineSettingsPolicy::defaultsForHardwareState(
        m_appliedSettings ? *m_appliedSettings : defaults,
        defaults,
        cameraOpen,
        plcConnected);
}

// 函数说明：applyToUi 函数更新或应用对应的配置和状态。
void MachineSettingsPage::applyToUi(
    const AppSettings &settings)
{
    if (!m_view.comboBox_detectionMode) {
        return;
    }
    const bool previousApplying =
        m_applyingSettings ? *m_applyingSettings : false;
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_applyingSettings) {
        *m_applyingSettings = true;
    }
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }

    m_view.comboBox_detectionMode->setCurrentIndex(
        indexOf(detectModeIds(), settings.detectModeId, 1));
    m_view.comboBox_imageSaveRange->setCurrentIndex(
        indexOf(imageSaveModeIds(), settings.imageSaveModeId, 0));
    m_view.comboBox_imageSaveContent->setCurrentIndex(
        indexOf(imageSaveTypeIds(), settings.imageSaveTypeId, 1));
    m_view.comboBox_colorChannel->setCurrentIndex(
        indexOf(colorChannelIds(), settings.colorChannelId, 0));
    m_view.comboBox_imageRotation->setCurrentIndex(
        indexOf(rotationIds(), settings.imageRotationId, 0));
    m_view.comboBox_plcTriggerMode->setCurrentIndex(
        indexOf(triggerModeIds(), settings.triggerModeId, 1));
    m_view.checkBox_hardwareTriggerEnabled->setChecked(settings.triggerEnabled);
    m_view.spinBox_cameraExposure->setValue(settings.cameraExposure);
    m_view.lineEdit_cameraGain->setText(
        QString::number(static_cast<int>(settings.cameraGain)));
    m_view.lineEdit_plcIpAddress->setText(settings.plcIp);
    m_view.lineEdit_plcRack->setText(QString::number(settings.plcRack));
    m_view.lineEdit_plcSlot->setText(QString::number(settings.plcSlot));
    m_view.lineEdit_photoDistance->setText(QString::number(settings.photoDistance));
    m_view.lineEdit_photoTime->setText(QString::number(settings.photoTime));
    m_view.lineEdit_hardwareTriggerDelay->setText(QString::number(settings.cameraDelay));
    m_view.lineEdit_rejectDistance->setText(QString::number(settings.rejectDistance));
    m_view.lineEdit_rejectTime->setText(QString::number(settings.rejectTime));
    m_view.lineEdit_rejectPosition->setText(QString::number(settings.rejectPosition));

    if (m_view.splitter_mainContent
            && !settings.rightPanelSplitterState.isEmpty()
            && !m_view.splitter_mainContent->restoreState(
                settings.rightPanelSplitterState)) {
        qWarning("[UI_SETTINGS] invalid right panel splitter state");
    }
    if (m_view.splitter_mainContent) {
        const int handleHeight = m_view.splitter_mainContent
                ->property("visualHandleHeight").toInt();
        if (handleHeight > 0) {
            m_view.splitter_mainContent->setHandleWidth(handleHeight);
            if (QSplitterHandle *handle =
                    m_view.splitter_mainContent->handle(1)) {
                handle->setMinimumHeight(handleHeight);
                handle->setMaximumHeight(handleHeight);
            }
        }
        m_view.splitter_mainContent->setChildrenCollapsible(true);
    }
    if (m_selectedDirectory) {
        *m_selectedDirectory = settings.imageSavePath;
    }
    if (m_callbacks.updateSaveDirectoryText) {
        m_callbacks.updateSaveDirectoryText();
    }
    if (m_callbacks.updateTissueVisibility) {
        m_callbacks.updateTissueVisibility();
    }

    if (m_applyingSettings) {
        *m_applyingSettings = previousApplying;
    }
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
}

// 函数说明：registerGlobalSetting 函数实现名称所表示的处理步骤。
void MachineSettingsPage::registerGlobalSetting(
    const QString &key,
    QWidget *editor,
    QLabel *label,
    bool requireApply,
    HardwareDependency hardwareDependency)
{
    if (key.trimmed().isEmpty() || !editor || !m_editState) {
        return;
    }
    GlobalSettingBinding binding;
    binding.key = key;
    binding.editor = editor;
    binding.label = label;
    binding.originalLabelText = label ? label->text() : QString();
    binding.requireApply = requireApply;
    binding.hardwareDependency = hardwareDependency;
    m_bindings.insert(key, binding);
    m_editState->registerGlobalSetting(key, binding.originalLabelText);

    auto changed = [this, key]() {
        if ((m_updatingSettingsUi && *m_updatingSettingsUi)
                || (m_applyingSettings && *m_applyingSettings)) {
            return;
        }
        const auto it = m_bindings.constFind(key);
        if (it == m_bindings.constEnd()) {
            return;
        }
        if (it.value().requireApply) {
            refreshDirty(key);
            return;
        }
        if (QLineEdit *lineEdit =
                qobject_cast<QLineEdit *>(it.value().editor)) {
            if (!lineEdit->hasAcceptableInput()) {
                return;
            }
        }
        updateAppliedFromUi(key);
        if (m_callbacks.saveSettings) {
            m_callbacks.saveSettings(false);
        }
    };

    if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor)) {
        QObject::connect(lineEdit, &QLineEdit::textChanged,
                         this, [changed](const QString &) { changed(); });
    } else if (QSpinBox *spinBox = qobject_cast<QSpinBox *>(editor)) {
        QObject::connect(
            spinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [changed](int) { changed(); });
    } else if (QComboBox *comboBox = qobject_cast<QComboBox *>(editor)) {
        QObject::connect(
            comboBox,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this, [changed](int) { changed(); });
    } else if (QCheckBox *checkBox = qobject_cast<QCheckBox *>(editor)) {
        QObject::connect(checkBox, &QCheckBox::toggled,
                         this, [changed](bool) { changed(); });
    }
}

// 函数说明：registerHardwareAction 函数实现名称所表示的处理步骤。
void MachineSettingsPage::registerHardwareAction(
    QWidget *control,
    HardwareDependency dependency)
{
    if (!control || dependency == HardwareDependency::None) {
        return;
    }
    HardwareActionBinding binding;
    binding.control = control;
    binding.hardwareDependency = dependency;
    m_hardwareActions.append(binding);
}

// 函数说明：isDirtyByValue 函数检查相关状态并返回判断结果。
bool MachineSettingsPage::isDirtyByValue(
    const QString &key) const
{
    if (!m_view.comboBox_detectionMode || !m_appliedSettings) {
        return false;
    }
    const auto it = m_bindings.constFind(key);
    if (it == m_bindings.constEnd() || !it.value().requireApply) {
        return false;
    }
    auto intDirty = [](QLineEdit *editor, int appliedValue) {
        int value = 0;
        return !editor || !parseInt(editor->text(), &value)
                || value != appliedValue;
    };
    auto doubleDirty = [](QLineEdit *editor, double appliedValue) {
        double value = 0.0;
        return !editor || !parseDouble(editor->text(), &value)
                || std::fabs(value - appliedValue) >= 0.000001;
    };
    auto comboDirty = [](QComboBox *editor,
                         const QStringList &ids,
                         const QString &appliedId) {
        return !editor
                || idAt(ids, editor->currentIndex(), QString())
                   != appliedId;
    };

    if (key == "camera.exposure") {
        return m_view.spinBox_cameraExposure->value()
                != m_appliedSettings->cameraExposure;
    }
    if (key == "camera.gain") {
        return intDirty(m_view.lineEdit_cameraGain,
                        static_cast<int>(m_appliedSettings->cameraGain));
    }
    if (key == "image.color_channel") {
        return comboDirty(m_view.comboBox_colorChannel, colorChannelIds(),
                          m_appliedSettings->colorChannelId);
    }
    if (key == "image.rotation") {
        return comboDirty(m_view.comboBox_imageRotation, rotationIds(),
                          m_appliedSettings->imageRotationId);
    }
    if (key == "plc.trigger_mode") {
        return comboDirty(m_view.comboBox_plcTriggerMode, triggerModeIds(),
                          m_appliedSettings->triggerModeId);
    }
    if (key == "plc.photo_distance") {
        return intDirty(m_view.lineEdit_photoDistance,
                        m_appliedSettings->photoDistance);
    }
    if (key == "plc.photo_time") {
        return intDirty(m_view.lineEdit_photoTime,
                        m_appliedSettings->photoTime);
    }
    if (key == "plc.camera_delay") {
        return intDirty(m_view.lineEdit_hardwareTriggerDelay,
                        m_appliedSettings->cameraDelay);
    }
    if (key == "plc.reject_distance") {
        return intDirty(m_view.lineEdit_rejectDistance,
                        m_appliedSettings->rejectDistance);
    }
    if (key == "plc.reject_time") {
        return intDirty(m_view.lineEdit_rejectTime,
                        m_appliedSettings->rejectTime);
    }
    if (key == "plc.reject_position") {
        return intDirty(m_view.lineEdit_rejectPosition,
                        m_appliedSettings->rejectPosition);
    }
    if (key == "plc.ip") {
        return m_view.lineEdit_plcIpAddress->text().trimmed()
                != m_appliedSettings->plcIp.trimmed();
    }
    if (key == "plc.rack") {
        return intDirty(m_view.lineEdit_plcRack, m_appliedSettings->plcRack);
    }
    if (key == "plc.slot") {
        return intDirty(m_view.lineEdit_plcSlot, m_appliedSettings->plcSlot);
    }
    return false;
}

// 函数说明：refreshDirty 函数更新或应用对应的配置和状态。
void MachineSettingsPage::refreshDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, isDirtyByValue(key));
    updateDirtyLabel(key);
}

// 函数说明：refreshDirty 函数更新或应用对应的配置和状态。
void MachineSettingsPage::refreshDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        refreshDirty(key);
    }
}

// 函数说明：refreshAllDirty 函数更新或应用对应的配置和状态。
void MachineSettingsPage::refreshAllDirty()
{
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        refreshDirty(it.key());
    }
}

// 函数说明：clearDirty 函数停止流程、清理状态或释放对应资源。
void MachineSettingsPage::clearDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, false);
    updateDirtyLabel(key);
}

// 函数说明：clearDirty 函数停止流程、清理状态或释放对应资源。
void MachineSettingsPage::clearDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        clearDirty(key);
    }
}

// 函数说明：clearAllDirty 函数停止流程、清理状态或释放对应资源。
void MachineSettingsPage::clearAllDirty()
{
    if (!m_editState) {
        return;
    }
    m_editState->clearAllGlobalDirty();
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        updateDirtyLabel(it.key());
    }
}

// 函数说明：updateDirtyLabel 函数更新或应用对应的配置和状态。
void MachineSettingsPage::updateDirtyLabel(const QString &key)
{
    if (!m_editState) {
        return;
    }
    const auto it = m_bindings.constFind(key);
    if (it == m_bindings.constEnd() || !it.value().label) {
        return;
    }
    QLabel *label = it.value().label;
    bool anyDirty = false;
    for (auto scan = m_bindings.constBegin();
         scan != m_bindings.constEnd(); ++scan) {
        if (scan.value().label == label
                && m_editState->isGlobalDirty(scan.key())) {
            anyDirty = true;
            break;
        }
    }
    label->setText(anyDirty
                   ? it.value().originalLabelText + " *"
                   : it.value().originalLabelText);
}

// 函数说明：updateAppliedFromUi 函数更新或应用对应的配置和状态。
void MachineSettingsPage::updateAppliedFromUi(
    const QString &key)
{
    if (!m_view.comboBox_detectionMode || !m_appliedSettings) {
        return;
    }
    if (key == "detect.mode") {
        m_appliedSettings->detectModeId = idAt(
            detectModeIds(), m_view.comboBox_detectionMode->currentIndex(),
            m_appliedSettings->detectModeId);
    } else if (key == "image.save_mode") {
        m_appliedSettings->imageSaveModeId = idAt(
            imageSaveModeIds(), m_view.comboBox_imageSaveRange->currentIndex(),
            m_appliedSettings->imageSaveModeId);
    } else if (key == "image.save_type") {
        m_appliedSettings->imageSaveTypeId = idAt(
            imageSaveTypeIds(),
            m_view.comboBox_imageSaveContent->currentIndex(),
            m_appliedSettings->imageSaveTypeId);
    } else if (key == "image.save_path" && m_selectedDirectory) {
        m_appliedSettings->imageSavePath = *m_selectedDirectory;
    } else if (key == "trigger.enabled") {
        m_appliedSettings->triggerEnabled = m_view.checkBox_hardwareTriggerEnabled->isChecked();
    } else if (key == "camera.exposure") {
        m_appliedSettings->cameraExposure = m_view.spinBox_cameraExposure->value();
    } else if (key == "camera.gain") {
        m_appliedSettings->cameraGain = m_view.lineEdit_cameraGain->text().toInt();
    } else if (key == "image.color_channel") {
        m_appliedSettings->colorChannelId = idAt(
            colorChannelIds(), m_view.comboBox_colorChannel->currentIndex(),
            m_appliedSettings->colorChannelId);
    } else if (key == "image.rotation") {
        m_appliedSettings->imageRotationId = idAt(
            rotationIds(), m_view.comboBox_imageRotation->currentIndex(),
            m_appliedSettings->imageRotationId);
    } else if (key == "plc.trigger_mode") {
        m_appliedSettings->triggerModeId = idAt(
            triggerModeIds(), m_view.comboBox_plcTriggerMode->currentIndex(),
            m_appliedSettings->triggerModeId);
    } else if (key == "plc.photo_distance") {
        m_appliedSettings->photoDistance = m_view.lineEdit_photoDistance->text().toInt();
    } else if (key == "plc.photo_time") {
        m_appliedSettings->photoTime = m_view.lineEdit_photoTime->text().toInt();
    } else if (key == "plc.camera_delay") {
        m_appliedSettings->cameraDelay = m_view.lineEdit_hardwareTriggerDelay->text().toInt();
    } else if (key == "plc.reject_distance") {
        m_appliedSettings->rejectDistance = m_view.lineEdit_rejectDistance->text().toInt();
    } else if (key == "plc.reject_time") {
        m_appliedSettings->rejectTime = m_view.lineEdit_rejectTime->text().toInt();
    } else if (key == "plc.reject_position") {
        m_appliedSettings->rejectPosition = m_view.lineEdit_rejectPosition->text().toInt();
    } else if (key == "plc.ip") {
        m_appliedSettings->plcIp = m_view.lineEdit_plcIpAddress->text().trimmed();
    } else if (key == "plc.rack") {
        m_appliedSettings->plcRack = m_view.lineEdit_plcRack->text().toInt();
    } else if (key == "plc.slot") {
        m_appliedSettings->plcSlot = m_view.lineEdit_plcSlot->text().toInt();
    }
}

// 函数说明：updateAppliedFromUi 函数更新或应用对应的配置和状态。
void MachineSettingsPage::updateAppliedFromUi(
    const QStringList &keys)
{
    for (const QString &key : keys) {
        updateAppliedFromUi(key);
    }
}

// 函数说明：syncImmediateSettings 函数实现名称所表示的处理步骤。
void MachineSettingsPage::syncImmediateSettings()
{
    updateAppliedFromUi(QStringList()
        << "detect.mode"
        << "image.save_mode"
        << "image.save_type"
        << "image.save_path"
        << "trigger.enabled"
        << "plc.ip"
        << "plc.rack"
        << "plc.slot"
        );
    if (m_view.comboBox_detectionMode && m_view.splitter_mainContent && m_appliedSettings) {
        m_appliedSettings->rightPanelSplitterState =
            m_view.splitter_mainContent->saveState();
    }
}

// 函数说明：restoreUnappliedMachineSettings 函数校验、转换或恢复对应数据。
void MachineSettingsPage::restoreUnappliedMachineSettings()
{
    if (!m_view.comboBox_detectionMode || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker exposure(m_view.spinBox_cameraExposure);
    QSignalBlocker gain(m_view.lineEdit_cameraGain);
    QSignalBlocker channel(m_view.comboBox_colorChannel);
    QSignalBlocker rotation(m_view.comboBox_imageRotation);
    QSignalBlocker trigger(m_view.comboBox_plcTriggerMode);
    QSignalBlocker photoDistance(m_view.lineEdit_photoDistance);
    QSignalBlocker photoTime(m_view.lineEdit_photoTime);
    QSignalBlocker cameraDelay(m_view.lineEdit_hardwareTriggerDelay);
    QSignalBlocker rejectDistance(m_view.lineEdit_rejectDistance);
    QSignalBlocker rejectTime(m_view.lineEdit_rejectTime);
    QSignalBlocker rejectPosition(m_view.lineEdit_rejectPosition);

    m_view.spinBox_cameraExposure->setValue(m_appliedSettings->cameraExposure);
    m_view.lineEdit_cameraGain->setText(QString::number(
        static_cast<int>(m_appliedSettings->cameraGain)));
    m_view.comboBox_colorChannel->setCurrentIndex(indexOf(
        colorChannelIds(), m_appliedSettings->colorChannelId, 0));
    m_view.comboBox_imageRotation->setCurrentIndex(indexOf(
        rotationIds(), m_appliedSettings->imageRotationId, 0));
    m_view.comboBox_plcTriggerMode->setCurrentIndex(indexOf(
        triggerModeIds(), m_appliedSettings->triggerModeId, 1));
    m_view.lineEdit_photoDistance->setText(
        QString::number(m_appliedSettings->photoDistance));
    m_view.lineEdit_photoTime->setText(
        QString::number(m_appliedSettings->photoTime));
    m_view.lineEdit_hardwareTriggerDelay->setText(
        QString::number(m_appliedSettings->cameraDelay));
    m_view.lineEdit_rejectDistance->setText(
        QString::number(m_appliedSettings->rejectDistance));
    m_view.lineEdit_rejectTime->setText(
        QString::number(m_appliedSettings->rejectTime));
    m_view.lineEdit_rejectPosition->setText(
        QString::number(m_appliedSettings->rejectPosition));
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
    refreshAllDirty();
}

// 函数说明：restoreCameraUiFromApplied 函数校验、转换或恢复对应数据。
void MachineSettingsPage::restoreCameraUiFromApplied()
{
    if (!m_view.comboBox_detectionMode || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker exposure(m_view.spinBox_cameraExposure);
    QSignalBlocker gain(m_view.lineEdit_cameraGain);
    m_view.spinBox_cameraExposure->setValue(m_appliedSettings->cameraExposure);
    m_view.lineEdit_cameraGain->setText(QString::number(
        static_cast<int>(m_appliedSettings->cameraGain)));
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
    refreshDirty(QStringList() << "camera.exposure" << "camera.gain");
}

// 函数说明：restorePlcUiFromApplied 函数校验、转换或恢复对应数据。
void MachineSettingsPage::restorePlcUiFromApplied()
{
    if (!m_view.comboBox_detectionMode || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker trigger(m_view.comboBox_plcTriggerMode);
    QSignalBlocker photoDistance(m_view.lineEdit_photoDistance);
    QSignalBlocker photoTime(m_view.lineEdit_photoTime);
    QSignalBlocker cameraDelay(m_view.lineEdit_hardwareTriggerDelay);
    QSignalBlocker rejectDistance(m_view.lineEdit_rejectDistance);
    QSignalBlocker rejectTime(m_view.lineEdit_rejectTime);
    QSignalBlocker rejectPosition(m_view.lineEdit_rejectPosition);
    m_view.comboBox_plcTriggerMode->setCurrentIndex(
        m_appliedSettings->triggerModeId == "trigger_continuous"
            ? 0 : 1);
    m_view.lineEdit_photoDistance->setText(
        QString::number(m_appliedSettings->photoDistance));
    m_view.lineEdit_photoTime->setText(
        QString::number(m_appliedSettings->photoTime));
    m_view.lineEdit_hardwareTriggerDelay->setText(
        QString::number(m_appliedSettings->cameraDelay));
    m_view.lineEdit_rejectDistance->setText(
        QString::number(m_appliedSettings->rejectDistance));
    m_view.lineEdit_rejectTime->setText(
        QString::number(m_appliedSettings->rejectTime));
    m_view.lineEdit_rejectPosition->setText(
        QString::number(m_appliedSettings->rejectPosition));
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
    refreshDirty(QStringList()
        << "plc.trigger_mode"
        << "plc.photo_distance"
        << "plc.photo_time"
        << "plc.camera_delay"
        << "plc.reject_distance"
        << "plc.reject_time"
        << "plc.reject_position");
}

// 函数说明：disabledStyle 函数实现名称所表示的处理步骤。
QString MachineSettingsPage::disabledStyle(
    QWidget *widget) const
{
    if (qobject_cast<QPushButton *>(widget)) {
        return "QPushButton {background-color:#f5f7fa;color:#a8abb2;"
               "border:1px solid #e4e7ed;border-radius:4px;}"
               "QPushButton:hover,QPushButton:pressed {"
               "background-color:#f5f7fa;}";
    }
    if (qobject_cast<QComboBox *>(widget)) {
        return "QComboBox {background-color:#f5f7fa;color:#a8abb2;"
               "border:1px solid #e4e7ed;border-radius:4px;}"
               "QComboBox::drop-down {background-color:#eef0f3;"
               "border-left:1px solid #e4e7ed;}";
    }
    if (qobject_cast<QAbstractSpinBox *>(widget)) {
        return "QAbstractSpinBox {background-color:#f5f7fa;color:#a8abb2;"
               "border:1px solid #e4e7ed;border-radius:4px;}"
               "QAbstractSpinBox::up-button,QAbstractSpinBox::down-button {"
               "background-color:#eef0f3;}";
    }
    if (qobject_cast<QLineEdit *>(widget)) {
        return "QLineEdit {background-color:#f5f7fa;color:#a8abb2;"
               "border:1px solid #e4e7ed;border-radius:4px;"
               "padding:5px 10px;}";
    }
    if (qobject_cast<QLabel *>(widget)) {
        return "QLabel {background-color:#f5f7fa;color:#a8abb2;"
               "border:1px solid #e4e7ed;border-radius:4px;"
               "padding:5px 10px;}";
    }
    return QString();
}

// 函数说明：setHardwareControlEnabled 函数更新或应用对应的配置和状态。
void MachineSettingsPage::setHardwareControlEnabled(
    QWidget *widget,
    bool enabled,
    const QString &disabledReason,
    bool showDisabledReason)
{
    if (!widget) {
        return;
    }
    static const char styleProperty[] = "_hardwareOriginalStyleSheet";
    static const char toolTipProperty[] = "_hardwareOriginalToolTip";
    if (!widget->property(styleProperty).isValid()) {
        widget->setProperty(styleProperty, widget->styleSheet());
    }
    if (!widget->property(toolTipProperty).isValid()) {
        widget->setProperty(toolTipProperty, widget->toolTip());
    }
    const QString originalStyle = widget->property(styleProperty).toString();
    const QString originalToolTip = widget->property(toolTipProperty).toString();
    const bool label = qobject_cast<QLabel *>(widget) != nullptr;
    widget->setEnabled(label ? true : enabled);
    if (enabled) {
        widget->setStyleSheet(originalStyle);
        widget->setToolTip(originalToolTip);
        widget->unsetCursor();
        return;
    }
    const QString style = disabledStyle(widget);
    widget->setStyleSheet(style.isEmpty() ? originalStyle : style);
    widget->setToolTip(showDisabledReason
                       ? disabledReason : originalToolTip);
    if (showDisabledReason) {
        widget->setCursor(Qt::ForbiddenCursor);
    } else {
        widget->unsetCursor();
    }
}

// 函数说明：applyOperationState 根据统一权限快照更新设置页控件。
void MachineSettingsPage::applyOperationState(
    const OperationUiSnapshot &snapshot)
{
    if (!snapshot.cameraOpen) {
        restoreCameraUiFromApplied();
    }
    if (!snapshot.plcConnected) {
        restorePlcUiFromApplied();
    }
    auto accessFor = [&](HardwareDependency dependency)
            -> OperationUiSnapshot::Access {
        switch (dependency) {
        case HardwareDependency::Camera:
            return snapshot.cameraSettings;
        case HardwareDependency::PlcConnection:
            return snapshot.plcConnection;
        case HardwareDependency::PlcRuntime:
            return snapshot.plcRuntime;
        case HardwareDependency::None:
        default:
            return snapshot.generalSettings;
        }
    };
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        const OperationUiSnapshot::Access access =
                accessFor(it.value().hardwareDependency);
        setHardwareControlEnabled(
                    it.value().editor,
                    access.enabled,
                    access.disabledReason,
                    true);
        setHardwareControlEnabled(
                    it.value().label,
                    access.enabled,
                    access.disabledReason,
                    false);
    }
    for (const HardwareActionBinding &binding : m_hardwareActions) {
        const OperationUiSnapshot::Access access =
                accessFor(binding.hardwareDependency);
        setHardwareControlEnabled(
                    binding.control,
                    access.enabled,
                    access.disabledReason,
                    true);
    }
}

// 函数说明：eventFilter 函数实现名称所表示的处理步骤。
bool MachineSettingsPage::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (watched == m_softwareDataDirectoryEditor
            && event->type() == QEvent::MouseButtonDblClick) {
        const QString path =
            m_softwareDataDirectoryEditor->text().trimmed();
        if (path.isEmpty()) {
            return true;
        }
        QDir directory(path);
        if (!directory.exists()
                && !QDir().mkpath(directory.absolutePath())) {
            if (m_callbacks.reportDirectoryOpenFailure) {
                m_callbacks.reportDirectoryOpenFailure(
                    directory.absolutePath());
            }
            return true;
        }
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(directory.absolutePath()));
        return true;
    }
    if (event->type() == QEvent::Wheel
            && (watched->inherits("QComboBox")
                || watched->inherits("QAbstractSpinBox"))) {
        return true;
    }
    return QObject::eventFilter(watched, event);
}
