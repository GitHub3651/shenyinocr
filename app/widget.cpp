/**
 * @file widget.cpp
 * @brief 工业视觉识别系统主窗口实现文件
 * @details 实现图像采集、OCR识别、模板匹配、PLC通信等核心功能
 * @author 优化版本
 * @date 2024
 */

#include "widget.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_acquisition_controller.h"
#include "runtime/inspection_runtime_start_transaction.h"
#include "runtime/inspection_runtime_stop_transaction.h"
#include "ui_widget.h"
#include "recipes/recipe_selection.h"
#include "recipes/template_character_asset_workspace.h"
#include "recipes/template_profile_load_plan.h"
#include "recipes/template_profile_mapper.h"
#include "recipes/template_recipe_publisher.h"
#include "ui/dialogs/recipe_selection_dialog.h"
#include "multicamerawidget.h"
#include "charactertemplatecropdialog.h"
#include "DetectionModes.h"
#include "ui/controllers/inspection_result_coordinator.h"
#include "ui/controllers/inspection_runtime_ui_coordinator.h"
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
#include <QCoreApplication>
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
const int kDetectionImageJpegQuality = 92;

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
    return detectionModeFromId(modeId, &mode)
            && (mode == DetectionMode::Stamp
                || mode == DetectionMode::Ocr);
}

class CheckableDirectoryProxyModel : public QSortFilterProxyModel
{
public:
    explicit CheckableDirectoryProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::CheckStateRole && index.column() == 0) {
            const QString path = directoryPath(index);
            if (!path.isEmpty()) {
                return m_checkedPaths.contains(path) ? Qt::Checked : Qt::Unchecked;
            }
        }
        return QSortFilterProxyModel::data(index, role);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
    {
        if (role == Qt::CheckStateRole && index.column() == 0) {
            const QString path = directoryPath(index);
            if (path.isEmpty()) {
                return false;
            }

            if (value.toInt() == Qt::Checked) {
                m_checkedPaths.insert(path);
            } else {
                m_checkedPaths.remove(path);
            }
            emit dataChanged(index, index, QVector<int>() << Qt::CheckStateRole);
            return true;
        }
        return QSortFilterProxyModel::setData(index, value, role);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        Qt::ItemFlags itemFlags = QSortFilterProxyModel::flags(index);
        if (index.column() == 0 && !directoryPath(index).isEmpty()) {
            itemFlags |= Qt::ItemIsUserCheckable;
        }
        return itemFlags;
    }

    QStringList checkedDirectories() const
    {
        QStringList paths = m_checkedPaths.values();
        paths.sort(Qt::CaseInsensitive);
        return paths;
    }

private:
    QString directoryPath(const QModelIndex &proxyIndex) const
    {
        const QFileSystemModel *fileSystemModel =
                qobject_cast<const QFileSystemModel *>(sourceModel());
        if (!fileSystemModel || !proxyIndex.isValid()) {
            return QString();
        }

        const QModelIndex sourceIndex = mapToSource(proxyIndex);
        if (!sourceIndex.isValid() || !fileSystemModel->isDir(sourceIndex)) {
            return QString();
        }

        const QString fileName = fileSystemModel->fileName(sourceIndex);
        if (fileName == "." || fileName == "..") {
            return QString();
        }
        return QDir::cleanPath(fileSystemModel->filePath(sourceIndex));
    }

    QSet<QString> m_checkedPaths;
};
}

// OpenCV全局变量
cv::Point pt1, pt2;

// ==========================================
// 防重叠标定辅助功能 (精简版：去除了图像上的红色字体)
// ==========================================
struct PolygonUIState {
    cv::Mat displayImg;
    cv::Mat tempImg;
    std::string windowName;
    std::vector<cv::Point> points;
};

// ==========================================
// 钢印多边形描点功能 (点击画点，按回车键完成)
// ==========================================
static void polyMouseCallback(int event, int x, int y, int flags, void* userdata) {
    PolygonUIState* state = reinterpret_cast<PolygonUIState*>(userdata);
    if (event == cv::EVENT_LBUTTONDOWN) {
        state->points.push_back(cv::Point(x, y));
        state->tempImg = state->displayImg.clone();
        // 绘制已有的点和线
        for (size_t i = 0; i < state->points.size(); ++i) {
            cv::circle(state->tempImg, state->points[i], 3, cv::Scalar(0, 0, 255), -1);
            if (i > 0) {
                cv::line(state->tempImg, state->points[i - 1], state->points[i], cv::Scalar(0, 255, 0), 2);
            }
        }
        cv::imshow(state->windowName, state->tempImg);
    } else if (event == cv::EVENT_MOUSEMOVE && !state->points.empty()) {
        // 鼠标悬停时的预览辅助线
        cv::Mat hoverImg = state->tempImg.clone();
        cv::line(hoverImg, state->points.back(), cv::Point(x, y), cv::Scalar(255, 0, 0), 1);
        cv::imshow(state->windowName, hoverImg);
    }
}




// ==========================================
// 快速矩形标定功能 (拖拽并松开鼠标即完成)
// ==========================================
struct QuickROIState {
    cv::Mat displayImg;
    cv::Mat tempImg;
    std::string windowName;
    cv::Rect roi;
    cv::Point startPt;
    bool isDrawing = false;
    bool isDone = false;
};

static void quickMouseCallback(int event, int x, int y, int flags, void* userdata) {
    QuickROIState* state = reinterpret_cast<QuickROIState*>(userdata);

    if (event == cv::EVENT_LBUTTONDOWN) {
        state->startPt = cv::Point(x, y);
        state->isDrawing = true;
        state->isDone = false;
    }
    else if (event == cv::EVENT_MOUSEMOVE && state->isDrawing) {
        state->tempImg = state->displayImg.clone();
        cv::rectangle(state->tempImg, state->startPt, cv::Point(x, y), cv::Scalar(0, 255, 0), 2);
        cv::imshow(state->windowName, state->tempImg);
    }
    else if (event == cv::EVENT_LBUTTONUP) {
        state->roi = cv::Rect(state->startPt, cv::Point(x, y));
        // 处理反向拖拽的情况
        if (state->roi.width < 0) { state->roi.x += state->roi.width; state->roi.width = std::abs(state->roi.width); }
        if (state->roi.height < 0) { state->roi.y += state->roi.height; state->roi.height = std::abs(state->roi.height); }

        state->isDrawing = false;
        state->isDone = true; // 标记绘制完成
    }
}

static std::vector<cv::Point> getPolygonROI(const cv::Mat& img, const std::string& windowTitle) {
    cv::Mat displayImg = img.clone();
    int screenHeightLimit = 800;
    double scale = 1.0;
    if (displayImg.rows > screenHeightLimit) {
        scale = static_cast<double>(screenHeightLimit) / displayImg.rows;
        cv::resize(displayImg, displayImg, cv::Size(), scale, scale);
    }

    PolygonUIState state;
    state.displayImg = displayImg;
    state.tempImg = displayImg.clone();
    state.windowName = windowTitle;

    cv::namedWindow(windowTitle);
    cv::setMouseCallback(windowTitle, polyMouseCallback, &state);

    while (true) {
        cv::imshow(windowTitle, state.tempImg);
        int key = cv::waitKey(10) & 0xFF;
        if (key == 13) { // Enter键确认
            if (state.points.size() >= 3) {
                cv::line(state.tempImg, state.points.back(), state.points.front(), cv::Scalar(0, 255, 0), 2);
                cv::imshow(windowTitle, state.tempImg);
                cv::waitKey(300);
            }
            break;
        } else if (key == 27) { // ESC键取消
            state.points.clear();
            break;
        }
    }
    // 恢复 widget1.cpp 的简单销毁模式，不再手动注销 callback
    cv::destroyWindow(windowTitle);

    std::vector<cv::Point> finalPts;
    for (auto& pt : state.points) {
        finalPts.push_back(cv::Point(static_cast<int>(pt.x / scale), static_cast<int>(pt.y / scale)));
    }
    return finalPts;
}

static cv::Rect getQuickRectROI(const cv::Mat& img, const std::string& windowTitle) {
    cv::Mat displayImg = img.clone();
    int screenHeightLimit = 800;
    double scale = 1.0;
    if (displayImg.rows > screenHeightLimit) {
        scale = static_cast<double>(screenHeightLimit) / displayImg.rows;
        cv::resize(displayImg, displayImg, cv::Size(), scale, scale);
    }

    QuickROIState state;
    state.displayImg = displayImg;
    state.tempImg = displayImg.clone();
    state.windowName = windowTitle;

    cv::namedWindow(windowTitle);
    cv::setMouseCallback(windowTitle, quickMouseCallback, &state);

    while (!state.isDone) {
        cv::imshow(windowTitle, state.tempImg);
        int key = cv::waitKey(10) & 0xFF;
        if (key == 27) break;
    }

    cv::destroyWindow(windowTitle);

    cv::Rect finalRoi = state.roi;
    finalRoi.x = static_cast<int>(finalRoi.x / scale);
    finalRoi.y = static_cast<int>(finalRoi.y / scale);
    finalRoi.width = static_cast<int>(finalRoi.width / scale);
    finalRoi.height = static_cast<int>(finalRoi.height / scale);

    return finalRoi;
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
    QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget),
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
    settingsCallbacks.syncTemplateHistory = [this](
            GlobalSettings *settings) {
        if (!settings) {
            return;
        }
        storeCurrentTemplatePathsForMode(currentDetectModeId());
        settings->templateDirPathsByMode =
                m_templateEditorController->modeMemory()
                .templatePathsByMode();
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
                    &m_appliedGlobalSettings,
                    &m_settingsEditState,
                    &selectedDir,
                    &templateBaseDirPath,
                    &m_applyingGlobalSettings,
                    &m_updatingGlobalSettingsUi,
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
    setupTemplatePrivateSettingDirtyTracking();
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

    // 公共配置的默认值统一由 AppSettingsManager 提供。
    m_settingsPageController->setupNumericInputValidators();
    setupNonPersistentDefaults();
    qDebug() << "7. setupNonPersistentDefaults 执行完毕";

    loadSettings();
    qDebug() << "8. loadSettings 执行完毕";

    setupDetectModeChangeTracking();
    m_settingsPageController->setupBindings();
    m_settingsPageController->clearAllDirty();
    clearTemplatePrivateSettingDirty();
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

    QString filePath = "muban.png";
    QFile file(filePath);
    if (file.exists())
    {
        file.remove();
    }

    qDebug() << "Widget destroyed";
}
/**
 * @brief 初始化Widget组件
 * @details 创建图像保存文件夹、初始化图像对象、创建工作线程、连接信号槽
 */
