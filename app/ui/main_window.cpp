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
                        ? (ui->checkBox->isChecked()
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
    QScrollArea *detectionInfoScrollArea = new QScrollArea;
    detectionInfoScrollArea->setObjectName("detectionInfoScrollArea");
    detectionInfoScrollArea->setFrameShape(QFrame::NoFrame);
    detectionInfoScrollArea->setWidgetResizable(true);
    detectionInfoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detectionInfoScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detectionInfoScrollArea->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Expanding);
    detectionInfoScrollArea->setWidget(ui->groupBox1);
    ui->rightPanelSplitter->insertWidget(0, detectionInfoScrollArea);

    if (QSplitterHandle *rightPanelHandle =
            ui->rightPanelSplitter->handle(1)) {
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
        handleGrip->setObjectName("rightPanelSplitterGrip");
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
        ui->rightPanelSplitter->setProperty(
                    "visualHandleHeight",
                    splitterHandleHeight);
        ui->rightPanelSplitter->setHandleWidth(
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
                        || !ui->VideoShoot
                        || operationUiState()
                           != OperationState::TemplatePreviewing) {
            m_templateCaptureAttentionTimer->stop();
            m_templateCaptureAttentionOn = false;
        } else {
            m_templateCaptureAttentionOn =
                    !m_templateCaptureAttentionOn;
        }

        if (ui && ui->VideoShoot) {
            ui->VideoShoot->setProperty(
                        "templateCaptureActive",
                        operationUiState()
                        == OperationState::TemplatePreviewing);
            ui->VideoShoot->setProperty(
                        "templateCaptureAttention",
                        m_templateCaptureAttentionOn);
            ui->VideoShoot->style()->unpolish(
                        ui->VideoShoot);
            ui->VideoShoot->style()->polish(
                        ui->VideoShoot);
            ui->VideoShoot->update();
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
    imageLabel = ui->image_undetected;

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
            ui->statusLabel->setText("\u8bc6\u522b\u7ebf\u7a0b\u5df2\u505c\u6b62");
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
        if (ui->comboBox_4) {
            ui->comboBox_4->setEnabled(enabled);
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
    view.imageLabel = imageLabel;
    view.dateEdit = ui->dateEdit;
    view.lineEdit_yuzhi = ui->lineEdit_yuzhi;
    view.lineEdit_tissueRoughnessThreshold =
            ui->lineEdit_tissueRoughnessThreshold;
    view.comboBox_2 = ui->comboBox_2;
    view.comboBox_3 = ui->comboBox_3;
    view.comboBox_4 = ui->comboBox_4;
    view.comboBox_5 = ui->comboBox_5;
    view.currentTemplateName = ui->currentTemplateName;
    view.statusLabel = ui->statusLabel;
    view.label = ui->label;
    view.label_4 = ui->label_4;
    view.label_6 = ui->label_6;
    view.label_8 = ui->label_8;
    view.label_10 = ui->label_10;
    view.label_13 = ui->label_13;
    view.label_14 = ui->label_14;
    view.label_16 = ui->label_16;
    view.label_17 = ui->label_17;
    view.label_27 = ui->label_27;
    view.image_undetected = ui->image_undetected;
    view.imagedisplayBox = ui->imagedisplayBox;
    view.verticalLayout_InnerImg = ui->verticalLayout_InnerImg;
    view.manualCharacterCropButton = ui->manualCharacterCropButton;
    view.textsure_btn = ui->textsure_btn;
    view.batchTextsure_btn = ui->batchTextsure_btn;
    view.batchImageThresholdButton = ui->batchImageThresholdButton;
    view.WriteVDpushButton = ui->WriteVDpushButton;
    view.pushButton_3 = ui->pushButton_3;
    view.pushButton_7 = ui->pushButton_7;
    view.pushButton_8 = ui->pushButton_8;
    view.pushButton_9 = ui->pushButton_9;
    view.pushButton_10 = ui->pushButton_10;
    view.pushButton_12 = ui->pushButton_12;
    view.sureButton = ui->sureButton;
    view.pushButton_tissueRoughnessThreshold =
            ui->pushButton_tissueRoughnessThreshold;
    view.plcmodebtn = ui->plcmodebtn;
    view.ConnectpushButton = ui->ConnectpushButton;
    view.DisconnectpushButton = ui->DisconnectpushButton;
    view.pushButton_browseImageSavePath =
            ui->pushButton_browseImageSavePath;
    view.VideoShoot = ui->VideoShoot;
    view.checkBox = ui->checkBox;
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
    ui->dateEdit->setWordWrapMode(QTextOption::WordWrap);
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
        const QString targetIp = ui->lineEdit->text();
        PlcConnectionCommand command;
        command.address = targetIp;
        command.rack = ui->lineEdit_2->text().toInt();
        command.slot = ui->lineEdit_3->text().toInt();
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
