#include "ui/controllers/template_editor_controller.h"

#include "widget.h"
#include "ui_widget.h"

#include "DetectionModes.h"
#include "TrackingTypes.h"
#include "charactertemplatecropdialog.h"
#include "devices/barcode/barcode_decoder.h"
#include "detection/common/detection_roi_geometry.h"
#include "imagelabel.h"
#include "runtime/inspection_acquisition_controller.h"
#include "ui/controllers/machine_settings_page_controller.h"
#include "ui/dialogs/recipe_selection_dialog.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QPolygonF>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#pragma execution_character_set("utf-8")

namespace {

void setLabelTextIfChanged(QLabel *label, const QString &text)
{
    if (label && label->text() != text) {
        label->setText(text);
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

bool writeImageFile(const QString &filePath,
                    const cv::Mat &image,
                    const char *extension,
                    QString *errorMessage)
{
    if (image.empty()) {
        if (errorMessage) *errorMessage = QStringLiteral("图像为空。");
        return false;
    }
    std::vector<uchar> encoded;
    if (!cv::imencode(extension, image, encoded)) {
        if (errorMessage) *errorMessage = QStringLiteral("图像编码失败。");
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || file.write(reinterpret_cast<const char *>(encoded.data()),
                          static_cast<qint64>(encoded.size()))
               != static_cast<qint64>(encoded.size())) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

QImage imageFromBgrMat(const cv::Mat &image)
{
    if (image.empty()) return QImage();
    cv::Mat rgb;
    if (image.channels() == 1) {
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgb, cv::COLOR_BGRA2RGBA);
        return QImage(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step),
                      QImage::Format_RGBA8888).copy();
    } else {
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    }
    return QImage(rgb.data, rgb.cols, rgb.rows,
                  static_cast<int>(rgb.step),
                  QImage::Format_RGB888).copy();
}

bool writeCalibrationFile(
        const QString &path,
        const std::vector<cv::Point2f> &stamp,
        const std::vector<cv::Point2f> &date,
        const std::vector<cv::Point2f> &barcode,
        QString *errorMessage)
{
    cv::FileStorage storage(
                "calibrate_config.yaml",
                cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    storage << "stamp_poly" << stamp;
    storage << "date_poly" << date;
    storage << "barcode_poly" << barcode;
    const std::string yaml = storage.releaseAndGetString();
    QFile file(path);
    if (yaml.empty()
            || !file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || file.write(yaml.data(), static_cast<qint64>(yaml.size()))
               != static_cast<qint64>(yaml.size())) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

cv::Point2f transformPoint(const cv::Mat &affine,
                           const cv::Point2f &point)
{
    return cv::Point2f(
        static_cast<float>(affine.at<double>(0, 0) * point.x
                           + affine.at<double>(0, 1) * point.y
                           + affine.at<double>(0, 2)),
        static_cast<float>(affine.at<double>(1, 0) * point.x
                           + affine.at<double>(1, 1) * point.y
                           + affine.at<double>(1, 2)));
}

std::vector<cv::Point> transformPolygon(
    const std::vector<cv::Point> &polygon,
    const cv::Mat &affine)
{
    std::vector<cv::Point> transformed;
    transformed.reserve(polygon.size());
    for (const cv::Point &point : polygon) {
        const cv::Point2f mapped = transformPoint(
                    affine,
                    cv::Point2f(static_cast<float>(point.x),
                                static_cast<float>(point.y)));
        transformed.emplace_back(cvRound(mapped.x), cvRound(mapped.y));
    }
    return transformed;
}

struct BarcodeWordOrientedRois
{
    OrientedBarcodeRoi barcode;
    OrientedDateRoi date;
};

BarcodeWordOrientedRois prepareBarcodeWordOrientedRois(
    const cv::Mat &source,
    const DetectionPose &pose,
    int barcodePaddingPercent,
    int datePadding)
{
    BarcodeWordOrientedRois prepared;
    if (source.empty() || !pose.valid || pose.barcodePoly.size() != 4) {
        return prepared;
    }

    const cv::Mat rotationMatrix = cv::getRotationMatrix2D(
                pose.anchorCenter, -pose.angleDeg, 1.0);
    cv::Mat inverseRotationMatrix;
    cv::invertAffineTransform(rotationMatrix, inverseRotationMatrix);

    std::vector<cv::Point> rotatedBarcodePoly = transformPolygon(
                pose.barcodePoly, rotationMatrix);
    if (rotatedBarcodePoly.size() != 4) {
        return prepared;
    }
    rotatedBarcodePoly = DetectionRoiGeometry::clampPolygonToImage(
                rotatedBarcodePoly, source.size());
    const cv::Rect barcodeBounds = cv::boundingRect(rotatedBarcodePoly);
    if (barcodeBounds.width <= 0 || barcodeBounds.height <= 0) {
        return prepared;
    }

    const int shorterBarcodeSide = std::min(
                barcodeBounds.width, barcodeBounds.height);
    const int barcodePaddingPixels = cvRound(
                static_cast<double>(shorterBarcodeSide)
                * static_cast<double>(std::max(0, barcodePaddingPercent))
                / 100.0);
    const cv::Rect barcodeRoi =
            DetectionRoiGeometry::expandAndClampRect(
                barcodeBounds, barcodePaddingPixels, source.size());
    if (barcodeRoi.width <= 0 || barcodeRoi.height <= 0) {
        return prepared;
    }

    std::vector<cv::Point> rotatedDatePoly;
    cv::Rect dateRoi;
    bool hasValidDateRoi = false;
    if (pose.datePoly.size() >= 3) {
        rotatedDatePoly = transformPolygon(pose.datePoly, rotationMatrix);
        if (rotatedDatePoly.size() >= 3) {
            dateRoi = DetectionRoiGeometry::polygonRoiWithClampedPadding(
                        rotatedDatePoly,
                        std::max(0, datePadding),
                        source.size(),
                        &rotatedDatePoly);
            hasValidDateRoi = dateRoi.width > 0 && dateRoi.height > 0;
        }
    }

    const cv::Rect combinedRoi = hasValidDateRoi
            ? (barcodeRoi | dateRoi)
            : barcodeRoi;
    if (combinedRoi.width <= 0 || combinedRoi.height <= 0) {
        return prepared;
    }

    cv::Mat localRotationMatrix = rotationMatrix.clone();
    localRotationMatrix.at<double>(0, 2) -= combinedRoi.x;
    localRotationMatrix.at<double>(1, 2) -= combinedRoi.y;
    cv::Mat rotatedRegion;
    cv::warpAffine(source,
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
    const cv::Mat barcodeCrop = rotatedRegion(localBarcodeRoi);
    if (barcodeCrop.channels() == 1) {
        if (barcodeCrop.depth() == CV_8U) {
            prepared.barcode.grayRoi = barcodeCrop.clone();
        } else {
            barcodeCrop.convertTo(prepared.barcode.grayRoi, CV_8U);
        }
    } else if (barcodeCrop.channels() == 3) {
        cv::cvtColor(barcodeCrop,
                     prepared.barcode.grayRoi,
                     cv::COLOR_BGR2GRAY);
    } else if (barcodeCrop.channels() == 4) {
        cv::cvtColor(barcodeCrop,
                     prepared.barcode.grayRoi,
                     cv::COLOR_BGRA2GRAY);
    }
    if (!prepared.barcode.grayRoi.empty()
            && !prepared.barcode.grayRoi.isContinuous()) {
        prepared.barcode.grayRoi = prepared.barcode.grayRoi.clone();
    }

    prepared.barcode.rotatedImage = rotatedRegion;
    prepared.barcode.rotatedBarcodePoly = rotatedBarcodePoly;
    prepared.barcode.roi = barcodeRoi;
    prepared.barcode.rotationMatrix = rotationMatrix;
    prepared.barcode.inverseRotationMatrix = inverseRotationMatrix;
    prepared.barcode.valid = !prepared.barcode.grayRoi.empty()
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
    prepared.date.croppedImage = rotatedRegion(localDateRoi).clone();
    if (prepared.date.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (prepared.date.croppedImage.channels() == 1) {
            cv::cvtColor(prepared.date.croppedImage,
                         converted,
                         cv::COLOR_GRAY2BGR);
        } else if (prepared.date.croppedImage.channels() == 4) {
            cv::cvtColor(prepared.date.croppedImage,
                         converted,
                         cv::COLOR_BGRA2BGR);
        } else {
            converted = prepared.date.croppedImage.clone();
        }
        prepared.date.croppedImage = converted;
    }
    prepared.date.rotatedImage = rotatedRegion;
    prepared.date.rotatedDatePoly = rotatedDatePoly;
    prepared.date.roi = dateRoi;
    prepared.date.rotationMatrix = rotationMatrix;
    prepared.date.inverseRotationMatrix = inverseRotationMatrix;
    prepared.date.valid = !prepared.date.croppedImage.empty();
    return prepared;
}

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

} // namespace

TemplateEditorController::TemplateEditorController(
    Widget *host,
    Ui::Widget *view,
    ImageLabel *editorImageLabel,
    const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
    QObject *parent)
    : QObject(parent),
      m_host(host),
      ui(view),
      imageLabel(editorImageLabel),
      m_barcodeDecoder(barcodeDecoder),
      m_recipeEditorSession(
          host && host->m_settingsApplicationService
          ? host->m_settingsApplicationService
            ->editorWorkspacesRootPath()
          : QString())
{
}

void TemplateEditorController::bindRuntimeDependencies(
    InspectionAcquisitionController *acquisitionController,
    MachineSettingsPageController *settingsPageController)
{
    m_acquisitionController = acquisitionController;
    m_settingsPageController = settingsPageController;
}

void TemplateEditorController::setEditorsEnabled(bool enabled)
{
    if (m_wordTemplateEditComboBox) {
        m_wordTemplateEditComboBox->setEnabled(enabled);
    }
    if (m_publishTemplateGroupButton) {
        m_publishTemplateGroupButton->setEnabled(enabled);
    }
    if (m_publishedRecipeButton) {
        m_publishedRecipeButton->setEnabled(enabled);
    }
    if (m_manualCharacterCropButton) {
        m_manualCharacterCropButton->setEnabled(enabled);
    }
    if (ui && ui->dateEdit) {
        ui->dateEdit->setEnabled(enabled);
    }
    if (ui && ui->lineEdit_yuzhi) {
        ui->lineEdit_yuzhi->setEnabled(enabled);
    }
}

QFrame *TemplateEditorController::guideFrame() const
{
    return m_templateGuideFrame;
}

QPushButton *TemplateEditorController::manualCharacterCropButton() const
{
    return m_manualCharacterCropButton;
}

void TemplateEditorController::showParameterInfo(
    const QString &title, const QString &message)
{
    m_host->showParameterInfo(title, message);
}

void TemplateEditorController::showParameterInfoWithRedWarning(
    const QString &title,
    const QString &message,
    const QString &warningMessage)
{
    m_host->showParameterInfoWithRedWarning(title, message, warningMessage);
}

void TemplateEditorController::showParameterInfoAsError(
    const QString &title, const QString &message)
{
    m_host->showParameterInfoAsError(title, message);
}

void TemplateEditorController::showParameterWarning(
    const QString &title, const QString &message)
{
    m_host->showParameterWarning(title, message);
}

void TemplateEditorController::showParameterCritical(
    const QString &title, const QString &message)
{
    m_host->showParameterCritical(title, message);
}

bool TemplateEditorController::saveSettings(bool showErrorMessage)
{
    return m_host->saveSettings(showErrorMessage);
}

void TemplateEditorController::applyRecipeProfileToUi(
    const RecipeProfile &settings)
{
    m_host->applyRecipeProfileToUi(settings);
}

void TemplateEditorController::resetTemplateCaptureState()
{
    m_host->resetTemplateCaptureState();
}

void TemplateEditorController::selectPublishedRecipeForCurrentMode()
{
    if (m_host->isInspectionBusy()
            || m_host->m_templateCaptureState
               != Widget::TemplateCaptureState::Idle) {
        QMessageBox::warning(
                    m_host,
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止识别或退出模板制作，再选择产品配方。"));
        return;
    }
    selectPublishedRecipe();
}

void TemplateEditorController::saveCurrentTemplate()
{
    if (m_host->isInspectionBusy()
            || m_host->operationUiState()
               == Widget::OperationState::TemplatePreviewing) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("当前状态不能保存配方，请先停止识别或冻结模板画面。"));
        return;
    }
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("当前检测模式无效。"));
        return;
    }
    if (mode == DetectionMode::Tissue) {
        bool thresholdValid = false;
        const double roughness =
                ui->lineEdit_tissueRoughnessThreshold
                ->text().trimmed().toDouble(&thresholdValid);
        if (!thresholdValid || roughness <= 0.0) {
            showParameterWarning(
                        QStringLiteral("参数错误"),
                        QStringLiteral("纸巾粗糙度阈值必须大于0。"));
            return;
        }
        bool accepted = false;
        const QString displayName = QInputDialog::getText(
                    m_host,
                    QStringLiteral("保存产品配方"),
                    QStringLiteral("产品配方名称："),
                    QLineEdit::Normal,
                    QString(),
                    &accepted).trimmed();
        if (!accepted || displayName.isEmpty()) {
            return;
        }
        ProductRecipe recipe = createProductRecipe(
                    displayName, DetectionMode::Tissue);
        recipe.tissueParameters.roughnessThreshold = roughness;
        QString errorMessage;
        if (!m_recipeEditorSession.beginNew(recipe, &errorMessage)
                || !m_recipeEditorSession.replaceDraft(
                    recipe, QMap<QString, QString>(), &errorMessage)) {
            showParameterCritical(
                        QStringLiteral("严重警告"), errorMessage);
            return;
        }
        PreparedRecipeSnapshot prepared;
        if (!m_recipeEditorSession.publish(
                    *m_host->m_recipeStore,
                    &prepared,
                    &errorMessage)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "纸巾产品配方保存失败，上一完整版本保持不变：\n%1")
                        .arg(errorMessage));
            return;
        }
        m_activePreparedRecipe = prepared;
        m_currentTemplateDisplayName = displayName;
        m_currentTemplateNameVisible = true;
        const QString modeId = detectionModeUiId(mode);
        m_templateModeMemory.publishedRecipeIdsByMode()
                .insert(modeId, recipe.recipeId);
        m_host->m_appliedMachineSettings
                .publishedRecipeIdsByMode =
                m_templateModeMemory.publishedRecipeIdsByMode();
        saveSettings(false);
        updateCurrentTemplateName();
        showParameterInfo(
                    QStringLiteral("成功"),
                    QStringLiteral("纸巾产品配方已事务保存。"));
        return;
    }
    if (!m_host->m_recipeStore
            || !m_acquisitionController
            || !m_acquisitionController->hasCurrentImage()
            || !imageLabel
            || !imageLabel->isTemplateDrawingEnabled()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先点击【制作模板】获取图像并完成区域框选。"));
        return;
    }

    const bool barcodeMode = mode == DetectionMode::BarcodeWord;
    const bool characterMode = mode == DetectionMode::Stamp
            || mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord;

    const QRect trackingUi = imageLabel->getTrackingRect().normalized();
    const QRect barcodeUi = imageLabel->getBarcodeRect().normalized();
    const QPolygon dateUi = imageLabel->getDetectionPoly();
    if (trackingUi.width() <= 5 || trackingUi.height() <= 5
            || dateUi.size() < 3
            || !imageLabel->isDetectionPolyComplete()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请完成有效的定位区域和闭合的喷码检测区域。"));
        return;
    }
    if (barcodeMode) {
        if (barcodeUi.width() <= 5 || barcodeUi.height() <= 5) {
            showParameterWarning(
                        QStringLiteral("提示"),
                        QStringLiteral("请完整框选二维码区域。"));
            return;
        }
        if (!barcodeTemplateReadable()
                || validatedBarcodeRect() != barcodeUi) {
            BarcodeReadResult barcode;
            QString reason;
            if (!validateBarcodeTemplateRect(
                    barcodeUi,
                    barcodeTemplateValidationOptions(),
                    &barcode,
                    &reason)) {
                clearBarcodeTemplateValidation();
                imageLabel->retryBarcodeRegion();
                showParameterWarning(
                            QStringLiteral("二维码扫描失败"),
                            reason);
                return;
            }
            acceptBarcodeTemplateValidation(barcodeUi, barcode.text);
        }
    }

    const QString targetText = ui->dateEdit->toPlainText().trimmed();
    int threshold = RecipeProfile::DefaultImageThresholdPercent;
    if (characterMode
            && (!parseIntValue(ui->lineEdit_yuzhi->text(), &threshold)
                || threshold < 0 || threshold > 100)) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral("图像合格阈值必须是0到100之间的整数。"));
        return;
    }

    bool accepted = false;
    const QString displayName = QInputDialog::getText(
                m_host,
                QStringLiteral("保存产品配方"),
                QStringLiteral("产品配方名称："),
                QLineEdit::Normal,
                QString(),
                &accepted).trimmed();
    if (!accepted || displayName.isEmpty()) {
        return;
    }

    ProductRecipe recipe = createProductRecipe(displayName, mode);
    QString errorMessage;
    if (!m_recipeEditorSession.beginNew(recipe, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    const QString workspacePath =
            m_recipeEditorSession.workspacePath();
    if (workspacePath.isEmpty()) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("无法创建配方编辑工作区。"));
        return;
    }

    const cv::Mat rawImage =
            m_acquisitionController->currentImageClone();
    const QSize labelSize = imageLabel->size();
    const QSize imageSize(rawImage.cols, rawImage.rows);
    const QPixmap *pixmap = imageLabel->pixmap();
    const QSize displayedSize = pixmap && !pixmap->isNull()
            ? pixmap->size()
            : imageSize.scaled(labelSize, Qt::KeepAspectRatio);
    const int xOffset =
            (labelSize.width() - displayedSize.width()) / 2;
    const int yOffset =
            (labelSize.height() - displayedSize.height()) / 2;
    const double ratioX = static_cast<double>(imageSize.width())
            / displayedSize.width();
    const double ratioY = static_cast<double>(imageSize.height())
            / displayedSize.height();
    auto physicalPoint = [=](const QPoint &point) {
        return cv::Point2f(
                    static_cast<float>((point.x() - xOffset) * ratioX),
                    static_cast<float>((point.y() - yOffset) * ratioY));
    };
    auto physicalRect = [&](const QRect &rect) {
        const cv::Point2f topLeft = physicalPoint(rect.topLeft());
        const cv::Point2f bottomRight = physicalPoint(rect.bottomRight());
        QRectF result(topLeft.x, topLeft.y,
                      bottomRight.x - topLeft.x,
                      bottomRight.y - topLeft.y);
        result = result.intersected(
                    QRectF(0.0, 0.0, rawImage.cols, rawImage.rows));
        return result;
    };

    RecipeProfile profile;
    profile.name = displayName;
    profile.targetText = targetText;
    profile.imageThresholdPercent = threshold;
    profile.trackingRoi = physicalRect(trackingUi);
    const cv::Rect trackingRect(
                cvRound(profile.trackingRoi.x()),
                cvRound(profile.trackingRoi.y()),
                cvRound(profile.trackingRoi.width()),
                cvRound(profile.trackingRoi.height()));
    profile.trackingRoi = QRectF(
                trackingRect.x,
                trackingRect.y,
                trackingRect.width,
                trackingRect.height);
    if (profile.trackingRoi.width() <= 5.0
            || profile.trackingRoi.height() <= 5.0) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("定位区域转换后无效，配方未保存。"));
        return;
    }
    const cv::Point2f trackingCenter(
                static_cast<float>(profile.trackingRoi.center().x()),
                static_cast<float>(profile.trackingRoi.center().y()));
    std::vector<cv::Point2f> datePolygon;
    for (const QPoint &point : dateUi) {
        const cv::Point2f physical = physicalPoint(point);
        datePolygon.emplace_back(
                    physical.x - trackingCenter.x,
                    physical.y - trackingCenter.y);
    }
    std::vector<cv::Point2f> barcodePolygon;
    if (barcodeMode) {
        const QRectF rectangle = physicalRect(barcodeUi);
        const cv::Point2f corners[] = {
            cv::Point2f(static_cast<float>(rectangle.left()),
                        static_cast<float>(rectangle.top())),
            cv::Point2f(static_cast<float>(rectangle.right()),
                        static_cast<float>(rectangle.top())),
            cv::Point2f(static_cast<float>(rectangle.right()),
                        static_cast<float>(rectangle.bottom())),
            cv::Point2f(static_cast<float>(rectangle.left()),
                        static_cast<float>(rectangle.bottom()))
        };
        for (const cv::Point2f &point : corners) {
            barcodePolygon.emplace_back(
                        point.x - trackingCenter.x,
                        point.y - trackingCenter.y);
        }
    }

    if (trackingRect.x < 0 || trackingRect.y < 0
            || trackingRect.x + trackingRect.width > rawImage.cols
            || trackingRect.y + trackingRect.height > rawImage.rows) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("定位区域超出图像范围，配方未保存。"));
        return;
    }
    const cv::Mat trackingTemplate = rawImage(trackingRect).clone();

    std::vector<cv::Point2f> stampPolygon;
    cv::Mat stampRing;
    if (mode == DetectionMode::Stamp) {
        QMessageBox::information(
                    m_host,
                    QStringLiteral("标定提示"),
                    QStringLiteral("即将标定吸管口和钢印区。"));
        const cv::Rect ring = getQuickRectROI(rawImage, "ROI_1");
        if (ring.width <= 5 || ring.height <= 5
                || ring.x < 0 || ring.y < 0
                || ring.x + ring.width > rawImage.cols
                || ring.y + ring.height > rawImage.rows) {
            showParameterWarning(
                        QStringLiteral("提示"),
                        QStringLiteral("吸管口区域无效，配方未保存。"));
            return;
        }
        stampRing = rawImage(ring).clone();
        const cv::Point2f ringCenter(
                    ring.x + ring.width / 2.0f,
                    ring.y + ring.height / 2.0f);
        const std::vector<cv::Point> points =
                getPolygonROI(rawImage, "ROI_2");
        if (points.size() < 3u) {
            showParameterWarning(
                        QStringLiteral("提示"),
                        QStringLiteral("钢印区域点数不足，配方未保存。"));
            return;
        }
        for (const cv::Point &point : points) {
            stampPolygon.emplace_back(
                        point.x - ringCenter.x,
                        point.y - ringCenter.y);
        }
    }

    QDir workspace(workspacePath);
    const QString rawPath =
            workspace.filePath(QStringLiteral("template_raw.png"));
    const QString trackingPath =
            workspace.filePath(QStringLiteral("tracking_template.bmp"));
    const QString calibrationPath =
            workspace.filePath(QStringLiteral("calibrate_config.yaml"));
    if (!writeImageFile(rawPath, rawImage, ".png", &errorMessage)
            || !writeImageFile(trackingPath, trackingTemplate,
                               ".bmp", &errorMessage)
            || !writeCalibrationFile(calibrationPath,
                                     stampPolygon,
                                     datePolygon,
                                     barcodePolygon,
                                     &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方资产写入失败：\n%1").arg(errorMessage));
        return;
    }

    QMap<QString, QString> sources;
    auto addAsset = [&](const QString &key,
                        const QString &role,
                        const QString &source,
                        const QString &relative) {
        recipe.assets.insert(key, relative);
        profile.assetKeys.insert(role, key);
        sources.insert(key, source);
    };
    addAsset(QStringLiteral("profile0.rawImage"),
             QStringLiteral("rawImage"),
             rawPath,
             QStringLiteral("assets/profiles/0/template_raw.png"));
    addAsset(QStringLiteral("profile0.trackingTemplate"),
             QStringLiteral("trackingTemplate"),
             trackingPath,
             QStringLiteral("assets/profiles/0/tracking_template.bmp"));
    addAsset(QStringLiteral("profile0.calibration"),
             QStringLiteral("calibration"),
             calibrationPath,
             QStringLiteral("assets/profiles/0/calibrate_config.yaml"));
    if (mode == DetectionMode::Stamp) {
        const QString ringPath =
                workspace.filePath(QStringLiteral("template_ring.bmp"));
        if (!writeImageFile(ringPath, stampRing, ".bmp", &errorMessage)) {
            showParameterCritical(QStringLiteral("严重警告"), errorMessage);
            return;
        }
        addAsset(QStringLiteral("profile0.stampRing"),
                 QStringLiteral("stampRing"),
                 ringPath,
                 QStringLiteral("assets/profiles/0/template_ring.bmp"));
    }

    recipe.profiles.append(profile);
    if (!m_recipeEditorSession.replaceDraft(
            recipe, sources, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    PreparedRecipeSnapshot prepared;
    if (!m_recipeEditorSession.publish(
            *m_host->m_recipeStore, &prepared, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("产品配方保存失败；上一完整版本保持不变：\n%1")
                    .arg(errorMessage));
        return;
    }
    m_activePreparedRecipe = prepared;

    QStringList pendingMessages;
    const QString uiModeId = detectionModeUiId(mode);
    const bool activated =
            mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord
            ? activatePublishedWordRecipe(
                recipe.recipeId, uiModeId, false,
                &pendingMessages, &errorMessage)
            : activatePublishedSingleTemplateRecipe(
                recipe.recipeId, uiModeId, false, &errorMessage);
    if (!activated) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方已保存，但当前界面加载失败：\n%1")
                    .arg(errorMessage));
        return;
    }

    m_templateModeMemory.publishedRecipeIdsByMode()
            .insert(uiModeId, recipe.recipeId);
    m_host->m_appliedMachineSettings.publishedRecipeIdsByMode =
            m_templateModeMemory.publishedRecipeIdsByMode();
    saveSettings(false);
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    hideTemplateGuide();
    resetTemplateCaptureState();
    ui->statusLabel->setText(
                m_host->isCameraOpen()
                ? QStringLiteral("配方保存完成，相机已打开")
                : QStringLiteral("配方保存完成，相机已关闭"));
    if (characterMode) {
        QMessageBox splitMessageBox(m_host);
        splitMessageBox.setIcon(QMessageBox::Information);
        splitMessageBox.setWindowTitle(QStringLiteral("保存成功"));
        splitMessageBox.setText(
                    QStringLiteral(
                        "产品模板已保存成功。\n\n是否立即切割字符模板？"));
        QPushButton *splitButton = splitMessageBox.addButton(
                    QStringLiteral("确定"), QMessageBox::AcceptRole);
        splitMessageBox.addButton(
                    QStringLiteral("取消"), QMessageBox::RejectRole);
        splitMessageBox.setDefaultButton(splitButton);
        splitMessageBox.exec();
        if (splitMessageBox.clickedButton() == splitButton) {
            showManualCharacterTemplateCropDialog();
        }
    } else {
        showParameterInfo(
                    QStringLiteral("成功"),
                    QStringLiteral("产品模板和全部资源已事务保存。"));
    }
}

