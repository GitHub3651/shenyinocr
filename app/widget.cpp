/**
 * @file widget.cpp
 * @brief 工业视觉识别系统主窗口实现文件
 * @details 实现图像采集、OCR识别、模板匹配、PLC通信等核心功能
 * @author 优化版本
 * @date 2024
 */

#include "widget.h"
#include "runtime/inspection_acquisition_controller.h"
#include "runtime/inspection_runtime_start_transaction.h"
#include "runtime/inspection_runtime_stop_transaction.h"
#include "ui_widget.h"
#include "multicamerawidget.h"
#include "charactertemplatecropdialog.h"
#include "DetectionModes.h"
#include "ui/controllers/inspection_result_coordinator.h"
#include "ui/controllers/inspection_runtime_ui_coordinator.h"
#include "ui/controllers/inspection_start_controller.h"
#include "ui/controllers/inspection_stop_controller.h"
#include "ui/controllers/machine_settings_page_controller.h"
#include "ui/controllers/template_editor_controller.h"


// Qt核心组件
#include <QTimer>
#include <QThread>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QAbstractItemView>
#include <QImageReader>
#include <QLabel>
#include <QFontMetrics>
#include <QListView>
#include <QLineEdit>
#include <QInputDialog>
#include <QMetaType>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QPixmap>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QDateTime>
#include <QApplication>
#include <QTranslator>
#include <QIcon>
#include <QCamera>
#include <QCameraInfo>
#include <QDesktopWidget>
#include <QSplashScreen>
#include <QTextCodec>
#include <QDir>
#include <QStandardPaths>
#include <QTreeView>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolTip>
#include <QWhatsThis>
#include <QCursor>
#include <QFrame>
#include <QTextEdit>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QEvent>
#include <QEventLoop>
#include <QRegularExpression>
#include <QSplitterHandle>

// Qt串口和SQL
#include <QtSerialPort/QtSerialPort>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QVariantList>
#include <QtSql/QSqlDatabase>

// 标准库
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <cmath>

#pragma execution_character_set("utf-8")
using namespace std;

namespace {
bool sameProductKey(const ProductKey &left, const ProductKey &right)
{
    return left.runId == right.runId
            && left.sequence == right.sequence;
}

void appendUniqueProductKey(
    std::vector<ProductKey> *productKeys,
    const ProductKey &productKey)
{
    if (!productKeys || !productKey.isValid()) {
        return;
    }
    const std::vector<ProductKey>::const_iterator existing =
            std::find_if(
                productKeys->cbegin(),
                productKeys->cend(),
                [&productKey](const ProductKey &candidate) {
        return sameProductKey(candidate, productKey);
    });
    if (existing == productKeys->cend()) {
        productKeys->push_back(productKey);
    }
}

bool parseIntValue(const QString &text, int *value)
{
    bool ok = false;
    const int parsed = text.trimmed().toInt(&ok);
    if (!ok) {
        return false;
    }
    if (value) {
        *value = parsed;
    }
    return true;
}

bool isSingleTemplateRecipeMode(const QString &modeId)
{
    DetectionMode mode;
    return detectionModeFromUiId(modeId, &mode)
            && (mode == DetectionMode::Stamp
                || mode == DetectionMode::Ocr);
}

}


/**
 * @brief Widget构造函数
 * @param parent 父窗口指针
 * @details 初始化UI、相机、OCR模型、定时器等核心组件
 */
Widget::Widget(
    const std::shared_ptr<ICameraDevice> &cameraDevice,
    const std::shared_ptr<InspectionPlcController> &plcController,
    const OcrEngineFactory &ocrEngineFactory,
    const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
    const MachineSettings &startupSettings,
    const std::shared_ptr<MachineSettingsStore> &settingsStore,
    const std::shared_ptr<RecipeStore> &recipeStore,
    QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget),
      m_appliedMachineSettings(startupSettings),
      m_machineSettingsStore(settingsStore),
      m_recipeStore(recipeStore),
      m_runtimeController(
          InspectionRuntimeController::RunIdFactory(),
          plcController),
      timer(new QTimer(this)),
      timer1(new QTimer(this)),
      imageIndex(0),
      color(1),
      templatematch(nullptr),
      tracking(true),
      zhuizong(nullptr),
      first(false),
      savefirst(false),
      imageLabel(nullptr)
{
    m_barcodeDecoder = barcodeDecoder;
    ui->setupUi(this);
    InspectionResultCoordinatorCallbacks resultCallbacks;
    resultCallbacks.requestPlc = [this](
            DetectionPlcAction action,
            const ProductKey &productKey) {
        m_activePlcOutputProductKey = productKey;
        if (action == DetectionPlcAction::RequestOk) {
            rightremove();
        } else if (action == DetectionPlcAction::RequestNg) {
            wrongremove();
        }
        m_activePlcOutputProductKey = ProductKey();
    };
    resultCallbacks.warnMissingAnnotatedImage = [this]() {
        if (m_runtimeUiCoordinator) {
            m_runtimeUiCoordinator->warnMissingAnnotatedImage();
        }
    };
    resultCallbacks.reportImageSaveFailure = [this](
            quint64 totalFailed,
            const QString &latestError) {
        if (m_runtimeUiCoordinator) {
            m_runtimeUiCoordinator->reportImageSaveFailure(
                        totalFailed,
                        latestError);
        }
    };
    resultCallbacks.clearLegacyPresentationState = [this](
            bool clearImageLabelRects) {
        if (clearImageLabelRects && imageLabel) {
            imageLabel->clearGreenRects();
        }
        detectedRects.clear();
        string1.clear();
    };
    resultCallbacks.storeLegacyRecognitionText = [this](
            const QString &text) {
        allResults = text.toStdString();
    };
    resultCallbacks.showDetectionRoiWarning = [this]() {
        if (m_runtimeUiCoordinator) {
            m_runtimeUiCoordinator->showDetectionRoiWarning();
        }
    };
    resultCallbacks.clearDetectionRoiWarning = [this]() {
        if (m_runtimeUiCoordinator) {
            m_runtimeUiCoordinator->clearDetectionRoiWarning(
                        m_operationState == OperationState::Detecting
                        ? (ui->checkBox->isChecked()
                           ? QString::fromWCharArray(
                               L"\u89e6\u53d1\u6a21\u5f0f\u8fd0\u884c\u4e2d")
                           : QString::fromWCharArray(
                               L"\u8f6f\u89e6\u53d1\u6a21\u5f0f\u8fd0\u884c\u4e2d"))
                        : QString());
        }
    };
    m_resultCoordinator.reset(
                new InspectionResultCoordinator(
                    &m_runtimeController,
                    resultCallbacks,
                    this));

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
                || m_operationState
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
                        m_operationState
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

    InspectionRuntimeUiCoordinator::Callbacks runtimeUiCallbacks;
    runtimeUiCallbacks.setSettingsEnabled = [this](bool enabled) {
        if (m_settingsPageController) {
            m_settingsPageController->setAllEditorsEnabled(enabled);
        }
        if (ui->comboBox_4) {
            ui->comboBox_4->setEnabled(enabled);
        }
        if (m_templateEditorController) {
            m_templateEditorController->setEditorsEnabled(enabled);
        }
    };
    runtimeUiCallbacks.refreshHardwareSettingsEnabled = [this]() {
        updateHardwareParameterUiEnabled();
    };
    runtimeUiCallbacks.updateImageDisplayStatus = [this](
            const QString &text) {
        updateImageDisplayStatusText(text);
    };
    m_runtimeUiCoordinator.reset(
                new InspectionRuntimeUiCoordinator(
                    this,
                    ui,
                    m_multiCameraWidget,
                    m_templateCaptureAttentionTimer,
                    &m_templateCaptureAttentionOn,
                    runtimeUiCallbacks));

    // UI 文件中已经是 ImageLabel，直接使用
    imageLabel=ui->image_undetected;
    m_templateEditorController.reset(
                new TemplateEditorController(
                    this,
                    ui,
                    imageLabel,
                    barcodeDecoder,
                    this));
    m_resultCoordinator->bindView(
                m_runtimeUiCoordinator->resultViewBindings());

    // 注册Qt元类型，用于跨线程信号传递
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<cv::Mat *>("cv::Mat*");
    qRegisterMetaType<cv::Rect2d>("cv::Rect2d");
    qRegisterMetaType<std::vector<cv::Point>>("std::vector<cv::Point>");
    qRegisterMetaType<DetectionPose>("DetectionPose");
    qRegisterMetaType<TissueRollResult>("TissueRollResult");
    qRegisterMetaType<QString>("QString");

    // 初始化追踪对象（使用智能指针）
    unique_ptr<Zhuizong> zhuizong = make_unique<Zhuizong>();

    m_plcHealthTimer = new QTimer(this);
    m_plcHealthTimer->setInterval(500);
    connect(m_plcHealthTimer,
            &QTimer::timeout,
            this,
            &Widget::checkInspectionPlcHealth);
    m_plcHealthTimer->start();
    InspectionAcquisitionCallbacks acquisitionCallbacks;
    acquisitionCallbacks.suppressStreamingFrame = [this]() {
        return m_resultBoundDisplayActive.load();
    };
    acquisitionCallbacks.presentStreamingFrame = [this](
            const cv::Mat &image) {
        if (image.empty() || m_resultBoundDisplayActive.load()) {
            return;
        }
        cv::Mat displayFrame = image;
        slot_displayAndDetect(&displayFrame);
    };
    acquisitionCallbacks.presentTrackingPose = [this](
            const DetectionPose &pose) {
        slot_saveBoxesFromThread(pose);
    };
    acquisitionCallbacks.clearResultText = [this]() {
        slot_clearResultLabel();
    };
    acquisitionCallbacks.presentTemplatePreview = [this](
            quint64 sessionId,
            const cv::Mat &image) {
        if (m_templateCaptureState
                != TemplateCaptureState::Previewing
                || sessionId != m_templatePreviewSessionId
                || image.empty()) {
            return;
        }
        m_lastTemplatePreviewFrame = image.clone();
        slot_displayAndDetect(&m_lastTemplatePreviewFrame);
        updateImageDisplayStatusText(
                    "\u5b9e\u65f6\u53d6\u666f\u4e2d\uff0c\u8bf7\u8c03\u6574\u4ea7\u54c1\u4f4d\u7f6e\uff0c"
                    "\u786e\u8ba4\u540e\u70b9\u51fb\u3010\u62cd\u7167\u5e76\u5f00\u59cb\u6846\u9009\u3011\u3002");
    };
    acquisitionCallbacks.reportTemplatePreviewError = [this](
            quint64 sessionId,
            const QString &reason) {
        if (m_templateCaptureState
                != TemplateCaptureState::Previewing
                || sessionId != m_templatePreviewSessionId) {
            return;
        }
        resetTemplateCaptureState();
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        ui->statusLabel->setText(
                    m_bOpenDevice
                    ? "\u6a21\u677f\u5b9e\u65f6\u53d6\u666f\u5931\u8d25\uff0c\u76f8\u673a\u5df2\u6253\u5f00"
                    : "\u6a21\u677f\u5b9e\u65f6\u53d6\u666f\u5931\u8d25\uff0c\u76f8\u673a\u5df2\u5173\u95ed");
        updateImageDisplayStatusText(
                    "\u5b9e\u65f6\u53d6\u666f\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u76f8\u673a\u540e\u91cd\u8bd5\u3002");
        QMessageBox::warning(
                    this,
                    "\u5b9e\u65f6\u53d6\u666f\u5931\u8d25",
                    reason);
    };
    acquisitionCallbacks.softwareThreadFinished = [this]() {
        if (m_templateCaptureState
                == TemplateCaptureState::Previewing) {
            ++m_templatePreviewSessionId;
            m_templateCaptureState = TemplateCaptureState::Idle;
            m_lastTemplatePreviewFrame.release();
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            ui->statusLabel->setText(
                        m_bOpenDevice
                        ? "\u6a21\u677f\u5b9e\u65f6\u53d6\u666f\u5df2\u505c\u6b62\uff0c\u76f8\u673a\u5df2\u6253\u5f00"
                        : "\u6a21\u677f\u5b9e\u65f6\u53d6\u666f\u5df2\u505c\u6b62\uff0c\u76f8\u673a\u5df2\u5173\u95ed");
            updateOperationUiState();
            return;
        }
        if (m_operationState == OperationState::Detecting) {
            InspectionRuntimeStopTransaction stopTransaction(
                        m_runtimeController);
            stopTransaction.begin();
            stopTransaction.commit();
            isCollecting = false;
            m_resultBoundDisplayActive.store(false);
            m_barcodeWordRunActive = false;
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            ui->statusLabel->setText("\u8bc6\u522b\u7ebf\u7a0b\u5df2\u505c\u6b62");
            updateOperationUiState();
        }
    };
    acquisitionCallbacks.hardwareThreadFinished = [this]() {
        if (m_operationState != OperationState::Detecting) {
            return;
        }
        InspectionRuntimeStopTransaction stopTransaction(
                    m_runtimeController);
        stopTransaction.begin();
        stopTransaction.waitForDetectionWorker();
        isCollecting = false;
        m_resultBoundDisplayActive.store(false);
        m_barcodeWordRunActive = false;
        stopTransaction.commit();
        m_operationState = m_bOpenDevice
                ? OperationState::CameraReady
                : OperationState::CameraClosed;
        ui->statusLabel->setText("\u8bc6\u522b\u7ebf\u7a0b\u5df2\u505c\u6b62");
        updateOperationUiState();
    };
    acquisitionCallbacks.enterFault = [this](
            InspectionFaultReason reason,
            const QString &diagnostic) {
        enterInspectionFault(reason, diagnostic);
    };
    m_acquisitionController.reset(
                new InspectionAcquisitionController(
                    cameraDevice,
                    &m_runtimeController,
                    acquisitionCallbacks,
                    this));

    // 初始化窗口组件
    initWidget();
    qDebug() << "1. initWidget执行完毕 ";

    MachineSettingsPageController::Callbacks settingsCallbacks;
    settingsCallbacks.saveSettings = [this](bool showErrorMessage) {
        return saveSettings(showErrorMessage);
    };
    settingsCallbacks.syncRecipeHistory = [this](
            MachineSettings *settings) {
        if (!settings) {
            return;
        }
        settings->publishedRecipeIdsByMode =
                m_templateEditorController->modeMemory()
                .publishedRecipeIdsByMode();
    };
    settingsCallbacks.updateImageSaveOptionsVisibility = [this]() {
        updateImageSaveOptionsVisibility();
    };
    settingsCallbacks.updateSaveDirectoryText = [this]() {
        updateSaveDirButtonText();
    };
    settingsCallbacks.updateTissueVisibility = [this]() {
        updateTissueRoughnessUiVisibility();
    };
    settingsCallbacks.updateOperationUiState = [this]() {
        updateOperationUiState();
    };
    settingsCallbacks.reportDirectoryOpenFailure = [this](
            const QString &path) {
        showParameterWarning(
                    "提示",
                    QString("无法打开软件数据文件夹：\n%1").arg(path));
    };
    m_settingsPageController.reset(
                new MachineSettingsPageController(
                    ui,
                    &m_appliedMachineSettings,
                    m_machineSettingsStore.get(),
                    &m_settingsEditState,
                    &selectedDir,
                    &m_applyingMachineSettings,
                    &m_updatingMachineSettingsUi,
                    settingsCallbacks));
    m_templateEditorController->bindRuntimeDependencies(
                m_acquisitionController.get(),
                m_settingsPageController.get());

    m_ocrEngine = ocrEngineFactory
            ? ocrEngineFactory()
            : std::shared_ptr<IOcrEngine>();

    // 初始化统计变量
    hasValidBoxes = false;
    savedTrackingBox = cv::Rect2d(0, 0, 0, 0);
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    recognitionCompletedFlag = false;
    isCollecting = false;
    m_runtimeController.resetStatistics();
    allResults = "";
    wrongindex = ui->lineEdit_12->text().toInt();

    // 设置文本框自动换行
    ui->dateEdit->setWordWrapMode(QTextOption::WordWrap);
    setupRecipeProfileDirtyTracking();
    setupWordTemplateEditorCombo();
    setupTemplateGuide();
    setupManualCharacterCropUi();
    setupSoftwareSettingsPage();

    m_settingsPageController->installWheelProtection(this);

    // 连接定时器信号
    connect(timer, &QTimer::timeout, this, &Widget::rightremove);
    connect(imageLabel, &ImageLabel::signal_templateGuideEvent,
            this, &Widget::handleTemplateGuideEvent);

    qDebug() << "6. 变量初始化与信号连接完毕";

    // 机器配置的唯一默认值由 MachineSettings::defaults() 提供。
    m_settingsPageController->setupNumericInputValidators();
    setupNonPersistentDefaults();
    qDebug() << "7. setupNonPersistentDefaults 执行完毕";

    m_settingsPageController->initialize(startupSettings);
    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            m_appliedMachineSettings.publishedRecipeIdsByMode;
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
    qDebug() << "8. MachineSettings 快照已应用";

    setupDetectModeChangeTracking();
    m_settingsPageController->setupBindings();
    m_settingsPageController->clearAllDirty();
    clearRecipeProfileDirty();
    updateOperationUiState();

    updateCurrentTemplateName();

    QTimer::singleShot(1000, this, [this]() {
        // 1. 先把从界面获取的文本存为一个 QString 变量
        QString targetIp = ui->lineEdit->text();

        const PlcOperationResult result = m_runtimeController.connectPlc(
                    targetIp,
                    ui->lineEdit_2->text().toInt(),
                    ui->lineEdit_3->text().toInt());
        if (result.isSuccess()) {
            m_settingsPageController->updateAppliedFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
            m_settingsPageController->refreshDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
            saveSettings(false);
            QMessageBox::information(this, "提示", "PLC 自动连接成功");
        } else {
            // 2. 使用 QString::arg() 动态拼接字符串
            // %1 会被替换为 targetIp 的真实内容
            QString errorMsg = QString("PLC 自动连接失败！\n尝试连接的地址：%1\n请检查网络或稍后手动连接！").arg(targetIp);

            QMessageBox::warning(this, "警告", errorMsg);
        }
        updateHardwareParameterUiEnabled();
    });


    qDebug() << "9. Widget构造结束，产品模板等待用户选择";
}

