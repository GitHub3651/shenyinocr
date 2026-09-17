#pragma once

#include "application/settings_application_service.h"
#include "ui/main_window/operation_ui_policy.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>

class QLabel;
class QEvent;
class QCheckBox;
class QWidget;
class SettingsEditState;

namespace Ui {
class DetectionSettingsPage;
class ImageSettingsPage;
class PlcSettingsPage;
class SoftwareSettingsPage;
}

class MachineSettingsPage : public QObject
{
public:
    MachineSettingsPage(
        Ui::DetectionSettingsPage &detectionSettingsUi,
        Ui::ImageSettingsPage &imageSettingsUi,
        Ui::PlcSettingsPage &plcSettingsUi,
        Ui::SoftwareSettingsPage &softwareSettingsUi,
        QCheckBox &hardwareTriggerEnabled,
        SettingsApplicationService &settingsService,
        SettingsEditState &editState);

    void setupBindings();
    void setupNumericInputValidators();
    void installWheelProtection(QWidget &rootWidget);

    void initialize(const AppSettings &settings);
    void applyToUi(const AppSettings &settings);

    void copyUiValuesTo(AppSettings &settings,
                        const QStringList &keys) const;
    QString settingValueText(const QString &key) const;
    void refreshDirty(const QString &key);
    void refreshDirty(const QStringList &keys);
    void refreshAllDirty();
    void clearDirty(const QString &key);
    void clearDirty(const QStringList &keys);
    void clearAllDirty();
    void restoreAppliedValue(const QString &key);
    void restoreAppliedValues(const QStringList &keys);
    void restoreUnappliedMachineSettings();

    void applyOperationState(const OperationUiSnapshot &snapshot);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class HardwareDependency
    {
        None,
        ImageSettings,
        Camera,
        PlcConnection,
        PlcRuntime
    };

    struct GlobalSettingBinding
    {
        QString key;
        QWidget *editor = nullptr;
        QLabel *label = nullptr;
        QString originalLabelText;
        bool requireApply = false;
        HardwareDependency hardwareDependency = HardwareDependency::None;
    };

    struct HardwareActionBinding
    {
        QWidget *control = nullptr;
        HardwareDependency hardwareDependency = HardwareDependency::None;
    };

    void registerGlobalSetting(
        const QString &key,
        QWidget *editor,
        QLabel *label,
        bool requireApply,
        HardwareDependency hardwareDependency = HardwareDependency::None);
    void registerHardwareAction(
        QWidget *control,
        HardwareDependency hardwareDependency);
    void applyTissueRoughnessThreshold();
    void updateImageSaveOptionsVisibility();
    void updateSaveDirectoryText();
    bool isDirtyByValue(const QString &key) const;
    void updateDirtyLabel(const QString &key);
    void restoreCameraUiFromApplied();
    void restorePlcUiFromApplied();

    Ui::DetectionSettingsPage &m_detectionSettingsUi;
    Ui::ImageSettingsPage &m_imageSettingsUi;
    Ui::PlcSettingsPage &m_plcSettingsUi;
    Ui::SoftwareSettingsPage &m_softwareSettingsUi;
    QCheckBox &m_hardwareTriggerEnabled;
    SettingsApplicationService &m_settingsService;
    SettingsEditState &m_editState;
    bool m_applyingSettings = false;
    bool m_updatingSettingsUi = false;
    QMap<QString, GlobalSettingBinding> m_bindings;
    QList<HardwareActionBinding> m_hardwareActions;
    bool m_loaded = false;
};
