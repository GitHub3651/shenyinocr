// 文件作用：本文件用于绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 主要职责：绑定机器设置页面，管理控件映射、校验、脏状态和运行中禁用规则。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/settings_application_service.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <functional>

class QLabel;
class QLineEdit;
class QEvent;
class QWidget;
// 组件说明：SettingsEditState 组件封装本文件中与其名称对应的单一职责。
class SettingsEditState;

namespace Ui {
// 组件说明：MainWindow 组件负责对应界面区域的显示和用户交互。
class MainWindow;
}

// 组件说明：MachineSettingsPage 组件负责对应界面区域的显示和用户交互。
class MachineSettingsPage : public QObject
{
public:
    // 组件说明：Callbacks 数据结构集中传递该流程需要的只读数据或回调。
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

    MachineSettingsPage(
        Ui::MainWindow *ui,
        SettingsApplicationService *settingsService,
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
    // 组件说明：HardwareDependency 枚举列出该组件允许使用的稳定状态和选项。
    enum class HardwareDependency
    {
        None,
        Camera,
        PlcConnection,
        PlcRuntime
    };

    // 组件说明：GlobalSettingBinding 数据结构集中保存该流程需要的一组相关数据。
    struct GlobalSettingBinding
    {
        QString key;
        QWidget *editor = nullptr;
        QLabel *label = nullptr;
        QString originalLabelText;
        bool requireApply = false;
        HardwareDependency hardwareDependency = HardwareDependency::None;
    };

    // 组件说明：HardwareActionBinding 数据结构集中保存该流程需要的一组相关数据。
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

    Ui::MainWindow *m_ui = nullptr;
    MachineSettings *m_appliedSettings = nullptr;
    SettingsApplicationService *m_settingsService = nullptr;
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