/**
 * @brief Widget析构函数
 * @details 清理所有资源，关闭相机、停止线程、删除临时文件
 */
Widget::~Widget()
{
    qDebug() << "Widget destructor called";

    m_runtimeController.requestStop();
    resetTemplateCaptureState();

    if (m_acquisitionController) {
        m_acquisitionController->shutdown(3000);
    }
    m_runtimeController.waitForDetectionWorkerStop();
    m_bOpenDevice = false;

    if (m_runtimeController.isPlcConnected()) {
        m_runtimeController.disconnectPlc();
    }

    delete templatematch;
    templatematch = nullptr;

    try {
        cv::destroyAllWindows();
    } catch (...) {}

    if (m_resultCoordinator) {
        m_resultCoordinator->shutdown();
        m_resultCoordinator.reset();
    }

    m_runtimeController.finishStop();

    delete ui;
    ui = nullptr;

    qDebug() << "Widget destroyed";
}
/**
 * @brief 初始化Widget组件
 * @details 初始化图像对象并连接界面信号槽
 */
void Widget::initWidget()
{
    // 初始化设备打开标志
    m_bOpenDevice = false;

    // 创建模板匹配对象
    templatematch = new TemplateMatch();
    connect(this, &Widget::imgshibie, templatematch, &TemplateMatch::receshibie);
    connect(this, &Widget::ssim, templatematch, &TemplateMatch::ssimvalue);
    connect(this,
            &Widget::jiancestring,
            templatematch,
            &TemplateMatch::jianceshibiestr);
}

/**
 * @brief QString转换为std::string
 * @param qstr 输入的QString
 * @return std::string 转换后的标准字符串
 */
string Widget::qstr2str(const QString qstr)
{
    QByteArray cdata = qstr.toLocal8Bit();
    return std::string(cdata);
}

/**
 * @brief 正确剔除操作（发送信号给PLC）
 * @details 向PLC写入0值，表示产品合格，停止定时器
 */
void Widget::rightremove()
{
    if (!m_runtimeController.isPlcConnected())
    {
        enterInspectionFault(
                    InspectionFaultReason::PlcDisconnected,
                     QStringLiteral(
                         "PLC OK/\u590d\u4f4d\u8f93\u51fa\u524d\u68c0\u6d4b\u5230\u8fde\u63a5\u5df2\u65ad\u5f00\u3002"));
        recordFaultedPlcOutput(m_activePlcOutputProductKey);
        for (const ProductKey &productKey
             : m_pendingPlcResetProductKeys) {
            recordFaultedPlcOutput(productKey);
        }
        timer->stop();
        return;
    }

    const PlcOperationResult result =
            m_runtimeController.writePlcResultValue(0);
    if (!result.isSuccess())
    {
        const QString diagnostic = QStringLiteral(
                    "PLC OK/\u590d\u4f4d\u8f93\u51fa\u5931\u8d25\uff0c\u9519\u8bef\u7801 %1\u3002")
                .arg(result.nativeErrorCode);
        if (!m_runtimeController.isRunning()) {
            QMessageBox::warning(this, "error", diagnostic);
        }
        enterInspectionFault(
                    InspectionFaultReason::PlcDisconnected,
                    diagnostic);
        recordFaultedPlcOutput(m_activePlcOutputProductKey);
        for (const ProductKey &productKey
             : m_pendingPlcResetProductKeys) {
            recordFaultedPlcOutput(productKey);
        }
    } else {
        m_pendingPlcResetProductKeys.clear();
    }
    timer->stop();
}

/**
 * @brief 错误剔除操作（发送信号给PLC）
 * @details 向PLC写入49值，表示产品不合格，需要剔除，100ms后恢复
 */
void Widget::wrongremove()
{
    if (!m_runtimeController.isPlcConnected())
    {
        enterInspectionFault(
                    InspectionFaultReason::PlcDisconnected,
                    QStringLiteral(
                        "PLC NG\u8f93\u51fa\u524d\u68c0\u6d4b\u5230\u8fde\u63a5\u5df2\u65ad\u5f00\u3002"));
        return;
    }

    const PlcOperationResult result =
            m_runtimeController.writePlcResultValue(49);
    if (!result.isSuccess())
    {
        const QString diagnostic = QStringLiteral(
                    "PLC NG\u8f93\u51fa\u5931\u8d25\uff0c\u9519\u8bef\u7801 %1\u3002")
                .arg(result.nativeErrorCode);
        if (!m_runtimeController.isRunning()) {
            QMessageBox::warning(this, "error", diagnostic);
        }
        enterInspectionFault(
                    InspectionFaultReason::PlcDisconnected,
                    diagnostic);
        recordFaultedPlcOutput(m_activePlcOutputProductKey);
    }
    else
    {
        appendUniqueProductKey(
                    &m_pendingPlcResetProductKeys,
                    m_activePlcOutputProductKey);
        // 100ms后调用rightremove恢复信号
        timer->start(100);
    }
}

void Widget::enterInspectionFault(
        InspectionFaultReason reason,
        const QString &diagnostic)
{
    if (!m_runtimeController.enterFault(reason, diagnostic)) {
        return;
    }

    qCritical() << "[INSPECTION_FAULT] entered"
                << static_cast<int>(reason)
                << diagnostic;
    QMetaObject::invokeMethod(
                this,
                [this]() {
        presentInspectionFault();
    },
    Qt::QueuedConnection);
}

