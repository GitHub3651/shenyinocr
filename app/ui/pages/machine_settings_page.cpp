// 文件作用：本文件用于绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 主要职责：绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/pages/machine_settings_page.h"

#include "system_support/machine_settings_policy.h"
#include "contracts/detection_mode.h"
#include "ui/controllers/settings_edit_state.h"

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
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSplitterHandle>
#include <QUrl>
#include <QWidget>

#include <cmath>

namespace {

const QStringList &detectModeIds()
{
    static const QStringList ids = appSettingsDetectionModeIds();
    return ids;
}

const QStringList &imageSaveModeIds()
{
    static const QStringList ids = appSettingsImageSaveModeIds();
    return ids;
}

const QStringList &imageSaveTypeIds()
{
    static const QStringList ids = appSettingsImageSaveTypeIds();
    return ids;
}

const QStringList &colorChannelIds()
{
    static const QStringList ids = appSettingsColorChannelIds();
    return ids;
}

const QStringList &rotationIds()
{
    static const QStringList ids = appSettingsRotationIds();
    return ids;
}

const QStringList &triggerModeIds()
{
    static const QStringList ids = appSettingsTriggerModeIds();
    return ids;
}

QString idAt(
    const QStringList &ids,
    int index,
    const QString &fallback)
{
    return index >= 0 && index < ids.size()
            ? ids.at(index)
            : fallback;
}

int indexOf(
    const QStringList &ids,
    const QString &id,
    int fallback)
{
    const int index = ids.indexOf(id);
    return index >= 0 ? index : fallback;
}

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

} // namespace

MachineSettingsPage::MachineSettingsPage(
    const MachineSettingsPageViewBindings &view,
    SettingsApplicationService *settingsService,
    SettingsEditState *editState,
    const Callbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_view(view),
      m_settingsService(settingsService),
      m_editState(editState),
      m_callbacks(callbacks)
{
}

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
    registerGlobalSetting("tissue.roughness_threshold",
                          m_view.lineEdit_tissueRoughnessThreshold,
                          m_view.label_tissueRoughnessThreshold, true);
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
                          m_view.label_plcIpAddress, true,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.rack", m_view.lineEdit_plcRack,
                          m_view.label_plcRackSlot, true,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.slot", m_view.lineEdit_plcSlot,
                          m_view.label_plcRackSlot, true,
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
        m_view.pushButton_applyTissueRoughnessThreshold,
        &QPushButton::clicked,
        this,
        [this]() { applyTissueRoughnessThreshold(); });

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

    if (m_view.lineEdit_tissueRoughnessThreshold) {
        QDoubleValidator *validator = new QDoubleValidator(
            0.001, 1000000.0, 3,
            m_view.lineEdit_tissueRoughnessThreshold);
        validator->setNotation(QDoubleValidator::StandardNotation);
        m_view.lineEdit_tissueRoughnessThreshold->setValidator(validator);
    }
}

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

void MachineSettingsPage::initialize(
    const AppSettings &settings)
{
    m_loaded = true;
    applyToUi(settings);
}

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
    applyToUi(m_settingsService->current());
    return true;
}

AppSettings MachineSettingsPage::defaultsForHardwareState(
    bool cameraOpen,
    bool plcConnected) const
{
    const AppSettings defaults = AppSettings::defaults();
    return MachineSettingsPolicy::defaultsForHardwareState(
        m_settingsService ? m_settingsService->current() : defaults,
        defaults,
        cameraOpen,
        plcConnected);
}

