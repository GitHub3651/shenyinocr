#include "ui/main_window/main_window.h"
#include "contracts/detection_mode.h"
#include "ui/main_window/inspection/inspection_page.h"
#include "ui/main_window/settings/machine_settings_page.h"
#include "ui/main_window/template/template_editor_page.h"
#include "system_support/logging/log_categories.h"
#include "ui_detection_settings_page.h"
#include "ui_image_settings_page.h"
#include "ui_inspection_info_page.h"
#include "ui_main_window.h"
#include "ui_plc_settings_page.h"
#include "ui_software_settings_page.h"


#include <QTimer>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>
#include <QString>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextOption>
#include <QToolButton>

MainWindow::MainWindow(
    const std::shared_ptr<InspectionApplicationService> &inspectionService,
    InspectionRuntime *runtime,
    const std::shared_ptr<SettingsApplicationService> &settingsService,
    const std::shared_ptr<TemplateApplicationService> &templateService,
    QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWindow),
      m_inspectionApplicationService(inspectionService),
      m_runtime(runtime),
      m_settingsApplicationService(settingsService),
      m_templateApplicationService(templateService),
      m_inspectionInfoUi(new Ui::InspectionInfoPage),
      m_detectionSettingsUi(new Ui::DetectionSettingsPage),
      m_imageSettingsUi(new Ui::ImageSettingsPage),
      m_plcSettingsUi(new Ui::PlcSettingsPage),
      m_softwareSettingsUi(new Ui::SoftwareSettingsPage)
{
    ui->setupUi(this);
    m_inspectionInfoUi->setupUi(ui->page_inspectionInfo);
    m_detectionSettingsUi->setupUi(ui->page_detectionSettings);
    m_imageSettingsUi->setupUi(ui->page_imageSettings);
    m_plcSettingsUi->setupUi(ui->page_plcSettings);
    m_softwareSettingsUi->setupUi(ui->page_softwareSettings);
    if (m_settingsApplicationService->current()
            .leftDrawerSplitterState.isEmpty()) {
        ui->splitter_leftDrawerMain->setSizes(
                    QList<int>()
                    << 400
                    << ui->splitter_leftDrawerMain->width() - 400);
    } else {
        ui->splitter_leftDrawerMain->restoreState(
                    m_settingsApplicationService->current()
                    .leftDrawerSplitterState);
    }

    m_detectionSettingsUi->barcodeCsvOutputDirectory->setText(
                machineSettings().barcodeCsvOutputDirectory);
    {
        QSignalBlocker blocker(m_detectionSettingsUi->barcodeCsvEnable);
        m_detectionSettingsUi->barcodeCsvEnable->setChecked(
                    machineSettings().barcodeCsvEnabled);
    }
    initializePages();

    connect(ui->toolButton_showInspectionInfo,
            &QToolButton::clicked,
            this,
            [this]() {
        showLeftDrawerPage(
                    ui->page_inspectionInfo,
                    ui->toolButton_showInspectionInfo);
    });
    connect(ui->toolButton_showDetectionSettings,
            &QToolButton::clicked,
            this,
            [this]() {
        showLeftDrawerPage(
                    ui->page_detectionSettings,
                    ui->toolButton_showDetectionSettings);
    });
    connect(ui->toolButton_showImageSettings,
            &QToolButton::clicked,
            this,
            [this]() {
        showLeftDrawerPage(
                    ui->page_imageSettings,
                    ui->toolButton_showImageSettings);
    });
    connect(ui->toolButton_showPlcSettings,
            &QToolButton::clicked,
            this,
            [this]() {
        showLeftDrawerPage(
                    ui->page_plcSettings,
                    ui->toolButton_showPlcSettings);
    });
    connect(ui->toolButton_showSoftwareSettings,
            &QToolButton::clicked,
            this,
            [this]() {
        showLeftDrawerPage(
                    ui->page_softwareSettings,
                    ui->toolButton_showSoftwareSettings);
    });
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::runtimeSnapshotChanged,
            this,
            [this](const RuntimeSnapshot &snapshot) {
        updateBarcodeCsvUi(snapshot);
        updateOperationUiState();
    });

    m_plcHealthTimer = new QTimer(this);
    m_plcHealthTimer->setInterval(500);
    connect(m_plcHealthTimer,
            &QTimer::timeout,
            this,
            &MainWindow::checkInspectionPlcHealth);
    m_plcHealthTimer->start();
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::captureStopped,
            this,
            [this](bool preview) {
        if (preview) {
            return;
        }
        if (operationUiState() == OperationUiState::Detecting) {
            m_inspectionApplicationService
                    ->completeUnexpectedAcquisitionStop();
            m_inspectionInfoUi->label_runtimeStatus->setText(
                        "图像采集已停止，检测已暂停");
            updateOperationUiState();
        }
    },
    Qt::QueuedConnection);
    connect(
        m_runtime,
        &InspectionRuntime::presentationReady,
        this,
        [this](const InspectionPresentation &presentation) {
        m_inspectionPage->present(presentation);
        m_inspectionPage->setStatistics(m_runtime->statistics());
    },
    Qt::QueuedConnection);
    connect(
        m_runtime,
        &InspectionRuntime::imageSaveFailed,
        this,
        [this](quint64 totalFailed, const QString &latestError) {
        m_inspectionPage->reportImageSaveFailure(
                    totalFailed, latestError);
    },
    Qt::QueuedConnection);
    connect(
        m_runtime,
        &InspectionRuntime::faultSnapshotChanged,
        this,
        [this](const InspectionFaultSnapshot &snapshot) {
        if (!m_faultAlarmPresented) {
            ui->toolButton_showInspectionInfo->setChecked(true);
            showLeftDrawerPage(
                        ui->page_inspectionInfo,
                        ui->toolButton_showInspectionInfo);
        }
        m_inspectionPage->presentFault(snapshot, m_faultAlarmPresented);
        updateOperationUiState();
    },
    Qt::QueuedConnection);
}