void Widget::initWidget()
{
    // 初始化设备打开标志
    m_bOpenDevice = false;

    // 创建图像保存文件夹
    const QString imagePath = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("myImage"));
    QDir dstDir;
    if (!dstDir.exists(imagePath))
    {
        if (!dstDir.mkpath(imagePath))
        {
            qDebug() << "创建Image文件夹失败！";
        }
    }

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
        QString plcIp = m_appliedGlobalSettings.plcIp.trimmed();
        if (plcIp.isEmpty() && ui) {
            plcIp = ui->lineEdit->text().trimmed();
        }
        const int rack = m_appliedGlobalSettings.plcRack;
        const int slot = m_appliedGlobalSettings.plcSlot;
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
    configuration.tissueParameters = m_tissueRecipeParameters;
    configuration.barcodeDecoder = m_barcodeDecoder.get();
    if (modeIndex == 0) {
        configuration.targetText = setdatetime();
        configuration.stampTemplates = digitTemplates;
        configuration.stampThresholdPercent =
                ui->lineEdit_yuzhi->text().toInt(
                    &configuration.stampThresholdValid);
    } else if (modeIndex == 2 && configuration.ocrEngine) {
        configuration.targetText = setdatetime();
    }
    configuration.resultConfiguration.imageSaveModeIndex =
            ui->comboBox->currentIndex();
    configuration.resultConfiguration.plcOutputEnabled =
            ui->checkBox->isChecked();
    configuration.resultConfiguration.delayedNgOffset = wrongindex;
    configuration.resultConfiguration.saveOptions.rootDirectory = selectedDir;
    configuration.resultConfiguration.saveOptions.format =
            QStringLiteral("jpg");
    configuration.resultConfiguration.saveOptions.quality =
            kDetectionImageJpegQuality;
    configuration.resultConfiguration.saveOptions.imageContentModeIndex =
            ui->comboBox_saveImageType->currentIndex();

    if (modeIndex == 0 && QFile::exists(
            QDir(currentTemplateDirPath)
            .filePath(QStringLiteral("calibrate_config.yaml")))) {
        const std::shared_ptr<OverlapDetector> workerOverlapDetector(
                    new OverlapDetector(overlapDetector));
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
    const bool tissueMode = ui->comboBox_4->currentIndex() == 3;
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
                m_appliedGlobalSettings.cameraExposure,
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
    m_softwareDataDirLineEdit->setText(AppSettingsManager::globalDataDirPath());
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
            &Widget::restoreDefaultGlobalSettings);
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

    m_templateEditorController->modeMemory().templatePathsByMode() =
            m_appliedGlobalSettings.templateDirPathsByMode;
    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            m_appliedGlobalSettings.publishedRecipeIdsByMode;
    clearWordMultiTemplateState();
    currentTemplateDirPath.clear();
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
    clearTemplatePrivateSettingDirty();
    updateHardwareParameterUiEnabled();
    showParameterInfo("提示", "当前软件公共数据已清空，界面已恢复默认设置。");
}

void Widget::restoreDefaultGlobalSettings()
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
    const GlobalSettings editableDefaults =
            m_settingsPageController->defaultsForHardwareState(
                cameraOpen, plcConnected);

    applyGlobalSettingsToUi(editableDefaults);
    clearWordMultiTemplateState();
    currentTemplateDirPath.clear();
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
    clearTemplatePrivateSettingDirty();
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
        m_settingsPageController->restoreUnappliedGlobalSettings();
    }

    const bool oldUpdating = m_updatingGlobalSettingsUi;
    m_updatingGlobalSettingsUi = true;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(
                m_templateEditorController->wordTemplateProfiles().size())) {
        const TemplatePrivateSettings &settings =
                m_templateEditorController->wordTemplateProfiles()[
                    static_cast<size_t>(profileIndex)].settings;
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(QString::number(
            static_cast<int>(settings.imageThreshold)));
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateEditorController->singleTemplateEditSession().isActive()
               && m_templateEditorController->singleTemplateEditSession().recipe()
                  .profiles.size() == 1) {
        const TemplatePrivateSettings settings =
                templatePrivateSettingsFromRecipeProfile(
                    m_templateEditorController->singleTemplateEditSession().recipe()
                    .profiles.first());
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(QString::number(
            static_cast<int>(settings.imageThreshold)));
    }
    m_updatingGlobalSettingsUi = oldUpdating;
    refreshTemplatePrivateSettingDirty();
}

void Widget::setupTemplatePrivateSettingDirtyTracking()
{
    m_templateEditorController->setupTemplatePrivateSettingDirtyTracking();
}

void Widget::refreshTemplateTargetTextDirty()
{
    m_templateEditorController->refreshTemplateTargetTextDirty();
}

void Widget::refreshTemplateImageThresholdDirty()
{
    m_templateEditorController->refreshTemplateImageThresholdDirty();
}