void MachineSettingsPage::applyToUi(
    const AppSettings &settings)
{
    if (!m_view.comboBox_detectionMode) {
        return;
    }
    const bool previousApplying = m_applyingSettings;
    const bool previousUpdating = m_updatingSettingsUi;
    m_applyingSettings = true;
    m_updatingSettingsUi = true;
    QSignalBlocker detectionModeBlocker(m_view.comboBox_detectionMode);

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
    m_view.lineEdit_imageSavePath->setText(settings.imageSavePath);
    m_view.lineEdit_tissueRoughnessThreshold->setText(
                QString::number(
                    settings.detectionSchemes.tissueRoughnessThreshold,
                    'f', 3));

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
    if (m_callbacks.updateSaveDirectoryText) {
        m_callbacks.updateSaveDirectoryText();
    }
    if (m_callbacks.updateTissueVisibility) {
        m_callbacks.updateTissueVisibility();
    }

    m_applyingSettings = previousApplying;
    m_updatingSettingsUi = previousUpdating;
}

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
        if (m_updatingSettingsUi || m_applyingSettings) {
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
        if (key == QStringLiteral("detect.mode")) {
            return;
        }
        if (QLineEdit *lineEdit =
                qobject_cast<QLineEdit *>(it.value().editor)) {
            if (!lineEdit->hasAcceptableInput()) {
                return;
            }
        }
        if (!m_settingsService) {
            return;
        }
        AppSettings candidate = m_settingsService->current();
        copyUiValuesTo(candidate, QStringList() << key);
        const OperationResult saved =
                m_settingsService->saveConfiguration(candidate);
        if (!saved.isSuccess()) {
            restoreAppliedValue(key);
            QMessageBox::critical(
                        it.value().editor,
                        QStringLiteral("严重警告"),
                        QStringLiteral("当前界面设置保存失败：\n%1")
                        .arg(saved.error.userMessage));
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

bool MachineSettingsPage::isDirtyByValue(
    const QString &key) const
{
    if (!m_view.comboBox_detectionMode || !m_settingsService) {
        return false;
    }
    const AppSettings &applied = m_settingsService->current();
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
                != applied.cameraExposure;
    }
    if (key == "camera.gain") {
        return intDirty(m_view.lineEdit_cameraGain,
                        static_cast<int>(applied.cameraGain));
    }
    if (key == "image.color_channel") {
        return comboDirty(m_view.comboBox_colorChannel, colorChannelIds(),
                          applied.colorChannelId);
    }
    if (key == "image.rotation") {
        return comboDirty(m_view.comboBox_imageRotation, rotationIds(),
                          applied.imageRotationId);
    }
    if (key == "tissue.roughness_threshold") {
        return doubleDirty(
                    m_view.lineEdit_tissueRoughnessThreshold,
                    applied.detectionSchemes.tissueRoughnessThreshold);
    }
    if (key == "plc.trigger_mode") {
        return comboDirty(m_view.comboBox_plcTriggerMode, triggerModeIds(),
                          applied.triggerModeId);
    }
    if (key == "plc.photo_distance") {
        return intDirty(m_view.lineEdit_photoDistance,
                        applied.photoDistance);
    }
    if (key == "plc.photo_time") {
        return intDirty(m_view.lineEdit_photoTime,
                        applied.photoTime);
    }
    if (key == "plc.camera_delay") {
        return intDirty(m_view.lineEdit_hardwareTriggerDelay,
                        applied.cameraDelay);
    }
    if (key == "plc.reject_distance") {
        return intDirty(m_view.lineEdit_rejectDistance,
                        applied.rejectDistance);
    }
    if (key == "plc.reject_time") {
        return intDirty(m_view.lineEdit_rejectTime,
                        applied.rejectTime);
    }
    if (key == "plc.reject_position") {
        return intDirty(m_view.lineEdit_rejectPosition,
                        applied.rejectPosition);
    }
    if (key == "plc.ip") {
        return m_view.lineEdit_plcIpAddress->text().trimmed()
                != applied.plcIp.trimmed();
    }
    if (key == "plc.rack") {
        return intDirty(m_view.lineEdit_plcRack, applied.plcRack);
    }
    if (key == "plc.slot") {
        return intDirty(m_view.lineEdit_plcSlot, applied.plcSlot);
    }
    return false;
}

void MachineSettingsPage::refreshDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, isDirtyByValue(key));
    updateDirtyLabel(key);
}