void MainWindow::showLeftDrawerPage(
    QWidget *page,
    QToolButton *button)
{
    if (!button->isChecked()) {
        hideLeftDrawer();
        return;
    }

    ui->stackedWidget_leftDrawer->setCurrentWidget(page);
    ui->widget_leftDrawer->show();
    ui->toolButton_showInspectionInfo->setChecked(
                button == ui->toolButton_showInspectionInfo);
    ui->toolButton_showDetectionSettings->setChecked(
                button == ui->toolButton_showDetectionSettings);
    ui->toolButton_showImageSettings->setChecked(
                button == ui->toolButton_showImageSettings);
    ui->toolButton_showPlcSettings->setChecked(
                button == ui->toolButton_showPlcSettings);
    ui->toolButton_showSoftwareSettings->setChecked(
                button == ui->toolButton_showSoftwareSettings);
}

void MainWindow::hideLeftDrawer()
{
    ui->widget_leftDrawer->hide();
    ui->toolButton_showInspectionInfo->setChecked(false);
    ui->toolButton_showDetectionSettings->setChecked(false);
    ui->toolButton_showImageSettings->setChecked(false);
    ui->toolButton_showPlcSettings->setChecked(false);
    ui->toolButton_showSoftwareSettings->setChecked(false);
}

void MainWindow::initializePages()
{
    m_inspectionPage.reset(new InspectionPage(
                *this, *ui, *m_inspectionInfoUi));
    m_machineSettingsPage.reset(new MachineSettingsPage(
                *m_detectionSettingsUi,
                *m_imageSettingsUi,
                *m_plcSettingsUi,
                *m_softwareSettingsUi,
                *m_settingsApplicationService,
                m_settingsEditState));
    m_templateEditorPage.reset(new TemplateEditorPage(
                *this,
                *ui,
                *m_detectionSettingsUi,
                *m_imageSettingsUi,
                *m_templateApplicationService,
                *m_inspectionApplicationService,
                *m_settingsApplicationService,
                m_settingsEditState));
    connect(m_templateEditorPage.get(),
            &TemplateEditorPage::operationUiRefreshRequested,
            this, &MainWindow::updateOperationUiState);
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::templatePreviewFrameReady,
            this, [this](const cv::Mat &image) {
        if (m_templateEditorPage->captureState()
                == TemplateEditorPage::CaptureState::Previewing) {
            presentTemplatePreviewFrame(image);
        }
    }, Qt::QueuedConnection);
    connect(m_templateEditorPage.get(),
            &TemplateEditorPage::previewFramePresentationRequested,
            this, &MainWindow::presentTemplatePreviewFrame);
    connect(ui->toolButton_createTemplate,
            &QToolButton::clicked,
            m_templateEditorPage.get(),
            &TemplateEditorPage::handleTemplateCaptureButton);
    connect(m_detectionSettingsUi->pushButton_applyPhotoDistance,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyPhotoDistance_clicked);
    connect(m_detectionSettingsUi->barcodeCsvBrowseDirectory,
            &QPushButton::clicked,
            this, &MainWindow::on_barcodeCsvBrowseDirectory_clicked);
    connect(m_detectionSettingsUi->barcodeCsvEnable,
            &QCheckBox::toggled,
            this, &MainWindow::on_barcodeCsvEnable_toggled);

    connect(m_imageSettingsUi->pushButton_browseImageSavePath,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_browseImageSavePath_clicked);
    connect(m_imageSettingsUi->pushButton_applyCameraExposure,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyCameraExposure_clicked);
    connect(m_imageSettingsUi->pushButton_applyCameraGain,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyCameraGain_clicked);
    connect(m_imageSettingsUi->pushButton_applyColorChannel,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyColorChannel_clicked);
    connect(m_imageSettingsUi->pushButton_applyImageRotation,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyImageRotation_clicked);

    connect(m_plcSettingsUi->pushButton_connectPlc,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_connectPlc_clicked);
    connect(m_plcSettingsUi->pushButton_disconnectPlc,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_disconnectPlc_clicked);
    connect(m_plcSettingsUi->pushButton_applyPlcTriggerMode,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyPlcTriggerMode_clicked);
    connect(m_plcSettingsUi->pushButton_applyPlcProcessParameters,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_applyPlcProcessParameters_clicked);

    connect(m_softwareSettingsUi->pushButton_clearSoftwareData,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_clearSoftwareData_clicked);

    connect(m_inspectionInfoUi->pushButton_resetTotalCount,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_resetTotalCount_clicked);
    connect(m_inspectionInfoUi->pushButton_resetNgCount,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_resetNgCount_clicked);
    connect(m_inspectionInfoUi->pushButton_resetRejectQueue,
            &QPushButton::clicked,
            this, &MainWindow::on_pushButton_resetRejectQueue_clicked);

    const OperationResult resetResult =
            m_inspectionApplicationService->resetStatistics();
    if (!resetResult.isSuccess()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=statistics.reset_failed context=startup code=%1 reason=%2")
                   .arg(resetResult.error.code,
                        resetResult.error.userMessage);
    } else {
        m_inspectionPage->setStatistics(m_runtime->statistics());
    }
    m_detectionSettingsUi->textEdit_targetText->setWordWrapMode(
                QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_softwareSettingsUi->lineEdit_softwareDataDirectory->setText(
                m_settingsApplicationService->applicationDataRoot());
    m_machineSettingsPage->installWheelProtection(*this);
    m_machineSettingsPage->setupNumericInputValidators();
    setupNonPersistentDefaults();
    m_machineSettingsPage->setupBindings();
    m_machineSettingsPage->initialize(
                m_settingsApplicationService->current());
    m_currentDetectModeId = m_templateEditorPage->currentDetectModeId();
    m_templateEditorPage->restoreTemplatesForMode(
                m_currentDetectModeId, false);
    updateTissueRoughnessUiVisibility();
    setupDetectModeChangeTracking();
    m_machineSettingsPage->clearAllDirty();
    m_templateEditorPage->clearTemplateDirty();
    updateOperationUiState();
    updateBarcodeCsvUi(m_inspectionApplicationService->runtimeSnapshot());

    QTimer::singleShot(1000, this, [this]() {
        const QString targetIp =
                m_plcSettingsUi->lineEdit_plcIpAddress->text();
        PlcConnectionCommand command;
        command.address = targetIp;
        command.rack = m_plcSettingsUi->lineEdit_plcRack->text().toInt();
        command.slot = m_plcSettingsUi->lineEdit_plcSlot->text().toInt();
        const OperationResult result =
                m_inspectionApplicationService->connectPlc(command);
        if (result.isSuccess()) {
            const QStringList connectionKeys =
                    QStringList() << "plc.ip" << "plc.rack" << "plc.slot";
            if (saveAppliedHardwareSettings(connectionKeys)) {
                QMessageBox::information(this, "提示", "PLC 自动连接成功");
            }
        } else {
            const bool stateRejected =
                    result.error.code == QStringLiteral("PLC_RUNTIME_BUSY")
                    || result.error.code
                       == QStringLiteral("PLC_ALREADY_CONNECTED");
            const QString errorMessage = stateRejected
                    ? result.error.userMessage
                    : QString(
                        "PLC 自动连接失败！\n尝试连接的地址：%1\n"
                        "请检查网络或稍后手动连接！").arg(targetIp);
            m_machineSettingsPage->restoreAppliedValues(
                        QStringList()
                        << "plc.ip" << "plc.rack" << "plc.slot");
            QMessageBox::warning(this, "警告", errorMessage);
        }
        updateOperationUiState();
    });
}