void Widget::presentInspectionFault()
{
    if (!m_runtimeUiCoordinator) {
        return;
    }
    m_resultBoundDisplayActive.store(true);
    m_operationState = OperationState::Fault;
    updateOperationUiState();
    m_runtimeUiCoordinator->presentFault(
                m_runtimeController.faultSnapshot(),
                &m_faultAlarmPresented);
}

bool Widget::confirmInspectionFaultRecovery()
{
    return m_runtimeUiCoordinator
            && m_runtimeUiCoordinator->confirmFaultRecovery(
                m_runtimeController.faultSnapshot());
}

bool Widget::writeInspectionPlcOutput(
    std::uint8_t value,
    QString *errorMessage)
{
    if (!m_runtimeController.isPlcConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "PLC is disconnected before DB1.DBB1033 write.");
        }
        return false;
    }

    const PlcOperationResult result =
            m_runtimeController.writePlcResultValue(value);
    if (!result.isSuccess()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "PLC DB1.DBB1033 write value %1 failed, code %2.")
                    .arg(static_cast<int>(value))
                    .arg(result.nativeErrorCode);
        }
        return false;
    }
    return true;
}

void Widget::recordFaultedPlcOutput(
    const ProductKey &productKey)
{
    if (!productKey.isValid()
            || !m_runtimeController.recordFaultedPlcOutput(
                productKey)) {
        return;
    }

    qCritical() << "[INSPECTION_FAULT] recorded PLC output as unconfirmed"
                << productKey.runId
                << productKey.sequence;
}

bool Widget::requestFaultFallbackNgPulse(
    const ProductKey &productKey,
    QString *errorMessage)
{
    if (!writeInspectionPlcOutput(49, errorMessage)) {
        return false;
    }

    appendUniqueProductKey(
                &m_pendingPlcResetProductKeys,
                productKey);
    QEventLoop pulseWait;
    QTimer::singleShot(100, &pulseWait, &QEventLoop::quit);
    pulseWait.exec(QEventLoop::ExcludeUserInputEvents);

    if (!writeInspectionPlcOutput(0, errorMessage)) {
        return false;
    }
    m_pendingPlcResetProductKeys.clear();
    return true;
}

bool Widget::reconcileInspectionFaultProducts(
    QString *summary,
    QString *errorMessage)
{
    QStringList warnings;

    if (timer) {
        timer->stop();
    }
    if (!m_runtimeController.isPlcConnected()) {
        QString plcIp = m_appliedMachineSettings.plcIp.trimmed();
        if (plcIp.isEmpty() && ui) {
            plcIp = ui->lineEdit->text().trimmed();
        }
        const int rack = m_appliedMachineSettings.plcRack;
        const int slot = m_appliedMachineSettings.plcSlot;
        const PlcOperationResult reconnectResult =
                m_runtimeController.connectPlc(
                    plcIp,
                    rack,
                    slot);
        if (!reconnectResult.isSuccess()) {
            warnings.append(QStringLiteral(
                                "PLC reconnect failed, code %1.")
                            .arg(reconnectResult.nativeErrorCode));
        }
    }

    if (!m_pendingPlcResetProductKeys.empty()) {
        QString resetError;
        if (!writeInspectionPlcOutput(0, &resetError)) {
            for (const ProductKey &productKey
                 : m_pendingPlcResetProductKeys) {
                recordFaultedPlcOutput(productKey);
            }
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "\u6545\u969c\u6062\u590d\u524d PLC \u590d\u4f4d 0 \u5199\u5165\u5931\u8d25\uff0c"
                            "\u7cfb\u7edf\u7ee7\u7eed\u4fdd\u6301 Fault\u3002\n%1")
                        .arg(resetError);
            }
            return false;
        }
        m_pendingPlcResetProductKeys.clear();
    }

    const bool plcWritable =
            m_runtimeController.isPlcConnected();
    const std::vector<InspectionFaultProductAction> actions =
            m_runtimeController.faultProductActions(plcWritable);
    for (const InspectionFaultProductAction &action : actions) {
        if (action.type
                == InspectionFaultProductActionType::RequestFallbackNg) {
            QString pulseError;
            if (requestFaultFallbackNgPulse(
                        action.productKey,
                        &pulseError)) {
                if (!m_runtimeController.resolveFaultProduct(
                            action.productKey,
                            InspectionFaultProductResolution::FallbackNgRequested)) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral(
                                    "Fault product reconciliation state changed unexpectedly.");
                    }
                    return false;
                }
                continue;
            }

            if (!m_runtimeController.resolveFaultProduct(
                        action.productKey,
                        InspectionFaultProductResolution::Unconfirmed)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                                "Fault product could not be marked unconfirmed.");
                }
                return false;
            }
            warnings.append(pulseError);
            if (!m_pendingPlcResetProductKeys.empty()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                                "\u6545\u969c\u515c\u5e95 NG \u5df2\u5199\u5165 49\uff0c\u4f46\u590d\u4f4d 0 \u5931\u8d25\u3002"
                                "\u4ea7\u54c1\u5df2\u8bb0\u4e3a\u672a\u786e\u8ba4\uff0c\u7cfb\u7edf\u7ee7\u7eed\u4fdd\u6301 Fault\u3002\n%1")
                            .arg(pulseError);
                }
                return false;
            }
            continue;
        }

        if (!m_runtimeController.resolveFaultProduct(
                    action.productKey,
                    InspectionFaultProductResolution::Unconfirmed)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "Fault product could not be marked unconfirmed.");
            }
            return false;
        }
    }

    const int fallbackNgRequested =
            m_runtimeController.faultFallbackNgResolutionCount();
    const int unconfirmedProducts =
            m_runtimeController.faultUnconfirmedProductCount();

    if (summary) {
        *summary = QStringLiteral(
                    "\u672c\u6b21\u6545\u969c\u4ea7\u54c1\u6536\u53e3\uff1a"
                    "\u515c\u5e95 NG \u8bf7\u6c42 %1 \u4ef6\uff0c"
                    "\u672a\u786e\u8ba4 %2 \u4ef6\u3002\n"
                    "\u672a\u786e\u8ba4\u4ea7\u54c1\u4e0d\u8fdb\u5165\u6b63\u5e38\u603b\u6570\u3001NG\u6570\u548c\u5408\u683c\u7387\uff0c"
                    "\u8bf7\u6309\u73b0\u573a\u6d41\u7a0b\u9694\u79bb\u3002")
                .arg(fallbackNgRequested)
                .arg(unconfirmedProducts);
        if (!warnings.isEmpty()) {
            *summary += QStringLiteral("\n\nPLC details:\n")
                    + warnings.join(QStringLiteral("\n"));
        }
    }
    qWarning() << "[INSPECTION_FAULT] product reconciliation completed"
               << "fallbackNgRequests=" << fallbackNgRequested
               << "unconfirmed=" << unconfirmedProducts;
    return true;
}

void Widget::checkInspectionPlcHealth()
{
    if (!m_runtimeController.isRunning()) {
        return;
    }
    if (!m_resultCoordinator
            || !m_resultCoordinator->requiresPlcForRun()) {
        return;
    }
    if (m_runtimeController.isPlcConnected()) {
        return;
    }

    enterInspectionFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral(
                    "\u8fd0\u884c\u4e2d PLC \u8fde\u63a5\u72b6\u6001\u5df2\u65ad\u5f00\u3002"));
}

void Widget::restoreNormalFaultUi()
{
    m_faultAlarmPresented = false;
    if (m_resultCoordinator) {
        m_resultCoordinator->clearTransientView();
    }
    if (m_runtimeUiCoordinator) {
        m_runtimeUiCoordinator->restoreNormalFaultStyle();
    }
}

bool Widget::startDetectionWorkerForMode(
        InspectionRuntimeStartTransaction &startTransaction,
        int modeIndex,
        const PreparedRecipeSnapshot &prepared,
        const InspectionProfileSnapshot &profileSnapshot,
        QString *errorMessage)
{
    if (!m_resultCoordinator) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u68c0\u6d4b\u7ed3\u679c\u534f\u8c03\u5668\u672a\u521d\u59cb\u5316\u3002");
        }
        return false;
    }

    InspectionDetectionWorkerStartConfiguration configuration;
    configuration.modeIndex = modeIndex;
    configuration.profiles = profileSnapshot.detectionProfiles;
    configuration.ocrEngine = m_ocrEngine.get();
    configuration.barcodeDecoder = m_barcodeDecoder.get();
    if (!prepared || !prepared->recipe) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "运行配方快照不可用。");
        }
        return false;
    }
    configuration.tissueParameters = prepared->tissue;
    if ((modeIndex == 0 || modeIndex == 2)
            && !prepared->profiles.isEmpty()) {
        const PreparedRecipeProfile &profile =
                prepared->profiles.first();
        configuration.targetText =
                profile.definition.targetText;
        configuration.stampThresholdPercent =
                profile.definition.imageThresholdPercent;
        configuration.stampThresholdValid = true;
        if (modeIndex == 0) {
            for (const cv::Mat &character
                 : profile.characterTemplates) {
                configuration.stampTemplates.push_back(
                            character.clone());
            }
        }
    }
    configuration.resultConfiguration.imageSaveModeIndex =
            machineSettingsImageSaveModeIds().indexOf(
                m_appliedMachineSettings.imageSaveModeId);
    configuration.resultConfiguration.plcOutputEnabled =
            m_appliedMachineSettings.triggerEnabled;
    configuration.resultConfiguration.delayedNgOffset =
            m_appliedMachineSettings.rejectPosition;
    configuration.resultConfiguration.saveOptions.rootDirectory =
            m_appliedMachineSettings.imageSavePath;
    configuration.resultConfiguration.saveOptions.format =
            QStringLiteral("jpg");
    configuration.resultConfiguration.saveOptions.quality =
            m_appliedMachineSettings.imageJpegQuality;
    configuration.resultConfiguration.saveOptions.imageContentModeIndex =
            machineSettingsImageSaveTypeIds().indexOf(
                m_appliedMachineSettings.imageSaveTypeId);

    if (modeIndex == 0) {
        const std::shared_ptr<OverlapDetector> workerOverlapDetector(
                    new OverlapDetector);
        if (prepared->profiles.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "钢印运行配方缺少Prepared Profile。");
            }
            return false;
        }
        const PreparedRecipeProfile &profile =
                prepared->profiles.first();
        CalibrationData calibration;
        calibration.stamp_poly = profile.stampPolygon;
        calibration.date_poly = profile.datePolygon;
        calibration.barcode_poly = profile.barcodePolygon;
        if (!workerOverlapDetector->init(
                profile.stampRingTemplate, calibration)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "钢印PreparedRecipe无法初始化防重叠检测资产。");
            }
            return false;
        }
        configuration.detectStampOverlap = [workerOverlapDetector](
                const cv::Mat &sourceImage,
                const std::vector<cv::Point> &datePoly) {
            const DetectResult overlap =
                    workerOverlapDetector->processImage(
                        sourceImage,
                        datePoly);
            StampOverlapResult result;
            result.isOk = overlap.isOk;
            result.finalStampPoly = overlap.finalStampPoly;
            return result;
        };
    }

    return m_resultCoordinator->startDetectionWorker(
                startTransaction,
                configuration,
                errorMessage);
}

/**
 * @brief 显示图像槽函数
 * @param image OpenCV Mat图像指针
 * @details 将OpenCV图像转换为QPixmap并显示在UI上
 */
void Widget::slot_displayAndDetect(cv::Mat *image)
{
    DetectionMode activeMode = DetectionMode::Word;
    const bool tissueMode = detectionModeFromUiId(
                m_appliedMachineSettings.detectModeId,
                &activeMode)
            && activeMode == DetectionMode::Tissue;
    const bool productionRunning =
            isCollecting
            || m_operationState
               == OperationState::Detecting
            || m_operationState
               == OperationState::Stopping;
    if (image && m_resultCoordinator) {
        m_resultCoordinator->presentPreviewFrame(
                    *image, tissueMode, productionRunning);
    }
}