void MachineSettingsPage::refreshDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        refreshDirty(key);
    }
}

void MachineSettingsPage::refreshAllDirty()
{
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        refreshDirty(it.key());
    }
}

void MachineSettingsPage::clearDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, false);
    updateDirtyLabel(key);
}

void MachineSettingsPage::clearDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        clearDirty(key);
    }
}

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

void MachineSettingsPage::copyUiValuesTo(
    AppSettings &settings,
    const QStringList &keys) const
{
    for (const QString &key : keys) {
        if (key == "detect.mode") {
            settings.detectModeId = idAt(
                detectModeIds(), m_view.comboBox_detectionMode->currentIndex(),
                settings.detectModeId);
        } else if (key == "image.save_mode") {
            settings.imageSaveModeId = idAt(
                imageSaveModeIds(), m_view.comboBox_imageSaveRange->currentIndex(),
                settings.imageSaveModeId);
        } else if (key == "image.save_type") {
            settings.imageSaveTypeId = idAt(
                imageSaveTypeIds(),
                m_view.comboBox_imageSaveContent->currentIndex(),
                settings.imageSaveTypeId);
        } else if (key == "image.save_path") {
            settings.imageSavePath =
                    m_view.lineEdit_imageSavePath->text().trimmed();
        } else if (key == "trigger.enabled") {
            settings.triggerEnabled =
                    m_view.checkBox_hardwareTriggerEnabled->isChecked();
        } else if (key == "camera.exposure") {
            settings.cameraExposure = m_view.spinBox_cameraExposure->value();
        } else if (key == "camera.gain") {
            settings.cameraGain = m_view.lineEdit_cameraGain->text().toInt();
        } else if (key == "image.color_channel") {
            settings.colorChannelId = idAt(
                colorChannelIds(), m_view.comboBox_colorChannel->currentIndex(),
                settings.colorChannelId);
        } else if (key == "image.rotation") {
            settings.imageRotationId = idAt(
                rotationIds(), m_view.comboBox_imageRotation->currentIndex(),
                settings.imageRotationId);
        } else if (key == "plc.trigger_mode") {
            settings.triggerModeId = idAt(
                triggerModeIds(), m_view.comboBox_plcTriggerMode->currentIndex(),
                settings.triggerModeId);
        } else if (key == "plc.photo_distance") {
            settings.photoDistance = m_view.lineEdit_photoDistance->text().toInt();
        } else if (key == "plc.photo_time") {
            settings.photoTime = m_view.lineEdit_photoTime->text().toInt();
        } else if (key == "plc.camera_delay") {
            settings.cameraDelay =
                    m_view.lineEdit_hardwareTriggerDelay->text().toInt();
        } else if (key == "plc.reject_distance") {
            settings.rejectDistance =
                    m_view.lineEdit_rejectDistance->text().toInt();
        } else if (key == "plc.reject_time") {
            settings.rejectTime = m_view.lineEdit_rejectTime->text().toInt();
        } else if (key == "plc.reject_position") {
            settings.rejectPosition =
                    m_view.lineEdit_rejectPosition->text().toInt();
        } else if (key == "plc.ip") {
            settings.plcIp = m_view.lineEdit_plcIpAddress->text().trimmed();
        } else if (key == "plc.rack") {
            settings.plcRack = m_view.lineEdit_plcRack->text().toInt();
        } else if (key == "plc.slot") {
            settings.plcSlot = m_view.lineEdit_plcSlot->text().toInt();
        }
    }
}

void MachineSettingsPage::restoreUnappliedMachineSettings()
{
    restoreAppliedValues(QStringList()
        << "camera.exposure"
        << "camera.gain"
        << "image.color_channel"
        << "image.rotation"
        << "tissue.roughness_threshold"
        << "plc.trigger_mode"
        << "plc.photo_distance"
        << "plc.photo_time"
        << "plc.camera_delay"
        << "plc.reject_distance"
        << "plc.reject_time"
        << "plc.reject_position"
        << "plc.ip"
        << "plc.rack"
        << "plc.slot");
}

