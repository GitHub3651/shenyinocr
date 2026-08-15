#include "ui/controllers/template_editor_controller.h"

#include "widget.h"
#include "ui_widget.h"

#include "DetectionModes.h"
#include "TrackingTypes.h"
#include "appsettingsmanager.h"
#include "charactertemplatecropdialog.h"
#include "devices/barcode/barcode_decoder.h"
#include "detection/common/detection_roi_geometry.h"
#include "imagelabel.h"
#include "recipes/template_character_asset_workspace.h"
#include "recipes/template_profile_load_plan.h"
#include "recipes/template_profile_mapper.h"
#include "recipes/template_recipe_publisher.h"
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
#include <QStandardPaths>
#include <QTemporaryDir>
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
      m_barcodeDecoder(barcodeDecoder)
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

bool TemplateEditorController::loadSettingsFromDir(
    const QString &dirPath, bool showErrorMessage)
{
    return m_host->loadSettingsFromDir(dirPath, showErrorMessage);
}

void TemplateEditorController::applyTemplatePrivateSettingsToUi(
    const TemplatePrivateSettings &settings)
{
    m_host->applyTemplatePrivateSettingsToUi(settings);
}

void TemplateEditorController::resetTemplateCaptureState()
{
    m_host->resetTemplateCaptureState();
}

