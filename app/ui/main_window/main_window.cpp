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
#include <QCoreApplication>
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
    const QStringList &authorizedModeIds,
    bool showLicenseExpiry,
    bool permanentLicense,
    const QDate &licenseExpiresDate,
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
    m_softwareSettingsUi->label_licenseExpiry->setVisible(
                showLicenseExpiry);
    m_softwareSettingsUi->label_licenseExpiryValue->setVisible(
                showLicenseExpiry);
    if (showLicenseExpiry) {
        m_softwareSettingsUi->label_licenseExpiryValue->setText(
                    permanentLicense
                    ? QStringLiteral("长期有效")
                    : licenseExpiresDate.toString(
                        QStringLiteral("yyyy-MM-dd")));
    }
    const auto updateHardwareTriggerText = [this](bool enabled) {
        ui->checkBox_hardwareTriggerEnabled->setText(
                    enabled ? QStringLiteral("开") : QStringLiteral("关"));
    };
    connect(ui->checkBox_hardwareTriggerEnabled,
            &QCheckBox::toggled,
            ui->checkBox_hardwareTriggerEnabled,
            updateHardwareTriggerText);
    updateHardwareTriggerText(
                ui->checkBox_hardwareTriggerEnabled->isChecked());
    if (m_settingsApplicationService->current()
            .leftDrawerSplitterState.isEmpty()) {
        ui->splitter_leftDrawerMain->setSizes(
                    QList<int>()
                    << 400
                    << 1200);
    } else {
        ui->splitter_leftDrawerMain->restoreState(
                    m_settingsApplicationService->current()
                    .leftDrawerSplitterState);
    }

    m_detectionSettingsUi->barcodeCsvOutputDirectory->setText(
                machineSettings().barcodeCsvOutputDirectory);
    updateBarcodeCsvDirectoryDisplay();
    {
        QSignalBlocker blocker(m_detectionSettingsUi->barcodeCsvEnable);
        m_detectionSettingsUi->barcodeCsvEnable->setChecked(
                    machineSettings().barcodeCsvEnabled);
    }
    initializePages(authorizedModeIds);
    connect(ui->toolButton_cameraAction,
            &QToolButton::clicked,
            this,
            &MainWindow::handleCameraAction);
    connect(ui->toolButton_inspectionAction,
            &QToolButton::clicked,
            this,
            &MainWindow::handleInspectionAction);
    connect(ui->toolButton_previewAction,
            &QToolButton::clicked,
            this,
            &MainWindow::handlePreviewAction);
    connect(m_detectionSettingsUi->pushButton_exitTemplate,
            &QPushButton::clicked,
            this,
            &MainWindow::exitTemplate);
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
            [this](const RuntimeSnapshot &) {
        updateOperationUiState();
    });

    m_plcHealthTimer = new QTimer(this);
    m_plcHealthTimer->setInterval(500);
    connect(m_plcHealthTimer,
            &QTimer::timeout,
            this,
            &MainWindow::checkInspectionPlcHealth);
    m_plcHealthTimer->start();
    connect(
        m_runtime,
        &InspectionRuntime::presentationReady,
        this,
        [this](const InspectionPresentation &presentation) {
        m_inspectionPage->present(presentation);
    });
    connect(
        m_runtime,
        &InspectionRuntime::faultSnapshotChanged,
        this,
        [this](const InspectionFaultSnapshot &snapshot) {
        const CameraRecoveryResultDto recovery =
                m_inspectionApplicationService->stop(snapshot.reason);
        finishInspectionStopUi(recovery);

        QString reasonText;
        QString actionHint;
        switch (snapshot.reason) {
        case InspectionFaultReason::CameraDisconnected:
            reasonText = QStringLiteral("相机连接或图像采集异常");
            actionHint = QStringLiteral(
                        "相机已关闭，请检查并重新连接相机后重新开始识别。");
            break;
        case InspectionFaultReason::PlcDisconnected:
            reasonText = QStringLiteral("PLC 连接或检测结果发送异常");
            actionHint = QStringLiteral(
                        "PLC 已断开，请检查并重新连接 PLC 后重新开始识别。");
            break;
        case InspectionFaultReason::HardTriggerQueueOverflow:
            reasonText = QStringLiteral("待检测图像过多");
            actionHint = QStringLiteral(
                        "请检查输送线节拍和图像处理速度后重新开始识别。");
            break;
        case InspectionFaultReason::RuntimeInvariantViolation:
            reasonText = QStringLiteral("系统运行状态异常");
            actionHint = QStringLiteral(
                        "请检查运行日志和故障期间的产品后重新开始识别。");
            break;
        case InspectionFaultReason::BarcodeCsvUnavailable:
            reasonText = QStringLiteral("二维码结果无法保存到本机");
            actionHint = QStringLiteral(
                        "请检查二维码 CSV 保存目录后重新开始识别。");
            break;
        case InspectionFaultReason::None:
            return;
        }
        QMessageBox::critical(
                    this,
                    QStringLiteral("系统故障－识别已停止"),
                    QStringLiteral(
                        "系统发生故障，识别已自动停止。\n\n"
                        "故障原因：%1\n"
                        "本次运行软件已接收 %2 张图像，"
                        "已完成检测 %3 张。\n"
                        "未完成检测 %4 张，已丢弃。\n\n"
                        "请检查输送线状态和故障期间的产品。\n"
                        "%5")
                    .arg(reasonText)
                    .arg(snapshot.acceptedProductCount)
                    .arg(snapshot.finalizedProductCount)
                    .arg(snapshot.acceptedProductCount
                         - snapshot.finalizedProductCount)
                    .arg(actionHint));
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

void MainWindow::initializePages(const QStringList &authorizedModeIds)
{
    m_inspectionPage.reset(new InspectionPage(
                *ui, *m_inspectionInfoUi));
    m_machineSettingsPage.reset(new MachineSettingsPage(
                *m_detectionSettingsUi,
                *m_imageSettingsUi,
                *m_plcSettingsUi,
                *m_softwareSettingsUi,
                *ui->checkBox_hardwareTriggerEnabled,
                *m_settingsApplicationService,
                m_settingsEditState,
                authorizedModeIds));
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
            &InspectionApplicationService::previewFrameReady,
            this, &MainWindow::presentPreviewFrame,
            Qt::QueuedConnection);
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::previewFailed,
            this, [this](const QString &reason) {
        if (!m_previewActive) {
            return;
        }
        m_previewActive = false;
        updateOperationUiState();
        showParameterWarning(QStringLiteral("实时预览失败"), reason);
    }, Qt::QueuedConnection);
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::previewStopped,
            this, [this]() {
        if (!m_previewActive) {
            return;
        }
        m_previewActive = false;
        updateOperationUiState();
        showParameterWarning(
                    QStringLiteral("实时预览已停止"),
                    QStringLiteral("相机预览已意外停止。"));
    }, Qt::QueuedConnection);
    connect(m_templateEditorPage.get(),
            &TemplateEditorPage::templateImagePresentationRequested,
            this, [this](const cv::Mat &image) {
        DetectionMode activeMode = DetectionMode::Word;
        const bool tissueMode = detectionModeFromUiId(
                    machineSettings().detectModeId,
                    &activeMode)
                && activeMode == DetectionMode::Tissue;
        const QImage preview =
                m_inspectionApplicationService->renderPreviewFrame(
                    image, tissueMode, false);
        m_inspectionPage->presentPreviewImage(preview);
    });
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

MainWindow::~MainWindow() = default;

void MainWindow::updateBarcodeCsvDirectoryDisplay()
{
    const QString path = m_detectionSettingsUi->barcodeCsvOutputDirectory->text();
    const QString openDirectoryTip = QCoreApplication::translate(
                "DetectionSettingsPage", "双击可打开当前文件夹");
    m_detectionSettingsUi->barcodeCsvOutputDirectory->setToolTip(
                path.isEmpty()
                ? openDirectoryTip
                : path + QStringLiteral("\n") + openDirectoryTip);
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
        updateBarcodeCsvDirectoryDisplay();
        showParameterWarning(QStringLiteral("二维码结果保存设置失败"),
                             saved.error.userMessage);
        return;
    }
    m_detectionSettingsUi->barcodeCsvOutputDirectory->setText(
                candidate.barcodeCsvOutputDirectory);
    updateBarcodeCsvDirectoryDisplay();
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