void Widget::clearBarcodeTemplateValidation()
{
    m_templateEditorController->clearBarcodeTemplateValidation();
}

QString Widget::barcodeTemplateValidationFailureText(
        const BarcodeReadResult &result) const
{
    return m_templateEditorController
            ->barcodeTemplateValidationFailureText(result);
}

bool Widget::validateBarcodeTemplateRect(
        const QRect &uiBarcodeRect,
        const BarcodeDecodeOptions &options,
        BarcodeReadResult *barcode,
        QString *failureReason)
{
    return m_templateEditorController->validateBarcodeTemplateRect(
                uiBarcodeRect,
                options,
                barcode,
                failureReason);
}

BarcodeDecodeOptions Widget::barcodeTemplateValidationOptions() const
{
    return m_templateEditorController
            ->barcodeTemplateValidationOptions();
}

bool Widget::hasRunningInspectionThread() const
{
    return m_acquisitionController
            && m_acquisitionController->hasRunningInspectionThread(
                m_templateCaptureState
                == TemplateCaptureState::Previewing);
}

void Widget::updateOperationUiState()
{
    if (m_runtimeUiCoordinator) {
        m_runtimeUiCoordinator->setMultiCameraWidget(
                    m_multiCameraWidget);
        m_runtimeUiCoordinator->updateOperationState(
                    m_operationState,
                    m_runtimeController.state()
                    == InspectionRuntimeState::Fault);
    }
}

bool Widget::stopTemplatePreview(int waitTimeMs)
{
    if (m_templateCaptureState
            != TemplateCaptureState::Previewing) {
        return true;
    }

    // 先使当前会话失效，已进入事件队列的旧帧将被直接忽略。
    ++m_templatePreviewSessionId;
    return !m_acquisitionController
            || m_acquisitionController->stopTemplatePreview(
                m_templatePreviewSessionId,
                waitTimeMs);
}

void Widget::resetTemplateCaptureState()
{
    const bool wasTemplateOperation =
            m_templateCaptureState
               != TemplateCaptureState::Idle
            || m_operationState
               == OperationState::TemplatePreviewing
            || m_operationState
               == OperationState::TemplateFrozen;
    if (!stopTemplatePreview()) {
        m_operationState = OperationState::Stopping;
        updateOperationUiState();
        return;
    }

    if (m_templateCaptureState
            != TemplateCaptureState::Previewing) {
        ++m_templatePreviewSessionId;
    }
    m_templateCaptureState = TemplateCaptureState::Idle;
    m_lastTemplatePreviewFrame.release();

    if (m_acquisitionController) {
        m_acquisitionController->disableTemplatePreview(
                    m_templatePreviewSessionId);
    }
    if (wasTemplateOperation
            || (m_operationState != OperationState::Detecting
                && m_operationState != OperationState::Stopping)) {
        m_operationState = m_bOpenDevice
                ? OperationState::CameraReady
                : OperationState::CameraClosed;
    }
    updateOperationUiState();
}

bool Widget::startTemplatePreview()
{
    if (!m_bOpenDevice
            || !m_acquisitionController
            || !m_acquisitionController->hasCamera()) {
        QMessageBox::warning(this, "提示", "请先点击【打开相机】！");
        return false;
    }
    if (isCollecting
            || m_acquisitionController->isHardwareRunning()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在进行正式检测，请先点击【停止识别】。");
        return false;
    }
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在进行正式检测，请先点击【停止识别】。");
        return false;
    }
    if (m_acquisitionController->isSoftwareRunning()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "相机采集线程仍在运行，请先停止当前任务。");
        return false;
    }
    m_acquisitionController->ensureWorkersReady();
    if (!m_acquisitionController->hasCamera()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "实时取景线程初始化失败。");
        return false;
    }

    try {
        if (!m_acquisitionController->setEnumValue(
                "TriggerMode",
                1).isSuccess()
                || !m_acquisitionController->setEnumValue(
                "TriggerSource",
                7).isSuccess()) {
            QMessageBox::warning(
                        this,
                        "警告",
                        "相机切换到软件触发模式失败！");
            return false;
        }
    } catch (...) {
        QMessageBox::warning(this, "警告", "相机配置失败！");
        return false;
    }

    QString exposureError;
    if (!applyCameraExposureValue(
                m_appliedMachineSettings.cameraExposure,
                &exposureError)) {
        QMessageBox::warning(
                    this,
                    "警告",
                    QString("制作模板前应用相机曝光失败：\n%1")
                    .arg(exposureError));
        return false;
    }

    angleValue = ui->comboBox_2->currentIndex();
    colorchannel = ui->comboBox_5->currentIndex();

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    clearBarcodeTemplateValidation();
    m_lastTemplatePreviewFrame.release();
    ++m_templatePreviewSessionId;
    m_templateCaptureState =
            TemplateCaptureState::Previewing;
    m_operationState =
            OperationState::TemplatePreviewing;
    if (m_resultCoordinator) {
        m_resultCoordinator->clearTransientView();
    }
    allResults.clear();
    updateOperationUiState();

    if (!m_acquisitionController->startTemplatePreview(
                m_templatePreviewSessionId,
                angleValue,
                colorchannel)) {
        QMessageBox::warning(
                    this,
                    "\u63d0\u793a",
                    "\u5b9e\u65f6\u53d6\u666f\u7ebf\u7a0b\u542f\u52a8\u5931\u8d25\u3002");
        resetTemplateCaptureState();
        return false;
    }

    updateImageDisplayStatusText(
                "实时取景中，请调整产品位置，确认后点击【拍照并开始框选】。");
    return true;
}

bool Widget::freezeTemplatePreview()
{
    if (m_templateCaptureState
            != TemplateCaptureState::Previewing) {
        return false;
    }
    if (m_lastTemplatePreviewFrame.empty()) {
        QMessageBox::information(
                    this,
                    "提示",
                    "相机尚未返回有效画面，请稍候再点击。");
        return false;
    }

    if (!stopTemplatePreview()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "实时取景线程尚未停止，请稍后重试。");
        return false;
    }

    m_templateCaptureState =
            TemplateCaptureState::Frozen;
    m_operationState =
            OperationState::TemplateFrozen;
    if (m_acquisitionController) {
        m_acquisitionController->replaceCurrentImage(
                    m_lastTemplatePreviewFrame);
    }
    cv::Mat frozenImage = m_lastTemplatePreviewFrame.clone();
    slot_displayAndDetect(&frozenImage);

    const QString modeId = currentDetectModeId();
    const bool needsTemplateDrawing =
            isSingleTemplateRecipeMode(modeId)
            || isWordFamilyMode(modeId);
    clearBarcodeTemplateValidation();
    imageLabel->setBarcodeRegionRequired(
                modeId == BarcodeWordDetectionMode);
    imageLabel->setTemplateDrawingEnabled(
                needsTemplateDrawing);
    if (needsTemplateDrawing) {
        imageLabel->resetDrawingStep();
        showTemplateGuideForCurrentMode();
    } else {
        hideTemplateGuide();
        updateImageDisplayStatusText(
                    "当前画面已冻结，如需调整请点击【重新取景】。");
    }
    updateOperationUiState();
    return true;
}

/**
 * @brief 制作模板按钮点击槽函数
 * @details 第一次点击进入实时取景，第二次点击冻结画面并开始框选
 */
void Widget::on_VideoShoot_clicked()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在进行正式检测，请先点击【停止识别】。");
        return;
    }
    if (!m_bOpenDevice) {
        QMessageBox::warning(this, "提示", "请先点击【打开相机】！");
        return;
    }

    if (m_templateCaptureState
            == TemplateCaptureState::Previewing) {
        freezeTemplatePreview();
        return;
    }

    if (m_templateCaptureState
            == TemplateCaptureState::Frozen
            && imageLabel
            && (!imageLabel->getTrackingRect().isNull()
                || !imageLabel->getBarcodeRect().isNull()
                || !imageLabel->getDetectionPoly().isEmpty())) {
        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Question);
        confirmBox.setWindowTitle("重新取景");
        confirmBox.setText(
                    "重新取景会清空当前已经绘制的框线。\n\n是否继续？");
        QPushButton *continueButton =
                confirmBox.addButton(
                    "重新取景",
                    QMessageBox::AcceptRole);
        QPushButton *cancelButton =
                confirmBox.addButton(
                    "取消",
                    QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();
        if (confirmBox.clickedButton()
                != continueButton) {
            return;
        }
    }

    startTemplatePreview();
}
/**
 * @brief 曝光值变化槽函数
 * @param value 新的曝光值
 */
void Widget::onSpinBoxValueChanged(int value)
{
    exposureValue = value;
}

/**
 * @brief 显示主窗口
 */
void Widget::showscreen()
{
    setWindowIcon(QIcon(":/2.png"));
    setWindowTitle(tr("识别系统"));
    this->show();
}

void Widget::showParameterInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void Widget::showParameterInfoWithRedWarning(const QString &title,
                                             const QString &message,
                                             const QString &warningMessage)
{
    QString infoHtml = message.toHtmlEscaped();
    infoHtml.replace("\r\n", "\n");
    infoHtml.replace('\r', '\n');
    infoHtml.replace("\n", "<br>");

    QString warningHtml = warningMessage.toHtmlEscaped();
    warningHtml.replace("\r\n", "\n");
    warningHtml.replace('\r', '\n');
    warningHtml.replace("\n", "<br>");

    QMessageBox messageBox(QMessageBox::Warning,
                           title,
                           QString(),
                           QMessageBox::Ok,
                           this);
    messageBox.setTextFormat(Qt::RichText);
    messageBox.setText(
                QString("<div>%1</div>"
                        "<div style=\"margin-top:12px;color:#c00000;"
                        "font-weight:700;\">%2</div>")
                .arg(infoHtml)
                .arg(warningHtml));
    messageBox.exec();
}

