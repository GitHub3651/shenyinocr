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


#include <QTimer>
#include <QLabel>
#include <QString>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QFrame>
#include <QScrollArea>
#include <QSplitterHandle>
#include <QTextOption>
#include <QDebug>

#include <opencv2/highgui.hpp>

#pragma execution_character_set("utf-8")

/**
 * @brief MainWindow构造函数
 * @param parent 父窗口指针
 * @details 初始化UI、相机、OCR模型、定时器等核心组件
 */
MainWindow::MainWindow(
    const std::shared_ptr<InspectionApplicationService> &inspectionService,
    const std::shared_ptr<SettingsApplicationService> &settingsService,
    const std::shared_ptr<TemplateApplicationService> &templateService,
    QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MainWindow),
      m_inspectionApplicationService(inspectionService),
      m_settingsApplicationService(settingsService),
      m_appliedMachineSettings(
          m_settingsApplicationService->editableDraft()),
      m_templateApplicationService(templateService),
      imageLabel(nullptr)
{
    ui->setupUi(this);
    InspectionUiCallbacks resultCallbacks;
    resultCallbacks.warnMissingAnnotatedImage = [this]() {
        if (m_inspectionPage) {
            m_inspectionPage->warnMissingAnnotatedImage();
        }
    };
    resultCallbacks.reportImageSaveFailure = [this](
            quint64 totalFailed,
            const QString &latestError) {
        if (m_inspectionPage) {
            m_inspectionPage->reportImageSaveFailure(
                        totalFailed,
                        latestError);
        }
    };
    resultCallbacks.clearPreviousOverlay = [this](
        bool clearImageLabelRects) {
        if (clearImageLabelRects && imageLabel) {
            imageLabel->clearSelection();
        }
    };
    resultCallbacks.showDetectionRoiWarning = [this]() {
        if (m_inspectionPage) {
            m_inspectionPage->showDetectionRoiWarning();
        }
    };
    resultCallbacks.clearDetectionRoiWarning = [this]() {
        if (m_inspectionPage) {
            m_inspectionPage->clearDetectionRoiWarning(
                        operationUiState() == OperationState::Detecting
                        ? (ui->checkBox_hardwareTriggerEnabled->isChecked()
                           ? QString::fromWCharArray(
                               L"\u89e6\u53d1\u6a21\u5f0f\u8fd0\u884c\u4e2d")
                           : QString::fromWCharArray(
                               L"\u8f6f\u89e6\u53d1\u6a21\u5f0f\u8fd0\u884c\u4e2d"))
                        : QString());
        }
    };
    m_inspectionApplicationService->setUiCallbacks(resultCallbacks);

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
                        L"\u4e0a\u4e0b\u62d6\u52a8"
                        L"\u8c03\u6574\u533a\u57df\u9ad8\u5ea6"));

        QHBoxLayout *handleLayout =
                new QHBoxLayout(rightPanelHandle);
        handleLayout->setContentsMargins(0, 0, 0, 0);
        handleLayout->setSpacing(0);

        QLabel *handleGrip =
                new QLabel(QString::fromWCharArray(
                               L"\u2195  \u62d6\u52a8\u8c03\u6574"),
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
            ui->toolButton_createTemplate->setProperty(
                        "templateCaptureActive",
                        operationUiState()
                        == OperationState::TemplatePreviewing);
            ui->toolButton_createTemplate->setProperty(
                        "templateCaptureAttention",
                        m_templateCaptureAttentionOn);
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
            [this](const RuntimeSnapshot &) {
        updateOperationUiState();
        updateHardwareParameterUiEnabled();
    });

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
            &InspectionApplicationService::streamingFrameReady,
            this,
            [this](cv::Mat image) {
        if (image.empty() || m_resultBoundDisplayActive.load()) {
            return;
        }
        slot_displayAndDetect(&image);
    },
    Qt::QueuedConnection);
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
            m_resultBoundDisplayActive.store(false);
            m_barcodeWordRunActive = false;
            ui->label_runtimeStatus->setText("\u8bc6\u522b\u7ebf\u7a0b\u5df2\u505c\u6b62");
            updateOperationUiState();
        }
    },
    Qt::QueuedConnection);
    connect(m_inspectionApplicationService.get(),
            &InspectionApplicationService::faultEntered,
            this,
            [this]() {
        presentInspectionFault();
    },
    Qt::QueuedConnection);

    qDebug() << "MainWindow shell constructed";
}

Ui::MainWindow *MainWindow::viewForComposition() const
{
    return ui;
}

QTimer *MainWindow::templateAttentionTimerForComposition() const
{
    return m_templateCaptureAttentionTimer;
}

bool *MainWindow::templateAttentionFlagForComposition()
{
    return &m_templateCaptureAttentionOn;
}