void MachineSettingsPage::restoreAppliedValue(const QString &key)
{
    restoreAppliedValues(QStringList() << key);
}

void MachineSettingsPage::restoreAppliedValues(const QStringList &keys)
{
    if (!m_settingsService) {
        return;
    }
    const AppSettings &applied = m_settingsService->current();
    const bool previousUpdating = m_updatingSettingsUi;
    m_updatingSettingsUi = true;
    for (const QString &key : keys) {
        if (key == "detect.mode") {
            QSignalBlocker blocker(m_view.comboBox_detectionMode);
            m_view.comboBox_detectionMode->setCurrentIndex(indexOf(
                detectModeIds(), applied.detectModeId, 1));
        } else if (key == "image.save_mode") {
            QSignalBlocker blocker(m_view.comboBox_imageSaveRange);
            m_view.comboBox_imageSaveRange->setCurrentIndex(indexOf(
                imageSaveModeIds(), applied.imageSaveModeId, 0));
        } else if (key == "image.save_type") {
            QSignalBlocker blocker(m_view.comboBox_imageSaveContent);
            m_view.comboBox_imageSaveContent->setCurrentIndex(indexOf(
                imageSaveTypeIds(), applied.imageSaveTypeId, 1));
        } else if (key == "image.save_path") {
            QSignalBlocker blocker(m_view.lineEdit_imageSavePath);
            m_view.lineEdit_imageSavePath->setText(applied.imageSavePath);
        } else if (key == "trigger.enabled") {
            QSignalBlocker blocker(m_view.checkBox_hardwareTriggerEnabled);
            m_view.checkBox_hardwareTriggerEnabled->setChecked(
                        applied.triggerEnabled);
        } else if (key == "camera.exposure") {
            QSignalBlocker blocker(m_view.spinBox_cameraExposure);
            m_view.spinBox_cameraExposure->setValue(applied.cameraExposure);
        } else if (key == "camera.gain") {
            QSignalBlocker blocker(m_view.lineEdit_cameraGain);
            m_view.lineEdit_cameraGain->setText(QString::number(
                static_cast<int>(applied.cameraGain)));
        } else if (key == "image.color_channel") {
            QSignalBlocker blocker(m_view.comboBox_colorChannel);
            m_view.comboBox_colorChannel->setCurrentIndex(indexOf(
                colorChannelIds(), applied.colorChannelId, 0));
        } else if (key == "image.rotation") {
            QSignalBlocker blocker(m_view.comboBox_imageRotation);
            m_view.comboBox_imageRotation->setCurrentIndex(indexOf(
                rotationIds(), applied.imageRotationId, 0));
        } else if (key == "tissue.roughness_threshold") {
            QSignalBlocker blocker(
                        m_view.lineEdit_tissueRoughnessThreshold);
            m_view.lineEdit_tissueRoughnessThreshold->setText(
                        QString::number(
                            applied.detectionSchemes
                            .tissueRoughnessThreshold,
                            'f', 3));
        } else if (key == "plc.trigger_mode") {
            QSignalBlocker blocker(m_view.comboBox_plcTriggerMode);
            m_view.comboBox_plcTriggerMode->setCurrentIndex(indexOf(
                triggerModeIds(), applied.triggerModeId, 1));
        } else if (key == "plc.photo_distance") {
            QSignalBlocker blocker(m_view.lineEdit_photoDistance);
            m_view.lineEdit_photoDistance->setText(
                        QString::number(applied.photoDistance));
        } else if (key == "plc.photo_time") {
            QSignalBlocker blocker(m_view.lineEdit_photoTime);
            m_view.lineEdit_photoTime->setText(
                        QString::number(applied.photoTime));
        } else if (key == "plc.camera_delay") {
            QSignalBlocker blocker(m_view.lineEdit_hardwareTriggerDelay);
            m_view.lineEdit_hardwareTriggerDelay->setText(
                        QString::number(applied.cameraDelay));
        } else if (key == "plc.reject_distance") {
            QSignalBlocker blocker(m_view.lineEdit_rejectDistance);
            m_view.lineEdit_rejectDistance->setText(
                        QString::number(applied.rejectDistance));
        } else if (key == "plc.reject_time") {
            QSignalBlocker blocker(m_view.lineEdit_rejectTime);
            m_view.lineEdit_rejectTime->setText(
                        QString::number(applied.rejectTime));
        } else if (key == "plc.reject_position") {
            QSignalBlocker blocker(m_view.lineEdit_rejectPosition);
            m_view.lineEdit_rejectPosition->setText(
                        QString::number(applied.rejectPosition));
        } else if (key == "plc.ip") {
            QSignalBlocker blocker(m_view.lineEdit_plcIpAddress);
            m_view.lineEdit_plcIpAddress->setText(applied.plcIp);
        } else if (key == "plc.rack") {
            QSignalBlocker blocker(m_view.lineEdit_plcRack);
            m_view.lineEdit_plcRack->setText(QString::number(applied.plcRack));
        } else if (key == "plc.slot") {
            QSignalBlocker blocker(m_view.lineEdit_plcSlot);
            m_view.lineEdit_plcSlot->setText(QString::number(applied.plcSlot));
        }
    }
    m_updatingSettingsUi = previousUpdating;
    refreshDirty(keys);
    if (keys.contains(QStringLiteral("image.save_mode"))
            && m_callbacks.updateImageSaveOptionsVisibility) {
        m_callbacks.updateImageSaveOptionsVisibility();
    }
    if (keys.contains(QStringLiteral("image.save_path"))
            && m_callbacks.updateSaveDirectoryText) {
        m_callbacks.updateSaveDirectoryText();
    }
}