void Widget::showParameterInfoAsError(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void Widget::showParameterWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void Widget::showParameterCritical(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void Widget::updateCurrentTemplateName()
{
    m_templateEditorController->updateCurrentTemplateName();
}

void Widget::updateSaveDirButtonText()
{
    if (!ui || !ui->lineEdit_imageSavePath || !ui->pushButton_browseImageSavePath) {
        return;
    }

    const QString saveDir = selectedDir.trimmed();
    ui->pushButton_browseImageSavePath->setText("浏览");
    ui->pushButton_browseImageSavePath->setToolTip("点击选择图像保存路径");

    if (saveDir.isEmpty()) {
        ui->lineEdit_imageSavePath->clear();
        ui->lineEdit_imageSavePath->setToolTip("");
        return;
    }

    ui->lineEdit_imageSavePath->setText(saveDir);
    ui->lineEdit_imageSavePath->setToolTip(saveDir);
}

void Widget::updateImageSaveOptionsVisibility()
{
    if (!ui || !ui->comboBox) {
        return;
    }

    const bool saveImages =
            ui->comboBox->currentIndex() != 0;
    if (ui->label_saveImageType) {
        ui->label_saveImageType->setVisible(saveImages);
    }
    if (ui->comboBox_saveImageType) {
        ui->comboBox_saveImageType->setVisible(saveImages);
    }
    if (ui->label_imageSavePath) {
        ui->label_imageSavePath->setVisible(saveImages);
    }
    if (ui->lineEdit_imageSavePath) {
        ui->lineEdit_imageSavePath->setVisible(saveImages);
    }
    if (ui->pushButton_browseImageSavePath) {
        ui->pushButton_browseImageSavePath->setVisible(saveImages);
    }
    if (ui->imageSaveFrame) {
        ui->imageSaveFrame->updateGeometry();
    }
}

void Widget::updateTissueRoughnessUiVisibility()
{
    if (!ui) {
        return;
    }

    const bool showTissueThreshold = (ui->comboBox_4->currentIndex() == 3);
    ui->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->pushButton_tissueRoughnessThreshold->setVisible(showTissueThreshold);
}

void Widget::setupTemplateGuide()
{
    m_templateEditorController->setupTemplateGuide();
}

void Widget::adjustTemplateGuideHeight()
{
    m_templateEditorController->adjustTemplateGuideHeight();
}

void Widget::updateTemplateGuideText(
        const QString &title,
        const QString &body)
{
    m_templateEditorController->updateTemplateGuideText(title, body);
}

void Widget::hideTemplateGuide()
{
    m_templateEditorController->hideTemplateGuide();
}

void Widget::updateImageDisplayStatusText(const QString &body)
{
    m_templateEditorController->updateImageDisplayStatusText(body);
}

void Widget::showTemplateGuideForCurrentMode()
{
    m_templateEditorController->showTemplateGuideForCurrentMode();
}

void Widget::handleTemplateGuideEvent(
        const QString &eventName,
        int pointCount)
{
    m_templateEditorController->handleTemplateGuideEvent(
                eventName,
                pointCount);
}

void Widget::setupManualCharacterCropUi()
{
    m_templateEditorController->setupManualCharacterCropUi();
}

void Widget::setupSoftwareSettingsPage()
{
    if (!ui || !ui->lineEdit_softwareDataDir || !ui->pushButton_clearSoftwareData
            || !ui->pushButton_restoreDefaultSettings) {
        return;
    }

    m_softwareDataDirLineEdit = ui->lineEdit_softwareDataDir;
    m_softwareDataDirLineEdit->setReadOnly(true);
    m_softwareDataDirLineEdit->setCursor(Qt::PointingHandCursor);
    m_softwareDataDirLineEdit->setText(
                m_machineSettingsStore
                ? m_machineSettingsStore->applicationDataRoot()
                : QString());
    m_softwareDataDirLineEdit->setToolTip("软件公共设置保存在此文件夹。双击可打开目录；产品模板、识别图片、授权和日志不在清空范围内。");
    if (m_settingsPageController) {
        m_settingsPageController->setSoftwareDataDirectoryEditor(
                    m_softwareDataDirLineEdit);
    }

    ui->pushButton_clearSoftwareData->setStyleSheet(
                "QPushButton {"
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #d93025;"
                "padding: 5px 10px;"
                "}"
                "QPushButton:hover { background-color: #fff2f0; }"
                "QPushButton:pressed { background-color: #fde2e0; }");
    ui->pushButton_clearSoftwareData->setToolTip(
                "只清除当前 Windows 用户的软件公共界面设置，不删除产品模板、识别图片、授权文件或日志。");
    connect(ui->pushButton_clearSoftwareData,
            &QPushButton::clicked,
            this,
            &Widget::clearCurrentSoftwareData);

    ui->pushButton_restoreDefaultSettings->setStyleSheet(
                "QPushButton {"
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #333333;"
                "padding: 5px 10px;"
                "}"
                "QPushButton:hover { background-color: #f2f6fc; }"
                "QPushButton:pressed { background-color: #ebeef5; }");
    ui->pushButton_restoreDefaultSettings->setToolTip(
                "将软件公共界面设置恢复为默认值，不删除产品模板、识别图片、授权文件或日志。");
    connect(ui->pushButton_restoreDefaultSettings,
            &QPushButton::clicked,
            this,
            &Widget::restoreDefaultMachineSettings);
}

void Widget::clearCurrentSoftwareData()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "清空当前软件数据",
                "将清空当前 Windows 用户保存的软件界面设置和路径记录，并恢复默认设置。\n\n"
                "产品模板、识别图片、授权文件和日志不会被删除。\n\n"
                "是否继续？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_settingsPageController
            || !m_settingsPageController->clear(&errorMessage)) {
        showParameterCritical("严重警告", QString("清空软件公共数据失败：\n%1").arg(errorMessage));
        return;
    }

    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            m_appliedMachineSettings.publishedRecipeIdsByMode;
    clearWordMultiTemplateState();
    m_loadedTrackingTemplate.release();
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    savedTrackingBox = cv::Rect2d();
    hasValidBoxes = false;
    digitTemplates.clear();
    digitTemplateTargetIndexes.clear();
    m_templateEditorController->setCurrentTemplateNameVisible(false);
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
        imageLabel->clearGreenRects();
    }
    m_settingsPageController->clearAllDirty();
    clearRecipeProfileDirty();
    updateHardwareParameterUiEnabled();
    showParameterInfo("提示", "当前软件公共数据已清空，界面已恢复默认设置。");
}

void Widget::restoreDefaultMachineSettings()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "恢复默认设置",
                "将恢复软件公共设置为默认值。\n"
                "不会删除产品模板、识别图片、授权文件或日志。\n\n"
                "是否继续？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const bool cameraOpen = (m_acquisitionController
                             && m_acquisitionController->hasCamera()
                             && m_bOpenDevice);
    const bool plcConnected =
            m_runtimeController.isPlcConnected();
    const MachineSettings editableDefaults =
            m_settingsPageController->defaultsForHardwareState(
                cameraOpen, plcConnected);

    applyMachineSettingsToUi(editableDefaults);
    clearWordMultiTemplateState();
    m_loadedTrackingTemplate.release();
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    savedTrackingBox = cv::Rect2d();
    hasValidBoxes = false;
    digitTemplates.clear();
    digitTemplateTargetIndexes.clear();
    m_templateEditorController->setCurrentTemplateNameVisible(false);
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
        imageLabel->clearGreenRects();
    }
    clearRecipeProfileDirty();
    updateHardwareParameterUiEnabled();
    m_settingsPageController->refreshAllDirty();

    if (!saveSettings(false)) {
        showParameterCritical("严重警告", "恢复默认设置失败：公共配置保存失败。");
        return;
    }

    showParameterInfo(
        "提示",
        "当前可设置参数已恢复为默认值。带 * 的参数需要点击对应【设置】后才会生效。");
}

bool Widget::hasDirtySettings() const
{
    return m_settingsEditState.hasDirtySettings();
}

void Widget::updateHardwareParameterUiEnabled()
{
    if (!m_settingsPageController) {
        return;
    }
    const bool cameraOpen = m_acquisitionController
            && m_acquisitionController->hasCamera()
            && m_bOpenDevice;
    const bool operationBusy =
            m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_operationState == OperationState::TemplatePreviewing
            || m_operationState == OperationState::TemplateFrozen;
    m_settingsPageController->updateHardwareEnabled(
                cameraOpen,
                m_runtimeController.isPlcConnected(),
                operationBusy);
}

QString Widget::dirtySettingsMessage() const
{
    return m_settingsEditState.dirtySettingsMessage();
}

void Widget::restoreUnappliedSettingsFromApplied()
{
    if (m_settingsPageController) {
        m_settingsPageController->restoreUnappliedMachineSettings();
    }

    const bool oldUpdating = m_updatingMachineSettingsUi;
    m_updatingMachineSettingsUi = true;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(
                m_templateEditorController->wordTemplateProfiles().size())) {
        const RecipeProfile &settings =
                m_templateEditorController->wordTemplateProfiles()[
                    static_cast<size_t>(profileIndex)].settings;
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(QString::number(
            static_cast<int>(settings.imageThresholdPercent)));
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateEditorController->activePreparedRecipe()
               && m_templateEditorController->activePreparedRecipe()->recipe
               && m_templateEditorController->activePreparedRecipe()
                  ->recipe->profiles.size() == 1) {
        const RecipeProfile settings =
                m_templateEditorController->activePreparedRecipe()
                ->recipe->profiles.first();
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(QString::number(
            static_cast<int>(settings.imageThresholdPercent)));
    }
    m_updatingMachineSettingsUi = oldUpdating;
    refreshRecipeProfileDirty();
}

void Widget::setupRecipeProfileDirtyTracking()
{
    m_templateEditorController->setupRecipeProfileDirtyTracking();
}

void Widget::refreshTemplateTargetTextDirty()
{
    m_templateEditorController->refreshTemplateTargetTextDirty();
}

void Widget::refreshTemplateImageThresholdDirty()
{
    m_templateEditorController->refreshTemplateImageThresholdDirty();
}

void Widget::refreshRecipeProfileDirty()
{
    m_templateEditorController->refreshRecipeProfileDirty();
}

void Widget::markTemplateTargetTextDirty()
{
    m_templateEditorController->markTemplateTargetTextDirty();
}

void Widget::markTemplateImageThresholdDirty()
{
    m_templateEditorController->markTemplateImageThresholdDirty();
}

void Widget::clearTemplateTargetTextDirty()
{
    m_templateEditorController->clearTemplateTargetTextDirty();
}

void Widget::clearTemplateImageThresholdDirty()
{
    m_templateEditorController->clearTemplateImageThresholdDirty();
}

void Widget::clearRecipeProfileDirty()
{
    m_templateEditorController->clearRecipeProfileDirty();
}

void Widget::updateRecipeProfileDirtyUi()
{
    m_templateEditorController->updateRecipeProfileDirtyUi();
}

void Widget::showManualCharacterTemplateCropDialog()
{
    m_templateEditorController->showManualCharacterTemplateCropDialog();
}

void Widget::showStampCharacterTemplateCropDialog()
{
    m_templateEditorController->showStampCharacterTemplateCropDialog();
}

void Widget::showPublishedRecipeCharacterTemplateCropDialog(
        int profileIndex)
{
    m_templateEditorController
            ->showPublishedRecipeCharacterTemplateCropDialog(profileIndex);
}

void Widget::setupWordTemplateEditorCombo()
{
    m_templateEditorController->setupWordTemplateEditorCombo();
}

void Widget::setupDetectModeChangeTracking()
{
    connect(ui->comboBox_4,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (m_applyingMachineSettings) {
                    resetTemplateCaptureState();
                    updateTissueRoughnessUiVisibility();
                    refreshWordTemplateEditorCombo();
                    return;
                }

                const QString previousModeId = m_currentDetectModeId;
                const QString nextModeId = detectModeIdForIndex(index);
                resetTemplateCaptureState();
                m_resultBoundDisplayActive = false;
                m_currentDetectModeId = nextModeId;
                updateTissueRoughnessUiVisibility();
                if (imageLabel) {
                    imageLabel->setTemplateDrawingEnabled(false);
                }
                hideTemplateGuide();
                if (previousModeId != nextModeId) {
                    if (isWordFamilyMode(previousModeId)) {
                        clearWordMultiTemplateState();
                    } else if (isSingleTemplateRecipeMode(previousModeId)) {
                        clearSingleTemplateRecipeState();
                    } else {
                        refreshWordTemplateEditorCombo();
                    }
                } else {
                    refreshWordTemplateEditorCombo();
                }
                restoreTemplatesForMode(m_currentDetectModeId, false);
                saveSettings();
            });
}

void Widget::clearWordMultiTemplateState()
{
    m_templateEditorController->clearWordMultiTemplateState();
}

void Widget::clearSingleTemplateRecipeState()
{
    m_templateEditorController->clearSingleTemplateRecipeState();
}

QString Widget::detectModeIdForIndex(int index) const
{
    return m_templateEditorController->detectModeIdForIndex(index);
}

QString Widget::currentDetectModeId() const
{
    return m_templateEditorController->currentDetectModeId();
}

void Widget::restoreTemplatesForMode(
        const QString &modeId,
        bool showMessage)
{
    m_templateEditorController->restoreTemplatesForMode(
                modeId,
                showMessage);
}

void Widget::refreshWordTemplateEditorCombo()
{
    m_templateEditorController->refreshWordTemplateEditorCombo();
}

void Widget::applyWordTemplateEditorSelection(int comboIndex)
{
    m_templateEditorController
            ->applyWordTemplateEditorSelection(comboIndex);
}

void Widget::setCurrentWordTemplateEditIndex(int profileIndex)
{
    m_templateEditorController
            ->setCurrentWordTemplateEditIndex(profileIndex);
}