TemplateModeMemory &TemplateEditorController::modeMemory()
{
    return m_templateModeMemory;
}

const TemplateModeMemory &TemplateEditorController::modeMemory() const
{
    return m_templateModeMemory;
}

std::vector<WordTemplateProfile> &
TemplateEditorController::wordTemplateProfiles()
{
    return m_wordTemplateProfiles;
}

const std::vector<WordTemplateProfile> &
TemplateEditorController::wordTemplateProfiles() const
{
    return m_wordTemplateProfiles;
}

PreparedRecipeSnapshot TemplateEditorController::activePreparedRecipe() const
{
    return m_activePreparedRecipe;
}

QString TemplateEditorController::currentTemplateDisplayName() const
{
    return m_currentTemplateDisplayName;
}

void TemplateEditorController::setCurrentTemplateDisplayName(
    const QString &displayName)
{
    m_currentTemplateDisplayName = displayName;
}

void TemplateEditorController::setCurrentTemplateNameVisible(bool visible)
{
    m_currentTemplateNameVisible = visible;
}

int TemplateEditorController::currentWordTemplateEditIndex() const
{
    return m_currentWordTemplateEditIndex;
}

bool TemplateEditorController::barcodeTemplateReadable() const
{
    return m_barcodeTemplateReadable;
}

