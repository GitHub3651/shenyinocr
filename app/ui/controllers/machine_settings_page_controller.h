#pragma once

#include "system_support/settings/machine_settings_store.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <functional>

class QLabel;
class QLineEdit;
class QEvent;
class QWidget;
class SettingsEditState;

namespace Ui {
class Widget;
}

class MachineSettingsPageController : public QObject
{
public:
    struct Callbacks
    {
        std::function<bool(bool)> saveSettings;
        std::function<void(MachineSettings *)> syncRecipeHistory;
        std::function<void()> updateImageSaveOptionsVisibility;
        std::function<void()> updateSaveDirectoryText;
        std::function<void()> updateTissueVisibility;
        std::function<void()> updateOperationUiState;
        std::function<void(const QString &)> reportDirectoryOpenFailure;
    };

    MachineSettingsPageController(
        Ui::Widget *ui,
        MachineSettings *appliedSettings,
        MachineSettingsStore *settingsStore,
        SettingsEditState *editState,
        QString *selectedDirectory,
        bool *applyingSettings,
        bool *updatingSettingsUi,
        const Callbacks &callbacks,
        QObject *parent = nullptr);

    void setupBindings();
    void setupNumericInputValidators();
    void installWheelProtection(QWidget *rootWidget);
    void setSoftwareDataDirectoryEditor(QLineEdit *editor);

    void initialize(const MachineSettings &settings);
    bool save(bool showErrorMessage, QString *errorMessage);
    bool clear(QString *errorMessage);
    MachineSettings defaultsForHardwareState(
        bool cameraOpen,
        bool plcConnected) const;
    void applyToUi(const MachineSettings &settings);

    void updateAppliedFromUi(const QString &key);
    void updateAppliedFromUi(const QStringList &keys);
    void refreshDirty(const QString &key);
    void refreshDirty(const QStringList &keys);
    void refreshAllDirty();
    void markDirty(const QString &key);
    void clearDirty(const QString &key);
    void clearDirty(const QStringList &keys);
    void clearAllDirty();
    void restoreUnappliedMachineSettings();

    void updateHardwareEnabled(
        bool cameraOpen,
        bool plcConnected,
        bool operationBusy);
    void setAllEditorsEnabled(bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class HardwareDependency
    {
        None,
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
    bool isDirtyByValue(const QString &key) const;
    void updateDirtyLabel(const QString &key);
    void syncImmediateSettings();
    void restoreCameraUiFromApplied();
    void restorePlcUiFromApplied();
    QString disabledStyle(QWidget *widget) const;
    void setHardwareControlEnabled(
        QWidget *widget,
        bool enabled,
        const QString &disabledReason,
        bool showDisabledReason);

    Ui::Widget *m_ui = nullptr;
    MachineSettings *m_appliedSettings = nullptr;
    MachineSettingsStore *m_settingsStore = nullptr;
    SettingsEditState *m_editState = nullptr;
    QString *m_selectedDirectory = nullptr;
    bool *m_applyingSettings = nullptr;
    bool *m_updatingSettingsUi = nullptr;
    Callbacks m_callbacks;
    QMap<QString, GlobalSettingBinding> m_bindings;
    QList<HardwareActionBinding> m_hardwareActions;
    QLineEdit *m_softwareDataDirectoryEditor = nullptr;
    bool m_loaded = false;
};
