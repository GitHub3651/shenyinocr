// 文件作用：本文件用于构造主窗口、连接页面和应用服务，并维护顶层界面生命周期。
// 主要职责：构造主窗口、连接页面和应用服务，并维护顶层界面生命周期。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
/**
 * @file ui/main_window.cpp
 * @brief 工业视觉识别系统主窗口实现文件
 * @details 实现图像采集、OCR识别、模板匹配、PLC通信等核心功能
 * @author 优化版本
 * @date 2024
 */

#include "ui/main_window.h"
#include "ui_main_window.h"
#include "contracts/detection_mode.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"
#include "system_support/logging/log_categories.h"


#include <QTimer>
#include <QLabel>
#include <QString>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QFrame>
#include <QScrollArea>
#include <QSplitterHandle>
#include <QStyle>
#include <QTextOption>

/**
 * @brief MainWindow构造函数
 * @param parent 父窗口指针
 * @details 初始化UI、相机、OCR模型、定时器等核心组件
 */
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
      imageLabel(nullptr)
{
    ui->setupUi(this);
    ui->resultExportIp->setText(machineSettings().resultExportReceiverIp);
    ui->resultExportPort->setValue(machineSettings().resultExportReceiverPort);
    initStyle();

    // 检测信息区域允许被分隔条压缩；空间不足时只在该区域内部滚动。
    QScrollArea *scrollArea_inspectionInfo = new QScrollArea;
    scrollArea_inspectionInfo->setObjectName("scrollArea_inspectionInfo");
    scrollArea_inspectionInfo->setFrameShape(QFrame::NoFrame);
    scrollArea_inspectionInfo->setWidgetResizable(true);
    scrollArea_inspectionInfo->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_inspectionInfo->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea_inspectionInfo->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Expanding);
    scrollArea_inspectionInfo->setWidget(ui->groupBox_inspectionInfo);
    ui->splitter_mainContent->insertWidget(0, scrollArea_inspectionInfo);

    if (QSplitterHandle *rightPanelHandle =
            ui->splitter_mainContent->handle(1)) {
        rightPanelHandle->setCursor(Qt::SplitVCursor);
        rightPanelHandle->setToolTip(
                    QString::fromWCharArray(
                        L"上下拖动"
                        L"调整区域高度"));

        QHBoxLayout *handleLayout =
                new QHBoxLayout(rightPanelHandle);
        handleLayout->setContentsMargins(0, 0, 0, 0);
        handleLayout->setSpacing(0);

        QLabel *handleGrip =
                new QLabel(QString::fromWCharArray(
                               L"↕  拖动调整"),
                           rightPanelHandle);
        handleGrip->setObjectName("label_mainContentSplitterGrip");
        handleGrip->setAlignment(Qt::AlignCenter);
        handleGrip->setMinimumWidth(92);
        handleGrip->setAttribute(
                    Qt::WA_TransparentForMouseEvents);
        handleGrip->ensurePolished();

        const int handleGripHeight =
                qMax(24, handleGrip->fontMetrics().height() + 8);
        const int splitterHandleHeight =
                handleGripHeight + 8;
        handleGrip->setFixedHeight(handleGripHeight);
        ui->splitter_mainContent->setProperty(
                    "visualHandleHeight",
                    splitterHandleHeight);
        ui->splitter_mainContent->setHandleWidth(
                    splitterHandleHeight);
        rightPanelHandle->setMinimumHeight(
                    splitterHandleHeight);
        rightPanelHandle->setMaximumHeight(
                    splitterHandleHeight);

        handleLayout->addStretch();
        handleLayout->addWidget(handleGrip);
        handleLayout->addStretch();
    }

    m_templateCaptureAttentionTimer = new QTimer(this);
    m_templateCaptureAttentionTimer->setInterval(900);
    connect(m_templateCaptureAttentionTimer,
            &QTimer::timeout,
            this,
            [this]() {
                if (!ui
                        || !ui->toolButton_createTemplate
                        || operationUiState()
                           != OperationState::TemplatePreviewing) {
            m_templateCaptureAttentionTimer->stop();
            m_templateCaptureAttentionOn = false;
        } else {
            m_templateCaptureAttentionOn =
                    !m_templateCaptureAttentionOn;
        }

        if (ui && ui->toolButton_createTemplate) {
            const bool previewing = operationUiState()
                    == OperationState::TemplatePreviewing;
            ui->toolButton_createTemplate->setProperty(
                        "uiState",
                        previewing
                        ? (m_templateCaptureAttentionOn
                           ? QStringLiteral("attention")
                           : QStringLiteral("preview"))
                        : QString());
            ui->toolButton_createTemplate->style()->unpolish(
                        ui->toolButton_createTemplate);
            ui->toolButton_createTemplate->style()->polish(
                        ui->toolButton_createTemplate);
            ui->toolButton_createTemplate->update();
        }
    });

    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::runtimeSnapshotChanged,
            this,
            [this](const RuntimeSnapshot &snapshot) {
        updateResultExportUi(snapshot);
        updateOperationUiState();
    });
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::resultExportOutboxDispositionRequired,
            this,
            [this](int count) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QStringLiteral("发现当前进程待发送记录"));
        box.setText(QStringLiteral("当前进程有 %1 条待发送二维码结果。")
                    .arg(count));
        QPushButton *synchronize = box.addButton(
                    QStringLiteral("同步历史记录"), QMessageBox::AcceptRole);
        QPushButton *abandon = box.addButton(
                    QStringLiteral("放弃历史记录"), QMessageBox::DestructiveRole);
        box.setDefaultButton(synchronize);
        box.exec();
        if (box.clickedButton() == synchronize) {
            m_inspectionApplicationService->synchronizeResultExportOutbox();
        } else if (box.clickedButton() == abandon) {
            m_inspectionApplicationService->abandonResultExportOutbox();
        }
    }, Qt::QueuedConnection);

    // UI 文件中已经是 ImageLabel，直接使用。
    imageLabel = ui->imageLabel_inspection;

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
        if (operationUiState() == OperationState::Detecting) {
            m_inspectionApplicationService
                    ->completeUnexpectedAcquisitionStop();
            ui->label_runtimeStatus->setText("识别线程已停止");
            updateOperationUiState();
        }
    },
    Qt::QueuedConnection);
    initializePages();
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
        m_inspectionPage->presentFault(snapshot, &m_faultAlarmPresented);
        updateOperationUiState();
    },
    Qt::QueuedConnection);
}