QRect TemplateEditorController::validatedBarcodeRect() const
{
    return m_validatedBarcodeRect;
}

QString TemplateEditorController::validatedBarcodeText() const
{
    return m_validatedBarcodeText;
}

void TemplateEditorController::acceptBarcodeTemplateValidation(
    const QRect &barcodeRect,
    const QString &barcodeText)
{
    m_barcodeTemplateReadable = true;
    m_validatedBarcodeRect = barcodeRect;
    m_validatedBarcodeText = barcodeText;
}

void TemplateEditorController::clearBarcodeTemplateValidation()
{
    m_barcodeTemplateReadable = false;
    m_validatedBarcodeRect = QRect();
    m_validatedBarcodeText.clear();
}

QString TemplateEditorController::barcodeTemplateValidationFailureText(
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

bool TemplateEditorController::validateBarcodeTemplateRect(
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

    const cv::Mat templateImage = m_acquisitionController
            ? m_acquisitionController->currentImageClone()
            : cv::Mat();
    if (templateImage.empty() || !imageLabel) {
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
    const QSize imageSize(templateImage.cols, templateImage.rows);
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
    sourceRect &= cv::Rect(
                0,
                0,
                templateImage.cols,
                templateImage.rows);
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
                templateImage,
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

BarcodeDecodeOptions TemplateEditorController::barcodeTemplateValidationOptions() const
{
    BarcodeRecipeParameters parameters;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (currentDetectModeId() == BarcodeWordDetectionMode
            && profileIndex >= 0
            && profileIndex
               < static_cast<int>(m_wordTemplateProfiles.size())) {
        parameters = m_wordTemplateProfiles[
                    static_cast<size_t>(profileIndex)]
                .settings.barcodeParameters;
    }
    BarcodeDecodeOptions options;
    options.formatMask = parameters.formatMask;
    options.roiPaddingPercent = parameters.roiPaddingPercent;
    options.maxDecodeTimeMs = parameters.maxDecodeTimeMs;
    options.enableFallback = parameters.enableFallback;
    return options;
}


void TemplateEditorController::updateCurrentTemplateName()
{
    QString templateName = "--";

    if (m_currentTemplateNameVisible
            && !m_currentTemplateDisplayName.trimmed().isEmpty()) {
        templateName = m_currentTemplateDisplayName.trimmed();
    }

    ui->currentTemplateName->setText(templateName);

}


void TemplateEditorController::setupTemplateGuide()
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
    m_templateGuideFrame->installEventFilter(m_host);

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

void TemplateEditorController::adjustTemplateGuideHeight()
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

void TemplateEditorController::updateTemplateGuideText(const QString &title, const QString &body)
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

void TemplateEditorController::hideTemplateGuide()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->hide();
    }
}

void TemplateEditorController::updateImageDisplayStatusText(const QString &body)
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

void TemplateEditorController::showTemplateGuideForCurrentMode()
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

void TemplateEditorController::handleTemplateGuideEvent(const QString &eventName, int pointCount)
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
                QMessageBox::warning(m_host,
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

            QMessageBox saveMessageBox(m_host);
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
                saveCurrentTemplate();
            }
        });
    }
}