void Widget::publishCurrentWordTemplateGroup()
{
    m_templateEditorController->publishCurrentWordTemplateGroup();
}

void Widget::publishCurrentSingleTemplateRecipe()
{
    m_templateEditorController->publishCurrentSingleTemplateRecipe();
}

void Widget::selectPublishedRecipe()
{
    m_templateEditorController->selectPublishedRecipe();
}

bool Widget::activatePublishedWordRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QStringList *pendingMessages,
        QString *errorMessage)
{
    return m_templateEditorController->activatePublishedWordRecipe(
                recipeId,
                modeId,
                showErrorMessage,
                pendingMessages,
                errorMessage);
}

bool Widget::activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    return m_templateEditorController
            ->activatePublishedSingleTemplateRecipe(
                recipeId,
                modeId,
                showErrorMessage,
                errorMessage);
}

bool Widget::republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage)
{
    return m_templateEditorController
            ->republishSingleTemplateRecipeSettings(
                settings,
                errorMessage);
}

int Widget::currentWordTemplateProfileIndex() const
{
    return m_templateEditorController
            ->currentWordTemplateProfileIndex();
}

void Widget::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    m_templateEditorController->displayWordTemplateRawImage(profile);
}

bool Widget::loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    return m_templateEditorController->loadWordDigitTemplatesFromProfile(
                profile,
                baseNames,
                templates,
                templateTargetIndexes,
                errorMessage);
}

void Widget::refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const
{
    m_templateEditorController
            ->refreshWordTemplateRecipeProfile(profile);
}

bool Widget::saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage)
{
    return m_templateEditorController->saveWordRecipeProfile(
                profileIndex,
                settings,
                errorMessage);
}

bool Widget::publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage)
{
    return m_templateEditorController->publishWordTemplateRecipeEdit(
                profileIndex,
                errorMessage);
}

bool Widget::publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage)
{
    return m_templateEditorController->publishWordTemplateRecipeEdits(
                profileIndexes,
                errorMessage);
}

bool Widget::applyCameraExposureValue(
    int exposureValue,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_acquisitionController) {
        if (errorMessage) {
            *errorMessage = "相机未初始化，无法设置曝光";
        }
        return false;
    }
    const InspectionCameraParameterResult result =
            m_acquisitionController->applyExposure(exposureValue);
    if (result.minimumValue <= result.maximumValue) {
        QSignalBlocker blocker(ui->spinBox);
        ui->spinBox->setRange(
            result.minimumValue, result.maximumValue);
    }
    if (!result.success && errorMessage) {
        *errorMessage = result.diagnostic;
    }
    return result.success;
}

bool Widget::applySavedCameraExposure(
    QString *adjustmentMessage,
    QString *errorMessage)
{
    if (adjustmentMessage) {
        adjustmentMessage->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_acquisitionController) {
        if (errorMessage) {
            *errorMessage = "相机未初始化，无法设置曝光";
        }
        return false;
    }
    const int savedValue =
            m_appliedMachineSettings.cameraExposure;
    const InspectionCameraParameterResult result =
            m_acquisitionController->applySavedExposure(
                savedValue,
                [this, savedValue](int adjustedValue,
                                   QString *saveError) {
        m_appliedMachineSettings.cameraExposure = adjustedValue;
        if (saveSettings(false)) {
            return true;
        }
        m_appliedMachineSettings.cameraExposure = savedValue;
        if (saveError) {
            *saveError = "曝光值已根据相机范围调整，但公共配置保存失败";
        }
        return false;
    });
    if (result.minimumValue <= result.maximumValue) {
        QSignalBlocker blocker(ui->spinBox);
        ui->spinBox->setRange(
            result.minimumValue, result.maximumValue);
        if (result.success) {
            ui->spinBox->setValue(
                static_cast<int>(result.actualValue));
        }
    }
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.diagnostic;
        }
        return false;
    }
    const int adjustedValue = static_cast<int>(result.actualValue);
    if (adjustedValue != savedValue && adjustmentMessage) {
        *adjustmentMessage = QString(
            "原曝光值 %1 超出当前相机允许范围（%2 ~ %3），已调整为 %4。")
            .arg(savedValue)
            .arg(result.minimumValue)
            .arg(result.maximumValue)
            .arg(adjustedValue);
    }
    m_settingsPageController->refreshDirty("camera.exposure");
    return true;
}

bool Widget::applyCameraExposureFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    if (!m_acquisitionController
            || !m_acquisitionController->hasCamera()
            || !m_bOpenDevice) {
        const QString message = "未打开相机，无法设置曝光！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("警告", message);
        return false;
    }
    QString error;
    if (!applyCameraExposureValue(ui->spinBox->value(), &error)) {
        const QString message = error.isEmpty()
                ? QString("相机曝光设置失败") : error;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        m_settingsPageController->refreshDirty("camera.exposure");
        return false;
    }
    if (showSuccessMessage) {
        showParameterInfo("提示", "相机曝光设置成功！");
    }
    return true;
}

bool Widget::applyCameraGainFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    if (!m_acquisitionController
            || !m_acquisitionController->hasCamera()
            || !m_bOpenDevice) {
        const QString message =
                "相机未初始化或未打开，无法设置增益！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }
    int gainValue = 0;
    if (!parseIntValue(ui->lineEdit_14->text(), &gainValue)) {
        const InspectionCameraParameterResult range =
                m_acquisitionController->queryGainRange();
        const QString message = QString(
            "请输入有效的整数增益！当前相机允许范围：%1 ~ %2")
            .arg(range.minimumValue)
            .arg(range.maximumValue);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }
    const InspectionCameraParameterResult result =
            m_acquisitionController->applyGain(gainValue);
    if (!result.success) {
        if (errors) errors->append(result.diagnostic);
        if (showSuccessMessage) {
            showParameterWarning("提示", result.diagnostic);
        }
        return false;
    }
    if (showSuccessMessage) {
        showParameterInfo("提示", "相机增益设置成功！");
    }
    return true;
}

bool Widget::applyCameraHardwareSettingsFromUi(
    QStringList *errors,
    bool showSuccessMessage)
{
    bool ok = applyCameraExposureFromUi(
        errors, showSuccessMessage);
    ok = applyCameraGainFromUi(errors, showSuccessMessage) && ok;
    if (ok) {
        m_settingsPageController->updateAppliedFromUi(
            QStringList() << "camera.exposure" << "camera.gain");
        m_settingsPageController->refreshDirty(
            QStringList() << "camera.exposure" << "camera.gain");
        saveSettings(false);
    }
    return ok;
}

bool Widget::applyCameraHardwareSettingsForRun(QStringList *errors)
{
    QString exposureError;
    if (!applyCameraExposureValue(
                m_appliedMachineSettings.cameraExposure,
                &exposureError)) {
        if (errors) errors->append(exposureError);
        return false;
    }
    if (!m_acquisitionController
            || !m_acquisitionController->hasCamera()
            || !m_bOpenDevice) {
        if (errors) errors->append(
                    QStringLiteral("相机未初始化或未打开。"));
        return false;
    }
    const InspectionCameraParameterResult gain =
            m_acquisitionController->applyGain(
                m_appliedMachineSettings.cameraGain);
    if (!gain.success && errors) {
        errors->append(gain.diagnostic);
    }
    return gain.success;
}

bool Widget::applyRuntimeThreadSettingsForRun(
        const PreparedRecipeSnapshot &prepared,
        QStringList *errors)
{
    if (!prepared || !prepared->recipe
            || !m_acquisitionController) {
        if (errors) errors->append(
                    QStringLiteral("运行配方或采集控制器不可用。"));
        return false;
    }
    angleValue = machineSettingsRotationIds().indexOf(
                m_appliedMachineSettings.imageRotationId);
    colorchannel = machineSettingsColorChannelIds().indexOf(
                m_appliedMachineSettings.colorChannelId);
    if (angleValue < 0 || colorchannel < 0) {
        if (errors) errors->append(
                    QStringLiteral("机器设置中的旋转或颜色通道无效。"));
        return false;
    }
    m_acquisitionController->applyThreadSettings(
                angleValue,
                colorchannel,
                QString::number(
                    m_appliedMachineSettings.cameraDelay));
    return true;
}

bool Widget::applyPlcTriggerModeForRun(QStringList *errors)
{
    if (!m_runtimeController.isPlcConnected()) {
        return true;
    }
    PLCmode = machineSettingsTriggerModeIds().indexOf(
                m_appliedMachineSettings.triggerModeId);
    if (PLCmode < 0) {
        if (errors) errors->append(
                    QStringLiteral("机器设置中的PLC触发模式无效。"));
        return false;
    }
    const PlcOperationResult result =
            m_runtimeController.writePlcTriggerMode(PLCmode);
    if (!result.isSuccess() && errors) {
        errors->append(QStringLiteral("PLC触发模式下发失败。"));
    }
    return result.isSuccess();
}

bool Widget::applyPlcRunSettingsForRun(QStringList *errors)
{
    wrongindex = m_appliedMachineSettings.rejectPosition;
    if (!m_runtimeController.isPlcConnected()) {
        return true;
    }
    InspectionPlcRunSettings settings;
    settings.rejectTime = static_cast<std::uint16_t>(
                m_appliedMachineSettings.rejectTime);
    settings.rejectDistance = static_cast<std::uint32_t>(
                m_appliedMachineSettings.rejectDistance);
    settings.photoTime = static_cast<std::uint16_t>(
                m_appliedMachineSettings.photoTime);
    settings.photoDistance = static_cast<std::uint32_t>(
                m_appliedMachineSettings.photoDistance);
    const InspectionPlcRunSettingsResult result =
            m_runtimeController.applyPlcRunSettings(settings);
    if (!result.isSuccess() && errors) {
        errors->append(QStringLiteral("PLC运行参数下发失败。"));
    }
    return result.isSuccess();
}

bool Widget::applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage)
{
    PLCmode = ui->comboBox_3->currentIndex();

    if (!m_runtimeController.isPlcConnected()) {
        const QString message = "PLC未连接！";
        if (showSuccessMessage) {
            if (errors) errors->append(message);
            showParameterWarning("警告", message);
            return false;
        }
        return true;
    }

    if (PLCmode != 0 && PLCmode != 1) {
        const QString message = "PLC触发模式无效";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    const PlcOperationResult result =
            m_runtimeController.writePlcTriggerMode(PLCmode);
    if (!result.isSuccess()) {
        const QString message = PLCmode == 0 ? "设置连续模式失败" : "设置间歇模式失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", PLCmode == 0 ? "连续模式设置成功" : "间歇模式设置成功");
    }
    m_settingsPageController->updateAppliedFromUi("plc.trigger_mode");
    m_settingsPageController->refreshDirty("plc.trigger_mode");
    saveSettings(false);
    return true;
}

bool Widget::applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    if (!m_runtimeController.isPlcConnected()) {
        const QString message = "PLC未连接！";
        if (showSuccessMessage) {
            if (errors) errors->append(message);
            showParameterWarning("警告", message);
            return false;
        }
        return true;
    }

    wrongindex = ui->lineEdit_12->text().toInt();

    InspectionPlcRunSettings plcSettings;
    plcSettings.rejectTime = static_cast<std::uint16_t>(
                ui->lineEdit_8->text().toUInt());
    plcSettings.rejectDistance =
            ui->lineEdit_7->text().toUInt();
    plcSettings.photoTime = static_cast<std::uint16_t>(
                ui->lineEdit_20->text().toUInt());
    plcSettings.photoDistance =
            ui->lineEdit_6->text().toUInt();
    const InspectionPlcRunSettingsResult result =
            m_runtimeController.applyPlcRunSettings(plcSettings);
    if (!result.isSuccess()) {
        QString message;
        switch (result.failedField) {
        case InspectionPlcRunSettingField::RejectTime:
            message = "设置剔除时间失败";
            break;
        case InspectionPlcRunSettingField::RejectDistance:
            message = "设置剔除距离失败";
            break;
        case InspectionPlcRunSettingField::PhotoTime:
            message = "设置拍照时间失败";
            break;
        case InspectionPlcRunSettingField::PhotoDistance:
            message = "设置拍照距离失败";
            break;
        default:
            message = "设置PLC运行参数失败";
            break;
        }
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    if (m_acquisitionController) {
        m_acquisitionController->applyThreadSettings(
                    angleValue,
                    colorchannel,
                    ui->lineEdit_4->text());
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", "所有设置已经完成！");
    }
    m_settingsPageController->updateAppliedFromUi(QStringList()
                                      << "plc.photo_distance"
                                      << "plc.photo_time"
                                      << "plc.camera_delay"
                                      << "plc.reject_distance"
                                      << "plc.reject_time"
                                      << "plc.reject_position");
    m_settingsPageController->refreshDirty(QStringList()
                               << "plc.photo_distance"
                               << "plc.photo_time"
                               << "plc.camera_delay"
                               << "plc.reject_distance"
                               << "plc.reject_time"
                               << "plc.reject_position");
    saveSettings(false);
    return true;
}