InspectionPageViewBindings MainWindow::inspectionPageViewBindings() const
{
    InspectionPageViewBindings view;
    view.imageLabel_inspection = ui->imageLabel_inspection;
    view.label_recognitionText = ui->label_recognitionText;
    view.label_runtimeStatus = ui->label_runtimeStatus;
    view.label_verdictResult = ui->label_verdictResult;
    view.lineEdit_currentTemplateName = ui->lineEdit_currentTemplateName;
    view.lineEdit_detectionDuration = ui->lineEdit_detectionDuration;
    view.lineEdit_ngCount = ui->lineEdit_ngCount;
    view.lineEdit_passRate = ui->lineEdit_passRate;
    view.lineEdit_totalCount = ui->lineEdit_totalCount;
    view.pushButton_saveTemplate = ui->pushButton_saveTemplate;
    view.toolButton_closeCamera = ui->toolButton_closeCamera;
    view.toolButton_createTemplate = ui->toolButton_createTemplate;
    view.toolButton_openCamera = ui->toolButton_openCamera;
    view.toolButton_startInspection = ui->toolButton_startInspection;
    view.toolButton_stopInspection = ui->toolButton_stopInspection;
    return view;
}

MachineSettingsPageViewBindings
MainWindow::machineSettingsPageViewBindings() const
{
    MachineSettingsPageViewBindings view;
    view.checkBox_hardwareTriggerEnabled = ui->checkBox_hardwareTriggerEnabled;
    view.comboBox_colorChannel = ui->comboBox_colorChannel;
    view.comboBox_detectionMode = ui->comboBox_detectionMode;
    view.comboBox_imageRotation = ui->comboBox_imageRotation;
    view.comboBox_imageSaveContent = ui->comboBox_imageSaveContent;
    view.comboBox_imageSaveRange = ui->comboBox_imageSaveRange;
    view.comboBox_plcTriggerMode = ui->comboBox_plcTriggerMode;
    view.label_cameraExposure = ui->label_cameraExposure;
    view.label_cameraGain = ui->label_cameraGain;
    view.label_colorChannel = ui->label_colorChannel;
    view.label_detectionMode = ui->label_detectionMode;
    view.label_hardwareTriggerDelay = ui->label_hardwareTriggerDelay;
    view.label_imageRotation = ui->label_imageRotation;
    view.label_imageSaveContent = ui->label_imageSaveContent;
    view.label_imageSavePath = ui->label_imageSavePath;
    view.label_imageSaveRange = ui->label_imageSaveRange;
    view.label_photoDistance = ui->label_photoDistance;
    view.label_photoTime = ui->label_photoTime;
    view.label_plcIpAddress = ui->label_plcIpAddress;
    view.label_plcRackSlot = ui->label_plcRackSlot;
    view.label_plcTriggerMode = ui->label_plcTriggerMode;
    view.label_rejectDistance = ui->label_rejectDistance;
    view.label_rejectPosition = ui->label_rejectPosition;
    view.label_rejectTime = ui->label_rejectTime;
    view.label_tissueRoughnessThreshold =
            ui->label_tissueRoughnessThreshold;
    view.lineEdit_cameraGain = ui->lineEdit_cameraGain;
    view.lineEdit_hardwareTriggerDelay = ui->lineEdit_hardwareTriggerDelay;
    view.lineEdit_imageSavePath = ui->lineEdit_imageSavePath;
    view.lineEdit_photoDistance = ui->lineEdit_photoDistance;
    view.lineEdit_photoTime = ui->lineEdit_photoTime;
    view.lineEdit_plcIpAddress = ui->lineEdit_plcIpAddress;
    view.lineEdit_plcRack = ui->lineEdit_plcRack;
    view.lineEdit_plcSlot = ui->lineEdit_plcSlot;
    view.lineEdit_rejectDistance = ui->lineEdit_rejectDistance;
    view.lineEdit_rejectPosition = ui->lineEdit_rejectPosition;
    view.lineEdit_rejectTime = ui->lineEdit_rejectTime;
    view.lineEdit_tissueRoughnessThreshold = ui->lineEdit_tissueRoughnessThreshold;
    view.pushButton_applyCameraExposure = ui->pushButton_applyCameraExposure;
    view.pushButton_applyCameraGain = ui->pushButton_applyCameraGain;
    view.pushButton_applyPhotoDistance = ui->pushButton_applyPhotoDistance;
    view.pushButton_applyPlcProcessParameters = ui->pushButton_applyPlcProcessParameters;
    view.pushButton_applyPlcTriggerMode = ui->pushButton_applyPlcTriggerMode;
    view.pushButton_connectPlc = ui->pushButton_connectPlc;
    view.pushButton_disconnectPlc = ui->pushButton_disconnectPlc;
    view.pushButton_applyTissueRoughnessThreshold =
            ui->pushButton_applyTissueRoughnessThreshold;
    view.spinBox_cameraExposure = ui->spinBox_cameraExposure;
    view.splitter_mainContent = ui->splitter_mainContent;
    return view;
}