void TemplateEditorController::setupManualCharacterCropUi()
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
    m_manualCharacterCropButton->installEventFilter(m_host);

    connect(m_manualCharacterCropButton, &QPushButton::clicked,
            this, &TemplateEditorController::showManualCharacterTemplateCropDialog);
    refreshWordTemplateEditorCombo();
}



void TemplateEditorController::setupRecipeProfileDirtyTracking()
{
    m_templateTargetLabelText = ui->label ? ui->label->text() : QString("目标字符内容:");
    m_templateThresholdLabelText = ui->label_4 ? ui->label_4->text() : QString("图像合格阈值:");

    if (ui->dateEdit) {
        connect(ui->dateEdit, &QTextEdit::textChanged, this, [this]() {
            if (m_host->m_updatingMachineSettingsUi || m_host->m_applyingMachineSettings) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_wordTemplateProfiles.empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_recipeEditorSession.isActive())) {
                refreshTemplateTargetTextDirty();
            }
        });
    }
    if (ui->lineEdit_yuzhi) {
        connect(ui->lineEdit_yuzhi, &QLineEdit::textChanged, this, [this](const QString &) {
            if (m_host->m_updatingMachineSettingsUi || m_host->m_applyingMachineSettings) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_wordTemplateProfiles.empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_recipeEditorSession.isActive())) {
                refreshTemplateImageThresholdDirty();
            }
        });
    }
}

void TemplateEditorController::refreshTemplateTargetTextDirty()
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
               && m_recipeEditorSession.isActive()
               && m_recipeEditorSession.recipe().profiles.size() == 1) {
        dirty = ui->dateEdit->toPlainText()
                != m_recipeEditorSession.recipe()
                   .profiles.first().targetText;
    }

    m_host->m_settingsEditState.setTemplateTargetDirty(dirty);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::refreshTemplateImageThresholdDirty()
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
            dirty = (thresholdValue != static_cast<int>(profile.settings.imageThresholdPercent));
        }
    } else if (ui
               && isSingleTemplateRecipeMode(currentDetectModeId())
               && m_recipeEditorSession.isActive()
               && m_recipeEditorSession.recipe().profiles.size() == 1) {
        int thresholdValue = 0;
        if (!parseIntValue(ui->lineEdit_yuzhi->text(), &thresholdValue)) {
            dirty = true;
        } else {
            dirty = thresholdValue
                    != static_cast<int>(
                        m_recipeEditorSession.recipe()
                        .profiles.first().imageThresholdPercent);
        }
    }

    m_host->m_settingsEditState.setTemplateThresholdDirty(dirty);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::refreshRecipeProfileDirty()
{
    refreshTemplateTargetTextDirty();
    refreshTemplateImageThresholdDirty();
}

void TemplateEditorController::markTemplateTargetTextDirty()
{
    m_host->m_settingsEditState.setTemplateTargetDirty(true);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::markTemplateImageThresholdDirty()
{
    m_host->m_settingsEditState.setTemplateThresholdDirty(true);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::clearTemplateTargetTextDirty()
{
    m_host->m_settingsEditState.setTemplateTargetDirty(false);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::clearTemplateImageThresholdDirty()
{
    m_host->m_settingsEditState.setTemplateThresholdDirty(false);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::clearRecipeProfileDirty()
{
    m_host->m_settingsEditState.clearTemplateDirty();
    updateRecipeProfileDirtyUi();
}

void TemplateEditorController::updateRecipeProfileDirtyUi()
{
    if (ui->label) {
        ui->label->setText(m_host->m_settingsEditState.isTemplateTargetDirty()
                           ? m_templateTargetLabelText + " *"
                           : m_templateTargetLabelText);
    }
    if (ui->label_4) {
        ui->label_4->setText(m_host->m_settingsEditState.isTemplateThresholdDirty()
                             ? m_templateThresholdLabelText + " *"
                             : m_templateThresholdLabelText);
    }
}

void TemplateEditorController::showManualCharacterTemplateCropDialog()
{
    if (!m_activePreparedRecipe
            || !m_activePreparedRecipe->recipe) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先选择已保存的产品配方。"));
        return;
    }
    const DetectionMode mode =
            m_activePreparedRecipe->recipe->detectionMode;
    if (mode != DetectionMode::Stamp
            && mode != DetectionMode::Word
            && mode != DetectionMode::BarcodeWord) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("当前模式不使用字符模板。"));
        return;
    }
    const int profileIndex = mode == DetectionMode::Stamp
            ? 0 : currentWordTemplateProfileIndex();
    editActiveRecipeCharacterAssets(profileIndex);
}

void TemplateEditorController::showStampCharacterTemplateCropDialog()
{
    if (m_activePreparedRecipe
            && m_activePreparedRecipe->recipe
            && m_activePreparedRecipe->recipe->detectionMode
               == DetectionMode::Stamp) {
        editActiveRecipeCharacterAssets(0);
    }
}

void TemplateEditorController::showPublishedRecipeCharacterTemplateCropDialog(
        int profileIndex)
{
    editActiveRecipeCharacterAssets(profileIndex);
}

void TemplateEditorController::editActiveRecipeCharacterAssets(
        int profileIndex)
{
    if (!m_host->m_recipeStore
            || !m_activePreparedRecipe
            || !m_activePreparedRecipe->recipe
            || !m_recipeEditorSession.isActive()
            || profileIndex < 0
            || profileIndex >= m_activePreparedRecipe->profiles.size()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("当前配方没有可编辑的Profile。"));
        return;
    }
    const DetectionMode mode =
            m_activePreparedRecipe->recipe->detectionMode;
    if (mode != DetectionMode::Stamp
            && mode != DetectionMode::Word
            && mode != DetectionMode::BarcodeWord) {
        return;
    }

    const PreparedRecipeProfile &prepared =
            m_activePreparedRecipe->profiles.at(profileIndex);
    std::vector<cv::Point> datePolygon;
    const cv::Point2f center(
                static_cast<float>(
                    prepared.definition.trackingRoi.center().x()),
                static_cast<float>(
                    prepared.definition.trackingRoi.center().y()));
    for (const cv::Point2f &point : prepared.datePolygon) {
        datePolygon.emplace_back(
                    cvRound(center.x + point.x),
                    cvRound(center.y + point.y));
    }
    const cv::Rect bounds = cv::boundingRect(datePolygon)
            & cv::Rect(0, 0,
                       prepared.rawImage.cols,
                       prepared.rawImage.rows);
    const QImage rawImage = imageFromBgrMat(prepared.rawImage);
    if (bounds.width <= 0 || bounds.height <= 0
            || rawImage.isNull()) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方喷码区域无法用于字符切割。"));
        return;
    }

    CharacterTemplateCropDialog dialog(
                rawImage.copy(QRect(bounds.x, bounds.y,
                                    bounds.width, bounds.height)),
                prepared.definition,
                m_host);
    if (dialog.exec() != QDialog::Accepted
            || dialog.savedCount() <= 0) {
        return;
    }

    ProductRecipe recipe = m_recipeEditorSession.recipe();
    QMap<QString, QString> sources =
            m_recipeEditorSession.assetSourcePaths();
    RecipeProfile profile = dialog.resultProfile();
    const QStringList oldRoles = profile.assetKeys.keys();
    for (const QString &role : oldRoles) {
        if (!role.startsWith(QLatin1String("character/"))) {
            continue;
        }
        const QString key = profile.assetKeys.take(role);
        recipe.assets.remove(key);
        sources.remove(key);
    }

    const QString workspacePath =
            m_recipeEditorSession.workspacePath();
    if (workspacePath.isEmpty()) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方编辑工作区无效。"));
        return;
    }
    QDir workspace(workspacePath);
    const QMap<QString, QImage> images = dialog.characterImages();
    int characterIndex = 0;
    for (auto it = images.constBegin(); it != images.constEnd();
         ++it, ++characterIndex) {
        const QString source = workspace.filePath(
                    QStringLiteral("profile%1_%2")
                    .arg(profileIndex).arg(it.key()));
        if (!it.value().save(source, "PNG")) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("字符模板写入编辑工作区失败。"));
            return;
        }
        const QString key =
                QStringLiteral("profile%1.character%2")
                .arg(profileIndex)
                .arg(characterIndex, 4, 10, QLatin1Char('0'));
        recipe.assets.insert(
                    key,
                    QStringLiteral("assets/profiles/%1/character_templates/%2")
                    .arg(profileIndex).arg(it.key()));
        profile.assetKeys.insert(
                    QStringLiteral("character/") + it.key(), key);
        sources.insert(key, source);
    }
    recipe.profiles[profileIndex] = profile;
    QString errorMessage;
    if (!m_recipeEditorSession.replaceDraft(
            recipe, sources, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    PreparedRecipeSnapshot updated;
    if (!m_recipeEditorSession.publish(
            *m_host->m_recipeStore, &updated, &errorMessage)) {
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("字符模板修改未写入正式配方，原配方保持不变。"),
                    errorMessage);
        return;
    }
    m_activePreparedRecipe = updated;
    QStringList pending;
    const QString modeId = detectionModeUiId(mode);
    const bool activated =
            mode == DetectionMode::Stamp
            ? activatePublishedSingleTemplateRecipe(
                recipe.recipeId, modeId, false, &errorMessage)
            : activatePublishedWordRecipe(
                recipe.recipeId, modeId, false,
                &pending, &errorMessage);
    if (!activated) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方已更新但界面刷新失败：\n%1")
                    .arg(errorMessage));
        return;
    }
    setCurrentWordTemplateEditIndex(profileIndex);
    saveSettings(false);
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("已事务保存 %1 张字符模板图片。")
                .arg(dialog.savedCount()));
}

