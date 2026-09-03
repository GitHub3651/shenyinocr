// 文件作用：构造主窗口、连接页面和应用服务，并维护顶层界面生命周期。
#pragma once

#ifndef GLOG_NO_ABBREVIATED_SEVERITIES
#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#endif

#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "runtime/inspection_runtime.h"
#include "templates/template_store.h"
#include "ui/main_window/operation_ui_policy.h"
#include "ui/main_window/settings/settings_edit_state.h"

#include <QStringList>
#include <QWidget>

#include <opencv2/core.hpp>

#include <memory>

namespace Ui {
class MainWindow;
class InspectionInfoPage;
class DetectionSettingsPage;
class ImageSettingsPage;
class PlcSettingsPage;
class SoftwareSettingsPage;
}

class InspectionPage;
class MachineSettingsPage;
class TemplateEditorPage;
class QCloseEvent;
class QTimer;
class QToolButton;
class TemplateApplicationService;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(
        const std::shared_ptr<InspectionApplicationService> &inspectionService,
        InspectionRuntime *runtime,
        const std::shared_ptr<SettingsApplicationService> &settingsService,
        const std::shared_ptr<TemplateApplicationService> &templateService,
        QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_toolButton_openCamera_clicked();
    void on_toolButton_closeCamera_clicked();
    void on_pushButton_applyCameraExposure_clicked();
    void on_toolButton_startInspection_clicked();
    void on_pushButton_connectPlc_clicked();
    void on_pushButton_disconnectPlc_clicked();
    void on_pushButton_applyPhotoDistance_clicked();
    void on_toolButton_stopInspection_clicked();
    void on_pushButton_applyPlcTriggerMode_clicked();
    void on_pushButton_browseImageSavePath_clicked();
    void on_pushButton_applyPlcProcessParameters_clicked();
    void on_pushButton_applyImageRotation_clicked();
    void on_pushButton_resetTotalCount_clicked();
    void on_pushButton_resetNgCount_clicked();
    void on_pushButton_resetRejectQueue_clicked();
    void on_pushButton_applyColorChannel_clicked();
    void on_pushButton_applyCameraGain_clicked();
    void on_pushButton_clearSoftwareData_clicked();
    void on_resultExportConnect_clicked();
    void on_resultExportDisconnect_clicked();
    void on_resultExportEnable_toggled(bool enabled);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initializePages();
    void presentTemplatePreviewFrame(const cv::Mat &image);
    void showParameterInfo(const QString &title, const QString &message);
    void showParameterInfoWithRedWarning(const QString &title,
                                         const QString &message,
                                         const QString &warningMessage);
    void showParameterInfoAsError(const QString &title,
                                  const QString &message);
    void showParameterWarning(const QString &title, const QString &message);
    void showParameterCritical(const QString &title, const QString &message);
    bool applyCameraExposureValue(int exposureValue, QString *errorMessage);
    bool applyCameraExposureFromUi(QStringList *errors,
                                   bool showSuccessMessage);
    bool applyCameraGainFromUi(QStringList *errors,
                               bool showSuccessMessage);
    bool applyPlcTriggerModeFromUi(QStringList *errors,
                                   bool showSuccessMessage);
    bool applyPlcRunSettingsFromUi(QStringList *errors,
                                   bool showSuccessMessage);
    bool saveAppliedHardwareSettings(const QStringList &keys);
    bool hasDirtySettings() const;
    QString dirtySettingsMessage() const;
    void restoreUnappliedSettingsFromApplied();
    void updateTissueRoughnessUiVisibility();
    void setupSoftwareSettingsPage();
    void restoreDefaultMachineSettings();
    void updateOperationUiState();
    OperationUiState operationUiState() const;
    bool isCameraOpen() const;
    bool isInspectionBusy() const;
    const AppSettings &machineSettings() const;
    void presentStartFailure(const StartInspectionResult &result);
    void finishInspectionStopUi(const StopInspectionResult &result);
    void presentInspectionFault();
    void checkInspectionPlcHealth();
    void restoreNormalFaultUi();
    void showRightPanelPage(QWidget *page, QToolButton *button);
    void hideRightPanel();
    void setupDetectModeChangeTracking();
    void applyMachineSettingsToUi(const AppSettings &settings);
    void setupNonPersistentDefaults();
    void initStyle();
    void updateResultExportUi(const RuntimeSnapshot &snapshot);

    std::unique_ptr<Ui::MainWindow> ui;
    std::shared_ptr<InspectionApplicationService>
            m_inspectionApplicationService;
    InspectionRuntime *m_runtime = nullptr;
    std::shared_ptr<SettingsApplicationService>
            m_settingsApplicationService;
    std::shared_ptr<TemplateApplicationService>
            m_templateApplicationService;
    QString m_currentDetectModeId;
    SettingsEditState m_settingsEditState;
    bool m_applicationExitInProgress = false;
    bool m_faultAlarmPresented = false;
    QTimer *m_plcHealthTimer = nullptr;

    std::unique_ptr<Ui::InspectionInfoPage> m_inspectionInfoUi;
    std::unique_ptr<Ui::DetectionSettingsPage> m_detectionSettingsUi;
    std::unique_ptr<Ui::ImageSettingsPage> m_imageSettingsUi;
    std::unique_ptr<Ui::PlcSettingsPage> m_plcSettingsUi;
    std::unique_ptr<Ui::SoftwareSettingsPage> m_softwareSettingsUi;

    std::unique_ptr<InspectionPage> m_inspectionPage;
    std::unique_ptr<MachineSettingsPage> m_machineSettingsPage;
    std::unique_ptr<TemplateEditorPage> m_templateEditorPage;
};
