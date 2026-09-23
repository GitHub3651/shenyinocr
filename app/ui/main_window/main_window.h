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
        const QStringList &authorizedModeIds,
        QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void on_pushButton_applyCameraExposure_clicked();
    void on_pushButton_connectPlc_clicked();
    void on_pushButton_disconnectPlc_clicked();
    void on_pushButton_applyPhotoDistance_clicked();
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
    void on_barcodeCsvBrowseDirectory_clicked();
    void on_barcodeCsvEnable_toggled(bool enabled);
    void initializePages(const QStringList &authorizedModeIds);
    void handleCameraAction();
    void handleInspectionAction();
    void handlePreviewAction();
    void openCamera();
    void closeCamera();
    void startInspection();
    void stopInspection();
    void startPreviewOnly();
    void stopPreviewOnly();
    void exitTemplate();
    void presentPreviewFrame(const cv::Mat &image);
    void showParameterInfo(const QString &title, const QString &message);
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
    void updateOperationUiState();
    OperationUiState operationUiState(
        const RuntimeSnapshot &snapshot) const;
    bool isCameraOpen() const;
    bool isInspectionBusy() const;
    const AppSettings &machineSettings() const;
    void presentStartFailure(const StartInspectionResult &result);
    void finishInspectionStopUi(
        const CameraRecoveryResultDto &cameraRecovery);
    void checkInspectionPlcHealth();
    void showLeftDrawerPage(QWidget *page, QToolButton *button);
    void hideLeftDrawer();
    void setupDetectModeChangeTracking();
    void setupNonPersistentDefaults();
    void updateBarcodeCsvDirectoryDisplay();
    void updateBarcodeCsvUi(const RuntimeSnapshot &snapshot);

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
    bool m_previewActive = false;
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