void TemplateEditorController::setupWordTemplateEditorCombo()
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
        ui->textsure_btn->installEventFilter(m_host);
        ui->batchTextsure_btn->installEventFilter(m_host);
    }

    if (ui && ui->pushButton_browseImageSavePath) {
        ui->pushButton_browseImageSavePath->installEventFilter(m_host);
    }

    if (ui && ui->pushButton_7) {
        ui->pushButton_7->setToolTip("确认当前选择的颜色通道，用于后续图像处理和识别。");
        ui->pushButton_7->installEventFilter(m_host);
    }

    if (ui) {
        if (ui->checkBox) {
            ui->checkBox->setToolTip(
                        "控制检测的触发方式。\n"
                        "勾选：使用 PLC 外部触发信号控制相机拍照和检测，启动前必须连接 PLC。\n"
                        "不勾选：使用软件软触发，启动后由相机连续采集并检测。");
            ui->checkBox->installEventFilter(m_host);
        }
        if (ui->pushButton_10) {
            ui->pushButton_10->setToolTip("清空当前尚未发出的剔除队列。\n适用于异常停机、误判、手动停止后，防止之前累计的剔除信号继续输出。");
            ui->pushButton_10->installEventFilter(m_host);
        }
        if (ui->label_4) {
            ui->label_4->setToolTip("图像判定合格的分数阈值。\n识别匹配分数低于该值时，通常判为不合格；数值越高，判定越严格。");
            ui->label_4->installEventFilter(m_host);
        }
        if (ui->label_27) {
            ui->label_27->setToolTip("设置图像进入识别前的旋转方向。\n当相机安装方向、产品摆放方向和模板方向不一致时，需要调整这里。");
            ui->label_27->installEventFilter(m_host);
        }
        if (ui->label_16) {
            ui->label_16->setToolTip("设置相机增益。\n增益越高画面越亮，但噪声也可能增加；一般先调曝光，曝光不足时再调增益。");
            ui->label_16->installEventFilter(m_host);
        }
        if (ui->label_14) {
            ui->label_14->setToolTip("PLC拍照信号保持多久。\n相机偶尔漏拍、触发不稳定时可适当加大；正常不要过大，避免影响下一次触发节拍。");
            ui->label_14->installEventFilter(m_host);
        }
        if (ui->label_13) {
            ui->label_13->setToolTip("相机收到 PLC 拍照信号后，再等待多久才真正曝光采图。\n通常在拍照距离基本正确后，用它做小范围微调。\n画面中产品还没到合适位置就加大；产品已经走过或喷码偏后就减小。");
            ui->label_13->installEventFilter(m_host);
        }
        if (ui->label_6) {
            ui->label_6->setToolTip("检测拍照点到剔除机构中心的实际产线距离。\n剔除太早通常加大；剔除太晚通常减小。");
            ui->label_6->installEventFilter(m_host);
        }
        if (ui->label_10) {
            ui->label_10->setToolTip("剔除机构保持动作的时长。\n不合格品剔不干净就加大；影响相邻合格品或动作拖尾就减小。");
            ui->label_10->installEventFilter(m_host);
        }
        if (ui->label_17) {
            ui->label_17->setToolTip("选择第几路剔除输出或第几个剔除口。\n现场有多个气嘴、推杆或剔除工位时使用；填错会从错误位置剔除。");
            ui->label_17->installEventFilter(m_host);
        }
        if (ui->label_8) {
            ui->label_8->setToolTip("上游传感器触发点到相机拍照中心的实际产线距离。\nPLC 根据这个距离判断产品走到相机位置后再发出拍照信号。\n画面中产品还没到拍照位置，说明触发偏早，适当加大；产品已经走过拍照位置，说明触发偏晚，适当减小。");
            ui->label_8->installEventFilter(m_host);
        }
        if (ui->comboBox_3) {
            ui->comboBox_3->setToolTip("PLC触发工作模式。\n连续触发模式：产线连续经过时，PLC按连续节拍触发相机采图和检测。\n间歇触发模式：产品分批、停顿或按间隔到位时，PLC按间歇方式触发采图和检测。");
            ui->comboBox_3->installEventFilter(m_host);
        }
    }

    if (ui && ui->VideoShoot) {
        ui->VideoShoot->installEventFilter(m_host);
    }

    if (ui && ui->batchTextsure_btn) {
        ui->batchTextsure_btn->hide();
    }
    if (ui && ui->batchImageThresholdButton) {
        ui->batchImageThresholdButton->setToolTip(
                    "把当前图像合格阈值保存到所有已选择的产品模板。");
        ui->batchImageThresholdButton->installEventFilter(m_host);
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
                &TemplateEditorController::selectPublishedRecipe);

        m_wordTemplateEditComboBox->hide();
        m_wordTemplateEditWidget->hide();
    }

    m_host->updateTissueRoughnessUiVisibility();
}