MachineSettingsPage::Callbacks MainWindow::machineSettingsPageCallbacks()
{
    MachineSettingsPage::Callbacks callbacks;
    callbacks.updateImageSaveOptionsVisibility = [this]() {
        updateImageSaveOptionsVisibility();
    };
    callbacks.updateSaveDirectoryText = [this]() {
        updateSaveDirButtonText();
    };
    callbacks.updateTissueVisibility = [this]() {
        updateTissueRoughnessUiVisibility();
    };
    callbacks.reportDirectoryOpenFailure = [this](const QString &path) {
        showParameterWarning(
                    "提示",
                    QString("无法打开软件数据文件夹：\n%1").arg(path));
    };
    return callbacks;
}

TemplateEditorViewBindings MainWindow::templateEditorViewBindings() const
{
    TemplateEditorViewBindings view;
    view.parentWidget = const_cast<MainWindow *>(this);
    view.imageLabel_templateCanvas = imageLabel;
    view.label_targetText = ui->label_targetText;
    view.label_imageThreshold = ui->label_imageThreshold;
    view.textEdit_targetText = ui->textEdit_targetText;
    view.lineEdit_imageThreshold = ui->lineEdit_imageThreshold;
    view.comboBox_imageRotation = ui->comboBox_imageRotation;
    view.comboBox_detectionMode = ui->comboBox_detectionMode;
    view.comboBox_colorChannel = ui->comboBox_colorChannel;
    view.pushButton_editCharacterTemplates = ui->pushButton_editCharacterTemplates;
    view.pushButton_applyTargetText = ui->pushButton_applyTargetText;
    view.pushButton_applyBatchTargetText = ui->pushButton_applyBatchTargetText;
    view.pushButton_applyBatchImageThreshold = ui->pushButton_applyBatchImageThreshold;
    view.pushButton_applyImageThreshold = ui->pushButton_applyImageThreshold;
    view.pushButton_saveTemplate = ui->pushButton_saveTemplate;
    view.toolButton_selectTemplate = ui->toolButton_selectTemplate;
    view.widget_currentTemplateEditor = ui->widget_currentTemplateEditor;
    view.label_currentEditTemplate = ui->label_currentEditTemplate;
    view.comboBox_currentEditTemplate = ui->comboBox_currentEditTemplate;
    view.pushButton_removeCurrentTemplate =
            ui->toolButton_removeCurrentTemplate;
    view.frame_templateGuide = ui->frame_templateGuide;
    view.label_templateGuideTitle = ui->label_templateGuideTitle;
    view.label_templateGuideBody = ui->label_templateGuideBody;
    return view;
}