void MachineSettingsPage::restoreCameraUiFromApplied()
{
    restoreAppliedValues(
                QStringList() << "camera.exposure" << "camera.gain");
}

void MachineSettingsPage::restorePlcUiFromApplied()
{
    restoreAppliedValues(QStringList()
        << "plc.trigger_mode"
        << "plc.photo_distance"
        << "plc.photo_time"
        << "plc.camera_delay"
        << "plc.reject_distance"
        << "plc.reject_time"
        << "plc.reject_position");
}

void MachineSettingsPage::applyTissueRoughnessThreshold()
{
    double value = 0.0;
    if (!parseDouble(
            m_view.lineEdit_tissueRoughnessThreshold->text(), &value)
            || value < 0.0) {
        restoreAppliedValue("tissue.roughness_threshold");
        QMessageBox::warning(
                    m_view.lineEdit_tissueRoughnessThreshold,
                    QStringLiteral("参数错误"),
                    QStringLiteral("纸巾粗糙度阈值必须是非负数。"));
        return;
    }
    const OperationResult saved =
            m_settingsService->saveTissueThreshold(value);
    if (!saved.isSuccess()) {
        restoreAppliedValue("tissue.roughness_threshold");
        QMessageBox::critical(
                    m_view.lineEdit_tissueRoughnessThreshold,
                    QStringLiteral("纸巾阈值保存失败"),
                    saved.error.userMessage);
        return;
    }
    refreshDirty("tissue.roughness_threshold");
    QMessageBox::information(
                m_view.lineEdit_tissueRoughnessThreshold,
                QStringLiteral("成功"),
                QStringLiteral("纸巾检测阈值已保存。"));
}

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
        applyOperationUiAccess(it.value().editor, access);
        applyOperationUiAccess(it.value().label, access, false);
    }
    for (const HardwareActionBinding &binding : m_hardwareActions) {
        const OperationUiSnapshot::Access access =
                accessFor(binding.hardwareDependency);
        applyOperationUiAccess(binding.control, access);
    }
    applyOperationUiAccess(
                m_view.pushButton_applyTissueRoughnessThreshold,
                snapshot.generalSettings);
}

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