/**
 * @brief 曝光确定按钮点击槽函数
 * @details 设置相机曝光值
 */
void Widget::on_sureButton_clicked()
{
    QStringList errors;
    if (applyCameraExposureFromUi(&errors, true)) {
        m_settingsPageController->updateAppliedFromUi("camera.exposure");
        m_settingsPageController->refreshDirty("camera.exposure");
        saveSettings(false);
    }
}

/**
 * @brief 获取目标字符串
 * @return QString 目标字符串
 */
/**
 * @brief PLC连接按钮点击槽函数
 * @details 连接到西门子PLC
 */
void Widget::on_ConnectpushButton_clicked()
{
    const int rack = ui->lineEdit_2->text().toInt();
    const int slot = ui->lineEdit_3->text().toInt();
    const PlcOperationResult result =
            m_runtimeController.connectPlc(
                ui->lineEdit->text(),
                rack,
                slot);

    if (result.isSuccess())
    {
        m_settingsPageController->updateAppliedFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
        m_settingsPageController->refreshDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
        saveSettings(false);
        updateHardwareParameterUiEnabled();
        QMessageBox::information(this, "success", "PLC连接成功");
    }
    else
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
}

/**
 * @brief PLC断开按钮点击槽函数
 */
void Widget::on_DisconnectpushButton_clicked()
{
    const PlcOperationResult result =
            m_runtimeController.disconnectPlc();

    if (result.isSuccess())
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::information(this, "success", "PLC断开成功");
    }
    else
    {
        updateHardwareParameterUiEnabled();
        QMessageBox::critical(this, "error", "PLC断开失败");
    }
}

/**
 * @brief 写入批次时间按钮点击槽函数
 * @details 向PLC DB1.982写入WORD值（批次时间）
 */
void Widget::on_pushButton_8_clicked()
{
    QStringList errors;
    applyPlcRunSettingsFromUi(&errors, true);
}



void Widget::on_cancel_clicked()
{
    InspectionStopController(this).stopInspection();
}



/**
 * @brief 目标字符确定按钮点击槽函数
 */

void Widget::on_textsure_btn_clicked()
{
    m_templateEditorController->applyCurrentTargetText();
}

void Widget::on_batchTextsure_btn_clicked()
{
    m_templateEditorController->applyBatchTargetText();
}

void Widget::on_batchImageThresholdButton_clicked()
{
    m_templateEditorController->applyBatchImageThreshold();
}
Mat *Widget::QImageToMat(const QImage &image)
{
    cv::Mat *mat = nullptr;
    if (image.isNull())
    {
        qWarning() << "QImage is null";
        return nullptr;
    }

    qDebug() << "QImage format:" << image.format();
    switch (image.format())
    {
    case QImage::Format_RGB32:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC4,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_ARGB32:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC4,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_RGB888:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC3,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_RGB2BGR);
        break;
    }
    case QImage::Format_Grayscale8:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC1,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_RGB2BGR);
        break;
    }
    default:
    {
        qWarning() << "QImage format not handled in switch:" << image.format();
        break;
    }
    }

    if (!mat || mat->empty())
    {
        qWarning() << "Failed to convert QImage to cv::Mat";
        return nullptr;
    }
    return mat;
}

/**
 * @brief 清空结果标签槽函数
 */
void Widget::slot_clearResultLabel()
{
    ui->resultlabel_7->clear();
    allResults = "";
}

/**
 * @brief 窗口关闭事件
 * @param event 关闭事件对象
 * @details 关闭时保存设置，销毁所有OpenCV窗口
 */
void Widget::closeEvent(QCloseEvent *event)
{
    if (m_applicationExitInProgress) {
        event->accept();
        return;
    }

    m_applicationExitInProgress = true;
    m_operationState = OperationState::Stopping;
    m_runtimeController.requestStop();
    m_barcodeWordRunActive = false;
    isCollecting = false;
    if (m_templateCaptureAttentionTimer) {
        m_templateCaptureAttentionTimer->stop();
    }

    // 立即废弃当前模板取景会话，禁止迟到帧继续进入UI。
    ++m_templatePreviewSessionId;
    m_templateCaptureState = TemplateCaptureState::Idle;
    m_lastTemplatePreviewFrame.release();

    if (m_acquisitionController) {
        m_acquisitionController->disableTemplatePreview(
                    m_templatePreviewSessionId);
        m_acquisitionController->stopForApplicationExit(500);
        if (m_bOpenDevice) {
            m_acquisitionController->closeCamera();
            m_bOpenDevice = false;
        }
    }

    if (m_runtimeController.isPlcConnected()) {
        m_runtimeController.disconnectPlc();
    }

    try {
        cv::destroyAllWindows();
    } catch (...) {
    }
    saveSettings(false);
    m_runtimeController.finishStop();
    event->accept();
}

/**
 * @brief 阈值确定按钮点击槽函数
 * @details 设置相似度判断阈值
 */
void Widget::on_pushButton_3_clicked()
{
    m_templateEditorController->applyCurrentImageThreshold();
}

/**
 * @brief 保存当前图像按钮点击槽函数
 * @details 打开文件保存对话框，保存当前显示的图像
 */
void Widget::on_pushButton_5_clicked()
{
    m_templateEditorController->saveCurrentTemplate();
}

// 先定义一个保存参数到指定文件夹的函数（可放在Widget类中）
void Widget::on_pushButton_4_clicked()
{
    m_templateEditorController->selectPublishedRecipeForCurrentMode();
}

/**
 * @brief 选择保存文件夹按钮点击槽函数
 */
void Widget::on_pushButton_browseImageSavePath_clicked()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
                this,
                "选择图像保存路径",
                selectedDir.isEmpty() ? QString("C:/") : selectedDir,
                QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) {
        return;
    }

    selectedDir = dirPath;
    updateSaveDirButtonText();
    qDebug() << "save file path:" << selectedDir;
}



/**
 * @brief 清空总数统计按钮点击槽函数
 */
void Widget::on_cut_cancelButton_2_clicked()
{
    m_runtimeController.resetStatistics();
    if (m_resultCoordinator) {
        m_resultCoordinator->presentTotalAndNgCounts(
                    m_runtimeController.totalCount(),
                    m_runtimeController.ngCount());
    }
}

/**
 * @brief 清空NG数统计按钮点击槽函数
 */
void Widget::on_cut_cancelButton_3_clicked()
{
    m_runtimeController.resetNgCount();
    if (m_resultCoordinator) {
        m_resultCoordinator->presentNgCount(
                    m_runtimeController.ngCount());
    }
}

/**
 * @brief 旋转角度确定按钮点击槽函数
 * @details 设置图像旋转角度（0°、90°、180°、270°）
 */
void Widget::on_pushButton_9_clicked()
{
    int index = ui->comboBox_2->currentIndex();
    switch (index)
    {
    case 1:
        angleValue = 1;
        break;
    case 2:
        angleValue = 2;
        break;
    case 3:
        angleValue = 3;
        break;
    default:
        angleValue = 0;
    }

    if (m_acquisitionController) {
        m_acquisitionController->applyThreadSettings(
                    angleValue,
                    colorchannel,
                    ui->lineEdit_4->text());
    }
    m_settingsPageController->updateAppliedFromUi("image.rotation");
    m_settingsPageController->refreshDirty("image.rotation");
    saveSettings(false);
    showParameterInfo("提示", "旋转角度设置成功");
}



bool Widget::saveSettings(bool showErrorMessage)
{
    QString errorMessage;
    if (m_settingsPageController
            && m_settingsPageController->save(
                showErrorMessage, &errorMessage)) {
        return true;
    }
    if (showErrorMessage) {
        showParameterCritical(
            "严重警告",
            QString("当前界面设置保存失败：\n%1")
            .arg(errorMessage));
    } else {
        qDebug() << "[MACHINE_SETTINGS] silent save failed:"
                 << errorMessage;
    }
    return false;
}

void Widget::applyMachineSettingsToUi(
    const MachineSettings &settings)
{
    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            settings.publishedRecipeIdsByMode;
    if (m_settingsPageController) {
        m_settingsPageController->applyToUi(settings);
    }
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
}

void Widget::applyRecipeProfileToUi(const RecipeProfile &settings)
{
    QSignalBlocker targetBlocker(ui->dateEdit);
    QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
    ui->dateEdit->setPlainText(settings.targetText);
    ui->lineEdit_yuzhi->setText(QString::number(static_cast<int>(settings.imageThresholdPercent)));
    refreshRecipeProfileDirty();
}

/**
 * @brief 设置非公共配置初始值
 * @details 公共配置统一由 MachineSettings::defaults() 提供
 */
void Widget::setupNonPersistentDefaults()
{
    ui->lineEdit_yuzhi->setText(QString::number(
        RecipeProfile::DefaultImageThresholdPercent));
    ui->dateEdit->setPlainText("");
    ui->lineEdit_tissueRoughnessThreshold->setText(
        QString::number(
            TissueRecipeParameters().roughnessThreshold,
            'f', 3));
}

