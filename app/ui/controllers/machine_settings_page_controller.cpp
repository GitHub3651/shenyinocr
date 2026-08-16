#include "ui/controllers/machine_settings_page_controller.h"

#include "system_support/machine_settings_policy.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui_widget.h"

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
#include <QSplitterHandle>
#include <QUrl>
#include <QWidget>

#include <cmath>

namespace {

const QStringList &detectModeIds()
{
    static const QStringList ids = machineSettingsDetectionModeIds();
    return ids;
}

const QStringList &imageSaveModeIds()
{
    static const QStringList ids = machineSettingsImageSaveModeIds();
    return ids;
}

const QStringList &imageSaveTypeIds()
{
    static const QStringList ids = machineSettingsImageSaveTypeIds();
    return ids;
}

const QStringList &colorChannelIds()
{
    static const QStringList ids = machineSettingsColorChannelIds();
    return ids;
}

const QStringList &rotationIds()
{
    static const QStringList ids = machineSettingsRotationIds();
    return ids;
}

const QStringList &triggerModeIds()
{
    static const QStringList ids = machineSettingsTriggerModeIds();
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

QString text(const wchar_t *value)
{
    return QString::fromWCharArray(value);
}

} // namespace

MachineSettingsPageController::MachineSettingsPageController(
    Ui::Widget *ui,
    SettingsApplicationService *settingsService,
    SettingsEditState *editState,
    QString *selectedDirectory,
    bool *applyingSettings,
    bool *updatingSettingsUi,
    const Callbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_ui(ui),
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

void MachineSettingsPageController::setupBindings()
{
    if (!m_ui || !m_editState) {
        return;
    }

    m_bindings.clear();
    m_hardwareActions.clear();

    registerGlobalSetting("camera.exposure", m_ui->spinBox,
                          m_ui->label_19, true,
                          HardwareDependency::Camera);
    registerGlobalSetting("camera.gain", m_ui->lineEdit_14,
                          m_ui->label_16, true,
                          HardwareDependency::Camera);
    registerGlobalSetting("image.color_channel", m_ui->comboBox_5,
                          m_ui->label_12, true);
    registerGlobalSetting("image.rotation", m_ui->comboBox_2,
                          m_ui->label_27, true);
    registerGlobalSetting("plc.trigger_mode", m_ui->comboBox_3,
                          m_ui->label_15, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_distance", m_ui->lineEdit_6,
                          m_ui->label_8, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_time", m_ui->lineEdit_20,
                          m_ui->label_14, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.camera_delay", m_ui->lineEdit_4,
                          m_ui->label_13, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_distance", m_ui->lineEdit_7,
                          m_ui->label_6, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_time", m_ui->lineEdit_8,
                          m_ui->label_10, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_position", m_ui->lineEdit_12,
                          m_ui->label_17, true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.ip", m_ui->lineEdit,
                          m_ui->label_2, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.rack", m_ui->lineEdit_2,
                          m_ui->label_3, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.slot", m_ui->lineEdit_3,
                          m_ui->label_3, false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("detect.mode", m_ui->comboBox_4,
                          m_ui->label_18, false);
    registerGlobalSetting("image.save_mode", m_ui->comboBox,
                          m_ui->label_11, false);
    registerGlobalSetting("image.save_type", m_ui->comboBox_saveImageType,
                          m_ui->label_saveImageType, false);
    registerGlobalSetting("image.save_path", m_ui->lineEdit_imageSavePath,
                          m_ui->label_imageSavePath, false);
    registerGlobalSetting("trigger.enabled", m_ui->checkBox,
                          nullptr, false);

    registerHardwareAction(m_ui->sureButton, HardwareDependency::Camera);
    registerHardwareAction(m_ui->pushButton_12, HardwareDependency::Camera);
    registerHardwareAction(m_ui->ConnectpushButton,
                           HardwareDependency::PlcConnection);
    registerHardwareAction(m_ui->DisconnectpushButton,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_ui->plcmodebtn,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_ui->WriteVDpushButton,
                           HardwareDependency::PlcRuntime);
    registerHardwareAction(m_ui->pushButton_8,
                           HardwareDependency::PlcRuntime);

    QObject::connect(
        m_ui->comboBox,
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

void MachineSettingsPageController::setupNumericInputValidators()
{
    if (!m_ui) {
        return;
    }
    auto setIntValidator = [](QLineEdit *lineEdit) {
        if (lineEdit) {
            lineEdit->setValidator(
                new QIntValidator(0, 2147483647, lineEdit));
        }
    };
    setIntValidator(m_ui->lineEdit_14);
    setIntValidator(m_ui->lineEdit_2);
    setIntValidator(m_ui->lineEdit_3);
    setIntValidator(m_ui->lineEdit_6);
    setIntValidator(m_ui->lineEdit_4);
    setIntValidator(m_ui->lineEdit_7);
    setIntValidator(m_ui->lineEdit_12);
    m_ui->lineEdit_20->setValidator(
        new QIntValidator(0, 65535, m_ui->lineEdit_20));
    m_ui->lineEdit_8->setValidator(
        new QIntValidator(0, 65535, m_ui->lineEdit_8));

    if (m_ui->lineEdit_yuzhi) {
        m_ui->lineEdit_yuzhi->setValidator(
            new QIntValidator(0, 100, m_ui->lineEdit_yuzhi));
        m_ui->lineEdit_yuzhi->setMaxLength(3);
        m_ui->lineEdit_yuzhi->setToolTip(
            text(L"\u8bf7\u8f930\u5230100\u4e4b\u95f4\u7684\u6574\u6570\uff0c\u5355\u4f4d\uff1a%"));
    }
    if (m_ui->lineEdit_tissueRoughnessThreshold) {
        QDoubleValidator *validator = new QDoubleValidator(
            0.001, 1000000.0, 3,
            m_ui->lineEdit_tissueRoughnessThreshold);
        validator->setNotation(QDoubleValidator::StandardNotation);
        m_ui->lineEdit_tissueRoughnessThreshold->setValidator(validator);
    }
}

void MachineSettingsPageController::installWheelProtection(
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

void MachineSettingsPageController::setSoftwareDataDirectoryEditor(
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

void MachineSettingsPageController::initialize(
    const MachineSettings &settings)
{
    if (!m_appliedSettings) {
        return;
    }
    *m_appliedSettings = settings;
    m_loaded = true;
    applyToUi(settings);
}

bool MachineSettingsPageController::save(
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

bool MachineSettingsPageController::clear(QString *errorMessage)
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

MachineSettings MachineSettingsPageController::defaultsForHardwareState(
    bool cameraOpen,
    bool plcConnected) const
{
    const MachineSettings defaults = MachineSettings::defaults();
    return MachineSettingsPolicy::defaultsForHardwareState(
        m_appliedSettings ? *m_appliedSettings : defaults,
        defaults,
        cameraOpen,
        plcConnected);
}

void MachineSettingsPageController::applyToUi(
    const MachineSettings &settings)
{
    if (!m_ui) {
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

    m_ui->comboBox_4->setCurrentIndex(
        indexOf(detectModeIds(), settings.detectModeId, 1));
    m_ui->comboBox->setCurrentIndex(
        indexOf(imageSaveModeIds(), settings.imageSaveModeId, 0));
    m_ui->comboBox_saveImageType->setCurrentIndex(
        indexOf(imageSaveTypeIds(), settings.imageSaveTypeId, 1));
    m_ui->comboBox_5->setCurrentIndex(
        indexOf(colorChannelIds(), settings.colorChannelId, 0));
    m_ui->comboBox_2->setCurrentIndex(
        indexOf(rotationIds(), settings.imageRotationId, 0));
    m_ui->comboBox_3->setCurrentIndex(
        indexOf(triggerModeIds(), settings.triggerModeId, 1));
    m_ui->checkBox->setChecked(settings.triggerEnabled);
    m_ui->spinBox->setValue(settings.cameraExposure);
    m_ui->lineEdit_14->setText(
        QString::number(static_cast<int>(settings.cameraGain)));
    m_ui->lineEdit->setText(settings.plcIp);
    m_ui->lineEdit_2->setText(QString::number(settings.plcRack));
    m_ui->lineEdit_3->setText(QString::number(settings.plcSlot));
    m_ui->lineEdit_6->setText(QString::number(settings.photoDistance));
    m_ui->lineEdit_20->setText(QString::number(settings.photoTime));
    m_ui->lineEdit_4->setText(QString::number(settings.cameraDelay));
    m_ui->lineEdit_7->setText(QString::number(settings.rejectDistance));
    m_ui->lineEdit_8->setText(QString::number(settings.rejectTime));
    m_ui->lineEdit_12->setText(QString::number(settings.rejectPosition));

    if (m_ui->rightPanelSplitter
            && !settings.rightPanelSplitterState.isEmpty()
            && !m_ui->rightPanelSplitter->restoreState(
                settings.rightPanelSplitterState)) {
        qWarning("[UI_SETTINGS] invalid right panel splitter state");
    }
    if (m_ui->rightPanelSplitter) {
        const int handleHeight = m_ui->rightPanelSplitter
                ->property("visualHandleHeight").toInt();
        if (handleHeight > 0) {
            m_ui->rightPanelSplitter->setHandleWidth(handleHeight);
            if (QSplitterHandle *handle =
                    m_ui->rightPanelSplitter->handle(1)) {
                handle->setMinimumHeight(handleHeight);
                handle->setMaximumHeight(handleHeight);
            }
        }
        m_ui->rightPanelSplitter->setChildrenCollapsible(true);
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

void MachineSettingsPageController::registerGlobalSetting(
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

void MachineSettingsPageController::registerHardwareAction(
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

bool MachineSettingsPageController::isDirtyByValue(
    const QString &key) const
{
    if (!m_ui || !m_appliedSettings) {
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
        return m_ui->spinBox->value()
                != m_appliedSettings->cameraExposure;
    }
    if (key == "camera.gain") {
        return intDirty(m_ui->lineEdit_14,
                        static_cast<int>(m_appliedSettings->cameraGain));
    }
    if (key == "image.color_channel") {
        return comboDirty(m_ui->comboBox_5, colorChannelIds(),
                          m_appliedSettings->colorChannelId);
    }
    if (key == "image.rotation") {
        return comboDirty(m_ui->comboBox_2, rotationIds(),
                          m_appliedSettings->imageRotationId);
    }
    if (key == "plc.trigger_mode") {
        return comboDirty(m_ui->comboBox_3, triggerModeIds(),
                          m_appliedSettings->triggerModeId);
    }
    if (key == "plc.photo_distance") {
        return intDirty(m_ui->lineEdit_6,
                        m_appliedSettings->photoDistance);
    }
    if (key == "plc.photo_time") {
        return intDirty(m_ui->lineEdit_20,
                        m_appliedSettings->photoTime);
    }
    if (key == "plc.camera_delay") {
        return intDirty(m_ui->lineEdit_4,
                        m_appliedSettings->cameraDelay);
    }
    if (key == "plc.reject_distance") {
        return intDirty(m_ui->lineEdit_7,
                        m_appliedSettings->rejectDistance);
    }
    if (key == "plc.reject_time") {
        return intDirty(m_ui->lineEdit_8,
                        m_appliedSettings->rejectTime);
    }
    if (key == "plc.reject_position") {
        return intDirty(m_ui->lineEdit_12,
                        m_appliedSettings->rejectPosition);
    }
    if (key == "plc.ip") {
        return m_ui->lineEdit->text().trimmed()
                != m_appliedSettings->plcIp.trimmed();
    }
    if (key == "plc.rack") {
        return intDirty(m_ui->lineEdit_2, m_appliedSettings->plcRack);
    }
    if (key == "plc.slot") {
        return intDirty(m_ui->lineEdit_3, m_appliedSettings->plcSlot);
    }
    return false;
}

void MachineSettingsPageController::refreshDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, isDirtyByValue(key));
    updateDirtyLabel(key);
}

void MachineSettingsPageController::refreshDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        refreshDirty(key);
    }
}

void MachineSettingsPageController::refreshAllDirty()
{
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        refreshDirty(it.key());
    }
}

void MachineSettingsPageController::markDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, true);
    updateDirtyLabel(key);
}

void MachineSettingsPageController::clearDirty(const QString &key)
{
    if (!m_editState || !m_bindings.contains(key)) {
        return;
    }
    m_editState->setGlobalDirty(key, false);
    updateDirtyLabel(key);
}

void MachineSettingsPageController::clearDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        clearDirty(key);
    }
}

void MachineSettingsPageController::clearAllDirty()
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

void MachineSettingsPageController::updateDirtyLabel(const QString &key)
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

void MachineSettingsPageController::updateAppliedFromUi(
    const QString &key)
{
    if (!m_ui || !m_appliedSettings) {
        return;
    }
    if (key == "detect.mode") {
        m_appliedSettings->detectModeId = idAt(
            detectModeIds(), m_ui->comboBox_4->currentIndex(),
            m_appliedSettings->detectModeId);
    } else if (key == "image.save_mode") {
        m_appliedSettings->imageSaveModeId = idAt(
            imageSaveModeIds(), m_ui->comboBox->currentIndex(),
            m_appliedSettings->imageSaveModeId);
    } else if (key == "image.save_type") {
        m_appliedSettings->imageSaveTypeId = idAt(
            imageSaveTypeIds(),
            m_ui->comboBox_saveImageType->currentIndex(),
            m_appliedSettings->imageSaveTypeId);
    } else if (key == "image.save_path" && m_selectedDirectory) {
        m_appliedSettings->imageSavePath = *m_selectedDirectory;
    } else if (key == "trigger.enabled") {
        m_appliedSettings->triggerEnabled = m_ui->checkBox->isChecked();
    } else if (key == "recipe.history") {
        if (m_callbacks.syncRecipeHistory) {
            m_callbacks.syncRecipeHistory(m_appliedSettings);
        }
    } else if (key == "camera.exposure") {
        m_appliedSettings->cameraExposure = m_ui->spinBox->value();
    } else if (key == "camera.gain") {
        m_appliedSettings->cameraGain = m_ui->lineEdit_14->text().toInt();
    } else if (key == "image.color_channel") {
        m_appliedSettings->colorChannelId = idAt(
            colorChannelIds(), m_ui->comboBox_5->currentIndex(),
            m_appliedSettings->colorChannelId);
    } else if (key == "image.rotation") {
        m_appliedSettings->imageRotationId = idAt(
            rotationIds(), m_ui->comboBox_2->currentIndex(),
            m_appliedSettings->imageRotationId);
    } else if (key == "plc.trigger_mode") {
        m_appliedSettings->triggerModeId = idAt(
            triggerModeIds(), m_ui->comboBox_3->currentIndex(),
            m_appliedSettings->triggerModeId);
    } else if (key == "plc.photo_distance") {
        m_appliedSettings->photoDistance = m_ui->lineEdit_6->text().toInt();
    } else if (key == "plc.photo_time") {
        m_appliedSettings->photoTime = m_ui->lineEdit_20->text().toInt();
    } else if (key == "plc.camera_delay") {
        m_appliedSettings->cameraDelay = m_ui->lineEdit_4->text().toInt();
    } else if (key == "plc.reject_distance") {
        m_appliedSettings->rejectDistance = m_ui->lineEdit_7->text().toInt();
    } else if (key == "plc.reject_time") {
        m_appliedSettings->rejectTime = m_ui->lineEdit_8->text().toInt();
    } else if (key == "plc.reject_position") {
        m_appliedSettings->rejectPosition = m_ui->lineEdit_12->text().toInt();
    } else if (key == "plc.ip") {
        m_appliedSettings->plcIp = m_ui->lineEdit->text().trimmed();
    } else if (key == "plc.rack") {
        m_appliedSettings->plcRack = m_ui->lineEdit_2->text().toInt();
    } else if (key == "plc.slot") {
        m_appliedSettings->plcSlot = m_ui->lineEdit_3->text().toInt();
    }
}

void MachineSettingsPageController::updateAppliedFromUi(
    const QStringList &keys)
{
    for (const QString &key : keys) {
        updateAppliedFromUi(key);
    }
}

void MachineSettingsPageController::syncImmediateSettings()
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
        << "recipe.history");
    if (m_ui && m_ui->rightPanelSplitter && m_appliedSettings) {
        m_appliedSettings->rightPanelSplitterState =
            m_ui->rightPanelSplitter->saveState();
    }
}

void MachineSettingsPageController::restoreUnappliedMachineSettings()
{
    if (!m_ui || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker exposure(m_ui->spinBox);
    QSignalBlocker gain(m_ui->lineEdit_14);
    QSignalBlocker channel(m_ui->comboBox_5);
    QSignalBlocker rotation(m_ui->comboBox_2);
    QSignalBlocker trigger(m_ui->comboBox_3);
    QSignalBlocker photoDistance(m_ui->lineEdit_6);
    QSignalBlocker photoTime(m_ui->lineEdit_20);
    QSignalBlocker cameraDelay(m_ui->lineEdit_4);
    QSignalBlocker rejectDistance(m_ui->lineEdit_7);
    QSignalBlocker rejectTime(m_ui->lineEdit_8);
    QSignalBlocker rejectPosition(m_ui->lineEdit_12);

    m_ui->spinBox->setValue(m_appliedSettings->cameraExposure);
    m_ui->lineEdit_14->setText(QString::number(
        static_cast<int>(m_appliedSettings->cameraGain)));
    m_ui->comboBox_5->setCurrentIndex(indexOf(
        colorChannelIds(), m_appliedSettings->colorChannelId, 0));
    m_ui->comboBox_2->setCurrentIndex(indexOf(
        rotationIds(), m_appliedSettings->imageRotationId, 0));
    m_ui->comboBox_3->setCurrentIndex(indexOf(
        triggerModeIds(), m_appliedSettings->triggerModeId, 1));
    m_ui->lineEdit_6->setText(
        QString::number(m_appliedSettings->photoDistance));
    m_ui->lineEdit_20->setText(
        QString::number(m_appliedSettings->photoTime));
    m_ui->lineEdit_4->setText(
        QString::number(m_appliedSettings->cameraDelay));
    m_ui->lineEdit_7->setText(
        QString::number(m_appliedSettings->rejectDistance));
    m_ui->lineEdit_8->setText(
        QString::number(m_appliedSettings->rejectTime));
    m_ui->lineEdit_12->setText(
        QString::number(m_appliedSettings->rejectPosition));
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
    refreshAllDirty();
}

void MachineSettingsPageController::restoreCameraUiFromApplied()
{
    if (!m_ui || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker exposure(m_ui->spinBox);
    QSignalBlocker gain(m_ui->lineEdit_14);
    m_ui->spinBox->setValue(m_appliedSettings->cameraExposure);
    m_ui->lineEdit_14->setText(QString::number(
        static_cast<int>(m_appliedSettings->cameraGain)));
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = previousUpdating;
    }
    refreshDirty(QStringList() << "camera.exposure" << "camera.gain");
}

void MachineSettingsPageController::restorePlcUiFromApplied()
{
    if (!m_ui || !m_appliedSettings) {
        return;
    }
    const bool previousUpdating =
        m_updatingSettingsUi ? *m_updatingSettingsUi : false;
    if (m_updatingSettingsUi) {
        *m_updatingSettingsUi = true;
    }
    QSignalBlocker trigger(m_ui->comboBox_3);
    QSignalBlocker photoDistance(m_ui->lineEdit_6);
    QSignalBlocker photoTime(m_ui->lineEdit_20);
    QSignalBlocker cameraDelay(m_ui->lineEdit_4);
    QSignalBlocker rejectDistance(m_ui->lineEdit_7);
    QSignalBlocker rejectTime(m_ui->lineEdit_8);
    QSignalBlocker rejectPosition(m_ui->lineEdit_12);
    m_ui->comboBox_3->setCurrentIndex(
        m_appliedSettings->triggerModeId == "trigger_continuous"
            ? 0 : 1);
    m_ui->lineEdit_6->setText(
        QString::number(m_appliedSettings->photoDistance));
    m_ui->lineEdit_20->setText(
        QString::number(m_appliedSettings->photoTime));
    m_ui->lineEdit_4->setText(
        QString::number(m_appliedSettings->cameraDelay));
    m_ui->lineEdit_7->setText(
        QString::number(m_appliedSettings->rejectDistance));
    m_ui->lineEdit_8->setText(
        QString::number(m_appliedSettings->rejectTime));
    m_ui->lineEdit_12->setText(
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

QString MachineSettingsPageController::disabledStyle(
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

void MachineSettingsPageController::setHardwareControlEnabled(
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

void MachineSettingsPageController::updateHardwareEnabled(
    bool cameraOpen,
    bool plcConnected,
    bool operationBusy)
{
    if (!cameraOpen) {
        restoreCameraUiFromApplied();
    }
    if (!plcConnected) {
        restorePlcUiFromApplied();
    }
    const QString cameraReason = text(
        L"\u8bf7\u5148\u6253\u5f00\u76f8\u673a\u540e\u518d\u8bbe\u7f6e\u8be5\u53c2\u6570\u3002");
    const QString plcRunReason = text(
        L"\u8bf7\u5148\u8fde\u63a5 PLC \u540e\u518d\u8bbe\u7f6e\u8be5\u53c2\u6570\u3002");
    const QString plcConnectionReason = text(
        L"PLC \u5df2\u8fde\u63a5\u3002\u5982\u9700\u4fee\u6539\u8fde\u63a5\u53c2\u6570\uff0c\u8bf7\u5148\u65ad\u5f00 PLC\u3002");
    auto state = [&](HardwareDependency dependency,
                     bool *enabled,
                     QString *reason) {
        switch (dependency) {
        case HardwareDependency::Camera:
            *enabled = cameraOpen;
            *reason = cameraReason;
            break;
        case HardwareDependency::PlcConnection:
            *enabled = !plcConnected;
            *reason = plcConnectionReason;
            break;
        case HardwareDependency::PlcRuntime:
            *enabled = plcConnected;
            *reason = plcRunReason;
            break;
        case HardwareDependency::None:
            *enabled = true;
            reason->clear();
            break;
        }
    };
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        if (it.value().hardwareDependency == HardwareDependency::None) {
            continue;
        }
        bool enabled = true;
        QString reason;
        state(it.value().hardwareDependency, &enabled, &reason);
        setHardwareControlEnabled(it.value().editor, enabled, reason, true);
        setHardwareControlEnabled(it.value().label, enabled, reason, false);
    }
    for (const HardwareActionBinding &binding : m_hardwareActions) {
        bool enabled = true;
        QString reason;
        state(binding.hardwareDependency, &enabled, &reason);
        setHardwareControlEnabled(binding.control, enabled, reason, true);
    }
    if (operationBusy && m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
}

void MachineSettingsPageController::setAllEditorsEnabled(bool enabled)
{
    for (auto it = m_bindings.constBegin();
         it != m_bindings.constEnd(); ++it) {
        if (it.value().editor) {
            it.value().editor->setEnabled(enabled);
        }
    }
}

bool MachineSettingsPageController::eventFilter(
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