TemplateEditorPageCallbacks MainWindow::templateEditorPageCallbacks()
{
    TemplateEditorPageCallbacks callbacks;
    callbacks.updateOperationUiState = [this]() {
        updateOperationUiState();
    };
    callbacks.displayPreviewFrame = [this](const cv::Mat &image) {
        cv::Mat displayImage = image.clone();
        slot_displayAndDetect(&displayImage);
    };
    return callbacks;
}

void MainWindow::initializePages()
{
    m_inspectionPage.reset(new InspectionPage(
                this,
                inspectionPageViewBindings(),
                m_templateCaptureAttentionTimer,
                &m_templateCaptureAttentionOn));
    m_machineSettingsPage.reset(new MachineSettingsPage(
                machineSettingsPageViewBindings(),
                m_settingsApplicationService.get(),
                &m_settingsEditState,
                machineSettingsPageCallbacks()));
    m_templateEditorPage.reset(new TemplateEditorPage(
                templateEditorViewBindings(),
                m_templateApplicationService.get(),
                m_inspectionApplicationService.get(),
                m_settingsApplicationService.get(),
                &m_settingsEditState,
                templateEditorPageCallbacks()));
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
    ui->textEdit_targetText->setWordWrapMode(QTextOption::WordWrap);
    setupSoftwareSettingsPage();
    m_machineSettingsPage->installWheelProtection(this);
    m_machineSettingsPage->setupNumericInputValidators();
    setupNonPersistentDefaults();
    m_machineSettingsPage->setupBindings();
    m_machineSettingsPage->initialize(
                m_settingsApplicationService->current());
    m_currentDetectModeId = m_templateEditorPage->currentDetectModeId();
    m_templateEditorPage->restoreTemplatesForMode(
                m_currentDetectModeId, false);
    setupDetectModeChangeTracking();
    m_machineSettingsPage->clearAllDirty();
    m_templateEditorPage->clearTemplateDirty();
    updateOperationUiState();
    updateResultExportUi(m_inspectionApplicationService->runtimeSnapshot());

    QTimer::singleShot(1000, this, [this]() {
        const QString targetIp = ui->lineEdit_plcIpAddress->text();
        PlcConnectionCommand command;
        command.address = targetIp;
        command.rack = ui->lineEdit_plcRack->text().toInt();
        command.slot = ui->lineEdit_plcSlot->text().toInt();
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

/**
 * @brief MainWindow析构函数
 * @details 清理所有资源，关闭相机、停止线程、删除临时文件
 */
MainWindow::~MainWindow()
{
    resetTemplateCaptureState();
    m_inspectionApplicationService->shutdown();

    m_templateEditorPage.reset();
    m_machineSettingsPage.reset();
    m_inspectionPage.reset();
    delete ui;
    ui = nullptr;

}

void MainWindow::updateResultExportUi(const RuntimeSnapshot &snapshot)
{
    DetectionMode mode = DetectionMode::Word;
    const bool barcodeMode = detectionModeFromUiId(
                machineSettings().detectModeId, &mode)
            && mode == DetectionMode::BarcodeWord;
    ui->groupBox_resultExport->setVisible(barcodeMode);
    if (!barcodeMode) {
        return;
    }
    const bool busy = snapshot.state == ApplicationRuntimeState::Starting
            || snapshot.state == ApplicationRuntimeState::Running
            || snapshot.state == ApplicationRuntimeState::Stopping;
    const bool connected = snapshot.resultExportConnectionState
            == ResultExportConnectionState::Connected;
    const bool connecting = snapshot.resultExportConnectionState
            == ResultExportConnectionState::Connecting;
    const bool checking = snapshot.resultExportConnectionState
            == ResultExportConnectionState::Checking;
    const bool syncing = snapshot.resultExportConnectionState
            == ResultExportConnectionState::Syncing;
    if (!connected) {
        m_resultExportAutoEnableApplied = false;
        m_resultExportUserEnabled = false;
    } else if (!m_resultExportAutoEnableApplied
               && snapshot.state != ApplicationRuntimeState::Fault
               && !snapshot.resultExportDispositionPending) {
        m_resultExportUserEnabled = true;
        m_resultExportAutoEnableApplied = true;
    }
    QString status = QStringLiteral("● 已断开");
    QString statusUiState = QStringLiteral("offline");
    if (!snapshot.resultExportStartupReady) {
        status = QStringLiteral("● 远程传输：本地队列不可用");
        statusUiState = QStringLiteral("error");
    } else if (snapshot.resultExportDispositionPending) {
        status = QStringLiteral("● 远程传输：%1 条历史结果待处理")
                .arg(snapshot.resultExportPendingCount);
        statusUiState = QStringLiteral("attention");
    } else if (connecting) {
        status = QStringLiteral("● 正在连接…");
        statusUiState = QStringLiteral("working");
    } else if (checking) {
        status = QStringLiteral("● 正在检查…");
        statusUiState = QStringLiteral("working");
    } else if (syncing) {
        status = QStringLiteral("● 正在同步历史结果…");
        statusUiState = QStringLiteral("working");
    } else if (connected) {
        status = snapshot.resultExportRoundTripMs >= 0.0
                ? QStringLiteral("● 已连接 · %1 ms")
                  .arg(snapshot.resultExportRoundTripMs, 0, 'f', 2)
                : QStringLiteral("● 已连接");
        statusUiState = QStringLiteral("online");
    }
    ui->resultExportStatus->setText(status);
    if (ui->resultExportStatus->property("uiState").toString()
            != statusUiState) {
        ui->resultExportStatus->setProperty("uiState", statusUiState);
        if (ui->resultExportStatus->style()) {
            ui->resultExportStatus->style()->unpolish(
                        ui->resultExportStatus);
            ui->resultExportStatus->style()->polish(
                        ui->resultExportStatus);
        }
        ui->resultExportStatus->update();
    }
    const bool canEdit = !busy && !connected && !connecting
            && !checking && !syncing;
    ui->resultExportIp->setEnabled(canEdit && snapshot.resultExportStartupReady);
    ui->resultExportPort->setEnabled(canEdit && snapshot.resultExportStartupReady);
    ui->resultExportConnect->setEnabled(!busy && snapshot.resultExportStartupReady
                                        && !snapshot.resultExportDispositionPending
                                        && !connecting && !checking && !syncing);
    ui->resultExportDisconnect->setEnabled(!busy
                                           && snapshot.resultExportConnectionState
                                           != ResultExportConnectionState::Disconnected);
    const bool enableAvailable = !busy
            && snapshot.state != ApplicationRuntimeState::Fault
            && connected
            && !snapshot.resultExportDispositionPending;
    {
        QSignalBlocker blocker(ui->resultExportEnable);
        ui->resultExportEnable->setChecked(m_resultExportUserEnabled
                                           && enableAvailable);
    }
    ui->resultExportEnable->setEnabled(enableAvailable);
}

void MainWindow::on_resultExportConnect_clicked()
{
    const RuntimeSnapshot snapshot = m_inspectionApplicationService->runtimeSnapshot();
    if (snapshot.resultExportConnectionState == ResultExportConnectionState::Connected) {
        m_inspectionApplicationService->requestResultExportConnectionCheck();
        return;
    }
    AppSettings candidate = m_settingsApplicationService->current();
    candidate.resultExportReceiverIp = ui->resultExportIp->text().trimmed();
    candidate.resultExportReceiverPort = ui->resultExportPort->value();
    const OperationResult saved = m_settingsApplicationService
            ->saveConfiguration(candidate);
    if (!saved.isSuccess()) {
        showParameterWarning(QStringLiteral("结果传输设置保存失败"),
                             saved.error.userMessage);
        return;
    }
    m_inspectionApplicationService->requestResultExportConnect(
                candidate.resultExportReceiverIp,
                static_cast<quint16>(candidate.resultExportReceiverPort));
}

void MainWindow::on_resultExportDisconnect_clicked()
{
    m_resultExportUserEnabled = false;
    m_inspectionApplicationService->requestResultExportDisconnect();
}

void MainWindow::on_resultExportEnable_toggled(bool enabled)
{
    m_resultExportUserEnabled = enabled;
}