// ================= 拦截滚轮误操作事件 =================
bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_templateEditorController->guideFrame()
            && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this]() {
            adjustTemplateGuideHeight();
        });
        return false;
    }

        if (watched == ui->textsure_btn
            || watched == ui->batchTextsure_btn
            || watched == ui->batchImageThresholdButton
            || watched == ui->checkBox
            || watched == ui->pushButton_browseImageSavePath
            || watched == ui->pushButton_7
            || watched == ui->pushButton_10
            || watched == ui->label_4
            || watched == ui->label_27
            || watched == ui->label_16
            || watched == ui->label_14
            || watched == ui->label_13
            || watched == ui->label_6
            || watched == ui->label_10
            || watched == ui->label_17
            || watched == ui->label_8
            || watched == ui->comboBox_3
            || watched == m_templateEditorController->manualCharacterCropButton()
            || watched == ui->VideoShoot) {
        QWidget *button = qobject_cast<QWidget *>(watched);
        if (!button) {
            return QWidget::eventFilter(watched, event);
        }

        if (event->type() == QEvent::Enter) {
            QTimer::singleShot(500, this, [this, button, watched]() {
                if (!button->underMouse()) {
                    return;
                }

                QString tooltipText;
                if (watched == ui->VideoShoot) {
                    switch (ui->comboBox_4->currentIndex()) {
                    case 0:
                        tooltipText =
                                "制作模板匹配产品模板：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 在冻结图像上框选定位区域和检测区域。\n"
                                "4. 点击【保存模板】保存产品模板。";
                        break;
                    case 1:
                        tooltipText =
                                "制作字库产品模板步骤：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 按住鼠标左键框选定位区域。\n"
                                "4. 用鼠标左键点击喷码区域边缘，右键闭合。\n"
                                "5. 点击【保存模板】保存产品模板。";
                        break;
                    case 2:
                        tooltipText =
                                "点击后进入实时取景，再次点击可冻结当前画面。\n\n"
                                "深度模型模式通常不需要制作传统产品模板。";
                        break;
                    case 3:
                        tooltipText =
                                "点击后进入实时取景，再次点击可冻结当前画面。\n\n"
                                "纸巾检测通常不需要制作产品模板。";
                        break;
                    case 4:
                        tooltipText =
                                "制作二维码+三期产品模板步骤：\n\n"
                                "1. 点击【制作模板】进入实时取景。\n"
                                "2. 调整产品位置后点击【拍照并开始框选】。\n"
                                "3. 框选稳定且不会变化的定位锚点。\n"
                                "4. 框选二维码区域并等待扫描验证。\n"
                                "5. 用鼠标左键点击日期区域边缘，右键闭合。\n"
                                "6. 点击【保存模板】保存产品模板。";
                        break;
                    default:
                        tooltipText = "点击后进入实时取景，再次点击冻结当前画面。";
                        break;
                    }
                } else {
                    tooltipText = button->toolTip();
                }

                if (!tooltipText.isEmpty()) {
                    QToolTip::showText(QCursor::pos(), tooltipText, button);
                }
            });
            return false;
        }

        if (event->type() == QEvent::Leave) {
            QToolTip::hideText();
            return false;
        }

        if (event->type() == QEvent::ToolTip) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

//关闭相机按钮
void Widget::on_CloseCamera_clicked()
{
    if (m_templateCaptureState
            != TemplateCaptureState::Idle
            || m_operationState
               == OperationState::TemplatePreviewing
            || m_operationState
               == OperationState::TemplateFrozen) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在制作模板，请先点击【退出模板制作】。");
        return;
    }

    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || isCollecting
            || hasRunningInspectionThread()
            || m_runtimeController.isBusy()) {
        QMessageBox::warning(
                    this,
                    "警告",
                    "相机正在检测采图中！\n"
                    "请先点击【停止识别】完全停止检测后，再关闭相机。");
        return;
    }

    if (m_acquisitionController
            && m_acquisitionController->hasCamera()
            && m_bOpenDevice)
    {
        m_acquisitionController->closeCamera();
        m_bOpenDevice = false;
    }
    // 清空文本并将文本置0
    ui->resultlabel->clear();
    imageLabel->setTemplateDrawingEnabled(false);
    hideTemplateGuide();
    imageLabel->clear();
    ui->image_undetected->clear();
    ui->imagenum->clear();
    ui->ngnum->clear();
    //    ui->ocrResult->clear();
    ui->resultlabel_7->clear();
    ui->speedLabel->clear();
    m_runtimeController.resetStatistics();
    //    qDebug()<<"totaltime"<<totalTime<<"s";
    //    totalTime=0;
    // 标记相机关闭状态
    m_bOpenDevice = false;
    m_templateCaptureState =
            TemplateCaptureState::Idle;
    if (m_runtimeUiCoordinator) {
        m_runtimeUiCoordinator->clearDetectionRoiWarning(QString());
    }
    m_lastTemplatePreviewFrame.release();
    m_operationState =
            OperationState::CameraClosed;
    ui->statusLabel->setText("相机已关闭");
    ui->statusLabel->setStyleSheet("QLabel{color:#e74c3c; font-weight:bold;}");
    updateOperationUiState();
}

void Widget::on_MultiCameraMode_clicked()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState
               != TemplateCaptureState::Idle) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "请先停止当前识别或退出模板制作。");
        return;
    }
    if (!m_multiCameraWidget) {
        m_multiCameraWidget = new MultiCameraWidget(this);
        m_multiCameraWidget->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_multiCameraWidget, &QObject::destroyed, this, [this]() {
            m_multiCameraWidget = nullptr;
        });
    }

    m_multiCameraWidget->show();
    m_multiCameraWidget->raise();
    m_multiCameraWidget->activateWindow();
}


void Widget::on_plcbtn_clicked()
{
    InspectionStartController(this).startInspection();
}
// 检测相机
void Widget::on_HandwareDetect_clicked()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState
               != TemplateCaptureState::Idle
            || hasRunningInspectionThread()
            || m_runtimeController.isBusy()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前有任务正在运行，不能重新打开相机。");
        return;
    }
    if (m_bOpenDevice)
    {
        QMessageBox::warning(this, "警告", "相机已连接！");
        return;
    }

    // 查找设备。SDK枚举类型由相机适配器内部持有。
    int deviceCount = 0;
    const CameraOperationResult enumerateResult =
            m_acquisitionController->enumerateDevices(&deviceCount);
    if (!enumerateResult.isSuccess() || deviceCount == 0)
    {
        QMessageBox::warning(this, "警告", "未找到相机设备！");
        return;
    }

    //连接PLC
    const PlcOperationResult result = m_runtimeController.connectPlc(
                ui->lineEdit->text(),
                ui->lineEdit_2->text().toInt(),
                ui->lineEdit_3->text().toInt());

    if (!result.isSuccess())
    {
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
    else{
    m_settingsPageController->updateAppliedFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    m_settingsPageController->refreshDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    saveSettings(false);
    qDebug()<<"opencamera，plc connect success";
    }
    updateHardwareParameterUiEnabled();

    const int savedExposure =
            m_appliedMachineSettings.cameraExposure;
    const InspectionCameraOpenResult openResult =
            m_acquisitionController->openFirstCamera(
                savedExposure,
                [this, savedExposure](
                    int adjustedExposure,
                    QString *saveError) {
        m_appliedMachineSettings.cameraExposure = adjustedExposure;
        if (saveSettings(false)) {
            return true;
        }
        m_appliedMachineSettings.cameraExposure = savedExposure;
        if (saveError) {
            *saveError = "曝光值已根据相机范围调整，但公共配置保存失败";
        }
        return false;
    },
                deviceCount);
    if (!openResult.isSuccess()) {
        m_bOpenDevice = false;
        if (openResult.issue
                == InspectionCameraOpenIssue::DeviceOpenFailed) {
            QMessageBox::warning(
                this, "警告", "打开设备失败！");
            return;
        }
        {
            QSignalBlocker blocker(ui->spinBox);
            ui->spinBox->setRange(
                0, (std::numeric_limits<int>::max)());
            ui->spinBox->setValue(
                m_appliedMachineSettings.cameraExposure);
        }
        m_settingsPageController->refreshDirty("camera.exposure");
        m_operationState = OperationState::CameraClosed;
        updateOperationUiState();
        QMessageBox::warning(
            this,
            "警告",
            QString("打开相机后应用曝光参数失败：\n%1")
            .arg(openResult.diagnostic));
        return;
    }

    m_bOpenDevice = true;
    {
        QSignalBlocker blocker(ui->spinBox);
        ui->spinBox->setRange(
            openResult.exposureMinimum,
            openResult.exposureMaximum);
        ui->spinBox->setValue(openResult.appliedExposure);
    }
    m_settingsPageController->refreshDirty("camera.exposure");

    ui->statusLabel->setText("相机已打开");
    ui->statusLabel->setStyleSheet("QLabel{color:#2ecc71; font-weight:bold;}");
    m_operationState =
            OperationState::CameraReady;
    updateOperationUiState();
    const QString openMessage = openResult.adjustmentMessage.isEmpty()
            ? QString("相机打开成功！")
            : QString("相机打开成功！\n\n%1")
              .arg(openResult.adjustmentMessage);
    QMessageBox::information(this, "提示", openMessage);
}

// PLC模式选择
void Widget::on_plcmodebtn_clicked()
{
    QStringList errors;
    applyPlcTriggerModeFromUi(&errors, true);
}

// 剔除位置设置
void Widget::on_eliminatebutton_clicked()
{
    wrongindex = ui->lineEdit_12->text().toInt();
    QMessageBox::information(this, "提示", "剔除位置设置成功");
}

//剔除队列复位 清空还未发出的剔除信号
void Widget::on_pushButton_10_clicked()
{
    m_runtimeController.clearPendingDelayedNgRequests();
    QMessageBox::information(this, "提示", "剔除队列已清空！");
}

/**
 * @brief 接收运行时姿态并更新非结果绑定预览
 */
void Widget::slot_saveBoxesFromThread(DetectionPose pose)
{
    // 字库家族生产检测显示“最后一次完整检测结果帧”。
    // 实时追踪位姿不能再移动上一张检测结果的字符框，否则画面、框和OK/NG会来自不同帧。
    if (m_resultBoundDisplayActive.load()) {
        return;
    }
    if (m_resultCoordinator) {
        m_resultCoordinator->updatePose(pose);
    }
}

//加载UI样式表模板
void Widget::initStyle()
    {
        QFile file(":/qss/1.css");// 淡蓝色风格
        if(file.open(QFile::ReadOnly)){
            QString qss = QLatin1String(file.readAll());
            qss +=
                    "\nQGroupBox#topControlPanel QToolButton:disabled {"
                    "background-color: #f2f3f5;"
                    "color: #a8abb2;"
                    "border-color: #dcdfe6;"
                    "}"
                    "QPushButton:disabled {"
                    "background-color: #f2f3f5;"
                    "color: #a8abb2;"
                    "border-color: #dcdfe6;"
                    "}"
                    "QComboBox:disabled,"
                    "QLineEdit:disabled,"
                    "QTextEdit:disabled,"
                    "QPlainTextEdit:disabled,"
                    "QSpinBox:disabled,"
                    "QDoubleSpinBox:disabled,"
                    "QDateEdit:disabled,"
                    "QTimeEdit:disabled {"
                    "color: #a8abb2;"
                    "}";

            // 提取主色调用于设置系统调色板
            QString paletteColor = qss.mid(20,7);// 获取QSS中定义的主色
            qApp->setPalette(QPalette(QColor(paletteColor)));

            // 应用样式表（qApp 是全局应用程序对象，作用于所有控件）
            qApp->setStyleSheet(qss);

            file.close();
        }
    }



//设置颜色通道
void Widget::on_pushButton_7_clicked()
{
    int index = ui->comboBox_5->currentIndex();
    switch (index)
    {
    case 1:
         colorchannel= 1;
        break;
    case 2:
        colorchannel = 2;
        break;
    case 3:
        colorchannel = 3;
        break;
    default:
        colorchannel = 0;
    }

    if (m_acquisitionController) {
        m_acquisitionController->applyThreadSettings(
                    angleValue,
                    colorchannel,
                    ui->lineEdit_4->text());
    }
    m_settingsPageController->updateAppliedFromUi("image.color_channel");
    m_settingsPageController->refreshDirty("image.color_channel");
    saveSettings(false);
    showParameterInfo("提示", "颜色通道设置成功");

}


//设置相机增益
void Widget::on_pushButton_12_clicked()
{
    QStringList errors;
    if (applyCameraGainFromUi(&errors, true)) {
        m_settingsPageController->updateAppliedFromUi("camera.gain");
        m_settingsPageController->refreshDirty("camera.gain");
        saveSettings(false);
    }
}

void Widget::on_pushButton_tissueRoughnessThreshold_clicked()
{
    m_templateEditorController->applyCurrentTissueThreshold();
}


void Widget::on_WriteVDpushButton_clicked()
{

    if (!m_runtimeController.isPlcConnected())
    {
        showParameterWarning("警告", "PLC未连接！");
        return;
    }

    const std::uint32_t value =
            ui->lineEdit_6->text().toUInt();
    const PlcOperationResult result =
            m_runtimeController.writePlcPhotoDistance(value);
// 判断写入结果
    if (!result.isSuccess())
    {
        // 写入失败
        showParameterWarning("error", "设置拍照距离失败");
    }
    else
    {
        // 写入成功
        m_settingsPageController->updateAppliedFromUi("plc.photo_distance");
        m_settingsPageController->refreshDirty("plc.photo_distance");
        saveSettings(false);
        showParameterInfo("提示", "拍照距离设置成功");
    }
}