SettingsEditState *MainWindow::settingsEditStateForComposition()
{
    return &m_settingsEditState;
}

QString *MainWindow::selectedDirectoryForComposition()
{
    return &selectedDir;
}

bool *MainWindow::applyingSettingsFlagForComposition()
{
    return &m_applyingMachineSettings;
}

bool *MainWindow::updatingSettingsUiFlagForComposition()
{
    return &m_updatingMachineSettingsUi;
}

InspectionPage::Callbacks MainWindow::inspectionPageCallbacks()
{
    InspectionPage::Callbacks callbacks;
    callbacks.setSettingsEnabled = [this](bool enabled) {
        if (m_machineSettingsPage) {
            m_machineSettingsPage->setAllEditorsEnabled(enabled);
        }
        if (ui->comboBox_detectionMode) {
            ui->comboBox_detectionMode->setEnabled(enabled);
        }
        if (m_templateEditorPage) {
            m_templateEditorPage->setEditorsEnabled(enabled);
        }
    };
    callbacks.refreshHardwareSettingsEnabled = [this]() {
        updateHardwareParameterUiEnabled();
    };
    callbacks.updateImageDisplayStatus = [this](const QString &text) {
        updateImageDisplayStatusText(text);
    };
    return callbacks;
}

MachineSettingsPage::Callbacks MainWindow::machineSettingsPageCallbacks()
{
    MachineSettingsPage::Callbacks callbacks;
    callbacks.saveSettings = [this](bool showErrorMessage) {
        return saveSettings(showErrorMessage);
    };
    callbacks.syncRecipeHistory = [this](MachineSettings *settings) {
        if (settings) {
            settings->publishedRecipeIdsByMode =
                    m_templateApplicationService->modeMemory()
                    .publishedRecipeIdsByMode();
        }
    };
    callbacks.updateImageSaveOptionsVisibility = [this]() {
        updateImageSaveOptionsVisibility();
    };
    callbacks.updateSaveDirectoryText = [this]() {
        updateSaveDirButtonText();
    };
    callbacks.updateTissueVisibility = [this]() {
        updateTissueRoughnessUiVisibility();
    };
    callbacks.updateOperationUiState = [this]() {
        updateOperationUiState();
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
    view.eventFilterTarget = const_cast<MainWindow *>(this);
    view.imageLabel_templateCanvas = imageLabel;
    view.textEdit_targetText = ui->textEdit_targetText;
    view.lineEdit_imageThreshold = ui->lineEdit_imageThreshold;
    view.lineEdit_tissueRoughnessThreshold =
            ui->lineEdit_tissueRoughnessThreshold;
    view.comboBox_imageRotation = ui->comboBox_imageRotation;
    view.comboBox_plcTriggerMode = ui->comboBox_plcTriggerMode;
    view.comboBox_detectionMode = ui->comboBox_detectionMode;
    view.comboBox_colorChannel = ui->comboBox_colorChannel;
    view.lineEdit_currentRecipeName = ui->lineEdit_currentRecipeName;
    view.label_runtimeStatus = ui->label_runtimeStatus;
    view.label_targetText = ui->label_targetText;
    view.label_imageThreshold = ui->label_imageThreshold;
    view.label_rejectDistance = ui->label_rejectDistance;
    view.label_photoDistance = ui->label_photoDistance;
    view.label_rejectTime = ui->label_rejectTime;
    view.label_hardwareTriggerDelay = ui->label_hardwareTriggerDelay;
    view.label_photoTime = ui->label_photoTime;
    view.label_cameraGain = ui->label_cameraGain;
    view.label_rejectPosition = ui->label_rejectPosition;
    view.label_imageRotation = ui->label_imageRotation;
    view.imageLabel_inspectionDisplay = ui->imageLabel_inspection;
    view.groupBox_imageDisplay = ui->groupBox_imageDisplay;
    view.verticalLayout_imageDisplay = ui->verticalLayout_imageDisplay;
    view.pushButton_editCharacterTemplates = ui->pushButton_editCharacterTemplates;
    view.pushButton_applyTargetText = ui->pushButton_applyTargetText;
    view.pushButton_applyBatchTargetText = ui->pushButton_applyBatchTargetText;
    view.pushButton_applyBatchImageThreshold = ui->pushButton_applyBatchImageThreshold;
    view.pushButton_applyPhotoDistance = ui->pushButton_applyPhotoDistance;
    view.pushButton_applyImageThreshold = ui->pushButton_applyImageThreshold;
    view.pushButton_applyColorChannel = ui->pushButton_applyColorChannel;
    view.pushButton_applyPlcProcessParameters = ui->pushButton_applyPlcProcessParameters;
    view.pushButton_applyImageRotation = ui->pushButton_applyImageRotation;
    view.pushButton_resetRejectQueue = ui->pushButton_resetRejectQueue;
    view.pushButton_applyCameraGain = ui->pushButton_applyCameraGain;
    view.pushButton_applyCameraExposure = ui->pushButton_applyCameraExposure;
    view.pushButton_applyTissueRoughnessThreshold =
            ui->pushButton_applyTissueRoughnessThreshold;
    view.pushButton_applyPlcTriggerMode = ui->pushButton_applyPlcTriggerMode;
    view.pushButton_connectPlc = ui->pushButton_connectPlc;
    view.pushButton_disconnectPlc = ui->pushButton_disconnectPlc;
    view.pushButton_browseImageSavePath =
            ui->pushButton_browseImageSavePath;
    view.toolButton_createTemplate = ui->toolButton_createTemplate;
    view.checkBox_hardwareTriggerEnabled = ui->checkBox_hardwareTriggerEnabled;
    return view;
}

TemplateEditorPageCallbacks MainWindow::templateEditorPageCallbacks()
{
    TemplateEditorPageCallbacks callbacks;
    callbacks.updateOperationUiState = [this]() {
        updateOperationUiState();
    };
    callbacks.updateTissueVisibility = [this]() {
        updateTissueRoughnessUiVisibility();
    };
    callbacks.clearTransientView = [this]() {
        m_inspectionApplicationService->clearTransientView();
    };
    callbacks.displayPreviewFrame = [this](const cv::Mat &image) {
        cv::Mat displayImage = image.clone();
        slot_displayAndDetect(&displayImage);
    };
    callbacks.isApplyingSettings = [this]() {
        return m_applyingMachineSettings;
    };
    callbacks.isUpdatingSettingsUi = [this]() {
        return m_updatingMachineSettingsUi;
    };
    callbacks.saveSettings = [this](bool showErrorMessage) {
        return saveSettings(showErrorMessage);
    };
    callbacks.applyRecipeProfileToUi = [this](
            const RecipeProfile &profile) {
        applyRecipeProfileToUi(profile);
    };
    return callbacks;
}

void MainWindow::attachPages(
    InspectionPage *inspectionPage,
    MachineSettingsPage *machineSettingsPage,
    TemplateEditorPage *templateEditorPage)
{
    if (!inspectionPage || !machineSettingsPage || !templateEditorPage) {
        qFatal("MainWindow requires all pages");
    }
    m_inspectionPage = inspectionPage;
    m_machineSettingsPage = machineSettingsPage;
    m_templateEditorPage = templateEditorPage;
    m_inspectionApplicationService->bindView(
                m_inspectionPage->resultViewBindings());
    m_inspectionApplicationService->resetStatistics();
    ui->textEdit_targetText->setWordWrapMode(QTextOption::WordWrap);
    setupRecipeProfileDirtyTracking();
    setupWordTemplateEditorCombo();
    setupTemplateGuide();
    setupManualCharacterCropUi();
    setupSoftwareSettingsPage();
    m_machineSettingsPage->installWheelProtection(this);
    connect(imageLabel, &ImageLabel::signal_templateGuideEvent,
            this, &MainWindow::handleTemplateGuideEvent);

    m_machineSettingsPage->setupNumericInputValidators();
    setupNonPersistentDefaults();
    m_machineSettingsPage->initialize(
                m_settingsApplicationService->current());
    m_templateApplicationService->replacePublishedRecipeIdsByMode(
                m_appliedMachineSettings.publishedRecipeIdsByMode);
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
    setupDetectModeChangeTracking();
    m_machineSettingsPage->setupBindings();
    m_machineSettingsPage->clearAllDirty();
    clearRecipeProfileDirty();
    updateOperationUiState();
    updateCurrentTemplateName();

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
            m_machineSettingsPage->updateAppliedFromUi(connectionKeys);
            m_machineSettingsPage->refreshDirty(connectionKeys);
            saveSettings(false);
            QMessageBox::information(this, "提示", "PLC 自动连接成功");
        } else {
            const QString errorMessage = QString(
                        "PLC 自动连接失败！\n尝试连接的地址：%1\n"
                        "请检查网络或稍后手动连接！").arg(targetIp);
            QMessageBox::warning(this, "警告", errorMessage);
        }
        updateHardwareParameterUiEnabled();
    });
}

/**
 * @brief MainWindow析构函数
 * @details 清理所有资源，关闭相机、停止线程、删除临时文件
 */
MainWindow::~MainWindow()
{
    qDebug() << "MainWindow destructor called";

    resetTemplateCaptureState();
    m_inspectionApplicationService->clearUiBindings();
    m_inspectionApplicationService->shutdown();

    try {
        cv::destroyAllWindows();
    } catch (...) {}
    delete ui;
    ui = nullptr;

    qDebug() << "MainWindow destroyed";
}