MainWindow::~MainWindow()
{
    m_templateEditorPage->resetTemplateCaptureState();
    m_inspectionApplicationService->shutdown();

}

void MainWindow::updateBarcodeCsvUi(const RuntimeSnapshot &snapshot)
{
    DetectionMode mode = DetectionMode::Word;
    const bool barcodeMode = detectionModeFromUiId(
                machineSettings().detectModeId, &mode)
            && mode == DetectionMode::BarcodeWord;
    m_detectionSettingsUi->groupBox_barcodeCsv->setVisible(barcodeMode);
    if (!barcodeMode) {
        return;
    }
    const bool canEdit = !snapshot.isInspectionBusy();
    m_detectionSettingsUi->barcodeCsvEnable->setEnabled(canEdit);
    m_detectionSettingsUi->barcodeCsvBrowseDirectory->setEnabled(canEdit);
}

void MainWindow::on_barcodeCsvBrowseDirectory_clicked()
{
    const QString currentDirectory =
            machineSettings().barcodeCsvOutputDirectory;
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择二维码结果保存文件夹"),
                currentDirectory.isEmpty()
                ? QStringLiteral("C:/")
                : currentDirectory,
                QFileDialog::ShowDirsOnly
                | QFileDialog::DontUseNativeDialog);
    if (selectedDirectory.isEmpty()) {
        return;
    }

    AppSettings candidate = m_settingsApplicationService->current();
    candidate.barcodeCsvOutputDirectory = QDir::cleanPath(
                QFileInfo(selectedDirectory).absoluteFilePath());
    const OperationResult saved = m_settingsApplicationService
            ->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        m_detectionSettingsUi->barcodeCsvOutputDirectory->setText(
                    machineSettings().barcodeCsvOutputDirectory);
        showParameterWarning(QStringLiteral("二维码结果保存设置失败"),
                             saved.error.userMessage);
        return;
    }
    m_detectionSettingsUi->barcodeCsvOutputDirectory->setText(
                candidate.barcodeCsvOutputDirectory);
}

void MainWindow::on_barcodeCsvEnable_toggled(bool enabled)
{
    AppSettings candidate = m_settingsApplicationService->current();
    candidate.barcodeCsvEnabled = enabled;
    const OperationResult saved = m_settingsApplicationService
            ->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        QSignalBlocker blocker(m_detectionSettingsUi->barcodeCsvEnable);
        m_detectionSettingsUi->barcodeCsvEnable->setChecked(
                    m_settingsApplicationService->current()
                    .barcodeCsvEnabled);
        showParameterWarning(QStringLiteral("二维码结果保存设置失败"),
                             saved.error.userMessage);
    }
}