void Widget::refreshTemplatePrivateSettingDirty()
{
    m_templateEditorController->refreshTemplatePrivateSettingDirty();
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

void Widget::clearTemplatePrivateSettingDirty()
{
    m_templateEditorController->clearTemplatePrivateSettingDirty();
}

void Widget::updateTemplatePrivateSettingDirtyUi()
{
    m_templateEditorController->updateTemplatePrivateSettingDirtyUi();
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
                if (m_applyingGlobalSettings) {
                    resetTemplateCaptureState();
                    updateTissueRoughnessUiVisibility();
                    refreshWordTemplateEditorCombo();
                    return;
                }

                const QString previousModeId = m_currentDetectModeId;
                const QString nextModeId = detectModeIdForIndex(index);
                resetTemplateCaptureState();
                m_resultBoundDisplayActive = false;
                storeCurrentTemplatePathsForMode(previousModeId);
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

QStringList Widget::currentTemplatePathsForMode(
        const QString &modeId) const
{
    return m_templateEditorController
            ->currentTemplatePathsForMode(modeId);
}

void Widget::storeCurrentTemplatePathsForMode(const QString &modeId)
{
    m_templateEditorController->storeCurrentTemplatePathsForMode(modeId);
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

bool Widget::loadSingleTemplateCharacterAssets(
        const QMap<QString, QString> &assetPathsByRole,
        const QStringList &targetUnits,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    return m_templateEditorController
            ->loadSingleTemplateCharacterAssets(
                assetPathsByRole,
                targetUnits,
                templates,
                templateTargetIndexes,
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
        const TemplatePrivateSettings &settings,
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

QString Widget::wordTemplateProfileAssetPath(
        const WordTemplateProfile &profile,
        const QString &role,
        const QString &legacyFileName) const
{
    return m_templateEditorController->wordTemplateProfileAssetPath(
                profile,
                role,
                legacyFileName);
}

void Widget::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    m_templateEditorController->displayWordTemplateRawImage(profile);
}

void Widget::displayWordTemplateRawImage(const QString &dirPath)
{
    m_templateEditorController->displayWordTemplateRawImage(dirPath);
}

void Widget::displayWordTemplateRawImageFile(
        const QString &rawImagePath,
        const QString &templateName)
{
    m_templateEditorController->displayWordTemplateRawImageFile(
                rawImagePath,
                templateName);
}

QStringList Widget::wordTemplateImagePathsForKey(
        const QDir &directory,
        const QString &searchKey,
        bool includeVariants) const
{
    return m_templateEditorController->wordTemplateImagePathsForKey(
                directory,
                searchKey,
                includeVariants);
}

bool Widget::loadWordDigitTemplatesFromDir(
        const QString &dirPath,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage,
        bool includeVariants) const
{
    return m_templateEditorController->loadWordDigitTemplatesFromDir(
                dirPath,
                baseNames,
                templates,
                templateTargetIndexes,
                errorMessage,
                includeVariants);
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

bool Widget::loadWordTemplateProfileFromDir(
        const QString &dirPath,
        WordTemplateProfile *profile,
        QString *errorMessage)
{
    return m_templateEditorController->loadWordTemplateProfileFromDir(
                dirPath,
                profile,
                errorMessage);
}

bool Widget::loadWordTemplateProfileFromRecipeSelection(
        const RecipeSelection &selection,
        int profileIndex,
        WordTemplateProfile *profile,
        QString *errorMessage)
{
    return m_templateEditorController
            ->loadWordTemplateProfileFromRecipeSelection(
                selection,
                profileIndex,
                profile,
                errorMessage);
}

bool Widget::loadWordTemplateProfilesFromRecipeSelection(
        const RecipeSelection &selection,
        std::vector<WordTemplateProfile> *profiles,
        QStringList *pendingMessages,
        QString *errorMessage)
{
    return m_templateEditorController
            ->loadWordTemplateProfilesFromRecipeSelection(
                selection,
                profiles,
                pendingMessages,
                errorMessage);
}

void Widget::refreshWordTemplateProfileDigitCache(
        WordTemplateProfile *profile) const
{
    m_templateEditorController
            ->refreshWordTemplateProfileDigitCache(profile);
}

InspectionProfileSnapshot
Widget::createWordTemplateRunSnapshot() const
{
    return m_templateEditorController->createWordTemplateRunSnapshot();
}

bool Widget::applyTissueRoughnessThresholdFromUi(bool showMessage)
{
    bool ok = false;
    const double threshold = ui->lineEdit_tissueRoughnessThreshold->text().trimmed().toDouble(&ok);
    if (!ok || threshold <= 0.0) {
        if (showMessage) {
            showParameterWarning("参数错误", "粗糙度阈值必须是大于0的数字");
        }
        return false;
    }

    updateTissueRecipeParameters(threshold);
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(threshold, 'f', 3));
    if (showMessage) {
        showParameterInfo("提示", "粗糙度阈值设置成功");
    }
    return true;
}

void Widget::refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const
{
    m_templateEditorController
            ->refreshWordTemplateRecipeProfile(profile);
}

bool Widget::saveWordTemplatePrivateSettings(
        int profileIndex,
        const TemplatePrivateSettings &settings,
        QString *errorMessage)
{
    return m_templateEditorController->saveWordTemplatePrivateSettings(
                profileIndex,
                settings,
                errorMessage);
}

void Widget::refreshWordTemplateRecipeAssets()
{
    m_templateEditorController->refreshWordTemplateRecipeAssets();
}

void Widget::prepareWordTemplateRecipeDraft(
        const WordTemplateProfile &profile)
{
    m_templateEditorController->prepareWordTemplateRecipeDraft(profile);
}

bool Widget::publishWordTemplateRecipeDraft(QString *errorMessage)
{
    return m_templateEditorController
            ->publishWordTemplateRecipeDraft(errorMessage);
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

void Widget::updateTissueRecipeParameters(
        double roughnessThreshold)
{
    TissueRecipeParameters parameters;
    parameters.roughnessThreshold = roughnessThreshold;
    m_tissueRecipeParameters = parameters;
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
            m_appliedGlobalSettings.cameraExposure;
    const InspectionCameraParameterResult result =
            m_acquisitionController->applySavedExposure(
                savedValue,
                [this, savedValue](int adjustedValue,
                                   QString *saveError) {
        m_appliedGlobalSettings.cameraExposure = adjustedValue;
        if (saveSettings(false)) {
            return true;
        }
        m_appliedGlobalSettings.cameraExposure = savedValue;
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

bool Widget::applyRuntimeThreadSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    InspectionRuntimeSettingsInput settingsInput;
    settingsInput.imageThresholdText =
            ui->lineEdit_yuzhi->text();
    settingsInput.tissueThresholdText =
            ui->lineEdit_tissueRoughnessThreshold->text();
    settingsInput.rotationIndex =
            ui->comboBox_2->currentIndex();
    settingsInput.colorChannelIndex =
            ui->comboBox_5->currentIndex();
    const InspectionRuntimeSettingsResult settingsResult =
            InspectionRunConfiguration::parseSettings(settingsInput);

    if (settingsResult.issue
            == InspectionRuntimeSettingsIssue::InvalidImageThreshold) {
        const QString message = "图像合格阈值必须是0到100之间的整数（单位：%）";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("参数错误", message);
        return false;
    }

    if (settingsResult.issue
            == InspectionRuntimeSettingsIssue::InvalidTissueThreshold) {
        const QString message = "纸巾检测粗糙度阈值必须是大于0的数字";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("参数错误", message);
        return false;
    }

    const int thresholdValue =
            settingsResult.settings.imageThreshold;
    const double tissueThreshold =
            settingsResult.settings.tissueThreshold;
    angleValue = settingsResult.settings.rotationCode;
    colorchannel = settingsResult.settings.colorChannelCode;

    updateTissueRecipeParameters(tissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(tissueThreshold, 'f', 3));

    emit ssim(thresholdValue);
    if (m_acquisitionController) {
        m_acquisitionController->applyThreadSettings(
                    angleValue,
                    colorchannel,
                    ui->lineEdit_4->text());
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", "运行参数设置成功");
    }
    m_settingsPageController->updateAppliedFromUi(QStringList()
                                      << "image.rotation"
                                      << "image.color_channel"
                                      << "tissue.roughness_threshold");
    m_settingsPageController->refreshDirty(QStringList()
                               << "image.rotation"
                               << "image.color_channel"
                               << "tissue.roughness_threshold");
    saveSettings(false);
    return true;
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
QString Widget::setdatetime()
{
    QString datetime = ui->dateEdit->toPlainText();
    return datetime;
}

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
    qDebug() << "=== on_cancel_clicked() START ===";
    if (m_runtimeUiCoordinator) {
        m_runtimeUiCoordinator->clearDetectionRoiWarning(QString());
    }

    const bool recoveringInspectionFault =
            m_runtimeController.state()
            == InspectionRuntimeState::Fault;
    if (recoveringInspectionFault
            && !confirmInspectionFaultRecovery()) {
        presentInspectionFault();
        return;
    }

    const bool templateOperation =
            m_templateCaptureState
               != TemplateCaptureState::Idle
            || m_operationState
               == OperationState::TemplatePreviewing
            || m_operationState
               == OperationState::TemplateFrozen;
    if (templateOperation) {
        resetTemplateCaptureState();
        if (m_operationState == OperationState::Stopping) {
            ui->statusLabel->setText(
                        "正在退出模板制作，请稍候");
            return;
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
            imageLabel->clearSelection();
        }
        clearBarcodeTemplateValidation();
        hideTemplateGuide();
        ui->statusLabel->setText(
                    m_bOpenDevice
                    ? "已退出模板制作，相机已打开"
                    : "已退出模板制作，相机已关闭");
        updateOperationUiState();
        return;
    }

    if (m_operationState != OperationState::Detecting
            && !isCollecting
            && !hasRunningInspectionThread()
            && !m_runtimeController.isBusy()) {
        updateOperationUiState();
        return;
    }

    m_operationState = OperationState::Stopping;
    InspectionRuntimeStopTransaction stopTransaction(
                m_runtimeController);
    stopTransaction.begin();
    updateOperationUiState();
    m_barcodeWordRunActive = false;

    const InspectionAcquisitionStopResult acquisitionStopResult =
            m_acquisitionController
            ? m_acquisitionController->stopInspection()
            : InspectionAcquisitionStopResult();
    stopTransaction.waitForDetectionWorker();

    if (!acquisitionStopResult.allStopped()) {
        if (!acquisitionStopResult.softwareStopped) {
            qDebug() << "WARNING: software acquisition worker did not stop";
        }
        if (!acquisitionStopResult.hardwareStopped) {
            qDebug() << "WARNING: hardware acquisition worker did not stop";
        }
        ui->statusLabel->setText("停止中，请稍后再关闭相机");
        isCollecting = true;
        m_operationState = OperationState::Stopping;
        updateOperationUiState();
        return;
    }

    const InspectionCameraRecoveryResult cameraRecoveryResult =
            m_acquisitionController->recoverCamera(
                acquisitionStopResult.shouldRestoreCamera(),
                m_bOpenDevice,
                [this](QString *adjustmentMessage,
                       QString *errorMessage) {
        return applySavedCameraExposure(
                    adjustmentMessage,
                    errorMessage);
    });
    m_bOpenDevice = cameraRecoveryResult.cameraOpen;
    if (cameraRecoveryResult.issue
            == InspectionCameraRecoveryIssue::ExposureRejected) {
        {
            QSignalBlocker blocker(ui->spinBox);
            ui->spinBox->setRange(
                        0,
                        (std::numeric_limits<int>::max)());
            ui->spinBox->setValue(
                        m_appliedGlobalSettings.cameraExposure);
        }
        m_settingsPageController->refreshDirty("camera.exposure");
        QMessageBox::warning(
                    this,
                    "警告",
                    QString("停止识别后恢复相机曝光失败：\n%1")
                    .arg(cameraRecoveryResult.errorMessage));
    } else if (cameraRecoveryResult.isRecovered()
               && cameraRecoveryResult.recoveryAttempted) {
        ui->statusLabel->setText("相机已打开");
        if (!cameraRecoveryResult.adjustmentMessage.isEmpty()) {
            QMessageBox::information(
                        this,
                        "提示",
                        cameraRecoveryResult.adjustmentMessage);
        }
    }

    // Step 6: 处理事件队列
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    // Step 7: 清理临时绘制状态；生产统计保留，由“清零”按钮负责清空
    detectedRects.clear();
    selectionRect1 = QRect();

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->setColor(1);
        imageLabel->clearSelection();
    }
    hideTemplateGuide();

    if (m_resultCoordinator) {
        m_resultCoordinator->clear();
    }
    m_barcodeWordRunActive = false;

    first = false;

    ui->statusLabel->setText("已停止");
    isCollecting = false;
    stopTransaction.commit();
    if (!recoveringInspectionFault
            && m_runtimeController.state()
               == InspectionRuntimeState::Fault) {
        m_operationState = OperationState::Fault;
        presentInspectionFault();
        return;
    }
    if (recoveringInspectionFault) {
        QString reconciliationSummary;
        QString reconciliationError;
        if (!reconcileInspectionFaultProducts(
                    &reconciliationSummary,
                    &reconciliationError)) {
            m_operationState = OperationState::Fault;
            presentInspectionFault();
            QMessageBox::critical(
                        this,
                        QStringLiteral("\u6545\u969c\u4ea7\u54c1\u6536\u53e3\u5931\u8d25"),
                        reconciliationError);
            return;
        }
        if (!m_runtimeController.acknowledgeFault()) {
            m_operationState = OperationState::Fault;
            presentInspectionFault();
            return;
        }
        restoreNormalFaultUi();
        if (!reconciliationSummary.isEmpty()) {
            QMessageBox::warning(
                        this,
                        QStringLiteral("\u6545\u969c\u4ea7\u54c1\u6536\u53e3\u7ed3\u679c"),
                        reconciliationSummary);
        }
        qDebug() << "[INSPECTION_FAULT] operator acknowledged"
                 << "software runtime unlocked";
    }
    m_operationState = m_bOpenDevice
            ? OperationState::CameraReady
            : OperationState::CameraClosed;
    // 停止识别后保留最后一次判定结果、识别内容和耗时，
    // 便于现场人员复核。进入模板制作时仍会主动清除这些内容。
    updateOperationUiState();

    qDebug() << "=== on_cancel_clicked() COMPLETED ===";
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
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_operationState == OperationState::TemplatePreviewing) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前状态不能保存模板，请先停止识别或冻结模板画面。");
        return;
    }
    const bool isBarcodeWordTemplateMode =
            currentDetectModeId() == BarcodeWordDetectionMode;
    const bool isWordTemplateMode =
            isWordFamilyMode(currentDetectModeId());
    const bool isStampTemplateMode =
            currentDetectModeId() == QStringLiteral("stamp_detection");

    if (!m_acquisitionController
            || !m_acquisitionController->hasCurrentImage()) {
        QMessageBox::warning(this, "提示", "请先点击【制作模板】拍照获取图像。");
        return;
    }
    if (!imageLabel->isTemplateDrawingEnabled()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    isBarcodeWordTemplateMode
                        ? "请先点击【制作模板】拍照，并完成定位锚点、二维码区域和日期检测区域框选。"
                        : "请先点击【制作模板】拍照，并完成定位区域和喷码检测区域框选。");
        return;
    }

    // 字库匹配模式下，保存前先检查框选状态，避免输入名称后才发现无法保存。
    QRect uiTrackRect = imageLabel->getTrackingRect();
    QRect uiBarcodeRect = imageLabel->getBarcodeRect();
    QPolygon uiDetectPoly = imageLabel->getDetectionPoly();

    if (isWordTemplateMode) {
        if (uiTrackRect.isNull()) {
            QMessageBox::warning(
                        this,
                        "提示",
                        isBarcodeWordTemplateMode
                            ? "请先框选稳定定位锚点。"
                            : "请先框选定位区域。");
            return;
        }
        if (uiTrackRect.width() <= 5 || uiTrackRect.height() <= 5) {
            QMessageBox::warning(
                        this,
                        "提示",
                        isBarcodeWordTemplateMode
                            ? "定位锚点区域太小，请重新框选。"
                            : "定位区域太小，请重新框选。");
            return;
        }
        if (isBarcodeWordTemplateMode) {
            if (uiBarcodeRect.isNull()) {
                QMessageBox::warning(this, "提示", "请先框选二维码区域。");
                return;
            }
            if (uiBarcodeRect.width() <= 5 || uiBarcodeRect.height() <= 5) {
                QMessageBox::warning(this, "提示", "二维码区域太小，请重新框选。");
                return;
            }
            const QRect normalizedBarcodeRect =
                    uiBarcodeRect.normalized();
            if (!m_templateEditorController->barcodeTemplateReadable()
                    || m_templateEditorController->validatedBarcodeRect()
                       != normalizedBarcodeRect) {
                BarcodeReadResult barcode;
                QString failureReason;
                if (!validateBarcodeTemplateRect(
                            normalizedBarcodeRect,
                            barcodeTemplateValidationOptions(),
                            &barcode,
                            &failureReason)) {
                    clearBarcodeTemplateValidation();
                    imageLabel->retryBarcodeRegion();
                    QMessageBox::warning(
                                this,
                                "二维码扫描失败",
                                failureReason
                                + "\n\n定位锚点已保留。模板不能保存，"
                                  "请重新完整框选二维码区域，扫描成功后再框选日期区域。");
                    return;
                }

                m_templateEditorController->acceptBarcodeTemplateValidation(
                            normalizedBarcodeRect, barcode.text);
            }
        }
        if (uiDetectPoly.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先框选喷码检测区域。");
            return;
        }
        if (uiDetectPoly.size() < 3 || !imageLabel->isDetectionPolyComplete()) {
            QMessageBox::warning(this, "提示", "喷码检测区域未闭合或点数不足，请重新框选。");
            return;
        }
    }

    if (!isWordTemplateMode
            && (uiTrackRect.isNull()
                || uiDetectPoly.isEmpty()
                || uiDetectPoly.size() < 3)) {
        QMessageBox::warning(this, "警告",
                             "保存模板前，请先在图像上完成以下操作：\n\n"
                             "1. 框选定位区域\n"
                             "2. 框选并闭合喷码检测区域\n\n"
                             "完成后再点击【保存模板】。");
        return;
    }

    int thresholdValue = 0;
    if (!parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue)
            || thresholdValue < 0
            || thresholdValue > 100) {
        showParameterWarning("参数错误",
                             "图像合格阈值必须是0到100之间的整数（单位：%），模板未保存。");
        return;
    }

    auto defaultTemplateBaseDir = [this]() -> QString {
        if (!templateBaseDirPath.trimmed().isEmpty() && QDir(templateBaseDirPath).exists()) {
            return QDir(templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    QDialog inputDialog(this);
    inputDialog.setWindowTitle("保存模板");
    inputDialog.setWindowFlags(inputDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *mainLayout = new QVBoxLayout(&inputDialog);
    QFormLayout *formLayout = new QFormLayout();

    QLineEdit *nameEdit = new QLineEdit(&inputDialog);
    formLayout->addRow("产品模板文件夹名称：", nameEdit);
    QLabel *nameErrorLabel = new QLabel("模板文件夹名称不能为空", &inputDialog);
    nameErrorLabel->setStyleSheet("color: #d93025;");
    formLayout->addRow("", nameErrorLabel);

    QLineEdit *baseDirEdit = new QLineEdit(defaultTemplateBaseDir(), &inputDialog);
    QPushButton *browseButton = new QPushButton("浏览", &inputDialog);
    QHBoxLayout *baseDirLayout = new QHBoxLayout();
    baseDirLayout->addWidget(baseDirEdit);
    baseDirLayout->addWidget(browseButton);
    formLayout->addRow("模板文件夹保存目录：", baseDirLayout);

    QLabel *hintLabel = new QLabel(
                isBarcodeWordTemplateMode
                    ? "保存后会记录稳定定位锚点、独立二维码区域和日期检测区域。"
                    : "保存后会记录当前产品的定位区域、喷码检测区域和参数配置。",
                &inputDialog);
    hintLabel->setWordWrap(true);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定", &inputDialog);
    QPushButton *cancelButton = new QPushButton("取消", &inputDialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(hintLabel);
    mainLayout->addLayout(buttonLayout);

    connect(browseButton, &QPushButton::clicked, this, [this, baseDirEdit]() {
        const QString selectedBaseDir = QFileDialog::getExistingDirectory(
                    this,
                    "选择模板文件夹保存目录",
                    baseDirEdit->text().trimmed().isEmpty() ? QDir::homePath() : baseDirEdit->text(),
                    QFileDialog::ShowDirsOnly);
        if (!selectedBaseDir.isEmpty()) {
            baseDirEdit->setText(QDir(selectedBaseDir).absolutePath());
        }
    });
    auto updateNameState = [nameEdit, nameErrorLabel, okButton]() {
        const bool isEmpty = nameEdit->text().trimmed().isEmpty();
        okButton->setEnabled(!isEmpty);
        nameErrorLabel->setVisible(isEmpty);
        nameEdit->setStyleSheet(isEmpty ? "QLineEdit { border: 1px solid #d93025; }" : "");
    };
    connect(nameEdit, &QLineEdit::textChanged, &inputDialog, updateNameState);
    updateNameState();
    connect(okButton, &QPushButton::clicked, &inputDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &inputDialog, &QDialog::reject);

    if (inputDialog.exec() != QDialog::Accepted) return;

    QString newFolderName = nameEdit->text().trimmed();
    if (newFolderName.isEmpty()) return;
    const QRegularExpression invalidFolderNameChars(R"([\\/:*?"<>|])");
    if (newFolderName == "."
            || newFolderName == ".."
            || newFolderName.contains(invalidFolderNameChars)) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "产品模板文件夹名称不能是“.”或“..”，"
                    "也不能包含 \\ / : * ? \" < > | 这些字符。");
        return;
    }

    QString baseDirPath = baseDirEdit->text().trimmed();
    if (baseDirPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择模板文件夹保存目录。");
        return;
    }
    baseDirPath = QDir(baseDirPath).absolutePath();

    QString savePath = QDir::cleanPath(
                QDir(baseDirPath).absoluteFilePath(newFolderName));
    if (QDir(QFileInfo(savePath).absolutePath()).absolutePath()
            .compare(baseDirPath, Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, "错误", "产品模板保存路径无效，模板未保存。");
        return;
    }

    QDir dir(savePath);
    if (dir.exists()) {
        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("确认覆盖");
        confirmBox.setText(
                    QString("产品模板 [%1] 已存在。\n\n"
                            "继续保存会先清空该文件夹中的全部旧文件和子文件夹，"
                            "然后写入当前模板。\n"
                            "清空后旧模板无法恢复。\n\n"
                            "是否继续？")
                    .arg(newFolderName));
        QPushButton *overwriteButton = confirmBox.addButton("覆盖", QMessageBox::AcceptRole);
        QPushButton *cancelButton = confirmBox.addButton("取消", QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();
        if (confirmBox.clickedButton() != overwriteButton) {
            return;
        }

        const QFileInfo existingTemplateInfo(savePath);
        if (existingTemplateInfo.isSymLink()) {
            QMessageBox::warning(this,
                                 "错误",
                                 "同名产品模板文件夹是快捷链接，无法安全清空。");
            return;
        }
        if (!dir.removeRecursively()) {
            QMessageBox::warning(
                        this,
                        "错误",
                        "同名产品模板文件夹清空失败。\n"
                        "请检查其中的文件是否被其他程序占用。");
            return;
        }
        dir = QDir(savePath);
    }

    if (!QDir().mkpath(savePath)) {
        QMessageBox::warning(this, "错误", "产品模板文件夹创建失败，无法保存模板。");
        return;
    }
    dir = QDir(savePath);
    templateBaseDirPath = baseDirPath;

    // 2. 转换坐标 (使用局部 clone 确保计算基准稳定)
    cv::Mat calibImg = m_acquisitionController->currentImageClone();

    auto toPhysicalPoint = [&](QPoint uiPt) -> cv::Point2f {
        QSize labelSize = imageLabel->size();
        QSize imgSize(calibImg.cols, calibImg.rows);
        const QPixmap *displayedPixmap = imageLabel->pixmap();
        QSize displayedSize =
                displayedPixmap && !displayedPixmap->isNull()
                ? displayedPixmap->size()
                : imgSize.scaled(labelSize, Qt::KeepAspectRatio);
        int xOff = (labelSize.width() - displayedSize.width()) / 2;
        int yOff = (labelSize.height() - displayedSize.height()) / 2;
        double ratioX =
                static_cast<double>(imgSize.width()) / displayedSize.width();
        double ratioY =
                static_cast<double>(imgSize.height()) / displayedSize.height();

        float px = static_cast<float>((uiPt.x() - xOff) * ratioX);
        float py = static_cast<float>((uiPt.y() - yOff) * ratioY);
        return cv::Point2f(px, py);
    };

    auto toPhysicalRect = [&](QRect uiRect) -> cv::Rect2d {
        cv::Point2f tl = toPhysicalPoint(uiRect.topLeft());
        cv::Point2f br = toPhysicalPoint(uiRect.bottomRight());
        cv::Rect2d phys(tl.x, tl.y, br.x - tl.x, br.y - tl.y);
        
        phys.x = std::max(0.0, phys.x);
        phys.y = std::max(0.0, phys.y);
        if (phys.x + phys.width > calibImg.cols) phys.width = calibImg.cols - phys.x;
        if (phys.y + phys.height > calibImg.rows) phys.height = calibImg.rows - phys.y;
        return phys;
    };

    savedTrackingBox = toPhysicalRect(uiTrackRect);
    
    // 计算多边形的绝对物理坐标，并存入 YAML 相对坐标 (相对于追踪框中心)
    std::vector<cv::Point2f> absDatePoly;
    for (const QPoint& pt : uiDetectPoly) {
        absDatePoly.push_back(toPhysicalPoint(pt));
    }
    
    cv::Point2f trackCenter(savedTrackingBox.x + savedTrackingBox.width / 2.0, 
                            savedTrackingBox.y + savedTrackingBox.height / 2.0);

    std::vector<cv::Point2f> relBarcodePoly;
    if (isBarcodeWordTemplateMode) {
        const cv::Rect2d barcodeBox =
                toPhysicalRect(uiBarcodeRect.normalized());
        const std::vector<cv::Point2f> absBarcodePoly = {
            cv::Point2f(static_cast<float>(barcodeBox.x),
                        static_cast<float>(barcodeBox.y)),
            cv::Point2f(static_cast<float>(barcodeBox.x + barcodeBox.width),
                        static_cast<float>(barcodeBox.y)),
            cv::Point2f(static_cast<float>(barcodeBox.x + barcodeBox.width),
                        static_cast<float>(barcodeBox.y + barcodeBox.height)),
            cv::Point2f(static_cast<float>(barcodeBox.x),
                        static_cast<float>(barcodeBox.y + barcodeBox.height))
        };
        relBarcodePoly.reserve(absBarcodePoly.size());
        for (const cv::Point2f &point : absBarcodePoly) {
            relBarcodePoly.push_back(
                        cv::Point2f(point.x - trackCenter.x,
                                    point.y - trackCenter.y));
        }
    }
                            
    std::vector<cv::Point2f> relDatePoly;
    for (const auto& pt : absDatePoly) {
        relDatePoly.push_back(cv::Point2f(pt.x - trackCenter.x, pt.y - trackCenter.y));
    }
    savedBarcodePoly = relBarcodePoly;
    savedDatePoly = relDatePoly;
    hasValidBoxes = true;

    // 3. 物理保存
    cv::imwrite(dir.absoluteFilePath("template_raw.png").toLocal8Bit().toStdString(), calibImg);
    cv::Mat tplImg = calibImg(savedTrackingBox).clone();
    cv::imwrite(dir.absoluteFilePath("tracking_template.bmp").toLocal8Bit().toStdString(), tplImg);
    m_loadedTrackingTemplate = tplImg.clone();

    currentTemplateDirPath = savePath;
    m_templateEditorController->singleTemplateEditSession().reset();
    m_templateEditorController->singleTemplateResolvedAssets().clear();
    m_templateEditorController->setCurrentTemplateDisplayName(QString());

    QString yamlPath = savePath + "/calibrate_config.yaml";
    {
        cv::FileStorage fs(yamlPath.toLocal8Bit().toStdString(), cv::FileStorage::WRITE);
        if (isBarcodeWordTemplateMode) {
            fs << "barcode_poly" << relBarcodePoly;
        }
        fs << "date_poly" << relDatePoly;
        fs.release();
    }

    // 4. 特征标定 (仅模式 0)
    if (ui->comboBox_4->currentIndex() == 0) {
        QMessageBox::information(this, "标定提示", "即将标定吸管口和钢印区。");

        // 吸管口标定
        cv::Rect ringRect = getQuickRectROI(calibImg, "ROI_1");
        if (ringRect.width > 5 && ringRect.height > 5) {
            cv::Mat ringTpl = calibImg(ringRect).clone();
            QString ringPath = savePath + "/template_ring.bmp";
            cv::imwrite(ringPath.toLocal8Bit().toStdString(), ringTpl);

            // 计算中心点用于相对坐标转换 (仿照 widget1.cpp 逻辑)
            cv::Point2f cRing(ringRect.x + ringRect.width / 2.0f, ringRect.y + ringRect.height / 2.0f);

            // 钢印多边形标定
            std::vector<cv::Point> stampPts = getPolygonROI(calibImg, "ROI_2");
            if (stampPts.size() >= 3) {
                // 转换相对坐标并使用 FileStorage 保存 (关键：确保引擎能读懂)
                std::vector<cv::Point2f> relStamp;
                for (const auto& pt : stampPts) {
                    relStamp.push_back(cv::Point2f(pt.x - cRing.x, pt.y - cRing.y));
                }

                cv::FileStorage fs(yamlPath.toLocal8Bit().toStdString(), cv::FileStorage::WRITE);
                fs << "stamp_poly" << relStamp;
                fs << "date_poly" << relDatePoly;
                fs.release();

                // 重新初始化检测引擎
                initOverlapDetectorFromCurrentDir();
            }
        }
    }

    // 5. 保存所有配置
    if (!saveSettingsToDir(savePath)) {
        return;
    }
    saveSettings();
    if (isWordTemplateMode) {
        WordTemplateProfile savedProfile;
        QString profileMessage;
        if (!loadWordTemplateProfileFromDir(savePath, &savedProfile, &profileMessage)) {
            showParameterCritical("严重警告",
                                  QString("产品模板文件已保存，但重新加载模板失败：\n%1")
                                  .arg(profileMessage));
            return;
        }
        m_templateEditorController->wordTemplateProfiles().clear();
        m_templateEditorController->wordTemplateProfiles().push_back(savedProfile);
        refreshWordTemplateRecipeAssets();
        prepareWordTemplateRecipeDraft(m_templateEditorController->wordTemplateProfiles().front());
        m_templateEditorController->setCurrentWordTemplateEditIndex(0);
        refreshWordTemplateEditorCombo();
        storeCurrentTemplatePathsForMode(currentDetectModeId());
        saveSettings();
    }
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    hideTemplateGuide();
    resetTemplateCaptureState();
    ui->statusLabel->setText(
                m_bOpenDevice
                ? "模板保存完成，相机已打开"
                : "模板保存完成，相机已关闭");
    m_templateEditorController->setCurrentTemplateNameVisible(true);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    if (isWordTemplateMode || isStampTemplateMode) {
        QMessageBox splitMessageBox(this);
        splitMessageBox.setIcon(QMessageBox::Information);
        splitMessageBox.setWindowTitle("保存成功");
        splitMessageBox.setText("产品模板已保存成功。\n\n是否立即切割字符模板？");
        QPushButton *splitButton = splitMessageBox.addButton("确定", QMessageBox::AcceptRole);
        splitMessageBox.addButton("取消", QMessageBox::RejectRole);
        splitMessageBox.setDefaultButton(splitButton);
        splitMessageBox.exec();

        if (splitMessageBox.clickedButton() == splitButton) {
            showManualCharacterTemplateCropDialog();
        }
    } else {
        QMessageBox::information(this, "成功", "模板及双框配置已全部保存！");
    }
}

// 先定义一个保存参数到指定文件夹的函数（可放在Widget类中）
bool Widget::saveSettingsToDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        showParameterCritical("严重警告", QString("无法创建产品模板文件夹：%1").arg(dirPath));
        return false;
    }

    TemplatePrivateSettings privateSettings = AppSettingsManager::defaultTemplatePrivateSettings();
    QString loadError;
    if (QFileInfo::exists(dir.filePath("app_settings.appset"))) {
        TemplatePrivateSettings existingSettings;
        if (AppSettingsManager::loadTemplatePrivateSettings(dirPath, &existingSettings, &loadError)) {
            privateSettings = existingSettings;
        }
    }

    int imageThreshold = 0;
    if (!parseIntValue(ui->lineEdit_yuzhi->text(), &imageThreshold)
            || imageThreshold < 0
            || imageThreshold > 100) {
        showParameterWarning("参数错误",
                             "图像合格阈值必须是0到100之间的整数（单位：%），模板配置未保存。");
        return false;
    }
    privateSettings.targetText = ui->dateEdit->toPlainText();
    privateSettings.imageThreshold = imageThreshold;

    const bool currentTrackingBoxValid = hasValidBoxes
            && savedTrackingBox.width > 0
            && savedTrackingBox.height > 0;

    bool trackingTemplateFileValid = false;
    const QString trackingTemplatePath = dir.absoluteFilePath("tracking_template.bmp");
    QFile trackingTemplateFile(trackingTemplatePath);
    if (trackingTemplateFile.open(QIODevice::ReadOnly)) {
        const QByteArray trackingTemplateBytes = trackingTemplateFile.readAll();
        if (!trackingTemplateBytes.isEmpty()) {
            const uchar *trackingTemplateData = reinterpret_cast<const uchar *>(trackingTemplateBytes.constData());
            std::vector<uchar> trackingTemplateBuffer(trackingTemplateData,
                                                      trackingTemplateData + trackingTemplateBytes.size());
            trackingTemplateFileValid = !cv::imdecode(trackingTemplateBuffer, cv::IMREAD_COLOR).empty();
        }
    }

    bool datePolyFileValid = false;
    bool barcodePolyFileValid =
            currentDetectModeId() != BarcodeWordDetectionMode;
    const QString calibratePath = dir.absoluteFilePath("calibrate_config.yaml");
    if (QFile::exists(calibratePath)) {
        CalibrationData calib;
        if (calib.load(calibratePath.toLocal8Bit().toStdString())) {
            datePolyFileValid = !calib.date_poly.empty();
            if (currentDetectModeId() == BarcodeWordDetectionMode) {
                barcodePolyFileValid = calib.barcode_poly.size() == 4;
            }
        }
    }

    const bool existingTemplateFilesValid =
            trackingTemplateFileValid
            && datePolyFileValid
            && barcodePolyFileValid;

    if (currentTrackingBoxValid) {
        privateSettings.trackingBox = savedTrackingBox;
        privateSettings.hasValidBoxes = true;
    } else if (existingTemplateFilesValid) {
        privateSettings.hasValidBoxes = true;
    } else {
        privateSettings.hasValidBoxes = false;
    }

    QString saveError;
    if (!AppSettingsManager::saveTemplatePrivateSettings(dirPath, privateSettings, &saveError)) {
        showParameterCritical("严重警告",
                              QString("产品模板配置保存失败：\n%1\n\n原配置未被覆盖。")
                              .arg(saveError));
        return false;
    }
    return true;
}
void Widget::initOverlapDetectorFromCurrentDir() {
    if (currentTemplateDirPath.isEmpty()) {
        qDebug() << "[DEBUG] currentTemplateDirPath is EMPTY. Skipping engine init.";
        return;
    }

    // 1. 定义文件路径
    QString ringPath = currentTemplateDirPath + "/template_ring.bmp";
    QString yamlPath = currentTemplateDirPath + "/calibrate_config.yaml";

    // 2. 获取绝对路径（用于排查由于相对路径导致的加载失败）
    QFileInfo ringInfo(ringPath);
    QFileInfo yamlInfo(yamlPath);

    qDebug() << "============ Path Debug Info ============";
    qDebug() << "Template Dir: " << currentTemplateDirPath;
    qDebug() << "Absolute Ring Path: " << ringInfo.absoluteFilePath();
    qDebug() << "Ring File Exists? " << (ringInfo.exists() ? "YES" : "NO");
    qDebug() << "Absolute YAML Path: " << yamlInfo.absoluteFilePath();
    qDebug() << "YAML File Exists? " << (yamlInfo.exists() ? "YES" : "NO");
    qDebug() << "=========================================";

    if (ringInfo.exists() && yamlInfo.exists()) {
        // 使用 toLocal8Bit().toStdString() 以支持 Windows 下的本地编码路径
        try {
            bool ok = overlapDetector.init(ringPath.toLocal8Bit().toStdString(),
                                           yamlPath.toLocal8Bit().toStdString());
            if (!ok) {
                qDebug() << "[ERROR] overlapDetector.init returned FALSE. Check if BMP is corrupted.";
            } else {
                qDebug() << "[SUCCESS] Overlap Engine is initialized and ready.";
            }
        } catch (...) {
            qDebug() << "[致命错误] overlapDetector.init 内部发生 C++ 崩溃！可能是 OpenCV 异常或 YAML 解析错误！";
        }
    } else {
        qDebug() << "[ERROR] Cannot start engine: One or more files missing on disk.";
    }
}

/**
 * @brief 加载字库按钮点击槽函数
 * @details 支持带括号的字符格式，如"0(1)"表示0字符的第1个变体
 */
// 按钮pushButton_4的点击事件槽函数
// 功能：从用户输入解析模板文件名，选择产品模板文件夹
void Widget::on_pushButton_4_clicked()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState
               != TemplateCaptureState::Idle) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "请先停止识别或退出模板制作，再选择产品模板。");
        return;
    }
    QString dirPath;
    auto templateDialogStartDir = [this]() -> QString {
        if (!templateBaseDirPath.trimmed().isEmpty() && QDir(templateBaseDirPath).exists()) {
            return QDir(templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    if (isWordFamilyMode(currentDetectModeId())) {
        QFileDialog dialog(this, "选择产品模板文件夹（可勾选多个）", templateDialogStartDir());
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setLabelText(QFileDialog::LookIn, "查找范围:");
        dialog.setLabelText(QFileDialog::FileName, "文件夹:");
        dialog.setLabelText(QFileDialog::FileType, "文件类型:");
        dialog.setLabelText(QFileDialog::Accept, "选择");
        dialog.setLabelText(QFileDialog::Reject, "取消");
        dialog.setNameFilter("所有文件 (*)");

        CheckableDirectoryProxyModel *checkableDirectoryModel =
                new CheckableDirectoryProxyModel(&dialog);
        dialog.setProxyModel(checkableDirectoryModel);

        QListView *listView = dialog.findChild<QListView *>("listView");
        if (listView) {
            listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        }
        QTreeView *treeView = dialog.findChild<QTreeView *>();
        if (treeView) {
            treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
            treeView->setHeaderHidden(true);
            treeView->setColumnHidden(1, true);
            treeView->setColumnHidden(2, true);
            treeView->setColumnHidden(3, true);
        }

        if (dialog.exec() != QDialog::Accepted) return;

        QStringList selectedDirs = checkableDirectoryModel->checkedDirectories();
        const QStringList dialogSelectedDirs =
                selectedDirs.isEmpty() ? dialog.selectedFiles() : QStringList();
        for (const QString &selectedDirPath : dialogSelectedDirs) {
            const QString cleanDir = QDir(selectedDirPath).absolutePath();
            if (!cleanDir.isEmpty() && !selectedDirs.contains(cleanDir)) {
                selectedDirs.append(cleanDir);
            }
        }

        if (selectedDirs.isEmpty()) return;
        resetTemplateCaptureState();
        const QFileInfo firstSelectedDirInfo(selectedDirs.first());
        if (firstSelectedDirInfo.dir().exists()) {
            templateBaseDirPath = firstSelectedDirInfo.dir().absolutePath();
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        hideTemplateGuide();

        if (!selectedDirs.isEmpty()) {
            std::vector<WordTemplateProfile> loadedProfiles;
            QStringList skippedMessages;
            QStringList pendingTargetMessages;

            for (const QString &selectedDirPath : selectedDirs) {
                QDir templateDir(selectedDirPath);
                WordTemplateProfile profile;
                QString profileMessage;
                if (!loadWordTemplateProfileFromDir(templateDir.absolutePath(),
                                                    &profile,
                                                    &profileMessage)) {
                    skippedMessages.append(QString("%1：%2")
                                           .arg(templateDir.dirName())
                                           .arg(profileMessage));
                    continue;
                }
                if (!profileMessage.trimmed().isEmpty()) {
                    pendingTargetMessages.append(QString("%1：%2")
                                                 .arg(profile.name)
                                                 .arg(profileMessage));
                }
                loadedProfiles.push_back(profile);
            }

            if (loadedProfiles.empty()) {
                QString detailMessage = "所选产品模板配置全部无效，已保留当前加载的模板。";
                if (!skippedMessages.isEmpty()) {
                    detailMessage += "\n\n具体原因：\n" + skippedMessages.join("\n");
                }
                if (!pendingTargetMessages.isEmpty()) {
                    detailMessage += "\n\n目标字符待设置：\n" + pendingTargetMessages.join("\n");
                }
                showParameterCritical("严重警告", detailMessage);
                return;
            }

            m_templateEditorController->wordDraftSession().reset();
            m_templateEditorController->wordEditSession().reset();
            m_templateEditorController->singleTemplateEditSession().reset();
            m_templateEditorController->singleTemplateResolvedAssets().clear();
            m_templateEditorController->setCurrentTemplateDisplayName(QString());
            m_templateEditorController->wordTemplateProfiles().swap(loadedProfiles);
            refreshWordTemplateRecipeAssets();
            currentTemplateDirPath = m_templateEditorController->wordTemplateProfiles().front().dirPath;
            m_templateEditorController->setCurrentTemplateNameVisible(false);
            updateCurrentTemplateName();
            refreshWordTemplateEditorCombo();
            saveSettings();

            qDebug() << "[WORD_TEMPLATE] loaded profile count:"
                     << static_cast<int>(m_templateEditorController->wordTemplateProfiles().size());
            if (!skippedMessages.isEmpty() || !pendingTargetMessages.isEmpty()) {
                QString detailMessage = QString("已加载 %1 个字库模板").arg(static_cast<int>(m_templateEditorController->wordTemplateProfiles().size()));
                if (!pendingTargetMessages.isEmpty()) {
                    detailMessage += "\n\n以下模板已加载，但目标字符待设置：\n" + pendingTargetMessages.join("\n");
                }
                if (!skippedMessages.isEmpty()) {
                    detailMessage += "\n\n以下模板已跳过：\n" + skippedMessages.join("\n");
                }
                showParameterWarning("提示",
                                     detailMessage);
            } else {
                showParameterInfo("提示",
                                  QString("已加载 %1 个有效字库模板")
                                  .arg(static_cast<int>(m_templateEditorController->wordTemplateProfiles().size())));
            }
            return;
        }
    } else {
        dirPath = QFileDialog::getExistingDirectory(nullptr, "选择产品模板文件夹",
                                                    templateDialogStartDir(),
                                                    QFileDialog::ShowDirsOnly);
        if (dirPath.isEmpty()) return;
        resetTemplateCaptureState();
        const QFileInfo selectedDirInfo(dirPath);
        if (selectedDirInfo.dir().exists()) {
            templateBaseDirPath = selectedDirInfo.dir().absolutePath();
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        hideTemplateGuide();
    }

    if (dirPath.isEmpty()) return;

    m_templateEditorController->wordDraftSession().reset();
    m_templateEditorController->wordEditSession().reset();
    m_templateEditorController->singleTemplateEditSession().reset();
    m_templateEditorController->singleTemplateResolvedAssets().clear();
    m_templateEditorController->setCurrentTemplateDisplayName(QString());
    m_templateEditorController->wordTemplateProfiles().clear();
    currentTemplateDirPath = dirPath;

    saveSettings(); // 保存路径
    const bool templateLoaded = loadSettingsFromDir(dirPath);
    m_templateEditorController->setCurrentTemplateNameVisible(templateLoaded);
    if (!templateLoaded) {
        updateCurrentTemplateName();
        return;
    }
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    if (isWordFamilyMode(currentDetectModeId()) && imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
        hideTemplateGuide();
        displayWordTemplateRawImage(dirPath);
    }
    wrongindex = ui->lineEdit_12->text().toInt();

    qDebug()<<"currentTemplate"<<currentTemplateDirPath;

    initOverlapDetectorFromCurrentDir();

    QMessageBox::information(this, "提示", "模板已选择");
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



bool Widget::loadSettingsFromDir(const QString &dirPath, bool showErrorMessage)
{
    TemplatePrivateSettings privateSettings;
    QString loadError;
    if (!AppSettingsManager::loadTemplatePrivateSettings(dirPath, &privateSettings, &loadError)) {
        qDebug() << "[TEMPLATE_PRIVATE_SETTINGS] load failed:" << dirPath << loadError;
        if (showErrorMessage) {
            showParameterCritical("严重警告",
                                  QString("产品模板 [%1] 的私有配置无效：\n%2")
                                  .arg(QDir(dirPath).dirName())
                                  .arg(loadError));
        }
        return false;
    }

    applyTemplatePrivateSettingsToUi(privateSettings);
    savedTrackingBox = privateSettings.trackingBox;
    hasValidBoxes = privateSettings.hasValidBoxes
            || (savedTrackingBox.width > 0 && savedTrackingBox.height > 0);

    // 🔥 新增：加载局部静态追踪模板 (Anchor Template)
    QString tplPath = dirPath + "/tracking_template.bmp";
    m_loadedTrackingTemplate.release();
    QFile trackingTemplateFile(tplPath);
    if (trackingTemplateFile.open(QIODevice::ReadOnly)) {
        const QByteArray trackingTemplateBytes = trackingTemplateFile.readAll();
        if (!trackingTemplateBytes.isEmpty()) {
            const uchar *trackingTemplateData = reinterpret_cast<const uchar *>(trackingTemplateBytes.constData());
            std::vector<uchar> trackingTemplateBuffer(trackingTemplateData,
                                                      trackingTemplateData + trackingTemplateBytes.size());
            m_loadedTrackingTemplate = cv::imdecode(trackingTemplateBuffer, cv::IMREAD_COLOR);
        }
    }
    if (!m_loadedTrackingTemplate.empty()) {
        qDebug() << "成功加载定位模板图片：" << tplPath;
    } else {
        qDebug() << "警告：未找到 tracking_template.bmp";
    }

    savedBarcodePoly.clear();
    savedDatePoly.clear();
    QString yamlPath = dirPath + "/calibrate_config.yaml";
    if (QFile::exists(yamlPath)) {
        CalibrationData calib;
        if (calib.load(yamlPath.toLocal8Bit().toStdString())) {
            savedBarcodePoly = calib.barcode_poly;
            savedDatePoly = calib.date_poly;
        }
    }

    const bool barcodePolyValid =
            currentDetectModeId() != BarcodeWordDetectionMode
            || savedBarcodePoly.size() == 4;
    const bool templateFilesValid =
            !m_loadedTrackingTemplate.empty()
            && !savedDatePoly.empty()
            && barcodePolyValid;
    const bool trackingBoxValid = savedTrackingBox.width > 0 && savedTrackingBox.height > 0;
    if (templateFilesValid && trackingBoxValid) {
        if (!hasValidBoxes) {
            qDebug() << "[TEMPLATE_REPAIR] repairing hasValidBoxes:" << dirPath;
            privateSettings.hasValidBoxes = true;
            QString repairError;
            if (!AppSettingsManager::saveTemplatePrivateSettings(dirPath, privateSettings, &repairError)) {
                qDebug() << "[TEMPLATE_REPAIR] save failed:" << repairError;
                return false;
            }
        }
        hasValidBoxes = true;
    } else {
        hasValidBoxes = false;
    }

    updateCurrentTemplateName();
    return true;
}


/**
 * @brief 加载设置
 * @details 从当前用户 AppData 加载软件公共设置
 */
void Widget::loadSettings()
{
    QString errorMessage;
    if (!m_settingsPageController
            || !m_settingsPageController->load(&errorMessage)) {
        qDebug() << "[GLOBAL_SETTINGS] load failed, using defaults:"
                 << errorMessage;
    }
    m_templateEditorController->modeMemory().templatePathsByMode() =
            m_appliedGlobalSettings.templateDirPathsByMode;
    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            m_appliedGlobalSettings.publishedRecipeIdsByMode;
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
    applyTissueRoughnessThresholdFromUi(false);
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
        qDebug() << "[GLOBAL_SETTINGS] silent save failed:"
                 << errorMessage;
    }
    return false;
}

void Widget::applyGlobalSettingsToUi(
    const GlobalSettings &settings)
{
    m_templateEditorController->modeMemory().templatePathsByMode() =
            settings.templateDirPathsByMode;
    m_templateEditorController->modeMemory().publishedRecipeIdsByMode() =
            settings.publishedRecipeIdsByMode;
    if (m_settingsPageController) {
        m_settingsPageController->applyToUi(settings);
    }
    m_currentDetectModeId = currentDetectModeId();
    restoreTemplatesForMode(m_currentDetectModeId, false);
}

void Widget::applyTemplatePrivateSettingsToUi(const TemplatePrivateSettings &settings)
{
    QSignalBlocker targetBlocker(ui->dateEdit);
    QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
    ui->dateEdit->setPlainText(settings.targetText);
    ui->lineEdit_yuzhi->setText(QString::number(static_cast<int>(settings.imageThreshold)));
    refreshTemplatePrivateSettingDirty();
}

/**
 * @brief 设置非公共配置初始值
 * @details 公共配置统一由 AppSettingsManager::defaultGlobalSettings() 提供
 */
void Widget::setupNonPersistentDefaults()
{
    ui->lineEdit_yuzhi->setText("70");
    ui->dateEdit->setPlainText("");
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
    qDebug() << "=== on_plcbtn_clicked() START ===";

    InspectionStartAccessInput startAccess;
    startAccess.templateOperationActive =
            m_templateCaptureState != TemplateCaptureState::Idle
            || m_operationState == OperationState::TemplatePreviewing
            || m_operationState == OperationState::TemplateFrozen;
    startAccess.runtimeBusy =
            m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || isCollecting
            || hasRunningInspectionThread()
            || m_runtimeController.isBusy();
    startAccess.cameraOpen = m_bOpenDevice;

    InspectionStartPreflightResult accessResult =
            InspectionStartPreflight::evaluateAccess(startAccess);
    if (accessResult.issue
            == InspectionStartIssue::TemplateOperationActive) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前正在制作模板，请先点击【退出模板制作】。");
        return;
    }
    if (accessResult.issue == InspectionStartIssue::RuntimeBusy) {
        QMessageBox::information(
                    this,
                    "提示",
                    "当前正在识别或停止中，请勿重复启动。");
        return;
    }
    if (accessResult.issue == InspectionStartIssue::CameraClosed) {
        QMessageBox::warning(this, "提示", "请先点击【打开相机】！");
        return;
    }

    updateHardwareParameterUiEnabled();
    m_settingsPageController->refreshAllDirty();
    refreshTemplatePrivateSettingDirty();

    startAccess = InspectionStartAccessInput();
    startAccess.cameraOpen = true;
    startAccess.dirtySettings = hasDirtySettings();
    startAccess.plcTriggerEnabled = ui->checkBox->isChecked();
    startAccess.plcConnected =
            m_runtimeController.isPlcConnected();
    accessResult = InspectionStartPreflight::evaluateAccess(startAccess);

    if (accessResult.issue
            == InspectionStartIssue::DirtySettingsConfirmationRequired) {
        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("提示");
        confirmBox.setText(dirtySettingsMessage());
        QPushButton *continueButton =
                confirmBox.addButton("继续运行", QMessageBox::AcceptRole);
        QPushButton *cancelButton =
                confirmBox.addButton("取消", QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();

        if (confirmBox.clickedButton() != continueButton) {
            return;
        }

        restoreUnappliedSettingsFromApplied();
        startAccess.dirtySettings = false;
        startAccess.plcTriggerEnabled = ui->checkBox->isChecked();
        startAccess.plcConnected =
                m_runtimeController.isPlcConnected();
        accessResult =
                InspectionStartPreflight::evaluateAccess(startAccess);
    }

    if (accessResult.issue == InspectionStartIssue::PlcDisconnected) {
        showParameterWarning("提示", "已启用 PLC 触发，但 PLC 未连接，请先连接 PLC。");
        return;
    }

    updateCurrentTemplateName();
    const bool isWordMode = isWordFamilyMode(currentDetectModeId());
    const bool isBarcodeWordMode =
            currentDetectModeId() == BarcodeWordDetectionMode;
    const bool isTissueMode = (ui->comboBox_4->currentIndex() == 3);
    const bool isWordProfileMode =
            isWordMode && !m_templateEditorController->wordTemplateProfiles().empty();

    InspectionStartResourceInput resourceInput;
    if (isTissueMode) {
        resourceInput.modeKind = InspectionStartModeKind::Tissue;
    } else if (isBarcodeWordMode) {
        resourceInput.modeKind =
                InspectionStartModeKind::BarcodeWordProfiles;
    } else if (isWordMode) {
        resourceInput.modeKind = InspectionStartModeKind::WordProfiles;
    } else {
        resourceInput.modeKind =
                InspectionStartModeKind::SingleTemplate;
    }
    resourceInput.productTemplateDirectorySelected =
            !currentTemplateDirPath.trimmed().isEmpty();
    resourceInput.trackingTemplateReady =
            !m_loadedTrackingTemplate.empty();
    resourceInput.dateRegionReady = !savedDatePoly.empty();

    if (isWordMode) {
        resourceInput.barcodeDecoderReady = !isBarcodeWordMode;
        if (isBarcodeWordMode
                && !m_templateEditorController->wordTemplateProfiles().empty()) {
            resourceInput.barcodeDecoderReady =
                    m_barcodeDecoder->ensureLoaded();
            if (!resourceInput.barcodeDecoderReady) {
                resourceInput.barcodeDecoderError =
                        m_barcodeDecoder->lastError();
            }
        }

        for (const WordTemplateProfile &profile
             : m_templateEditorController->wordTemplateProfiles()) {
            const QString profileName = profile.name.isEmpty()
                    ? QDir(profile.dirPath).dirName()
                    : profile.name;
            InspectionStartProfileReadiness readiness;
            readiness.displayName = profileName;
            readiness.targetTextReady =
                    !profile.settings.targetText.trimmed().isEmpty();
            readiness.characterTemplatesReady =
                    !profile.digitTemplates.empty();

            if (isBarcodeWordMode) {
                const QString trackingPath =
                        wordTemplateProfileAssetPath(
                            profile,
                            QStringLiteral("trackingTemplate"),
                            QStringLiteral("tracking_template.bmp"));
                QFile trackingFile(trackingPath);
                cv::Mat diskTrackingTemplate;
                if (trackingFile.open(QIODevice::ReadOnly)) {
                    const QByteArray bytes = trackingFile.readAll();
                    if (!bytes.isEmpty()) {
                        try {
                            const std::vector<uchar> buffer(
                                        bytes.begin(),
                                        bytes.end());
                            diskTrackingTemplate =
                                    cv::imdecode(
                                        buffer,
                                        cv::IMREAD_COLOR);
                        } catch (...) {
                            diskTrackingTemplate.release();
                        }
                    }
                }
                readiness.trackingTemplateReady =
                        !profile.trackingTemplate.empty()
                        && !diskTrackingTemplate.empty();

                CalibrationData diskCalibration;
                const QString calibrationPath =
                        wordTemplateProfileAssetPath(
                            profile,
                            QStringLiteral("calibration"),
                            QStringLiteral("calibrate_config.yaml"));
                readiness.calibrationReady =
                        QFileInfo::exists(calibrationPath)
                        && diskCalibration.load(
                            calibrationPath
                            .toLocal8Bit()
                            .toStdString());
                if (readiness.calibrationReady) {
                    readiness.barcodeRegionReady =
                            diskCalibration.barcode_poly.size() == 4
                            && profile.barcodePoly.size() == 4;
                    readiness.dateRegionReady =
                            diskCalibration.date_poly.size() >= 3
                            && profile.datePoly.size() >= 3;
                }

                readiness.targetTextReady =
                        readiness.targetTextReady
                        && profile.targetCount > 0;
                readiness.characterTemplatesReady =
                        readiness.characterTemplatesReady
                        && profile.digitTemplates.size()
                           == profile.digitTemplateTargetIndexes.size();
            }
            resourceInput.profiles.push_back(readiness);
        }
    }

    const InspectionStartPreflightResult resourceResult =
            InspectionStartPreflight::evaluateResources(resourceInput);
    if (resourceResult.issue
            == InspectionStartIssue::WordProfilesMissing) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "当前没有加载产品模板，请重新选择产品模板文件夹。");
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::BarcodeResourcesInvalid) {
        QMessageBox::warning(
                    this,
                    "二维码+三期模板预检失败",
                    QString("以下问题必须处理后才能启动检测：\n\n%1")
                    .arg(resourceResult.details.join("\n")));
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::ProductTemplateIncomplete) {
        QMessageBox::warning(this, "操作规范",
                             QString("缺少可用产品模板，无法启动检测。\n\n"
                                     "具体原因：\n%1\n\n"
                                     "如果是新产品：\n"
                                     "请先【拍照】，框选定位区域和喷码检测区域，然后点击【保存模板】。\n\n"
                                     "如果是已有产品：\n"
                                     "请点击【选择模板】，选择对应产品模板文件夹。")
                             .arg(resourceResult.details.join("\n")));
        return;
    }
    if (resourceResult.issue
            == InspectionStartIssue::WordProfilesIncomplete) {
        QMessageBox::warning(
                    this,
                    "提示",
                    QString("以下产品模板还没有确认目标字符，不能启动检测：\n%1")
                    .arg(resourceResult.details.join("\n")));
        return;
    }

    const InspectionRunPlan runPlan =
            InspectionRunConfiguration::createPlan(
                resourceInput.modeKind,
                ui->checkBox->isChecked());

    InspectionProfileSnapshot profileSnapshotForRun;
    if (isWordProfileMode) {
        profileSnapshotForRun = createWordTemplateRunSnapshot();
        if (!profileSnapshotForRun.isValid()) {
            QMessageBox::warning(this, "提示", "没有可用的字库定位配置。");
            return;
        }
    }

    m_barcodeWordRunActive = false;

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();
    if (m_runtimeUiCoordinator) {
        m_runtimeUiCoordinator->clearDetectionRoiWarning(QString());
    }

    // ==========================================================
    // 以下为原有启动线程逻辑，完全保留你所有的 PLC/相机 流程
    // ==========================================================
    if (runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger)
    {
        // 外部触发/硬触发模式逻辑
        if (isCollecting) {
            QMessageBox::information(this, "提示", "已在采集中，若要停止请点击【停止识别】按钮");
            return;
        }

        QStringList applyErrors;
        if (!applyCameraHardwareSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = ui->lineEdit_14->text().toFloat();

        ui->image_undetected->clear();
        ui->imagenum->clear();
        ui->ngnum->clear();
        ui->resultlabel_7->clear();
        ui->speedLabel->clear();
        m_runtimeController.resetStatistics();

        // 重置相机状态。具体SDK调用顺序由运行层统一维护。
        if (m_acquisitionController
                && m_acquisitionController->hasCamera()
                && m_bOpenDevice) {
            const InspectionCameraStartResult cameraStartResult =
                    m_acquisitionController->applyCameraStart(
                InspectionAcquisitionKind::HardwareTrigger,
                gainValue,
                [this](QString *errorMessage) {
                return applyCameraExposureValue(
                            m_appliedGlobalSettings.cameraExposure,
                            errorMessage);
            });
            if (!cameraStartResult.isAccepted()) {
                if (cameraStartResult.issue
                        == InspectionCameraStartIssue::ExposureRejected) {
                    QMessageBox::warning(
                                this,
                                "启动失败",
                                QString("切换硬触发模式后恢复相机曝光失败：\n%1")
                                .arg(cameraStartResult.errorMessage));
                } else {
                    QMessageBox::critical(this, "错误", "相机初始化失败！");
                }
                return;
            }
        }

        m_acquisitionController->configureHardwareWorker(
                    runPlan,
                    profileSnapshotForRun.trackingProfiles,
                    savedDatePoly,
                    savedTrackingBox,
                    m_loadedTrackingTemplate);

        applyErrors.clear();
        if (!applyRuntimeThreadSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!applyPlcTriggerModeFromUi(&applyErrors, false)
                || !applyPlcRunSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前 PLC 参数下发失败：\n") + applyErrors.join("\n"));
            return;
        }

        // 发送模板匹配相关参数
        emit jiancestring(ui->dateEdit->toPlainText().toStdString());

        if (isWordProfileMode) {
            qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                     << static_cast<int>(
                            profileSnapshotForRun.detectionProfiles.size())
                     << "mode:" << currentDetectModeId();
        }
        m_barcodeWordRunActive = isBarcodeWordMode;
        m_resultBoundDisplayActive.store(true);
        InspectionRuntimeStartTransaction startTransaction(
                    m_runtimeController);
        if (!startTransaction.begin()) {
            m_resultBoundDisplayActive.store(false);
            m_barcodeWordRunActive = false;
            QMessageBox::warning(
                        this,
                        QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                        QStringLiteral("\u5f53\u524d\u8fd0\u884c\u72b6\u6001\u4e0d\u5141\u8bb8\u91cd\u590d\u542f\u52a8\u3002"));
            return;
        }
        qDebug() << "[RUNTIME_CONTROLLER] starting"
                 << startTransaction.runId();
        QString workerError;
        if (!startDetectionWorkerForMode(
                    startTransaction,
                    ui->comboBox_4->currentIndex(),
                    profileSnapshotForRun,
                    &workerError)) {
            startTransaction.rollback();
            m_resultBoundDisplayActive.store(false);
            m_barcodeWordRunActive = false;
            QMessageBox::warning(
                        this,
                        QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                        workerError);
            return;
        }
        qDebug() << "[DETECTION_WORKER] hard-trigger ingress enabled"
                 << "modeIndex=" << ui->comboBox_4->currentIndex();
        if (m_acquisitionController->startHardwareWorker()) {
            if (!startTransaction.commit()) {
                m_acquisitionController->requestHardwareStop();
                m_acquisitionController->waitForHardware(1500);
                m_resultBoundDisplayActive.store(false);
                m_barcodeWordRunActive = false;
                isCollecting = false;
                m_operationState = m_bOpenDevice
                        ? OperationState::CameraReady
                        : OperationState::CameraClosed;
                updateOperationUiState();
                QMessageBox::warning(
                            this,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8fd0\u884c\u72b6\u6001\u63d0\u4ea4\u5931\u8d25\u3002"));
                return;
            }
            isCollecting = true;
            m_operationState =
                    OperationState::Detecting;
            ui->statusLabel->setText("触发模式运行中");
            updateOperationUiState();
        } else {
            startTransaction.rollback();
            m_resultBoundDisplayActive = false;
            m_barcodeWordRunActive = false;
            isCollecting = false;
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            updateOperationUiState();
        }
    }
    else
    {
        // 软触发/连续模式逻辑
        QStringList applyErrors;
        if (!applyCameraHardwareSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前相机参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }
        const float gainValue = ui->lineEdit_14->text().toFloat();

        m_acquisitionController->ensureWorkersReady();
        m_acquisitionController->configureSoftwareWorker(
                    runPlan,
                    profileSnapshotForRun.trackingProfiles,
                    savedDatePoly,
                    savedTrackingBox,
                    m_loadedTrackingTemplate);

        applyErrors.clear();
        if (!applyRuntimeThreadSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前运行参数应用失败：\n") + applyErrors.join("\n"));
            return;
        }

        applyErrors.clear();
        if (!applyPlcTriggerModeFromUi(&applyErrors, false)
                || !applyPlcRunSettingsFromUi(&applyErrors, false)) {
            QMessageBox::warning(this, "启动失败",
                                 QString("启动识别前 PLC 参数下发失败：\n") + applyErrors.join("\n"));
            return;
        }

        const InspectionCameraStartResult cameraStartResult =
                m_acquisitionController->applyCameraStart(
            InspectionAcquisitionKind::SoftwareTrigger,
            gainValue,
            [this](QString *errorMessage) {
            return applyCameraExposureValue(
                        m_appliedGlobalSettings.cameraExposure,
                        errorMessage);
        });
        if (!cameraStartResult.isAccepted()) {
            if (cameraStartResult.issue
                    == InspectionCameraStartIssue::ExposureRejected) {
                QMessageBox::warning(
                            this,
                            "启动失败",
                            QString("切换软触发模式后恢复相机曝光失败：\n%1")
                            .arg(cameraStartResult.errorMessage));
            } else {
                QMessageBox::critical(this, "错误", "相机初始化失败！");
            }
            return;
        }
        if (!m_acquisitionController->isSoftwareRunning()) {
            if (isWordProfileMode) {
                qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                         << static_cast<int>(
                                profileSnapshotForRun.detectionProfiles.size())
                         << "mode:" << currentDetectModeId();
            }
            m_resultBoundDisplayActive.store(true);
            // Publish the selected orchestration mode before acquisition can
            // emit its first frame.
            m_barcodeWordRunActive = isBarcodeWordMode;
            InspectionRuntimeStartTransaction startTransaction(
                        m_runtimeController);
            if (!startTransaction.begin()) {
                m_resultBoundDisplayActive.store(false);
                m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            this,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u5f53\u524d\u8fd0\u884c\u72b6\u6001\u4e0d\u5141\u8bb8\u91cd\u590d\u542f\u52a8\u3002"));
                return;
            }
            qDebug() << "[RUNTIME_CONTROLLER] starting"
                     << startTransaction.runId();
            QString workerError;
            if (!startDetectionWorkerForMode(
                        startTransaction,
                        ui->comboBox_4->currentIndex(),
                        profileSnapshotForRun,
                        &workerError)) {
                startTransaction.rollback();
                m_resultBoundDisplayActive.store(false);
                m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            this,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            workerError);
                return;
            }
            if (!m_acquisitionController->startSoftwareWorker()) {
                startTransaction.rollback();
                m_resultBoundDisplayActive.store(false);
                m_barcodeWordRunActive = false;
                QMessageBox::warning(
                            this,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8f6f\u89e6\u53d1\u91c7\u96c6\u7ebf\u7a0b\u542f\u52a8\u5931\u8d25\u3002"));
                return;
            }
            if (!startTransaction.commit()) {
                m_acquisitionController->requestSoftwareStop();
                m_acquisitionController->waitForSoftware(1500);
                m_resultBoundDisplayActive.store(false);
                m_barcodeWordRunActive = false;
                isCollecting = false;
                m_operationState = m_bOpenDevice
                        ? OperationState::CameraReady
                        : OperationState::CameraClosed;
                updateOperationUiState();
                QMessageBox::warning(
                            this,
                            QStringLiteral("\u542f\u52a8\u5931\u8d25"),
                            QStringLiteral("\u8fd0\u884c\u72b6\u6001\u63d0\u4ea4\u5931\u8d25\u3002"));
                return;
            }
            isCollecting = true;
            m_operationState =
                    OperationState::Detecting;
            ui->statusLabel->setText("软触发模式运行中");
            updateOperationUiState();
        }
    }


    qDebug() << "=== on_plcbtn_clicked() COMPLETED ===";
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
            m_appliedGlobalSettings.cameraExposure;
    const InspectionCameraOpenResult openResult =
            m_acquisitionController->openFirstCamera(
                savedExposure,
                [this, savedExposure](
                    int adjustedExposure,
                    QString *saveError) {
        m_appliedGlobalSettings.cameraExposure = adjustedExposure;
        if (saveSettings(false)) {
            return true;
        }
        m_appliedGlobalSettings.cameraExposure = savedExposure;
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
                m_appliedGlobalSettings.cameraExposure);
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
    if (applyTissueRoughnessThresholdFromUi(true)) {
        m_settingsPageController->updateAppliedFromUi("tissue.roughness_threshold");
        m_settingsPageController->refreshDirty("tissue.roughness_threshold");
        saveSettings(false);
    }
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