void TemplateEditorController::clearWordMultiTemplateState()
{
    clearBarcodeTemplateValidation();
    m_recipeEditorSession.reset();
    m_activePreparedRecipe.reset();
    m_currentTemplateDisplayName.clear();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    m_host->m_loadedTrackingTemplate.release();
    m_host->savedBarcodePoly.clear();
    m_host->savedDatePoly.clear();
    m_host->savedTrackingBox = cv::Rect2d();
    m_host->hasValidBoxes = false;
    if (ui) {
        QSignalBlocker targetBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->clear();
        ui->lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

void TemplateEditorController::clearSingleTemplateRecipeState()
{
    m_recipeEditorSession.reset();
    m_activePreparedRecipe.reset();
    m_currentTemplateDisplayName.clear();
    m_host->m_loadedTrackingTemplate.release();
    m_host->digitTemplates.clear();
    m_host->digitTemplateTargetIndexes.clear();
    m_host->savedBarcodePoly.clear();
    m_host->savedDatePoly.clear();
    m_host->savedTrackingBox = cv::Rect2d();
    m_host->hasValidBoxes = false;
    if (ui) {
        QSignalBlocker targetBlocker(ui->dateEdit);
        QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
        ui->dateEdit->clear();
        ui->lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

QString TemplateEditorController::detectModeIdForIndex(int index) const
{
    return TemplateModeMemory::modeIdForIndex(index);
}

QString TemplateEditorController::currentDetectModeId() const
{
    return detectModeIdForIndex(ui ? ui->comboBox_4->currentIndex() : 1);
}

void TemplateEditorController::restoreTemplatesForMode(
        const QString &modeId,
        bool showMessage)
{
    clearWordMultiTemplateState();
    clearSingleTemplateRecipeState();
    m_recipeEditorSession.reset();
    m_activePreparedRecipe.reset();
    const QString recipeId = m_templateModeMemory
            .publishedRecipeIdsByMode().value(modeId).trimmed();
    if (recipeId.isEmpty()) {
        updateCurrentTemplateName();
        return;
    }

    DetectionMode mode;
    QString errorMessage;
    QStringList pending;
    const bool modeValid = detectionModeFromUiId(modeId, &mode);
    bool restored = false;
    if (modeValid && mode == DetectionMode::Tissue) {
        restored = activatePublishedTissueRecipe(
                    recipeId, false, &errorMessage);
    } else if (modeValid
               && (mode == DetectionMode::Word
                   || mode == DetectionMode::BarcodeWord)) {
        restored = activatePublishedWordRecipe(
                    recipeId, modeId, false,
                    &pending, &errorMessage);
    } else if (modeValid) {
        restored = activatePublishedSingleTemplateRecipe(
                    recipeId, modeId, false, &errorMessage);
    }
    if (restored) {
        updateCurrentTemplateName();
        return;
    }

    m_templateModeMemory.publishedRecipeIdsByMode().remove(modeId);
    m_host->m_appliedMachineSettings
            .publishedRecipeIdsByMode.remove(modeId);
    saveSettings(false);
    updateCurrentTemplateName();
    if (showMessage) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("上次产品配方无法恢复，记录已清除；不会读取旧模板目录：\n%1")
                    .arg(errorMessage));
    }
}

void TemplateEditorController::refreshWordTemplateEditorCombo()
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
    DetectionMode selectedMode = DetectionMode::Word;
    const bool isRecipeMode = detectionModeFromUiId(
                currentDetectModeId(), &selectedMode);
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

    if (m_publishTemplateGroupButton) {
        const bool canPublishWordGroup =
                isWordMode
                && !m_wordTemplateProfiles.empty()
                && m_recipeEditorSession.isActive();
        const bool canPublishSingleTemplate =
                isSingleMode
                && m_recipeEditorSession.isActive();
        m_publishTemplateGroupButton->setVisible(
                    canPublishWordGroup || canPublishSingleTemplate);
        m_publishTemplateGroupButton->setText(
                    isSingleMode
                    ? QStringLiteral("\u53D1\u5E03\u5F53\u524D\u6A21\u677F")
                    : QStringLiteral("\u53D1\u5E03\u6A21\u677F\u7EC4"));
        m_publishTemplateGroupButton->setToolTip(
                    QStringLiteral("事务保存当前产品配方及其完整资源目录。"));
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
            comboBox->addItem(displayName.isEmpty()
                              ? QStringLiteral("--")
                              : displayName,
                              -1);
        }
    };

    fillCombo(m_wordTemplateEditComboBox);

    m_wordTemplateEditWidget->setVisible(isRecipeMode);
    if (m_wordTemplateEditLabel) {
        m_wordTemplateEditLabel->setText(
                    selectedMode == DetectionMode::Tissue
                    ? QStringLiteral("\u5F53\u524D\u4EA7\u54C1\u914D\u65B9:")
                    : QStringLiteral("\u5F53\u524D\u7F16\u8F91\u6A21\u677F:"));
    }
    m_wordTemplateEditComboBox->setVisible(isTemplateMode);
    if (m_publishedRecipeButton) {
        m_publishedRecipeButton->setVisible(isRecipeMode);
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

void TemplateEditorController::applyWordTemplateEditorSelection(int comboIndex)
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

void TemplateEditorController::setCurrentWordTemplateEditIndex(int profileIndex)
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
    m_host->savedTrackingBox = cv::Rect2d(
                profile.settings.trackingRoi.x(),
                profile.settings.trackingRoi.y(),
                profile.settings.trackingRoi.width(),
                profile.settings.trackingRoi.height());
    m_host->hasValidBoxes =
            profile.settings.trackingRoi.width() > 0.0
            && profile.settings.trackingRoi.height() > 0.0;
    {
        QSignalBlocker blocker(ui->dateEdit);
        ui->dateEdit->setPlainText(profile.settings.targetText);
    }
    {
        QSignalBlocker blocker(ui->lineEdit_yuzhi);
        ui->lineEdit_yuzhi->setText(QString::number(static_cast<int>(profile.settings.imageThresholdPercent)));
    }
    refreshRecipeProfileDirty();

    qDebug() << "[WORD_TEMPLATE_PROFILE] editing profile:"
             << profileIndex
             << profile.name
             << "threshold:" << profile.settings.imageThresholdPercent;

    displayWordTemplateRawImage(profile);
}

void TemplateEditorController::publishCurrentWordTemplateGroup()
{
    publishCurrentRecipeSession();
}

void TemplateEditorController::publishCurrentSingleTemplateRecipe()
{
    publishCurrentRecipeSession();
}

void TemplateEditorController::publishCurrentRecipeSession()
{
    if (!m_recipeEditorSession.isActive()
            || !m_host->m_recipeStore) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先通过【保存模板】创建产品配方。"));
        return;
    }
    QString errorMessage;
    PreparedRecipeSnapshot prepared;
    if (!m_recipeEditorSession.publish(
            *m_host->m_recipeStore, &prepared, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("产品配方事务保存失败，原配方保持不变：\n%1")
                    .arg(errorMessage));
        return;
    }
    m_activePreparedRecipe = prepared;
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("当前产品配方已完整保存。"));
}

void TemplateEditorController::selectPublishedRecipe()
{
    if (m_host->isInspectionBusy()
            || m_host->m_templateCaptureState != Widget::TemplateCaptureState::Idle) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u505C\u6B62\u8BC6\u522B\u6216\u9000\u51FA"
                        "\u6A21\u677F\u5236\u4F5C\uFF0C\u518D\u9009\u62E9"
                        "\u5DF2\u53D1\u5E03\u914D\u65B9\u3002"));
        return;
    }

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(currentDetectModeId(), &detectionMode)) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F\u65E0\u6548\u3002"));
        return;
    }

    const RecipeStore &store = *m_host->m_recipeStore;
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

    RecipeSelectionDialog dialog(matchingRecipes, m_host);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList pendingMessages;
    QString activationError;
    bool activated = false;
    if (detectionMode == DetectionMode::Tissue) {
        activated = activatePublishedTissueRecipe(
                    dialog.selectedRecipeId(), true,
                    &activationError);
    } else if (isWordFamilyMode(currentDetectModeId())) {
        activated = activatePublishedWordRecipe(
                    dialog.selectedRecipeId(),
                    currentDetectModeId(), true,
                    &pendingMessages, &activationError);
    } else {
        activated = activatePublishedSingleTemplateRecipe(
                    dialog.selectedRecipeId(),
                    currentDetectModeId(), true,
                    &activationError);
    }
    if (!activated) {
        return;
    }
    saveSettings(false);

    const ProductRecipe &activeRecipe =
            m_recipeEditorSession.recipe();
    QString message = detectionMode == DetectionMode::Tissue
            ? QStringLiteral(
                "\u5DF2\u52A0\u8F7D\u7EB8\u5DFE\u4EA7\u54C1\u914D\u65B9\u201C%1\u201D\u3002")
              .arg(activeRecipe.displayName)
            : QStringLiteral(
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

bool TemplateEditorController::activatePublishedTissueRecipe(
        const QString &recipeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_host->m_recipeStore->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode
               != DetectionMode::Tissue) {
        const QString message = loadError.isEmpty()
                ? QStringLiteral("纸巾产品配方无效。")
                : loadError;
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("纸巾产品配方准备失败：\n%1")
                        .arg(message));
        }
        return false;
    }
    QString sessionError;
    if (!m_recipeEditorSession.beginEdit(
                *m_host->m_recipeStore, recipeId,
                &sessionError)) {
        if (errorMessage) {
            *errorMessage = sessionError;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        sessionError);
        }
        return false;
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    m_host->m_loadedTrackingTemplate.release();
    m_host->digitTemplates.clear();
    m_host->digitTemplateTargetIndexes.clear();
    m_host->savedBarcodePoly.clear();
    m_host->savedDatePoly.clear();
    m_host->savedTrackingBox = cv::Rect2d();
    m_host->hasValidBoxes = false;
    m_activePreparedRecipe = prepared;
    m_currentTemplateDisplayName =
            prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    ui->lineEdit_tissueRoughnessThreshold->setText(
                QString::number(
                    prepared->tissue.roughnessThreshold,
                    'f', 3));
    const QString modeId =
            detectionModeUiId(DetectionMode::Tissue);
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorController::activatePublishedWordRecipe(
        const QString &recipeId,
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
    if (!detectionModeFromUiId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Word
                && detectionMode != DetectionMode::BarcodeWord)) {
        const QString message = QStringLiteral(
                    "已发布字库配方只用于字库匹配和二维码+三期模式。");
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterInfoAsError(QStringLiteral("提示"), message);
        }
        return false;
    }

    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_host->m_recipeStore->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode != detectionMode
            || prepared->profiles.isEmpty()) {
        const QString message = loadError.isEmpty()
                ? QStringLiteral("产品配方模式或Profile无效。")
                : loadError;
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方准备失败，当前模板保持不变：\n%1")
                        .arg(message));
        }
        return false;
    }

    std::vector<WordTemplateProfile> loadedProfiles;
    loadedProfiles.reserve(
                static_cast<std::size_t>(prepared->profiles.size()));
    for (const PreparedRecipeProfile &source : prepared->profiles) {
        WordTemplateProfile profile;
        profile.name = source.definition.name;
        profile.rawImage = source.rawImage.clone();
        profile.trackingTemplate = source.trackingTemplate.clone();
        profile.barcodePoly = source.barcodePolygon;
        profile.datePoly = source.datePolygon;
        profile.settings = source.definition;
        profile.targetCount =
                preparedRecipeTargetUnits(
                    source.definition.targetText).size();
        profile.characterAssets.reserve(
                    source.characterAssets.size());
        for (const PreparedRecipeCharacterAsset &asset
             : source.characterAssets) {
            PreparedRecipeCharacterAsset copy = asset;
            copy.image = asset.image.clone();
            profile.characterAssets.push_back(copy);
        }
        for (const cv::Mat &character : source.characterTemplates) {
            profile.digitTemplates.push_back(character.clone());
        }
        profile.digitTemplateTargetIndexes =
                source.characterTemplateTargetIndexes;
        loadedProfiles.push_back(profile);
    }

    QString sessionError;
    if (!m_recipeEditorSession.beginEdit(
                *m_host->m_recipeStore, recipeId, &sessionError)) {
        if (errorMessage) {
            *errorMessage = sessionError;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方编辑会话无法建立，当前模板保持不变：\n%1")
                        .arg(sessionError));
        }
        return false;
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();
    m_wordTemplateProfiles.swap(loadedProfiles);
    m_activePreparedRecipe = prepared;
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                modeId, prepared->recipe->recipeId);
    if (!m_wordTemplateProfiles.empty()) {
        setCurrentWordTemplateEditIndex(0);
    }
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();

    qDebug() << "[RECIPE_SELECT] selected prepared word recipe:"
             << prepared->recipe->recipeId
             << prepared->recipe->displayName
             << "profiles:" << m_wordTemplateProfiles.size();
    return true;
}


