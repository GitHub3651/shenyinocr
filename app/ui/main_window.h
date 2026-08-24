// 文件作用：本文件用于构造主窗口、连接页面和应用服务，并维护顶层界面生命周期。
// 主要职责：构造主窗口、连接页面和应用服务，并维护顶层界面生命周期。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef OCRGANGYIN_UI_MAIN_WINDOW_H
#define OCRGANGYIN_UI_MAIN_WINDOW_H

#ifndef GLOG_NO_ABBREVIATED_SEVERITIES
#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#endif

#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <opencv2/core.hpp>

#include <memory>

#include "application/inspection_application_service.h"
#include "runtime/inspection_runtime.h"
#include "application/settings_application_service.h"
#include "templates/template_store.h"
#include "ui/controllers/operation_ui_policy.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"
#include "ui/widgets/image_label.h"


namespace Ui {
class MainWindow;
}

class QLabel;
class QComboBox;
class QFrame;
class QDialog;
class QPushButton;
class QLineEdit;
class QTimer;
class QCloseEvent;
// 组件说明：TemplateApplicationService 组件封装对应业务职责和生命周期边界。
class TemplateApplicationService;

/**
 * @brief 主窗口类
 *
 * 功能：
 * - 相机控制和图像采集
 * - OCR识别和检测
 * - 模板匹配和字库匹配
 * - PLC通信
 * - 图像跟踪
 */
// 组件说明：MainWindow 组件负责对应界面区域的显示和用户交互。
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
    ~MainWindow();

private slots:
    void slot_displayAndDetect(cv::Mat *image);  ///< 显示和检测槽

    void on_toolButton_createTemplate_clicked();       ///< 单词采集按钮
    void on_toolButton_openCamera_clicked();   ///< 相机检测按钮
    void on_toolButton_closeCamera_clicked();      ///< 关闭相机按钮
    void on_pushButton_applyCameraExposure_clicked();       ///< 确定按钮

    void on_toolButton_startInspection_clicked();           ///< PLC按钮
    void on_pushButton_connectPlc_clicked(); ///< 连接PLC按钮
    void on_pushButton_disconnectPlc_clicked(); ///< 断开PLC按钮
    void on_pushButton_applyPhotoDistance_clicked(); ///< 写入VD按钮

    void on_toolButton_stopInspection_clicked();           ///< 取消按钮
    void closeEvent(QCloseEvent *event) override; ///< 关闭事件

    void on_pushButton_applyPlcTriggerMode_clicked();       ///< PLC模式按钮

    void on_pushButton_browseImageSavePath_clicked();
    void on_pushButton_applyPlcProcessParameters_clicked();
    void on_pushButton_applyImageRotation_clicked();
    void on_pushButton_resetTotalCount_clicked();
    void on_pushButton_resetNgCount_clicked();


    void on_pushButton_resetRejectQueue_clicked();

    void on_pushButton_applyColorChannel_clicked();


    void on_pushButton_applyCameraGain_clicked();
    void on_pushButton_clearSoftwareData_clicked();

private:
    void initializePages();
    InspectionPageViewBindings inspectionPageViewBindings() const;
    MachineSettingsPageViewBindings machineSettingsPageViewBindings() const;
    MachineSettingsPage::Callbacks machineSettingsPageCallbacks();
    TemplateEditorViewBindings templateEditorViewBindings() const;
    TemplateEditorPageCallbacks templateEditorPageCallbacks();
    void showParameterInfo(const QString &title, const QString &message);
    void showParameterInfoWithRedWarning(const QString &title,
                                         const QString &message,
                                         const QString &warningMessage);
    void showParameterInfoAsError(const QString &title, const QString &message);
    void showParameterWarning(const QString &title, const QString &message);
    void showParameterCritical(const QString &title, const QString &message);
    bool applyCameraExposureValue(int exposureValue, QString *errorMessage);
    bool applyCameraExposureFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyCameraGainFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    bool saveAppliedHardwareSettings(const QStringList &keys);
    bool hasDirtySettings() const;
    QString dirtySettingsMessage() const;
    void restoreUnappliedSettingsFromApplied();
    void updateSaveDirButtonText();
    void updateImageSaveOptionsVisibility();
    void updateTissueRoughnessUiVisibility();
    void setupSoftwareSettingsPage();
    void restoreDefaultMachineSettings();
    void resetTemplateCaptureState();
    void updateOperationUiState();
    OperationUiState operationUiState() const;
    bool isCameraOpen() const;
    bool isInspectionBusy() const;
    const AppSettings &machineSettings() const;
    void presentStartFailure(const StartInspectionResult &result);
    void finishInspectionStopUi(const StopInspectionResult &result);
    void presentInspectionFault();
    bool confirmInspectionFaultRecovery();
    void checkInspectionPlcHealth();
    void restoreNormalFaultUi();

    // ========== UI对象 ==========
    Ui::MainWindow *ui;                     ///< UI界面指针
    QLineEdit *m_softwareDataDirLineEdit = nullptr;
    std::shared_ptr<InspectionApplicationService>
            m_inspectionApplicationService;
    InspectionRuntime *m_runtime = nullptr;
    std::shared_ptr<SettingsApplicationService>
            m_settingsApplicationService;
    std::shared_ptr<TemplateApplicationService>
            m_templateApplicationService;
    QString m_currentDetectModeId;
    SettingsEditState m_settingsEditState;
    std::unique_ptr<MachineSettingsPage> m_machineSettingsPage;
    std::unique_ptr<TemplateEditorPage> m_templateEditorPage;

    using OperationState = OperationUiState;
    bool m_applicationExitInProgress = false;
    std::unique_ptr<InspectionPage> m_inspectionPage;
    bool m_faultAlarmPresented = false;

    // ========== 定时器 ==========
    QTimer *m_plcHealthTimer = nullptr; ///< 运行中PLC连接监视
    QTimer *m_templateCaptureAttentionTimer = nullptr;
    bool m_templateCaptureAttentionOn = false;

    QPointer<ImageLabel> imageLabel;
    void setupDetectModeChangeTracking();

    // ========== 设置相关函数 ==========
    void applyMachineSettingsToUi(const AppSettings &settings);
    void setupNonPersistentDefaults();  ///< 设置不属于公共配置的初始值
    void initStyle();  // 声明后才能在 cpp 中实现和调用
};

#endif // OCRGANGYIN_UI_MAIN_WINDOW_H