void TemplateEditorController::selectLegacyTemplates()
{
    if (m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
            || m_host->m_templateCaptureState
               != Widget::TemplateCaptureState::Idle) {
        QMessageBox::warning(m_host,
                    "提示",
                    "请先停止识别或退出模板制作，再选择产品模板。");
        return;
    }
    QString dirPath;
    auto templateDialogStartDir = [this]() -> QString {
        if (!m_host->templateBaseDirPath.trimmed().isEmpty() && QDir(m_host->templateBaseDirPath).exists()) {
            return QDir(m_host->templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    if (isWordFamilyMode(currentDetectModeId())) {
        QFileDialog dialog(m_host, "选择产品模板文件夹（可勾选多个）", templateDialogStartDir());
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
            m_host->templateBaseDirPath = firstSelectedDirInfo.dir().absolutePath();
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

            wordDraftSession().reset();
            wordEditSession().reset();
            singleTemplateEditSession().reset();
            singleTemplateResolvedAssets().clear();
            setCurrentTemplateDisplayName(QString());
            wordTemplateProfiles().swap(loadedProfiles);
            refreshWordTemplateRecipeAssets();
            m_host->currentTemplateDirPath = wordTemplateProfiles().front().dirPath;
            setCurrentTemplateNameVisible(false);
            updateCurrentTemplateName();
            refreshWordTemplateEditorCombo();
            saveSettings();

            qDebug() << "[WORD_TEMPLATE] loaded profile count:"
                     << static_cast<int>(wordTemplateProfiles().size());
            if (!skippedMessages.isEmpty() || !pendingTargetMessages.isEmpty()) {
                QString detailMessage = QString("已加载 %1 个字库模板").arg(static_cast<int>(wordTemplateProfiles().size()));
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
                                  .arg(static_cast<int>(wordTemplateProfiles().size())));
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
            m_host->templateBaseDirPath = selectedDirInfo.dir().absolutePath();
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        hideTemplateGuide();
    }

    if (dirPath.isEmpty()) return;

    wordDraftSession().reset();
    wordEditSession().reset();
    singleTemplateEditSession().reset();
    singleTemplateResolvedAssets().clear();
    setCurrentTemplateDisplayName(QString());
    wordTemplateProfiles().clear();
    m_host->currentTemplateDirPath = dirPath;

    saveSettings(); // 保存路径
    const bool templateLoaded = loadSettingsFromDir(dirPath, true);
    setCurrentTemplateNameVisible(templateLoaded);
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
    m_host->wrongindex = ui->lineEdit_12->text().toInt();

    qDebug()<<"currentTemplate"<<m_host->currentTemplateDirPath;

    initOverlapDetectorFromCurrentDir();

    QMessageBox::information(m_host, "提示", "模板已选择");
}

void TemplateEditorController::saveCurrentTemplate()
{
    if (m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
            || m_host->m_operationState == Widget::OperationState::TemplatePreviewing) {
        QMessageBox::warning(m_host,
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
        QMessageBox::warning(m_host, "提示", "请先点击【制作模板】拍照获取图像。");
        return;
    }
    if (!imageLabel->isTemplateDrawingEnabled()) {
        QMessageBox::warning(m_host,
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
            QMessageBox::warning(m_host,
                        "提示",
                        isBarcodeWordTemplateMode
                            ? "请先框选稳定定位锚点。"
                            : "请先框选定位区域。");
            return;
        }
        if (uiTrackRect.width() <= 5 || uiTrackRect.height() <= 5) {
            QMessageBox::warning(m_host,
                        "提示",
                        isBarcodeWordTemplateMode
                            ? "定位锚点区域太小，请重新框选。"
                            : "定位区域太小，请重新框选。");
            return;
        }
        if (isBarcodeWordTemplateMode) {
            if (uiBarcodeRect.isNull()) {
                QMessageBox::warning(m_host, "提示", "请先框选二维码区域。");
                return;
            }
            if (uiBarcodeRect.width() <= 5 || uiBarcodeRect.height() <= 5) {
                QMessageBox::warning(m_host, "提示", "二维码区域太小，请重新框选。");
                return;
            }
            const QRect normalizedBarcodeRect =
                    uiBarcodeRect.normalized();
            if (!barcodeTemplateReadable()
                    || validatedBarcodeRect()
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
                    QMessageBox::warning(m_host,
                                "二维码扫描失败",
                                failureReason
                                + "\n\n定位锚点已保留。模板不能保存，"
                                  "请重新完整框选二维码区域，扫描成功后再框选日期区域。");
                    return;
                }

                acceptBarcodeTemplateValidation(
                            normalizedBarcodeRect, barcode.text);
            }
        }
        if (uiDetectPoly.isEmpty()) {
            QMessageBox::warning(m_host, "提示", "请先框选喷码检测区域。");
            return;
        }
        if (uiDetectPoly.size() < 3 || !imageLabel->isDetectionPolyComplete()) {
            QMessageBox::warning(m_host, "提示", "喷码检测区域未闭合或点数不足，请重新框选。");
            return;
        }
    }

    if (!isWordTemplateMode
            && (uiTrackRect.isNull()
                || uiDetectPoly.isEmpty()
                || uiDetectPoly.size() < 3)) {
        QMessageBox::warning(m_host, "警告",
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
        if (!m_host->templateBaseDirPath.trimmed().isEmpty() && QDir(m_host->templateBaseDirPath).exists()) {
            return QDir(m_host->templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    QDialog inputDialog(m_host);
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
                    m_host,
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
        QMessageBox::warning(m_host,
                    "提示",
                    "产品模板文件夹名称不能是“.”或“..”，"
                    "也不能包含 \\ / : * ? \" < > | 这些字符。");
        return;
    }

    QString baseDirPath = baseDirEdit->text().trimmed();
    if (baseDirPath.isEmpty()) {
        QMessageBox::warning(m_host, "提示", "请选择模板文件夹保存目录。");
        return;
    }
    baseDirPath = QDir(baseDirPath).absolutePath();

    QString savePath = QDir::cleanPath(
                QDir(baseDirPath).absoluteFilePath(newFolderName));
    if (QDir(QFileInfo(savePath).absolutePath()).absolutePath()
            .compare(baseDirPath, Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(m_host, "错误", "产品模板保存路径无效，模板未保存。");
        return;
    }

    QDir dir(savePath);
    if (dir.exists()) {
        QMessageBox confirmBox(m_host);
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
            QMessageBox::warning(m_host,
                                 "错误",
                                 "同名产品模板文件夹是快捷链接，无法安全清空。");
            return;
        }
        if (!dir.removeRecursively()) {
            QMessageBox::warning(m_host,
                        "错误",
                        "同名产品模板文件夹清空失败。\n"
                        "请检查其中的文件是否被其他程序占用。");
            return;
        }
        dir = QDir(savePath);
    }

    if (!QDir().mkpath(savePath)) {
        QMessageBox::warning(m_host, "错误", "产品模板文件夹创建失败，无法保存模板。");
        return;
    }
    dir = QDir(savePath);
    m_host->templateBaseDirPath = baseDirPath;

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

    m_host->savedTrackingBox = toPhysicalRect(uiTrackRect);

    // 计算多边形的绝对物理坐标，并存入 YAML 相对坐标 (相对于追踪框中心)
    std::vector<cv::Point2f> absDatePoly;
    for (const QPoint& pt : uiDetectPoly) {
        absDatePoly.push_back(toPhysicalPoint(pt));
    }

    cv::Point2f trackCenter(m_host->savedTrackingBox.x + m_host->savedTrackingBox.width / 2.0,
                            m_host->savedTrackingBox.y + m_host->savedTrackingBox.height / 2.0);

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
    m_host->savedBarcodePoly = relBarcodePoly;
    m_host->savedDatePoly = relDatePoly;
    m_host->hasValidBoxes = true;

    // 3. 物理保存
    cv::imwrite(dir.absoluteFilePath("template_raw.png").toLocal8Bit().toStdString(), calibImg);
    cv::Mat tplImg = calibImg(m_host->savedTrackingBox).clone();
    cv::imwrite(dir.absoluteFilePath("tracking_template.bmp").toLocal8Bit().toStdString(), tplImg);
    m_host->m_loadedTrackingTemplate = tplImg.clone();

    m_host->currentTemplateDirPath = savePath;
    singleTemplateEditSession().reset();
    singleTemplateResolvedAssets().clear();
    setCurrentTemplateDisplayName(QString());

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
        QMessageBox::information(m_host, "标定提示", "即将标定吸管口和钢印区。");

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
    if (!m_host->saveSettingsToDir(savePath)) {
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
        wordTemplateProfiles().clear();
        wordTemplateProfiles().push_back(savedProfile);
        refreshWordTemplateRecipeAssets();
        prepareWordTemplateRecipeDraft(wordTemplateProfiles().front());
        setCurrentWordTemplateEditIndex(0);
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
                m_host->m_bOpenDevice
                ? "模板保存完成，相机已打开"
                : "模板保存完成，相机已关闭");
    setCurrentTemplateNameVisible(true);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    if (isWordTemplateMode || isStampTemplateMode) {
        QMessageBox splitMessageBox(m_host);
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
        QMessageBox::information(m_host, "成功", "模板及双框配置已全部保存！");
    }
}

void TemplateEditorController::initOverlapDetectorFromCurrentDir()
{
    m_host->initOverlapDetectorFromCurrentDir();
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

TemplateRecipeDraftSession &TemplateEditorController::wordDraftSession()
{
    return m_wordTemplateRecipeDraftSession;
}

TemplateRecipeEditSession &TemplateEditorController::wordEditSession()
{
    return m_wordTemplateRecipeEditSession;
}

TemplateRecipeEditSession &
TemplateEditorController::singleTemplateEditSession()
{
    return m_singleTemplateRecipeEditSession;
}

QMap<QString, QString> &
TemplateEditorController::singleTemplateResolvedAssets()
{
    return m_singleTemplateResolvedAssetPathsByRole;
}

const QMap<QString, QString> &
TemplateEditorController::singleTemplateResolvedAssets() const
{
    return m_singleTemplateResolvedAssetPathsByRole;
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


void TemplateEditorController::updateCurrentTemplateName()
{
    QString templateName = "--";

    if (m_currentTemplateNameVisible
            && !m_currentTemplateDisplayName.trimmed().isEmpty()) {
        templateName = m_currentTemplateDisplayName.trimmed();
    } else if (m_currentTemplateNameVisible && !m_host->currentTemplateDirPath.isEmpty()) {
        QDir templateDir(m_host->currentTemplateDirPath);
        if (templateDir.exists() && !templateDir.dirName().isEmpty()) {
            templateName = templateDir.dirName();
        }
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



void TemplateEditorController::setupTemplatePrivateSettingDirtyTracking()
{
    m_templateTargetLabelText = ui->label ? ui->label->text() : QString("目标字符内容:");
    m_templateThresholdLabelText = ui->label_4 ? ui->label_4->text() : QString("图像合格阈值:");

    if (ui->dateEdit) {
        connect(ui->dateEdit, &QTextEdit::textChanged, this, [this]() {
            if (m_host->m_updatingGlobalSettingsUi || m_host->m_applyingGlobalSettings) {
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
            if (m_host->m_updatingGlobalSettingsUi || m_host->m_applyingGlobalSettings) {
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
               && m_singleTemplateRecipeEditSession.isActive()
               && m_singleTemplateRecipeEditSession.recipe().profiles.size() == 1) {
        dirty = ui->dateEdit->toPlainText()
                != m_singleTemplateRecipeEditSession.recipe()
                   .profiles.first().targetText;
    }

    m_host->m_settingsEditState.setTemplateTargetDirty(dirty);
    updateTemplatePrivateSettingDirtyUi();
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

    m_host->m_settingsEditState.setTemplateThresholdDirty(dirty);
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::refreshTemplatePrivateSettingDirty()
{
    refreshTemplateTargetTextDirty();
    refreshTemplateImageThresholdDirty();
}

void TemplateEditorController::markTemplateTargetTextDirty()
{
    m_host->m_settingsEditState.setTemplateTargetDirty(true);
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::markTemplateImageThresholdDirty()
{
    m_host->m_settingsEditState.setTemplateThresholdDirty(true);
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::clearTemplateTargetTextDirty()
{
    m_host->m_settingsEditState.setTemplateTargetDirty(false);
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::clearTemplateImageThresholdDirty()
{
    m_host->m_settingsEditState.setTemplateThresholdDirty(false);
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::clearTemplatePrivateSettingDirty()
{
    m_host->m_settingsEditState.clearTemplateDirty();
    updateTemplatePrivateSettingDirtyUi();
}

void TemplateEditorController::updateTemplatePrivateSettingDirtyUi()
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

    CharacterTemplateCropDialog dialog(rawImage.copy(cropRect), templateDirPath, m_host);
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

void TemplateEditorController::showStampCharacterTemplateCropDialog()
{
    if (!ui
            || currentDetectModeId()
               != QStringLiteral("stamp_detection")) {
        return;
    }
    if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
        templateDirPath = m_host->currentTemplateDirPath.trimmed();
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
                                           m_host);
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
            m_host->digitTemplates.swap(reloadedTemplates);
            m_host->digitTemplateTargetIndexes.swap(
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
                                       m_host);
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

void TemplateEditorController::showPublishedRecipeCharacterTemplateCropDialog(
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
                                       m_host);
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
    m_wordTemplateRecipeDraftSession.reset();
    m_wordTemplateRecipeEditSession.reset();
    m_currentTemplateDisplayName.clear();
    m_wordTemplateProfiles.clear();
    m_currentWordTemplateEditIndex = -1;
    m_host->currentTemplateDirPath.clear();
    m_host->m_loadedTrackingTemplate.release();
    m_host->savedBarcodePoly.clear();
    m_host->savedDatePoly.clear();
    m_host->savedTrackingBox = cv::Rect2d();
    m_host->hasValidBoxes = false;
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

void TemplateEditorController::clearSingleTemplateRecipeState()
{
    m_singleTemplateRecipeEditSession.reset();
    m_singleTemplateResolvedAssetPathsByRole.clear();
    m_currentTemplateDisplayName.clear();
    m_host->currentTemplateDirPath.clear();
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
        ui->lineEdit_yuzhi->setText(QStringLiteral("70"));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearTemplatePrivateSettingDirty();
}

QString TemplateEditorController::detectModeIdForIndex(int index) const
{
    return TemplateModeMemory::modeIdForIndex(index);
}

QString TemplateEditorController::currentDetectModeId() const
{
    return detectModeIdForIndex(ui ? ui->comboBox_4->currentIndex() : 1);
}

QStringList TemplateEditorController::currentTemplatePathsForMode(const QString &modeId) const
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

    if (!m_host->currentTemplateDirPath.trimmed().isEmpty()) {
        paths.append(QDir(m_host->currentTemplateDirPath).absolutePath());
    }
    return paths;
}

void TemplateEditorController::storeCurrentTemplatePathsForMode(const QString &modeId)
{
    if (modeId.trimmed().isEmpty()) {
        return;
    }
    if (isWordFamilyMode(modeId)
            && m_wordTemplateRecipeEditSession.isActive()) {
        const QString recipeId =
                m_wordTemplateRecipeEditSession.recipe().recipeId.trimmed();
        if (!recipeId.isEmpty()) {
            m_templateModeMemory.publishedRecipeIdsByMode().insert(modeId, recipeId);
            return;
        }
    }
    if (isSingleTemplateRecipeMode(modeId)
            && m_singleTemplateRecipeEditSession.isActive()) {
        const QString recipeId =
                m_singleTemplateRecipeEditSession.recipe()
                .recipeId.trimmed();
        if (!recipeId.isEmpty()) {
            m_templateModeMemory.publishedRecipeIdsByMode().insert(modeId, recipeId);
            return;
        }
    }
    m_templateModeMemory.publishedRecipeIdsByMode().remove(modeId);
    m_templateModeMemory.templatePathsByMode().insert(modeId, currentTemplatePathsForMode(modeId));
}

void TemplateEditorController::restoreTemplatesForMode(const QString &modeId, bool showMessage)
{
    const QString rememberedRecipeId =
            m_templateModeMemory.publishedRecipeIdsByMode().value(modeId).trimmed();
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
        m_templateModeMemory.publishedRecipeIdsByMode().remove(modeId);
        m_host->m_appliedGlobalSettings.publishedRecipeIdsByMode.remove(modeId);
        if (!saveSettings(false)) {
            qWarning() << "[RECIPE_RESTORE] failed to remove unavailable recipe memory:"
                       << rememberedRecipeId;
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

    const QStringList paths = m_templateModeMemory.templatePathsByMode().value(modeId);
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
                m_templateModeMemory.templatePathsByMode().insert(modeId, QStringList());
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
            m_templateModeMemory.templatePathsByMode().insert(modeId, validPaths);
            saveSettings(false);
            if (!userMessages.isEmpty()) {
                showParameterWarning("提示", userMessages.join("\n"));
            }
        }
        m_host->currentTemplateDirPath = m_wordTemplateProfiles.front().dirPath;
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
        m_host->currentTemplateDirPath.clear();
        m_currentTemplateNameVisible = false;
        updateCurrentTemplateName();
        m_templateModeMemory.templatePathsByMode().insert(modeId, QStringList());
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
    m_host->currentTemplateDirPath = firstPath;
    m_currentTemplateNameVisible = loadSettingsFromDir(firstPath, showMessage);
    if (!m_currentTemplateNameVisible) {
        m_host->currentTemplateDirPath.clear();
        updateCurrentTemplateName();
        m_templateModeMemory.templatePathsByMode().insert(modeId, QStringList());
        saveSettings(false);
        showParameterWarning("提示",
                             QString("加载历史模板路径 %1 失败，该模板状态异常，已跳过读取。")
                             .arg(firstPath));
        return;
    }
    if (paths.size() != 1 || paths.first() != firstPath) {
        m_templateModeMemory.templatePathsByMode().insert(modeId, QStringList() << firstPath);
        saveSettings(false);
    }
    updateCurrentTemplateName();
    if (m_currentTemplateNameVisible) {
        initOverlapDetectorFromCurrentDir();
    }
    qDebug() << "[TEMPLATE_RESTORE] restored template for mode:" << modeId << firstPath;
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
                && !m_host->currentTemplateDirPath.trimmed().isEmpty()
                && QDir(m_host->currentTemplateDirPath).exists()
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
                    && !m_host->currentTemplateDirPath.trimmed().isEmpty()) {
                displayName = QDir(m_host->currentTemplateDirPath).dirName();
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
    m_host->currentTemplateDirPath = profile.dirPath;
    m_host->savedTrackingBox = profile.settings.trackingBox;
    m_host->hasValidBoxes = profile.settings.hasValidBoxes;
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

void TemplateEditorController::publishCurrentWordTemplateGroup()
{
    if (m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
            || m_host->m_templateCaptureState != Widget::TemplateCaptureState::Idle) {
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
    if (m_host->m_settingsEditState.isTemplateTargetDirty()
            || m_host->m_settingsEditState.isTemplateThresholdDirty()) {
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
    const QString displayName = QInputDialog::getText(m_host,
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

void TemplateEditorController::publishCurrentSingleTemplateRecipe()
{
    if (m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
            || m_host->m_templateCaptureState != Widget::TemplateCaptureState::Idle) {
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
    if (m_host->currentTemplateDirPath.trimmed().isEmpty()
            || !QDir(m_host->currentTemplateDirPath).exists()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u4E00\u4E2A\u6709\u6548\u4EA7\u54C1\u6A21\u677F\u3002"));
        return;
    }
    if (m_host->m_settingsEditState.isTemplateTargetDirty()
            || m_host->m_settingsEditState.isTemplateThresholdDirty()) {
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
                m_host->currentTemplateDirPath,
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

    const QDir sourceDirectory(m_host->currentTemplateDirPath);
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
    const QString displayName = QInputDialog::getText(m_host,
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

void TemplateEditorController::selectPublishedRecipe()
{
    if (m_host->m_operationState == Widget::OperationState::Detecting
            || m_host->m_operationState == Widget::OperationState::Stopping
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

    RecipeSelectionDialog dialog(matchingRecipes, m_host);
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

bool TemplateEditorController::activatePublishedWordRecipe(const QString &recipeId,
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
    m_host->currentTemplateDirPath = selection.recipeDirectoryPath;
    m_currentTemplateNameVisible = false;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(modeId,
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

bool TemplateEditorController::loadSingleTemplateCharacterAssets(
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

bool TemplateEditorController::activatePublishedSingleTemplateRecipe(
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
    m_host->currentTemplateDirPath =
            QFileInfo(loadPlan.trackingTemplatePath).absolutePath();
    m_host->m_loadedTrackingTemplate = trackingTemplate;
    m_host->savedTrackingBox = privateSettings.trackingBox;
    m_host->savedBarcodePoly = calibration.barcode_poly;
    m_host->savedDatePoly = calibration.date_poly;
    m_host->hasValidBoxes = true;
    m_host->digitTemplates.swap(candidateDigitTemplates);
    m_host->digitTemplateTargetIndexes.swap(candidateDigitTargetIndexes);
    if (detectionMode == DetectionMode::Stamp) {
        m_host->overlapDetector = candidateOverlapDetector;
    }
    applyTemplatePrivateSettingsToUi(privateSettings);
    emit m_host->ssim(static_cast<int>(privateSettings.imageThreshold));
    m_currentTemplateNameVisible = true;
    m_templateModeMemory.publishedRecipeIdsByMode().insert(modeId,
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

bool TemplateEditorController::republishSingleTemplateRecipeSettings(
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
    m_templateModeMemory.publishedRecipeIdsByMode().insert(
                currentDetectModeId(),
                publishedSelection.recipe->recipeId);
    saveSettings(false);
    qDebug() << "[RECIPE_PUBLISH] republished single-template recipe:"
             << publishedSelection.recipe->recipeId
             << "mode:" << currentDetectModeId();
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

QString TemplateEditorController::wordTemplateProfileAssetPath(
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

void TemplateEditorController::displayWordTemplateRawImage(
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

void TemplateEditorController::displayWordTemplateRawImage(const QString &dirPath)
{
    if (dirPath.trimmed().isEmpty()) {
        return;
    }
    displayWordTemplateRawImageFile(
                QDir(dirPath).filePath(QStringLiteral("template_raw.png")),
                QDir(dirPath).dirName());
}

void TemplateEditorController::displayWordTemplateRawImageFile(
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

QStringList TemplateEditorController::wordTemplateImagePathsForKey(const QDir &directory,
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

bool TemplateEditorController::loadWordDigitTemplatesFromDir(const QString &dirPath,
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

bool TemplateEditorController::loadWordDigitTemplatesFromProfile(
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
            *errorMessage = QStringLiteral(
                        "\u5185\u90e8\u53c2\u6570\u65e0\u6548");
        }
        return false;
    }

    templates->clear();
    templateTargetIndexes->clear();
    if (baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u76ee\u6807\u5b57\u7b26\u4e3a\u7a7a"
                        "\u6216\u89e3\u6790\u5931\u8d25");
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
                    ? QStringLiteral(
                        "\u672a\u627e\u5230\u5bf9\u5e94"
                        "\u5b57\u7b26\u56fe\u7247")
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
            *errorMessage = QStringLiteral(
                        "\u4ee5\u4e0b\u5b57\u7b26\u56fe\u7247"
                        "\u8bfb\u53d6\u5931\u8d25\uff1a\n[ %1 ]")
                    .arg(failedNames.join(QLatin1Char(' ')));
        }
        return false;
    }
    return !templates->empty();
}

bool TemplateEditorController::loadWordTemplateProfileFromDir(const QString &dirPath,
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

bool TemplateEditorController::loadWordTemplateProfileFromRecipeSelection(
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

bool TemplateEditorController::loadWordTemplateProfilesFromRecipeSelection(
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

void TemplateEditorController::refreshWordTemplateProfileDigitCache(
    WordTemplateProfile *profile) const
{
    if (!profile) {
        return;
    }

    profile->preparedDigitTemplates =
            TemplateMatch::prepareDigitTemplates(
                profile->digitTemplates);
}

InspectionProfileSnapshot
TemplateEditorController::createWordTemplateRunSnapshot() const
{
    std::vector<InspectionProfileSource> sources;
    sources.reserve(m_wordTemplateProfiles.size());
    for (const WordTemplateProfile &sourceProfile : m_wordTemplateProfiles) {
        InspectionProfileSource source;
        source.name = sourceProfile.name;
        source.directoryPath = sourceProfile.dirPath;
        source.trackingTemplate = sourceProfile.trackingTemplate;
        source.barcodePoly = sourceProfile.barcodePoly;
        source.datePoly = sourceProfile.datePoly;
        source.targetText = sourceProfile.settings.targetText;
        source.imageThreshold = sourceProfile.settings.imageThreshold;
        source.digitTemplates = sourceProfile.digitTemplates;
        source.digitTemplateTargetIndexes =
                sourceProfile.digitTemplateTargetIndexes;
        source.barcodeOptions = sourceProfile.settings.barcodeOptions;
        source.decodeStrategy.preferredStrategyId =
                sourceProfile.preferredBarcodeStrategyId;
        source.decodeStrategy.preferredOptionFlags =
                sourceProfile.preferredBarcodeOptionFlags;
        source.decodeStrategy.consecutiveFailures =
                sourceProfile.consecutiveBarcodeFailures;
        sources.push_back(source);
    }
    return InspectionProfileSnapshotBuilder::create(
                sources,
                ui->lineEdit_yuzhi->text());
}

void TemplateEditorController::refreshWordTemplateRecipeProfile(
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

bool TemplateEditorController::saveWordTemplatePrivateSettings(
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
            *errorMessage = QStringLiteral(
                        "\u5f53\u524d\u4ea7\u54c1\u6a21\u677f"
                        "Profile\u65e0\u6548\u3002");
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
                        "\u5F53\u524D\u5DF2\u53D1\u5E03\u914D\u65B9"
                        "\u6CA1\u6709\u6709\u6548\u7684\u7F16\u8F91\u4F1A\u8BDD\u3002");
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

void TemplateEditorController::refreshWordTemplateRecipeAssets()
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

void TemplateEditorController::prepareWordTemplateRecipeDraft(
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

bool TemplateEditorController::publishWordTemplateRecipeDraft(QString *errorMessage)
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

    m_templateModeMemory.publishedRecipeIdsByMode().insert(currentDetectModeId(),
                                      publishedSelection.recipe->recipeId);
    m_host->m_appliedGlobalSettings.publishedRecipeIdsByMode =
            m_templateModeMemory.publishedRecipeIdsByMode();
    saveSettings(false);

    qDebug() << "[RECIPE_PUBLISH] published word recipe:"
             << publishedSelection.recipe->recipeId
             << publishedSelection.recipeDirectoryPath;
    return true;
}

bool TemplateEditorController::publishWordTemplateRecipeEdit(int profileIndex,
                                           QString *errorMessage)
{
    QVector<int> profileIndexes;
    profileIndexes.append(profileIndex);
    return publishWordTemplateRecipeEdits(profileIndexes, errorMessage);
}

bool TemplateEditorController::publishWordTemplateRecipeEdits(
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

void TemplateEditorController::applyCurrentTargetText()
{
    if (isWordFamilyMode(currentDetectModeId()))
    {
        if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
        if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
            m_host->digitTemplates.swap(candidateDigitTemplates);
            m_host->digitTemplateTargetIndexes.swap(
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
        if (m_host->currentTemplateDirPath.isEmpty()) {
            showParameterInfoAsError("提示", "请先选择产品模板文件夹");
            return;
        }
        // 2. 读取当前修改后的目标字符
        QString newMubiaozifu = ui->dateEdit->toPlainText();
        if (newMubiaozifu.isEmpty()) {
            m_host->digitTemplates.clear();
            m_host->digitTemplateTargetIndexes.clear();
            showParameterInfoAsError("提示", "目标字符为空，已清空模板");
            return;
        }

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        QString loadError;
        const QStringList baseNamesToFind = parseTemplateTargetUnits(newMubiaozifu);
        const bool includeVariantTemplates = false;

        if (!loadWordDigitTemplatesFromDir(m_host->currentTemplateDirPath,
                                           baseNamesToFind,
                                           &tempTemplates,
                                           &tempTemplateTargetIndexes,
                                           &loadError,
                                           includeVariantTemplates)) {
            // 如果有任何图片读取失败或丢失，绝不更新运行字符模板，同时给出严厉警告。
            showParameterCritical("严重警告",
                QString("%1\n\n请检查产品模板文件夹内的字符图片是否存在或是否损坏（支持中文，无需关心后缀和大小写）！\n本次更新已撤销。")
                .arg(loadError));
            return;
        }

        // 5. 全部成功后，再更新到全局容器
        m_host->digitTemplates = tempTemplates;
        m_host->digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        showParameterInfo("提示",
                          QString("目标字符确认成功，目标字符 %1 个，字符模板图 %2 张！")
                          .arg(baseNamesToFind.size())
                          .arg(static_cast<int>(m_host->digitTemplates.size())));
    }
    else{
     showParameterInfo("提示", "目标字符确认成功");
    }


}



void TemplateEditorController::applyBatchTargetText()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentTargetText();
        return;
    }

    if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
        QMessageBox::warning(m_host,
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

void TemplateEditorController::applyCurrentImageThreshold()
{
    if (isWordFamilyMode(currentDetectModeId())) {
        if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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

        WordTemplateProfile &profile =
                m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
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
        emit m_host->ssim(thresholdValue);
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
        if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
                        QStringLiteral("\u56FE\u50CF\u9608\u503C\u672A\u751F\u6548\uFF0C\u5F53\u524D\u914D\u65B9\u4FDD\u6301\u4E0D\u53D8\u3002"),
                        QStringLiteral("\u4EA7\u54C1\u914D\u65B9\u91CD\u65B0\u53D1\u5E03\u5931\u8D25\uFF1A\n%1")
                        .arg(publishError));
            return;
        }

        emit m_host->ssim(thresholdValue);
        clearTemplateImageThresholdDirty();
        showParameterInfo(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u56FE\u50CF\u9608\u503C\u8BBE\u7F6E\u6210\u529F\uFF1A%1\n\u4EA7\u54C1\u914D\u65B9\u5DF2\u4F7F\u7528\u539F\u914D\u65B9\u7F16\u53F7\u91CD\u65B0\u53D1\u5E03\u3002")
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
    emit m_host->ssim(number);
    showParameterInfo("提示", "阈值设置成功");
}

void TemplateEditorController::applyBatchImageThreshold()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentImageThreshold();
        return;
    }

    if (m_host->hasRunningInspectionThread() || m_host->isCollecting) {
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
        emit m_host->ssim(thresholdValue);
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
        QMessageBox::warning(m_host,
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


// TEMPLATE_EDITOR_IMPLEMENTATIONS