bool TemplateEditorController::activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const auto fail = [this, showErrorMessage, errorMessage](
            const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方资源无法完整准备，当前模板保持不变：\n%1")
                        .arg(message));
        }
        return false;
    };

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Stamp
                && detectionMode != DetectionMode::Ocr)) {
        return fail(QStringLiteral(
                        "已发布单模板配方只用于模板匹配和深度OCR模式。"));
    }

    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_host->m_recipeStore->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode != detectionMode
            || prepared->profiles.size() != 1) {
        return fail(loadError.isEmpty()
                    ? QStringLiteral(
                        "单模板产品配方必须且只能包含一个Profile。")
                    : loadError);
    }

    const PreparedRecipeProfile &profile = prepared->profiles.first();
    OverlapDetector overlapDetector;
    if (detectionMode == DetectionMode::Stamp) {
        CalibrationData calibration;
        calibration.stamp_poly = profile.stampPolygon;
        calibration.date_poly = profile.datePolygon;
        calibration.barcode_poly = profile.barcodePolygon;
        if (!overlapDetector.init(
                    profile.stampRingTemplate, calibration)) {
            return fail(QStringLiteral(
                            "钢印环图或钢印区域无效，防重叠引擎无法初始化。"));
        }
    }

    QString sessionError;
    if (!m_recipeEditorSession.beginEdit(
                *m_host->m_recipeStore,
                recipeId,
                &sessionError)) {
        return fail(sessionError);
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    hideTemplateGuide();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    m_activePreparedRecipe = prepared;
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    m_host->m_loadedTrackingTemplate =
            profile.trackingTemplate.clone();
    m_host->savedTrackingBox = cv::Rect2d(
                profile.definition.trackingRoi.x(),
                profile.definition.trackingRoi.y(),
                profile.definition.trackingRoi.width(),
                profile.definition.trackingRoi.height());
    m_host->savedBarcodePoly = profile.barcodePolygon;
    m_host->savedDatePoly = profile.datePolygon;
    m_host->hasValidBoxes = true;
    m_host->digitTemplates.clear();
    for (const cv::Mat &character : profile.characterTemplates) {
        m_host->digitTemplates.push_back(character.clone());
    }
    m_host->digitTemplateTargetIndexes =
            profile.characterTemplateTargetIndexes;
    if (detectionMode == DetectionMode::Stamp) {
        m_host->overlapDetector = overlapDetector;
    }
    applyRecipeProfileToUi(profile.definition);
    emit m_host->ssim(
                profile.definition.imageThresholdPercent);
    m_currentTemplateNameVisible = true;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorController::republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_recipeEditorSession.isActive()
            || m_recipeEditorSession.recipe().profiles.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前已发布单模板没有有效编辑会话。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_recipeEditorSession.recipe().profiles.first().name;
    updated.assetKeys =
            m_recipeEditorSession.recipe().profiles.first().assetKeys;
    if (!m_recipeEditorSession.updateProfile(
                0, updated, errorMessage)) {
        return false;
    }

    PreparedRecipeSnapshot prepared;
    if (!m_recipeEditorSession.publish(
                *m_host->m_recipeStore,
                &prepared,
                errorMessage)) {
        return false;
    }
    m_activePreparedRecipe = prepared;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                currentDetectModeId(),
                prepared->recipe->recipeId);
    saveSettings(false);
    return true;
}

int TemplateEditorController::currentWordTemplateProfileIndex() const
{
    if (m_currentWordTemplateEditIndex < 0
            || m_currentWordTemplateEditIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return -1;
    }
    return m_currentWordTemplateEditIndex;
}

void TemplateEditorController::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    if (!ui || !ui->image_undetected || profile.rawImage.empty()) {
        return;
    }
    const QImage image = imageFromBgrMat(profile.rawImage);
    if (image.isNull()) {
        return;
    }
    ui->image_undetected->setScaledContents(false);
    ui->image_undetected->setAlignment(Qt::AlignCenter);
    ui->image_undetected->setAutoFitPixmap(
                QPixmap::fromImage(image));
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    updateImageDisplayStatusText(
                QStringLiteral("正在显示模板【%1】的产品图像")
                .arg(profile.name));
}

bool TemplateEditorController::loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!templates || !templateTargetIndexes
            || baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标字符或输出参数无效。");
        }
        return false;
    }

    std::vector<cv::Mat> selected;
    std::vector<int> indexes;
    QVector<bool> found(baseNames.size(), false);
    for (int targetIndex = 0;
         targetIndex < baseNames.size(); ++targetIndex) {
        for (const PreparedRecipeCharacterAsset &asset
             : profile.characterAssets) {
            if (preparedRecipeCharacterAssetMatchesTarget(
                    asset.normalizedBaseName,
                    baseNames.at(targetIndex))) {
                selected.push_back(asset.image.clone());
                indexes.push_back(targetIndex);
                found[targetIndex] = true;
            }
        }
    }

    QStringList missing;
    for (int i = 0; i < found.size(); ++i) {
        if (!found.at(i)) {
            missing.append(baseNames.at(i));
        }
    }
    if (!missing.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "配方中缺少目标字符资产：%1")
                    .arg(missing.join(QLatin1Char(' ')));
        }
        return false;
    }

    templates->swap(selected);
    templateTargetIndexes->swap(indexes);
    return true;
}

void TemplateEditorController::refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const
{
    if (profile) {
        profile->settings.name = profile->name;
    }
}

bool TemplateEditorController::saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_recipeEditorSession.isActive()
            || profileIndex < 0
            || profileIndex >= static_cast<int>(
                m_wordTemplateProfiles.size())
            || profileIndex
               >= m_recipeEditorSession.recipe().profiles.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方Profile无效。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_recipeEditorSession.recipe()
            .profiles.at(profileIndex).name;
    updated.assetKeys =
            m_recipeEditorSession.recipe()
            .profiles.at(profileIndex).assetKeys;
    return m_recipeEditorSession.updateProfile(
                profileIndex, updated, errorMessage);
}

bool TemplateEditorController::publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage)
{
    QVector<int> profileIndexes;
    profileIndexes.append(profileIndex);
    return publishWordTemplateRecipeEdits(
                profileIndexes, errorMessage);
}

bool TemplateEditorController::publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_recipeEditorSession.isActive()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方没有有效编辑会话。");
        }
        return false;
    }

    ProductRecipe candidate = m_recipeEditorSession.recipe();
    for (int profileIndex : profileIndexes) {
        if (profileIndex < 0
                || profileIndex >= static_cast<int>(
                    m_wordTemplateProfiles.size())
                || profileIndex >= candidate.profiles.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "当前配方编辑Profile无效。");
            }
            return false;
        }
        RecipeProfile updated =
                m_wordTemplateProfiles[
                    static_cast<std::size_t>(
                        profileIndex)].settings;
        updated.name = candidate.profiles.at(profileIndex).name;
        updated.assetKeys =
                candidate.profiles.at(profileIndex).assetKeys;
        candidate.profiles[profileIndex] = updated;
    }

    if (!m_recipeEditorSession.replaceDraft(
                candidate,
                m_recipeEditorSession.assetSourcePaths(),
                errorMessage)) {
        return false;
    }
    PreparedRecipeSnapshot prepared;
    if (!m_recipeEditorSession.publish(
                *m_host->m_recipeStore,
                &prepared,
                errorMessage)) {
        return false;
    }
    m_activePreparedRecipe = prepared;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                currentDetectModeId(),
                prepared->recipe->recipeId);
    saveSettings(false);
    return true;
}

