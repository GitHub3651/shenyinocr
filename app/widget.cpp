/**
 * @file widget.cpp
 * @brief 工业视觉识别系统主窗口实现文件
 * @details 实现图像采集、OCR识别、模板匹配、PLC通信等核心功能
 * @author 优化版本
 * @date 2024
 */

#include "widget.h"
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
#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/ocr/ocr_detection_pipeline.h"
#include "detection/stamp/stamp_detection_pipeline.h"
#include "detection/word/word_detection_pipeline.h"
#include "devices/camera/hikvision_camera_device.h"
#include "devices/ocr/paddle_ocr_engine.h"
#include "devices/plc/snap7_plc_device.h"
#include "runtime/image_save_service.h"


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
#include <QPainter>
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
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QUuid>
#include <QApplication>
#include <QCoreApplication>
#include <QTranslator>
#include <QIcon>
#include <QCamera>
#include <QCameraInfo>
#include <QDesktopWidget>
#include <QSplashScreen>
#include <QTextCodec>
#include <QElapsedTimer>
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
#include <QIntValidator>
#include <QDoubleValidator>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QEvent>
#include <QRegularExpression>
#include <QAbstractButton>
#include <QSplitterHandle>
#include <QStyle>

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
double elapsedMilliseconds(const QElapsedTimer &timer)
{
    return static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
}

QString normalizedImageFormat(QString format)
{
    format = format.trimmed();
    if (format.startsWith(QStringLiteral("."))) {
        format.remove(0, 1);
    }
    return format.isEmpty() ? QStringLiteral("png") : format.toLower();
}

QString imageSaveFilePath(
    const QString &directoryPath,
    const QString &baseName,
    const QString &format)
{
    return QDir(directoryPath).filePath(baseName + QStringLiteral(".") + format);
}

ImageSaveItem makeImageSaveItem(
    const QImage &image,
    const std::shared_ptr<const FrameData> &frame,
    const QString &directoryPath,
    const QString &baseName,
    const QString &format)
{
    ImageSaveItem item;
    item.image = image;
    item.frame = frame;
    item.filePath = imageSaveFilePath(directoryPath, baseName, format);
    item.format = format.toUpper().toLatin1();
    return item;
}

void setLabelTextIfChanged(QLabel *label, const QString &text)
{
    if (label && label->text() != text) {
        label->setText(text);
    }
}

const QStringList &detectModeIds()
{
    static const QStringList ids = {
        "stamp_detection",
        "word_detection",
        "ocr_detection",
        "tissue_detection",
        BarcodeWordDetectionMode
    };
    return ids;
}

const QStringList &imageSaveModeIds()
{
    static const QStringList ids = {"save_none", "save_ng", "save_ok", "save_all"};
    return ids;
}

const QStringList &imageSaveTypeIds()
{
    static const QStringList ids = {"save_both", "save_annotated_only", "save_raw_only"};
    return ids;
}

const QStringList &colorChannelIds()
{
    static const QStringList ids = {"color", "red", "green", "blue"};
    return ids;
}

const QStringList &rotationIds()
{
    static const QStringList ids = {"rotate_none", "rotate_clockwise_90", "rotate_counterclockwise_90", "rotate_180"};
    return ids;
}

const QStringList &triggerModeIds()
{
    static const QStringList ids = {"trigger_continuous", "trigger_interval"};
    return ids;
}

QString idAt(const QStringList &ids, int index, const QString &fallback)
{
    return (index >= 0 && index < ids.size()) ? ids.at(index) : fallback;
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

bool parseDoubleValue(const QString &text, double *value)
{
    bool ok = false;
    const double parsed = text.trimmed().toDouble(&ok);
    if (!ok) {
        return false;
    }
    if (value) {
        *value = parsed;
    }
    return true;
}

bool fuzzyEqual(double left, double right)
{
    return std::fabs(left - right) < 0.000001;
}

bool isSingleTemplateRecipeMode(const QString &modeId)
{
    DetectionMode mode;
    return detectionModeFromId(modeId, &mode)
            && (mode == DetectionMode::Stamp
                || mode == DetectionMode::Ocr);
}

cv::Mat decodeImageFile(const QString &filePath, int flags)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return cv::Mat();
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return cv::Mat();
    }

    try {
        const std::vector<uchar> buffer(bytes.begin(), bytes.end());
        return cv::imdecode(buffer, flags);
    } catch (...) {
        return cv::Mat();
    }
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





struct CVDrawResult {
    std::vector<cv::Point> poly;
    double score;
};
static std::vector<CVDrawResult> g_lastDrawResults;
static DetectionPose g_lastPose;
static qint64 g_lastDetectTime = 0;

// ============ 新增：用于绘制钢印的数据缓存 ============
static std::vector<cv::Point> g_lastStampPoly; // 保存钢印的多边形坐标
static bool g_lastStampIsOverlap = false;      // 记录钢印是否发生重叠
static bool g_allowTissueDetectionFrameDisplay = false;
static TissueRollItem g_lastTissueRoll;
static bool g_hasLastTissueRoll = false;

static cv::Mat makeBgrCopy(const cv::Mat& image)
{
    if (image.empty()) {
        return cv::Mat();
    }
    if (image.channels() == 3) {
        return image.clone();
    }

    cv::Mat bgr;
    if (image.channels() == 1) {
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
    }
    return bgr;
}

static void drawTissueRollOverlay(cv::Mat& image, const TissueRollItem& roll)
{
    if (image.empty()) {
        return;
    }

    const double dynamicScale = std::max(1.0, image.rows / 800.0);
    const int boxThickness = std::max(2, static_cast<int>(2 * dynamicScale));
    const int outerRadius = std::max(1, cvRound(std::max(roll.outerAxes.width, roll.outerAxes.height)));
    const int innerRadius = std::max(1, cvRound(std::max(roll.innerAxes.width, roll.innerAxes.height)));

    cv::circle(image,
               cv::Point(cvRound(roll.center.x), cvRound(roll.center.y)),
               outerRadius,
               cv::Scalar(0, 255, 255),
               boxThickness);
    cv::circle(image,
               cv::Point(cvRound(roll.innerCenter.x), cvRound(roll.innerCenter.y)),
               innerRadius,
               cv::Scalar(255, 0, 0),
               boxThickness);
}

static cv::Point2f transformPoint(const cv::Mat& affine, const cv::Point2f& pt)
{
    return cv::Point2f(
        static_cast<float>(affine.at<double>(0, 0) * pt.x + affine.at<double>(0, 1) * pt.y + affine.at<double>(0, 2)),
        static_cast<float>(affine.at<double>(1, 0) * pt.x + affine.at<double>(1, 1) * pt.y + affine.at<double>(1, 2))
    );
}

static std::vector<cv::Point> transformPolygon(const std::vector<cv::Point>& poly, const cv::Mat& affine)
{
    std::vector<cv::Point> transformed;
    transformed.reserve(poly.size());
    for (const auto& pt : poly) {
        const cv::Point2f mapped = transformPoint(affine, cv::Point2f(static_cast<float>(pt.x), static_cast<float>(pt.y)));
        transformed.emplace_back(cvRound(mapped.x), cvRound(mapped.y));
    }
    return transformed;
}

static QString formatPolygonPoints(const std::vector<cv::Point>& poly)
{
    QStringList points;
    points.reserve(static_cast<int>(poly.size()));
    for (const cv::Point &point : poly) {
        points.append(QString("(%1,%2)").arg(point.x).arg(point.y));
    }
    return QString("[%1]").arg(points.join(","));
}

static cv::Rect expandAndClampRect(const cv::Rect& rect, int padding, const cv::Size& bounds)
{
    cv::Rect expanded(rect.x - padding,
                      rect.y - padding,
                      rect.width + padding * 2,
                      rect.height + padding * 2);
    return expanded & cv::Rect(0, 0, bounds.width, bounds.height);
}

static cv::Point getPolygonTopCenter(const std::vector<cv::Point>& poly)
{
    if (poly.empty()) {
        return cv::Point();
    }
    if (poly.size() == 1) {
        return poly.front();
    }

    std::vector<cv::Point> sorted = poly;
    std::sort(sorted.begin(), sorted.end(), [](const cv::Point& lhs, const cv::Point& rhs) {
        if (lhs.y != rhs.y) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });

    const cv::Point& p1 = sorted[0];
    const cv::Point& p2 = sorted[1];
    return cv::Point((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
}

static OrientedDateRoi prepareOrientedDateRoi(const cv::Mat& src, const DetectionPose& pose, int padding)
{
    OrientedDateRoi oriented;
    if (src.empty() || !pose.valid || pose.datePoly.size() < 3) {
        return oriented;
    }

    oriented.rotationMatrix = cv::getRotationMatrix2D(pose.anchorCenter, -pose.angleDeg, 1.0);
    cv::invertAffineTransform(oriented.rotationMatrix, oriented.inverseRotationMatrix);
    cv::warpAffine(src, oriented.rotatedImage, oriented.rotationMatrix, src.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    oriented.rotatedDatePoly = transformPolygon(pose.datePoly, oriented.rotationMatrix);
    if (oriented.rotatedDatePoly.size() < 3) {
        return oriented;
    }

    oriented.roi = expandAndClampRect(cv::boundingRect(oriented.rotatedDatePoly), padding, oriented.rotatedImage.size());
    if (oriented.roi.width <= 0 || oriented.roi.height <= 0) {
        return oriented;
    }

    oriented.croppedImage = oriented.rotatedImage(oriented.roi).clone();
    if (oriented.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (oriented.croppedImage.channels() == 1) {
            cv::cvtColor(oriented.croppedImage, converted, cv::COLOR_GRAY2BGR);
        } else if (oriented.croppedImage.channels() == 4) {
            cv::cvtColor(oriented.croppedImage, converted, cv::COLOR_BGRA2BGR);
        } else {
            converted = oriented.croppedImage.clone();
        }
        oriented.croppedImage = converted;
    }

    oriented.valid = !oriented.croppedImage.empty();
    return oriented;
}

struct BarcodeWordOrientedRois
{
    OrientedBarcodeRoi barcode;
    OrientedDateRoi date;
};

static BarcodeWordOrientedRois prepareBarcodeWordOrientedRois(
    const cv::Mat& src,
    const DetectionPose& pose,
    int barcodePaddingPercent,
    int datePadding)
{
    BarcodeWordOrientedRois prepared;
    if (src.empty() || !pose.valid || pose.barcodePoly.size() != 4) {
        return prepared;
    }

    const cv::Mat rotationMatrix =
            cv::getRotationMatrix2D(
                pose.anchorCenter,
                -pose.angleDeg,
                1.0);
    cv::Mat inverseRotationMatrix;
    cv::invertAffineTransform(
                rotationMatrix,
                inverseRotationMatrix);

    const std::vector<cv::Point> rotatedBarcodePoly =
            transformPolygon(
                pose.barcodePoly,
                rotationMatrix);
    if (rotatedBarcodePoly.size() != 4) {
        return prepared;
    }

    const cv::Rect barcodeBounds =
            cv::boundingRect(rotatedBarcodePoly);
    if (barcodeBounds.width <= 0 || barcodeBounds.height <= 0) {
        return prepared;
    }

    const int shorterBarcodeSide =
            std::min(
                barcodeBounds.width,
                barcodeBounds.height);
    const int barcodePaddingPixels =
            cvRound(
                static_cast<double>(shorterBarcodeSide)
                * static_cast<double>(
                    std::max(0, barcodePaddingPercent))
                / 100.0);
    const cv::Rect barcodeRoi =
            expandAndClampRect(
                barcodeBounds,
                barcodePaddingPixels,
                src.size());
    if (barcodeRoi.width <= 0 || barcodeRoi.height <= 0) {
        return prepared;
    }

    std::vector<cv::Point> rotatedDatePoly;
    cv::Rect dateRoi;
    bool hasValidDateRoi = false;
    if (pose.datePoly.size() >= 3) {
        rotatedDatePoly =
                transformPolygon(
                    pose.datePoly,
                    rotationMatrix);
        if (rotatedDatePoly.size() >= 3) {
            dateRoi =
                    expandAndClampRect(
                        cv::boundingRect(rotatedDatePoly),
                        std::max(0, datePadding),
                        src.size());
            hasValidDateRoi =
                    dateRoi.width > 0
                    && dateRoi.height > 0;
        }
    }

    const cv::Rect combinedRoi =
            hasValidDateRoi
            ? (barcodeRoi | dateRoi)
            : barcodeRoi;
    if (combinedRoi.width <= 0 || combinedRoi.height <= 0) {
        return prepared;
    }

    cv::Mat localRotationMatrix = rotationMatrix.clone();
    localRotationMatrix.at<double>(0, 2) -= combinedRoi.x;
    localRotationMatrix.at<double>(1, 2) -= combinedRoi.y;

    cv::Mat rotatedRegion;
    cv::warpAffine(
                src,
                rotatedRegion,
                localRotationMatrix,
                combinedRoi.size(),
                cv::INTER_LINEAR,
                cv::BORDER_REPLICATE);
    if (rotatedRegion.empty()) {
        return prepared;
    }

    const cv::Rect localBarcodeRoi(
                barcodeRoi.x - combinedRoi.x,
                barcodeRoi.y - combinedRoi.y,
                barcodeRoi.width,
                barcodeRoi.height);
    const cv::Mat barcodeCrop =
            rotatedRegion(localBarcodeRoi);
    if (barcodeCrop.channels() == 1) {
        if (barcodeCrop.depth() == CV_8U) {
            prepared.barcode.grayRoi =
                    barcodeCrop.clone();
        } else {
            barcodeCrop.convertTo(
                        prepared.barcode.grayRoi,
                        CV_8U);
        }
    } else if (barcodeCrop.channels() == 3) {
        cv::cvtColor(
                    barcodeCrop,
                    prepared.barcode.grayRoi,
                    cv::COLOR_BGR2GRAY);
    } else if (barcodeCrop.channels() == 4) {
        cv::cvtColor(
                    barcodeCrop,
                    prepared.barcode.grayRoi,
                    cv::COLOR_BGRA2GRAY);
    }

    if (!prepared.barcode.grayRoi.empty()
            && !prepared.barcode.grayRoi.isContinuous()) {
        prepared.barcode.grayRoi =
                prepared.barcode.grayRoi.clone();
    }

    prepared.barcode.rotatedImage = rotatedRegion;
    prepared.barcode.rotatedBarcodePoly =
            rotatedBarcodePoly;
    prepared.barcode.roi = barcodeRoi;
    prepared.barcode.rotationMatrix =
            rotationMatrix;
    prepared.barcode.inverseRotationMatrix =
            inverseRotationMatrix;
    prepared.barcode.valid =
            !prepared.barcode.grayRoi.empty()
            && prepared.barcode.grayRoi.type() == CV_8UC1
            && prepared.barcode.grayRoi.isContinuous();

    if (!hasValidDateRoi) {
        return prepared;
    }

    const cv::Rect localDateRoi(
                dateRoi.x - combinedRoi.x,
                dateRoi.y - combinedRoi.y,
                dateRoi.width,
                dateRoi.height);
    prepared.date.croppedImage =
            rotatedRegion(localDateRoi).clone();
    if (prepared.date.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (prepared.date.croppedImage.channels() == 1) {
            cv::cvtColor(
                        prepared.date.croppedImage,
                        converted,
                        cv::COLOR_GRAY2BGR);
        } else if (prepared.date.croppedImage.channels() == 4) {
            cv::cvtColor(
                        prepared.date.croppedImage,
                        converted,
                        cv::COLOR_BGRA2BGR);
        } else {
            converted =
                    prepared.date.croppedImage.clone();
        }
        prepared.date.croppedImage = converted;
    }

    prepared.date.rotatedImage = rotatedRegion;
    prepared.date.rotatedDatePoly = rotatedDatePoly;
    prepared.date.roi = dateRoi;
    prepared.date.rotationMatrix = rotationMatrix;
    prepared.date.inverseRotationMatrix =
            inverseRotationMatrix;
    prepared.date.valid =
            !prepared.date.croppedImage.empty();

    return prepared;
}

static std::vector<CVDrawResult> mapMatchResultsToOriginal(
    const std::vector<std::tuple<cv::Rect, double, size_t>>& matchResults,
    const OrientedDateRoi& oriented,
    const cv::Size& originalSize)
{
    std::vector<CVDrawResult> mapped;
    mapped.reserve(matchResults.size());

    for (const auto& match : matchResults) {
        cv::Rect rect = std::get<0>(match);
        rect.x += oriented.roi.x;
        rect.y += oriented.roi.y;

        const std::vector<cv::Point> rectPoly = {
            cv::Point(rect.x, rect.y),
            cv::Point(rect.x + rect.width, rect.y),
            cv::Point(rect.x + rect.width, rect.y + rect.height),
            cv::Point(rect.x, rect.y + rect.height)
        };
        std::vector<cv::Point> mappedPoly = transformPolygon(rectPoly, oriented.inverseRotationMatrix);
        cv::Rect mappedBounds = cv::boundingRect(mappedPoly) &
                                cv::Rect(0, 0, originalSize.width, originalSize.height);
        if (mappedBounds.width <= 0 || mappedBounds.height <= 0) {
            continue;
        }

        CVDrawResult drawResult;
        drawResult.poly = std::move(mappedPoly);
        drawResult.score = std::get<1>(match);
        mapped.push_back(drawResult);
    }

    return mapped;
}

/**
 * @brief Widget构造函数
 * @param parent 父窗口指针
 * @details 初始化UI、相机、OCR模型、定时器等核心组件
 */
Widget::Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget),
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
    m_barcodeDecoder.reset(new BarcodeDecoderAdapter);
    ui->setupUi(this);
    m_imageSaveService.reset(new ImageSaveService(
                                 32,
                                 ImageSaveService::WriteFunction(),
                                 2));
    connect(m_imageSaveService.get(),
            &ImageSaveService::taskFailed,
            this,
            [this](quint64 totalFailed, const QString &latestError) {
        m_imageSaveFailedCount = totalFailed;
        m_latestImageSaveError = latestError;
        scheduleImageSaveWarning();
    },
    Qt::QueuedConnection);

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

    // UI 文件中已经是 ImageLabel，直接使用
    imageLabel=ui->image_undetected;

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

    // Snap7客户端生命周期和原生常量由设备适配器独占。
    m_plcDevice.reset(new Snap7PlcDevice);
    // 海康单相机SDK生命周期和原生类型由设备适配器独占。
    m_cameraDevice = std::make_shared<HikvisionCameraDevice>();

    // 初始化窗口组件
    initWidget();
    qDebug() << "1. initWidget执行完毕 ";

    // OCR配置、模型和原生调用顺序由设备适配器独占。
    const QString configPath = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("config1.txt"));
    m_ocrEngine.reset(new PaddleOcrEngine(configPath));

    // 初始化统计变量
    hasValidBoxes = false;
    savedTrackingBox = cv::Rect2d(0, 0, 0, 0);
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    recognitionCompletedFlag = false;
    isCollecting = false;
    totalImages = 0;
    ngImages = 0;
    allResults = "";
    wrongindex = ui->lineEdit_12->text().toInt();

    // 设置文本框自动换行
    ui->dateEdit->setWordWrapMode(QTextOption::WordWrap);
    setupTemplatePrivateSettingDirtyTracking();
    setupWordTemplateEditorCombo();
    setupTemplateGuide();
    setupManualCharacterCropUi();
    setupSoftwareSettingsPage();

    // 禁用焦点滚动调节（防误触）- 遍历全局所有下拉框和数字输入框，一劳永逸
    QList<QComboBox *> comboBoxes = this->findChildren<QComboBox *>();
    for (QComboBox *cb : comboBoxes) {
        cb->installEventFilter(this);
    }
    QList<QAbstractSpinBox *> spinBoxes = this->findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *sb : spinBoxes) {
        sb->installEventFilter(this);
    }

    // 连接定时器信号
    connect(timer, &QTimer::timeout, this, &Widget::rightremove);
    connect(imageLabel, &ImageLabel::signal_templateGuideEvent,
            this, &Widget::handleTemplateGuideEvent);

    qDebug() << "6. 变量初始化与信号连接完毕";

    // 公共配置的默认值统一由 AppSettingsManager 提供。
    setupNumericInputValidators();
    setupNonPersistentDefaults();
    qDebug() << "7. setupNonPersistentDefaults 执行完毕";

    loadSettings();
    qDebug() << "8. loadSettings 执行完毕";

    setupDetectModeChangeTracking();
    setupGlobalSettingBindings();
    clearAllGlobalSettingDirty();
    clearTemplatePrivateSettingDirty();
    updateOperationUiState();

    updateCurrentTemplateName();

    QTimer::singleShot(1000, this, [this]() {
        // 1. 先把从界面获取的文本存为一个 QString 变量
        QString targetIp = ui->lineEdit->text();

        const QByteArray address = targetIp.toUtf8();
        const PlcOperationResult result = m_plcDevice->connectTo(
                    address.constData(),
                    ui->lineEdit_2->text().toInt(),
                    ui->lineEdit_3->text().toInt());
        if (result.isSuccess()) {
            updateAppliedGlobalSettingsFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
            refreshGlobalSettingsDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
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

    resetTemplateCaptureState();

    // 先停止线程并断开信号，避免窗口销毁时 queued signal 再访问 ui。
    bool myThreadStopped = true;
    bool cameraThreadStopped = true;
    if (myThread) {
        disconnect(myThread, nullptr, this, nullptr);
        disconnect(this, nullptr, myThread, nullptr);
        if (myThread->isRunning()) {
            myThread->requestStop();
            myThread->stop();
            if (!myThread->wait(3000)) {
                qDebug() << "WARNING: myThread did not stop in destructor";
                myThreadStopped = false;
            }
        }
        if (myThreadStopped && !myThread->isRunning()) {
            delete myThread;
        } else {
            myThread->setParent(nullptr);
        }
        myThread = nullptr;
    }

    if (cameraThread) {
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);
        if (cameraThread->isRunning()) {
            cameraThread->requestStop();
            if (!cameraThread->wait(3000)) {
                qDebug() << "WARNING: cameraThread did not stop in destructor";
                cameraThreadStopped = false;
            }
        }
        if (cameraThreadStopped && !cameraThread->isRunning()) {
            delete cameraThread;
        } else {
            cameraThread->setParent(nullptr);
        }
        cameraThread = nullptr;
    }

    // 线程退出后再关闭相机，避免工作线程仍在访问相机对象。
    if (m_cameraDevice && m_bOpenDevice
            && myThreadStopped && cameraThreadStopped)
    {
        m_cameraDevice->close();
        m_bOpenDevice = false;
        m_cameraDevice.reset();
    } else if (m_cameraDevice && m_bOpenDevice) {
        qDebug() << "WARNING: camera not released because worker thread is still running";
    }

    if (m_plcDevice) {
        if (m_plcDevice->isConnected()) {
            m_plcDevice->disconnect();
        }
        m_plcDevice.reset();
    }

    delete templatematch;
    templatematch = nullptr;

    if (myThreadStopped) {
        delete myImage;
        myImage = nullptr;
    } else {
        qDebug() << "WARNING: myImage not released because myThread is still running";
    }

    try {
        cv::destroyAllWindows();
    } catch (...) {}

    if (m_imageSaveService) {
        m_imageSaveService->shutdown();
        m_imageSaveService.reset();
    }

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

    // 初始化图像指针
    myImage = new Mat();

    // 创建工作线程
    myThread = new MyThread();

    // 创建模板匹配对象
    templatematch = new TemplateMatch();

    // 连接线程信号槽 - 图像显示
    connect(myThread, &MyThread::signal_messImage, this, [this](cv::Mat img) {
        this->handleStreamingFrame(img);
    }, Qt::QueuedConnection);

    // 连接线程信号槽 - 图像检测（根据检测模式选择不同的处理函数）
    QObject::connect(myThread, &MyThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    });
    QObject::connect(myThread, &MyThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);


    // 连接其他信号槽
    connect(myThread, SIGNAL(signal_cleanlabel()), this, SLOT(slot_clearResultLabel()));
    connect(this, &Widget::rotate, myThread, &MyThread::receiveangle);
    connect(this, &Widget::choosechannel,myThread,&MyThread::receivecolorchannel1);
    connect(this, &Widget::sendDataTo, myThread, &MyThread::received);
    connect(this, &Widget::imgshibie, templatematch, &TemplateMatch::receshibie);
    connect(this, &Widget::ssim, templatematch, &TemplateMatch::ssimvalue);
    connectTemplatePreviewSignals(myThread);
}



/**
 * @brief QImage转换为cv::Mat
 * @param image 输入的QImage对象
 * @return cv::Mat 转换后的OpenCV Mat对象
 */
cv::Mat QImage2cvMat(QImage image)
{
    cv::Mat mat;
    switch (image.format())
    {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        cv::Mat mat_temp = cv::Mat(image.height(), image.width(), CV_8UC4,
                                   (void *)image.constBits(), image.bytesPerLine());
        cvtColor(mat_temp, mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                      (void *)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_Indexed8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                      (void *)image.constBits(), image.bytesPerLine());
        break;
    }
    return mat;
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
    if (!m_plcDevice || !m_plcDevice->isConnected())
    {
        return;
    }

    std::uint8_t value = 0;
    unsigned char remove_data[1] = {0};
    remove_data[0] = (unsigned char)(0xFF & value);

    // 写入DB1.1033位置，1个字节
    const PlcOperationResult result = m_plcDevice->writeDbArea(
                1, 1033, 1, PlcDataWidth::Byte, remove_data);
    if (!result.isSuccess())
    {
        QMessageBox::warning(this, "error", "设置失败");
    }
    timer->stop();
}

/**
 * @brief 错误剔除操作（发送信号给PLC）
 * @details 向PLC写入49值，表示产品不合格，需要剔除，100ms后恢复
 */
void Widget::wrongremove()
{
    if (!m_plcDevice || !m_plcDevice->isConnected())
    {
        return;
    }

    std::uint8_t value = 49;
    unsigned char remove_data[1] = {0};
    remove_data[0] = (unsigned char)(0xFF & value);

    // 写入DB1.1033位置，1个字节
    const PlcOperationResult result = m_plcDevice->writeDbArea(
                1, 1033, 1, PlcDataWidth::Byte, remove_data);
    if (!result.isSuccess())
    {
        QMessageBox::warning(this, "error", "设置失败");
    }
    else
    {
        // 100ms后调用rightremove恢复信号
        timer->start(100);
    }
}

// 每次正式启动生成新的运行身份；序号只在形成正式检测完成对象时递增。
void Widget::beginDetectionSession()
{
    m_detectionRunId = QUuid::createUuid()
            .toString(QUuid::WithoutBraces)
            .toLower();
    m_detectionProductSequence = 0;
    qDebug() << "[DETECTION_SESSION] started" << m_detectionRunId;
}

DetectionCompletion Widget::makeDetectionCompletion(
    const cv::Mat &image,
    AlgorithmVerdict verdict,
    const QString &recognizedText,
    const QString &diagnostic,
    double elapsedMs)
{
    if (m_detectionRunId.trimmed().isEmpty()) {
        beginDetectionSession();
    }

    ProductKey productKey;
    productKey.runId = m_detectionRunId;
    productKey.sequence = ++m_detectionProductSequence;

    DetectionCompletion completion;
    completion.frame = makeFrameData(
                productKey,
                productKey.sequence,
                0,
                QDateTime::currentDateTimeUtc(),
                image);
    completion.result.modeId = currentDetectModeId();
    completion.result.verdict = verdict;
    completion.result.status = DetectionStatus::Completed;
    completion.result.recognizedText = recognizedText;
    completion.result.diagnostic = diagnostic;
    completion.result.elapsedMs = elapsedMs;

    const auto appendPolygon = [&completion](
        const QString &role,
        const std::vector<cv::Point> &points,
        double score) {
        if (points.empty()) {
            return;
        }
        DetectionOverlayPolygon polygon;
        polygon.role = role;
        polygon.points = points;
        polygon.score = score;
        completion.result.overlay.polygons.push_back(polygon);
    };
    appendPolygon(QStringLiteral("tracking"), g_lastPose.trackingPoly, g_lastPose.score);
    appendPolygon(QStringLiteral("barcode"), g_lastPose.barcodePoly, 0.0);
    appendPolygon(QStringLiteral("date"), g_lastPose.datePoly, 0.0);
    for (const CVDrawResult &drawResult : g_lastDrawResults) {
        appendPolygon(QStringLiteral("character"), drawResult.poly, drawResult.score);
    }
    appendPolygon(QStringLiteral("stamp"), g_lastStampPoly, 0.0);
    if (g_hasLastTissueRoll) {
        const cv::Rect &box = g_lastTissueRoll.outerBbox;
        const std::vector<cv::Point> tissueBox = {
            cv::Point(box.x, box.y),
            cv::Point(box.x + box.width, box.y),
            cv::Point(box.x + box.width, box.y + box.height),
            cv::Point(box.x, box.y + box.height)
        };
        appendPolygon(
                    QStringLiteral("tissue_roll"),
                    tissueBox,
                    g_lastTissueRoll.roughnessScore);
    }

    return completion;
}

void Widget::scheduleImageSaveWarning()
{
    if (m_imageSaveWarningScheduled) {
        return;
    }
    m_imageSaveWarningScheduled = true;
    QTimer::singleShot(250, this, [this]() {
        m_imageSaveWarningScheduled = false;
        QString warningText = QString::fromWCharArray(
                    L"\u5b58\u56fe\u5931\u8d25\uff1a\u7d2f\u8ba1 %1 \u4e2a\u4efb\u52a1\u3002"
                    L"\u8bf7\u68c0\u67e5\u5b58\u56fe\u76ee\u5f55\u3001\u6743\u9650\u548c\u78c1\u76d8\u7a7a\u95f4\u3002")
                .arg(m_imageSaveFailedCount);
        if (!m_latestImageSaveError.trimmed().isEmpty()) {
            warningText += QString::fromWCharArray(
                        L"\n\u6700\u8fd1\u9519\u8bef\uff1a%1")
                    .arg(m_latestImageSaveError);
        }
        qWarning().noquote() << "[IMAGE_SAVE]" << warningText;
        if (ui && ui->statusLabel) {
            ui->statusLabel->setWordWrap(true);
            ui->statusLabel->setText(warningText);
            ui->statusLabel->setStyleSheet(
                        QStringLiteral(
                            "QLabel{color:#d90000;font-weight:900;}"));
        }
    });
}

// 异步保存本次检测使用的原帧，避免检测完成后再向相机另取一帧。
void Widget::saveImage2Async(
    QString format,
    QString savePath,
    const DetectionCompletion &completion)
{
    if (!completion.isValid()) {
        qDebug() << "保存失败，检测完成对象没有有效原帧";
        return;
    }

    if (!m_imageSaveService) {
        qDebug() << "保存失败，存图服务不可用";
        return;
    }

    format = normalizedImageFormat(format);
    ImageSaveTask task;
    task.productKey = completion.frame->productKey;
    task.items.push_back(makeImageSaveItem(
                             QImage(),
                             completion.frame,
                             savePath,
                             QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz"),
                             format));
    const ImageSaveSubmitResult submitResult =
            m_imageSaveService->submit(task);
    if (!submitResult.isAccepted()) {
        qDebug() << "保存任务提交失败，状态："
                 << static_cast<int>(submitResult.status);
    }
}

bool Widget::shouldSaveRecognitionBoxImage() const
{
    if (!ui || !ui->comboBox_saveImageType) {
        return true;
    }

    const int index = ui->comboBox_saveImageType->currentIndex();
    return index == 0 || index == 1;
}

bool Widget::shouldSaveNoRecognitionBoxImage() const
{
    if (!ui || !ui->comboBox_saveImageType) {
        return false;
    }

    const int index = ui->comboBox_saveImageType->currentIndex();
    return index == 0 || index == 2;
}

void Widget::saveResultImages(
    QString format,
    const QString &resultDirName,
    const DetectionCompletion &completion)
{
    if (!completion.isValid()) {
        qDebug() << "检测图像保存失败，DetectionCompletion无有效原帧";
        return;
    }
    if (selectedDir.trimmed().isEmpty()) {
        qDebug() << "检测图像保存失败，图像保存路径为空";
        return;
    }
    if (!m_imageSaveService) {
        qDebug() << "检测图像保存失败，存图服务不可用";
        return;
    }

    const cv::Mat &image = completion.frame->originalImage;
    format = normalizedImageFormat(format);

    QString resultName = resultDirName.trimmed();
    if (resultName.isEmpty()) {
        resultName = "unknown";
    }

    const QString fileBaseName =
            QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    ImageSaveTask task;
    task.productKey = completion.frame->productKey;
    if (shouldSaveRecognitionBoxImage()) {
        QImage annotatedImage;
        const bool tissueMode = ui
                && ui->comboBox_4
                && ui->comboBox_4->currentIndex() == 3;
        if (tissueMode && g_hasLastTissueRoll) {
            cv::Mat annotatedMat = makeBgrCopy(image);
            if (!annotatedMat.empty()) {
                drawTissueRollOverlay(annotatedMat, g_lastTissueRoll);
                annotatedImage = cvMatToQImage(annotatedMat);
            } else {
                qDebug() << "纸巾带框图生成失败，回退保存界面图像";
            }
        }
        if (annotatedImage.isNull()) {
            const QPixmap *currentPixmap = ui->image_undetected->pixmap();
            if (currentPixmap) {
                annotatedImage = currentPixmap->toImage();
            } else {
                QMessageBox::warning(
                            this,
                            "警告",
                            "保存失败,未采集到图像！");
            }
        }
        if (!annotatedImage.isNull()) {
            task.items.push_back(makeImageSaveItem(
                                     annotatedImage,
                                     std::shared_ptr<const FrameData>(),
                                     selectedDir + "/" + resultName + "/",
                                     fileBaseName,
                                     format));
        }
    }
    if (shouldSaveNoRecognitionBoxImage()) {
        task.items.push_back(makeImageSaveItem(
                                 QImage(),
                                 completion.frame,
                                 selectedDir + "/" + resultName + "_raw/",
                                 fileBaseName,
                                 format));
    }

    if (task.items.empty()) {
        qDebug() << "检测图像保存任务为空";
        return;
    }
    const ImageSaveSubmitResult submitResult =
            m_imageSaveService->submit(task);
    if (!submitResult.isAccepted()) {
        qDebug() << "检测图像保存任务提交失败，状态："
                 << static_cast<int>(submitResult.status);
    }
}

void Widget::saveWordResultImages(
    QString format,
    const QString &resultDirName,
    const DetectionCompletion &completion)
{
    saveResultImages(format, resultDirName, completion);
}

/**
 * @brief 显示图像槽函数
 * @param image OpenCV Mat图像指针
 * @details 将OpenCV图像转换为QPixmap并显示在UI上
 */
void Widget::slot_displayAndDetect(cv::Mat *image)
{
    // 1. 校验图像有效性
    if (!image || image->empty()) return;

    // 纸巾检测生产运行时，画面应当和检测结果绑定。
    // 普通预览帧不再覆盖界面，只有检测槽主动放行的那一帧会显示。
    const bool tissueMode = ui->comboBox_4->currentIndex() == 3;
    const bool productionRunning =
            isCollecting
            || m_operationState
               == OperationState::Detecting
            || m_operationState
               == OperationState::Stopping;
    if (tissueMode && productionRunning && !g_allowTissueDetectionFrameDisplay) {
        return;
    }

    // 2. 深拷贝原图，准备作为画板
    cv::Mat displayImg;
    if (image->channels() == 3) {
        displayImg = image->clone();
    } else if (image->channels() == 1) {
        cv::cvtColor(*image, displayImg, cv::COLOR_GRAY2BGR);
    } else {
        return;
    }

    // 3. 核心重绘机制：只要缓存里还有上一轮检测结果，就持续绘制，直到被新结果覆盖或主动清空。
    if (!g_lastDrawResults.empty() ||
        !g_lastPose.trackingPoly.empty() ||
        !g_lastPose.barcodePoly.empty() ||
        !g_lastPose.datePoly.empty() ||
        !g_lastStampPoly.empty() ||
        (tissueMode && g_hasLastTissueRoll)) {

        // 动态计算自适应比例
        double dynamicScale = std::max(1.0, displayImg.rows / 800.0);
        double fontScale = 0.4 * dynamicScale;

        // 框和字的粗细
        int boxThickness = std::max(2, static_cast<int>(2 * dynamicScale));
        int textThickness = std::max(1, static_cast<int>(1.5 * dynamicScale));

        for (const auto& res : g_lastDrawResults) {
            if (res.poly.size() < 4) {
                continue;
            }

            // 画字符绿框
            std::vector<std::vector<cv::Point>> charPolys = {res.poly};
            cv::polylines(displayImg, charPolys, true, cv::Scalar(0, 255, 0), boxThickness);

            // 分数大于等于0才显示数字 (带描边显示)
            if (res.score >= 0) {
                std::string scoreText = std::to_string(static_cast<int>(res.score * 100));
                int baseline = 0;
                cv::Size textSize = cv::getTextSize(scoreText, cv::FONT_HERSHEY_SIMPLEX, fontScale, textThickness, &baseline);

                cv::Point textAnchor = getPolygonTopCenter(res.poly);
                int textX = std::max(0, std::min(textAnchor.x - textSize.width / 2, displayImg.cols - textSize.width));
                int textY = std::max(textSize.height, std::min(textAnchor.y - 5, displayImg.rows));

                cv::putText(displayImg, scoreText, cv::Point(textX, textY),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(0, 0, 0), textThickness + 2);
                cv::putText(displayImg, scoreText, cv::Point(textX, textY),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(0, 255, 255), textThickness);
            }
        }

        if (!g_lastPose.trackingPoly.empty()) {
            std::vector<std::vector<cv::Point>> trackingPolys = {g_lastPose.trackingPoly};
            cv::polylines(displayImg, trackingPolys, true, cv::Scalar(255, 0, 0), boxThickness);
        }

        // 二维码独立区域：黄色
        if (!g_lastPose.barcodePoly.empty()) {
            std::vector<std::vector<cv::Point>> barcodePolys = {g_lastPose.barcodePoly};
            cv::polylines(displayImg, barcodePolys, true, cv::Scalar(0, 255, 255), boxThickness);
        }

        // ================== 绘制喷码检测区域 ==================
        if (!g_lastPose.datePoly.empty()) {
            std::vector<std::vector<cv::Point>> datePolys = {g_lastPose.datePoly};
            cv::polylines(displayImg, datePolys, true, cv::Scalar(0, 255, 0), boxThickness);
        }

        // ================== 绘制钢印多边形 ==================
        if (!g_lastStampPoly.empty()) {
            // 正常颜色为黄色，重叠则显示红色
            cv::Scalar stampColor = g_lastStampIsOverlap ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 255);
            std::vector<std::vector<cv::Point>> polys = {g_lastStampPoly};
            cv::polylines(displayImg, polys, true, stampColor, boxThickness);
        }

        if (tissueMode && g_hasLastTissueRoll) {
            drawTissueRollOverlay(displayImg, g_lastTissueRoll);
        }
    }

    // 4. OpenCV Mat 转 Qt QImage 显示
    QImage img((const uchar *)displayImg.data, displayImg.cols, displayImg.rows, displayImg.step, QImage::Format_RGB888);
    img = img.rgbSwapped();

    // 🔥 【核心修改：这里彻底删除了 QPainter 绘制“日期”和“钢印”中文标签的所有代码】 🔥

    // 5. 渲染到 UI
    QPixmap pixmap = QPixmap::fromImage(img);

    ui->image_undetected->setScaledContents(false);
    ui->image_undetected->setAlignment(Qt::AlignCenter);
    ui->image_undetected->setAutoFitPixmap(pixmap);
    if (!imageLabel || !imageLabel->isTemplateDrawingEnabled()) {
        updateImageDisplayStatusText("正在显示相机采集图像...");
    }
}


/**
 * @brief 根据 comboBox_4 当前识别模式分发检测逻辑
 * @param image 输入图像指针
 * @param pose 当前检测姿态
 */
void Widget::dispatchDetectionByMode(cv::Mat *image, DetectionPose pose)
{
    if (!image || image->empty()) {
        qDebug() << "[DETECTION_DISPATCH] Empty image, skip detection.";
        return;
    }

    const int mode = m_barcodeWordRunActive
            ? 4
            : ui->comboBox_4->currentIndex();
    if (mode == 4 && !m_barcodeWordRunActive) {
        qDebug() << "[BARCODE_WORD] Ignore detection result because barcode-word run is not active.";
        return;
    }
    if (mode == 2) {
        slot_readAndDetect(image, pose);
    } else if (mode == 0) {
        slot_readAndDetect3(image, pose);
    } else if (mode == 1 || mode == 4) {
        if (!pose.valid) {
            ui->currentTemplateName->setText("--");
            if (mode == 4) {
                BarcodeReadResult barcode;
                finalizeBarcodeWordNg(
                            image,
                            pose,
                            barcode,
                            "未执行",
                            "未找到定位锚点",
                            0);
            } else {
                finalizeWordTrackingNg(
                            image,
                            pose,
                            "未找到字库定位区域");
            }
            return;
        }

        const std::vector<WordTemplateProfile> &detectionProfiles =
                (m_wordTemplateRunActive
                 && !m_runningWordTemplateProfiles.empty())
                ? m_runningWordTemplateProfiles
                : m_wordTemplateProfiles;
        if (pose.wordTemplateProfileIndex >= 0) {
            const int profileIndex = pose.wordTemplateProfileIndex;
            if (profileIndex < static_cast<int>(detectionProfiles.size())) {
                const WordTemplateProfile &profile =
                        detectionProfiles[static_cast<size_t>(profileIndex)];
                if (profile.settings.targetText.trimmed().isEmpty() || profile.digitTemplates.empty()) {
                    qDebug() << "[WORD_TEMPLATE_PROFILE] Selected profile has no target text/templates:"
                             << profileIndex
                             << profile.name;
                    return;
                }

                qDebug() << "[WORD_TEMPLATE_PROFILE] Widget dispatch profile:"
                         << profileIndex
                         << profile.name
                         << "score:" << pose.score;

                const QString currentUsedTemplateName = profile.name.isEmpty()
                        ? QDir(profile.dirPath).dirName()
                        : profile.name;
                ui->currentTemplateName->setText(currentUsedTemplateName.isEmpty()
                                                 ? QString("--")
                                                 : currentUsedTemplateName);

                if (mode == 4) {
                    runBarcodeWordDetection(image, pose, profile);
                } else {
                    runWordTemplateDetection(
                                image,
                                pose,
                                profile.digitTemplates,
                                profile.digitTemplateTargetIndexes,
                                profile.settings.targetText,
                                QString::number(profile.settings.imageThreshold),
                                currentUsedTemplateName,
                                &profile.preparedDigitTemplates);
                }
                return;
            }

            qDebug() << "[WORD_TEMPLATE_PROFILE] Invalid profile index from pose:"
                     << profileIndex
                     << "profile count:" << static_cast<int>(detectionProfiles.size());
            return;
        }

        qDebug() << "[WORD_TEMPLATE] Detection pose did not contain a valid profile index.";
    } else if (mode == 3) {
        qDebug() << "[DETECTION_DISPATCH] Tissue mode is handled in worker thread.";
    } else {
        qDebug() << "[DETECTION_DISPATCH] Unsupported comboBox_4 index:" << mode;
    }
}

/**
 * @brief OCR识别检测槽函数
 * @param image 输入图像指针
 * @param pose 当前检测姿态
 * @details 使用PaddleOCR进行文字识别，支持中英文、数字识别
 */
void Widget::slot_readAndDetect(cv::Mat *image, DetectionPose pose)
{
    // 1. 检查延迟剔除队列
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1)
    {
        qDebug() << "[PLC_LOG] Triggering delayed wrongremove, wrongindex:" << wrongindex;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty())
    {
        qDebug() << "[OCR_ERROR] Invalid input image. Image is null or empty.";
        return;
    }

    // 按周期清理数据，不再清理 imageLabel 的矩形，因为不再绘制
    if (judge)
    {
        j = 1;
        x++;
        judge = false;
    }
    if ((j - 1) % x == 0)
    {
        // imageLabel->clearGreenRects(); // 去掉框显示，不再需要清理
        detectedRects.clear();
        string1.clear();
    }

    qDebug() << "----------------- OCR PROCESS START -----------------";
    OrientedDateRoi oriented = prepareOrientedDateRoi(*image, pose, 0);
    if (!oriented.valid) {
        qDebug() << "[OCR_ERROR] Invalid selection area!";
        return;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();
    g_lastPose = pose;
    g_lastDrawResults.clear();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;

    // ================== 2. 执行 OCR 识别 (原生 Run API) ==================
    QString target_qstring = setdatetime();
    std::string target_string = target_qstring.toStdString();
    ui->imagenum->setText(QString::number(totalImages));

    const OcrDetectionPipeline ocrPipeline;
    const OcrDetectionResult ocrResult =
            ocrPipeline.detect(
                croppedImage,
                target_string,
                *m_ocrEngine);

    allResults = ocrResult.recognizedText;

    // ================== 4. UI 文本更新与 PLC 判定 ==================
    setLabelTextIfChanged(
                ui->resultlabel_7,
                QString::fromStdString(allResults));

    // 🔥 此处删掉了 imageLabel->addSelectionRect 和 imageLabel->update()
    // 界面上不会再出现任何检测框

    qDebug() << "[OCR_LOG] Final String:" << QString::fromStdString(allResults);

    // PLC 判定及存图逻辑
    if (j % x == 0)
    {
        const double completionElapsedMs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                    .count());
        const QString recognizedText = QString::fromStdString(allResults);
        const QString diagnostic = allResults.empty()
                ? QStringLiteral("OCR清洗后文本为空")
                : (ocrResult.isOk
                   ? QStringLiteral("OCR文本与目标完全一致")
                   : QStringLiteral("OCR文本与目标不一致"));
        const DetectionCompletion completion = makeDetectionCompletion(
                    *image,
                    ocrResult.isOk ? AlgorithmVerdict::Ok : AlgorithmVerdict::Ng,
                    recognizedText,
                    diagnostic,
                    completionElapsedMs);
        if (allResults.empty())
        {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3))
                saveImage2Async("jpg", selectedDir + "/ng/", completion);

            ngImages++;
            totalImages++;
            ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        }
        else
        {
            if (ocrResult.isOk)
            {
                totalImages++;
                if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3))
                    saveImage2Async("jpg", selectedDir + "/ok/", completion);

                ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font>"));
                rightremove();
            }
            else
            {
                if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3))
                    saveImage2Async("jpg", selectedDir + "/ng/", completion);

                ngImages++;
                totalImages++;
                ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
                if (wrongindex == 0) wrongremove();
                else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
            }
        }
    }

    // 更新统计
    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0.0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString("%1").arg(ngImages));
    ui->imagenum->setText(QString("%1").arg(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    qDebug() << "----------------- OCR PROCESS END -----------------";
    j++;
}

/**
 * @brief 钢印检测槽函数
 * @param image 输入图像指针
 * @param diffbox 检测区域
 * @details 使用SSIM算法进行模板相似度匹配
 */

//原始图像版
void Widget::slot_readAndDetect3(cv::Mat *image, DetectionPose pose)
{
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        qDebug() << "PLC延迟剔除触发，当前总数:" << totalImages;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) return;

    if (judge) { j = 1; x++; judge = false; }
    if ((j - 1) % x == 0) {
        imageLabel->clearGreenRects();
        detectedRects.clear();
        string1.clear();
    }
    
    OrientedDateRoi oriented = prepareOrientedDateRoi(*image, pose, 20);
    if (!oriented.valid) {
        QMessageBox::warning(this, "警告", "识别区域超出原图范围！");
        return;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();

    ui->imagenum->setText(QString::number(totalImages));

    QString targetString = ui->dateEdit->toPlainText();
    g_lastStampPoly.clear();

    StampDetectionPipeline::OverlapDetectionFunction detectOverlap;
    if (!QFile::exists(currentTemplateDirPath + "/calibrate_config.yaml")) {
        qDebug() << "[ERROR] Missing overlap config!";
    } else {
        detectOverlap = [this](
                const cv::Mat &sourceImage,
                const std::vector<cv::Point> &datePoly) {
            const DetectResult overlap =
                    overlapDetector.processImage(sourceImage, datePoly);
            StampOverlapResult result;
            result.isOk = overlap.isOk;
            result.finalStampPoly = overlap.finalStampPoly;
            return result;
        };
    }

    const bool hasOverlapDetection = static_cast<bool>(detectOverlap);
    const StampDetectionPipeline stampPipeline;
    const StampDetectionResult stampResult =
            stampPipeline.detect(
                croppedImage,
                *image,
                pose.datePoly,
                targetString,
                [this](cv::Mat &dateRoi) {
        emit imgshibie(&dateRoi);
        return templatematch->run3(digitTemplates);
    },
                detectOverlap);

    const bool charIsOk = stampResult.characterIsOk;
    const bool overlapIsOk = stampResult.overlapIsOk;
    g_lastStampPoly = stampResult.finalStampPoly;
    if (hasOverlapDetection) {
        g_lastStampIsOverlap = !overlapIsOk;
    }

    // ===================== 3. UI 数据更新与画面重绘 =====================
    g_lastDrawResults = mapMatchResultsToOriginal(templatematch->lastMatchResults, oriented, image->size());
    g_lastPose = pose;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    slot_displayAndDetect(image);

    // ===================== 4. 综合判定与 PLC 剔除输出 =====================
    if (j % x == 0) {
        QString diagnostic;
        if (!charIsOk && overlapIsOk) {
            diagnostic = QStringLiteral("喷码不合格");
        } else if (charIsOk && !overlapIsOk) {
            diagnostic = QStringLiteral("钢印重叠");
        } else if (!stampResult.isOk) {
            diagnostic = QStringLiteral("喷码与钢印均不合格");
        } else {
            diagnostic = QStringLiteral("喷码与钢印均合格");
        }
        const double completionElapsedMs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                    .count());
        const DetectionCompletion completion = makeDetectionCompletion(
                    *image,
                    stampResult.isOk ? AlgorithmVerdict::Ok : AlgorithmVerdict::Ng,
                    QString(),
                    diagnostic,
                    completionElapsedMs);
        if (!stampResult.isOk) {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ng", completion);
            }
            ngImages++;
            totalImages++;

            if (!charIsOk && overlapIsOk) ui->resultlabel->setText(QString("<font size='10' color='red'>错误(喷码不合格)</font>"));
            else if (charIsOk && !overlapIsOk) ui->resultlabel->setText(QString("<font size='10' color='red'>错误(钢印重叠)</font>"));
            else ui->resultlabel->setText(QString("<font size='10' color='red'>错误(喷码与钢印均不合格)</font>"));

            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ok", completion);
            }
            ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font>"));
            rightremove();
        }
    }

    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}



void Widget::runWordTemplateDetection(cv::Mat *image,
                                      const DetectionPose &pose,
                                      const std::vector<cv::Mat> &templates,
                                      const std::vector<int> &templateTargetIndexes,
                                      const QString &targetString,
                                      const QString &imageThresholdText,
                                      const QString &templateName,
                                      const TemplateMatchPreparedTemplates *preparedTemplates,
                                      const OrientedDateRoi *preparedDateRoi)
{
    // 普通字库模式不展示“识别内容”。二维码+三期会在外层检测完成后
    // 写入二维码和日期状态，因此这里不能统一清空。
    if (currentDetectModeId() != BarcodeWordDetectionMode) {
        setLabelTextIfChanged(ui->resultlabel_7, QString());
    }

    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) {
        qDebug().noquote() << "[WORD_DETECT] skip: input image is empty";
        return;
    }

    if (judge) { j = 1; x++; judge = false; }
    if ((j - 1) % x == 0) {
        imageLabel->clearGreenRects();
        detectedRects.clear();
        string1.clear();
    }

    OrientedDateRoi generatedOriented;
    if (!preparedDateRoi) {
        generatedOriented =
                prepareOrientedDateRoi(
                    *image,
                    pose,
                    20);
        preparedDateRoi = &generatedOriented;
    }
    const OrientedDateRoi &oriented =
            *preparedDateRoi;
    if (!oriented.valid) {
        qDebug().noquote() << QString("[WORD_DETECT] template=%1 result=NG reason=日期ROI无效或超出原图范围 poseValid=%2 poseScore=%3")
                              .arg(templateName.isEmpty() ? QString("--") : templateName)
                              .arg(pose.valid ? QString("true") : QString("false"))
                              .arg(pose.score, 0, 'f', 4);
        return;
    }

    cv::Mat croppedImage =
            oriented.croppedImage;
    ui->imagenum->setText(QString::number(totalImages));

    int thresholdValue = 0;
    bool thresholdOk = parseIntValue(imageThresholdText, &thresholdValue);
    if (!thresholdOk) {
        thresholdOk = parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue);
    }

    const WordDetectionPipeline wordPipeline;
    const WordDetectionResult wordResult =
            wordPipeline.detect(
                croppedImage,
                targetString,
                [this,
                 thresholdOk,
                 thresholdValue,
                 preparedTemplates,
                 &templates,
                 &templateTargetIndexes](cv::Mat &dateRoi) {
        if (thresholdOk) {
            templatematch->ssimvalue(thresholdValue);
        }
        if (preparedTemplates
                && preparedTemplates->isValid()) {
            return templatematch->run3(
                        dateRoi,
                        *preparedTemplates,
                        templateTargetIndexes);
        }

        emit imgshibie(&dateRoi);
        return templatematch->run3(
                    templates,
                    templateTargetIndexes);
    });
    const int targetNum = wordResult.targetCharacterCount;
    const int detectNum = wordResult.detectedCharacterCount;
    const QString judgeResult = wordResult.isOk ? "ok" : "no";

    const QStringList targetUnits = wordResult.targetUnits;
    QStringList detectedUnits;
    QStringList matchDetails;
    detectedUnits.reserve(static_cast<int>(templatematch->lastMatchResults.size()));
    matchDetails.reserve(static_cast<int>(templatematch->lastMatchResults.size()));
    for (int i = 0; i < static_cast<int>(templatematch->lastMatchResults.size()); ++i) {
        const auto &match = templatematch->lastMatchResults[static_cast<size_t>(i)];
        const cv::Rect rect = std::get<0>(match);
        const double score = std::get<1>(match);
        const int targetIndex = static_cast<int>(std::get<2>(match));
        const QString unit = targetUnits.value(targetIndex, QString("#%1").arg(targetIndex));
        detectedUnits.append(unit);
        matchDetails.append(QString("%1:%2 score=%3 rect=(%4,%5,%6,%7) targetIndex=%8")
                            .arg(i + 1)
                            .arg(unit)
                            .arg(score, 0, 'f', 3)
                            .arg(rect.x)
                            .arg(rect.y)
                            .arg(rect.width)
                            .arg(rect.height)
                            .arg(targetIndex));
    }

    QStringList missingUnits;
    for (int i = 0; i < targetUnits.size(); ++i) {
        bool found = false;
        for (const auto &match : templatematch->lastMatchResults) {
            if (static_cast<int>(std::get<2>(match)) == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            missingUnits.append(QString("%1:%2").arg(i + 1).arg(targetUnits.at(i)));
        }
    }

    QString reason;
    if (judgeResult == "ok") {
        reason = "识别数量等于目标数量";
    } else if (detectNum < targetNum) {
        reason = QString("识别数量少于目标数量，少%1个").arg(targetNum - detectNum);
    } else {
        reason = QString("识别数量多于目标数量，多%1个").arg(detectNum - targetNum);
    }
    if (!missingUnits.isEmpty()) {
        reason += QString("；未匹配目标=%1").arg(missingUnits.join(", "));
    }

    qDebug().noquote() << QString("[WORD_DETECT] template=%1 result=%2 reason=%3 targetText=\"%4\" targetCount=%5 detectedCount=%6 threshold=%7 poseScore=%8 roi=(%9,%10,%11,%12) detected=\"%13\"")
                          .arg(templateName.isEmpty() ? QString("--") : templateName)
                          .arg(judgeResult == "ok" ? QString("OK") : QString("NG"))
                          .arg(reason)
                          .arg(targetString)
                          .arg(targetNum)
                          .arg(detectNum)
                          .arg(thresholdOk ? QString::number(thresholdValue) : QString("无效"))
                          .arg(pose.score, 0, 'f', 4)
                          .arg(oriented.roi.x)
                          .arg(oriented.roi.y)
                          .arg(oriented.roi.width)
                          .arg(oriented.roi.height)
                          .arg(detectedUnits.join(""));
    qDebug().noquote() << QString("[WORD_DETECT_DETAIL] %1")
                          .arg(matchDetails.isEmpty() ? QString("no matched boxes") : matchDetails.join(" | "));

    g_lastDrawResults = mapMatchResultsToOriginal(templatematch->lastMatchResults, oriented, image->size());
    g_lastPose = pose;
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    slot_displayAndDetect(image);

    if (j % x == 0) {
        const double completionElapsedMs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                    .count());
        const DetectionCompletion completion = makeDetectionCompletion(
                    *image,
                    wordResult.isOk ? AlgorithmVerdict::Ok : AlgorithmVerdict::Ng,
                    detectedUnits.join(QStringLiteral("")),
                    reason,
                    completionElapsedMs);
        if (!wordResult.isOk) {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveWordResultImages("png", "ng", completion);
            }
            ngImages++;
            totalImages++;
            ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveWordResultImages("png", "ok", completion);
            }
            ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font>"));
            rightremove();
        }
    }

    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}

void Widget::runBarcodeWordDetection(
    cv::Mat *image,
    const DetectionPose &pose,
    const WordTemplateProfile &profile)
{
    QElapsedTimer totalTimer;
    totalTimer.start();

    const QString templateName = profile.name.isEmpty()
            ? QDir(profile.dirPath).dirName()
            : profile.name;
    BarcodeReadResult barcode;

    if (!image || image->empty()) {
        qDebug() << "[BARCODE_WORD] input image is empty; stop.";
        return;
    }
    if (!pose.valid) {
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "未执行",
                    "未找到定位锚点",
                    elapsedMilliseconds(totalTimer));
        return;
    }

    qDebug().noquote()
            << QString("[BARCODE_WORD_POSE] profileIndex=%1 template=%2 score=%3 anchor=(%4,%5) angle=%6 trackingPoly=%7 barcodePoly=%8 datePoly=%9")
               .arg(pose.wordTemplateProfileIndex)
               .arg(templateName)
               .arg(pose.score, 0, 'f', 4)
               .arg(pose.anchorCenter.x, 0, 'f', 2)
               .arg(pose.anchorCenter.y, 0, 'f', 2)
               .arg(pose.angleDeg, 0, 'f', 2)
               .arg(formatPolygonPoints(pose.trackingPoly))
               .arg(formatPolygonPoints(pose.barcodePoly))
               .arg(formatPolygonPoints(pose.datePoly));

    if (pose.barcodePoly.size() != 4) {
        barcode.status = BarcodeReadStatus::InvalidRoi;
        barcode.errorReason = "Barcode polygon is missing or invalid";
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "未执行",
                    "二维码区域配置无效或未映射",
                    elapsedMilliseconds(totalTimer));
        return;
    }
    if (pose.datePoly.size() < 3) {
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "未执行",
                    "日期检测区域配置无效或未映射",
                    elapsedMilliseconds(totalTimer));
        return;
    }

    const BarcodeWordOrientedRois orientedRois =
            prepareBarcodeWordOrientedRois(
                *image,
                pose,
                profile.settings.barcodeOptions.roiPaddingPercent,
                20);
    const OrientedBarcodeRoi &barcodeRoi =
            orientedRois.barcode;
    if (!barcodeRoi.valid) {
        barcode.status = BarcodeReadStatus::InvalidRoi;
        barcode.errorReason = "Invalid barcode ROI";
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "未执行",
                    "二维码区域无效或超出图像范围",
                    elapsedMilliseconds(totalTimer));
        return;
    }

    if (!m_barcodeDecoder->ensureLoaded()) {
        barcode.status = BarcodeReadStatus::DecoderUnavailable;
        barcode.errorReason = m_barcodeDecoder->lastError();
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "读码器不可用",
                    "BarcodeDecoder.dll不可用",
                    elapsedMilliseconds(totalTimer));
        return;
    }

    int successfulBarcodeStrategyId = -1;
    unsigned int successfulBarcodeOptionFlags =
            BARCODE_DECODER_OPTION_NONE;
    barcode = m_barcodeDecoder->decode(
                barcodeRoi.grayRoi,
                profile.settings.barcodeOptions,
                profile.preferredBarcodeStrategyId,
                profile.preferredBarcodeOptionFlags,
                &successfulBarcodeStrategyId,
                &successfulBarcodeOptionFlags);
    if (barcode.readable) {
        profile.preferredBarcodeStrategyId =
                successfulBarcodeStrategyId;
        profile.preferredBarcodeOptionFlags =
                successfulBarcodeOptionFlags;
        profile.consecutiveBarcodeFailures = 0;
    } else {
        profile.consecutiveBarcodeFailures =
                std::min(
                    3,
                    profile.consecutiveBarcodeFailures + 1);
        if (profile.consecutiveBarcodeFailures >= 3) {
            profile.preferredBarcodeStrategyId = -1;
            profile.preferredBarcodeOptionFlags =
                    BARCODE_DECODER_OPTION_NONE;
        }
    }

    barcode.cornersInOriginal.clear();
    barcode.cornersInOriginal.reserve(barcode.cornersInRoi.size());
    for (const cv::Point2f &corner : barcode.cornersInRoi) {
        const cv::Point2f cornerInRotatedImage(
                    corner.x + barcodeRoi.roi.x,
                    corner.y + barcodeRoi.roi.y);
        barcode.cornersInOriginal.push_back(
                    transformPoint(
                        barcodeRoi.inverseRotationMatrix,
                        cornerInRotatedImage));
    }

    const OrientedDateRoi &dateRoi =
            orientedRois.date;
    BarcodeWordDetectionPipeline::DateDetectionFunction detectDate;
    if (dateRoi.valid) {
        detectDate = [this,
                      image,
                      &pose,
                      &profile,
                      &templateName,
                      &barcode,
                      &dateRoi]() {
            qDebug().noquote()
                    << QString("[BARCODE_WORD] template=%1 barcode=OK barcodeMs=%2 text=\"%3\" date=START")
                       .arg(templateName)
                       .arg(barcode.elapsedMs, 0, 'f', 3)
                       .arg(barcode.text);

            const int totalBeforeDate = totalImages;
            const int ngBeforeDate = ngImages;
            runWordTemplateDetection(
                        image,
                        pose,
                        profile.digitTemplates,
                        profile.digitTemplateTargetIndexes,
                        profile.settings.targetText,
                        QString::number(profile.settings.imageThreshold),
                        templateName,
                        &profile.preparedDigitTemplates,
                        &dateRoi);

            BarcodeWordDateDetectionResult dateResult;
            dateResult.resultProduced = totalImages > totalBeforeDate;
            dateResult.isOk = dateResult.resultProduced
                    && ngImages == ngBeforeDate;
            return dateResult;
        };
    }

    const BarcodeWordDetectionPipeline barcodeWordPipeline;
    const BarcodeWordDetectionResult barcodeWordResult =
            barcodeWordPipeline.detect(
                barcode.readable,
                detectDate);

    if (!barcodeWordResult.barcodeIsReadable) {
        QString reason;
        if (barcode.status == BarcodeReadStatus::Timeout) {
            reason = "二维码读取超时";
        } else if (barcode.status == BarcodeReadStatus::InvalidRoi) {
            reason = "二维码区域无效";
        } else if (barcode.status == BarcodeReadStatus::InternalError) {
            reason = "二维码解码器内部错误";
        } else {
            reason = "二维码不可读或区域内没有二维码";
        }

        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "不可读",
                    reason,
                    elapsedMilliseconds(totalTimer));
        return;
    }

    // 日期区域无效也必须形成一次最终NG，不能进入原字库函数后无结果返回。
    if (!barcodeWordResult.dateDetectionExecuted) {
        finalizeBarcodeWordNg(
                    image,
                    pose,
                    barcode,
                    "可读",
                    "日期检测区域无效或超出图像范围",
                    elapsedMilliseconds(totalTimer));
        return;
    }

    QString dateState = "已执行";
    QString finalState = "已输出";
    if (barcodeWordResult.dateResultProduced) {
        dateState = barcodeWordResult.dateIsOk ? "正确" : "错误";
        finalState = barcodeWordResult.isOk ? "OK" : "NG";
    }

    QStringList resultLines;
    resultLines.append("二维码：可读");
    resultLines.append(QString("二维码内容：%1").arg(barcode.text));
    resultLines.append(QString("日期：%1").arg(dateState));
    setLabelTextIfChanged(
                ui->resultlabel_7,
                resultLines.join("\n"));

    const double postTrackingElapsedMs = elapsedMilliseconds(totalTimer);
    const double totalElapsedMs =
            pose.trackingElapsedMs + postTrackingElapsedMs;
    ui->speedLabel->setText(
                QString("检测耗时 %1 ms")
                .arg(totalElapsedMs, 0, 'f', 2));

    qDebug().noquote()
            << QString("[BARCODE_WORD] template=%1 barcode=OK date=%2 final=%3 trackingMs=%4 barcodeMs=%5 postTrackingMs=%6 totalMs=%7")
               .arg(templateName)
               .arg(dateState)
               .arg(finalState)
               .arg(pose.trackingElapsedMs, 0, 'f', 3)
               .arg(barcode.elapsedMs, 0, 'f', 3)
               .arg(postTrackingElapsedMs, 0, 'f', 3)
               .arg(totalElapsedMs, 0, 'f', 3);
}


void Widget::clearBarcodeTemplateValidation()
{
    m_barcodeTemplateReadable = false;
    m_validatedBarcodeRect = QRect();
    m_validatedBarcodeText.clear();
}

QString Widget::barcodeTemplateValidationFailureText(
    const BarcodeReadResult &barcode) const
{
    switch (barcode.status) {
    case BarcodeReadStatus::DecoderUnavailable:
        return barcode.errorReason.trimmed().isEmpty()
                ? "BarcodeDecoder.dll 不可用，无法验证二维码。"
                : QString("BarcodeDecoder.dll 不可用：%1")
                  .arg(barcode.errorReason);
    case BarcodeReadStatus::InvalidRoi:
        return "二维码框选区域无效，请重新框选。";
    case BarcodeReadStatus::Timeout:
        return "二维码扫描超时，请重新框选完整、清晰的二维码区域。";
    case BarcodeReadStatus::InternalError:
        return barcode.errorReason.trimmed().isEmpty()
                ? "二维码解码器发生内部错误。"
                : QString("二维码解码器发生内部错误：%1")
                  .arg(barcode.errorReason);
    case BarcodeReadStatus::NotFound:
        return "当前框选区域内没有扫描到可读的 Data Matrix 二维码。";
    case BarcodeReadStatus::Success:
        break;
    }

    return "当前框选区域内没有扫描到可读的 Data Matrix 二维码。";
}

bool Widget::validateBarcodeTemplateRect(
    const QRect &uiBarcodeRect,
    const BarcodeDecodeOptions &options,
    BarcodeReadResult *barcode,
    QString *failureReason)
{
    BarcodeReadResult result;

    auto finishFailure = [&](BarcodeReadStatus status,
                             const QString &reason) -> bool {
        result.status = status;
        result.readable = false;
        result.errorReason = reason;
        if (barcode) {
            *barcode = result;
        }
        if (failureReason) {
            *failureReason = barcodeTemplateValidationFailureText(result);
        }
        return false;
    };

    if (!myImage || myImage->empty() || !imageLabel) {
        return finishFailure(
                    BarcodeReadStatus::InvalidRoi,
                    "Template source image is unavailable");
    }

    const QRect normalizedRect = uiBarcodeRect.normalized();
    if (normalizedRect.width() <= 5 || normalizedRect.height() <= 5) {
        return finishFailure(
                    BarcodeReadStatus::InvalidRoi,
                    "Barcode rectangle is too small");
    }

    const QSize labelSize = imageLabel->size();
    const QSize imageSize(myImage->cols, myImage->rows);
    const QPixmap *displayedPixmap = imageLabel->pixmap();
    const QSize displayedSize =
            displayedPixmap && !displayedPixmap->isNull()
            ? displayedPixmap->size()
            : imageSize.scaled(labelSize, Qt::KeepAspectRatio);
    if (displayedSize.width() <= 0 || displayedSize.height() <= 0) {
        return finishFailure(
                    BarcodeReadStatus::InvalidRoi,
                    "Displayed image size is invalid");
    }

    const int xOffset = (labelSize.width() - displayedSize.width()) / 2;
    const int yOffset = (labelSize.height() - displayedSize.height()) / 2;
    const double scaleX =
            static_cast<double>(imageSize.width()) / displayedSize.width();
    const double scaleY =
            static_cast<double>(imageSize.height()) / displayedSize.height();

    const int sourceLeft = static_cast<int>(std::floor(
        (normalizedRect.left() - xOffset) * scaleX));
    const int sourceTop = static_cast<int>(std::floor(
        (normalizedRect.top() - yOffset) * scaleY));
    const int sourceRight = static_cast<int>(std::ceil(
        (normalizedRect.right() + 1 - xOffset) * scaleX));
    const int sourceBottom = static_cast<int>(std::ceil(
        (normalizedRect.bottom() + 1 - yOffset) * scaleY));

    cv::Rect sourceRect(
                sourceLeft,
                sourceTop,
                sourceRight - sourceLeft,
                sourceBottom - sourceTop);
    sourceRect &= cv::Rect(0, 0, myImage->cols, myImage->rows);
    if (sourceRect.width <= 5 || sourceRect.height <= 5) {
        return finishFailure(
                    BarcodeReadStatus::InvalidRoi,
                    "Barcode rectangle is outside the source image");
    }

    DetectionPose templatePose;
    templatePose.valid = true;
    templatePose.angleDeg = 0.0f;
    templatePose.anchorCenter = cv::Point2f(
                sourceRect.x + (sourceRect.width - 1) * 0.5f,
                sourceRect.y + (sourceRect.height - 1) * 0.5f);
    templatePose.barcodePoly = {
        cv::Point(sourceRect.x, sourceRect.y),
        cv::Point(sourceRect.x + sourceRect.width - 1, sourceRect.y),
        cv::Point(sourceRect.x + sourceRect.width - 1,
                  sourceRect.y + sourceRect.height - 1),
        cv::Point(sourceRect.x,
                  sourceRect.y + sourceRect.height - 1)
    };

    const BarcodeWordOrientedRois prepared =
            prepareBarcodeWordOrientedRois(
                *myImage,
                templatePose,
                options.roiPaddingPercent,
                0);
    const OrientedBarcodeRoi &barcodeRoi = prepared.barcode;
    if (!barcodeRoi.valid) {
        return finishFailure(
                    BarcodeReadStatus::InvalidRoi,
                    "Failed to prepare barcode ROI");
    }

    result = m_barcodeDecoder->decode(
                barcodeRoi.grayRoi,
                options);
    result.cornersInOriginal.clear();
    result.cornersInOriginal.reserve(result.cornersInRoi.size());
    for (const cv::Point2f &corner : result.cornersInRoi) {
        const cv::Point2f cornerInRotatedImage(
                    corner.x + barcodeRoi.roi.x,
                    corner.y + barcodeRoi.roi.y);
        result.cornersInOriginal.push_back(
                    transformPoint(
                        barcodeRoi.inverseRotationMatrix,
                        cornerInRotatedImage));
    }

    qDebug() << "[BARCODE_TEMPLATE]"
             << "uiRect=" << normalizedRect
             << "labelSize=" << labelSize
             << "pixmapSize=" << displayedSize
             << "sourceRect="
             << sourceRect.x << sourceRect.y
             << sourceRect.width << sourceRect.height
             << "decodeRoi="
             << barcodeRoi.roi.x << barcodeRoi.roi.y
             << barcodeRoi.roi.width << barcodeRoi.roi.height
             << "paddingPercent=" << options.roiPaddingPercent
             << "maxDecodeTimeMs=" << options.maxDecodeTimeMs
             << "fallback=" << options.enableFallback
             << "readable=" << result.readable
             << "elapsedMs=" << result.elapsedMs
             << "text=" << result.text
             << "reason=" << result.errorReason;

    const bool readable =
            result.status == BarcodeReadStatus::Success
            && result.readable
            && (!result.rawBytes.isEmpty() || !result.text.isEmpty());
    if (!readable) {
        result.readable = false;
        if (barcode) {
            *barcode = result;
        }
        if (failureReason) {
            *failureReason = barcodeTemplateValidationFailureText(result);
        }
        return false;
    }

    if (barcode) {
        *barcode = result;
    }
    if (failureReason) {
        failureReason->clear();
    }
    return true;
}

BarcodeDecodeOptions Widget::barcodeTemplateValidationOptions() const
{
    const int profileIndex = currentWordTemplateProfileIndex();
    if (currentDetectModeId() == BarcodeWordDetectionMode
            && profileIndex >= 0
            && profileIndex
               < static_cast<int>(m_wordTemplateProfiles.size())) {
        return m_wordTemplateProfiles[
                    static_cast<size_t>(profileIndex)]
                .settings.barcodeOptions;
    }

    return AppSettingsManager::defaultTemplatePrivateSettings()
            .barcodeOptions;
}

void Widget::finalizeWordTrackingNg(
    cv::Mat *image,
    const DetectionPose &pose,
    const QString &reason)
{
    if (!image || image->empty()) {
        qDebug() << "[WORD_DETECT] Cannot finalize tracking NG:"
                 << "input image is empty.";
        return;
    }

    QElapsedTimer finalizationTimer;
    finalizationTimer.start();

    if (!removalQueue.empty()
            && totalImages
               >= removalQueue.front().second - 1) {
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    if (judge) {
        j = 1;
        x++;
        judge = false;
    }
    imageLabel->clearGreenRects();
    detectedRects.clear();
    string1.clear();

    g_lastDrawResults.clear();
    g_lastPose = pose;
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime =
            QDateTime::currentMSecsSinceEpoch();
    slot_displayAndDetect(image);

    // 普通字库模式不需要展示识别内容；避免上一帧失败文案残留到后续OK结果。
    setLabelTextIfChanged(ui->resultlabel_7, QString());

    const DetectionCompletion completion = makeDetectionCompletion(
                *image,
                AlgorithmVerdict::Ng,
                QString(),
                reason,
                pose.trackingElapsedMs
                + elapsedMilliseconds(finalizationTimer));

    if (ui->comboBox->currentIndex() == 1
            || ui->comboBox->currentIndex() == 3) {
        saveWordResultImages(
                    "png",
                    "ng",
                    completion);
    }

    ngImages++;
    totalImages++;
    ui->resultlabel->setText(
                QString("<font size='10' color='red'>"
                        "错误！</font>"));

    if (wrongindex == 0) {
        wrongremove();
    } else {
        removalQueue.push(
                    std::make_pair(
                        totalImages,
                        totalImages + wrongindex));
    }

    const double passRate =
            totalImages > 0
            ? (1.0
               - static_cast<double>(ngImages)
                 / totalImages) * 100.0
            : 0.0;
    ui->lineBoxIndex_6->setText(
                QString::number(passRate, 'f', 1));
    ui->ngnum->setText(
                QString::number(ngImages));
    ui->imagenum->setText(
                QString::number(totalImages));

    const double totalElapsedMs =
            pose.trackingElapsedMs
            + elapsedMilliseconds(finalizationTimer);
    ui->speedLabel->setText(
                QString("检测耗时 %1 ms")
                .arg(totalElapsedMs, 0, 'f', 2));

    qDebug().noquote()
            << QString("[WORD_DETECT] result=NG "
                       "trackingMs=%1 reason=%2")
               .arg(pose.trackingElapsedMs, 0, 'f', 3)
               .arg(reason);
    j++;
}

void Widget::finalizeBarcodeWordNg(
    cv::Mat *image,
    const DetectionPose &pose,
    const BarcodeReadResult &barcode,
    const QString &barcodeState,
    const QString &reason,
    double postTrackingElapsedMs)
{
    if (!image || image->empty()) {
        qDebug() << "[BARCODE_WORD] Cannot finalize NG: input image is empty.";
        return;
    }
    QElapsedTimer finalizationTimer;
    finalizationTimer.start();

    if (!removalQueue.empty()
            && totalImages >= removalQueue.front().second - 1) {
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    if (judge) {
        j = 1;
        x++;
        judge = false;
    }
    if ((j - 1) % x == 0) {
        imageLabel->clearGreenRects();
        detectedRects.clear();
        string1.clear();
    }

    g_lastDrawResults.clear();
    g_lastPose = pose;
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();
    slot_displayAndDetect(image);

    QStringList resultLines;
    resultLines.append(QString("二维码：%1").arg(barcodeState));
    if (!barcode.text.isEmpty()) {
        resultLines.append(QString("二维码内容：%1").arg(barcode.text));
    }
    resultLines.append("日期：未执行");
    resultLines.append(QString("原因：%1").arg(reason));
    setLabelTextIfChanged(
                ui->resultlabel_7,
                resultLines.join("\n"));

    if (j % x == 0) {
        const DetectionCompletion completion = makeDetectionCompletion(
                    *image,
                    AlgorithmVerdict::Ng,
                    barcode.text,
                    reason,
                    pose.trackingElapsedMs
                    + postTrackingElapsedMs
                    + elapsedMilliseconds(finalizationTimer));
        if (ui->comboBox->currentIndex() == 1
                || ui->comboBox->currentIndex() == 3) {
            saveWordResultImages("png", "ng", completion);
        }

        ngImages++;
        totalImages++;
        ui->resultlabel->setText(
                    QString("<font size='10' color='red'>错误！</font>"));

        if (wrongindex == 0) {
            wrongremove();
        } else {
            removalQueue.push(
                        std::make_pair(
                            totalImages,
                            totalImages + wrongindex));
        }
    }

    const double passRate = totalImages > 0
            ? (1.0 - static_cast<double>(ngImages) / totalImages) * 100.0
            : 0.0;
    ui->lineBoxIndex_6->setText(QString::number(passRate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));
    const double finalizationElapsedMs =
            elapsedMilliseconds(finalizationTimer);
    const double totalElapsedMs =
            pose.trackingElapsedMs
            + postTrackingElapsedMs
            + finalizationElapsedMs;
    ui->speedLabel->setText(
                QString("检测耗时 %1 ms")
                .arg(totalElapsedMs, 0, 'f', 2));

    qDebug().noquote()
            << QString("[BARCODE_WORD] barcode=%1 date=NOT_EXECUTED final=NG trackingMs=%2 barcodeMs=%3 postTrackingMs=%4 finalizeMs=%5 totalMs=%6 reason=%7 decoderReason=%8")
               .arg(barcodeState)
               .arg(pose.trackingElapsedMs, 0, 'f', 3)
               .arg(barcode.elapsedMs, 0, 'f', 3)
               .arg(postTrackingElapsedMs, 0, 'f', 3)
               .arg(finalizationElapsedMs, 0, 'f', 3)
               .arg(totalElapsedMs, 0, 'f', 3)
               .arg(reason)
               .arg(barcode.errorReason);

    j++;
}

/**
 * @brief 纸巾卷检测结果处理槽函数
 * @param image 输入整张相机图像
 * @param tissueResult 工作线程计算出的纸巾检测结果
 */
void Widget::slot_handleTissueResult(cv::Mat *image, TissueRollResult tissueResult)
{
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        qDebug() << "[TISSUE_DETECT] Delayed wrongremove triggered, wrongindex:" << wrongindex;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) {
        qDebug() << "[TISSUE_DETECT] Invalid input image.";
        return;
    }

    if (judge) {
        j = 1;
        x++;
        judge = false;
    }
    if ((j - 1) % x == 0) {
        detectedRects.clear();
        string1.clear();
    }

    const bool isOk = tissueResult.isOk;

    g_lastDrawResults.clear();
    g_lastPose = DetectionPose();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    if (tissueResult.rollFound) {
        g_lastTissueRoll = tissueResult.roll;
        g_hasLastTissueRoll = true;
    } else {
        g_lastTissueRoll = TissueRollItem();
        g_hasLastTissueRoll = false;
    }
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    ui->resultlabel->setText(isOk ? "OK" : "NG");
    ui->resultlabel->setStyleSheet(isOk
        ? "background-color: #eef1f6; border-radius: 6px; font-size: 36px; font-weight: 900; color: #20b455;"
        : "background-color: #eef1f6; border-radius: 6px; font-size: 36px; font-weight: 900; color: #ff4d4f;");
    ui->resultlabel->setWordWrap(true);
    const QString tissueRecognitionText =
            tissueResult.rollFound
            ? QString("粗糙度：%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QString("粗糙度：--");
    setLabelTextIfChanged(
                ui->resultlabel_7,
                tissueRecognitionText);

    g_allowTissueDetectionFrameDisplay = true;
    slot_displayAndDetect(image);
    g_allowTissueDetectionFrameDisplay = false;

    qDebug() << "[TISSUE_DETECT]" << QString::fromStdString(tissueResult.message);
    qDebug() << "[TISSUE_DETECT_DEBUG]"
             << "image" << tissueResult.imageWidth << "x" << tissueResult.imageHeight
             << "processingTimeMs" << tissueResult.processingTimeMs
             << "rollFound" << tissueResult.rollFound
             << "overall" << (tissueResult.isOk ? "OK" : "NG");
    if (tissueResult.rollFound) {
        const TissueRollItem& roll = tissueResult.roll;
        qDebug() << "[TISSUE_DETECT_DEBUG]"
                 << (roll.isOk ? "OK" : "NG")
                 << "reason" << QString::fromStdString(roll.rejectReason)
                 << "rough" << roll.roughnessScore
                 << "roughNg" << roll.roughnessNg
                 << "ringPixels" << roll.ringPixelCount
                 << "outerCenter" << roll.center.x << roll.center.y
                 << "outerRadius" << roll.outerAxes.width
                 << "outerBbox" << roll.outerBbox.x << roll.outerBbox.y
                 << roll.outerBbox.width << roll.outerBbox.height
                 << "innerFound" << roll.innerHoleFound
                 << "innerCenter" << roll.innerCenter.x << roll.innerCenter.y
                 << "innerRadius" << roll.innerAxes.width;
    }

    if (j % x == 0) {
        const DetectionCompletion completion = makeDetectionCompletion(
                    *image,
                    isOk ? AlgorithmVerdict::Ok : AlgorithmVerdict::Ng,
                    tissueRecognitionText,
                    QString::fromStdString(tissueResult.message),
                    tissueResult.processingTimeMs > 0
                    ? static_cast<double>(tissueResult.processingTimeMs)
                    : static_cast<double>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::high_resolution_clock::now() - start)
                        .count()));
        if (!isOk) {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ng", completion);
            }
            ngImages++;
            totalImages++;

            if (wrongindex == 0) {
                wrongremove();
            } else {
                removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
            }
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ok", completion);
            }
            rightremove();
        }
    }

    const double hegerate = (totalImages > 0)
        ? (1 - static_cast<double>(ngImages) / totalImages) * 100
        : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (tissueResult.processingTimeMs > 0) {
        duration = tissueResult.processingTimeMs;
    }
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}

bool Widget::hasRunningInspectionThread() const
{
    const bool softwareInspectionRunning =
            myThread
            && myThread->isRunning()
            && m_templateCaptureState
               != TemplateCaptureState::Previewing;
    const bool hardwareInspectionRunning =
            cameraThread
            && cameraThread->isRunning();
    return softwareInspectionRunning
            || hardwareInspectionRunning;
}

void Widget::clearInspectionTransientDisplay()
{
    if (!ui) {
        return;
    }
    if (ui->resultlabel) {
        ui->resultlabel->clear();
    }
    if (ui->resultlabel_7) {
        ui->resultlabel_7->clear();
    }
    if (ui->speedLabel) {
        ui->speedLabel->clear();
    }
    allResults.clear();
}

bool Widget::shouldSuppressStreamingFrame() const
{
    return m_resultBoundDisplayActive
            && isWordFamilyMode(currentDetectModeId());
}

void Widget::handleStreamingFrame(const cv::Mat &image)
{
    if (image.empty() || shouldSuppressStreamingFrame()) {
        return;
    }

    // cv::Mat在这里仅做浅拷贝，slot_displayAndDetect内部会创建独立显示图，
    // 不修改相机线程传入的源图。
    cv::Mat displayFrame = image;
    slot_displayAndDetect(&displayFrame);
}

void Widget::updateOperationUiState()
{
    if (!ui) {
        return;
    }

    QList<QAbstractButton *> operationButtons =
            findChildren<QAbstractButton *>();
    for (QAbstractButton *button : operationButtons) {
        if (!button) {
            continue;
        }
        if (m_multiCameraWidget
                && m_multiCameraWidget->isAncestorOf(button)) {
            continue;
        }
        const QString operationDisabledStyleMarker =
                "/* operation-disabled-style */";
        if (!button->styleSheet().contains(
                    operationDisabledStyleMarker)) {
            button->setStyleSheet(
                        button->styleSheet()
                        + "\n/* operation-disabled-style */"
                          "QPushButton:disabled,"
                          "QToolButton:disabled,"
                          "QCheckBox:disabled {"
                          "background-color: #f2f3f5;"
                          "color: #a8abb2;"
                          "border-color: #dcdfe6;"
                          "}");
        }
        button->setEnabled(false);
    }

    const bool idleState =
            m_operationState == OperationState::CameraClosed
            || m_operationState == OperationState::CameraReady;
    if (idleState) {
        for (QAbstractButton *button : operationButtons) {
            if (!button) {
                continue;
            }
            if (m_multiCameraWidget
                    && m_multiCameraWidget->isAncestorOf(button)) {
                continue;
            }
            button->setEnabled(true);
        }
    }

    if (ui->plcbtn) {
        ui->plcbtn->setText(
                    m_operationState == OperationState::Detecting
                    ? "采集中..."
                    : (m_operationState == OperationState::Stopping
                       ? "停止中..."
                       : "启动识别"));
    }
    if (ui->cancel) {
        const bool templateOperation =
                m_operationState == OperationState::TemplatePreviewing
                || m_operationState == OperationState::TemplateFrozen;
        ui->cancel->setText(
                    m_operationState == OperationState::Stopping
                    ? "停止中..."
                    : (templateOperation
                       ? "退出模板制作"
                       : "停止识别"));
    }
    if (ui->VideoShoot) {
        if (m_operationState
                == OperationState::TemplatePreviewing) {
            ui->VideoShoot->setText(
                        "拍照并开始框选");
        } else if (m_operationState
                   == OperationState::TemplateFrozen) {
            ui->VideoShoot->setText("重新取景");
        } else {
            ui->VideoShoot->setText("制作模板");
        }
    }

    switch (m_operationState) {
    case OperationState::CameraClosed:
        if (ui->cancel) ui->cancel->setEnabled(false);
        if (ui->CloseCamera) ui->CloseCamera->setEnabled(false);
        if (ui->plcbtn) ui->plcbtn->setEnabled(false);
        if (ui->VideoShoot) ui->VideoShoot->setEnabled(false);
        if (ui->pushButton_5) ui->pushButton_5->setEnabled(false);
        break;
    case OperationState::CameraReady:
        if (ui->HandwareDetect) ui->HandwareDetect->setEnabled(false);
        if (ui->cancel) ui->cancel->setEnabled(false);
        if (ui->pushButton_5) ui->pushButton_5->setEnabled(false);
        break;
    case OperationState::Detecting:
        if (ui->cancel) ui->cancel->setEnabled(true);
        break;
    case OperationState::Stopping:
        if (ui->cancel) ui->cancel->setEnabled(true);
        break;
    case OperationState::TemplatePreviewing:
        if (ui->VideoShoot) ui->VideoShoot->setEnabled(true);
        if (ui->cancel) ui->cancel->setEnabled(true);
        if (ui->statusLabel) {
            ui->statusLabel->setText(
                        "模板制作中：实时取景");
        }
        break;
    case OperationState::TemplateFrozen:
        if (ui->VideoShoot) ui->VideoShoot->setEnabled(true);
        if (ui->pushButton_5) ui->pushButton_5->setEnabled(true);
        if (ui->cancel) ui->cancel->setEnabled(true);
        if (ui->statusLabel) {
            ui->statusLabel->setText(
                        "模板制作中：请完成框选并保存");
        }
        break;
    }

    const bool normalSettingsEnabled = idleState;
    for (auto it = m_globalSettingBindings.constBegin();
         it != m_globalSettingBindings.constEnd();
         ++it) {
        if (it.value().editor) {
            it.value().editor->setEnabled(
                        normalSettingsEnabled);
        }
    }
    if (ui->comboBox_4) {
        ui->comboBox_4->setEnabled(
                    normalSettingsEnabled);
    }
    if (m_wordTemplateEditComboBox) {
        m_wordTemplateEditComboBox->setEnabled(
                    normalSettingsEnabled);
    }
    if (m_publishTemplateGroupButton) {
        m_publishTemplateGroupButton->setEnabled(
                    normalSettingsEnabled);
    }
    if (m_publishedRecipeButton) {
        m_publishedRecipeButton->setEnabled(
                    normalSettingsEnabled);
    }
    if (m_manualCharacterCropButton) {
        m_manualCharacterCropButton->setEnabled(
                    normalSettingsEnabled);
    }
    if (ui->dateEdit) {
        ui->dateEdit->setEnabled(
                    normalSettingsEnabled);
    }
    if (ui->lineEdit_yuzhi) {
        ui->lineEdit_yuzhi->setEnabled(
                    normalSettingsEnabled);
    }

    if (m_templateCaptureAttentionTimer
            && ui->VideoShoot) {
        if (m_operationState
                == OperationState::TemplatePreviewing) {
            if (!m_templateCaptureAttentionTimer->isActive()) {
                m_templateCaptureAttentionOn = true;
                ui->VideoShoot->setProperty(
                            "templateCaptureActive",
                            true);
                ui->VideoShoot->setProperty(
                            "templateCaptureAttention",
                            true);
                ui->VideoShoot->style()->unpolish(
                            ui->VideoShoot);
                ui->VideoShoot->style()->polish(
                            ui->VideoShoot);
                ui->VideoShoot->update();
                m_templateCaptureAttentionTimer->start();
            }
        } else {
            m_templateCaptureAttentionTimer->stop();
            if (m_templateCaptureAttentionOn
                    || ui->VideoShoot->property(
                        "templateCaptureActive").toBool()
                    || ui->VideoShoot->property(
                        "templateCaptureAttention").toBool()) {
                m_templateCaptureAttentionOn = false;
                ui->VideoShoot->setProperty(
                            "templateCaptureActive",
                            false);
                ui->VideoShoot->setProperty(
                            "templateCaptureAttention",
                            false);
                ui->VideoShoot->style()->unpolish(
                            ui->VideoShoot);
                ui->VideoShoot->style()->polish(
                            ui->VideoShoot);
                ui->VideoShoot->update();
            }
        }
    }

    if (idleState) {
        updateHardwareParameterUiEnabled();
    }
}

void Widget::connectTemplatePreviewSignals(MyThread *thread)
{
    if (!thread) {
        return;
    }

    connect(thread,
            &MyThread::signal_templatePreviewImage,
            this,
            [this, thread](cv::Mat image, quint64 sessionId) {
        if (m_templateCaptureState
                != TemplateCaptureState::Previewing
                || sessionId != m_templatePreviewSessionId
                || image.empty()) {
            thread->acknowledgeTemplatePreviewFrame(
                        sessionId);
            return;
        }

        m_lastTemplatePreviewFrame = image.clone();
        slot_displayAndDetect(&m_lastTemplatePreviewFrame);
        updateImageDisplayStatusText(
                    "实时取景中，请调整产品位置，确认后点击【拍照并开始框选】。");
        thread->acknowledgeTemplatePreviewFrame(
                    sessionId);
    },
    Qt::QueuedConnection);

    connect(thread,
            &MyThread::signal_templatePreviewError,
            this,
            [this](const QString &reason, quint64 sessionId) {
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
                    ? "模板实时取景失败，相机已打开"
                    : "模板实时取景失败，相机已关闭");
        updateImageDisplayStatusText("实时取景失败，请检查相机后重试。");
        QMessageBox::warning(this, "实时取景失败", reason);
    },
    Qt::QueuedConnection);

    connect(thread,
            &QThread::finished,
            this,
            [this, thread]() {
        if (thread != myThread) {
            return;
        }

        if (m_templateCaptureState
                == TemplateCaptureState::Previewing) {
            ++m_templatePreviewSessionId;
            m_templateCaptureState =
                    TemplateCaptureState::Idle;
            m_lastTemplatePreviewFrame.release();
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            ui->statusLabel->setText(
                        m_bOpenDevice
                        ? "模板实时取景已停止，相机已打开"
                        : "模板实时取景已停止，相机已关闭");
            updateOperationUiState();
            return;
        }

        if (m_operationState
                == OperationState::Detecting) {
            isCollecting = false;
            clearWordTemplateRunSnapshot();
            m_barcodeWordRunActive = false;
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            ui->statusLabel->setText(
                        "识别线程已停止");
            updateOperationUiState();
        }
    },
    Qt::QueuedConnection);
}

bool Widget::hasTemplateDrawingSelection() const
{
    return imageLabel
            && (!imageLabel->getTrackingRect().isNull()
                || !imageLabel->getBarcodeRect().isNull()
                || !imageLabel->getDetectionPoly().isEmpty());
}

bool Widget::stopTemplatePreview(int waitTimeMs)
{
    if (m_templateCaptureState
            != TemplateCaptureState::Previewing) {
        return true;
    }

    // 先使当前会话失效，已进入事件队列的旧帧将被直接忽略。
    ++m_templatePreviewSessionId;
    if (!myThread) {
        return true;
    }

    myThread->setTemplatePreviewMode(
                false,
                m_templatePreviewSessionId);
    myThread->requestStop();
    myThread->stop();

    if (myThread->isRunning()
            && !myThread->wait(waitTimeMs)) {
        qDebug() << "[TEMPLATE_PREVIEW] Worker did not stop within"
                 << waitTimeMs << "ms";
        return false;
    }
    return true;
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

    if (myThread) {
        myThread->setTemplatePreviewMode(
                    false,
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
    if (!m_bOpenDevice || !m_cameraDevice) {
        QMessageBox::warning(this, "提示", "请先点击【打开相机】！");
        return false;
    }
    if (isCollecting
            || (cameraThread && cameraThread->isRunning())) {
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
    if (myThread && myThread->isRunning()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "相机采集线程仍在运行，请先停止当前任务。");
        return false;
    }
    if (!myThread) {
        reinitializeMyThread();
    }
    if (!myThread || !myImage) {
        QMessageBox::warning(
                    this,
                    "提示",
                    "实时取景线程初始化失败。");
        return false;
    }

    try {
        if (!m_cameraDevice->setEnumValue(
                "TriggerMode",
                1).isSuccess()
                || !m_cameraDevice->setEnumValue(
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
    clearInspectionTransientDisplay();
    updateOperationUiState();

    myThread->setCameraDevice(m_cameraDevice);
    myThread->getImagePtr(myImage);
    myThread->receiveangle(angleValue);
    myThread->receivecolorchannel1(colorchannel);
    myThread->setTemplatePreviewMode(
                true,
                m_templatePreviewSessionId);
    myThread->start();

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
    *myImage = m_lastTemplatePreviewFrame.clone();
    slot_displayAndDetect(myImage);

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
            && hasTemplateDrawingSelection()) {
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
    QString templateName = "--";

    if (m_currentTemplateNameVisible
            && !m_currentTemplateDisplayName.trimmed().isEmpty()) {
        templateName = m_currentTemplateDisplayName.trimmed();
    } else if (m_currentTemplateNameVisible && !currentTemplateDirPath.isEmpty()) {
        QDir templateDir(currentTemplateDirPath);
        if (templateDir.exists() && !templateDir.dirName().isEmpty()) {
            templateName = templateDir.dirName();
        }
    }

    ui->currentTemplateName->setText(templateName);

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
    if (!ui || m_templateGuideFrame) {
        return;
    }

    m_templateGuideFrame = new QFrame(ui->imagedisplayBox);
    m_templateGuideFrame->setObjectName("templateGuideFrame");
    m_templateGuideFrame->setFrameShape(QFrame::NoFrame);
    m_templateGuideFrame->setStyleSheet(
                "#templateGuideFrame {"
                "background-color: transparent;"
                "border: none;"
                "}");
    m_templateGuideFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_templateGuideFrame->installEventFilter(this);

    QVBoxLayout *guideLayout = new QVBoxLayout(m_templateGuideFrame);
    guideLayout->setContentsMargins(14, 0, 14, 0);
    guideLayout->setSpacing(0);

    m_templateGuideTitleLabel = new QLabel(m_templateGuideFrame);
    m_templateGuideTitleLabel->setStyleSheet("color: #1677d2; font-size: 18px; font-weight: bold; border: none; background: transparent;");
    m_templateGuideTitleLabel->setWordWrap(true);
    m_templateGuideTitleLabel->hide();

    m_templateGuideBodyLabel = new QLabel(m_templateGuideFrame);
    m_templateGuideBodyLabel->setStyleSheet(
                "color: #000000;"
                "font-family: 'Microsoft YaHei';"
                "font-size: 18px;"
                "font-weight: bold;"
                "border: none;"
                "background: transparent;");
    m_templateGuideBodyLabel->setAlignment(
                Qt::AlignLeft | Qt::AlignTop);
    m_templateGuideBodyLabel->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Fixed);
    m_templateGuideBodyLabel->setWordWrap(true);

    guideLayout->addWidget(m_templateGuideBodyLabel);

    ui->verticalLayout_InnerImg->insertWidget(0, m_templateGuideFrame);
    hideTemplateGuide();
}

void Widget::adjustTemplateGuideHeight()
{
    if (!m_templateGuideFrame
            || !m_templateGuideBodyLabel
            || !m_templateGuideFrame->layout()) {
        return;
    }

    const QMargins margins =
            m_templateGuideFrame->layout()->contentsMargins();
    const int availableWidth =
            m_templateGuideFrame->contentsRect().width()
            - margins.left()
            - margins.right();
    if (availableWidth <= 0) {
        return;
    }

    m_templateGuideBodyLabel->ensurePolished();
    const QFontMetrics metrics(m_templateGuideBodyLabel->font());
    const QRect textRect = metrics.boundingRect(
                QRect(0,
                      0,
                      availableWidth,
                      std::numeric_limits<int>::max()),
                Qt::TextWordWrap | Qt::AlignLeft,
                m_templateGuideBodyLabel->text());
    const int contentHeight =
            qMax(metrics.lineSpacing(), textRect.height());

    if (m_templateGuideBodyLabel->height() != contentHeight) {
        m_templateGuideBodyLabel->setFixedHeight(contentHeight);
    }
    if (m_templateGuideFrame->height() != contentHeight) {
        m_templateGuideFrame->setFixedHeight(contentHeight);
    }
}

void Widget::updateTemplateGuideText(const QString &title, const QString &body)
{
    if (!m_templateGuideFrame || !m_templateGuideTitleLabel || !m_templateGuideBodyLabel) {
        return;
    }

    Q_UNUSED(title);
    m_templateGuideTitleLabel->clear();
    m_templateGuideTitleLabel->hide();
    QString guideText = body.trimmed();
    if (!guideText.startsWith("【操作步骤】")) {
        guideText.prepend("【操作步骤】 ");
    }
    if (!guideText.contains("【按下esc退出当前模板制作】")) {
        guideText.append("  【按下esc退出当前模板制作】");
    }
    setLabelTextIfChanged(
                m_templateGuideBodyLabel,
                guideText);
    m_templateGuideFrame->setVisible(true);
    adjustTemplateGuideHeight();
    QTimer::singleShot(0, this, [this]() {
        adjustTemplateGuideHeight();
    });
}

void Widget::hideTemplateGuide()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->hide();
    }
}

void Widget::updateImageDisplayStatusText(const QString &body)
{
    if (!m_templateGuideFrame || !m_templateGuideTitleLabel || !m_templateGuideBodyLabel) {
        return;
    }

    m_templateGuideTitleLabel->clear();
    m_templateGuideTitleLabel->hide();
    setLabelTextIfChanged(
                m_templateGuideBodyLabel,
                QString("【当前状态】 %1").arg(body.trimmed()));
    m_templateGuideFrame->setVisible(true);
    adjustTemplateGuideHeight();
    QTimer::singleShot(0, this, [this]() {
        adjustTemplateGuideHeight();
    });
}

void Widget::showTemplateGuideForCurrentMode()
{
    const int modeIndex = ui->comboBox_4->currentIndex();
    const QString modeId = detectModeIdForIndex(modeIndex);

    if (isSingleTemplateRecipeMode(modeId)) {
        updateTemplateGuideText(
                    modeId == QStringLiteral("ocr_detection")
                        ? "深度模型模板制作"
                        : "模板匹配模板制作",
                    "请按住鼠标左键拖动，框选定位区域。");
        return;
    }

    if (isWordFamilyMode(modeId)) {
        const bool barcodeWordMode =
                modeId == BarcodeWordDetectionMode;
        updateTemplateGuideText(
                    barcodeWordMode
                        ? "二维码+三期模板制作"
                        : "字库匹配模板制作",
                    barcodeWordMode
                        ? "【步骤1/3】请按住鼠标左键拖动，框选稳定且不会变化的定位锚点。"
                        : "请按住鼠标左键拖动，框选定位区域。");
        return;
    }

    hideTemplateGuide();
}

void Widget::handleTemplateGuideEvent(const QString &eventName, int pointCount)
{
    if (!ui || !imageLabel) {
        return;
    }

    const int modeIndex = ui->comboBox_4->currentIndex();
    const QString modeId = detectModeIdForIndex(modeIndex);
    if (!isSingleTemplateRecipeMode(modeId)
            && !isWordFamilyMode(modeId)) {
        if (m_templateGuideFrame && m_templateGuideFrame->isVisible()) {
            hideTemplateGuide();
        }
        return;
    }

    const bool barcodeWordMode =
            modeId == BarcodeWordDetectionMode;
    const bool guideVisible =
            m_templateGuideFrame && m_templateGuideFrame->isVisible();
    const QString title = barcodeWordMode
            ? "二维码+三期模板制作"
            : (modeId == QStringLiteral("word_detection")
               ? "字库匹配模板制作"
               : (modeId == QStringLiteral("ocr_detection")
                  ? "深度模型模板制作"
                  : "模板匹配模板制作"));
    const QString trackingRegionName = "定位区域";

    if (barcodeWordMode
            && (eventName == "tracking_started"
                || eventName == "barcode_started"
                || eventName == "barcode_too_small"
                || eventName == "template_reset")) {
        clearBarcodeTemplateValidation();
    }

    if (barcodeWordMode && eventName == "barcode_done") {
        const QRect barcodeRect =
                imageLabel->getBarcodeRect().normalized();
        BarcodeReadResult barcode;
        QString failureReason;
        if (!validateBarcodeTemplateRect(
                    barcodeRect,
                    barcodeTemplateValidationOptions(),
                    &barcode,
                    &failureReason)) {
            clearBarcodeTemplateValidation();
            imageLabel->retryBarcodeRegion();
            if (guideVisible) {
                updateTemplateGuideText(
                            title,
                            "【步骤2/3】二维码扫描失败，定位锚点已保留，请重新框选二维码区域。");
            }

            QTimer::singleShot(0, this, [this, failureReason]() {
                QMessageBox::warning(
                            this,
                            "二维码扫描失败",
                            failureReason
                            + "\n\n请重新完整框选二维码区域，"
                              "四周保留少量背景，不要包含右侧日期。");
            });
            return;
        }

        m_barcodeTemplateReadable = true;
        m_validatedBarcodeRect = barcodeRect;
        m_validatedBarcodeText = barcode.text;
        if (guideVisible) {
            updateTemplateGuideText(
                        title,
                        "【步骤3/3】二维码扫描成功。请用鼠标左键依次点击日期区域边缘，右键闭合。");
        }
        return;
    }

    if (!guideVisible) {
        return;
    }

    if (eventName == "tracking_started") {
        updateTemplateGuideText(
                    title,
                    barcodeWordMode
                        ? "【步骤1/3】松开鼠标左键完成定位锚点。"
                        : QString("松开鼠标左键完成%1。").arg(trackingRegionName));
    } else if (eventName == "template_reset") {
        updateTemplateGuideText(
                    title,
                    barcodeWordMode
                        ? "【步骤1/3】已清空当前框线，请重新框选稳定定位锚点。"
                        : QString("已清空当前框线，请重新按住鼠标左键拖动，框选%1。")
                          .arg(trackingRegionName));
    } else if (eventName == "tracking_too_small") {
        updateTemplateGuideText(
                    title,
                    barcodeWordMode
                        ? "【步骤1/3】定位锚点太小，请重新框选更大的稳定定位锚点。"
                        : QString("%1太小，请重新框选更大的%1。")
                          .arg(trackingRegionName));
    } else if (eventName == "tracking_done") {
        updateTemplateGuideText(
                    title,
                    barcodeWordMode
                        ? "【步骤2/3】请按住鼠标左键拖动，完整框选二维码区域，四周保留少量背景。"
                        : "请用鼠标左键依次点击喷码区域边缘，右键闭合。");
    } else if (eventName == "barcode_started") {
        updateTemplateGuideText(
                    title,
                    "【步骤2/3】松开鼠标左键后，程序将立即验证二维码是否可读。");
    } else if (eventName == "barcode_too_small") {
        updateTemplateGuideText(
                    title,
                    "【步骤2/3】二维码区域太小，请重新框选完整二维码区域。");
    } else if (eventName == "poly_point_added") {
        updateTemplateGuideText(title,
                                barcodeWordMode
                                    ? QString("【步骤3/3】已选择 %1 个点，继续点击日期区域边缘或右键闭合。")
                                      .arg(pointCount)
                                    : QString("已选择 %1 个点，继续点击边缘或右键闭合。")
                                      .arg(pointCount));
    } else if (eventName == "poly_too_few") {
        updateTemplateGuideText(title,
                                barcodeWordMode
                                    ? QString("【步骤3/3】至少需要3个点，当前%1个，请继续点击日期区域边缘。")
                                      .arg(pointCount)
                                    : QString("至少需要 3 个点，当前 %1 个，请继续点击喷码区域边缘。")
                                      .arg(pointCount));
    } else if (eventName == "poly_done") {
        updateTemplateGuideText(title,
                                barcodeWordMode
                                    ? "【步骤3/3】日期检测区域已完成，请点击【保存模板】。"
                                    : "喷码检测区域已完成，请点击【保存模板】。");
        QTimer::singleShot(0, this, [this]() {
            if (!imageLabel || !imageLabel->isTemplateDrawingEnabled()) {
                return;
            }

            QMessageBox saveMessageBox(this);
            saveMessageBox.setIcon(QMessageBox::Question);
            saveMessageBox.setWindowTitle("保存模板");
            saveMessageBox.setText(
                        currentDetectModeId() == BarcodeWordDetectionMode
                            ? "定位锚点、二维码区域和日期检测区域均已完成。\n\n是否立即保存当前产品模板？"
                            : "喷码检测区域已闭合。\n\n是否立即保存当前产品模板？");
            QPushButton *saveButton = saveMessageBox.addButton("保存", QMessageBox::AcceptRole);
            saveMessageBox.addButton("取消", QMessageBox::RejectRole);
            saveMessageBox.setDefaultButton(saveButton);
            saveMessageBox.exec();

            if (saveMessageBox.clickedButton() == saveButton) {
                on_pushButton_5_clicked();
            }
        });
    }
}

void Widget::setupManualCharacterCropUi()
{
    if (!ui || m_manualCharacterCropButton) {
        return;
    }

    if (!ui->manualCharacterCropButton) {
        return;
    }

    const QString splitPushButtonStyle =
            "QPushButton {"
            "background-color: transparent;"
            "border: 1px solid #ebeef5;"
            "border-radius: 4px;"
            "color: #333333;"
            "padding: 5px 10px;"
            "}"
            "QPushButton:hover {"
            "background-color: #f2f6fc;"
            "}"
            "QPushButton:pressed {"
            "background-color: #ebeef5;"
            "}";

    m_manualCharacterCropButton = ui->manualCharacterCropButton;
    m_manualCharacterCropButton->setStyleSheet(splitPushButtonStyle);
    m_manualCharacterCropButton->setMinimumHeight(42);
    m_manualCharacterCropButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_manualCharacterCropButton->setToolTip("打开当前产品模板的喷码区域图，手动框选字符并批量保存字符模板图片。");
    m_manualCharacterCropButton->installEventFilter(this);

    connect(m_manualCharacterCropButton, &QPushButton::clicked,
            this, &Widget::showManualCharacterTemplateCropDialog);
    refreshWordTemplateEditorCombo();
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
    m_softwareDataDirLineEdit->installEventFilter(this);

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
    if (!AppSettingsManager::clearGlobalSettings(&errorMessage)) {
        showParameterCritical("严重警告", QString("清空软件公共数据失败：\n%1").arg(errorMessage));
        return;
    }

    const GlobalSettings defaultSettings = AppSettingsManager::defaultGlobalSettings();
    m_appliedGlobalSettings = defaultSettings;
    applyGlobalSettingsToUi(defaultSettings);
    clearWordMultiTemplateState();
    currentTemplateDirPath.clear();
    m_loadedTrackingTemplate.release();
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    savedTrackingBox = cv::Rect2d();
    hasValidBoxes = false;
    digitTemplates.clear();
    digitTemplateTargetIndexes.clear();
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
        imageLabel->clearGreenRects();
    }
    clearAllGlobalSettingDirty();
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

    const GlobalSettings defaultSettings = AppSettingsManager::defaultGlobalSettings();
    GlobalSettings editableDefaults = m_appliedGlobalSettings;
    const bool cameraOpen = (m_cameraDevice && m_bOpenDevice);
    const bool plcConnected =
            (m_plcDevice && m_plcDevice->isConnected());

    // 始终可以修改的软件参数。
    editableDefaults.detectModeId = defaultSettings.detectModeId;
    editableDefaults.imageSaveModeId = defaultSettings.imageSaveModeId;
    editableDefaults.imageSaveTypeId = defaultSettings.imageSaveTypeId;
    editableDefaults.imageSavePath = defaultSettings.imageSavePath;
    editableDefaults.templateBaseDirPath = defaultSettings.templateBaseDirPath;
    editableDefaults.colorChannelId = defaultSettings.colorChannelId;
    editableDefaults.imageRotationId = defaultSettings.imageRotationId;
    editableDefaults.triggerEnabled = defaultSettings.triggerEnabled;
    editableDefaults.tissueRoughnessThreshold = defaultSettings.tissueRoughnessThreshold;
    editableDefaults.templateDirPathsByMode.clear();
    editableDefaults.publishedRecipeIdsByMode.clear();

    // 相机参数只有在相机已打开、控件可设置时才恢复。
    if (cameraOpen) {
        editableDefaults.cameraExposure = defaultSettings.cameraExposure;
        editableDefaults.cameraGain = defaultSettings.cameraGain;
    }

    // PLC连接参数只在PLC未连接、控件可编辑时恢复。
    if (!plcConnected) {
        editableDefaults.plcIp = defaultSettings.plcIp;
        editableDefaults.plcRack = defaultSettings.plcRack;
        editableDefaults.plcSlot = defaultSettings.plcSlot;
    }

    // PLC运行参数只有在PLC已连接、控件可设置时才恢复。
    if (plcConnected) {
        editableDefaults.triggerModeId = defaultSettings.triggerModeId;
        editableDefaults.plcModeId = defaultSettings.plcModeId;
        editableDefaults.photoDistance = defaultSettings.photoDistance;
        editableDefaults.photoTime = defaultSettings.photoTime;
        editableDefaults.cameraDelay = defaultSettings.cameraDelay;
        editableDefaults.rejectDistance = defaultSettings.rejectDistance;
        editableDefaults.rejectTime = defaultSettings.rejectTime;
        editableDefaults.rejectPosition = defaultSettings.rejectPosition;
    }

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
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
        imageLabel->clearGreenRects();
    }
    clearTemplatePrivateSettingDirty();
    updateHardwareParameterUiEnabled();
    refreshAllGlobalSettingDirty();

    if (!saveSettings(false)) {
        showParameterCritical("严重警告", "恢复默认设置失败：公共配置保存失败。");
        return;
    }

    showParameterInfo(
        "提示",
        "当前可设置参数已恢复为默认值。带 * 的参数需要点击对应【设置】后才会生效。");
}

void Widget::restoreCameraHardwareUiFromApplied()
{
    const bool oldUpdating = m_updatingGlobalSettingsUi;
    m_updatingGlobalSettingsUi = true;

    QSignalBlocker exposureBlocker(ui->spinBox);
    QSignalBlocker gainBlocker(ui->lineEdit_14);
    ui->spinBox->setValue(m_appliedGlobalSettings.cameraExposure);
    ui->lineEdit_14->setText(QString::number(static_cast<int>(m_appliedGlobalSettings.cameraGain)));

    m_updatingGlobalSettingsUi = oldUpdating;
    refreshGlobalSettingsDirty(QStringList() << "camera.exposure" << "camera.gain");
}

void Widget::restorePlcRunUiFromApplied()
{
    const bool oldUpdating = m_updatingGlobalSettingsUi;
    m_updatingGlobalSettingsUi = true;

    const int triggerModeIndex =
            (m_appliedGlobalSettings.triggerModeId == "trigger_continuous") ? 0 : 1;

    QSignalBlocker triggerModeBlocker(ui->comboBox_3);
    QSignalBlocker photoDistanceBlocker(ui->lineEdit_6);
    QSignalBlocker photoTimeBlocker(ui->lineEdit_20);
    QSignalBlocker cameraDelayBlocker(ui->lineEdit_4);
    QSignalBlocker rejectDistanceBlocker(ui->lineEdit_7);
    QSignalBlocker rejectTimeBlocker(ui->lineEdit_8);
    QSignalBlocker rejectPositionBlocker(ui->lineEdit_12);

    ui->comboBox_3->setCurrentIndex(triggerModeIndex);
    ui->lineEdit_6->setText(QString::number(m_appliedGlobalSettings.photoDistance));
    ui->lineEdit_20->setText(QString::number(m_appliedGlobalSettings.photoTime));
    ui->lineEdit_4->setText(QString::number(m_appliedGlobalSettings.cameraDelay));
    ui->lineEdit_7->setText(QString::number(m_appliedGlobalSettings.rejectDistance));
    ui->lineEdit_8->setText(QString::number(m_appliedGlobalSettings.rejectTime));
    ui->lineEdit_12->setText(QString::number(m_appliedGlobalSettings.rejectPosition));

    m_updatingGlobalSettingsUi = oldUpdating;
    refreshGlobalSettingsDirty(QStringList()
                               << "plc.trigger_mode"
                               << "plc.photo_distance"
                               << "plc.photo_time"
                               << "plc.camera_delay"
                               << "plc.reject_distance"
                               << "plc.reject_time"
                               << "plc.reject_position");
}

QString Widget::hardwareDisabledStyle(QWidget *widget) const
{
    if (qobject_cast<QPushButton *>(widget)) {
        return QString(
                    "QPushButton {"
                    "background-color: #f5f7fa;"
                    "color: #a8abb2;"
                    "border: 1px solid #e4e7ed;"
                    "border-radius: 4px;"
                    "}"
                    "QPushButton:hover { background-color: #f5f7fa; }"
                    "QPushButton:pressed { background-color: #f5f7fa; }");
    }
    if (qobject_cast<QComboBox *>(widget)) {
        return QString(
                    "QComboBox {"
                    "background-color: #f5f7fa;"
                    "color: #a8abb2;"
                    "border: 1px solid #e4e7ed;"
                    "border-radius: 4px;"
                    "}"
                    "QComboBox::drop-down {"
                    "background-color: #eef0f3;"
                    "border-left: 1px solid #e4e7ed;"
                    "}");
    }
    if (qobject_cast<QAbstractSpinBox *>(widget)) {
        return QString(
                    "QAbstractSpinBox {"
                    "background-color: #f5f7fa;"
                    "color: #a8abb2;"
                    "border: 1px solid #e4e7ed;"
                    "border-radius: 4px;"
                    "}"
                    "QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {"
                    "background-color: #eef0f3;"
                    "}");
    }
    if (qobject_cast<QLineEdit *>(widget)) {
        return QString(
                    "QLineEdit {"
                    "background-color: #f5f7fa;"
                    "color: #a8abb2;"
                    "border: 1px solid #e4e7ed;"
                    "border-radius: 4px;"
                    "padding: 5px 10px;"
                    "}");
    }
    if (qobject_cast<QLabel *>(widget)) {
        return QString(
                    "QLabel {"
                    "background-color: #f5f7fa;"
                    "color: #a8abb2;"
                    "border: 1px solid #e4e7ed;"
                    "border-radius: 4px;"
                    "padding: 5px 10px;"
                    "}");
    }
    return QString();
}

void Widget::setHardwareControlEnabled(QWidget *widget,
                                       bool enabled,
                                       const QString &disabledReason,
                                       bool showDisabledReason)
{
    if (!widget) {
        return;
    }

    static const char originalStyleProperty[] = "_hardwareOriginalStyleSheet";
    static const char originalToolTipProperty[] = "_hardwareOriginalToolTip";
    if (!widget->property(originalStyleProperty).isValid()) {
        widget->setProperty(originalStyleProperty, widget->styleSheet());
    }
    if (!widget->property(originalToolTipProperty).isValid()) {
        widget->setProperty(originalToolTipProperty, widget->toolTip());
    }

    const QString originalStyle = widget->property(originalStyleProperty).toString();
    const QString originalToolTip = widget->property(originalToolTipProperty).toString();
    const bool isLabel = qobject_cast<QLabel *>(widget) != nullptr;
    widget->setEnabled(isLabel ? true : enabled);

    if (enabled) {
        widget->setStyleSheet(originalStyle);
        widget->setToolTip(originalToolTip);
        widget->unsetCursor();
        return;
    }

    const QString disabledStyle = hardwareDisabledStyle(widget);
    widget->setStyleSheet(disabledStyle.isEmpty()
                          ? originalStyle
                          : disabledStyle);
    widget->setToolTip(showDisabledReason ? disabledReason : originalToolTip);
    if (showDisabledReason) {
        widget->setCursor(Qt::ForbiddenCursor);
    } else {
        widget->unsetCursor();
    }
}

void Widget::updateHardwareParameterUiEnabled()
{
    const bool cameraOpen = (m_cameraDevice && m_bOpenDevice);
    const bool plcConnected =
            (m_plcDevice && m_plcDevice->isConnected());
    const QString cameraDisabledReason = "请先打开相机后再设置该参数。";
    const QString plcRunDisabledReason = "请先连接 PLC 后再设置该参数。";
    const QString plcConnectDisabledReason = "PLC 已连接。如需修改连接参数，请先断开 PLC。";

    if (!cameraOpen) {
        restoreCameraHardwareUiFromApplied();
    }
    if (!plcConnected) {
        restorePlcRunUiFromApplied();
    }

    auto dependencyState = [&](HardwareDependency dependency,
                               bool *enabled,
                               QString *disabledReason) {
        if (!enabled || !disabledReason) {
            return;
        }
        switch (dependency) {
        case HardwareDependency::Camera:
            *enabled = cameraOpen;
            *disabledReason = cameraDisabledReason;
            break;
        case HardwareDependency::PlcConnection:
            *enabled = !plcConnected;
            *disabledReason = plcConnectDisabledReason;
            break;
        case HardwareDependency::PlcRuntime:
            *enabled = plcConnected;
            *disabledReason = plcRunDisabledReason;
            break;
        case HardwareDependency::None:
            *enabled = true;
            disabledReason->clear();
            break;
        }
    };

    for (auto it = m_globalSettingBindings.constBegin();
         it != m_globalSettingBindings.constEnd();
         ++it) {
        const GlobalSettingBinding &binding = it.value();
        if (binding.hardwareDependency == HardwareDependency::None) {
            continue;
        }

        bool enabled = true;
        QString disabledReason;
        dependencyState(binding.hardwareDependency, &enabled, &disabledReason);
        setHardwareControlEnabled(binding.editor, enabled, disabledReason, true);
        setHardwareControlEnabled(binding.label, enabled, disabledReason, false);
    }

    for (const HardwareActionBinding &binding : m_hardwareActionBindings) {
        bool enabled = true;
        QString disabledReason;
        dependencyState(binding.hardwareDependency, &enabled, &disabledReason);
        setHardwareControlEnabled(binding.control, enabled, disabledReason, true);
    }

    const bool operationBusy =
            m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_operationState == OperationState::TemplatePreviewing
            || m_operationState == OperationState::TemplateFrozen;
    if (operationBusy) {
        updateOperationUiState();
    }
}

void Widget::setupNumericInputValidators()
{
    auto setIntValidator = [this](QLineEdit *lineEdit) {
        if (lineEdit) {
            lineEdit->setValidator(new QIntValidator(0, 2147483647, lineEdit));
        }
    };

    setIntValidator(ui->lineEdit_14);
    setIntValidator(ui->lineEdit_2);
    setIntValidator(ui->lineEdit_3);
    setIntValidator(ui->lineEdit_6);
    setIntValidator(ui->lineEdit_20);
    setIntValidator(ui->lineEdit_4);
    setIntValidator(ui->lineEdit_7);
    setIntValidator(ui->lineEdit_8);
    setIntValidator(ui->lineEdit_12);

    if (ui->lineEdit_yuzhi) {
        ui->lineEdit_yuzhi->setValidator(
                    new QIntValidator(0, 100, ui->lineEdit_yuzhi));
        ui->lineEdit_yuzhi->setMaxLength(3);
        ui->lineEdit_yuzhi->setToolTip(
                    "请输入0到100之间的整数，单位：%");
    }

    if (ui->lineEdit_tissueRoughnessThreshold) {
        QDoubleValidator *validator = new QDoubleValidator(0.001, 1000000.0, 3, ui->lineEdit_tissueRoughnessThreshold);
        validator->setNotation(QDoubleValidator::StandardNotation);
        ui->lineEdit_tissueRoughnessThreshold->setValidator(validator);
    }
}

void Widget::setupGlobalSettingBindings()
{
    m_globalSettingBindings.clear();
    m_hardwareActionBindings.clear();

    registerGlobalSetting("camera.exposure",
                          ui->spinBox,
                          ui->label_19,
                          true,
                          HardwareDependency::Camera);
    registerGlobalSetting("camera.gain",
                          ui->lineEdit_14,
                          ui->label_16,
                          true,
                          HardwareDependency::Camera);
    registerGlobalSetting("image.color_channel", ui->comboBox_5, ui->label_12, true);
    registerGlobalSetting("image.rotation", ui->comboBox_2, ui->label_27, true);
    registerGlobalSetting("tissue.roughness_threshold",
                          ui->lineEdit_tissueRoughnessThreshold,
                          ui->label_tissueRoughnessThreshold,
                          true);
    registerGlobalSetting("plc.trigger_mode",
                          ui->comboBox_3,
                          ui->label_15,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_distance",
                          ui->lineEdit_6,
                          ui->label_8,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.photo_time",
                          ui->lineEdit_20,
                          ui->label_14,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.camera_delay",
                          ui->lineEdit_4,
                          ui->label_13,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_distance",
                          ui->lineEdit_7,
                          ui->label_6,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_time",
                          ui->lineEdit_8,
                          ui->label_10,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.reject_position",
                          ui->lineEdit_12,
                          ui->label_17,
                          true,
                          HardwareDependency::PlcRuntime);
    registerGlobalSetting("plc.ip",
                          ui->lineEdit,
                          ui->label_2,
                          false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.rack",
                          ui->lineEdit_2,
                          ui->label_3,
                          false,
                          HardwareDependency::PlcConnection);
    registerGlobalSetting("plc.slot",
                          ui->lineEdit_3,
                          ui->label_3,
                          false,
                          HardwareDependency::PlcConnection);

    registerGlobalSetting("detect.mode", ui->comboBox_4, ui->label_18, false);
    registerGlobalSetting("image.save_mode", ui->comboBox, ui->label_11, false);
    registerGlobalSetting("image.save_type", ui->comboBox_saveImageType, ui->label_saveImageType, false);
    registerGlobalSetting("image.save_path", ui->lineEdit_imageSavePath, ui->label_imageSavePath, false);
    registerGlobalSetting("trigger.enabled", ui->checkBox, nullptr, false);

    registerHardwareAction(ui->sureButton, HardwareDependency::Camera);
    registerHardwareAction(ui->pushButton_12, HardwareDependency::Camera);
    registerHardwareAction(ui->ConnectpushButton, HardwareDependency::PlcConnection);
    registerHardwareAction(ui->DisconnectpushButton, HardwareDependency::PlcRuntime);
    registerHardwareAction(ui->plcmodebtn, HardwareDependency::PlcRuntime);
    registerHardwareAction(ui->WriteVDpushButton, HardwareDependency::PlcRuntime);
    registerHardwareAction(ui->pushButton_8, HardwareDependency::PlcRuntime);

    connect(ui->comboBox,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this,
            [this](int) {
        updateImageSaveOptionsVisibility();
    });
    updateImageSaveOptionsVisibility();
}

void Widget::registerGlobalSetting(const QString &key,
                                   QWidget *editor,
                                   QLabel *label,
                                   bool requireApply,
                                   HardwareDependency hardwareDependency)
{
    if (key.trimmed().isEmpty() || !editor) {
        return;
    }

    GlobalSettingBinding binding;
    binding.key = key;
    binding.editor = editor;
    binding.label = label;
    binding.originalLabelText = label ? label->text() : QString();
    binding.requireApply = requireApply;
    binding.dirty = false;
    binding.hardwareDependency = hardwareDependency;
    m_globalSettingBindings.insert(key, binding);

    auto onChanged = [this, key]() {
        if (m_updatingGlobalSettingsUi || m_applyingGlobalSettings) {
            return;
        }

        auto it = m_globalSettingBindings.find(key);
        if (it == m_globalSettingBindings.end()) {
            return;
        }

        if (it.value().requireApply) {
            refreshGlobalSettingDirty(key);
        } else {
            if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(it.value().editor)) {
                if (!lineEdit->hasAcceptableInput()) {
                    return;
                }
            }
            updateAppliedGlobalSettingFromUi(key);
            saveSettings(false);
        }
    };

    if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor)) {
        connect(lineEdit, &QLineEdit::textChanged, this, [onChanged](const QString &) {
            onChanged();
        });
    } else if (QSpinBox *spinBox = qobject_cast<QSpinBox *>(editor)) {
        connect(spinBox,
                static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this,
                [onChanged](int) {
            onChanged();
        });
    } else if (QComboBox *comboBox = qobject_cast<QComboBox *>(editor)) {
        connect(comboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [onChanged](int) {
            onChanged();
        });
    } else if (QCheckBox *checkBox = qobject_cast<QCheckBox *>(editor)) {
        connect(checkBox, &QCheckBox::toggled, this, [onChanged](bool) {
            onChanged();
        });
    }
}

void Widget::registerHardwareAction(QWidget *control,
                                    HardwareDependency hardwareDependency)
{
    if (!control || hardwareDependency == HardwareDependency::None) {
        return;
    }

    HardwareActionBinding binding;
    binding.control = control;
    binding.hardwareDependency = hardwareDependency;
    m_hardwareActionBindings.append(binding);
}

bool Widget::isGlobalSettingDirtyByValue(const QString &key) const
{
    const auto it = m_globalSettingBindings.constFind(key);
    if (it == m_globalSettingBindings.constEnd() || !it.value().requireApply) {
        return false;
    }

    auto lineEditIntDirty = [](QLineEdit *lineEdit, int appliedValue) {
        int value = 0;
        return !lineEdit || !parseIntValue(lineEdit->text(), &value) || value != appliedValue;
    };
    auto lineEditDoubleDirty = [](QLineEdit *lineEdit, double appliedValue) {
        double value = 0.0;
        return !lineEdit || !parseDoubleValue(lineEdit->text(), &value) || !fuzzyEqual(value, appliedValue);
    };
    auto comboDirty = [](QComboBox *comboBox, const QStringList &ids, const QString &appliedId) {
        if (!comboBox) {
            return true;
        }
        return idAt(ids, comboBox->currentIndex(), QString()) != appliedId;
    };

    if (key == "camera.exposure") {
        return ui->spinBox->value() != m_appliedGlobalSettings.cameraExposure;
    }
    if (key == "camera.gain") {
        return lineEditIntDirty(ui->lineEdit_14, static_cast<int>(m_appliedGlobalSettings.cameraGain));
    }
    if (key == "image.color_channel") {
        return comboDirty(ui->comboBox_5, colorChannelIds(), m_appliedGlobalSettings.colorChannelId);
    }
    if (key == "image.rotation") {
        return comboDirty(ui->comboBox_2, rotationIds(), m_appliedGlobalSettings.imageRotationId);
    }
    if (key == "tissue.roughness_threshold") {
        return lineEditDoubleDirty(ui->lineEdit_tissueRoughnessThreshold,
                                   m_appliedGlobalSettings.tissueRoughnessThreshold);
    }
    if (key == "plc.trigger_mode") {
        return comboDirty(ui->comboBox_3, triggerModeIds(), m_appliedGlobalSettings.triggerModeId);
    }
    if (key == "plc.photo_distance") {
        return lineEditIntDirty(ui->lineEdit_6, m_appliedGlobalSettings.photoDistance);
    }
    if (key == "plc.photo_time") {
        return lineEditIntDirty(ui->lineEdit_20, m_appliedGlobalSettings.photoTime);
    }
    if (key == "plc.camera_delay") {
        return lineEditIntDirty(ui->lineEdit_4, m_appliedGlobalSettings.cameraDelay);
    }
    if (key == "plc.reject_distance") {
        return lineEditIntDirty(ui->lineEdit_7, m_appliedGlobalSettings.rejectDistance);
    }
    if (key == "plc.reject_time") {
        return lineEditIntDirty(ui->lineEdit_8, m_appliedGlobalSettings.rejectTime);
    }
    if (key == "plc.reject_position") {
        return lineEditIntDirty(ui->lineEdit_12, m_appliedGlobalSettings.rejectPosition);
    }
    if (key == "plc.ip") {
        return ui->lineEdit->text().trimmed() != m_appliedGlobalSettings.plcIp.trimmed();
    }
    if (key == "plc.rack") {
        return lineEditIntDirty(ui->lineEdit_2, m_appliedGlobalSettings.plcRack);
    }
    if (key == "plc.slot") {
        return lineEditIntDirty(ui->lineEdit_3, m_appliedGlobalSettings.plcSlot);
    }

    return false;
}

void Widget::refreshGlobalSettingDirty(const QString &key)
{
    auto it = m_globalSettingBindings.find(key);
    if (it == m_globalSettingBindings.end()) {
        return;
    }

    it.value().dirty = isGlobalSettingDirtyByValue(key);
    updateGlobalSettingDirtyUi(key);
}

void Widget::refreshGlobalSettingsDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        refreshGlobalSettingDirty(key);
    }
}

void Widget::refreshAllGlobalSettingDirty()
{
    for (auto it = m_globalSettingBindings.constBegin();
         it != m_globalSettingBindings.constEnd();
         ++it) {
        refreshGlobalSettingDirty(it.key());
    }
}

void Widget::markGlobalSettingDirty(const QString &key)
{
    auto it = m_globalSettingBindings.find(key);
    if (it == m_globalSettingBindings.end()) {
        return;
    }

    it.value().dirty = true;
    updateGlobalSettingDirtyUi(key);
}

void Widget::clearGlobalSettingDirty(const QString &key)
{
    auto it = m_globalSettingBindings.find(key);
    if (it == m_globalSettingBindings.end()) {
        return;
    }

    it.value().dirty = false;
    updateGlobalSettingDirtyUi(key);
}

void Widget::clearGlobalSettingsDirty(const QStringList &keys)
{
    for (const QString &key : keys) {
        clearGlobalSettingDirty(key);
    }
}

void Widget::clearAllGlobalSettingDirty()
{
    for (auto it = m_globalSettingBindings.begin(); it != m_globalSettingBindings.end(); ++it) {
        it.value().dirty = false;
    }

    for (auto it = m_globalSettingBindings.constBegin(); it != m_globalSettingBindings.constEnd(); ++it) {
        updateGlobalSettingDirtyUi(it.key());
    }
}

void Widget::updateGlobalSettingDirtyUi(const QString &key)
{
    auto it = m_globalSettingBindings.find(key);
    if (it == m_globalSettingBindings.end() || !it.value().label) {
        return;
    }

    QLabel *targetLabel = it.value().label;
    const QString originalText = it.value().originalLabelText;
    bool anyDirtyOnSameLabel = false;
    for (auto scan = m_globalSettingBindings.constBegin();
         scan != m_globalSettingBindings.constEnd();
         ++scan) {
        if (scan.value().label == targetLabel && scan.value().dirty) {
            anyDirtyOnSameLabel = true;
            break;
        }
    }

    targetLabel->setText(anyDirtyOnSameLabel ? originalText + " *" : originalText);
}

void Widget::updateAppliedGlobalSettingFromUi(const QString &key)
{
    static const QStringList detectModeIds = {
        "stamp_detection",
        "word_detection",
        "ocr_detection",
        "tissue_detection",
        BarcodeWordDetectionMode
    };
    static const QStringList imageSaveModeIds = {"save_none", "save_ng", "save_ok", "save_all"};
    static const QStringList imageSaveTypeIds = {"save_both", "save_annotated_only", "save_raw_only"};
    static const QStringList colorChannelIds = {"color", "red", "green", "blue"};
    static const QStringList rotationIds = {"rotate_none", "rotate_clockwise_90", "rotate_counterclockwise_90", "rotate_180"};
    static const QStringList triggerModeIds = {"trigger_continuous", "trigger_interval"};

    auto idAt = [](const QStringList &ids, int index, const QString &fallback) {
        return (index >= 0 && index < ids.size()) ? ids.at(index) : fallback;
    };

    if (key == "detect.mode") {
        m_appliedGlobalSettings.detectModeId = idAt(detectModeIds,
                                                    ui->comboBox_4->currentIndex(),
                                                    m_appliedGlobalSettings.detectModeId);
        m_appliedGlobalSettings.plcModeId = m_appliedGlobalSettings.triggerModeId;
    } else if (key == "image.save_mode") {
        m_appliedGlobalSettings.imageSaveModeId = idAt(imageSaveModeIds,
                                                       ui->comboBox->currentIndex(),
                                                       m_appliedGlobalSettings.imageSaveModeId);
    } else if (key == "image.save_type") {
        m_appliedGlobalSettings.imageSaveTypeId = idAt(imageSaveTypeIds,
                                                       ui->comboBox_saveImageType->currentIndex(),
                                                       m_appliedGlobalSettings.imageSaveTypeId);
    } else if (key == "image.save_path") {
        m_appliedGlobalSettings.imageSavePath = selectedDir;
    } else if (key == "trigger.enabled") {
        m_appliedGlobalSettings.triggerEnabled = ui->checkBox->isChecked();
    } else if (key == "template.base_dir") {
        m_appliedGlobalSettings.templateBaseDirPath = templateBaseDirPath;
    } else if (key == "template.history_paths") {
        storeCurrentTemplatePathsForMode(currentDetectModeId());
        m_appliedGlobalSettings.templateDirPathsByMode = m_templateDirPathsByMode;
        m_appliedGlobalSettings.publishedRecipeIdsByMode =
                m_publishedRecipeIdsByMode;
    } else if (key == "camera.exposure") {
        m_appliedGlobalSettings.cameraExposure = ui->spinBox->value();
    } else if (key == "camera.gain") {
        m_appliedGlobalSettings.cameraGain = ui->lineEdit_14->text().toInt();
    } else if (key == "image.color_channel") {
        m_appliedGlobalSettings.colorChannelId = idAt(colorChannelIds,
                                                      ui->comboBox_5->currentIndex(),
                                                      m_appliedGlobalSettings.colorChannelId);
    } else if (key == "image.rotation") {
        m_appliedGlobalSettings.imageRotationId = idAt(rotationIds,
                                                       ui->comboBox_2->currentIndex(),
                                                       m_appliedGlobalSettings.imageRotationId);
    } else if (key == "tissue.roughness_threshold") {
        m_appliedGlobalSettings.tissueRoughnessThreshold = ui->lineEdit_tissueRoughnessThreshold->text().toDouble();
    } else if (key == "plc.trigger_mode") {
        m_appliedGlobalSettings.triggerModeId = idAt(triggerModeIds,
                                                     ui->comboBox_3->currentIndex(),
                                                     m_appliedGlobalSettings.triggerModeId);
        m_appliedGlobalSettings.plcModeId = m_appliedGlobalSettings.triggerModeId;
    } else if (key == "plc.photo_distance") {
        m_appliedGlobalSettings.photoDistance = ui->lineEdit_6->text().toInt();
    } else if (key == "plc.photo_time") {
        m_appliedGlobalSettings.photoTime = ui->lineEdit_20->text().toInt();
    } else if (key == "plc.camera_delay") {
        m_appliedGlobalSettings.cameraDelay = ui->lineEdit_4->text().toInt();
    } else if (key == "plc.reject_distance") {
        m_appliedGlobalSettings.rejectDistance = ui->lineEdit_7->text().toInt();
    } else if (key == "plc.reject_time") {
        m_appliedGlobalSettings.rejectTime = ui->lineEdit_8->text().toInt();
    } else if (key == "plc.reject_position") {
        m_appliedGlobalSettings.rejectPosition = ui->lineEdit_12->text().toInt();
    } else if (key == "plc.ip") {
        m_appliedGlobalSettings.plcIp = ui->lineEdit->text().trimmed();
    } else if (key == "plc.rack") {
        m_appliedGlobalSettings.plcRack = ui->lineEdit_2->text().toInt();
    } else if (key == "plc.slot") {
        m_appliedGlobalSettings.plcSlot = ui->lineEdit_3->text().toInt();
    }
}

void Widget::updateAppliedGlobalSettingsFromUi(const QStringList &keys)
{
    for (const QString &key : keys) {
        updateAppliedGlobalSettingFromUi(key);
    }
}

void Widget::syncImmediateGlobalSettingsFromUi()
{
    updateAppliedGlobalSettingsFromUi(QStringList()
                                      << "detect.mode"
                                      << "image.save_mode"
                                      << "image.save_type"
                                      << "image.save_path"
                                      << "trigger.enabled"
                                      << "plc.ip"
                                      << "plc.rack"
                                      << "plc.slot"
                                       << "template.base_dir"
                                       << "template.history_paths");
    if (ui->rightPanelSplitter) {
        m_appliedGlobalSettings.rightPanelSplitterState =
                ui->rightPanelSplitter->saveState();
    }
}

QStringList Widget::dirtyGlobalSettingNames() const
{
    QStringList names;
    for (auto it = m_globalSettingBindings.constBegin();
         it != m_globalSettingBindings.constEnd();
         ++it) {
        if (!it.value().dirty) {
            continue;
        }

        QString name = it.value().originalLabelText.trimmed();
        name.remove(":");
        name.remove("：");
        if (name.isEmpty()) {
            name = it.key();
        }
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

QStringList Widget::dirtyTemplateSettingNames() const
{
    QStringList names;
    if (m_templateTargetTextDirty) {
        names.append("目标字符内容");
    }
    if (m_templateImageThresholdDirty) {
        names.append("图像合格阈值");
    }
    return names;
}

QStringList Widget::dirtySettingNames() const
{
    QStringList names = dirtyGlobalSettingNames();
    const QStringList templateNames = dirtyTemplateSettingNames();
    for (const QString &name : templateNames) {
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

bool Widget::hasDirtySettings() const
{
    return !dirtySettingNames().isEmpty();
}

QString Widget::dirtySettingsMessage() const
{
    const QStringList names = dirtySettingNames();
    if (names.isEmpty()) {
        return QString();
    }

    QStringList lines;
    for (const QString &name : names) {
        lines.append(QString("- %1").arg(name));
    }
    return QString("存在未应用参数：\n\n%1\n\n"
                   "继续运行将放弃以上未应用修改，并使用之前已设置的参数。")
            .arg(lines.join("\n"));
}

void Widget::restoreUnappliedSettingsFromApplied()
{
    static const QStringList colorChannelIds = {"color", "red", "green", "blue"};
    static const QStringList rotationIds = {
        "rotate_none",
        "rotate_clockwise_90",
        "rotate_counterclockwise_90",
        "rotate_180"
    };
    static const QStringList triggerModeIds = {"trigger_continuous", "trigger_interval"};

    auto indexOf = [](const QStringList &ids, const QString &id, int fallback) {
        const int index = ids.indexOf(id);
        return index >= 0 ? index : fallback;
    };

    const bool oldUpdating = m_updatingGlobalSettingsUi;
    m_updatingGlobalSettingsUi = true;

    QSignalBlocker exposureBlocker(ui->spinBox);
    QSignalBlocker gainBlocker(ui->lineEdit_14);
    QSignalBlocker channelBlocker(ui->comboBox_5);
    QSignalBlocker rotationBlocker(ui->comboBox_2);
    QSignalBlocker tissueThresholdBlocker(ui->lineEdit_tissueRoughnessThreshold);
    QSignalBlocker triggerModeBlocker(ui->comboBox_3);
    QSignalBlocker photoDistanceBlocker(ui->lineEdit_6);
    QSignalBlocker photoTimeBlocker(ui->lineEdit_20);
    QSignalBlocker cameraDelayBlocker(ui->lineEdit_4);
    QSignalBlocker rejectDistanceBlocker(ui->lineEdit_7);
    QSignalBlocker rejectTimeBlocker(ui->lineEdit_8);
    QSignalBlocker rejectPositionBlocker(ui->lineEdit_12);

    ui->spinBox->setValue(m_appliedGlobalSettings.cameraExposure);
    ui->lineEdit_14->setText(QString::number(static_cast<int>(m_appliedGlobalSettings.cameraGain)));
    ui->comboBox_5->setCurrentIndex(
                indexOf(colorChannelIds, m_appliedGlobalSettings.colorChannelId, 0));
    ui->comboBox_2->setCurrentIndex(
                indexOf(rotationIds, m_appliedGlobalSettings.imageRotationId, 0));
    ui->lineEdit_tissueRoughnessThreshold->setText(
                QString::number(m_appliedGlobalSettings.tissueRoughnessThreshold, 'f', 3));
    ui->comboBox_3->setCurrentIndex(
                indexOf(triggerModeIds, m_appliedGlobalSettings.triggerModeId, 1));
    ui->lineEdit_6->setText(QString::number(m_appliedGlobalSettings.photoDistance));
    ui->lineEdit_20->setText(QString::number(m_appliedGlobalSettings.photoTime));
    ui->lineEdit_4->setText(QString::number(m_appliedGlobalSettings.cameraDelay));
    ui->lineEdit_7->setText(QString::number(m_appliedGlobalSettings.rejectDistance));
    ui->lineEdit_8->setText(QString::number(m_appliedGlobalSettings.rejectTime));
    ui->lineEdit_12->setText(QString::number(m_appliedGlobalSettings.rejectPosition));

    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
        const TemplatePrivateSettings &settings =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)].settings;
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker imageThresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(QString::number(static_cast<int>(settings.imageThreshold)));
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_singleTemplateRecipeEditSession.isActive()
               && m_singleTemplateRecipeEditSession.recipe().profiles.size()
                  == 1) {
        const TemplatePrivateSettings settings =
                templatePrivateSettingsFromRecipeProfile(
                    m_singleTemplateRecipeEditSession.recipe()
                    .profiles.first());
        QSignalBlocker targetTextBlocker(ui->dateEdit);
        QSignalBlocker imageThresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->setPlainText(settings.targetText);
        ui->lineEdit_yuzhi->setText(
                    QString::number(
                        static_cast<int>(settings.imageThreshold)));
    }

    m_updatingGlobalSettingsUi = oldUpdating;
    refreshAllGlobalSettingDirty();
    refreshTemplatePrivateSettingDirty();
}

void Widget::setupTemplatePrivateSettingDirtyTracking()
{
    m_templateTargetLabelText = ui->label ? ui->label->text() : QString("目标字符内容:");
    m_templateThresholdLabelText = ui->label_4 ? ui->label_4->text() : QString("图像合格阈值:");

    if (ui->dateEdit) {
        connect(ui->dateEdit, &QTextEdit::textChanged, this, [this]() {
            if (m_updatingGlobalSettingsUi || m_applyingGlobalSettings) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_wordTemplateProfiles.empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_singleTemplateRecipeEditSession.isActive())) {
                refreshTemplateTargetTextDirty();
            }
        });
    }
    if (ui->lineEdit_yuzhi) {
        connect(ui->lineEdit_yuzhi, &QLineEdit::textChanged, this, [this](const QString &) {
            if (m_updatingGlobalSettingsUi || m_applyingGlobalSettings) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_wordTemplateProfiles.empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_singleTemplateRecipeEditSession.isActive())) {
                refreshTemplateImageThresholdDirty();
            }
        });
    }
}

void Widget::refreshTemplateTargetTextDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (ui && isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
        const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        dirty = (ui->dateEdit->toPlainText() != profile.settings.targetText);
    } else if (ui
               && isSingleTemplateRecipeMode(currentDetectModeId())
               && m_singleTemplateRecipeEditSession.isActive()
               && m_singleTemplateRecipeEditSession.recipe().profiles.size() == 1) {
        dirty = ui->dateEdit->toPlainText()
                != m_singleTemplateRecipeEditSession.recipe()
                   .profiles.first().targetText;
    }

    m_templateTargetTextDirty = dirty;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::refreshTemplateImageThresholdDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (ui && isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
        int thresholdValue = 0;
        if (!parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue)) {
            dirty = true;
        } else {
            const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
            dirty = (thresholdValue != static_cast<int>(profile.settings.imageThreshold));
        }
    } else if (ui
               && isSingleTemplateRecipeMode(currentDetectModeId())
               && m_singleTemplateRecipeEditSession.isActive()
               && m_singleTemplateRecipeEditSession.recipe().profiles.size() == 1) {
        int thresholdValue = 0;
        if (!parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue)) {
            dirty = true;
        } else {
            dirty = thresholdValue
                    != static_cast<int>(
                        m_singleTemplateRecipeEditSession.recipe()
                        .profiles.first().imageThreshold);
        }
    }

    m_templateImageThresholdDirty = dirty;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::refreshTemplatePrivateSettingDirty()
{
    refreshTemplateTargetTextDirty();
    refreshTemplateImageThresholdDirty();
}

void Widget::markTemplateTargetTextDirty()
{
    m_templateTargetTextDirty = true;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::markTemplateImageThresholdDirty()
{
    m_templateImageThresholdDirty = true;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::clearTemplateTargetTextDirty()
{
    m_templateTargetTextDirty = false;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::clearTemplateImageThresholdDirty()
{
    m_templateImageThresholdDirty = false;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::clearTemplatePrivateSettingDirty()
{
    m_templateTargetTextDirty = false;
    m_templateImageThresholdDirty = false;
    updateTemplatePrivateSettingDirtyUi();
}

void Widget::updateTemplatePrivateSettingDirtyUi()
{
    if (ui->label) {
        ui->label->setText(m_templateTargetTextDirty
                           ? m_templateTargetLabelText + " *"
                           : m_templateTargetLabelText);
    }
    if (ui->label_4) {
        ui->label_4->setText(m_templateImageThresholdDirty
                             ? m_templateThresholdLabelText + " *"
                             : m_templateThresholdLabelText);
    }
}

void Widget::showManualCharacterTemplateCropDialog()
{
    if (!ui) {
        return;
    }
    if (currentDetectModeId() == QStringLiteral("stamp_detection")) {
        showStampCharacterTemplateCropDialog();
        return;
    }
    if (!isWordFamilyMode(currentDetectModeId())) {
        showParameterInfoAsError("提示", "手动切割字符模板只用于钢印或字库类检测模式。");
        return;
    }

    const int profileIndex = currentWordTemplateProfileIndex();
    if (profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        showParameterInfoAsError("提示", "请先选择当前编辑的产品模板。");
        return;
    }
    const WordTemplateProfile &selectedProfile =
            m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
    if (!selectedProfile.resolvedAssetPathsByRole.isEmpty()) {
        showPublishedRecipeCharacterTemplateCropDialog(profileIndex);
        return;
    }

    const QString templateDirPath = selectedProfile.dirPath;

    if (templateDirPath.trimmed().isEmpty() || !QDir(templateDirPath).exists()) {
        showParameterInfoAsError("提示", "请先选择产品模板文件夹。");
        return;
    }

    const QString rawImagePath = QDir(templateDirPath).filePath("template_raw.png");
    QImage rawImage(rawImagePath);
    if (rawImage.isNull()) {
        showParameterCritical("严重警告", "当前产品模板缺少 template_raw.png，无法手动切割字符模板。");
        return;
    }

    const TemplatePrivateSettings &privateSettings =
            m_wordTemplateProfiles[static_cast<size_t>(profileIndex)].settings;
    cv::Rect2d trackingBox = privateSettings.trackingBox;
    bool trackingBoxValid = trackingBox.width > 0 && trackingBox.height > 0;
    if (!trackingBoxValid) {
        showParameterCritical("严重警告", "当前产品模板缺少有效定位区域，无法还原喷码检测区域。");
        return;
    }

    const QString yamlPath = QDir(templateDirPath).filePath("calibrate_config.yaml");
    CalibrationData calib;
    if (!QFile::exists(yamlPath)
            || !calib.load(yamlPath.toLocal8Bit().toStdString())
            || calib.date_poly.empty()) {
        showParameterCritical("严重警告", "当前产品模板缺少有效喷码检测区域，无法手动切割字符模板。");
        return;
    }

    QPolygonF datePolygon;
    const QPointF trackingCenter(trackingBox.x + trackingBox.width / 2.0,
                                 trackingBox.y + trackingBox.height / 2.0);
    for (const cv::Point2f &point : calib.date_poly) {
        datePolygon << QPointF(trackingCenter.x() + point.x,
                               trackingCenter.y() + point.y);
    }

    QRect cropRect = datePolygon.boundingRect().toAlignedRect()
            .intersected(QRect(0, 0, rawImage.width(), rawImage.height()));
    if (cropRect.width() <= 0 || cropRect.height() <= 0) {
        showParameterCritical("严重警告", "喷码检测区域超出模板图像范围，无法手动切割字符模板。");
        return;
    }

    CharacterTemplateCropDialog dialog(rawImage.copy(cropRect), templateDirPath, this);
    if (dialog.exec() != QDialog::Accepted || dialog.savedCount() <= 0) {
        return;
    }

    TemplatePrivateSettings refreshedSettings;
    QString refreshedSettingsError;
    if (AppSettingsManager::loadTemplatePrivateSettings(templateDirPath,
                                                        &refreshedSettings,
                                                        &refreshedSettingsError)) {
        WordTemplateProfile &profile =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        profile.settings = refreshedSettings;
        refreshWordTemplateRecipeProfile(&profile);
        refreshWordTemplateRecipeAssets();
    } else {
        showParameterCritical("严重警告",
                              QString("字符模板图片已生成，但字符框配置重新读取失败：\n%1")
                              .arg(refreshedSettingsError));
        return;
    }

    const QString targetText = ui->dateEdit->toPlainText();
    QString reloadMessage;
    if (!targetText.trimmed().isEmpty()) {
        const QStringList baseNames = parseTemplateTargetUnits(targetText);
        std::vector<cv::Mat> reloadedTemplates;
        std::vector<int> reloadedTemplateTargetIndexes;
        QString loadError;
        if (loadWordDigitTemplatesFromDir(templateDirPath,
                                          baseNames,
                                          &reloadedTemplates,
                                          &reloadedTemplateTargetIndexes,
                                          &loadError,
                                          true)) {
            if (profileIndex >= 0
                    && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
                WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
                profile.digitTemplates = reloadedTemplates;
                profile.digitTemplateTargetIndexes = reloadedTemplateTargetIndexes;
                refreshWordTemplateProfileDigitCache(
                            &profile);
            }
        } else {
            reloadMessage = QString("\n\n字符模板已保存，但当前目标字符仍有图片未加载成功：\n%1").arg(loadError);
        }
    }

    QString recipePublishMessage;
    if (reloadMessage.isEmpty() && m_wordTemplateRecipeDraftSession.isActive()) {
        QString publishError;
        if (!publishWordTemplateRecipeDraft(&publishError)) {
            recipePublishMessage =
                    QString("\n\n字符模板已保存，但新产品配方同步失败：\n%1")
                    .arg(publishError);
        }
    }

    showParameterInfo("提示",
                      QString("已保存 %1 张字符模板图片。%2%3")
                      .arg(dialog.savedCount())
                      .arg(reloadMessage)
                      .arg(recipePublishMessage));
}

void Widget::showStampCharacterTemplateCropDialog()
{
    if (!ui
            || currentDetectModeId()
               != QStringLiteral("stamp_detection")) {
        return;
    }
    if ((myThread && myThread->isRunning())
            || (cameraThread && cameraThread->isRunning())
            || isCollecting) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u8BF7\u5148\u505C\u6B62\u68C0\u6D4B\u540E\u518D\u5207\u5272\u94A2\u5370\u5B57\u7B26\u6A21\u677F\u3002"));
        return;
    }

    const bool isPublishedRecipe =
            m_singleTemplateRecipeEditSession.isActive();
    TemplatePrivateSettings privateSettings;
    QString rawImagePath;
    QString calibrationPath;
    QString templateDirPath;
    QString settingsError;

    if (isPublishedRecipe) {
        if (m_singleTemplateRecipeEditSession.recipe().detectionMode
                != DetectionMode::Stamp
                || m_singleTemplateRecipeEditSession.recipe().profiles.size()
                   != 1
                || m_singleTemplateResolvedAssetPathsByRole.isEmpty()) {
            showParameterInfoAsError(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u5F53\u524D\u5DF2\u53D1\u5E03\u94A2\u5370\u914D\u65B9\u6CA1\u6709\u6709\u6548\u7684\u7F16\u8F91\u4F1A\u8BDD\u3002"));
            return;
        }
        privateSettings = templatePrivateSettingsFromRecipeProfile(
                    m_singleTemplateRecipeEditSession.recipe()
                    .profiles.first());
        rawImagePath = m_singleTemplateResolvedAssetPathsByRole
                .value(QStringLiteral("rawImage"));
        calibrationPath = m_singleTemplateResolvedAssetPathsByRole
                .value(QStringLiteral("calibration"));
    } else {
        templateDirPath = currentTemplateDirPath.trimmed();
        if (templateDirPath.isEmpty()
                || !QDir(templateDirPath).exists()) {
            showParameterInfoAsError(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u8BF7\u5148\u9009\u62E9\u6216\u4FDD\u5B58\u94A2\u5370\u4EA7\u54C1\u6A21\u677F\u3002"));
            return;
        }
        if (!AppSettingsManager::loadTemplatePrivateSettings(
                    templateDirPath,
                    &privateSettings,
                    &settingsError)) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral("\u5F53\u524D\u94A2\u5370\u6A21\u677F\u53C2\u6570\u65E0\u6CD5\u8BFB\u53D6\uFF1A\n%1")
                        .arg(settingsError));
            return;
        }
        rawImagePath = QDir(templateDirPath).filePath(
                    QStringLiteral("template_raw.png"));
        calibrationPath = QDir(templateDirPath).filePath(
                    QStringLiteral("calibrate_config.yaml"));
    }

    QImage rawImage(rawImagePath);
    if (rawImage.isNull()) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5F53\u524D\u94A2\u5370\u6A21\u677F\u7F3A\u5C11\u53EF\u8BFB\u7684 template_raw.png\uFF0C\u65E0\u6CD5\u5207\u5272\u5B57\u7B26\u6A21\u677F\u3002"));
        return;
    }

    CalibrationData calibration;
    const cv::Rect2d trackingBox = privateSettings.trackingBox;
    if (trackingBox.width <= 0.0
            || trackingBox.height <= 0.0
            || calibrationPath.trimmed().isEmpty()
            || !calibration.load(
                calibrationPath.toLocal8Bit().toStdString())
            || calibration.date_poly.size() < 3) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5F53\u524D\u94A2\u5370\u6A21\u677F\u7F3A\u5C11\u6709\u6548\u5B9A\u4F4D\u533A\u57DF\u6216\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\uFF0C\u65E0\u6CD5\u5207\u5272\u5B57\u7B26\u6A21\u677F\u3002"));
        return;
    }

    QPolygonF datePolygon;
    const QPointF trackingCenter(trackingBox.x + trackingBox.width / 2.0,
                                 trackingBox.y + trackingBox.height / 2.0);
    for (const cv::Point2f &point : calibration.date_poly) {
        datePolygon << QPointF(trackingCenter.x() + point.x,
                               trackingCenter.y() + point.y);
    }
    const QRect cropRect = datePolygon.boundingRect().toAlignedRect()
            .intersected(QRect(0, 0,
                               rawImage.width(),
                               rawImage.height()));
    if (cropRect.width() <= 0 || cropRect.height() <= 0) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\u8D85\u51FA\u94A2\u5370\u6A21\u677F\u539F\u56FE\u8303\u56F4\u3002"));
        return;
    }

    if (!isPublishedRecipe) {
        CharacterTemplateCropDialog dialog(rawImage.copy(cropRect),
                                           templateDirPath,
                                           this);
        if (dialog.exec() != QDialog::Accepted
                || dialog.savedCount() <= 0) {
            return;
        }

        TemplatePrivateSettings refreshedSettings;
        if (!AppSettingsManager::loadTemplatePrivateSettings(
                    templateDirPath,
                    &refreshedSettings,
                    &settingsError)) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral("\u5B57\u7B26\u6A21\u677F\u56FE\u7247\u5DF2\u751F\u6210\uFF0C\u4F46\u5B57\u7B26\u6846\u914D\u7F6E\u91CD\u65B0\u8BFB\u53D6\u5931\u8D25\uFF1A\n%1")
                        .arg(settingsError));
            return;
        }

        std::vector<cv::Mat> reloadedTemplates;
        std::vector<int> reloadedTemplateTargetIndexes;
        QString reloadMessage;
        const QStringList targetUnits = parseTemplateTargetUnits(
                    refreshedSettings.targetText);
        if (targetUnits.isEmpty()) {
            reloadMessage = QStringLiteral(
                        "\n\n\u5B57\u7B26\u6A21\u677F\u5DF2\u4FDD\u5B58\uFF0C\u8BF7\u586B\u5199\u76EE\u6807\u5B57\u7B26\u5E76\u70B9\u51FB\u201C\u786E\u8BA4\u5B57\u7B26\u201D\u540E\u751F\u6548\u3002");
        } else if (loadWordDigitTemplatesFromDir(
                       templateDirPath,
                       targetUnits,
                       &reloadedTemplates,
                       &reloadedTemplateTargetIndexes,
                       &settingsError,
                       false)) {
            digitTemplates.swap(reloadedTemplates);
            digitTemplateTargetIndexes.swap(
                        reloadedTemplateTargetIndexes);
        } else {
            reloadMessage = QStringLiteral(
                        "\n\n\u5B57\u7B26\u6A21\u677F\u5DF2\u4FDD\u5B58\uFF0C\u4F46\u5F53\u524D\u76EE\u6807\u5B57\u7B26\u6240\u9700\u56FE\u7247\u672A\u5168\u90E8\u52A0\u8F7D\uFF1A\n%1")
                    .arg(settingsError);
        }

        showParameterInfo(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u5DF2\u4E3A\u5F53\u524D\u94A2\u5370\u6A21\u677F\u4FDD\u5B58 %1 \u5F20\u5B57\u7B26\u6A21\u677F\u56FE\u7247\u3002%2")
                    .arg(dialog.savedCount())
                    .arg(reloadMessage));
        return;
    }

    TemplateCharacterAssetWorkspace workspace;
    if (!workspace.prepare(m_singleTemplateResolvedAssetPathsByRole,
                           &settingsError)
            || !AppSettingsManager::saveTemplatePrivateSettings(
                workspace.directoryPath(),
                privateSettings,
                &settingsError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u65E0\u6CD5\u51C6\u5907\u94A2\u5370\u5B57\u7B26\u6A21\u677F\u4E34\u65F6\u7F16\u8F91\u533A\uFF1A\n%1")
                    .arg(settingsError));
        return;
    }

    CharacterTemplateCropDialog dialog(rawImage.copy(cropRect),
                                       workspace.directoryPath(),
                                       this);
    if (dialog.exec() != QDialog::Accepted
            || dialog.savedCount() <= 0) {
        return;
    }

    TemplatePrivateSettings updatedSettings;
    if (!AppSettingsManager::loadTemplatePrivateSettings(
                workspace.directoryPath(),
                &updatedSettings,
                &settingsError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5B57\u7B26\u56FE\u7247\u5DF2\u5728\u4E34\u65F6\u533A\u751F\u6210\uFF0C\u4F46\u5B57\u7B26\u6846\u6570\u636E\u65E0\u6CD5\u8BFB\u53D6\uFF1A\n%1")
                    .arg(settingsError));
        return;
    }

    std::vector<cv::Mat> candidateTemplates;
    std::vector<int> candidateTemplateTargetIndexes;
    const QStringList targetUnits = parseTemplateTargetUnits(
                updatedSettings.targetText);
    if (targetUnits.isEmpty()
            || !loadWordDigitTemplatesFromDir(
                workspace.directoryPath(),
                targetUnits,
                &candidateTemplates,
                &candidateTemplateTargetIndexes,
                &settingsError,
                false)) {
        showParameterInfoWithRedWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u5B57\u7B26\u6A21\u677F\u4FEE\u6539\u672A\u5199\u5165\u6B63\u5F0F\u94A2\u5370\u914D\u65B9\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                    targetUnits.isEmpty()
                    ? QStringLiteral("\u8BF7\u5148\u786E\u8BA4\u94A2\u5370\u76EE\u6807\u5B57\u7B26\u3002")
                    : QStringLiteral("\u5F53\u524D\u76EE\u6807\u5B57\u7B26\u5BF9\u5E94\u56FE\u7247\u4E0D\u5B8C\u6574\uFF1A\n%1")
                      .arg(settingsError));
        return;
    }

    const TemplateProfileAssetManifest updatedManifest =
            workspace.assetManifest(0);
    const RecipeProfile &currentProfile =
            m_singleTemplateRecipeEditSession.recipe().profiles.first();
    const RecipeProfile updatedProfile =
            recipeProfileFromTemplatePrivateSettings(
                currentProfile.name,
                updatedSettings,
                updatedManifest.profileAssetKeys);
    TemplateRecipeEditSession candidateSession =
            m_singleTemplateRecipeEditSession;
    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection publishedSelection;
    TemplateRecipeWorkflowFailureStage failureStage =
            TemplateRecipeWorkflowFailureStage::None;
    if (!TemplateRecipeWorkflow::republishProfileAssets(
                &candidateSession,
                store,
                0,
                updatedProfile,
                updatedManifest,
                &publishedSelection,
                &failureStage,
                &settingsError)) {
        const QString failureDetail =
                failureStage
                == TemplateRecipeWorkflowFailureStage::Validation
                ? QStringLiteral("\u914D\u65B9\u8D44\u4EA7\u66F4\u65B0\u6821\u9A8C\u5931\u8D25\uFF1A\n%1")
                  .arg(settingsError)
                : QStringLiteral("\u94A2\u5370\u4EA7\u54C1\u914D\u65B9\u91CD\u65B0\u53D1\u5E03\u5931\u8D25\uFF1A\n%1")
                  .arg(settingsError);
        showParameterInfoWithRedWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u5B57\u7B26\u6A21\u677F\u4FEE\u6539\u672A\u5199\u5165\u6B63\u5F0F\u94A2\u5370\u914D\u65B9\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                    failureDetail);
        return;
    }

    if (!activatePublishedSingleTemplateRecipe(
                publishedSelection.recipe->recipeId,
                QStringLiteral("stamp_detection"),
                false,
                &settingsError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u94A2\u5370\u4EA7\u54C1\u914D\u65B9\u5DF2\u91CD\u65B0\u53D1\u5E03\uFF0C\u4F46\u5F53\u524D\u7F13\u5B58\u5237\u65B0\u5931\u8D25\uFF1B\u8BF7\u91CD\u65B0\u9009\u62E9\u8BE5\u914D\u65B9\uFF1A\n%1")
                    .arg(settingsError));
        return;
    }

    saveSettings(false);
    qDebug() << "[RECIPE_PUBLISH] republished stamp recipe assets:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath
             << "characters:" << dialog.savedCount();
    showParameterInfo(
                QStringLiteral("\u63D0\u793A"),
                QStringLiteral("\u5DF2\u4E3A\u5F53\u524D\u94A2\u5370\u914D\u65B9\u4FDD\u5B58 %1 \u5F20\u5B57\u7B26\u6A21\u677F\u56FE\u7247\uFF0C\u5E76\u4F7F\u7528\u539F\u914D\u65B9\u7F16\u53F7\u91CD\u65B0\u53D1\u5E03\u3002")
                .arg(dialog.savedCount()));
}

void Widget::showPublishedRecipeCharacterTemplateCropDialog(
        int profileIndex)
{
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())
            || !m_wordTemplateRecipeEditSession.isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u5F53\u524D\u5DF2\u53D1\u5E03\u914D\u65B9\u6CA1\u6709\u6709\u6548\u7684\u7F16\u8F91\u4F1A\u8BDD\u3002"));
        return;
    }

    const WordTemplateProfile profile =
            m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
    const QString rawImagePath = wordTemplateProfileAssetPath(
                profile,
                QStringLiteral("rawImage"),
                QStringLiteral("template_raw.png"));
    QImage rawImage(rawImagePath);
    if (rawImage.isNull()) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5F53\u524D\u4EA7\u54C1\u914D\u65B9\u7F3A\u5C11\u53EF\u8BFB\u7684\u539F\u56FE\uFF0C\u65E0\u6CD5\u5207\u5272\u5B57\u7B26\u6A21\u677F\u3002"));
        return;
    }

    const cv::Rect2d trackingBox = profile.settings.trackingBox;
    if (trackingBox.width <= 0.0
            || trackingBox.height <= 0.0
            || profile.datePoly.empty()) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5F53\u524D\u4EA7\u54C1\u914D\u65B9\u7F3A\u5C11\u6709\u6548\u5B9A\u4F4D\u533A\u57DF\u6216\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\u3002"));
        return;
    }

    QPolygonF datePolygon;
    const QPointF trackingCenter(trackingBox.x + trackingBox.width / 2.0,
                                 trackingBox.y + trackingBox.height / 2.0);
    for (const cv::Point2f &point : profile.datePoly) {
        datePolygon << QPointF(trackingCenter.x() + point.x,
                               trackingCenter.y() + point.y);
    }
    const QRect cropRect = datePolygon.boundingRect().toAlignedRect()
            .intersected(QRect(0, 0, rawImage.width(), rawImage.height()));
    if (cropRect.width() <= 0 || cropRect.height() <= 0) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\u8D85\u51FA\u4EA7\u54C1\u914D\u65B9\u539F\u56FE\u8303\u56F4\u3002"));
        return;
    }

    QString workspaceError;
    TemplateCharacterAssetWorkspace workspace;
    if (!workspace.prepare(profile.resolvedAssetPathsByRole,
                           &workspaceError)) {
        showParameterCritical(QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                              QStringLiteral("\u65E0\u6CD5\u51C6\u5907\u5B57\u7B26\u6A21\u677F\u4E34\u65F6\u7F16\u8F91\u533A\uFF1A\n%1")
                              .arg(workspaceError));
        return;
    }

    if (!AppSettingsManager::saveTemplatePrivateSettings(
                workspace.directoryPath(),
                profile.settings,
                &workspaceError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u65E0\u6CD5\u51C6\u5907\u5B57\u7B26\u6846\u7F16\u8F91\u6570\u636E\uFF1A\n%1")
                    .arg(workspaceError));
        return;
    }

    CharacterTemplateCropDialog dialog(rawImage.copy(cropRect),
                                       workspace.directoryPath(),
                                       this);
    if (dialog.exec() != QDialog::Accepted || dialog.savedCount() <= 0) {
        return;
    }

    TemplatePrivateSettings updatedSettings;
    if (!AppSettingsManager::loadTemplatePrivateSettings(
                workspace.directoryPath(),
                &updatedSettings,
                &workspaceError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u5B57\u7B26\u56FE\u7247\u5DF2\u5728\u4E34\u65F6\u533A\u751F\u6210\uFF0C\u4F46\u5B57\u7B26\u6846\u6570\u636E\u65E0\u6CD5\u8BFB\u53D6\uFF1A\n%1")
                    .arg(workspaceError));
        return;
    }

    const TemplateProfileAssetManifest updatedManifest =
            workspace.assetManifest(profileIndex);
    const RecipeProfile updatedProfile =
            recipeProfileFromTemplatePrivateSettings(
                profile.name,
                updatedSettings,
                updatedManifest.profileAssetKeys);

    const QStringList targetUnits = parseTemplateTargetUnits(
                updatedSettings.targetText);
    if (!updatedSettings.targetText.trimmed().isEmpty()) {
        std::vector<cv::Mat> templates;
        std::vector<int> templateTargetIndexes;
        if (!loadWordDigitTemplatesFromDir(workspace.directoryPath(),
                                           targetUnits,
                                           &templates,
                                           &templateTargetIndexes,
                                           &workspaceError,
                                           true)) {
            showParameterInfoWithRedWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u5B57\u7B26\u6A21\u677F\u4FEE\u6539\u672A\u5199\u5165\u6B63\u5F0F\u4EA7\u54C1\u914D\u65B9\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                        QStringLiteral("\u5F53\u524D\u76EE\u6807\u5B57\u7B26\u5BF9\u5E94\u56FE\u7247\u4E0D\u5B8C\u6574\uFF1A\n%1")
                        .arg(workspaceError));
            return;
        }
    }

    const RecipeStore store(QDir(AppSettingsManager::globalDataDirPath())
                            .filePath(QStringLiteral("recipes")));
    TemplateRecipeEditSession candidateSession =
            m_wordTemplateRecipeEditSession;
    RecipeSelection publishedSelection;
    TemplateRecipeWorkflowFailureStage failureStage =
            TemplateRecipeWorkflowFailureStage::None;
    if (!TemplateRecipeWorkflow::republishProfileAssets(
                &candidateSession,
                store,
                profileIndex,
                updatedProfile,
                updatedManifest,
                &publishedSelection,
                &failureStage,
                &workspaceError)) {
        const QString failureDetail =
                failureStage
                == TemplateRecipeWorkflowFailureStage::Validation
                ? QStringLiteral("\u914D\u65B9\u8D44\u4EA7\u66F4\u65B0\u6821\u9A8C\u5931\u8D25\uFF1A\n%1")
                  .arg(workspaceError)
                : QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u91CD\u65B0\u53D1\u5E03\u5931\u8D25\uFF1A\n%1")
                  .arg(workspaceError);
        showParameterInfoWithRedWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u5B57\u7B26\u6A21\u677F\u4FEE\u6539\u672A\u5199\u5165\u6B63\u5F0F\u4EA7\u54C1\u914D\u65B9\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                    failureDetail);
        return;
    }

    QStringList pendingMessages;
    if (!activatePublishedWordRecipe(
                publishedSelection.recipe->recipeId,
                currentDetectModeId(),
                false,
                &pendingMessages,
                &workspaceError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u5DF2\u91CD\u65B0\u53D1\u5E03\uFF0C\u4F46\u5F53\u524D\u7F13\u5B58\u5237\u65B0\u5931\u8D25\uFF1B\u8BF7\u91CD\u65B0\u9009\u62E9\u8BE5\u914D\u65B9\uFF1A\n%1")
                    .arg(workspaceError));
        return;
    }
    setCurrentWordTemplateEditIndex(profileIndex);
    saveSettings(false);

    qDebug() << "[RECIPE_PUBLISH] republished word recipe assets:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath
             << "profile:" << profileIndex
             << "characters:" << dialog.savedCount();
    QString message = QStringLiteral("\u5DF2\u4E3AProfile [%1] \u4FDD\u5B58 %2 \u5F20\u5B57\u7B26\u6A21\u677F\u56FE\u7247\uFF0C\u5E76\u4F7F\u7528\u539F\u914D\u65B9\u7F16\u53F7\u91CD\u65B0\u53D1\u5E03\u3002")
            .arg(profile.name)
            .arg(dialog.savedCount());
    if (!pendingMessages.isEmpty()) {
        message += QStringLiteral("\n\n\u76EE\u6807\u5B57\u7B26\u5F85\u786E\u8BA4\uFF1A\n%1")
                .arg(pendingMessages.join(QLatin1Char('\n')));
        showParameterWarning(QStringLiteral("\u63D0\u793A"), message);
    } else {
        showParameterInfo(QStringLiteral("\u63D0\u793A"), message);
    }
}

void Widget::setupWordTemplateEditorCombo()
{
    const QString commonPushButtonStyle =
            "QPushButton {"
            "background-color: transparent;"
            "border: 1px solid #ebeef5;"
            "border-radius: 4px;"
            "color: #333333;"
            "padding: 5px 10px;"
            "}"
            "QPushButton:hover {"
            "background-color: #f2f6fc;"
            "}"
            "QPushButton:pressed {"
            "background-color: #ebeef5;"
            "}"
            "QPushButton:disabled {"
            "background-color: #f2f3f5;"
            "color: #a8abb2;"
            "border-color: #dcdfe6;"
            "}";

    if (ui) {

        if (ui->textsure_btn) ui->textsure_btn->setStyleSheet(commonPushButtonStyle);
        if (ui->batchTextsure_btn) ui->batchTextsure_btn->setStyleSheet(commonPushButtonStyle);
        if (ui->batchImageThresholdButton) ui->batchImageThresholdButton->setStyleSheet(commonPushButtonStyle);
        if (ui->WriteVDpushButton) ui->WriteVDpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_7) ui->pushButton_7->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_3) ui->pushButton_3->setStyleSheet(commonPushButtonStyle);
        if (ui->sureButton) ui->sureButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_9) ui->pushButton_9->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_12) ui->pushButton_12->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_tissueRoughnessThreshold) ui->pushButton_tissueRoughnessThreshold->setStyleSheet(commonPushButtonStyle);
        if (ui->plcmodebtn) ui->plcmodebtn->setStyleSheet(commonPushButtonStyle);
        if (ui->ConnectpushButton) ui->ConnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->DisconnectpushButton) ui->DisconnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_8) ui->pushButton_8->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_browseImageSavePath) ui->pushButton_browseImageSavePath->setStyleSheet(commonPushButtonStyle);
    }

    if (ui && ui->textsure_btn && ui->batchTextsure_btn) {
        ui->textsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        ui->batchTextsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QHBoxLayout *buttonLayout = qobject_cast<QHBoxLayout *>(ui->textsure_btn->parentWidget()
                ? ui->textsure_btn->parentWidget()->layout()
                : nullptr);
        if (buttonLayout) {
            buttonLayout->setStretch(0, 1);
            buttonLayout->setStretch(1, 1);
        }

        ui->textsure_btn->setToolTip("只保存当前编辑模板的目标字符，并重新加载该模板的字符图片。");
        ui->batchTextsure_btn->setToolTip("把当前目标字符保存到所有已选择的字库模板，并分别重新加载字符图片。");
        ui->textsure_btn->installEventFilter(this);
        ui->batchTextsure_btn->installEventFilter(this);
    }

    if (ui && ui->pushButton_browseImageSavePath) {
        ui->pushButton_browseImageSavePath->installEventFilter(this);
    }

    if (ui && ui->pushButton_7) {
        ui->pushButton_7->setToolTip("确认当前选择的颜色通道，用于后续图像处理和识别。");
        ui->pushButton_7->installEventFilter(this);
    }

    if (ui) {
        if (ui->checkBox) {
            ui->checkBox->setToolTip(
                        "控制检测的触发方式。\n"
                        "勾选：使用 PLC 外部触发信号控制相机拍照和检测，启动前必须连接 PLC。\n"
                        "不勾选：使用软件软触发，启动后由相机连续采集并检测。");
            ui->checkBox->installEventFilter(this);
        }
        if (ui->pushButton_10) {
            ui->pushButton_10->setToolTip("清空当前尚未发出的剔除队列。\n适用于异常停机、误判、手动停止后，防止之前累计的剔除信号继续输出。");
            ui->pushButton_10->installEventFilter(this);
        }
        if (ui->label_4) {
            ui->label_4->setToolTip("图像判定合格的分数阈值。\n识别匹配分数低于该值时，通常判为不合格；数值越高，判定越严格。");
            ui->label_4->installEventFilter(this);
        }
        if (ui->label_27) {
            ui->label_27->setToolTip("设置图像进入识别前的旋转方向。\n当相机安装方向、产品摆放方向和模板方向不一致时，需要调整这里。");
            ui->label_27->installEventFilter(this);
        }
        if (ui->label_16) {
            ui->label_16->setToolTip("设置相机增益。\n增益越高画面越亮，但噪声也可能增加；一般先调曝光，曝光不足时再调增益。");
            ui->label_16->installEventFilter(this);
        }
        if (ui->label_14) {
            ui->label_14->setToolTip("PLC拍照信号保持多久。\n相机偶尔漏拍、触发不稳定时可适当加大；正常不要过大，避免影响下一次触发节拍。");
            ui->label_14->installEventFilter(this);
        }
        if (ui->label_13) {
            ui->label_13->setToolTip("相机收到 PLC 拍照信号后，再等待多久才真正曝光采图。\n通常在拍照距离基本正确后，用它做小范围微调。\n画面中产品还没到合适位置就加大；产品已经走过或喷码偏后就减小。");
            ui->label_13->installEventFilter(this);
        }
        if (ui->label_6) {
            ui->label_6->setToolTip("检测拍照点到剔除机构中心的实际产线距离。\n剔除太早通常加大；剔除太晚通常减小。");
            ui->label_6->installEventFilter(this);
        }
        if (ui->label_10) {
            ui->label_10->setToolTip("剔除机构保持动作的时长。\n不合格品剔不干净就加大；影响相邻合格品或动作拖尾就减小。");
            ui->label_10->installEventFilter(this);
        }
        if (ui->label_17) {
            ui->label_17->setToolTip("选择第几路剔除输出或第几个剔除口。\n现场有多个气嘴、推杆或剔除工位时使用；填错会从错误位置剔除。");
            ui->label_17->installEventFilter(this);
        }
        if (ui->label_8) {
            ui->label_8->setToolTip("上游传感器触发点到相机拍照中心的实际产线距离。\nPLC 根据这个距离判断产品走到相机位置后再发出拍照信号。\n画面中产品还没到拍照位置，说明触发偏早，适当加大；产品已经走过拍照位置，说明触发偏晚，适当减小。");
            ui->label_8->installEventFilter(this);
        }
        if (ui->comboBox_3) {
            ui->comboBox_3->setToolTip("PLC触发工作模式。\n连续触发模式：产线连续经过时，PLC按连续节拍触发相机采图和检测。\n间歇触发模式：产品分批、停顿或按间隔到位时，PLC按间歇方式触发采图和检测。");
            ui->comboBox_3->installEventFilter(this);
        }
    }

    if (ui && ui->VideoShoot) {
        ui->VideoShoot->installEventFilter(this);
    }

    if (ui && ui->batchTextsure_btn) {
        ui->batchTextsure_btn->hide();
    }
    if (ui && ui->batchImageThresholdButton) {
        ui->batchImageThresholdButton->setToolTip(
                    "把当前图像合格阈值保存到所有已选择的产品模板。");
        ui->batchImageThresholdButton->installEventFilter(this);
        ui->batchImageThresholdButton->hide();
    }

    if (m_wordTemplateEditComboBox || !ui || !ui->dateEdit || !ui->lineEdit_yuzhi) {
        return;
    }

    QWidget *parentWidget = ui->dateEdit->parentWidget();
    if (parentWidget) {
        QGridLayout *targetLayout = qobject_cast<QGridLayout *>(parentWidget->layout());

        m_wordTemplateEditWidget = new QWidget(parentWidget);
        m_wordTemplateEditWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditWidget->setFixedHeight(50);
        QHBoxLayout *editorLayout = new QHBoxLayout(m_wordTemplateEditWidget);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(6);

        const QString editorBoxStyle =
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #333333;"
                "padding: 5px 10px;";

        m_wordTemplateEditLabel = new QLabel("当前产品模板:", m_wordTemplateEditWidget);
        m_wordTemplateEditLabel->setFixedHeight(50);
        m_wordTemplateEditLabel->setStyleSheet(editorBoxStyle);

        m_wordTemplateEditComboBox = new QComboBox(m_wordTemplateEditWidget);
        m_wordTemplateEditComboBox->setObjectName("wordTemplateComboBox");
        m_wordTemplateEditComboBox->setMinimumHeight(50);
        m_wordTemplateEditComboBox->setMaximumHeight(50);
        m_wordTemplateEditComboBox->setMinimumWidth(160);
        m_wordTemplateEditComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditComboBox->setStyleSheet(
                    "QComboBox {"
                    "background-color: transparent;"
                    "border: 1px solid #ebeef5;"
                    "border-radius: 4px;"
                    "color: #333333;"
                    "padding: 5px 10px;"
                    "}"
                    "QComboBox:disabled {"
                    "color: #a8abb2;"
                    "}");

        m_publishTemplateGroupButton = new QPushButton(
                    QStringLiteral("\u53D1\u5E03\u6A21\u677F\u7EC4"),
                    m_wordTemplateEditWidget);
        m_publishTemplateGroupButton->setObjectName(
                    QStringLiteral("publishTemplateGroupButton"));
        m_publishTemplateGroupButton->setFixedHeight(50);
        m_publishTemplateGroupButton->setToolTip(
                    QStringLiteral(
                        "\u628A\u5F53\u524D\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u7684\u591A\u4E2AProfile\uFF0C\u6309\u5F53\u524D\u987A\u5E8F"
                        "\u53D1\u5E03\u4E3A\u4E00\u4E2A\u4EA7\u54C1\u914D\u65B9\u3002"));
        m_publishTemplateGroupButton->setStyleSheet(commonPushButtonStyle);

        m_publishedRecipeButton = new QPushButton(
                    QStringLiteral("\u5DF2\u53D1\u5E03\u914D\u65B9"),
                    m_wordTemplateEditWidget);
        m_publishedRecipeButton->setObjectName(
                    QStringLiteral("publishedRecipeButton"));
        m_publishedRecipeButton->setFixedHeight(50);
        m_publishedRecipeButton->setToolTip(
                    QStringLiteral(
                        "\u4ECE\u8F6F\u4EF6\u914D\u65B9\u5E93\u4E2D\u9009\u62E9"
                        "\u5F53\u524D\u6A21\u5F0F\u5DF2\u7ECF\u53D1\u5E03\u7684"
                        "\u4EA7\u54C1\u914D\u65B9\u3002"));
        m_publishedRecipeButton->setStyleSheet(commonPushButtonStyle);

        editorLayout->addWidget(m_wordTemplateEditLabel);
        editorLayout->addWidget(m_wordTemplateEditComboBox, 1);
        editorLayout->addWidget(m_publishTemplateGroupButton);
        editorLayout->addWidget(m_publishedRecipeButton);
        if (targetLayout) {
            targetLayout->addWidget(m_wordTemplateEditWidget, 0, 0, 1, 3);
        }

        connect(m_wordTemplateEditComboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int index) {
                    applyWordTemplateEditorSelection(index);
                });
        connect(m_publishTemplateGroupButton,
                &QPushButton::clicked,
                this,
                [this]() {
                    if (isWordFamilyMode(currentDetectModeId())) {
                        publishCurrentWordTemplateGroup();
                    } else {
                        publishCurrentSingleTemplateRecipe();
                    }
                });
        connect(m_publishedRecipeButton,
                &QPushButton::clicked,
                this,
                &Widget::selectPublishedRecipe);

        m_wordTemplateEditComboBox->hide();
        m_wordTemplateEditWidget->hide();
    }

    updateTissueRoughnessUiVisibility();
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
    clearBarcodeTemplateValidation();
    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateRecipeEditSession.reset();
    m_currentTemplateDisplayName.clear();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    currentTemplateDirPath.clear();
    m_loadedTrackingTemplate.release();
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    savedTrackingBox = cv::Rect2d();
    hasValidBoxes = false;
    if (ui) {
        QSignalBlocker targetBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->clear();
        ui->lineEdit_yuzhi->setText("70");
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearTemplatePrivateSettingDirty();
}

void Widget::clearSingleTemplateRecipeState()
{
    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();
    currentTemplateDirPath.clear();
    m_loadedTrackingTemplate.release();
    digitTemplates.clear();
    digitTemplateTargetIndexes.clear();
    savedBarcodePoly.clear();
    savedDatePoly.clear();
    savedTrackingBox = cv::Rect2d();
    hasValidBoxes = false;
    if (ui) {
        QSignalBlocker targetBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->clear();
        ui->lineEdit_yuzhi->setText(QStringLiteral("70"));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearTemplatePrivateSettingDirty();
}

QString Widget::detectModeIdForIndex(int index) const
{
    static const QStringList detectModeIds = {
        "stamp_detection",
        "word_detection",
        "ocr_detection",
        "tissue_detection",
        BarcodeWordDetectionMode
    };
    return (index >= 0 && index < detectModeIds.size())
            ? detectModeIds.at(index)
            : QString("word_detection");
}

QString Widget::currentDetectModeId() const
{
    return detectModeIdForIndex(ui ? ui->comboBox_4->currentIndex() : 1);
}

QStringList Widget::currentTemplatePathsForMode(const QString &modeId) const
{
    QStringList paths;

    if (isWordFamilyMode(modeId)) {
        for (const WordTemplateProfile &profile : m_wordTemplateProfiles) {
            if (profile.dirPath.trimmed().isEmpty()) {
                continue;
            }
            const QString path = QDir(profile.dirPath).absolutePath();
            if (!paths.contains(path)) {
                paths.append(path);
            }
        }
        return paths;
    }

    if (!currentTemplateDirPath.trimmed().isEmpty()) {
        paths.append(QDir(currentTemplateDirPath).absolutePath());
    }
    return paths;
}

void Widget::storeCurrentTemplatePathsForMode(const QString &modeId)
{
    if (modeId.trimmed().isEmpty()) {
        return;
    }
    if (isWordFamilyMode(modeId)
            && m_wordTemplateRecipeEditSession.isActive()) {
        const QString recipeId =
                m_wordTemplateRecipeEditSession.recipe().recipeId.trimmed();
        if (!recipeId.isEmpty()) {
            m_publishedRecipeIdsByMode.insert(modeId, recipeId);
            return;
        }
    }
    if (isSingleTemplateRecipeMode(modeId)
            && m_singleTemplateRecipeEditSession.isActive()) {
        const QString recipeId =
                m_singleTemplateRecipeEditSession.recipe()
                .recipeId.trimmed();
        if (!recipeId.isEmpty()) {
            m_publishedRecipeIdsByMode.insert(modeId, recipeId);
            return;
        }
    }
    m_publishedRecipeIdsByMode.remove(modeId);
    m_templateDirPathsByMode.insert(modeId, currentTemplatePathsForMode(modeId));
}

void Widget::restoreTemplatesForMode(const QString &modeId, bool showMessage)
{
    const QString rememberedRecipeId =
            m_publishedRecipeIdsByMode.value(modeId).trimmed();
    if ((isWordFamilyMode(modeId)
         || isSingleTemplateRecipeMode(modeId))
            && !rememberedRecipeId.isEmpty()) {
        QStringList pendingMessages;
        QString restoreError;
        const bool restored = isWordFamilyMode(modeId)
                ? activatePublishedWordRecipe(rememberedRecipeId,
                                              modeId,
                                              false,
                                              &pendingMessages,
                                              &restoreError)
                : activatePublishedSingleTemplateRecipe(
                    rememberedRecipeId,
                    modeId,
                    false,
                    &restoreError);
        if (restored) {
            qDebug() << "[RECIPE_RESTORE] restored published recipe:"
                     << modeId
                     << rememberedRecipeId
                     << "profiles:"
                     << (isWordFamilyMode(modeId)
                         ? static_cast<int>(m_wordTemplateProfiles.size())
                         : 1);
            if (showMessage && !pendingMessages.isEmpty()) {
                showParameterWarning(
                            QStringLiteral("\u63D0\u793A"),
                            pendingMessages.join(QLatin1Char('\n')));
            }
            return;
        }

        qWarning() << "[RECIPE_RESTORE] remembered recipe is unavailable;"
                   << "falling back to legacy template paths:"
                   << modeId
                   << rememberedRecipeId
                   << restoreError;
        m_publishedRecipeIdsByMode.remove(modeId);
        m_appliedGlobalSettings.publishedRecipeIdsByMode.remove(modeId);
        QString settingsError;
        if (!AppSettingsManager::saveGlobalSettings(m_appliedGlobalSettings,
                                                    &settingsError)) {
            qWarning() << "[RECIPE_RESTORE] failed to remove unavailable recipe memory:"
                       << settingsError;
        }
        if (showMessage) {
            showParameterWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral(
                            "\u4E0A\u6B21\u5DF2\u53D1\u5E03\u914D\u65B9\u65E0\u6CD5\u6062\u590D\uFF0C"
                            "\u5DF2\u56DE\u9000\u5230\u539F\u6A21\u677F\u8DEF\u5F84\uFF1A\n%1")
                        .arg(restoreError));
        }
    }

    const QStringList paths = m_templateDirPathsByMode.value(modeId);
    if (paths.isEmpty()) {
        return;
    }

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    hideTemplateGuide();

    if (isWordFamilyMode(modeId)) {
        std::vector<WordTemplateProfile> loadedProfiles;
        QStringList validPaths;
        QStringList skippedMessages;
        QStringList userMessages;
        bool removedRecipeStorePath = false;
        const QString recipesRootPath = QDir::cleanPath(
                    QDir(AppSettingsManager::globalDataDirPath())
                    .filePath(QStringLiteral("recipes")));

        for (const QString &path : paths) {
            QDir templateDir(path);
            const QString parentPath = QDir::cleanPath(
                        QFileInfo(templateDir.absolutePath())
                        .dir()
                        .absolutePath());
            if (QString::compare(parentPath,
                                 recipesRootPath,
                                 Qt::CaseInsensitive) == 0) {
                removedRecipeStorePath = true;
                qDebug() << "[TEMPLATE_RESTORE] removed recipe-store path from legacy memory:"
                         << templateDir.absolutePath();
                continue;
            }
            if (!templateDir.exists()) {
                skippedMessages.append(QString("%1：产品模板文件夹不存在").arg(path));
                userMessages.append(
                            QString("模板路径 %1 不存在，已跳过读取。")
                            .arg(path));
                continue;
            }

            WordTemplateProfile profile;
            QString message;
            if (!loadWordTemplateProfileFromDir(templateDir.absolutePath(), &profile, &message)) {
                skippedMessages.append(QString("%1：%2")
                                       .arg(templateDir.dirName())
                                       .arg(message));
                userMessages.append(
                            QString("模板“%1”无法加载：%2")
                            .arg(templateDir.dirName())
                            .arg(message));
                continue;
            }
            validPaths.append(templateDir.absolutePath());
            loadedProfiles.push_back(profile);
        }

        if (loadedProfiles.empty()) {
            qDebug() << "[TEMPLATE_RESTORE] word templates restore failed:" << skippedMessages;
            clearWordMultiTemplateState();
            if (removedRecipeStorePath || !userMessages.isEmpty()) {
                m_templateDirPathsByMode.insert(modeId, QStringList());
                saveSettings(false);
            }
            if (!userMessages.isEmpty()) {
                showParameterWarning("提示", userMessages.join("\n"));
            }
            return;
        }

        m_wordTemplateRecipeDraftSession.reset();
        m_wordTemplateRecipeEditSession.reset();
        m_wordTemplateProfiles.swap(loadedProfiles);
        refreshWordTemplateRecipeAssets();
        if (removedRecipeStorePath || validPaths != paths) {
            m_templateDirPathsByMode.insert(modeId, validPaths);
            saveSettings(false);
            if (!userMessages.isEmpty()) {
                showParameterWarning("提示", userMessages.join("\n"));
            }
        }
        currentTemplateDirPath = m_wordTemplateProfiles.front().dirPath;
        m_currentTemplateNameVisible = false;
        updateCurrentTemplateName();
        refreshWordTemplateEditorCombo();
        qDebug() << "[TEMPLATE_RESTORE] restored word templates:"
                 << static_cast<int>(m_wordTemplateProfiles.size());
        return;
    }

    const QString firstPath = QDir(paths.first()).absolutePath();
    if (!QDir(firstPath).exists()) {
        qDebug() << "[TEMPLATE_RESTORE] template path not exists:" << firstPath;
        currentTemplateDirPath.clear();
        m_currentTemplateNameVisible = false;
        updateCurrentTemplateName();
        m_templateDirPathsByMode.insert(modeId, QStringList());
        saveSettings(false);
        showParameterWarning("提示",
                             QString("加载历史模板路径 %1 失败，该模板状态异常，已跳过读取。")
                             .arg(firstPath));
        return;
    }

    if (!m_wordTemplateProfiles.empty()) {
        m_wordTemplateRecipeDraftSession.reset();
        m_wordTemplateRecipeEditSession.reset();
        m_wordTemplateProfiles.clear();
        refreshWordTemplateEditorCombo();
    }

    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();
    currentTemplateDirPath = firstPath;
    m_currentTemplateNameVisible = loadSettingsFromDir(firstPath, showMessage);
    if (!m_currentTemplateNameVisible) {
        currentTemplateDirPath.clear();
        updateCurrentTemplateName();
        m_templateDirPathsByMode.insert(modeId, QStringList());
        saveSettings(false);
        showParameterWarning("提示",
                             QString("加载历史模板路径 %1 失败，该模板状态异常，已跳过读取。")
                             .arg(firstPath));
        return;
    }
    if (paths.size() != 1 || paths.first() != firstPath) {
        m_templateDirPathsByMode.insert(modeId, QStringList() << firstPath);
        saveSettings(false);
    }
    updateCurrentTemplateName();
    if (m_currentTemplateNameVisible) {
        initOverlapDetectorFromCurrentDir();
    }
    qDebug() << "[TEMPLATE_RESTORE] restored template for mode:" << modeId << firstPath;
}

void Widget::refreshWordTemplateEditorCombo()
{
    if (!ui) {
        return;
    }

    const bool isWordMode = isWordFamilyMode(currentDetectModeId());
    const bool isSingleMode = isSingleTemplateRecipeMode(
                currentDetectModeId());
    const bool isStampMode =
            currentDetectModeId() == QStringLiteral("stamp_detection");
    const bool isTemplateMode = isWordMode || isSingleMode;
    const bool hasWordProfiles = isWordMode && !m_wordTemplateProfiles.empty();

    if (ui->batchTextsure_btn) {
        ui->batchTextsure_btn->setVisible(isWordMode && m_wordTemplateProfiles.size() > 1);
    }
    if (ui->batchImageThresholdButton) {
        ui->batchImageThresholdButton->setVisible(
                    isWordMode && m_wordTemplateProfiles.size() > 1);
    }

    if (m_manualCharacterCropButton) {
        m_manualCharacterCropButton->setVisible(isWordMode || isStampMode);
    }

    bool allProfilesUseLegacyDirectories = hasWordProfiles;
    for (const WordTemplateProfile &profile : m_wordTemplateProfiles) {
        if (!profile.resolvedAssetPathsByRole.isEmpty()) {
            allProfilesUseLegacyDirectories = false;
            break;
        }
    }
    if (m_publishTemplateGroupButton) {
        const bool canPublishWordGroup =
                isWordMode
                && m_wordTemplateProfiles.size() > 1
                && allProfilesUseLegacyDirectories
                && !m_wordTemplateRecipeEditSession.isActive();
        const bool canPublishSingleTemplate =
                isSingleMode
                && !currentTemplateDirPath.trimmed().isEmpty()
                && QDir(currentTemplateDirPath).exists()
                && !m_singleTemplateRecipeEditSession.isActive();
        m_publishTemplateGroupButton->setVisible(
                    canPublishWordGroup || canPublishSingleTemplate);
        m_publishTemplateGroupButton->setText(
                    isSingleMode
                    ? QStringLiteral("\u53D1\u5E03\u5F53\u524D\u6A21\u677F")
                    : QStringLiteral("\u53D1\u5E03\u6A21\u677F\u7EC4"));
        m_publishTemplateGroupButton->setToolTip(
                    isSingleMode
                    ? QStringLiteral(
                        "\u628A\u5F53\u524D\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u7684\u4EA7\u54C1\u6A21\u677F\u53D1\u5E03\u4E3A"
                        "\u53EF\u6309\u6A21\u5F0F\u6062\u590D\u7684\u4EA7\u54C1\u914D\u65B9\u3002")
                    : QStringLiteral(
                        "\u628A\u5F53\u524D\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u7684\u591A\u4E2AProfile\uFF0C\u6309\u5F53\u524D\u987A\u5E8F"
                        "\u53D1\u5E03\u4E3A\u4E00\u4E2A\u4EA7\u54C1\u914D\u65B9\u3002"));
    }

    if (!m_wordTemplateEditComboBox || !m_wordTemplateEditWidget) {
        return;
    }

    auto fillCombo = [this, hasWordProfiles, isSingleMode](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        QSignalBlocker blocker(comboBox);
        comboBox->clear();

        if (hasWordProfiles) {
            for (int i = 0; i < static_cast<int>(m_wordTemplateProfiles.size()); ++i) {
                const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(i)];
                const QString displayName = profile.name.isEmpty()
                        ? QString("模板%1").arg(i + 1)
                        : profile.name;
                comboBox->addItem(displayName, i);
            }
        } else if (isSingleMode) {
            QString displayName = m_currentTemplateDisplayName.trimmed();
            if (displayName.isEmpty()
                    && !currentTemplateDirPath.trimmed().isEmpty()) {
                displayName = QDir(currentTemplateDirPath).dirName();
            }
            comboBox->addItem(displayName.isEmpty()
                              ? QStringLiteral("--")
                              : displayName,
                              -1);
        }
    };

    fillCombo(m_wordTemplateEditComboBox);

    m_wordTemplateEditWidget->setVisible(isTemplateMode);
    if (m_wordTemplateEditLabel) {
        m_wordTemplateEditLabel->setText(
                    QStringLiteral("\u5F53\u524D\u7F16\u8F91\u6A21\u677F:"));
    }
    m_wordTemplateEditComboBox->setVisible(isTemplateMode);
    if (m_publishedRecipeButton) {
        m_publishedRecipeButton->setVisible(isTemplateMode);
    }

    if (!isWordMode) {
        m_currentWordTemplateEditIndex = -1;
        return;
    }

    if (hasWordProfiles) {
        int profileIndex = m_currentWordTemplateEditIndex;
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
            profileIndex = 0;
        }
        setCurrentWordTemplateEditIndex(profileIndex);
    } else {
        m_currentWordTemplateEditIndex = -1;
    }
}

void Widget::applyWordTemplateEditorSelection(int comboIndex)
{
    if (!m_wordTemplateEditComboBox || comboIndex < 0) {
        return;
    }

    bool ok = false;
    const int profileIndex = m_wordTemplateEditComboBox->itemData(comboIndex).toInt(&ok);
    if (!ok || profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return;
    }

    setCurrentWordTemplateEditIndex(profileIndex);
}

void Widget::setCurrentWordTemplateEditIndex(int profileIndex)
{
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return;
    }

    m_currentWordTemplateEditIndex = profileIndex;

    auto syncCombo = [profileIndex](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        int comboIndex = -1;
        for (int i = 0; i < comboBox->count(); ++i) {
            if (comboBox->itemData(i).toInt() == profileIndex) {
                comboIndex = i;
                break;
            }
        }

        if (comboIndex >= 0) {
            QSignalBlocker blocker(comboBox);
            comboBox->setCurrentIndex(comboIndex);
        }
    };

    syncCombo(m_wordTemplateEditComboBox);

    const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
    currentTemplateDirPath = profile.dirPath;
    savedTrackingBox = profile.settings.trackingBox;
    hasValidBoxes = profile.settings.hasValidBoxes;
    {
        QSignalBlocker blocker(ui->dateEdit);
        ui->dateEdit->setPlainText(profile.settings.targetText);
    }
    {
        QSignalBlocker blocker(ui->lineEdit_yuzhi);
        ui->lineEdit_yuzhi->setText(QString::number(static_cast<int>(profile.settings.imageThreshold)));
    }
    refreshTemplatePrivateSettingDirty();

    qDebug() << "[WORD_TEMPLATE_PROFILE] editing profile:"
             << profileIndex
             << profile.name
             << profile.dirPath
             << "threshold:" << profile.settings.imageThreshold;

    displayWordTemplateRawImage(profile);
}

void Widget::publishCurrentWordTemplateGroup()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState != TemplateCaptureState::Idle) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u505C\u6B62\u8BC6\u522B\u6216\u9000\u51FA"
                        "\u6A21\u677F\u5236\u4F5C\uFF0C\u518D\u53D1\u5E03"
                        "\u5F53\u524D\u6A21\u677F\u7EC4\u3002"));
        return;
    }

    DetectionMode detectionMode;
    if (!detectionModeFromId(currentDetectModeId(), &detectionMode)
            || (detectionMode != DetectionMode::Word
                && detectionMode != DetectionMode::BarcodeWord)) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u591AProfile\u914D\u65B9\u53EA\u7528\u4E8E"
                        "\u5B57\u5E93\u5339\u914D\u548C\u4E8C\u7EF4\u7801+"
                        "\u4E09\u671F\u6A21\u5F0F\u3002"));
        return;
    }
    if (m_wordTemplateProfiles.size() < 2) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u81F3\u5C11\u52A0\u8F7D\u4E24\u4E2A\u6709\u6548\u6A21\u677F\u3002"));
        return;
    }
    if (m_wordTemplateRecipeEditSession.isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u5DF2\u7ECF\u662F\u5DF2\u53D1\u5E03\u914D\u65B9\uFF0C"
                        "\u65E0\u9700\u91CD\u590D\u53D1\u5E03\u6A21\u677F\u7EC4\u3002"));
        return;
    }
    if (m_templateTargetTextDirty || m_templateImageThresholdDirty) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524DProfile\u8FD8\u6709\u672A\u786E\u8BA4\u7684"
                        "\u76EE\u6807\u5B57\u7B26\u6216\u56FE\u50CF\u9608\u503C\u3002\n"
                        "\u8BF7\u5148\u70B9\u51FB\u5BF9\u5E94\u7684\u786E\u8BA4/\u8BBE\u7F6E"
                        "\u6309\u94AE\uFF0C\u518D\u53D1\u5E03\u6A21\u677F\u7EC4\u3002"));
        return;
    }

    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.reserve(static_cast<int>(m_wordTemplateProfiles.size()));
    for (int profileIndex = 0;
         profileIndex < static_cast<int>(m_wordTemplateProfiles.size());
         ++profileIndex) {
        const WordTemplateProfile &profile =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        if (!profile.resolvedAssetPathsByRole.isEmpty()) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "Profile [%1] \u4E0D\u662F\u65E7\u6A21\u677F\u76EE\u5F55\uFF0C"
                            "\u5F53\u524D\u6A21\u677F\u7EC4\u4FDD\u6301\u4E0D\u53D8\u3002")
                        .arg(profile.name));
            return;
        }

        const QDir sourceDirectory(profile.dirPath);
        if (!sourceDirectory.exists()) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "Profile [%1] \u7684\u65E7\u6A21\u677F\u76EE\u5F55\u5DF2\u4E0D\u5B58\u5728\uFF1A\n%2\n\n"
                            "\u4EA7\u54C1\u914D\u65B9\u672A\u53D1\u5E03\uFF0C\u5F53\u524D\u6A21\u677F\u7EC4\u4FDD\u6301\u4E0D\u53D8\u3002")
                        .arg(profile.name, profile.dirPath));
            return;
        }

        TemplateRecipeProfileSource profileSource;
        profileSource.assetManifest = buildTemplateProfileAssetManifest(
                    sourceDirectory.absolutePath(), profileIndex);
        const QString profileName = profile.name.trimmed().isEmpty()
                ? sourceDirectory.dirName()
                : profile.name.trimmed();
        profileSource.profile = recipeProfileFromTemplatePrivateSettings(
                    profileName,
                    profile.settings,
                    profileSource.assetManifest.profileAssetKeys);
        profileSources.append(profileSource);
    }

    const WordTemplateProfile &firstProfile = m_wordTemplateProfiles.front();
    QString firstProfileName = firstProfile.name.trimmed();
    if (firstProfileName.isEmpty()) {
        firstProfileName = QDir(firstProfile.dirPath).dirName();
    }
    const QString defaultDisplayName = QStringLiteral("%1\u7B49%2\u4E2A\u6A21\u677F")
            .arg(firstProfileName)
            .arg(static_cast<int>(m_wordTemplateProfiles.size()));
    bool accepted = false;
    const QString displayName = QInputDialog::getText(
                this,
                QStringLiteral("\u53D1\u5E03\u591AProfile\u4EA7\u54C1\u914D\u65B9"),
                QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u540D\u79F0\uFF1A"),
                QLineEdit::Normal,
                defaultDisplayName,
                &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (displayName.isEmpty()) {
        showParameterWarning(
                    QStringLiteral("\u53C2\u6570\u9519\u8BEF"),
                    QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A\u3002"));
        return;
    }

    const ProductRecipe recipeHeader = createProductRecipe(displayName,
                                                            detectionMode);
    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection publishedSelection;
    QString publishError;
    if (!publishTemplateRecipe(store,
                               recipeHeader,
                               profileSources,
                               &publishedSelection,
                               &publishError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u591AProfile\u4EA7\u54C1\u914D\u65B9\u53D1\u5E03\u5931\u8D25\uFF0C"
                        "\u5F53\u524D\u65E7\u6A21\u677F\u7EC4\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                    .arg(publishError));
        return;
    }

    QStringList pendingMessages;
    QString activationError;
    if (!activatePublishedWordRecipe(
                publishedSelection.recipe->recipeId,
                currentDetectModeId(),
                false,
                &pendingMessages,
                &activationError)) {
        qWarning() << "[RECIPE_PUBLISH] published word template group;"
                   << "activation failed:"
                   << publishedSelection.recipe->recipeId
                   << activationError;
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u591AProfile\u4EA7\u54C1\u914D\u65B9\u5DF2\u53D1\u5E03\uFF0C"
                        "\u4F46\u5F53\u524D\u7F13\u5B58\u88C5\u914D\u5931\u8D25\u3002\n"
                        "\u5F53\u524D\u65E7\u6A21\u677F\u7EC4\u4FDD\u6301\u4E0D\u53D8\uFF0C"
                        "\u8BF7\u7A0D\u540E\u901A\u8FC7\u201C\u5DF2\u53D1\u5E03\u914D\u65B9\u201D"
                        "\u91CD\u65B0\u9009\u62E9\uFF1A\n%1")
                    .arg(activationError));
        return;
    }
    saveSettings(false);

    qDebug() << "[RECIPE_PUBLISH] published word template group:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath
             << "profiles:" << publishedSelection.profiles.size();
    QString message = QStringLiteral(
                "\u5DF2\u628A %1 \u4E2AProfile\u53D1\u5E03\u4E3A\u4E00\u4E2A\u4EA7\u54C1\u914D\u65B9\u201C%2\u201D\u3002")
            .arg(publishedSelection.profiles.size())
            .arg(displayName);
    if (!pendingMessages.isEmpty()) {
        message += QStringLiteral(
                    "\n\n\u4EE5\u4E0BProfile\u76EE\u6807\u5B57\u7B26\u5F85\u786E\u8BA4\uFF1A\n%1")
                .arg(pendingMessages.join(QLatin1Char('\n')));
        showParameterWarning(QStringLiteral("\u63D0\u793A"), message);
    } else {
        showParameterInfo(QStringLiteral("\u63D0\u793A"), message);
    }
}

void Widget::publishCurrentSingleTemplateRecipe()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState != TemplateCaptureState::Idle) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u505C\u6B62\u8BC6\u522B\u6216\u9000\u51FA\u6A21\u677F\u5236\u4F5C\uFF0C"
                        "\u518D\u53D1\u5E03\u5F53\u524D\u6A21\u677F\u3002"));
        return;
    }

    DetectionMode detectionMode;
    if (!detectionModeFromId(currentDetectModeId(), &detectionMode)
            || (detectionMode != DetectionMode::Stamp
                && detectionMode != DetectionMode::Ocr)) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u53D1\u5E03\u5F53\u524D\u5355\u6A21\u677F\u53EA\u7528\u4E8E"
                        "\u6A21\u677F\u5339\u914D\u548C\u6DF1\u5EA6OCR\u6A21\u5F0F\u3002"));
        return;
    }
    if (m_singleTemplateRecipeEditSession.isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u5DF2\u7ECF\u662F\u5DF2\u53D1\u5E03\u914D\u65B9\uFF0C"
                        "\u65E0\u9700\u91CD\u590D\u53D1\u5E03\u3002"));
        return;
    }
    if (currentTemplateDirPath.trimmed().isEmpty()
            || !QDir(currentTemplateDirPath).exists()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u4E00\u4E2A\u6709\u6548\u4EA7\u54C1\u6A21\u677F\u3002"));
        return;
    }
    if (m_templateTargetTextDirty || m_templateImageThresholdDirty) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8FD8\u6709\u672A\u786E\u8BA4\u7684\u76EE\u6807\u5B57\u7B26\u6216\u56FE\u50CF\u9608\u503C\u3002\n"
                        "\u8BF7\u5148\u70B9\u51FB\u5BF9\u5E94\u7684\u786E\u8BA4/\u8BBE\u7F6E\u6309\u94AE\u3002"));
        return;
    }

    TemplatePrivateSettings privateSettings;
    QString settingsError;
    if (!AppSettingsManager::loadTemplatePrivateSettings(
                currentTemplateDirPath,
                &privateSettings,
                &settingsError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u5F53\u524D\u65E7\u6A21\u677F\u79C1\u6709\u8BBE\u7F6E\u65E0\u6CD5\u8BFB\u53D6\uFF0C"
                        "\u4EA7\u54C1\u914D\u65B9\u672A\u53D1\u5E03\uFF1A\n%1")
                    .arg(settingsError));
        return;
    }
    int currentUiThreshold = 0;
    if (!parseIntValue(ui->lineEdit_yuzhi->text(),
                       &currentUiThreshold)
            || currentUiThreshold < 0
            || currentUiThreshold > 100) {
        showParameterWarning(
                    QStringLiteral("\u53C2\u6570\u9519\u8BEF"),
                    QStringLiteral(
                        "\u56FE\u50CF\u5408\u683C\u9608\u503C\u5FC5\u987B\u662F0\u5230100\u4E4B\u95F4\u7684\u6574\u6570\uFF08\u5355\u4F4D\uFF1A%\uFF09\u3002"));
        return;
    }
    privateSettings.targetText = ui->dateEdit->toPlainText();
    privateSettings.imageThreshold = currentUiThreshold;

    const QDir sourceDirectory(currentTemplateDirPath);
    const cv::Mat sourceTrackingTemplate = decodeImageFile(
                sourceDirectory.filePath(
                    QStringLiteral("tracking_template.bmp")),
                cv::IMREAD_COLOR);
    CalibrationData sourceCalibration;
    const QString sourceCalibrationPath = sourceDirectory.filePath(
                QStringLiteral("calibrate_config.yaml"));
    if (!privateSettings.hasValidBoxes
            || privateSettings.trackingBox.width <= 0
            || privateSettings.trackingBox.height <= 0
            || sourceTrackingTemplate.empty()
            || !sourceCalibration.load(
                sourceCalibrationPath.toLocal8Bit().toStdString())
            || sourceCalibration.date_poly.size() < 3) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u5F53\u524D\u65E7\u6A21\u677F\u7684\u5B9A\u4F4D\u56FE\u3001\u5B9A\u4F4D\u6846\u6216\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\u65E0\u6548\uFF0C"
                        "\u4EA7\u54C1\u914D\u65B9\u672A\u53D1\u5E03\u3002"));
        return;
    }
    if (detectionMode == DetectionMode::Stamp) {
        OverlapDetector sourceOverlapDetector;
        std::vector<cv::Mat> sourceDigitTemplates;
        std::vector<int> sourceDigitTargetIndexes;
        QString sourceCharacterError;
        if (sourceCalibration.stamp_poly.size() < 3
                || !sourceOverlapDetector.init(
                    sourceDirectory.filePath(
                        QStringLiteral("template_ring.bmp"))
                    .toLocal8Bit().toStdString(),
                    sourceCalibrationPath.toLocal8Bit().toStdString())
                || !loadWordDigitTemplatesFromDir(
                    sourceDirectory.absolutePath(),
                    parseTemplateTargetUnits(privateSettings.targetText),
                    &sourceDigitTemplates,
                    &sourceDigitTargetIndexes,
                    &sourceCharacterError,
                    false)) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "\u5F53\u524D\u94A2\u5370\u6A21\u677F\u7684\u94A2\u5370\u73AF\u3001\u94A2\u5370\u533A\u57DF\u6216\u76EE\u6807\u5B57\u7B26\u8D44\u4EA7\u65E0\u6548\uFF0C"
                            "\u4EA7\u54C1\u914D\u65B9\u672A\u53D1\u5E03\u3002\n%1")
                        .arg(sourceCharacterError));
            return;
        }
    }

    TemplateRecipeProfileSource profileSource;
    profileSource.assetManifest =
            buildTemplateProfileAssetManifest(
                sourceDirectory.absolutePath(), 0);
    const QString profileName = sourceDirectory.dirName().trimmed().isEmpty()
            ? QStringLiteral("Profile 1")
            : sourceDirectory.dirName().trimmed();
    profileSource.profile = recipeProfileFromTemplatePrivateSettings(
                profileName,
                privateSettings,
                profileSource.assetManifest.profileAssetKeys);

    bool accepted = false;
    const QString displayName = QInputDialog::getText(
                this,
                QStringLiteral("\u53D1\u5E03\u4EA7\u54C1\u914D\u65B9"),
                QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u540D\u79F0\uFF1A"),
                QLineEdit::Normal,
                profileName,
                &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (displayName.isEmpty()) {
        showParameterWarning(
                    QStringLiteral("\u53C2\u6570\u9519\u8BEF"),
                    QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A\u3002"));
        return;
    }

    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);
    const ProductRecipe recipeHeader =
            createProductRecipe(displayName, detectionMode);
    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection publishedSelection;
    QString publishError;
    if (!publishTemplateRecipe(store,
                               recipeHeader,
                               profileSources,
                               &publishedSelection,
                               &publishError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u4EA7\u54C1\u914D\u65B9\u53D1\u5E03\u5931\u8D25\uFF0C"
                        "\u5F53\u524D\u65E7\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                    .arg(publishError));
        return;
    }

    QString activationError;
    if (!activatePublishedSingleTemplateRecipe(
                publishedSelection.recipe->recipeId,
                currentDetectModeId(),
                false,
                &activationError)) {
        qWarning() << "[RECIPE_PUBLISH] published single-template recipe;"
                   << "activation failed:"
                   << publishedSelection.recipe->recipeId
                   << activationError;
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u4EA7\u54C1\u914D\u65B9\u5DF2\u53D1\u5E03\uFF0C\u4F46\u5F53\u524D\u7F13\u5B58\u88C5\u914D\u5931\u8D25\u3002\n"
                        "\u5F53\u524D\u65E7\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF0C"
                        "\u8BF7\u7A0D\u540E\u4ECE\u201C\u5DF2\u53D1\u5E03\u914D\u65B9\u201D\u91CD\u65B0\u9009\u62E9\uFF1A\n%1")
                    .arg(activationError));
        return;
    }
    saveSettings(false);

    qDebug() << "[RECIPE_PUBLISH] published single-template recipe:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath
             << "mode:" << currentDetectModeId();
    showParameterInfo(
                QStringLiteral("\u63D0\u793A"),
                QStringLiteral("\u5DF2\u53D1\u5E03\u5E76\u52A0\u8F7D\u4EA7\u54C1\u914D\u65B9\u201C%1\u201D\u3002")
                .arg(displayName));
}

void Widget::selectPublishedRecipe()
{
    if (m_operationState == OperationState::Detecting
            || m_operationState == OperationState::Stopping
            || m_templateCaptureState != TemplateCaptureState::Idle) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u505C\u6B62\u8BC6\u522B\u6216\u9000\u51FA"
                        "\u6A21\u677F\u5236\u4F5C\uFF0C\u518D\u9009\u62E9"
                        "\u5DF2\u53D1\u5E03\u914D\u65B9\u3002"));
        return;
    }

    DetectionMode detectionMode;
    if (!detectionModeFromId(currentDetectModeId(), &detectionMode)
            || !isTemplateRecipeMode(detectionMode)) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F\u4E0D\u4F7F\u7528\u6A21\u677F\u4EA7\u54C1\u914D\u65B9\u3002"));
        return;
    }

    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeCatalog catalog;
    QString catalogError;
    if (!store.listRecipes(&catalog, &catalogError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u4EA7\u54C1\u914D\u65B9\u5217\u8868\u8BFB\u53D6\u5931\u8D25\uFF1A\n%1")
                    .arg(catalogError));
        return;
    }

    QVector<RecipeCatalogEntry> matchingRecipes;
    for (const RecipeCatalogEntry &entry : catalog.recipes) {
        if (entry.detectionMode == detectionMode) {
            matchingRecipes.append(entry);
        }
    }
    if (matchingRecipes.isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F\u8FD8\u6CA1\u6709"
                        "\u53EF\u52A0\u8F7D\u7684\u5DF2\u53D1\u5E03\u914D\u65B9\u3002"));
        return;
    }

    RecipeSelectionDialog dialog(matchingRecipes, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList pendingMessages;
    QString activationError;
    const bool activated = isWordFamilyMode(currentDetectModeId())
            ? activatePublishedWordRecipe(dialog.selectedRecipeId(),
                                          currentDetectModeId(),
                                          true,
                                          &pendingMessages,
                                          &activationError)
            : activatePublishedSingleTemplateRecipe(
                dialog.selectedRecipeId(),
                currentDetectModeId(),
                true,
                &activationError);
    if (!activated) {
        return;
    }
    saveSettings(false);

    const ProductRecipe &activeRecipe = isWordFamilyMode(
                currentDetectModeId())
            ? m_wordTemplateRecipeEditSession.recipe()
            : m_singleTemplateRecipeEditSession.recipe();
    QString message = QStringLiteral(
                "\u5DF2\u52A0\u8F7D\u4EA7\u54C1\u914D\u65B9\u201C%1\u201D\uFF0C"
                "\u5171 %2 \u4E2AProfile\u3002")
            .arg(activeRecipe.displayName)
            .arg(activeRecipe.profiles.size());
    if (!pendingMessages.isEmpty()) {
        message += QStringLiteral(
                    "\n\n\u4EE5\u4E0BProfile\u76EE\u6807\u5B57\u7B26"
                    "\u5F85\u786E\u8BA4\uFF1A\n%1")
                .arg(pendingMessages.join(QLatin1Char('\n')));
        showParameterWarning(QStringLiteral("\u63D0\u793A"), message);
    } else {
        showParameterInfo(QStringLiteral("\u63D0\u793A"), message);
    }
}

bool Widget::activatePublishedWordRecipe(const QString &recipeId,
                                         const QString &modeId,
                                         bool showErrorMessage,
                                         QStringList *pendingMessages,
                                         QString *errorMessage)
{
    if (pendingMessages) {
        pendingMessages->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    DetectionMode detectionMode;
    if (!detectionModeFromId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Word
                && detectionMode != DetectionMode::BarcodeWord)) {
        const QString message = QStringLiteral(
                    "\u5DF2\u53D1\u5E03\u5B57\u5E93\u914D\u65B9\u53EA\u7528\u4E8E"
                    "\u5B57\u5E93\u5339\u914D\u548C\u4E8C\u7EF4\u7801+"
                    "\u4E09\u671F\u6A21\u5F0F\u3002");
        if (errorMessage) *errorMessage = message;
        if (showErrorMessage) {
            showParameterInfoAsError(QStringLiteral("\u63D0\u793A"), message);
        }
        return false;
    }

    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection selection;
    QString selectionError;
    if (!loadRecipeSelection(store,
                             recipeId,
                             detectionMode,
                             &selection,
                             &selectionError)) {
        if (errorMessage) *errorMessage = selectionError;
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u52A0\u8F7D\u5931\u8D25\uFF0C"
                            "\u5F53\u524D\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                        .arg(selectionError));
        }
        return false;
    }

    std::vector<WordTemplateProfile> loadedProfiles;
    QStringList loadedPendingMessages;
    QString cacheError;
    if (!loadWordTemplateProfilesFromRecipeSelection(
                selection,
                &loadedProfiles,
                &loadedPendingMessages,
                &cacheError)) {
        if (errorMessage) *errorMessage = cacheError;
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u8D44\u6E90\u65E0\u6CD5\u88C5\u914D\uFF0C"
                            "\u5F53\u524D\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                        .arg(cacheError));
        }
        return false;
    }

    TemplateRecipeEditSession candidateEditSession;
    QString editSessionError;
    if (!candidateEditSession.begin(selection, &editSessionError)) {
        if (errorMessage) *errorMessage = editSessionError;
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u7F16\u8F91\u4F1A\u8BDD\u65E0\u6CD5\u5EFA\u7ACB\uFF0C"
                            "\u5F53\u524D\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                        .arg(editSessionError));
        }
        return false;
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();
    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();
    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateProfiles.swap(loadedProfiles);
    m_wordTemplateRecipeEditSession = candidateEditSession;
    currentTemplateDirPath = selection.recipeDirectoryPath;
    m_currentTemplateNameVisible = false;
    m_publishedRecipeIdsByMode.insert(modeId,
                                      selection.recipe->recipeId);
    refreshWordTemplateEditorCombo();
    clearTemplatePrivateSettingDirty();
    if (pendingMessages) {
        *pendingMessages = loadedPendingMessages;
    }

    qDebug() << "[RECIPE_SELECT] selected word recipe:"
             << selection.recipe->recipeId
             << selection.recipe->displayName
             << "profiles:" << m_wordTemplateProfiles.size();
    return true;
}

bool Widget::loadSingleTemplateCharacterAssets(
        const QMap<QString, QString> &assetPathsByRole,
        const QStringList &targetUnits,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!templates || !templateTargetIndexes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("\u5185\u90E8\u5B57\u7B26\u6A21\u677F\u8F93\u51FA\u65E0\u6548\u3002");
        }
        return false;
    }

    templates->clear();
    templateTargetIndexes->clear();
    if (targetUnits.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("\u76EE\u6807\u5B57\u7B26\u4E3A\u7A7A\u6216\u89E3\u6790\u5931\u8D25\u3002");
        }
        return false;
    }

    QStringList failedFiles;
    for (int targetIndex = 0;
         targetIndex < targetUnits.size();
         ++targetIndex) {
        const QString targetUnit =
                targetUnits.at(targetIndex).trimmed().toLower();
        QStringList matchingPaths;
        for (auto it = assetPathsByRole.constBegin();
             it != assetPathsByRole.constEnd();
             ++it) {
            if (!it.key().startsWith(QStringLiteral("character/"))) {
                continue;
            }
            const QString fileName =
                    it.key().mid(QStringLiteral("character/").size());
            if (QFileInfo(fileName).completeBaseName().trimmed().toLower()
                    == targetUnit) {
                matchingPaths.append(it.value());
            }
        }
        matchingPaths.sort(Qt::CaseInsensitive);
        if (matchingPaths.isEmpty()) {
            failedFiles.append(targetUnit);
            continue;
        }

        for (const QString &matchingPath : matchingPaths) {
            const cv::Mat characterTemplate =
                    decodeImageFile(matchingPath,
                                    cv::IMREAD_GRAYSCALE);
            if (characterTemplate.empty()) {
                failedFiles.append(
                            QFileInfo(matchingPath).fileName());
                continue;
            }
            templates->push_back(characterTemplate);
            templateTargetIndexes->push_back(targetIndex);
        }
    }
    if (!failedFiles.isEmpty()) {
        templates->clear();
        templateTargetIndexes->clear();
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u4EE5\u4E0B\u5B57\u7B26\u6A21\u677F\u56FE\u65E0\u6CD5\u89E3\u7801\uFF1A%1")
                    .arg(failedFiles.join(QLatin1Char(' ')));
        }
        return false;
    }
    return !templates->empty();
}

bool Widget::activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    auto fail = [this, showErrorMessage, errorMessage](
            const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u8D44\u6E90\u65E0\u6CD5\u5B8C\u6574\u88C5\u914D\uFF0C"
                            "\u5F53\u524D\u6A21\u677F\u4FDD\u6301\u4E0D\u53D8\uFF1A\n%1")
                        .arg(message));
        }
        return false;
    };

    DetectionMode detectionMode;
    if (!detectionModeFromId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Stamp
                && detectionMode != DetectionMode::Ocr)) {
        return fail(QStringLiteral(
                        "\u5DF2\u53D1\u5E03\u5355\u6A21\u677F\u914D\u65B9\u53EA\u7528\u4E8E"
                        "\u6A21\u677F\u5339\u914D\u548C\u6DF1\u5EA6OCR\u6A21\u5F0F\u3002"));
    }

    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection selection;
    QString selectionError;
    if (!loadRecipeSelection(store,
                             recipeId,
                             detectionMode,
                             &selection,
                             &selectionError)) {
        return fail(selectionError);
    }
    if (selection.profiles.size() != 1
            || selection.recipe->profiles.size() != 1) {
        return fail(QStringLiteral(
                        "\u5355\u6A21\u677F\u4EA7\u54C1\u914D\u65B9\u5FC5\u987B\u4E14\u53EA\u80FD\u5305\u542B\u4E00\u4E2AProfile\u3002"));
    }

    const ResolvedRecipeProfile &resolvedProfile =
            selection.profiles.first();
    const TemplatePrivateSettings privateSettings =
            templatePrivateSettingsFromRecipeProfile(
                resolvedProfile.profile);
    if (!privateSettings.hasValidBoxes
            || privateSettings.trackingBox.width <= 0
            || privateSettings.trackingBox.height <= 0) {
        return fail(QStringLiteral(
                        "\u4EA7\u54C1\u914D\u65B9\u5B9A\u4F4D\u6846\u53C2\u6570\u65E0\u6548\u3002"));
    }

    TemplateProfileLoadPlan loadPlan;
    QString loadPlanError;
    if (!buildTemplateProfileLoadPlan(
                resolvedProfile,
                parseTemplateTargetUnits(privateSettings.targetText),
                &loadPlan,
                &loadPlanError)) {
        return fail(loadPlanError);
    }

    const cv::Mat trackingTemplate =
            decodeImageFile(loadPlan.trackingTemplatePath,
                            cv::IMREAD_COLOR);
    if (trackingTemplate.empty()) {
        return fail(QStringLiteral(
                        "\u5B9A\u4F4D\u6A21\u677F\u56FE tracking_template.bmp \u65E0\u6CD5\u89E3\u7801\u3002"));
    }

    CalibrationData calibration;
    if (!calibration.load(
                loadPlan.calibrationPath.toLocal8Bit().toStdString())
            || calibration.date_poly.size() < 3) {
        return fail(QStringLiteral(
                        "\u6807\u5B9A\u6587\u4EF6\u7F3A\u5C11\u6709\u6548\u7684\u55B7\u7801\u68C0\u6D4B\u533A\u57DF\u3002"));
    }

    OverlapDetector candidateOverlapDetector;
    std::vector<cv::Mat> candidateDigitTemplates;
    std::vector<int> candidateDigitTargetIndexes;
    if (detectionMode == DetectionMode::Stamp) {
        const QString stampRingPath =
                resolvedProfile.assetPathsByRole
                .value(QStringLiteral("stampRing")).trimmed();
        if (stampRingPath.isEmpty()
                || calibration.stamp_poly.size() < 3
                || !candidateOverlapDetector.init(
                    stampRingPath.toLocal8Bit().toStdString(),
                    loadPlan.calibrationPath.toLocal8Bit().toStdString())) {
            return fail(QStringLiteral(
                            "\u94A2\u5370\u73AF\u56FE\u6216\u94A2\u5370\u533A\u57DF\u65E0\u6548\uFF0C"
                            "\u9632\u91CD\u53E0\u5F15\u64CE\u65E0\u6CD5\u521D\u59CB\u5316\u3002"));
        }
        QString characterError;
        if (!loadSingleTemplateCharacterAssets(
                    resolvedProfile.assetPathsByRole,
                    parseTemplateTargetUnits(privateSettings.targetText),
                    &candidateDigitTemplates,
                    &candidateDigitTargetIndexes,
                    &characterError)) {
            return fail(QStringLiteral(
                            "\u94A2\u5370\u76EE\u6807\u5B57\u7B26\u8D44\u4EA7\u4E0D\u5B8C\u6574\uFF1A%1")
                        .arg(characterError));
        }
    }

    TemplateRecipeEditSession candidateEditSession;
    QString editError;
    if (!candidateEditSession.begin(selection, &editError)) {
        return fail(editError);
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    hideTemplateGuide();
    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateRecipeEditSession.reset();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    m_singleTemplateRecipeEditSession = candidateEditSession;
    m_singleTemplateResolvedAssetPathsByRole =
            resolvedProfile.assetPathsByRole;
    m_currentTemplateDisplayName = selection.recipe->displayName;
    currentTemplateDirPath =
            QFileInfo(loadPlan.trackingTemplatePath).absolutePath();
    m_loadedTrackingTemplate = trackingTemplate;
    savedTrackingBox = privateSettings.trackingBox;
    savedBarcodePoly = calibration.barcode_poly;
    savedDatePoly = calibration.date_poly;
    hasValidBoxes = true;
    digitTemplates.swap(candidateDigitTemplates);
    digitTemplateTargetIndexes.swap(candidateDigitTargetIndexes);
    if (detectionMode == DetectionMode::Stamp) {
        overlapDetector = candidateOverlapDetector;
    }
    applyTemplatePrivateSettingsToUi(privateSettings);
    emit ssim(static_cast<int>(privateSettings.imageThreshold));
    m_currentTemplateNameVisible = true;
    m_publishedRecipeIdsByMode.insert(modeId,
                                      selection.recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearTemplatePrivateSettingDirty();

    qDebug() << "[RECIPE_SELECT] selected single-template recipe:"
             << selection.recipe->recipeId
             << selection.recipe->displayName
             << "mode:" << modeId;
    return true;
}

bool Widget::republishSingleTemplateRecipeSettings(
        const TemplatePrivateSettings &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_singleTemplateRecipeEditSession.isActive()
            || m_singleTemplateRecipeEditSession.recipe().profiles.size()
               != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u5F53\u524D\u5DF2\u53D1\u5E03\u5355\u6A21\u677F\u6CA1\u6709\u6709\u6548\u7F16\u8F91\u4F1A\u8BDD\u3002");
        }
        return false;
    }

    const RecipeProfile &currentProfile =
            m_singleTemplateRecipeEditSession.recipe().profiles.first();
    const RecipeProfile updatedProfile =
            recipeProfileFromTemplatePrivateSettings(
                currentProfile.name,
                settings,
                currentProfile.assetKeys);
    QVector<RecipeProfile> updatedProfiles;
    updatedProfiles.append(updatedProfile);

    const RecipeStore store(
                QDir(AppSettingsManager::globalDataDirPath())
                .filePath(QStringLiteral("recipes")));
    RecipeSelection publishedSelection;
    if (!TemplateRecipeWorkflow::republishProfiles(
                &m_singleTemplateRecipeEditSession,
                store,
                updatedProfiles,
                &publishedSelection,
                nullptr,
                errorMessage)) {
        return false;
    }
    if (publishedSelection.profiles.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u91CD\u65B0\u53D1\u5E03\u540E\u7684\u5355\u6A21\u677FProfile\u6570\u91CF\u65E0\u6548\u3002");
        }
        return false;
    }

    m_singleTemplateResolvedAssetPathsByRole =
            publishedSelection.profiles.first().assetPathsByRole;
    m_publishedRecipeIdsByMode.insert(
                currentDetectModeId(),
                publishedSelection.recipe->recipeId);
    saveSettings(false);
    qDebug() << "[RECIPE_PUBLISH] republished single-template recipe:"
             << publishedSelection.recipe->recipeId
             << "mode:" << currentDetectModeId();
    return true;
}

int Widget::currentWordTemplateProfileIndex() const
{
    if (m_currentWordTemplateEditIndex < 0
            || m_currentWordTemplateEditIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return -1;
    }
    return m_currentWordTemplateEditIndex;
}

QString Widget::wordTemplateProfileAssetPath(
        const WordTemplateProfile &profile,
        const QString &role,
        const QString &legacyFileName) const
{
    const QString resolvedPath =
            profile.resolvedAssetPathsByRole.value(role).trimmed();
    if (!resolvedPath.isEmpty()) {
        return QFileInfo(resolvedPath).absoluteFilePath();
    }

    if (profile.dirPath.trimmed().isEmpty()
            || legacyFileName.trimmed().isEmpty()) {
        return QString();
    }
    return QDir(profile.dirPath).filePath(legacyFileName);
}

void Widget::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    const QString rawImagePath = wordTemplateProfileAssetPath(
                profile,
                QStringLiteral("rawImage"),
                QStringLiteral("template_raw.png"));
    const QString templateName = profile.name.trimmed().isEmpty()
            ? QDir(profile.dirPath).dirName()
            : profile.name.trimmed();
    displayWordTemplateRawImageFile(rawImagePath, templateName);
}

void Widget::displayWordTemplateRawImage(const QString &dirPath)
{
    if (dirPath.trimmed().isEmpty()) {
        return;
    }
    displayWordTemplateRawImageFile(
                QDir(dirPath).filePath(QStringLiteral("template_raw.png")),
                QDir(dirPath).dirName());
}

void Widget::displayWordTemplateRawImageFile(
        const QString &rawImagePath,
        const QString &templateName)
{
    if (!ui || !ui->image_undetected || rawImagePath.trimmed().isEmpty()) {
        return;
    }

    if (!QFile::exists(rawImagePath)) {
        qDebug() << "[WORD_TEMPLATE_PROFILE] template_raw.png not found:" << rawImagePath;
        return;
    }

    QPixmap rawPixmap;
    if (!rawPixmap.load(rawImagePath)) {
        qDebug() << "[WORD_TEMPLATE_PROFILE] template_raw.png load failed:" << rawImagePath;
        return;
    }

    ui->image_undetected->setScaledContents(false);
    ui->image_undetected->setAlignment(Qt::AlignCenter);
    ui->image_undetected->setAutoFitPixmap(rawPixmap);

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    updateImageDisplayStatusText(QString("正在显示模板【%1】的产品图像")
                                 .arg(templateName));
}

QStringList Widget::wordTemplateImagePathsForKey(const QDir &directory,
                                                 const QString &searchKey,
                                                 bool includeVariants) const
{
    QStringList exactPaths;
    QStringList variantPaths;
    const QString normalizedKey = searchKey.trimmed().toLower();
    if (normalizedKey.isEmpty() || !directory.exists()) {
        return QStringList();
    }

    static const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tiff"};
    const QFileInfoList fileList = directory.entryInfoList(
                filters,
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &fileInfo : fileList) {
        const QString baseName = fileInfo.completeBaseName().toLower();
        if (baseName == normalizedKey) {
            exactPaths.append(fileInfo.absoluteFilePath());
        } else if (includeVariants && baseName.startsWith(normalizedKey)) {
            const QString suffix = baseName.mid(normalizedKey.length());
            if (suffix.startsWith("_") || suffix.startsWith("-") || suffix.startsWith("(")) {
                variantPaths.append(fileInfo.absoluteFilePath());
            }
        }
    }

    exactPaths.sort(Qt::CaseInsensitive);
    variantPaths.sort(Qt::CaseInsensitive);
    exactPaths.append(variantPaths);
    return exactPaths;
}

bool Widget::loadWordDigitTemplatesFromDir(const QString &dirPath,
                                           const QStringList &baseNames,
                                           std::vector<cv::Mat> *templates,
                                           std::vector<int> *templateTargetIndexes,
                                           QString *errorMessage,
                                           bool includeVariants) const
{
    if (!templates || !templateTargetIndexes) {
        if (errorMessage) {
            *errorMessage = "内部参数无效";
        }
        return false;
    }

    templates->clear();
    templateTargetIndexes->clear();

    QDir directory(dirPath);
    if (!directory.exists()) {
        if (errorMessage) {
            *errorMessage = "产品模板文件夹不存在";
        }
        return false;
    }

    if (baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "目标字符为空或解析失败";
        }
        return false;
    }

    QStringList failedNames;
    for (int targetIndex = 0; targetIndex < baseNames.size(); ++targetIndex) {
        const QString searchKey = baseNames.at(targetIndex).trimmed().toLower();
        const QStringList imagePaths = wordTemplateImagePathsForKey(directory, searchKey, includeVariants);
        if (imagePaths.isEmpty()) {
            failedNames.append(searchKey);
            continue;
        }

        bool allVariantsLoaded = true;
        for (const QString &imagePath : imagePaths) {
            QFile file(imagePath);
            if (!file.open(QIODevice::ReadOnly)) {
                failedNames.append(QFileInfo(imagePath).completeBaseName() + "(无法打开)");
                allVariantsLoaded = false;
                continue;
            }

            const QByteArray data = file.readAll();
            cv::Mat templateImg;
            try {
                std::vector<uchar> buf(data.begin(), data.end());
                templateImg = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
            } catch (...) {
                qDebug() << "[WORD_TEMPLATE] digit imdecode crashed:" << imagePath;
            }

            if (templateImg.empty()) {
                failedNames.append(QFileInfo(imagePath).completeBaseName() + "(读取损坏)");
                allVariantsLoaded = false;
                continue;
            }

            templates->push_back(templateImg);
            templateTargetIndexes->push_back(targetIndex);
        }

        if (!allVariantsLoaded) {
            continue;
        }
    }

    if (!failedNames.isEmpty()) {
        templates->clear();
        templateTargetIndexes->clear();
        if (errorMessage) {
            *errorMessage = QString("以下字符未找到对应图片，或图片读取失败：\n[ %1 ]")
                    .arg(failedNames.join(" "));
        }
        return false;
    }

    return !templates->empty();
}

bool Widget::loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    if (profile.resolvedAssetPathsByRole.isEmpty()) {
        return loadWordDigitTemplatesFromDir(profile.dirPath,
                                             baseNames,
                                             templates,
                                             templateTargetIndexes,
                                             errorMessage,
                                             true);
    }
    if (!templates || !templateTargetIndexes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("内部参数无效");
        }
        return false;
    }

    templates->clear();
    templateTargetIndexes->clear();
    if (baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标字符为空或解析失败");
        }
        return false;
    }

    ResolvedRecipeProfile resolvedProfile;
    resolvedProfile.profile = profile.recipeProfile;
    resolvedProfile.assetPathsByRole = profile.resolvedAssetPathsByRole;
    TemplateProfileLoadPlan loadPlan;
    QString planError;
    if (!buildTemplateProfileLoadPlan(resolvedProfile,
                                      baseNames,
                                      &loadPlan,
                                      &planError)) {
        if (errorMessage) {
            *errorMessage = planError;
        }
        return false;
    }
    if (!loadPlan.pendingTargetMessage.isEmpty()
            || loadPlan.characterTemplates.isEmpty()) {
        if (errorMessage) {
            *errorMessage = loadPlan.pendingTargetMessage.isEmpty()
                    ? QStringLiteral("未找到对应字符图片")
                    : loadPlan.pendingTargetMessage;
        }
        return false;
    }

    QStringList failedNames;
    for (const TemplateCharacterLoadItem &item : loadPlan.characterTemplates) {
        QFile file(item.absoluteFilePath);
        cv::Mat templateImage;
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray data = file.readAll();
            try {
                const std::vector<uchar> buffer(data.begin(), data.end());
                templateImage = cv::imdecode(buffer, cv::IMREAD_GRAYSCALE);
            } catch (...) {
                templateImage.release();
            }
        }

        if (templateImage.empty()) {
            failedNames.append(item.fileName);
            continue;
        }
        templates->push_back(templateImage);
        templateTargetIndexes->push_back(item.targetIndex);
    }

    if (!failedNames.isEmpty()) {
        templates->clear();
        templateTargetIndexes->clear();
        if (errorMessage) {
            *errorMessage = QStringLiteral("以下字符图片读取失败：\n[ %1 ]")
                    .arg(failedNames.join(QLatin1Char(' ')));
        }
        return false;
    }
    return !templates->empty();
}

bool Widget::loadWordTemplateProfileFromDir(const QString &dirPath,
                                            WordTemplateProfile *profile,
                                            QString *errorMessage)
{
    if (!profile) {
        if (errorMessage) *errorMessage = "内部模板对象为空";
        return false;
    }

    QDir templateDir(dirPath);
    if (!templateDir.exists()) {
        if (errorMessage) *errorMessage = "产品模板文件夹不存在";
        return false;
    }

    TemplatePrivateSettings privateSettings;
    QString privateError;
    if (!AppSettingsManager::loadTemplatePrivateSettings(templateDir.absolutePath(),
                                                         &privateSettings,
                                                         &privateError)) {
        if (errorMessage) *errorMessage = privateError;
        return false;
    }

    const QString yamlPath = templateDir.filePath("calibrate_config.yaml");
    CalibrationData calib;
    if (!QFileInfo::exists(yamlPath)
            || !calib.load(yamlPath.toLocal8Bit().toStdString())
            || calib.date_poly.empty()) {
        if (errorMessage) *errorMessage = "calibrate_config.yaml 中缺少有效喷码检测区域";
        return false;
    }
    if (currentDetectModeId() == BarcodeWordDetectionMode
            && calib.barcode_poly.size() != 4) {
        if (errorMessage) {
            *errorMessage = QString(
                        "使用旧版二维码区域格式，缺少有效 barcode_poly；"
                        "请重新制作稳定定位锚点、二维码区域和日期区域");
        }
        return false;
    }

    const QString trackingPath = templateDir.filePath("tracking_template.bmp");
    QFile trackingFile(trackingPath);
    cv::Mat trackingTemplate;
    if (trackingFile.open(QIODevice::ReadOnly)) {
        const QByteArray data = trackingFile.readAll();
        try {
            std::vector<uchar> buffer(data.begin(), data.end());
            trackingTemplate = cv::imdecode(buffer, cv::IMREAD_COLOR);
        } catch (...) {
            qDebug() << "[WORD_TEMPLATE] tracking template decode failed:" << trackingPath;
        }
    }
    if (trackingTemplate.empty()) {
        if (errorMessage) *errorMessage = "tracking_template.bmp 缺失或无法读取";
        return false;
    }

    const bool trackingBoxValid = privateSettings.trackingBox.width > 0
            && privateSettings.trackingBox.height > 0;
    if (!trackingBoxValid) {
        if (errorMessage) *errorMessage = "模板私有配置中的定位区域无效";
        return false;
    }
    if (!privateSettings.hasValidBoxes) {
        privateSettings.hasValidBoxes = true;
        QString repairError;
        if (!AppSettingsManager::saveTemplatePrivateSettings(templateDir.absolutePath(),
                                                             privateSettings,
                                                             &repairError)) {
            if (errorMessage) {
                *errorMessage = QString("定位区域有效，但 hasValidBoxes 自动修复失败：%1").arg(repairError);
            }
            return false;
        }
    }

    WordTemplateProfile loadedProfile;
    loadedProfile.name = templateDir.dirName();
    loadedProfile.dirPath = templateDir.absolutePath();
    loadedProfile.trackingTemplate = trackingTemplate;
    loadedProfile.barcodePoly = calib.barcode_poly;
    loadedProfile.datePoly = calib.date_poly;
    loadedProfile.settings = privateSettings;
    refreshWordTemplateRecipeProfile(&loadedProfile);

    const QStringList baseNames = parseTemplateTargetUnits(privateSettings.targetText);
    loadedProfile.targetCount = baseNames.size();
    QString pendingMessage;
    if (privateSettings.targetText.trimmed().isEmpty() || baseNames.isEmpty()) {
        pendingMessage = "目标字符为空或解析失败，目标字符待设置";
    } else {
        QString digitError;
        if (!loadWordDigitTemplatesFromDir(loadedProfile.dirPath,
                                           baseNames,
                                           &loadedProfile.digitTemplates,
                                           &loadedProfile.digitTemplateTargetIndexes,
                                           &digitError)) {
            pendingMessage = QString("字符图片不完整，目标字符待重新确认：%1").arg(digitError);
        }
    }

    refreshWordTemplateProfileDigitCache(
                &loadedProfile);
    *profile = loadedProfile;
    if (errorMessage) *errorMessage = pendingMessage;
    return true;
}

bool Widget::loadWordTemplateProfileFromRecipeSelection(
        const RecipeSelection &selection,
        int profileIndex,
        WordTemplateProfile *profile,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!profile) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal template profile output is null.");
        }
        return false;
    }
    if (!selection.recipe
            || (selection.recipe->detectionMode != DetectionMode::Word
                && selection.recipe->detectionMode
                   != DetectionMode::BarcodeWord)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Selected recipe is not a word-family recipe.");
        }
        return false;
    }
    if (profileIndex < 0 || profileIndex >= selection.profiles.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Selected recipe profile index is invalid.");
        }
        return false;
    }

    const ResolvedRecipeProfile &resolvedProfile =
            selection.profiles.at(profileIndex);
    const QStringList targetUnits =
            parseTemplateTargetUnits(resolvedProfile.profile.targetText);
    TemplateProfileLoadPlan loadPlan;
    if (!buildTemplateProfileLoadPlan(resolvedProfile,
                                      targetUnits,
                                      &loadPlan,
                                      errorMessage)) {
        return false;
    }

    CalibrationData calibration;
    if (!calibration.load(
                loadPlan.calibrationPath.toLocal8Bit().toStdString())
            || calibration.date_poly.empty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Selected recipe calibration has no valid date polygon.");
        }
        return false;
    }
    if (selection.recipe->detectionMode == DetectionMode::BarcodeWord
            && calibration.barcode_poly.size() != 4) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Selected barcode recipe must contain a four-point barcode polygon.");
        }
        return false;
    }

    cv::Mat trackingTemplate;
    QFile trackingFile(loadPlan.trackingTemplatePath);
    if (trackingFile.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = trackingFile.readAll();
        try {
            const std::vector<uchar> buffer(bytes.begin(), bytes.end());
            trackingTemplate = cv::imdecode(buffer, cv::IMREAD_COLOR);
        } catch (...) {
            trackingTemplate.release();
        }
    }
    if (trackingTemplate.empty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Selected recipe tracking template cannot be decoded.");
        }
        return false;
    }

    WordTemplateProfile loadedProfile;
    loadedProfile.name = loadPlan.profile.name;
    loadedProfile.dirPath = selection.recipeDirectoryPath;
    loadedProfile.trackingTemplate = trackingTemplate;
    loadedProfile.barcodePoly = calibration.barcode_poly;
    loadedProfile.datePoly = calibration.date_poly;
    loadedProfile.settings =
            templatePrivateSettingsFromRecipeProfile(loadPlan.profile);
    loadedProfile.recipeProfile = loadPlan.profile;
    loadedProfile.resolvedAssetPathsByRole =
            resolvedProfile.assetPathsByRole;
    loadedProfile.targetCount = loadPlan.targetUnits.size();
    loadedProfile.recipeAssetManifest.profileAssetKeys =
            loadPlan.profile.assetKeys;

    for (auto it = loadPlan.profile.assetKeys.constBegin();
         it != loadPlan.profile.assetKeys.constEnd();
         ++it) {
        const QString assetKey = it.value();
        const QString sourcePath =
                resolvedProfile.assetPathsByRole.value(it.key());
        const QString relativePath =
                selection.recipe->assets.value(assetKey);
        if (!sourcePath.isEmpty()) {
            loadedProfile.recipeAssetManifest.assetSourcePaths.insert(
                        assetKey,
                        sourcePath);
        }
        if (!relativePath.isEmpty()) {
            loadedProfile.recipeAssetManifest.recipeAssets.insert(
                        assetKey,
                        relativePath);
        }
    }

    QString pendingMessage = loadPlan.pendingTargetMessage;
    if (pendingMessage.isEmpty()) {
        QStringList failedCharacterFiles;
        for (const TemplateCharacterLoadItem &item :
             loadPlan.characterTemplates) {
            QFile characterFile(item.absoluteFilePath);
            cv::Mat characterTemplate;
            if (characterFile.open(QIODevice::ReadOnly)) {
                const QByteArray bytes = characterFile.readAll();
                try {
                    const std::vector<uchar> buffer(bytes.begin(), bytes.end());
                    characterTemplate =
                            cv::imdecode(buffer, cv::IMREAD_GRAYSCALE);
                } catch (...) {
                    characterTemplate.release();
                }
            }

            if (characterTemplate.empty()) {
                failedCharacterFiles.append(item.fileName);
                continue;
            }
            loadedProfile.digitTemplates.push_back(characterTemplate);
            loadedProfile.digitTemplateTargetIndexes.push_back(
                        item.targetIndex);
        }

        if (!failedCharacterFiles.isEmpty()) {
            loadedProfile.digitTemplates.clear();
            loadedProfile.digitTemplateTargetIndexes.clear();
            pendingMessage = QStringLiteral(
                        "Character assets cannot be decoded: %1")
                    .arg(failedCharacterFiles.join(QLatin1Char(' ')));
        }
    }

    refreshWordTemplateProfileDigitCache(&loadedProfile);
    *profile = loadedProfile;
    if (errorMessage) {
        *errorMessage = pendingMessage;
    }
    return true;
}

bool Widget::loadWordTemplateProfilesFromRecipeSelection(
        const RecipeSelection &selection,
        std::vector<WordTemplateProfile> *profiles,
        QStringList *pendingMessages,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!profiles) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Internal template profile collection output is null.");
        }
        return false;
    }

    TemplateRecipeLoadPlan recipeLoadPlan;
    if (!buildTemplateRecipeLoadPlan(selection,
                                     &recipeLoadPlan,
                                     errorMessage)) {
        return false;
    }

    std::vector<WordTemplateProfile> loadedProfiles;
    loadedProfiles.reserve(
                static_cast<size_t>(recipeLoadPlan.profiles.size()));
    QStringList loadedPendingMessages;
    for (int profileIndex = 0;
         profileIndex < recipeLoadPlan.profiles.size();
         ++profileIndex) {
        WordTemplateProfile loadedProfile;
        QString profileMessage;
        if (!loadWordTemplateProfileFromRecipeSelection(
                    selection,
                    profileIndex,
                    &loadedProfile,
                    &profileMessage)) {
            if (errorMessage) {
                const QString profileName =
                        recipeLoadPlan.profiles.at(profileIndex)
                        .profile.name.trimmed().isEmpty()
                        ? QString::number(profileIndex + 1)
                        : recipeLoadPlan.profiles.at(profileIndex)
                          .profile.name;
                *errorMessage = QStringLiteral(
                            "Recipe profile %1 cache cannot be assembled: %2")
                        .arg(profileName, profileMessage);
            }
            return false;
        }

        if (!profileMessage.trimmed().isEmpty()) {
            const QString profileName = loadedProfile.name.trimmed().isEmpty()
                    ? QString::number(profileIndex + 1)
                    : loadedProfile.name;
            loadedPendingMessages.append(
                        QStringLiteral("%1: %2")
                        .arg(profileName, profileMessage));
        }
        loadedProfiles.push_back(loadedProfile);
    }

    profiles->swap(loadedProfiles);
    if (pendingMessages) {
        *pendingMessages = loadedPendingMessages;
    }
    return true;
}

void Widget::refreshWordTemplateProfileDigitCache(
    WordTemplateProfile *profile) const
{
    if (!profile) {
        return;
    }

    profile->preparedDigitTemplates =
            TemplateMatch::prepareDigitTemplates(
                profile->digitTemplates);
}

std::vector<Widget::WordTemplateProfile>
Widget::createWordTemplateRunSnapshot() const
{
    std::vector<WordTemplateProfile> snapshot;
    snapshot.reserve(m_wordTemplateProfiles.size());
    for (const WordTemplateProfile &sourceProfile : m_wordTemplateProfiles) {
        WordTemplateProfile runtimeProfile = sourceProfile;
        runtimeProfile.trackingTemplate =
                sourceProfile.trackingTemplate.clone();
        runtimeProfile.digitTemplates.clear();
        runtimeProfile.digitTemplates.reserve(
                    sourceProfile.digitTemplates.size());
        for (const cv::Mat &digitTemplate : sourceProfile.digitTemplates) {
            runtimeProfile.digitTemplates.push_back(
                        digitTemplate.clone());
        }
        refreshWordTemplateProfileDigitCache(&runtimeProfile);
        snapshot.push_back(std::move(runtimeProfile));
    }
    return snapshot;
}

void Widget::clearWordTemplateRunSnapshot()
{
    m_wordTemplateRunActive = false;
    m_runningWordTemplateProfiles.clear();
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

    applyTissueRecipeParametersToThreads(threshold);
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(threshold, 'f', 3));
    if (showMessage) {
        showParameterInfo("提示", "粗糙度阈值设置成功");
    }
    return true;
}

void Widget::refreshWordTemplateRecipeProfile(
    WordTemplateProfile *profile) const
{
    if (!profile) {
        return;
    }

    const QMap<QString, QString> assetKeys =
            profile->recipeProfile.assetKeys;
    profile->recipeProfile =
            recipeProfileFromTemplatePrivateSettings(profile->name,
                                                     profile->settings,
                                                     assetKeys);
}

bool Widget::saveWordTemplatePrivateSettings(
        int profileIndex,
        const TemplatePrivateSettings &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前产品模板Profile无效。");
        }
        return false;
    }

    const WordTemplateProfile &profile =
            m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
    if (profile.resolvedAssetPathsByRole.isEmpty()) {
        return AppSettingsManager::saveTemplatePrivateSettings(
                    profile.dirPath,
                    settings,
                    errorMessage);
    }

    if (!m_wordTemplateRecipeEditSession.isActive()
            || profileIndex
               >= m_wordTemplateRecipeEditSession.recipe().profiles.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前已发布配方没有有效的编辑会话。");
        }
        return false;
    }

    const RecipeProfile candidateProfile =
            recipeProfileFromTemplatePrivateSettings(
                profile.name,
                settings,
                profile.recipeProfile.assetKeys);
    return TemplateRecipeWorkflow::validateProfileUpdate(
                m_wordTemplateRecipeEditSession,
                profileIndex,
                candidateProfile,
                errorMessage);
}

void Widget::refreshWordTemplateRecipeAssets()
{
    for (int profileIndex = 0;
         profileIndex < static_cast<int>(m_wordTemplateProfiles.size());
         ++profileIndex) {
        WordTemplateProfile &profile =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        profile.recipeAssetManifest =
                buildTemplateProfileAssetManifest(profile.dirPath,
                                                  profileIndex);
        profile.recipeProfile.assetKeys =
                profile.recipeAssetManifest.profileAssetKeys;
    }
}

void Widget::prepareWordTemplateRecipeDraft(
        const WordTemplateProfile &profile)
{
    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateRecipeEditSession.reset();

    DetectionMode detectionMode;
    if (!detectionModeFromId(currentDetectModeId(), &detectionMode)
            || (detectionMode != DetectionMode::Word
                && detectionMode != DetectionMode::BarcodeWord)
            || profile.dirPath.trimmed().isEmpty()) {
        return;
    }

    const QString displayName = profile.name.trimmed().isEmpty()
            ? QDir(profile.dirPath).dirName()
            : profile.name.trimmed();
    const ProductRecipe recipeHeader =
            createProductRecipe(displayName, detectionMode);
    QString sessionError;
    if (!m_wordTemplateRecipeDraftSession.begin(recipeHeader,
                                                profile.dirPath,
                                                &sessionError)) {
        qWarning() << "[RECIPE_DRAFT] cannot begin word recipe draft:"
                   << sessionError;
    }
}

bool Widget::publishWordTemplateRecipeDraft(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_wordTemplateRecipeDraftSession.isActive()) {
        return true;
    }
    if (m_wordTemplateProfiles.size() != 1) {
        if (errorMessage) {
            *errorMessage = "本次新建配方只允许包含一个产品模板。";
        }
        return false;
    }

    const WordTemplateProfile &profile = m_wordTemplateProfiles.front();
    TemplateRecipeProfileSource profileSource;
    profileSource.profile = profile.recipeProfile;
    profileSource.assetManifest = profile.recipeAssetManifest;
    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);

    const RecipeStore store(QDir(AppSettingsManager::globalDataDirPath())
                            .filePath("recipes"));
    RecipeSelection publishedSelection;
    TemplateRecipeWorkflowFailureStage failureStage =
            TemplateRecipeWorkflowFailureStage::None;
    if (!TemplateRecipeWorkflow::publishDraftAndBeginEdit(
                &m_wordTemplateRecipeDraftSession,
                &m_wordTemplateRecipeEditSession,
                store,
                profile.dirPath,
                profileSources,
                &publishedSelection,
                &failureStage,
                errorMessage)) {
        if (failureStage == TemplateRecipeWorkflowFailureStage::EditSession
                && errorMessage) {
            *errorMessage = QString(
                        "产品配方已经发布，但无法建立后续编辑会话：%1")
                    .arg(*errorMessage);
        }
        return false;
    }

    m_publishedRecipeIdsByMode.insert(currentDetectModeId(),
                                      publishedSelection.recipe->recipeId);
    m_appliedGlobalSettings.publishedRecipeIdsByMode =
            m_publishedRecipeIdsByMode;
    saveSettings(false);

    qDebug() << "[RECIPE_PUBLISH] published word recipe:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath;
    return true;
}

bool Widget::publishWordTemplateRecipeEdit(int profileIndex,
                                           QString *errorMessage)
{
    QVector<int> profileIndexes;
    profileIndexes.append(profileIndex);
    return publishWordTemplateRecipeEdits(profileIndexes, errorMessage);
}

bool Widget::publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_wordTemplateRecipeEditSession.isActive()) {
        return true;
    }
    QVector<RecipeProfile> updatedProfiles =
            m_wordTemplateRecipeEditSession.recipe().profiles;
    for (int profileIndex : profileIndexes) {
        if (profileIndex < 0
                || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())
                || profileIndex >= updatedProfiles.size()) {
            if (errorMessage) {
                *errorMessage = "当前配方编辑Profile无效。";
            }
            return false;
        }

        updatedProfiles[profileIndex] =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)]
                .recipeProfile;
    }

    const RecipeStore store(QDir(AppSettingsManager::globalDataDirPath())
                            .filePath("recipes"));
    RecipeSelection publishedSelection;
    if (!TemplateRecipeWorkflow::republishProfiles(
                &m_wordTemplateRecipeEditSession,
                store,
                updatedProfiles,
                &publishedSelection,
                nullptr,
                errorMessage)) {
        return false;
    }

    qDebug() << "[RECIPE_PUBLISH] republished word recipe:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath;
    return true;
}

void Widget::applyTissueRecipeParametersToThreads(
        double roughnessThreshold)
{
    TissueRecipeParameters parameters;
    parameters.roughnessThreshold = roughnessThreshold;
    if (myThread) {
        myThread->setTissueRecipeParameters(parameters);
    }
    if (cameraThread) {
        cameraThread->setTissueRecipeParameters(parameters);
    }
}

bool Widget::queryCameraExposureRange(int *minimumValue,
                                      int *maximumValue,
                                      double *currentValue,
                                      QString *errorMessage)
{
    if (!m_cameraDevice) {
        if (errorMessage) {
            *errorMessage = "相机未初始化，无法读取曝光范围";
        }
        return false;
    }

    CameraFloatValue exposureInfo;
    const CameraOperationResult result =
            m_cameraDevice->getFloatValue(
                "ExposureTime",
                &exposureInfo);
    if (!result.isSuccess()) {
        if (errorMessage) {
            *errorMessage = QString("读取相机曝光范围失败，错误码：%1")
                    .arg(result.nativeErrorCode);
        }
        return false;
    }

    const double rawMinimum =
            static_cast<double>(exposureInfo.minimumValue);
    const double rawMaximum =
            static_cast<double>(exposureInfo.maximumValue);
    const double rawCurrent =
            static_cast<double>(exposureInfo.currentValue);
    if (!std::isfinite(rawMinimum)
            || !std::isfinite(rawMaximum)
            || !std::isfinite(rawCurrent)) {
        if (errorMessage) {
            *errorMessage = "相机返回的曝光范围无效";
        }
        return false;
    }

    const double integerMinimum = std::ceil(rawMinimum);
    const double integerMaximum = std::floor(rawMaximum);
    if (integerMinimum > integerMaximum
            || integerMinimum < static_cast<double>((std::numeric_limits<int>::min)())
            || integerMaximum > static_cast<double>((std::numeric_limits<int>::max)())) {
        if (errorMessage) {
            *errorMessage = QString("相机曝光范围无法转换为整数：%1 ~ %2")
                    .arg(rawMinimum)
                    .arg(rawMaximum);
        }
        return false;
    }

    if (minimumValue) {
        *minimumValue = static_cast<int>(integerMinimum);
    }
    if (maximumValue) {
        *maximumValue = static_cast<int>(integerMaximum);
    }
    if (currentValue) {
        *currentValue = rawCurrent;
    }
    return true;
}

bool Widget::applyCameraExposureValue(int exposureValue, QString *errorMessage)
{
    int minimumValue = 0;
    int maximumValue = 0;
    if (!queryCameraExposureRange(&minimumValue, &maximumValue, nullptr, errorMessage)) {
        return false;
    }

    {
        QSignalBlocker blocker(ui->spinBox);
        ui->spinBox->setRange(minimumValue, maximumValue);
    }

    if (exposureValue < minimumValue || exposureValue > maximumValue) {
        if (errorMessage) {
            *errorMessage = QString("曝光值 %1 超出当前相机允许范围：%2 ~ %3")
                    .arg(exposureValue)
                    .arg(minimumValue)
                    .arg(maximumValue);
        }
        return false;
    }

    CameraOperationResult result = m_cameraDevice->setFloatValue(
                "ExposureTime",
                static_cast<float>(exposureValue));
    if (!result.isSuccess()) {
        if (errorMessage) {
            *errorMessage = QString("相机曝光设置失败，错误码：%1")
                    .arg(result.nativeErrorCode);
        }
        return false;
    }

    CameraFloatValue readBackInfo;
    result = m_cameraDevice->getFloatValue(
                "ExposureTime",
                &readBackInfo);
    if (!result.isSuccess()) {
        if (errorMessage) {
            *errorMessage = QString("相机曝光写入后回读失败，错误码：%1")
                    .arg(result.nativeErrorCode);
        }
        return false;
    }

    const double actualValue =
            static_cast<double>(readBackInfo.currentValue);
    if (!std::isfinite(actualValue)
            || std::fabs(actualValue - static_cast<double>(exposureValue)) > 0.5) {
        if (errorMessage) {
            *errorMessage = QString("相机曝光写入值与实际值不一致：设置 %1，实际 %2")
                    .arg(exposureValue)
                    .arg(actualValue);
        }
        return false;
    }

    qDebug() << "SetExposureTime verified:"
             << exposureValue
             << "range:" << minimumValue << "~" << maximumValue
             << "actual:" << actualValue;
    return true;
}

bool Widget::applySavedCameraExposure(QString *adjustmentMessage,
                                      QString *errorMessage)
{
    int minimumValue = 0;
    int maximumValue = 0;
    if (!queryCameraExposureRange(&minimumValue, &maximumValue, nullptr, errorMessage)) {
        return false;
    }

    const int savedValue = m_appliedGlobalSettings.cameraExposure;
    const int adjustedValue = qBound(minimumValue, savedValue, maximumValue);

    {
        QSignalBlocker blocker(ui->spinBox);
        ui->spinBox->setRange(minimumValue, maximumValue);
        ui->spinBox->setValue(adjustedValue);
    }

    if (!applyCameraExposureValue(adjustedValue, errorMessage)) {
        return false;
    }

    if (adjustedValue != savedValue) {
        m_appliedGlobalSettings.cameraExposure = adjustedValue;
        if (!saveSettings(false)) {
            m_appliedGlobalSettings.cameraExposure = savedValue;
            if (errorMessage) {
                *errorMessage = "曝光值已根据相机范围调整，但公共配置保存失败";
            }
            return false;
        }
        if (adjustmentMessage) {
            *adjustmentMessage =
                    QString("原曝光值 %1 超出当前相机允许范围（%2 ~ %3），已调整为 %4。")
                    .arg(savedValue)
                    .arg(minimumValue)
                    .arg(maximumValue)
                    .arg(adjustedValue);
        }
    }

    refreshGlobalSettingDirty("camera.exposure");
    return true;
}

bool Widget::applyCameraExposureFromUi(QStringList *errors, bool showSuccessMessage)
{
    if (!m_cameraDevice || !m_bOpenDevice) {
        const QString message = "未打开相机，无法设置曝光！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("警告", message);
        return false;
    }

    const int exposureValue = ui->spinBox->value();
    QString errorMessage;
    if (!applyCameraExposureValue(exposureValue, &errorMessage)) {
        const QString message = errorMessage.isEmpty()
                ? QString("相机曝光设置失败")
                : errorMessage;
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        refreshGlobalSettingDirty("camera.exposure");
        return false;
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", "相机曝光设置成功！");
    }
    return true;
}

bool Widget::applyCameraGainFromUi(QStringList *errors, bool showSuccessMessage)
{
    if (!m_cameraDevice || !m_bOpenDevice) {
        const QString message = "相机未初始化或未打开，无法设置增益！";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }

    CameraFloatValue gainInfo;
    CameraOperationResult result =
            m_cameraDevice->getFloatValue("Gain", &gainInfo);
    if (!result.isSuccess()) {
        const QString message =
                QString("无法获取相机增益支持的范围！错误码：%1")
                .arg(result.nativeErrorCode);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }

    int gainIntValue = 0;
    if (!parseIntValue(ui->lineEdit_14->text(), &gainIntValue)) {
        const QString message = QString("请输入有效的整数增益！当前相机允许范围：%1 ~ %2")
                .arg(gainInfo.minimumValue)
                .arg(gainInfo.maximumValue);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }

    const float gainValue = static_cast<float>(gainIntValue);
    if (gainValue < gainInfo.minimumValue
            || gainValue > gainInfo.maximumValue) {
        const QString message = QString("输入的增益值超出限制！当前相机允许范围：%1 ~ %2")
                .arg(gainInfo.minimumValue)
                .arg(gainInfo.maximumValue);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }

    result = m_cameraDevice->setFloatValue("Gain", gainValue);
    if (!result.isSuccess()) {
        const QString message =
                QString("相机增益设置失败！错误码：%1")
                .arg(result.nativeErrorCode);
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("提示", message);
        return false;
    }

    qDebug() << "SetGain success:" << gainValue;
    if (showSuccessMessage) {
        showParameterInfo("提示", "相机增益设置成功！");
    }
    return true;
}

bool Widget::applyCameraHardwareSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    bool ok = true;
    ok = applyCameraExposureFromUi(errors, showSuccessMessage) && ok;
    ok = applyCameraGainFromUi(errors, showSuccessMessage) && ok;

    if (ok) {
        updateAppliedGlobalSettingsFromUi(QStringList() << "camera.exposure" << "camera.gain");
        refreshGlobalSettingsDirty(QStringList() << "camera.exposure" << "camera.gain");
        saveSettings(false);
    }
    return ok;
}

bool Widget::applyRuntimeThreadSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    int thresholdValue = 0;
    if (!parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue)
            || thresholdValue < 0
            || thresholdValue > 100) {
        const QString message = "图像合格阈值必须是0到100之间的整数（单位：%）";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("参数错误", message);
        return false;
    }

    bool tissueThresholdOk = false;
    const double tissueThreshold = ui->lineEdit_tissueRoughnessThreshold->text().trimmed().toDouble(&tissueThresholdOk);
    if (!tissueThresholdOk || tissueThreshold <= 0.0) {
        const QString message = "纸巾检测粗糙度阈值必须是大于0的数字";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("参数错误", message);
        return false;
    }

    switch (ui->comboBox_2->currentIndex()) {
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
        break;
    }

    switch (ui->comboBox_5->currentIndex()) {
    case 1:
        colorchannel = 1;
        break;
    case 2:
        colorchannel = 2;
        break;
    case 3:
        colorchannel = 3;
        break;
    default:
        colorchannel = 0;
        break;
    }

    applyTissueRecipeParametersToThreads(tissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(tissueThreshold, 'f', 3));

    emit rotate(angleValue);
    emit choosechannel(colorchannel);
    emit ssim(thresholdValue);

    if (showSuccessMessage) {
        showParameterInfo("提示", "运行参数设置成功");
    }
    updateAppliedGlobalSettingsFromUi(QStringList()
                                      << "image.rotation"
                                      << "image.color_channel"
                                      << "tissue.roughness_threshold");
    refreshGlobalSettingsDirty(QStringList()
                               << "image.rotation"
                               << "image.color_channel"
                               << "tissue.roughness_threshold");
    saveSettings(false);
    return true;
}

bool Widget::applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage)
{
    PLCmode = ui->comboBox_3->currentIndex();

    if (!m_plcDevice || !m_plcDevice->isConnected()) {
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

    const std::uint8_t value =
            static_cast<std::uint8_t>(PLCmode == 0 ? 0 : 1);
    unsigned char mode_data[1] = {0};
    mode_data[0] = static_cast<unsigned char>(0xFF & value);

    const PlcOperationResult result = m_plcDevice->writeDbArea(
                1, 1032, 1, PlcDataWidth::Byte, mode_data);
    if (!result.isSuccess()) {
        const QString message = PLCmode == 0 ? "设置连续模式失败" : "设置间歇模式失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    if (showSuccessMessage) {
        showParameterInfo("提示", PLCmode == 0 ? "连续模式设置成功" : "间歇模式设置成功");
    }
    updateAppliedGlobalSettingFromUi("plc.trigger_mode");
    refreshGlobalSettingDirty("plc.trigger_mode");
    saveSettings(false);
    return true;
}

bool Widget::applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage)
{
    if (!m_plcDevice || !m_plcDevice->isConnected()) {
        const QString message = "PLC未连接！";
        if (showSuccessMessage) {
            if (errors) errors->append(message);
            showParameterWarning("警告", message);
            return false;
        }
        return true;
    }

    wrongindex = ui->lineEdit_12->text().toInt();

    std::uint16_t rejectTime = ui->lineEdit_8->text().toUInt();
    unsigned char rejectTimeData[2] = {0};
    rejectTimeData[1] = static_cast<unsigned char>(0xFF & rejectTime);
    rejectTimeData[0] = static_cast<unsigned char>((0xFF00 & rejectTime) >> 8);
    PlcOperationResult result = m_plcDevice->writeDbArea(
                1, 980, 2, PlcDataWidth::Word, rejectTimeData);
    if (!result.isSuccess()) {
        const QString message = "设置剔除时间失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    std::uint32_t rejectDistance = ui->lineEdit_7->text().toUInt();
    unsigned char rejectDistanceData[4] = {0};
    rejectDistanceData[3] = static_cast<unsigned char>(0xFF & rejectDistance);
    rejectDistanceData[2] = static_cast<unsigned char>((0xFF00 & rejectDistance) >> 8);
    rejectDistanceData[1] = static_cast<unsigned char>((0xFF0000 & rejectDistance) >> 16);
    rejectDistanceData[0] = static_cast<unsigned char>((0xFF000000 & rejectDistance) >> 24);
    result = m_plcDevice->writeDbArea(
                1, 920, 4, PlcDataWidth::DWord, rejectDistanceData);
    if (!result.isSuccess()) {
        const QString message = "设置剔除距离失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    std::uint16_t photoTime = ui->lineEdit_20->text().toUInt();
    unsigned char photoTimeData[2] = {0};
    photoTimeData[1] = static_cast<unsigned char>(0xFF & photoTime);
    photoTimeData[0] = static_cast<unsigned char>((0xFF00 & photoTime) >> 8);
    result = m_plcDevice->writeDbArea(
                1, 982, 2, PlcDataWidth::Word, photoTimeData);
    if (!result.isSuccess()) {
        const QString message = "设置拍照时间失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    std::uint32_t photoDistance = ui->lineEdit_6->text().toUInt();
    unsigned char photoDistanceData[4] = {0};
    photoDistanceData[3] = static_cast<unsigned char>(0xFF & photoDistance);
    photoDistanceData[2] = static_cast<unsigned char>((0xFF00 & photoDistance) >> 8);
    photoDistanceData[1] = static_cast<unsigned char>((0xFF0000 & photoDistance) >> 16);
    photoDistanceData[0] = static_cast<unsigned char>((0xFF000000 & photoDistance) >> 24);
    result = m_plcDevice->writeDbArea(
                1, 924, 4, PlcDataWidth::DWord, photoDistanceData);
    if (!result.isSuccess()) {
        const QString message = "设置拍照距离失败";
        if (errors) errors->append(message);
        if (showSuccessMessage) showParameterWarning("error", message);
        return false;
    }

    emit sendDataTo(ui->lineEdit_4->text());

    if (showSuccessMessage) {
        showParameterInfo("提示", "所有设置已经完成！");
    }
    updateAppliedGlobalSettingsFromUi(QStringList()
                                      << "plc.photo_distance"
                                      << "plc.photo_time"
                                      << "plc.camera_delay"
                                      << "plc.reject_distance"
                                      << "plc.reject_time"
                                      << "plc.reject_position");
    refreshGlobalSettingsDirty(QStringList()
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
        updateAppliedGlobalSettingFromUi("camera.exposure");
        refreshGlobalSettingDirty("camera.exposure");
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
    const QByteArray address = ui->lineEdit->text().toUtf8();

    const int rack = ui->lineEdit_2->text().toInt();
    const int slot = ui->lineEdit_3->text().toInt();
    const PlcOperationResult result =
            m_plcDevice->connectTo(address.constData(), rack, slot);

    if (result.isSuccess())
    {
        updateAppliedGlobalSettingsFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
        refreshGlobalSettingsDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
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
    const PlcOperationResult result = m_plcDevice->disconnect();

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

///**
// * @brief 写入延时按钮点击槽函数
// * @details 向PLC DB1.920写入DWORD值（延时时间）
// */
//void Widget::on_WriteVDpushButton_2_clicked()
//{
//    if (!m_plcDevice->isConnected())
//    {
//        return;
//    }

//    std::uint32_t value2 = ui->lineEdit_7->text().toUInt();
//    unsigned char delay_data[4] = {0};

//    // 大小端转换
//    delay_data[3] = (unsigned char)(0xFF & value2);
//    delay_data[2] = (unsigned char)((0xFF00 & value2) >> 8);
//    delay_data[1] = (unsigned char)((0xFF0000 & value2) >> 16);
//    delay_data[0] = (unsigned char)((0xFF000000 & value2) >> 24);

//    // 写入DB1.920
//    const PlcOperationResult result = m_plcDevice->writeDbArea(
//                1, 920, 4, PlcDataWidth::DWord, delay_data);
//    if (!result.isSuccess())
//    {
//        QMessageBox::warning(this, "error", "设置失败");
//    }
//    else
//    {
//        QMessageBox::information(this, "success", "设置成功");
//    }
//}

///**
// * @brief 写入延时时间按钮点击槽函数
// * @details 向PLC DB1.980写入WORD值（延时时间）
// */
//void Widget::on_WriteVDpushButton_3_clicked()
//{
//    if (!m_plcDevice->isConnected())
//    {
//        return;
//    }

//    std::uint16_t value4 = ui->lineEdit_8->text().toUInt();
//    unsigned char delay_time[2] = {0};

//    // 大小端转换
//    delay_time[1] = (unsigned char)(0xFF & value4);
//    delay_time[0] = (unsigned char)((0xFF00 & value4) >> 8);

//    // 写入DB1.980
//    const PlcOperationResult result = m_plcDevice->writeDbArea(
//                1, 980, 2, PlcDataWidth::Word, delay_time);
//    if (!result.isSuccess())
//    {
//        QMessageBox::warning(this, "error", "设置失败");
//    }
//    else
//    {
//        QMessageBox::information(this, "success", "设置成功");
//    }
//}

/**
 * @brief 写入批次时间按钮点击槽函数
 * @details 向PLC DB1.982写入WORD值（批次时间）
 */
void Widget::on_pushButton_8_clicked()
{
    QStringList errors;
    applyPlcRunSettingsFromUi(&errors, true);
}

/**
 * @brief cv::Mat转换为QImage
 * @param mat 输入的cv::Mat对象
 * @return QImage 转换后的QImage对象
 */
QImage Widget::cvMatToQImage(const cv::Mat &mat)
{
    if (mat.type() == CV_8UC1)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }
    else if (mat.type() == CV_8UC3)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    else if (mat.type() == CV_8UC4)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {
        qDebug() << "ERROR: Mat could not be converted to QImage.";
        return QImage();
    }
}



void Widget::on_cancel_clicked()
{
    qDebug() << "=== on_cancel_clicked() START ===";

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
            && !hasRunningInspectionThread()) {
        updateOperationUiState();
        return;
    }

    m_operationState = OperationState::Stopping;
    updateOperationUiState();
    m_barcodeWordRunActive = false;

    // Step 2: 请求线程停止，保留当前模板状态，便于再次启动
    if (myThread) {
        myThread->requestStop();
    }

    if (cameraThread) {
        cameraThread->requestStop();
    }

    // 🔥 Step 3: myThread - 保持原逻辑
    bool myThreadWasRunning = false;
    bool myThreadStopped = true;
    if (myThread && myThread->isRunning()) {
        myThreadWasRunning = true;
        myThread->stop();
        if (!myThread->wait(3000)) {
            qDebug() << "WARNING: myThread did not stop";
            myThreadStopped = false;
        } else {
            myThread->stopTracking();
        }
    }

    // 🔥 Step 4: cameraThread - 停止逻辑
    bool needRestartCamera = false;
    bool cameraThreadStopped = true;
    if (cameraThread != nullptr) {
        needRestartCamera = true;
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);

        cameraThread->requestStop();
        if (!cameraThread->wait(3000)) {
            qDebug() << "WARNING: cameraThread did not stop";
            cameraThreadStopped = false;
        } else {
            cameraThread->stopTracking();
            cameraThread->deleteLater();
            cameraThread = nullptr;
        }
    }

    if (!myThreadStopped || !cameraThreadStopped) {
        ui->statusLabel->setText("停止中，请稍后再关闭相机");
        isCollecting = true;
        m_operationState = OperationState::Stopping;
        updateOperationUiState();
        return;
    }

    // 🔥 Step 5: 如果cameraThread运行过，重启相机
    if ((needRestartCamera || myThreadWasRunning)
            && m_cameraDevice) {
        try {
            m_cameraDevice->close();
            m_bOpenDevice = false;

            QThread::msleep(100);

            const CameraOperationResult openResult =
                    m_cameraDevice->openDevice(0);

            if (openResult.isSuccess()) {
                m_bOpenDevice = true;
                m_cameraDevice->setEnumValue("TriggerMode", 1);
                m_cameraDevice->setEnumValue("TriggerSource", 7);
                QString adjustmentMessage;
                QString exposureError;
                if (!applySavedCameraExposure(&adjustmentMessage, &exposureError)) {
                    m_cameraDevice->close();
                    m_bOpenDevice = false;
                    {
                        QSignalBlocker blocker(ui->spinBox);
                        ui->spinBox->setRange(0, (std::numeric_limits<int>::max)());
                        ui->spinBox->setValue(m_appliedGlobalSettings.cameraExposure);
                    }
                    refreshGlobalSettingDirty("camera.exposure");
                    QMessageBox::warning(this,
                                         "警告",
                                         QString("停止识别后恢复相机曝光失败：\n%1")
                                         .arg(exposureError));
                } else {
                    m_cameraDevice->setFloatValue("TriggerDelay", 0);
                    m_cameraDevice->registerImageCallback();
                    m_cameraDevice->startGrabbing();
                    ui->statusLabel->setText("相机已打开");
                    if (!adjustmentMessage.isEmpty()) {
                        QMessageBox::information(this, "提示", adjustmentMessage);
                    }
                }
            }
        } catch (...) {}
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

    g_lastDrawResults.clear();
    g_lastPose = DetectionPose();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = 0;
    clearWordTemplateRunSnapshot();
    m_barcodeWordRunActive = false;

    first = false;
    x = 1;
    j = 1;
    judge = false;

    ui->statusLabel->setText("已停止");
    isCollecting = false;
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
    if (isWordFamilyMode(currentDetectModeId()))
    {
        if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
            showParameterWarning("提示", "请先停止检测后再修改模板字符");
            return;
        }

        if (m_wordTemplateProfiles.empty()) {
            showParameterInfoAsError("提示", "请先选择字库模板");
            return;
        }

        const int profileIndex = currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError("提示", "当前编辑模板无效");
            return;
        }

        WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        const QString newMubiaozifu = ui->dateEdit->toPlainText();

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        QString loadError;
        const QStringList baseNamesToFind = parseTemplateTargetUnits(newMubiaozifu);
        if (!newMubiaozifu.trimmed().isEmpty()
                && !loadWordDigitTemplatesFromProfile(
                    profile,
                    baseNamesToFind,
                    &tempTemplates,
                    &tempTemplateTargetIndexes,
                    &loadError)) {
            showParameterCritical("严重警告",
                QString("当前模板 [%1] 字符图片加载失败：\n%2\n\n本次更新已撤销。")
                .arg(profile.name)
                .arg(loadError));
            return;
        }

        TemplatePrivateSettings updatedSettings = profile.settings;
        updatedSettings.targetText = newMubiaozifu;
        QString saveError;
        if (!saveWordTemplatePrivateSettings(profileIndex,
                                             updatedSettings,
                                             &saveError)) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 的目标字符写入失败：\n%2")
                                  .arg(profile.name)
                                  .arg(saveError));
            return;
        }

        profile.settings = updatedSettings;
        refreshWordTemplateRecipeProfile(&profile);
        profile.targetCount = baseNamesToFind.size();
        profile.digitTemplates = tempTemplates;
        profile.digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        refreshWordTemplateProfileDigitCache(
                    &profile);
        refreshTemplateTargetTextDirty();

        const QString profileName = profile.name.isEmpty()
                ? QDir(profile.dirPath).dirName()
                : profile.name;
        QString recipePublishMessage;
        QString recipePublishWarning;
        if (m_wordTemplateRecipeEditSession.isActive()) {
            QString publishError;
            if (publishWordTemplateRecipeEdit(profileIndex, &publishError)) {
                recipePublishMessage = "\n产品配方已使用原配方编号重新发布。";
            } else {
                recipePublishWarning =
                        QString("目标字符已保存到当前模板，但产品配方重新发布失败：\n%1")
                        .arg(publishError);
            }
        }
        const QString message =
                QString("已更新产品模板 %1 的目标字符。\n其他产品模板未修改。%2")
                .arg(profileName)
                .arg(recipePublishMessage);
        if (recipePublishWarning.isEmpty()) {
            showParameterInfo("提示", message);
        } else {
            showParameterInfoWithRedWarning("提示",
                                            message,
                                            recipePublishWarning);
        }
        return;
    }

    if (isSingleTemplateRecipeMode(currentDetectModeId())
            && m_singleTemplateRecipeEditSession.isActive()) {
        if ((myThread && myThread->isRunning())
                || (cameraThread && cameraThread->isRunning())
                || isCollecting) {
            showParameterWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u8BF7\u5148\u505C\u6B62\u68C0\u6D4B\u540E\u518D\u4FEE\u6539\u76EE\u6807\u5B57\u7B26\u3002"));
            return;
        }

        const QString targetText = ui->dateEdit->toPlainText();
        std::vector<cv::Mat> candidateDigitTemplates;
        std::vector<int> candidateDigitTargetIndexes;
        if (currentDetectModeId() == QStringLiteral("stamp_detection")) {
            QString characterError;
            if (!loadSingleTemplateCharacterAssets(
                        m_singleTemplateResolvedAssetPathsByRole,
                        parseTemplateTargetUnits(targetText),
                        &candidateDigitTemplates,
                        &candidateDigitTargetIndexes,
                        &characterError)) {
                showParameterCritical(
                            QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                            QStringLiteral(
                                "\u5F53\u524D\u5DF2\u53D1\u5E03\u94A2\u5370\u914D\u65B9\u7684\u5B57\u7B26\u8D44\u4EA7\u4E0D\u5B8C\u6574\uFF1A\n%1\n\n"
                                "\u672C\u6B21\u66F4\u65B0\u5DF2\u64A4\u9500\u3002")
                            .arg(characterError));
                return;
            }
        }

        TemplatePrivateSettings settings =
                templatePrivateSettingsFromRecipeProfile(
                    m_singleTemplateRecipeEditSession.recipe()
                    .profiles.first());
        settings.targetText = targetText;
        QString publishError;
        if (!republishSingleTemplateRecipeSettings(settings,
                                                   &publishError)) {
            showParameterInfoWithRedWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral(
                            "\u76EE\u6807\u5B57\u7B26\u672A\u751F\u6548\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u91CD\u65B0\u53D1\u5E03\u5931\u8D25\uFF1A\n%1")
                        .arg(publishError));
            return;
        }

        if (currentDetectModeId() == QStringLiteral("stamp_detection")) {
            digitTemplates.swap(candidateDigitTemplates);
            digitTemplateTargetIndexes.swap(
                        candidateDigitTargetIndexes);
        }
        clearTemplateTargetTextDirty();
        showParameterInfo(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u76EE\u6807\u5B57\u7B26\u8BBE\u7F6E\u6210\u529F\uFF0C"
                        "\u4EA7\u54C1\u914D\u65B9\u5DF2\u4F7F\u7528\u539F\u914D\u65B9\u7F16\u53F7\u91CD\u65B0\u53D1\u5E03\u3002"));
        return;
    }

    if (ui->comboBox_4->currentIndex() == 0)
    {
        // 1. 检查是否存在有效的模板路径
        if (currentTemplateDirPath.isEmpty()) {
            showParameterInfoAsError("提示", "请先选择产品模板文件夹");
            return;
        }
        // 2. 读取当前修改后的目标字符
        QString newMubiaozifu = ui->dateEdit->toPlainText();
        if (newMubiaozifu.isEmpty()) {
            digitTemplates.clear();
            digitTemplateTargetIndexes.clear();
            showParameterInfoAsError("提示", "目标字符为空，已清空模板");
            return;
        }

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        QString loadError;
        const QStringList baseNamesToFind = parseTemplateTargetUnits(newMubiaozifu);
        const bool includeVariantTemplates = false;

        if (!loadWordDigitTemplatesFromDir(currentTemplateDirPath,
                                           baseNamesToFind,
                                           &tempTemplates,
                                           &tempTemplateTargetIndexes,
                                           &loadError,
                                           includeVariantTemplates)) {
            // 如果有任何图片读取失败或丢失，绝不更新到全局的 digitTemplates，同时给出严厉警告
            showParameterCritical("严重警告",
                QString("%1\n\n请检查产品模板文件夹内的字符图片是否存在或是否损坏（支持中文，无需关心后缀和大小写）！\n本次更新已撤销。")
                .arg(loadError));
            return;
        }

        // 5. 全部成功后，再更新到全局容器
        digitTemplates = tempTemplates;
        digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        showParameterInfo("提示",
                          QString("目标字符确认成功，目标字符 %1 个，字符模板图 %2 张！")
                          .arg(baseNamesToFind.size())
                          .arg(static_cast<int>(digitTemplates.size())));
    }
    else{
     showParameterInfo("提示", "目标字符确认成功");
    }


}



void Widget::on_batchTextsure_btn_clicked()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        on_textsure_btn_clicked();
        return;
    }

    if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
        showParameterWarning("提示", "请先停止检测后再批量修改模板字符");
        return;
    }

    if (m_wordTemplateProfiles.empty()) {
        showParameterInfoAsError("提示", "请先选择字库模板");
        return;
    }

    const QString newMubiaozifu = ui->dateEdit->toPlainText();
    const bool needLoadDigitTemplates = !newMubiaozifu.trimmed().isEmpty();
    QStringList baseNamesToFind;
    if (needLoadDigitTemplates) {
        baseNamesToFind = parseTemplateTargetUnits(newMubiaozifu);
        if (baseNamesToFind.isEmpty()) {
            showParameterInfoAsError("提示", "目标字符解析失败");
            return;
        }
    }

    const int oldProfileIndex = currentWordTemplateProfileIndex();
    int successCount = 0;
    QStringList failedMessages;

    for (int profileIndex = 0;
         profileIndex < static_cast<int>(m_wordTemplateProfiles.size());
         ++profileIndex) {
        WordTemplateProfile &profile =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        const QString profileName = profile.name.isEmpty()
                ? QDir(profile.dirPath).dirName()
                : profile.name;

        QDir directory(profile.dirPath);
        if (profile.dirPath.isEmpty() || !directory.exists()) {
            failedMessages.append(QString("%1：产品模板文件夹不存在").arg(profileName));
            continue;
        }

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        if (needLoadDigitTemplates) {
            QString loadError;
            if (!loadWordDigitTemplatesFromProfile(
                        profile,
                        baseNamesToFind,
                        &tempTemplates,
                        &tempTemplateTargetIndexes,
                        &loadError)) {
                failedMessages.append(QString("%1：%2")
                                      .arg(profileName)
                                      .arg(loadError));
                continue;
            }
        }

        TemplatePrivateSettings updatedSettings = profile.settings;
        updatedSettings.targetText = newMubiaozifu;
        QString saveError;
        if (!saveWordTemplatePrivateSettings(profileIndex,
                                             updatedSettings,
                                             &saveError)) {
            failedMessages.append(QString("%1：目标字符写入失败，%2").arg(profileName).arg(saveError));
            continue;
        }

        profile.settings = updatedSettings;
        refreshWordTemplateRecipeProfile(&profile);
        profile.targetCount = baseNamesToFind.size();
        profile.digitTemplates = tempTemplates;
        profile.digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        refreshWordTemplateProfileDigitCache(
                    &profile);
        ++successCount;
    }

    if (oldProfileIndex >= 0) {
        setCurrentWordTemplateEditIndex(oldProfileIndex);
    }

    if (successCount == 0) {
        showParameterCritical("严重警告",
                              QString("所有模板的目标字符批量保存失败：\n%1")
                              .arg(failedMessages.join("\n")));
        return;
    }

    if (!failedMessages.isEmpty()) {
        QMessageBox::warning(this,
                             "提示",
                             QString("已成功保存 %1 个模板，失败 %2 个：\n%3")
                             .arg(successCount)
                             .arg(failedMessages.size())
                             .arg(failedMessages.join("\n")));
        return;
    }

    refreshTemplateTargetTextDirty();

    QString recipePublishMessage;
    QString recipePublishWarning;
    if (m_wordTemplateRecipeEditSession.isActive()) {
        QVector<int> profileIndexes;
        for (int profileIndex = 0;
             profileIndex < static_cast<int>(m_wordTemplateProfiles.size());
             ++profileIndex) {
            profileIndexes.append(profileIndex);
        }

        QString publishError;
        if (publishWordTemplateRecipeEdits(profileIndexes, &publishError)) {
            recipePublishMessage =
                    "\n产品配方已使用原配方编号重新发布。";
        } else {
            recipePublishWarning =
                    QString("目标字符已批量保存到当前模板，但产品配方重新发布失败：\n%1")
                    .arg(publishError);
        }
    }

    const QString message =
            QString("已将当前目标字符保存到所有已选择的产品模板。%1")
            .arg(recipePublishMessage);
    if (recipePublishWarning.isEmpty()) {
        showParameterInfo("提示", message);
    } else {
        showParameterInfoWithRedWarning("提示",
                                        message,
                                        recipePublishWarning);
    }
}

void Widget::on_batchImageThresholdButton_clicked()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        on_pushButton_3_clicked();
        return;
    }

    if ((myThread && myThread->isRunning())
            || (cameraThread && cameraThread->isRunning())
            || isCollecting) {
        showParameterWarning("提示", "请先停止检测后再批量修改模板阈值");
        return;
    }

    if (m_wordTemplateProfiles.empty()) {
        showParameterInfoAsError("提示", "请先选择字库模板");
        return;
    }

    const QString thresholdText = ui->lineEdit_yuzhi->text().trimmed();
    int thresholdValue = 0;
    if (!parseIntValue(thresholdText, &thresholdValue)
            || thresholdValue < 0
            || thresholdValue > 100) {
        showParameterWarning("参数错误",
                             "图像合格阈值必须是0到100之间的整数（单位：%）");
        return;
    }

    const int currentProfileIndex = currentWordTemplateProfileIndex();
    bool currentProfileUpdated = false;
    int successCount = 0;
    QStringList failedMessages;

    for (int i = 0; i < static_cast<int>(m_wordTemplateProfiles.size()); ++i) {
        WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(i)];
        const QString profileName = profile.name.isEmpty()
                ? QDir(profile.dirPath).dirName()
                : profile.name;

        QDir directory(profile.dirPath);
        if (profile.dirPath.isEmpty() || !directory.exists()) {
            failedMessages.append(QString("%1：产品模板文件夹不存在").arg(profileName));
            continue;
        }

        TemplatePrivateSettings updatedSettings = profile.settings;
        updatedSettings.imageThreshold = thresholdValue;
        QString saveError;
        if (!saveWordTemplatePrivateSettings(i,
                                             updatedSettings,
                                             &saveError)) {
            failedMessages.append(
                        QString("%1：图像合格阈值写入失败，%2")
                        .arg(profileName)
                        .arg(saveError));
            continue;
        }

        profile.settings = updatedSettings;
        refreshWordTemplateRecipeProfile(&profile);
        if (i == currentProfileIndex) {
            currentProfileUpdated = true;
        }
        ++successCount;
    }

    if (currentProfileUpdated) {
        emit ssim(thresholdValue);
    }
    refreshTemplateImageThresholdDirty();

    if (successCount == 0) {
        showParameterCritical(
                    "严重警告",
                    QString("所有模板的图像合格阈值批量设置失败：\n%1")
                    .arg(failedMessages.join("\n")));
        return;
    }

    if (!failedMessages.isEmpty()) {
        QMessageBox::warning(
                    this,
                    "提示",
                    QString("已成功设置 %1 个模板，失败 %2 个：\n%3")
                    .arg(successCount)
                    .arg(failedMessages.size())
                    .arg(failedMessages.join("\n")));
        return;
    }

    QString recipePublishMessage;
    QString recipePublishWarning;
    if (m_wordTemplateRecipeEditSession.isActive()) {
        QVector<int> profileIndexes;
        for (int profileIndex = 0;
             profileIndex < static_cast<int>(m_wordTemplateProfiles.size());
             ++profileIndex) {
            profileIndexes.append(profileIndex);
        }

        QString publishError;
        if (publishWordTemplateRecipeEdits(profileIndexes, &publishError)) {
            recipePublishMessage =
                    "\n产品配方已使用原配方编号重新发布。";
        } else {
            recipePublishWarning =
                    QString("图像阈值已批量保存到当前模板，但产品配方重新发布失败：\n%1")
                    .arg(publishError);
        }
    }

    const QString message =
            QString("已将当前图像合格阈值保存到所有已选择的产品模板。%1")
            .arg(recipePublishMessage);
    if (recipePublishWarning.isEmpty()) {
        showParameterInfo("提示", message);
    } else {
        showParameterInfoWithRedWarning("提示",
                                        message,
                                        recipePublishWarning);
    }
}





/**
 * @brief QImage转换为cv::Mat指针
 * @param image 输入的QImage对象
 * @return cv::Mat* 转换后的Mat指针
 */
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

///**
// * @brief 延时确定按钮点击槽函数
// * @details 设置相机采集延时
// */
//void Widget::on_delayButton_clicked()
//{
//    QString text = ui->lineEdit_4->text();
//    emit sendDataTo(text);
//    QMessageBox::information(this, "提示", "相机延时设置成功");
//}

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
    clearWordTemplateRunSnapshot();
    m_barcodeWordRunActive = false;
    isCollecting = false;
    if (m_templateCaptureAttentionTimer) {
        m_templateCaptureAttentionTimer->stop();
    }

    // 立即废弃当前模板取景会话，禁止迟到帧继续进入UI。
    ++m_templatePreviewSessionId;
    m_templateCaptureState = TemplateCaptureState::Idle;
    m_lastTemplatePreviewFrame.release();

    if (myThread) {
        disconnect(myThread, nullptr, this, nullptr);
        disconnect(this, nullptr, myThread, nullptr);
        myThread->setTemplatePreviewMode(
                    false,
                    m_templatePreviewSessionId);
        myThread->requestStop();
        myThread->stop();
    }
    if (cameraThread) {
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);
        cameraThread->requestStop();
    }
    if (m_cameraDevice) {
        m_cameraDevice->requestStop();
    }

    // 正常情况下线程会在下一次停止检查时立即退出。
    // 只做短暂等待，不再因为工作线程而阻止窗口关闭。
    const bool myThreadStopped =
            !myThread
            || !myThread->isRunning()
            || myThread->wait(500);
    const bool cameraThreadStopped =
            !cameraThread
            || !cameraThread->isRunning()
            || cameraThread->wait(500);

    // 只有工作线程已经停止时才主动释放相机，防止线程继续访问失效句柄。
    if (m_cameraDevice
            && m_bOpenDevice
            && myThreadStopped
            && cameraThreadStopped) {
        try {
            m_cameraDevice->close();
        } catch (...) {
        }
        m_bOpenDevice = false;
    }

    if (m_plcDevice && m_plcDevice->isConnected()) {
        m_plcDevice->disconnect();
    }

    try {
        cv::destroyAllWindows();
    } catch (...) {
    }
    saveSettings(false);
    event->accept();
}

/**
 * @brief 阈值确定按钮点击槽函数
 * @details 设置相似度判断阈值
 */
void Widget::on_pushButton_3_clicked()
{
    if (isWordFamilyMode(currentDetectModeId())) {
        if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
            showParameterWarning("提示", "请先停止检测后再修改模板阈值");
            return;
        }

        const int profileIndex = currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError("提示", "当前产品模板无效");
            return;
        }

        const QString thresholdText = ui->lineEdit_yuzhi->text().trimmed();
        int thresholdValue = 0;
        if (!parseIntValue(thresholdText, &thresholdValue)
                || thresholdValue < 0
                || thresholdValue > 100) {
            showParameterWarning("参数错误",
                                 "图像合格阈值必须是0到100之间的整数（单位：%）");
            return;
        }

        WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        TemplatePrivateSettings updatedSettings = profile.settings;
        updatedSettings.imageThreshold = thresholdValue;
        QString saveError;
        if (!saveWordTemplatePrivateSettings(profileIndex,
                                             updatedSettings,
                                             &saveError)) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 的图像阈值写入失败：\n%2")
                                  .arg(profile.name)
                                  .arg(saveError));
            return;
        }

        profile.settings = updatedSettings;
        refreshWordTemplateRecipeProfile(&profile);
        emit ssim(thresholdValue);
        refreshTemplateImageThresholdDirty();

        QString recipePublishMessage;
        QString recipePublishWarning;
        if (m_wordTemplateRecipeEditSession.isActive()) {
            QString publishError;
            if (publishWordTemplateRecipeEdit(profileIndex, &publishError)) {
                recipePublishMessage =
                        "\n产品配方已使用原配方编号重新发布。";
            } else {
                recipePublishWarning =
                        QString("图像阈值已保存到当前模板，但产品配方重新发布失败：\n%1")
                        .arg(publishError);
            }
        }

        const QString message =
                QString("模板 [%1] 图像阈值设置成功：%2%3")
                .arg(profile.name)
                .arg(thresholdText)
                .arg(recipePublishMessage);
        if (recipePublishWarning.isEmpty()) {
            showParameterInfo("提示", message);
        } else {
            showParameterInfoWithRedWarning("提示",
                                            message,
                                            recipePublishWarning);
        }
        return;
    }

    if (isSingleTemplateRecipeMode(currentDetectModeId())
            && m_singleTemplateRecipeEditSession.isActive()) {
        if ((myThread && myThread->isRunning())
                || (cameraThread && cameraThread->isRunning())
                || isCollecting) {
            showParameterWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u8BF7\u5148\u505C\u6B62\u68C0\u6D4B\u540E\u518D\u4FEE\u6539\u6A21\u677F\u9608\u503C\u3002"));
            return;
        }

        int thresholdValue = 0;
        if (!parseIntValue(ui->lineEdit_yuzhi->text(),
                           &thresholdValue)
                || thresholdValue < 0
                || thresholdValue > 100) {
            showParameterWarning(
                        QStringLiteral("\u53C2\u6570\u9519\u8BEF"),
                        QStringLiteral("\u56FE\u50CF\u5408\u683C\u9608\u503C\u5FC5\u987B\u662F0\u5230100\u4E4B\u95F4\u7684\u6574\u6570\uFF08\u5355\u4F4D\uFF1A%\uFF09"));
            return;
        }

        TemplatePrivateSettings settings =
                templatePrivateSettingsFromRecipeProfile(
                    m_singleTemplateRecipeEditSession.recipe()
                    .profiles.first());
        settings.imageThreshold = thresholdValue;
        QString publishError;
        if (!republishSingleTemplateRecipeSettings(settings,
                                                   &publishError)) {
            showParameterInfoWithRedWarning(
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral(
                            "\u56FE\u50CF\u9608\u503C\u672A\u751F\u6548\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                        QStringLiteral(
                            "\u4EA7\u54C1\u914D\u65B9\u91CD\u65B0\u53D1\u5E03\u5931\u8D25\uFF1A\n%1")
                        .arg(publishError));
            return;
        }

        emit ssim(thresholdValue);
        clearTemplateImageThresholdDirty();
        showParameterInfo(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u56FE\u50CF\u9608\u503C\u8BBE\u7F6E\u6210\u529F\uFF1A%1\n"
                        "\u4EA7\u54C1\u914D\u65B9\u5DF2\u4F7F\u7528\u539F\u914D\u65B9\u7F16\u53F7\u91CD\u65B0\u53D1\u5E03\u3002")
                    .arg(thresholdValue));
        return;
    }

    int number = 0;
    if (!parseIntValue(ui->lineEdit_yuzhi->text(), &number)
            || number < 0
            || number > 100) {
        showParameterWarning("参数错误",
                             "图像合格阈值必须是0到100之间的整数（单位：%）");
        return;
    }
    emit ssim(number);
    showParameterInfo("提示", "阈值设置成功");
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

    if (!myImage || myImage->empty()) {
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
            if (!m_barcodeTemplateReadable
                    || m_validatedBarcodeRect
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

                m_barcodeTemplateReadable = true;
                m_validatedBarcodeRect =
                        normalizedBarcodeRect;
                m_validatedBarcodeText = barcode.text;
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
    cv::Mat calibImg = myImage->clone();

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
    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();

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
        m_wordTemplateProfiles.clear();
        m_wordTemplateProfiles.push_back(savedProfile);
        refreshWordTemplateRecipeAssets();
        prepareWordTemplateRecipeDraft(m_wordTemplateProfiles.front());
        m_currentWordTemplateEditIndex = 0;
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
    m_currentTemplateNameVisible = true;
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

            m_wordTemplateRecipeDraftSession.reset();
            m_wordTemplateRecipeEditSession.reset();
            m_singleTemplateRecipeEditSession.reset();
            m_singleTemplateResolvedAssetPathsByRole.clear();
            m_currentTemplateDisplayName.clear();
            m_wordTemplateProfiles.swap(loadedProfiles);
            refreshWordTemplateRecipeAssets();
            currentTemplateDirPath = m_wordTemplateProfiles.front().dirPath;
            m_currentTemplateNameVisible = false;
            updateCurrentTemplateName();
            refreshWordTemplateEditorCombo();
            saveSettings();

            qDebug() << "[WORD_TEMPLATE] loaded profile count:"
                     << static_cast<int>(m_wordTemplateProfiles.size());
            if (!skippedMessages.isEmpty() || !pendingTargetMessages.isEmpty()) {
                QString detailMessage = QString("已加载 %1 个字库模板").arg(static_cast<int>(m_wordTemplateProfiles.size()));
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
                                  .arg(static_cast<int>(m_wordTemplateProfiles.size())));
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

    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateRecipeEditSession.reset();
    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();
    m_wordTemplateProfiles.clear();
    currentTemplateDirPath = dirPath;

    saveSettings(); // 保存路径
    m_currentTemplateNameVisible = loadSettingsFromDir(dirPath);
    if (!m_currentTemplateNameVisible) {
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
    totalImages = 0;
    ngImages = 0;
    ui->ngnum->setText(QString("%1").arg(ngImages));
    ui->imagenum->setText(QString("%1").arg(totalImages));
}

/**
 * @brief 清空NG数统计按钮点击槽函数
 */
void Widget::on_cut_cancelButton_3_clicked()
{
    ngImages = 0;
    ui->ngnum->setText(QString("%1").arg(ngImages));
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

    emit rotate(angleValue);
    updateAppliedGlobalSettingFromUi("image.rotation");
    refreshGlobalSettingDirty("image.rotation");
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
    GlobalSettings settings;
    QString errorMessage;
    if (!AppSettingsManager::loadGlobalSettings(&settings, &errorMessage)) {
        qDebug() << "[GLOBAL_SETTINGS] load failed, using defaults:" << errorMessage;
        settings = AppSettingsManager::defaultGlobalSettings();
    }
    settings.cameraGain = static_cast<int>(settings.cameraGain);
    m_appliedGlobalSettings = settings;
    m_globalSettingsLoaded = true;
    applyGlobalSettingsToUi(settings);
    applyTissueRoughnessThresholdFromUi(false);
}

/**
 * @brief 保存设置
 * @details 将软件公共设置保存到当前用户 AppData
 */
bool Widget::saveSettings(bool showErrorMessage)
{
    if (!m_globalSettingsLoaded) {
        qWarning() << "[GLOBAL_SETTINGS] save skipped before initial load completed";
        return false;
    }

    syncImmediateGlobalSettingsFromUi();

    QString errorMessage;
    if (!AppSettingsManager::saveGlobalSettings(m_appliedGlobalSettings, &errorMessage)) {
        if (showErrorMessage) {
            showParameterCritical("严重警告", QString("当前界面设置保存失败：\n%1").arg(errorMessage));
        } else {
            qDebug() << "[GLOBAL_SETTINGS] silent save failed:" << errorMessage;
        }
        return false;
    }
    return true;
}

GlobalSettings Widget::collectGlobalSettingsFromUi() const
{
    static const QStringList detectModeIds = {
        "stamp_detection",
        "word_detection",
        "ocr_detection",
        "tissue_detection",
        BarcodeWordDetectionMode
    };
    static const QStringList imageSaveModeIds = {"save_none", "save_ng", "save_ok", "save_all"};
    static const QStringList imageSaveTypeIds = {"save_both", "save_annotated_only", "save_raw_only"};
    static const QStringList colorChannelIds = {"color", "red", "green", "blue"};
    static const QStringList rotationIds = {"rotate_none", "rotate_clockwise_90", "rotate_counterclockwise_90", "rotate_180"};
    static const QStringList triggerModeIds = {"trigger_continuous", "trigger_interval"};

    auto idAt = [](const QStringList &ids, int index, const QString &fallback) {
        return (index >= 0 && index < ids.size()) ? ids.at(index) : fallback;
    };

    GlobalSettings settings = AppSettingsManager::defaultGlobalSettings();
    settings.detectModeId = idAt(detectModeIds, ui->comboBox_4->currentIndex(), settings.detectModeId);
    settings.imageSaveModeId = idAt(imageSaveModeIds, ui->comboBox->currentIndex(), settings.imageSaveModeId);
    settings.imageSaveTypeId = idAt(imageSaveTypeIds, ui->comboBox_saveImageType->currentIndex(), settings.imageSaveTypeId);
    settings.imageSavePath = selectedDir;
    settings.templateBaseDirPath = templateBaseDirPath;
    settings.cameraExposure = ui->spinBox->value();
    settings.cameraGain = ui->lineEdit_14->text().toInt();
    settings.colorChannelId = idAt(colorChannelIds, ui->comboBox_5->currentIndex(), settings.colorChannelId);
    settings.imageRotationId = idAt(rotationIds, ui->comboBox_2->currentIndex(), settings.imageRotationId);
    settings.triggerEnabled = ui->checkBox->isChecked();
    settings.triggerModeId = idAt(triggerModeIds, ui->comboBox_3->currentIndex(), settings.triggerModeId);
    settings.plcModeId = settings.triggerModeId;
    settings.plcIp = ui->lineEdit->text().trimmed();
    settings.plcRack = ui->lineEdit_2->text().toInt();
    settings.plcSlot = ui->lineEdit_3->text().toInt();
    settings.photoDistance = ui->lineEdit_6->text().toInt();
    settings.photoTime = ui->lineEdit_20->text().toInt();
    settings.cameraDelay = ui->lineEdit_4->text().toInt();
    settings.rejectDistance = ui->lineEdit_7->text().toInt();
    settings.rejectTime = ui->lineEdit_8->text().toInt();
    settings.rejectPosition = ui->lineEdit_12->text().toInt();
    settings.tissueRoughnessThreshold = ui->lineEdit_tissueRoughnessThreshold->text().toDouble();
    settings.templateDirPathsByMode = m_templateDirPathsByMode;
    settings.publishedRecipeIdsByMode = m_publishedRecipeIdsByMode;
    if (ui->rightPanelSplitter) {
        settings.rightPanelSplitterState =
                ui->rightPanelSplitter->saveState();
    }
    return settings;
}

void Widget::applyGlobalSettingsToUi(const GlobalSettings &settings)
{
    static const QStringList detectModeIds = {
        "stamp_detection",
        "word_detection",
        "ocr_detection",
        "tissue_detection",
        BarcodeWordDetectionMode
    };
    static const QStringList imageSaveModeIds = {"save_none", "save_ng", "save_ok", "save_all"};
    static const QStringList imageSaveTypeIds = {"save_both", "save_annotated_only", "save_raw_only"};
    static const QStringList colorChannelIds = {"color", "red", "green", "blue"};
    static const QStringList rotationIds = {"rotate_none", "rotate_clockwise_90", "rotate_counterclockwise_90", "rotate_180"};
    static const QStringList triggerModeIds = {"trigger_continuous", "trigger_interval"};

    auto indexOf = [](const QStringList &ids, const QString &id, int fallback) {
        const int index = ids.indexOf(id);
        return index >= 0 ? index : fallback;
    };

    const bool oldApplyingGlobalSettings = m_applyingGlobalSettings;
    const bool oldUpdatingGlobalSettingsUi = m_updatingGlobalSettingsUi;
    m_applyingGlobalSettings = true;
    m_updatingGlobalSettingsUi = true;
    m_templateDirPathsByMode = settings.templateDirPathsByMode;
    m_publishedRecipeIdsByMode = settings.publishedRecipeIdsByMode;

    ui->comboBox_4->setCurrentIndex(indexOf(detectModeIds, settings.detectModeId, 1));
    ui->comboBox->setCurrentIndex(indexOf(imageSaveModeIds, settings.imageSaveModeId, 0));
    ui->comboBox_saveImageType->setCurrentIndex(indexOf(imageSaveTypeIds, settings.imageSaveTypeId, 1));
    ui->comboBox_5->setCurrentIndex(indexOf(colorChannelIds, settings.colorChannelId, 0));
    ui->comboBox_2->setCurrentIndex(indexOf(rotationIds, settings.imageRotationId, 0));
    ui->comboBox_3->setCurrentIndex(indexOf(triggerModeIds, settings.triggerModeId, 1));
    ui->checkBox->setChecked(settings.triggerEnabled);
    ui->spinBox->setValue(settings.cameraExposure);
    ui->lineEdit_14->setText(QString::number(static_cast<int>(settings.cameraGain)));
    ui->lineEdit->setText(settings.plcIp);
    ui->lineEdit_2->setText(QString::number(settings.plcRack));
    ui->lineEdit_3->setText(QString::number(settings.plcSlot));
    ui->lineEdit_6->setText(QString::number(settings.photoDistance));
    ui->lineEdit_20->setText(QString::number(settings.photoTime));
    ui->lineEdit_4->setText(QString::number(settings.cameraDelay));
    ui->lineEdit_7->setText(QString::number(settings.rejectDistance));
    ui->lineEdit_8->setText(QString::number(settings.rejectTime));
    ui->lineEdit_12->setText(QString::number(settings.rejectPosition));
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(settings.tissueRoughnessThreshold, 'f', 3));
    if (ui->rightPanelSplitter
            && !settings.rightPanelSplitterState.isEmpty()
            && !ui->rightPanelSplitter->restoreState(
                settings.rightPanelSplitterState)) {
        qDebug() << "[UI_SETTINGS] right panel splitter state is invalid;"
                 << "using the default layout.";
    }
    if (ui->rightPanelSplitter) {
        const int visualHandleHeight =
                ui->rightPanelSplitter
                ->property("visualHandleHeight").toInt();
        if (visualHandleHeight > 0) {
            ui->rightPanelSplitter->setHandleWidth(
                        visualHandleHeight);
            if (QSplitterHandle *rightPanelHandle =
                    ui->rightPanelSplitter->handle(1)) {
                rightPanelHandle->setMinimumHeight(
                            visualHandleHeight);
                rightPanelHandle->setMaximumHeight(
                            visualHandleHeight);
            }
        }
        ui->rightPanelSplitter->setChildrenCollapsible(true);
    }
    selectedDir = settings.imageSavePath;
    templateBaseDirPath = settings.templateBaseDirPath;
    updateSaveDirButtonText();
    updateTissueRoughnessUiVisibility();
    m_currentDetectModeId = currentDetectModeId();
    m_applyingGlobalSettings = oldApplyingGlobalSettings;
    m_updatingGlobalSettingsUi = oldUpdatingGlobalSettingsUi;
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
    if (watched == m_templateGuideFrame
            && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this]() {
            adjustTemplateGuideHeight();
        });
        return false;
    }

    if (watched == m_softwareDataDirLineEdit) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            const QString dirPath = m_softwareDataDirLineEdit->text().trimmed();
            if (dirPath.isEmpty()) {
                return true;
            }

            QDir dir(dirPath);
            if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
                showParameterWarning("提示", QString("无法打开软件数据文件夹：\n%1").arg(dir.absolutePath()));
                return true;
            }

            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
            return true;
        }
        return QWidget::eventFilter(watched, event);
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
            || watched == m_manualCharacterCropButton
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

    if (event->type() == QEvent::Wheel) {
        // 利用类的继承关系，全局拦截所有 QComboBox 和 QAbstractSpinBox(如QSpinBox, QDoubleSpinBox)
        if (watched->inherits("QComboBox") || watched->inherits("QAbstractSpinBox")) {
            return true; // 返回 true 表示事件已处理（被丢弃），彻底禁止滚轮
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
            || hasRunningInspectionThread()) {
        QMessageBox::warning(
                    this,
                    "警告",
                    "相机正在检测采图中！\n"
                    "请先点击【停止识别】完全停止检测后，再关闭相机。");
        return;
    }

    if (m_cameraDevice && m_bOpenDevice)
    {
        m_cameraDevice->close();
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
    ngImages = 0;
    totalImages = 0;
    //    qDebug()<<"totaltime"<<totalTime<<"s";
    //    totalTime=0;
    // 标记相机关闭状态
    m_bOpenDevice = false;
    m_templateCaptureState =
            TemplateCaptureState::Idle;
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
            || hasRunningInspectionThread()) {
        QMessageBox::information(
                    this,
                    "提示",
                    "当前正在识别或停止中，请勿重复启动。");
        return;
    }
    if (!m_bOpenDevice)
    {
        QMessageBox::warning(this, "提示", "请先点击【打开相机】！");
        return;
    }

    updateHardwareParameterUiEnabled();
    refreshAllGlobalSettingDirty();
    refreshTemplatePrivateSettingDirty();

    if (hasDirtySettings()) {
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
    }

    if (ui->checkBox->isChecked()
            && (!m_plcDevice || !m_plcDevice->isConnected())) {
        showParameterWarning("提示", "已启用 PLC 触发，但 PLC 未连接，请先连接 PLC。");
        return;
    }

    updateCurrentTemplateName();
    const bool isWordMode = isWordFamilyMode(currentDetectModeId());
    const bool isBarcodeWordMode =
            currentDetectModeId() == BarcodeWordDetectionMode;
    const bool isTissueMode = (ui->comboBox_4->currentIndex() == 3);
    const bool isWordProfileMode = isWordMode && !m_wordTemplateProfiles.empty();

    if (isWordMode && m_wordTemplateProfiles.empty()) {
        QMessageBox::warning(this, "提示", "当前没有加载产品模板，请重新选择产品模板文件夹。");
        return;
    }

    if (isBarcodeWordMode) {
        QStringList barcodeStartErrors;
        if (!m_barcodeDecoder->ensureLoaded()) {
            barcodeStartErrors.append(
                        QString("读码组件不可用：%1")
                        .arg(m_barcodeDecoder->lastError()));
        }

        for (const WordTemplateProfile &profile : m_wordTemplateProfiles) {
            const QString profileName = profile.name.isEmpty()
                    ? QDir(profile.dirPath).dirName()
                    : profile.name;
            QStringList profileErrors;

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
                                cv::imdecode(buffer, cv::IMREAD_COLOR);
                    } catch (...) {
                        diskTrackingTemplate.release();
                    }
                }
            }
            if (profile.trackingTemplate.empty()
                    || diskTrackingTemplate.empty()) {
                profileErrors.append(
                            "定位模板 tracking_template.bmp 缺失或无法读取");
            }

            CalibrationData diskCalibration;
            const QString calibrationPath =
                    wordTemplateProfileAssetPath(
                        profile,
                        QStringLiteral("calibration"),
                        QStringLiteral("calibrate_config.yaml"));
            const bool calibrationValid =
                    QFileInfo::exists(calibrationPath)
                    && diskCalibration.load(
                        calibrationPath.toLocal8Bit().toStdString());
            if (!calibrationValid) {
                profileErrors.append(
                            "calibrate_config.yaml 缺失或无法读取");
            } else {
                if (diskCalibration.barcode_poly.size() != 4
                        || profile.barcodePoly.size() != 4) {
                    profileErrors.append(
                                "二维码区域 barcode_poly 必须包含4个点");
                }
                if (diskCalibration.date_poly.size() < 3
                        || profile.datePoly.size() < 3) {
                    profileErrors.append(
                                "日期区域 date_poly 至少需要3个点");
                }
            }

            if (profile.settings.targetText.trimmed().isEmpty()
                    || profile.targetCount <= 0) {
                profileErrors.append("目标字符尚未设置");
            }
            if (profile.digitTemplates.empty()
                    || profile.digitTemplates.size()
                       != profile.digitTemplateTargetIndexes.size()) {
                profileErrors.append("字符模板缺失或索引配置无效");
            }

            if (!profileErrors.isEmpty()) {
                barcodeStartErrors.append(
                            QString("模板“%1”：%2")
                            .arg(profileName)
                            .arg(profileErrors.join("；")));
            }
        }

        if (!barcodeStartErrors.isEmpty()) {
            QMessageBox::warning(
                        this,
                        "二维码+三期模板预检失败",
                        QString("以下问题必须处理后才能启动检测：\n\n%1")
                        .arg(barcodeStartErrors.join("\n")));
            return;
        }
    }

    // 启动检测只检查真实模板文件，不再让历史 hasValidBoxes=false 单独阻止启动。
    QStringList productTemplateErrors;
    if (!isTissueMode && !isWordProfileMode) {
        if (currentTemplateDirPath.trimmed().isEmpty()) {
            productTemplateErrors.append("未选择产品模板文件夹");
        }
        if (m_loadedTrackingTemplate.empty()) {
            productTemplateErrors.append("定位模板图片 tracking_template.bmp 缺失或读取失败");
        }
        if (savedDatePoly.empty()) {
            productTemplateErrors.append("喷码检测区域 calibrate_config.yaml/date_poly 缺失或读取失败");
        }
    }
    if (!productTemplateErrors.isEmpty()) {
        QMessageBox::warning(this, "操作规范",
                             QString("缺少可用产品模板，无法启动检测。\n\n"
                                     "具体原因：\n%1\n\n"
                                     "如果是新产品：\n"
                                     "请先【拍照】，框选定位区域和喷码检测区域，然后点击【保存模板】。\n\n"
                                     "如果是已有产品：\n"
                                     "请点击【选择模板】，选择对应产品模板文件夹。")
                             .arg(productTemplateErrors.join("\n")));
        return;
    }

    std::vector<WordTrackingProfile> wordTrackingProfilesForRun;
    std::vector<WordTemplateProfile> wordTemplateProfilesForRun;
    if (isWordProfileMode) {
        wordTemplateProfilesForRun = createWordTemplateRunSnapshot();
        const std::vector<WordTemplateProfile> &profilesForRun =
                wordTemplateProfilesForRun;
        QStringList pendingProfiles;
        for (int i = 0; i < static_cast<int>(profilesForRun.size()); ++i) {
            const WordTemplateProfile &profile = profilesForRun[static_cast<size_t>(i)];
            const QString profileName = profile.name.isEmpty()
                    ? QDir(profile.dirPath).dirName()
                    : profile.name;

            if (profile.settings.targetText.trimmed().isEmpty() || profile.digitTemplates.empty()) {
                pendingProfiles.append(profileName);
                continue;
            }

            WordTrackingProfile trackingProfile;
            trackingProfile.name = profileName;
            trackingProfile.profileIndex = i;
            trackingProfile.trackingTemplate = profile.trackingTemplate;
            trackingProfile.barcodePoly = profile.barcodePoly;
            trackingProfile.datePoly = profile.datePoly;
            wordTrackingProfilesForRun.push_back(trackingProfile);
        }

        if (!pendingProfiles.isEmpty()) {
            QMessageBox::warning(this,
                                 "提示",
                                 QString("以下产品模板还没有确认目标字符，不能启动检测：\n%1")
                                 .arg(pendingProfiles.join("\n")));
            return;
        }
        if (wordTrackingProfilesForRun.empty()) {
            QMessageBox::warning(this, "提示", "没有可用的字库定位配置。");
            return;
        }
    }

    clearWordTemplateRunSnapshot();
    m_barcodeWordRunActive = false;

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();

    // ==========================================================
    // 以下为原有启动线程逻辑，完全保留你所有的 PLC/相机 流程
    // ==========================================================
    if (ui->checkBox->isChecked())
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

        j = 1;
        ui->image_undetected->clear();
        ui->imagenum->clear();
        ui->ngnum->clear();
        ui->resultlabel_7->clear();
        ui->speedLabel->clear();
        ngImages = 0;
        totalImages = 0;

        // 重置相机状态
        if (m_cameraDevice && m_bOpenDevice) {
            try {
                m_cameraDevice->stopGrabbing();
                QThread::msleep(200);
                m_cameraDevice->setEnumValue("TriggerMode", 1);
                m_cameraDevice->setEnumValue("TriggerSource", 0); // 硬触发
                QString exposureError;
                if (!applyCameraExposureValue(m_appliedGlobalSettings.cameraExposure,
                                              &exposureError)) {
                    QMessageBox::warning(this,
                                         "启动失败",
                                         QString("切换硬触发模式后恢复相机曝光失败：\n%1")
                                         .arg(exposureError));
                    return;
                }
                m_cameraDevice->setFloatValue("Gain", gainValue); // 恢复写入增益
                m_cameraDevice->setFloatValue("TriggerDelay", 0);
                m_cameraDevice->registerImageCallback();
                m_cameraDevice->startGrabbing();

                m_cameraDevice->setEnumValue("LineDebouncerTime", 5000U); // 硬触发

                QThread::msleep(100);
            } catch (...) {
                QMessageBox::critical(this, "错误", "相机初始化失败！");
                return;
            }
        }

        // 清理并新建硬触发线程
        if (cameraThread) {
            if (cameraThread->isRunning()) {
                cameraThread->requestStop();
                cameraThread->wait(1500);
            }
            disconnect(cameraThread, nullptr, this, nullptr);
            delete cameraThread;
        }

        cameraThread = new CameraThread(this, m_cameraDevice);
        CameraThread *startedCameraThread =
                cameraThread;
        connect(cameraThread,
                &QThread::finished,
                this,
                [this, startedCameraThread]() {
            if (cameraThread != startedCameraThread
                    || m_operationState
                       != OperationState::Detecting) {
                return;
            }
            isCollecting = false;
            clearWordTemplateRunSnapshot();
            m_barcodeWordRunActive = false;
            m_operationState = m_bOpenDevice
                    ? OperationState::CameraReady
                    : OperationState::CameraClosed;
            ui->statusLabel->setText(
                        "识别线程已停止");
            updateOperationUiState();
        },
        Qt::QueuedConnection);

        cameraThread->setBypassTracking(isTissueMode);
        cameraThread->setBarcodeWordHardTriggerMode(isBarcodeWordMode);
        if (isTissueMode) {
            cameraThread->clearPresetBoxes();
            cameraThread->clearWordTemplateTrackingProfiles();
        } else if (isWordProfileMode) {
            cameraThread->clearPresetBoxes();
            cameraThread->setWordTemplateTrackingProfiles(wordTrackingProfilesForRun);
        } else {
            // 🔥 核心修改：将双框坐标和静态模板喂给线程
            cameraThread->setPresetBoxes(savedDatePoly, savedTrackingBox);
            cameraThread->setPreloadedTemplate(m_loadedTrackingTemplate);
        }

        // 连接所有功能信号
        connect(this, &Widget::rotate, cameraThread, &CameraThread::receiveangle1);
        connect(this, &Widget::choosechannel,cameraThread,&CameraThread::receivecolorchannel);
        connect(this, &Widget::sendDataTo, cameraThread, &CameraThread::received);
        connect(cameraThread, &CameraThread::signal_cleanlabel, this, &Widget::slot_clearResultLabel, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_messImage, this, [this](cv::Mat img) {
            this->handleStreamingFrame(img);
        }, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_boxesSelected, this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
            this->dispatchDetectionByMode(&img, pose);
        }, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
            this->slot_handleTissueResult(&img, result);
        }, Qt::QueuedConnection);

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

        m_resultBoundDisplayActive = isWordMode;
        beginDetectionSession();
        cameraThread->start();
        if (!cameraThread->wait(100)) {
            if (isWordProfileMode) {
                m_runningWordTemplateProfiles.swap(
                            wordTemplateProfilesForRun);
                m_wordTemplateRunActive = true;
                qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                         << static_cast<int>(m_runningWordTemplateProfiles.size())
                         << "mode:" << currentDetectModeId();
            }
            m_barcodeWordRunActive = isBarcodeWordMode;
            isCollecting = true;
            m_operationState =
                    OperationState::Detecting;
            ui->statusLabel->setText("触发模式运行中");
            updateOperationUiState();
        } else {
            m_resultBoundDisplayActive = false;
            clearWordTemplateRunSnapshot();
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

        ensureThreadsReady();
        if (!myThread) reinitializeMyThread();

        myThread->setBypassTracking(isTissueMode);
        if (isTissueMode) {
            myThread->clearPresetBoxes();
            myThread->clearWordTemplateTrackingProfiles();
        } else if (isWordProfileMode) {
            myThread->clearPresetBoxes();
            myThread->setWordTemplateTrackingProfiles(wordTrackingProfilesForRun);
        } else {
            // 🔥 核心修改：将双框坐标和静态模板喂给线程
            myThread->setPresetBoxes(savedDatePoly, savedTrackingBox);
            myThread->setPreloadedTemplate(m_loadedTrackingTemplate);
        }

        connect(myThread, &MyThread::signal_boxesSelected, this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

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

        m_cameraDevice->setEnumValue("TriggerSource", 7); // 软触发
        QString exposureError;
        if (!applyCameraExposureValue(m_appliedGlobalSettings.cameraExposure,
                                      &exposureError)) {
            QMessageBox::warning(this,
                                 "启动失败",
                                 QString("切换软触发模式后恢复相机曝光失败：\n%1")
                                 .arg(exposureError));
            return;
        }
        m_cameraDevice->setFloatValue("Gain", gainValue); // 软触发重新设置增益
        myThread->setCameraDevice(m_cameraDevice);
        myThread->getImagePtr(myImage);

        if (!myThread->isRunning()) {
            if (isWordProfileMode) {
                m_runningWordTemplateProfiles.swap(
                            wordTemplateProfilesForRun);
                m_wordTemplateRunActive = true;
                qDebug() << "[WORD_TEMPLATE_PROFILE] Runtime profile snapshot ready:"
                         << static_cast<int>(m_runningWordTemplateProfiles.size())
                         << "mode:" << currentDetectModeId();
            }
            m_resultBoundDisplayActive = isWordMode;
            beginDetectionSession();
            myThread->start();
            m_barcodeWordRunActive = isBarcodeWordMode;
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
            || hasRunningInspectionThread()) {
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
            m_cameraDevice->enumerateDevices(&deviceCount);
    if (!enumerateResult.isSuccess() || deviceCount == 0)
    {
        QMessageBox::warning(this, "警告", "未找到相机设备！");
        return;
    }

    //连接PLC
    const QByteArray address = ui->lineEdit->text().toUtf8();
    const PlcOperationResult result = m_plcDevice->connectTo(
                address.constData(),
                ui->lineEdit_2->text().toInt(),
                ui->lineEdit_3->text().toInt());

    if (!result.isSuccess())
    {
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
    else{
    updateAppliedGlobalSettingsFromUi(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    refreshGlobalSettingsDirty(QStringList() << "plc.ip" << "plc.rack" << "plc.slot");
    saveSettings(false);
    qDebug()<<"opencamera，plc connect success";
    }
    updateHardwareParameterUiEnabled();

    // 假设只有一个相机，直接打开第一个设备
    const CameraOperationResult openResult =
            m_cameraDevice->openDevice(0);
    if (!openResult.isSuccess())
    {
        QMessageBox::warning(this, "警告", "打开设备失败！");
        return;
    }

    m_bOpenDevice = true;
    // 设置为触发模式
    m_cameraDevice->setEnumValue("TriggerMode", 1);
    // 设置触发源为编码器触发
    m_cameraDevice->setEnumValue("TriggerSource", 0);

    QString exposureAdjustmentMessage;
    QString exposureError;
    if (!applySavedCameraExposure(&exposureAdjustmentMessage, &exposureError)) {
        m_cameraDevice->close();
        m_bOpenDevice = false;
        {
            QSignalBlocker blocker(ui->spinBox);
            ui->spinBox->setRange(0, (std::numeric_limits<int>::max)());
            ui->spinBox->setValue(m_appliedGlobalSettings.cameraExposure);
        }
        refreshGlobalSettingDirty("camera.exposure");
        m_operationState =
                OperationState::CameraClosed;
        updateOperationUiState();
        QMessageBox::warning(this,
                             "警告",
                             QString("打开相机后应用曝光参数失败：\n%1")
                             .arg(exposureError));
        return;
    }

    m_cameraDevice->setFloatValue("TriggerDelay", 0);
    // 开启相机采集
    m_cameraDevice->registerImageCallback();
    m_cameraDevice->startGrabbing();
    //        connect(cameraThread,&CameraThread::threaderror,this,&Widget::onthreaderrormessage);

    myThread->setCameraDevice(m_cameraDevice);
    myThread->getImagePtr(myImage);

    ui->statusLabel->setText("相机已打开");
    ui->statusLabel->setStyleSheet("QLabel{color:#2ecc71; font-weight:bold;}");
    m_operationState =
            OperationState::CameraReady;
    updateOperationUiState();
    const QString openMessage = exposureAdjustmentMessage.isEmpty()
            ? QString("相机打开成功！")
            : QString("相机打开成功！\n\n%1").arg(exposureAdjustmentMessage);
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
    // 清空剔除队列
    while (!removalQueue.empty())
    {
        removalQueue.pop();
    }
    QMessageBox::information(this, "提示", "剔除队列已清空！");
}

/**
 * @brief 重新初始化 myThread
 * @details 完全清理旧的 myThread 并创建新实例，重新连接所有信号槽
 */
void Widget::reinitializeMyThread()
{
    qDebug() << "=== Reinitializing myThread ===";

    // 步骤1: 如果旧线程还存在，先安全清理
    if (myThread) {
        qDebug() << "Cleaning up old myThread...";

        // 停止线程
        if (myThread->isRunning()) {
            myThread->requestStop();
            myThread->stop();

            // 等待线程完全停止
            if (!myThread->wait(2000)) {
                qDebug() << "WARNING: Old myThread did not stop within 2 seconds";
            }
        }

        // 断开所有信号连接
        disconnect(myThread, nullptr, this, nullptr);

        // 删除旧对象
        delete myThread;
        myThread = nullptr;
        qDebug() << "✓ Old myThread cleaned up";
    }

    // 步骤2: 创建新线程
    qDebug() << "Creating new myThread...";
    myThread = new MyThread();

    // 连接信号槽 - 图像显示
    connect(myThread, &MyThread::signal_messImage, this, [this](cv::Mat img) {
        this->handleStreamingFrame(img);
    }, Qt::QueuedConnection);
    // 连接信号槽 - 清除标签
    connect(myThread, &MyThread::signal_cleanlabel,
            this, &Widget::slot_clearResultLabel);

    // 🔥 新增：连接框坐标信号（关键！）
    connect(myThread, &MyThread::signal_boxesSelected,
            this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

    // 连接信号槽 - 图像检测（根据检测模式）
    QObject::connect(myThread, &MyThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    });
    QObject::connect(myThread, &MyThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);

    // 步骤6: 连接其他控制信号
    connect(this, &Widget::rotate, myThread, &MyThread::receiveangle);
    connect(this, &Widget::choosechannel,myThread,&MyThread::receivecolorchannel1);
    connect(this, &Widget::sendDataTo, myThread, &MyThread::received);
    connectTemplatePreviewSignals(myThread);

    // 步骤7: 如果相机已打开，传递相机指针
    if (m_cameraDevice && m_bOpenDevice) {
        myThread->setCameraDevice(m_cameraDevice);
        myThread->getImagePtr(myImage);
        qDebug() << "✓ Camera pointers passed to myThread";
    }

    qDebug() << "✓ myThread reinitialized successfully";
}

/**
 * @brief 重新初始化 cameraThread
 * @details 完全清理旧的 cameraThread 并创建新实例，重新连接所有信号槽
 */
void Widget::reinitializeCameraThread()
{
    qDebug() << "=== Reinitializing cameraThread ===";

    // 步骤1: 如果旧线程还存在，先安全清理
    if (cameraThread) {
        qDebug() << "Cleaning up old cameraThread...";

        // 停止线程
        if (cameraThread->isRunning()) {
            cameraThread->requestStop();
            cameraThread->stopTracking();

            // 关闭OpenCV窗口
            try {
                cv::destroyAllWindows();
            } catch (...) {
                qDebug() << "Exception destroying windows";
            }

            // 等待线程完全停止
            if (!cameraThread->wait(3000)) {
                qDebug() << "WARNING: Old cameraThread did not stop within 3 seconds";
            }
        }

        // 断开所有信号连接
        disconnect(cameraThread, nullptr, this, nullptr);

        // 删除旧对象
        delete cameraThread;
        cameraThread = nullptr;
        qDebug() << "✓ Old cameraThread cleaned up";
    }

    // 步骤2: 检查相机是否可用
    if (!m_cameraDevice || !m_bOpenDevice) {
        qDebug() << "ERROR: Cannot reinitialize cameraThread - camera is null";
        return;
    }

    // 步骤3: 创建新线程
    qDebug() << "Creating new cameraThread...";
    cameraThread = new CameraThread(this, m_cameraDevice);

    // 步骤4: 连接信号槽 - 旋转角度 图像颜色通道
    connect(this, &Widget::rotate, cameraThread, &CameraThread::receiveangle1);
    connect(this, &Widget::choosechannel,cameraThread,&CameraThread::receivecolorchannel);

    // 步骤5: 连接信号槽 - 清除标签
    connect(cameraThread, &CameraThread::signal_cleanlabel,
            this, &Widget::slot_clearResultLabel);

    // 步骤6: 连接信号槽 - 图像显示
    connect(cameraThread, &CameraThread::signal_messImage, this, [this](cv::Mat img) {
        this->handleStreamingFrame(img);
    }, Qt::QueuedConnection);

    connect(cameraThread, &CameraThread::signal_boxesSelected,
            this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

    // 步骤7: 连接信号槽 - 图像检测（根据检测模式）
    connect(cameraThread, &CameraThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    }, Qt::QueuedConnection);
    connect(cameraThread, &CameraThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);

    // 步骤8: 连接模板匹配相关信号
    connect(this, &Widget::jiancestring, templatematch, &TemplateMatch::jianceshibiestr);
    connect(this, &Widget::sendDataTo, cameraThread, &CameraThread::received);

    qDebug() << "✓ cameraThread reinitialized successfully";
}

/**
 * @brief 确保线程已就绪
 * @details 在启动线程前调用此函数，检查并重新初始化必要的线程
 *          这是防止崩溃的关键函数
 */
void Widget::ensureThreadsReady()
{
    qDebug() << "=== Ensuring threads are ready ===";

    // 检查 myThread
    if (!myThread) {
        qDebug() << "myThread is null, reinitializing...";
        reinitializeMyThread();
    } else if (myThread->isRunning()) {
        qDebug() << "myThread is already running, stopping and reinitializing...";
        reinitializeMyThread();
    } else {
        qDebug() << "✓ myThread is ready";
    }

    // 检查 cameraThread（只在需要时）
    // 注意：cameraThread 通常在 on_plcbtn_clicked 中创建，这里不检查

    // 处理事件队列，确保清理完成
    QCoreApplication::processEvents();

    qDebug() << "✓ Threads readiness check completed";
}

/**
 * @brief 接收线程发射的框坐标信号并保存
 */
void Widget::slot_saveBoxesFromThread(DetectionPose pose)
{
    // 字库家族生产检测显示“最后一次完整检测结果帧”。
    // 实时追踪位姿不能再移动上一张检测结果的字符框，否则画面、框和OK/NG会来自不同帧。
    if (shouldSuppressStreamingFrame()) {
        return;
    }

    // 1. 如果目标离开了视野，立刻清空屏幕上的字符框和钢印框，保持画面干净
    if (!pose.valid) {
        g_lastDrawResults.clear();
        g_lastStampPoly.clear();
        g_lastStampIsOverlap = false;
    }
    // 2. 如果目标还在视野中，并且内存里有上一轮识别出的字符框
    else if (g_lastPose.valid && (!g_lastDrawResults.empty() || !g_lastStampPoly.empty())) {

        // 计算两帧之间的物理位移和旋转角度差
        float angleDiff = pose.angleDeg - g_lastPose.angleDeg;
        cv::Point2f oldCenter = g_lastPose.anchorCenter;
        cv::Point2f newCenter = pose.anchorCenter;

        // 让所有字符框跟随产品一起物理移动（AR视觉跟随）
        for (auto& res : g_lastDrawResults) {
            for (auto& pt : res.poly) {
                // 转为相对于旧中心的相对坐标
                cv::Point2f rel(pt.x - oldCenter.x, pt.y - oldCenter.y);
                // 叠加这两帧之间的微小旋转
                cv::Point2f rot = rotateRelativePoint(rel, angleDiff);
                // 叠加上新中心点，得出全新的绝对坐标
                pt = cv::Point(cvRound(rot.x + newCenter.x), cvRound(rot.y + newCenter.y));
            }
        }

        // 让黄/红色的钢印检测框也跟随产品一起移动
        for (auto& pt : g_lastStampPoly) {
            cv::Point2f rel(pt.x - oldCenter.x, pt.y - oldCenter.y);
            cv::Point2f rot = rotateRelativePoint(rel, angleDiff);
            pt = cv::Point(cvRound(rot.x + newCenter.x), cvRound(rot.y + newCenter.y));
        }
    }

    // 最后更新全局位姿
    g_lastPose = pose;
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

    emit choosechannel(colorchannel);
    updateAppliedGlobalSettingFromUi("image.color_channel");
    refreshGlobalSettingDirty("image.color_channel");
    saveSettings(false);
    showParameterInfo("提示", "颜色通道设置成功");

}


//设置相机增益
void Widget::on_pushButton_12_clicked()
{
    QStringList errors;
    if (applyCameraGainFromUi(&errors, true)) {
        updateAppliedGlobalSettingFromUi("camera.gain");
        refreshGlobalSettingDirty("camera.gain");
        saveSettings(false);
    }
}

void Widget::on_pushButton_tissueRoughnessThreshold_clicked()
{
    if (applyTissueRoughnessThresholdFromUi(true)) {
        updateAppliedGlobalSettingFromUi("tissue.roughness_threshold");
        refreshGlobalSettingDirty("tissue.roughness_threshold");
        saveSettings(false);
    }
}


void Widget::on_WriteVDpushButton_clicked()
{

    if (!m_plcDevice || !m_plcDevice->isConnected())
    {
        showParameterWarning("警告", "PLC未连接！");
        return;
    }

    std::uint32_t value = ui->lineEdit_6->text().toUInt();
    unsigned char v_data[4] = {0};

    // 大小端转换
    v_data[3] = (unsigned char)(0xFF & value);
    v_data[2] = (unsigned char)((0xFF00 & value) >> 8);
    v_data[1] = (unsigned char)((0xFF0000 & value) >> 16);
    v_data[0] = (unsigned char)((0xFF000000 & value) >> 24);

    // 写入DB1.924
    const PlcOperationResult result = m_plcDevice->writeDbArea(
                1, 924, 4, PlcDataWidth::DWord, v_data);
// 判断写入结果
    if (!result.isSuccess())
    {
        // 写入失败
        showParameterWarning("error", "设置拍照距离失败");
    }
    else
    {
        // 写入成功
        updateAppliedGlobalSettingFromUi("plc.photo_distance");
        refreshGlobalSettingDirty("plc.photo_distance");
        saveSettings(false);
        showParameterInfo("提示", "拍照距离设置成功");
    }
}