void TemplateEditorController::applyCurrentTargetText()
{
    if (m_host->isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改目标字符。"));
        return;
    }
    if (!m_recipeEditorSession.isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先创建或加载产品配方。"));
        return;
    }

    const QString targetText =
            ui->dateEdit->toPlainText();
    if (targetText.trimmed().isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("目标字符不能为空。"));
        return;
    }

    if (isWordFamilyMode(currentDetectModeId())) {
        const int profileIndex =
                currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError(
                        QStringLiteral("提示"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }

        WordTemplateProfile previous =
                m_wordTemplateProfiles[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile &profile =
                m_wordTemplateProfiles[
                    static_cast<std::size_t>(profileIndex)];
        std::vector<cv::Mat> templates;
        std::vector<int> indexes;
        QString validationError;
        const QStringList targetUnits =
                preparedRecipeTargetUnits(targetText);
        if (!loadWordDigitTemplatesFromProfile(
                    profile,
                    targetUnits,
                    &templates,
                    &indexes,
                    &validationError)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "目标字符对应的配方资产不完整：\n%1")
                        .arg(validationError));
            return;
        }

        profile.settings.targetText = targetText;
        profile.targetCount = targetUnits.size();
        profile.digitTemplates.swap(templates);
        profile.digitTemplateTargetIndexes.swap(indexes);

        QString publishError;
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            profile = previous;
            if (m_activePreparedRecipe
                    && m_activePreparedRecipe->recipe) {
                m_recipeEditorSession.beginEdit(
                            *m_host->m_recipeStore,
                            m_activePreparedRecipe->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "目标字符未生效，正式配方保持不变。"),
                        publishError);
            return;
        }

        clearTemplateTargetTextDirty();
        showParameterInfo(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "当前Profile目标字符已事务保存。"));
        return;
    }

    if (isSingleTemplateRecipeMode(currentDetectModeId())) {
        RecipeProfile settings =
                m_recipeEditorSession.recipe()
                .profiles.first();
        settings.targetText = targetText;
        QString publishError;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_activePreparedRecipe
                    && m_activePreparedRecipe->recipe) {
                m_recipeEditorSession.beginEdit(
                            *m_host->m_recipeStore,
                            m_activePreparedRecipe->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "目标字符未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
        if (m_activePreparedRecipe
                && !m_activePreparedRecipe->profiles.isEmpty()) {
            m_host->digitTemplates.clear();
            for (const cv::Mat &character
                 : m_activePreparedRecipe->profiles.first()
                   .characterTemplates) {
                m_host->digitTemplates.push_back(
                            character.clone());
            }
            m_host->digitTemplateTargetIndexes =
                    m_activePreparedRecipe->profiles.first()
                    .characterTemplateTargetIndexes;
        }
        clearTemplateTargetTextDirty();
        showParameterInfo(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "目标字符已事务保存。"));
        return;
    }

    showParameterInfoAsError(
                QStringLiteral("提示"),
                QStringLiteral("当前模式不使用目标字符。"));
}

void TemplateEditorController::applyBatchTargetText()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentTargetText();
        return;
    }
    if (m_host->isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板字符。"));
        return;
    }
    if (!m_recipeEditorSession.isActive()
            || m_wordTemplateProfiles.empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const QString targetText =
            ui->dateEdit->toPlainText();
    const QStringList targetUnits =
            preparedRecipeTargetUnits(targetText);
    if (targetUnits.isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("目标字符不能为空。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_wordTemplateProfiles;
    for (WordTemplateProfile &profile
         : m_wordTemplateProfiles) {
        std::vector<cv::Mat> templates;
        std::vector<int> indexes;
        QString validationError;
        if (!loadWordDigitTemplatesFromProfile(
                    profile,
                    targetUnits,
                    &templates,
                    &indexes,
                    &validationError)) {
            m_wordTemplateProfiles = previous;
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "Profile【%1】缺少目标字符资产：\n%2")
                        .arg(profile.name, validationError));
            return;
        }
        profile.settings.targetText = targetText;
        profile.targetCount = targetUnits.size();
        profile.digitTemplates.swap(templates);
        profile.digitTemplateTargetIndexes.swap(indexes);
    }

    QVector<int> indexes;
    for (int i = 0;
         i < static_cast<int>(m_wordTemplateProfiles.size());
         ++i) {
        indexes.append(i);
    }
    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                indexes, &publishError)) {
        m_wordTemplateProfiles = previous;
        if (m_activePreparedRecipe
                && m_activePreparedRecipe->recipe) {
            m_recipeEditorSession.beginEdit(
                        *m_host->m_recipeStore,
                        m_activePreparedRecipe->recipe->recipeId,
                        nullptr);
        }
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "批量目标字符未生效，正式配方保持不变。"),
                    publishError);
        return;
    }

    clearTemplateTargetTextDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral(
                    "全部Profile目标字符已事务保存。"));
}

void TemplateEditorController::applyCurrentImageThreshold()
{
    if (m_host->isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                ui->lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_recipeEditorSession.isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先创建或加载产品配方。"));
        return;
    }

    QString publishError;
    if (isWordFamilyMode(currentDetectModeId())) {
        const int profileIndex =
                currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError(
                        QStringLiteral("提示"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }
        const WordTemplateProfile previous =
                m_wordTemplateProfiles[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile &profile =
                m_wordTemplateProfiles[
                    static_cast<std::size_t>(profileIndex)];
        profile.settings.imageThresholdPercent = threshold;
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            profile = previous;
            if (m_activePreparedRecipe
                    && m_activePreparedRecipe->recipe) {
                m_recipeEditorSession.beginEdit(
                            *m_host->m_recipeStore,
                            m_activePreparedRecipe->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "图像阈值未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
    } else if (isSingleTemplateRecipeMode(
                   currentDetectModeId())) {
        RecipeProfile settings =
                m_recipeEditorSession.recipe()
                .profiles.first();
        settings.imageThresholdPercent = threshold;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_activePreparedRecipe
                    && m_activePreparedRecipe->recipe) {
                m_recipeEditorSession.beginEdit(
                            *m_host->m_recipeStore,
                            m_activePreparedRecipe->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "图像阈值未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
    } else {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("当前模式不使用图像合格阈值。"));
        return;
    }

    emit m_host->ssim(threshold);
    clearTemplateImageThresholdDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("图像阈值已事务保存：%1")
                .arg(threshold));
}

void TemplateEditorController::applyCurrentTissueThreshold()
{
    if (m_host->isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改纸巾配方阈值。"));
        return;
    }
    bool valid = false;
    const double threshold =
            ui->lineEdit_tissueRoughnessThreshold
            ->text().trimmed().toDouble(&valid);
    if (!valid || !std::isfinite(threshold) || threshold <= 0.0) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral("纸巾粗糙度阈值必须是大于0的有限数字。"));
        return;
    }
    if (!m_host->m_recipeStore
            || !m_recipeEditorSession.isActive()
            || !m_activePreparedRecipe
            || !m_activePreparedRecipe->recipe
            || m_activePreparedRecipe->recipe->detectionMode
               != DetectionMode::Tissue) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先使用【保存模板】创建或加载纸巾产品配方。"));
        return;
    }

    const QString recipeId =
            m_activePreparedRecipe->recipe->recipeId;
    ProductRecipe candidate = m_recipeEditorSession.recipe();
    candidate.tissueParameters.roughnessThreshold = threshold;
    QString errorMessage;
    PreparedRecipeSnapshot updated;
    if (!m_recipeEditorSession.replaceDraft(
            candidate, QMap<QString, QString>(), &errorMessage)
            || !m_recipeEditorSession.publish(
                *m_host->m_recipeStore, &updated, &errorMessage)) {
        m_recipeEditorSession.beginEdit(
                    *m_host->m_recipeStore, recipeId, nullptr);
        ui->lineEdit_tissueRoughnessThreshold->setText(
                    QString::number(
                        m_activePreparedRecipe->tissue
                        .roughnessThreshold,
                        'f', 3));
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("纸巾阈值未生效，正式配方保持不变。"),
                    errorMessage);
        return;
    }

    m_activePreparedRecipe = updated;
    ui->lineEdit_tissueRoughnessThreshold->setText(
                QString::number(threshold, 'f', 3));
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("纸巾粗糙度阈值已事务保存到当前产品配方。"));
}

void TemplateEditorController::applyBatchImageThreshold()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentImageThreshold();
        return;
    }
    if (m_host->isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                ui->lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_recipeEditorSession.isActive()
            || m_wordTemplateProfiles.empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_wordTemplateProfiles;
    QVector<int> profileIndexes;
    for (int i = 0;
         i < static_cast<int>(m_wordTemplateProfiles.size());
         ++i) {
        m_wordTemplateProfiles[
                static_cast<std::size_t>(i)]
                .settings.imageThresholdPercent = threshold;
        profileIndexes.append(i);
    }

    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                profileIndexes, &publishError)) {
        m_wordTemplateProfiles = previous;
        if (m_activePreparedRecipe
                && m_activePreparedRecipe->recipe) {
            m_recipeEditorSession.beginEdit(
                        *m_host->m_recipeStore,
                        m_activePreparedRecipe->recipe->recipeId,
                        nullptr);
        }
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "批量图像阈值未生效，正式配方保持不变。"),
                    publishError);
        return;
    }

    emit m_host->ssim(threshold);
    clearTemplateImageThresholdDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral(
                    "全部Profile图像阈值已事务保存：%1")
                .arg(threshold));
}


/**
 * @brief QImage转换为cv::Mat指针
 * @param image 输入的QImage对象
 * @return cv::Mat* 转换后的Mat指针
 */


// TEMPLATE_EDITOR_IMPLEMENTATIONS
