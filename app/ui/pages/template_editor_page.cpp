#include "ui/pages/template_editor_page.h"

#include "DetectionModes.h"
#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "ui/dialogs/character_template_editor_dialog.h"
#include "imagelabel.h"
#include "ui/controllers/machine_settings_page_controller.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui/dialogs/recipe_selection_dialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>

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
        for (std::size_t i = 0; i < state->points.size(); ++i) {
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

} // namespace

TemplateEditorPage::TemplateEditorPage(
    const TemplateEditorViewBindings &view,
    TemplateApplicationService *templateService,
    InspectionApplicationService *inspectionService,
    SettingsApplicationService *settingsService,
    MachineSettingsPageController *settingsPageController,
    SettingsEditState *settingsEditState,
    const TemplateEditorPageCallbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_view(view),
      m_templateService(templateService),
      m_inspectionService(inspectionService),
      m_settingsService(settingsService),
      m_settingsPageController(settingsPageController),
      m_settingsEditState(settingsEditState),
      m_callbacks(callbacks),
      imageLabel(view.imageLabel)
{
    if (!m_view.parentWidget || !imageLabel || !m_templateService
            || !m_inspectionService || !m_settingsService
            || !m_settingsPageController || !m_settingsEditState) {
        throw std::invalid_argument(
                    "TemplateEditorPage requires complete bindings");
    }
    connect(m_inspectionService,
            &InspectionApplicationService::templatePreviewFrameReady,
            this,
            [this](quint64 sessionId, cv::Mat image) {
        handlePreviewFrame(sessionId, image);
    },
    Qt::QueuedConnection);
    connect(m_inspectionService,
            &InspectionApplicationService::templatePreviewFailed,
            this,
            [this](quint64 sessionId, const QString &reason) {
        handlePreviewFailure(sessionId, reason);
    },
    Qt::QueuedConnection);
    connect(m_inspectionService,
            &InspectionApplicationService::captureStopped,
            this,
            [this](bool preview) {
        if (!preview || m_captureState != CaptureState::Previewing) {
            return;
        }
        ++m_previewSessionId;
        m_captureState = CaptureState::Idle;
        m_lastPreviewFrame.release();
        if (m_view.statusLabel) {
            m_view.statusLabel->setText(
                        isCameraOpen()
                        ? QStringLiteral("模板实时取景已停止，相机已打开")
                        : QStringLiteral("模板实时取景已停止，相机已关闭"));
        }
        if (m_callbacks.updateOperationUiState) {
            m_callbacks.updateOperationUiState();
        }
    },
    Qt::QueuedConnection);
}

void TemplateEditorPage::setEditorsEnabled(bool enabled)
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
    if (m_view.dateEdit) {
        m_view.dateEdit->setEnabled(enabled);
    }
    if (m_view.lineEdit_yuzhi) {
        m_view.lineEdit_yuzhi->setEnabled(enabled);
    }
}

QFrame *TemplateEditorPage::guideFrame() const
{
    return m_templateGuideFrame;
}

QPushButton *TemplateEditorPage::manualCharacterCropButton() const
{
    return m_manualCharacterCropButton;
}

void TemplateEditorPage::showParameterInfo(
    const QString &title, const QString &message)
{
    QMessageBox::information(dialogParent(), title, message);
}

void TemplateEditorPage::showParameterInfoWithRedWarning(
    const QString &title,
    const QString &message,
    const QString &warningMessage)
{
    QMessageBox box(dialogParent());
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(message);
    box.setInformativeText(QStringLiteral("<font color='#d32f2f'>%1</font>")
                           .arg(warningMessage.toHtmlEscaped()));
    box.exec();
}

void TemplateEditorPage::showParameterInfoAsError(
    const QString &title, const QString &message)
{
    QMessageBox::critical(dialogParent(), title, message);
}

void TemplateEditorPage::showParameterWarning(
    const QString &title, const QString &message)
{
    QMessageBox::warning(dialogParent(), title, message);
}

void TemplateEditorPage::showParameterCritical(
    const QString &title, const QString &message)
{
    QMessageBox::critical(dialogParent(), title, message);
}

bool TemplateEditorPage::saveSettings(bool showErrorMessage)
{
    MachineSettings draft = m_settingsService->draft();
    draft.publishedRecipeIdsByMode =
            m_templateService->modeMemory().publishedRecipeIdsByMode();
    m_settingsService->updateDraft(draft);
    if (m_callbacks.saveSettings) {
        return m_callbacks.saveSettings(showErrorMessage);
    }
    QString errorMessage;
    const bool saved = m_settingsPageController->save(
                showErrorMessage, &errorMessage);
    if (!saved && showErrorMessage && !errorMessage.isEmpty()) {
        showParameterCritical(QStringLiteral("设置保存失败"), errorMessage);
    }
    return saved;
}

void TemplateEditorPage::applyRecipeProfileToUi(
    const RecipeProfile &settings)
{
    if (m_callbacks.applyRecipeProfileToUi) {
        m_callbacks.applyRecipeProfileToUi(settings);
        return;
    }
    QSignalBlocker targetBlocker(m_view.dateEdit);
    QSignalBlocker thresholdBlocker(m_view.lineEdit_yuzhi);
    m_view.dateEdit->setPlainText(settings.targetText);
    m_view.lineEdit_yuzhi->setText(
                QString::number(settings.imageThresholdPercent));
}

QWidget *TemplateEditorPage::dialogParent() const
{
    return m_view.parentWidget;
}

bool TemplateEditorPage::isInspectionBusy() const
{
    return m_inspectionService->runtimeSnapshot().isInspectionBusy();
}

bool TemplateEditorPage::isCameraOpen() const
{
    return m_inspectionService->isCameraOpen();
}

bool TemplateEditorPage::templateOperationActive() const
{
    return m_captureState != CaptureState::Idle;
}

TemplateEditorPage::CaptureState TemplateEditorPage::captureState() const
{
    return m_captureState;
}

bool TemplateEditorPage::stopTemplatePreview()
{
    if (m_captureState != CaptureState::Previewing) {
        return true;
    }
    ++m_previewSessionId;
    return m_inspectionService->stopTemplatePreview();
}

void TemplateEditorPage::resetTemplateCaptureState()
{
    if (!stopTemplatePreview()) {
        if (m_callbacks.updateOperationUiState) {
            m_callbacks.updateOperationUiState();
        }
        return;
    }
    if (m_captureState != CaptureState::Previewing) {
        ++m_previewSessionId;
    }
    m_captureState = CaptureState::Idle;
    m_lastPreviewFrame.release();
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
}

bool TemplateEditorPage::startTemplatePreview()
{
    if (!isCameraOpen()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先点击【打开相机】！"));
        return false;
    }
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("当前正在进行正式检测，请先点击【停止识别】。"));
        return false;
    }
    if (m_inspectionService->isCapturing()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("相机采集线程仍在运行，请先停止当前任务。"));
        return false;
    }
    const InspectionCameraParameterResult exposure =
            m_inspectionService->applyCameraExposure(
                m_settingsService->current().cameraExposure);
    if (!exposure.success) {
        showParameterWarning(
                    QStringLiteral("警告"),
                    QStringLiteral("制作模板前应用相机曝光失败：\n%1")
                    .arg(exposure.diagnostic));
        return false;
    }
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    m_lastPreviewFrame.release();
    ++m_previewSessionId;
    m_captureState = CaptureState::Previewing;
    if (m_callbacks.clearTransientView) {
        m_callbacks.clearTransientView();
    }
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
    QString errorMessage;
    const int rotationCode = m_view.comboBox_2
            ? m_view.comboBox_2->currentIndex() : 0;
    const int channelCode = m_view.comboBox_5
            ? m_view.comboBox_5->currentIndex() : 0;
    if (!m_inspectionService->startTemplatePreview(
            m_previewSessionId, rotationCode, channelCode,
            &errorMessage)) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    errorMessage.isEmpty()
                    ? QStringLiteral("实时取景线程启动失败。")
                    : errorMessage);
        resetTemplateCaptureState();
        return false;
    }
    updateImageDisplayStatusText(
                QStringLiteral(
                    "实时取景中，请调整产品位置，确认后点击【拍照并开始框选】。"));
    return true;
}

bool TemplateEditorPage::freezeTemplatePreview()
{
    if (m_captureState != CaptureState::Previewing) {
        return false;
    }
    if (m_lastPreviewFrame.empty()) {
        showParameterInfo(
                    QStringLiteral("提示"),
                    QStringLiteral("相机尚未返回有效画面，请稍候再点击。"));
        return false;
    }
    if (!stopTemplatePreview()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("实时取景线程尚未停止，请稍后重试。"));
        return false;
    }
    m_captureState = CaptureState::Frozen;
    m_inspectionService->replaceCurrentCameraImage(m_lastPreviewFrame);
    if (m_callbacks.displayPreviewFrame) {
        m_callbacks.displayPreviewFrame(m_lastPreviewFrame);
    }
    const QString modeId = currentDetectModeId();
    const bool needsTemplateDrawing =
            isSingleTemplateRecipeMode(modeId)
            || isWordFamilyMode(modeId);
    clearBarcodeTemplateValidation();
    imageLabel->setBarcodeRegionRequired(
                modeId == BarcodeWordDetectionMode);
    imageLabel->setTemplateDrawingEnabled(needsTemplateDrawing);
    if (needsTemplateDrawing) {
        imageLabel->resetDrawingStep();
        showTemplateGuideForCurrentMode();
    } else {
        hideTemplateGuide();
        updateImageDisplayStatusText(
                    QStringLiteral(
                        "当前画面已冻结，如需调整请点击【重新取景】。"));
    }
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
    return true;
}

void TemplateEditorPage::handleTemplateCaptureButton()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("当前正在进行正式检测，请先点击【停止识别】。"));
        return;
    }
    if (!isCameraOpen()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先点击【打开相机】！"));
        return;
    }
    if (m_captureState == CaptureState::Previewing) {
        freezeTemplatePreview();
        return;
    }
    if (m_captureState == CaptureState::Frozen
            && (!imageLabel->getTrackingRect().isNull()
                || !imageLabel->getBarcodeRect().isNull()
                || !imageLabel->getDetectionPoly().isEmpty())) {
        QMessageBox box(dialogParent());
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QStringLiteral("重新取景"));
        box.setText(QStringLiteral(
                        "重新取景会清空当前已经绘制的框线。\n\n是否继续？"));
        QPushButton *continueButton = box.addButton(
                    QStringLiteral("重新取景"), QMessageBox::AcceptRole);
        QPushButton *cancelButton = box.addButton(
                    QStringLiteral("取消"), QMessageBox::RejectRole);
        box.setDefaultButton(cancelButton);
        box.exec();
        if (box.clickedButton() != continueButton) {
            return;
        }
    }
    startTemplatePreview();
}

void TemplateEditorPage::handlePreviewFrame(
        quint64 sessionId, const cv::Mat &image)
{
    m_inspectionService->acknowledgeTemplatePreviewFrame(sessionId);
    if (m_captureState != CaptureState::Previewing
            || sessionId != m_previewSessionId || image.empty()) {
        return;
    }
    m_lastPreviewFrame = image.clone();
    if (m_callbacks.displayPreviewFrame) {
        m_callbacks.displayPreviewFrame(m_lastPreviewFrame);
    }
    updateImageDisplayStatusText(
                QStringLiteral(
                    "实时取景中，请调整产品位置，确认后点击【拍照并开始框选】。"));
}

void TemplateEditorPage::handlePreviewFailure(
        quint64 sessionId, const QString &reason)
{
    if (m_captureState != CaptureState::Previewing
            || sessionId != m_previewSessionId) {
        return;
    }
    resetTemplateCaptureState();
    imageLabel->setTemplateDrawingEnabled(false);
    if (m_view.statusLabel) {
        m_view.statusLabel->setText(
                    isCameraOpen()
                    ? QStringLiteral("模板实时取景失败，相机已打开")
                    : QStringLiteral("模板实时取景失败，相机已关闭"));
    }
    updateImageDisplayStatusText(
                QStringLiteral("实时取景失败，请检查相机后重试。"));
    showParameterWarning(
                QStringLiteral("实时取景失败"), reason);
}

void TemplateEditorPage::selectPublishedRecipeForCurrentMode()
{
    if (isInspectionBusy() || templateOperationActive()) {
        QMessageBox::warning(
                    dialogParent(),
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止识别或退出模板制作，再选择产品配方。"));
        return;
    }
    selectPublishedRecipe();
}

void TemplateEditorPage::saveCurrentTemplate()
{
    if (isInspectionBusy()
            || m_captureState == CaptureState::Previewing) {
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
                m_view.lineEdit_tissueRoughnessThreshold
                ->text().trimmed().toDouble(&thresholdValid);
        if (!thresholdValid || roughness <= 0.0) {
            showParameterWarning(
                        QStringLiteral("参数错误"),
                        QStringLiteral("纸巾粗糙度阈值必须大于0。"));
            return;
        }
        bool accepted = false;
        const QString displayName = QInputDialog::getText(
                    dialogParent(),
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
        if (!m_templateService->beginNew(recipe, &errorMessage)
                || !m_templateService->replaceDraft(
                    recipe, QMap<QString, QString>(), &errorMessage)) {
            showParameterCritical(
                        QStringLiteral("严重警告"), errorMessage);
            return;
        }
        PreparedRecipeSnapshot prepared;
        if (!m_templateService->publish(

                    &prepared,
                    &errorMessage)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "纸巾产品配方保存失败，上一完整版本保持不变：\n%1")
                        .arg(errorMessage));
            return;
        }
        m_templateService->setActivePreparedRecipe(prepared);
        m_currentTemplateDisplayName = displayName;
        m_currentTemplateNameVisible = true;
        const QString modeId = detectionModeUiId(mode);
        m_templateService->rememberPublishedRecipe(
                    modeId, recipe.recipeId);
        saveSettings(false);
        updateCurrentTemplateName();
        showParameterInfo(
                    QStringLiteral("成功"),
                    QStringLiteral("纸巾产品配方已事务保存。"));
        return;
    }
    if (!m_inspectionService->hasCurrentCameraImage()
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

    const QString targetText = m_view.dateEdit->toPlainText().trimmed();
    int threshold = RecipeProfile::DefaultImageThresholdPercent;
    if (characterMode
            && (!parseIntValue(m_view.lineEdit_yuzhi->text(), &threshold)
                || threshold < 0 || threshold > 100)) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral("图像合格阈值必须是0到100之间的整数。"));
        return;
    }

    bool accepted = false;
    const QString displayName = QInputDialog::getText(
                dialogParent(),
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
    if (!m_templateService->beginNew(recipe, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    const QString workspacePath =
            m_templateService->workspacePath();
    if (workspacePath.isEmpty()) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("无法创建配方编辑工作区。"));
        return;
    }

    const cv::Mat rawImage =
            m_inspectionService->currentCameraImageClone();
    const QSize labelSize = imageLabel->size();
    const QSize imageSize(rawImage.cols, rawImage.rows);
    const QPixmap *pixmap = imageLabel->pixmap();
    const QSize displayedSize = pixmap && !pixmap->isNull()
            ? pixmap->size()
            : imageSize.scaled(labelSize, Qt::KeepAspectRatio);
    const TemplateDisplayGeometry displayGeometry = {
        labelSize, displayedSize, imageSize
    };
    const TemplateProfileGeometry profileGeometry =
            m_templateService->buildProfileGeometry(
                trackingUi, barcodeUi, dateUi,
                barcodeMode, displayGeometry);
    if (!profileGeometry.valid) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    profileGeometry.errorMessage);
        return;
    }

    RecipeProfile profile;
    profile.name = displayName;
    profile.targetText = targetText;
    profile.imageThresholdPercent = threshold;
    profile.trackingRoi = profileGeometry.trackingRoi;

    std::vector<cv::Point2f> stampPolygon;
    cv::Mat stampRing;
    if (mode == DetectionMode::Stamp) {
        QMessageBox::information(
                    dialogParent(),
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

    QMap<QString, QString> sources;
    InitialRecipeProfileAssets assets;
    assets.profileIndex = 0;
    assets.rawImage = rawImage;
    assets.trackingImageRect = profileGeometry.trackingImageRect;
    assets.stampPolygon = stampPolygon;
    assets.datePolygon = profileGeometry.datePolygon;
    assets.barcodePolygon = profileGeometry.barcodePolygon;
    assets.stampRing = stampRing;
    if (!m_templateService->stageInitialProfileAssets(
            assets, &recipe, &profile, &sources, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("配方资产写入失败：\n%1")
                    .arg(errorMessage));
        return;
    }

    recipe.profiles.append(profile);
    if (!m_templateService->replaceDraft(
            recipe, sources, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(
            &prepared, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("产品配方保存失败；上一完整版本保持不变：\n%1")
                    .arg(errorMessage));
        return;
    }
    m_templateService->setActivePreparedRecipe(prepared);

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

    m_templateService->rememberPublishedRecipe(
                uiModeId, recipe.recipeId);
    saveSettings(false);
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    hideTemplateGuide();
    resetTemplateCaptureState();
    m_view.statusLabel->setText(
                isCameraOpen()
                ? QStringLiteral("配方保存完成，相机已打开")
                : QStringLiteral("配方保存完成，相机已关闭"));
    if (characterMode) {
        QMessageBox splitMessageBox(dialogParent());
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
            showManualCharacterTemplateEditorDialog();
        }
    } else {
        showParameterInfo(
                    QStringLiteral("成功"),
                    QStringLiteral("产品模板和全部资源已事务保存。"));
    }
}

const std::vector<WordTemplateProfile> &
TemplateEditorPage::wordTemplateProfiles() const
{
    return m_templateService->wordProfiles();
}

PreparedRecipeSnapshot TemplateEditorPage::activePreparedRecipe() const
{
    return m_templateService->activePreparedRecipe();
}

QString TemplateEditorPage::currentTemplateDisplayName() const
{
    return m_currentTemplateDisplayName;
}

void TemplateEditorPage::setCurrentTemplateDisplayName(
    const QString &displayName)
{
    m_currentTemplateDisplayName = displayName;
}

void TemplateEditorPage::setCurrentTemplateNameVisible(bool visible)
{
    m_currentTemplateNameVisible = visible;
}

int TemplateEditorPage::currentWordTemplateEditIndex() const
{
    return m_currentWordTemplateEditIndex;
}

bool TemplateEditorPage::barcodeTemplateReadable() const
{
    return m_barcodeTemplateReadable;
}

QRect TemplateEditorPage::validatedBarcodeRect() const
{
    return m_validatedBarcodeRect;
}

QString TemplateEditorPage::validatedBarcodeText() const
{
    return m_validatedBarcodeText;
}

void TemplateEditorPage::acceptBarcodeTemplateValidation(
    const QRect &barcodeRect,
    const QString &barcodeText)
{
    m_barcodeTemplateReadable = true;
    m_validatedBarcodeRect = barcodeRect;
    m_validatedBarcodeText = barcodeText;
}

void TemplateEditorPage::clearBarcodeTemplateValidation()
{
    m_barcodeTemplateReadable = false;
    m_validatedBarcodeRect = QRect();
    m_validatedBarcodeText.clear();
}

bool TemplateEditorPage::validateBarcodeTemplateRect(
    const QRect &uiBarcodeRect,
    const BarcodeDecodeOptions &options,
    BarcodeReadResult *barcode,
    QString *failureReason)
{
    const cv::Mat templateImage =
            m_inspectionService->currentCameraImageClone();
    const QPixmap *displayedPixmap = imageLabel->pixmap();
    const QSize imageSize(templateImage.cols, templateImage.rows);
    const TemplateDisplayGeometry geometry = {
        imageLabel->size(),
        displayedPixmap && !displayedPixmap->isNull()
                ? displayedPixmap->size()
                : imageSize.scaled(imageLabel->size(), Qt::KeepAspectRatio),
        imageSize
    };
    const QRect sourceRect = m_templateService->mapDisplayRectToImage(
                uiBarcodeRect.normalized(), geometry);
    return m_templateService->validateBarcodeTemplate(
                templateImage, sourceRect, options,
                barcode, failureReason);
}

BarcodeDecodeOptions TemplateEditorPage::barcodeTemplateValidationOptions() const
{
    BarcodeRecipeParameters parameters;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (currentDetectModeId() == BarcodeWordDetectionMode
            && profileIndex >= 0
            && profileIndex
               < static_cast<int>(m_templateService->wordProfiles().size())) {
        parameters = m_templateService->wordProfiles()[
                    static_cast<std::size_t>(profileIndex)]
                .settings.barcodeParameters;
    }
    BarcodeDecodeOptions options;
    options.formatMask = parameters.formatMask;
    options.roiPaddingPercent = parameters.roiPaddingPercent;
    options.maxDecodeTimeMs = parameters.maxDecodeTimeMs;
    options.enableFallback = parameters.enableFallback;
    return options;
}


void TemplateEditorPage::updateCurrentTemplateName()
{
    QString templateName = "--";

    if (m_currentTemplateNameVisible
            && !m_currentTemplateDisplayName.trimmed().isEmpty()) {
        templateName = m_currentTemplateDisplayName.trimmed();
    }

    m_view.currentTemplateName->setText(templateName);

}


void TemplateEditorPage::setupTemplateGuide()
{
    if (m_templateGuideFrame) {
        return;
    }

    m_templateGuideFrame = new QFrame(m_view.imagedisplayBox);
    m_templateGuideFrame->setObjectName("templateGuideFrame");
    m_templateGuideFrame->setFrameShape(QFrame::NoFrame);
    m_templateGuideFrame->setStyleSheet(
                "#templateGuideFrame {"
                "background-color: transparent;"
                "border: none;"
                "}");
    m_templateGuideFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_templateGuideFrame->installEventFilter(m_view.eventFilterTarget);

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

    m_view.verticalLayout_InnerImg->insertWidget(0, m_templateGuideFrame);
    hideTemplateGuide();
}

void TemplateEditorPage::adjustTemplateGuideHeight()
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

void TemplateEditorPage::updateTemplateGuideText(const QString &title, const QString &body)
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

void TemplateEditorPage::hideTemplateGuide()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->hide();
    }
}

void TemplateEditorPage::updateImageDisplayStatusText(const QString &body)
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

void TemplateEditorPage::showTemplateGuideForCurrentMode()
{
    const int modeIndex = m_view.comboBox_4->currentIndex();
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

void TemplateEditorPage::handleTemplateGuideEvent(const QString &eventName, int pointCount)
{
    if (!imageLabel) {
        return;
    }

    const int modeIndex = m_view.comboBox_4->currentIndex();
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
                QMessageBox::warning(dialogParent(),
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

            QMessageBox saveMessageBox(dialogParent());
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

void TemplateEditorPage::setupManualCharacterCropUi()
{
    if (m_manualCharacterCropButton) {
        return;
    }

    if (!m_view.manualCharacterCropButton) {
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

    m_manualCharacterCropButton = m_view.manualCharacterCropButton;
    m_manualCharacterCropButton->setStyleSheet(splitPushButtonStyle);
    m_manualCharacterCropButton->setMinimumHeight(42);
    m_manualCharacterCropButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_manualCharacterCropButton->setToolTip("打开当前产品模板的喷码区域图，手动框选字符并批量保存字符模板图片。");
    m_manualCharacterCropButton->installEventFilter(m_view.eventFilterTarget);

    connect(m_manualCharacterCropButton, &QPushButton::clicked,
            this, &TemplateEditorPage::showManualCharacterTemplateEditorDialog);
    refreshWordTemplateEditorCombo();
}



void TemplateEditorPage::setupRecipeProfileDirtyTracking()
{
    m_templateTargetLabelText = m_view.label ? m_view.label->text() : QString("目标字符内容:");
    m_templateThresholdLabelText = m_view.label_4 ? m_view.label_4->text() : QString("图像合格阈值:");

    if (m_view.dateEdit) {
        connect(m_view.dateEdit, &QTextEdit::textChanged, this, [this]() {
            if ((m_callbacks.isUpdatingSettingsUi && m_callbacks.isUpdatingSettingsUi()) || (m_callbacks.isApplyingSettings && m_callbacks.isApplyingSettings())) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_templateService->wordProfiles().empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_templateService->isActive())) {
                refreshTemplateTargetTextDirty();
            }
        });
    }
    if (m_view.lineEdit_yuzhi) {
        connect(m_view.lineEdit_yuzhi, &QLineEdit::textChanged, this, [this](const QString &) {
            if ((m_callbacks.isUpdatingSettingsUi && m_callbacks.isUpdatingSettingsUi()) || (m_callbacks.isApplyingSettings && m_callbacks.isApplyingSettings())) {
                return;
            }
            if ((isWordFamilyMode(currentDetectModeId())
                 && !m_templateService->wordProfiles().empty())
                    || (isSingleTemplateRecipeMode(currentDetectModeId())
                        && m_templateService->isActive())) {
                refreshTemplateImageThresholdDirty();
            }
        });
    }
}

void TemplateEditorPage::refreshTemplateTargetTextDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_templateService->wordProfiles().size())) {
        const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
        dirty = (m_view.dateEdit->toPlainText() != profile.settings.targetText);
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateService->isActive()
               && m_templateService->draft().profiles.size() == 1) {
        dirty = m_view.dateEdit->toPlainText()
                != m_templateService->draft()
                   .profiles.first().targetText;
    }

    m_settingsEditState->setTemplateTargetDirty(dirty);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::refreshTemplateImageThresholdDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_templateService->wordProfiles().size())) {
        int thresholdValue = 0;
        if (!parseIntValue(m_view.lineEdit_yuzhi->text(), &thresholdValue)) {
            dirty = true;
        } else {
            const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
            dirty = (thresholdValue != static_cast<int>(profile.settings.imageThresholdPercent));
        }
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateService->isActive()
               && m_templateService->draft().profiles.size() == 1) {
        int thresholdValue = 0;
        if (!parseIntValue(m_view.lineEdit_yuzhi->text(), &thresholdValue)) {
            dirty = true;
        } else {
            dirty = thresholdValue
                    != static_cast<int>(
                        m_templateService->draft()
                        .profiles.first().imageThresholdPercent);
        }
    }

    m_settingsEditState->setTemplateThresholdDirty(dirty);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::refreshRecipeProfileDirty()
{
    refreshTemplateTargetTextDirty();
    refreshTemplateImageThresholdDirty();
}

void TemplateEditorPage::markTemplateTargetTextDirty()
{
    m_settingsEditState->setTemplateTargetDirty(true);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::markTemplateImageThresholdDirty()
{
    m_settingsEditState->setTemplateThresholdDirty(true);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::clearTemplateTargetTextDirty()
{
    m_settingsEditState->setTemplateTargetDirty(false);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::clearTemplateImageThresholdDirty()
{
    m_settingsEditState->setTemplateThresholdDirty(false);
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::clearRecipeProfileDirty()
{
    m_settingsEditState->clearTemplateDirty();
    updateRecipeProfileDirtyUi();
}

void TemplateEditorPage::updateRecipeProfileDirtyUi()
{
    if (m_view.label) {
        m_view.label->setText(m_settingsEditState->isTemplateTargetDirty()
                           ? m_templateTargetLabelText + " *"
                           : m_templateTargetLabelText);
    }
    if (m_view.label_4) {
        m_view.label_4->setText(m_settingsEditState->isTemplateThresholdDirty()
                             ? m_templateThresholdLabelText + " *"
                             : m_templateThresholdLabelText);
    }
}

void TemplateEditorPage::showManualCharacterTemplateEditorDialog()
{
    if (!m_templateService->activePreparedRecipe()
            || !m_templateService->activePreparedRecipe()->recipe) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先选择已保存的产品配方。"));
        return;
    }
    const DetectionMode mode =
            m_templateService->activePreparedRecipe()->recipe->detectionMode;
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

void TemplateEditorPage::showStampCharacterTemplateEditorDialog()
{
    if (m_templateService->activePreparedRecipe()
            && m_templateService->activePreparedRecipe()->recipe
            && m_templateService->activePreparedRecipe()->recipe->detectionMode
               == DetectionMode::Stamp) {
        editActiveRecipeCharacterAssets(0);
    }
}

void TemplateEditorPage::showPublishedRecipeCharacterTemplateEditorDialog(
        int profileIndex)
{
    editActiveRecipeCharacterAssets(profileIndex);
}

void TemplateEditorPage::editActiveRecipeCharacterAssets(
        int profileIndex)
{
    if (!m_templateService->activePreparedRecipe()
            || !m_templateService->activePreparedRecipe()->recipe
            || !m_templateService->isActive()
            || profileIndex < 0
            || profileIndex >= m_templateService->activePreparedRecipe()->profiles.size()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("当前配方没有可编辑的Profile。"));
        return;
    }
    const DetectionMode mode =
            m_templateService->activePreparedRecipe()->recipe->detectionMode;
    if (mode != DetectionMode::Stamp
            && mode != DetectionMode::Word
            && mode != DetectionMode::BarcodeWord) {
        return;
    }

    const PreparedRecipeProfile &prepared =
            m_templateService->activePreparedRecipe()->profiles.at(profileIndex);
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

    CharacterTemplateEditorDialog dialog(
                rawImage.copy(QRect(bounds.x, bounds.y,
                                    bounds.width, bounds.height)),
                prepared.definition,
                dialogParent());
    if (dialog.exec() != QDialog::Accepted
            || dialog.savedCount() <= 0) {
        return;
    }

    ProductRecipe recipe = m_templateService->draft();
    QMap<QString, QString> sources =
            m_templateService->assetSourcePaths();
    RecipeProfile profile = dialog.resultProfile();
    const QMap<QString, QImage> images = dialog.characterImages();
    QString errorMessage;
    if (!m_templateService->stageCharacterAssets(
            profileIndex, images, &recipe, &profile,
            &sources, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"), errorMessage);
        return;
    }
    recipe.profiles[profileIndex] = profile;
    if (!m_templateService->replaceDraft(
            recipe, sources, &errorMessage)) {
        showParameterCritical(QStringLiteral("严重警告"), errorMessage);
        return;
    }
    PreparedRecipeSnapshot updated;
    if (!m_templateService->publish(
            &updated, &errorMessage)) {
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("字符模板修改未写入正式配方，原配方保持不变。"),
                    errorMessage);
        return;
    }
    m_templateService->setActivePreparedRecipe(updated);
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

void TemplateEditorPage::setupWordTemplateEditorCombo()
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

    {

        if (m_view.textsure_btn) m_view.textsure_btn->setStyleSheet(commonPushButtonStyle);
        if (m_view.batchTextsure_btn) m_view.batchTextsure_btn->setStyleSheet(commonPushButtonStyle);
        if (m_view.batchImageThresholdButton) m_view.batchImageThresholdButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.WriteVDpushButton) m_view.WriteVDpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_7) m_view.pushButton_7->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_3) m_view.pushButton_3->setStyleSheet(commonPushButtonStyle);
        if (m_view.sureButton) m_view.sureButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_9) m_view.pushButton_9->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_12) m_view.pushButton_12->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_tissueRoughnessThreshold) m_view.pushButton_tissueRoughnessThreshold->setStyleSheet(commonPushButtonStyle);
        if (m_view.plcmodebtn) m_view.plcmodebtn->setStyleSheet(commonPushButtonStyle);
        if (m_view.ConnectpushButton) m_view.ConnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.DisconnectpushButton) m_view.DisconnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_8) m_view.pushButton_8->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_browseImageSavePath) m_view.pushButton_browseImageSavePath->setStyleSheet(commonPushButtonStyle);
    }

    if (m_view.textsure_btn && m_view.batchTextsure_btn) {
        m_view.textsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_view.batchTextsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QHBoxLayout *buttonLayout = qobject_cast<QHBoxLayout *>(m_view.textsure_btn->parentWidget()
                ? m_view.textsure_btn->parentWidget()->layout()
                : nullptr);
        if (buttonLayout) {
            buttonLayout->setStretch(0, 1);
            buttonLayout->setStretch(1, 1);
        }

        m_view.textsure_btn->setToolTip("只保存当前编辑模板的目标字符，并重新加载该模板的字符图片。");
        m_view.batchTextsure_btn->setToolTip("把当前目标字符保存到所有已选择的字库模板，并分别重新加载字符图片。");
        m_view.textsure_btn->installEventFilter(m_view.eventFilterTarget);
        m_view.batchTextsure_btn->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.pushButton_browseImageSavePath) {
        m_view.pushButton_browseImageSavePath->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.pushButton_7) {
        m_view.pushButton_7->setToolTip("确认当前选择的颜色通道，用于后续图像处理和识别。");
        m_view.pushButton_7->installEventFilter(m_view.eventFilterTarget);
    }

    {
        if (m_view.checkBox) {
            m_view.checkBox->setToolTip(
                        "控制检测的触发方式。\n"
                        "勾选：使用 PLC 外部触发信号控制相机拍照和检测，启动前必须连接 PLC。\n"
                        "不勾选：使用软件软触发，启动后由相机连续采集并检测。");
            m_view.checkBox->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.pushButton_10) {
            m_view.pushButton_10->setToolTip("清空当前尚未发出的剔除队列。\n适用于异常停机、误判、手动停止后，防止之前累计的剔除信号继续输出。");
            m_view.pushButton_10->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_4) {
            m_view.label_4->setToolTip("图像判定合格的分数阈值。\n识别匹配分数低于该值时，通常判为不合格；数值越高，判定越严格。");
            m_view.label_4->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_27) {
            m_view.label_27->setToolTip("设置图像进入识别前的旋转方向。\n当相机安装方向、产品摆放方向和模板方向不一致时，需要调整这里。");
            m_view.label_27->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_16) {
            m_view.label_16->setToolTip("设置相机增益。\n增益越高画面越亮，但噪声也可能增加；一般先调曝光，曝光不足时再调增益。");
            m_view.label_16->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_14) {
            m_view.label_14->setToolTip("PLC拍照信号保持多久。\n相机偶尔漏拍、触发不稳定时可适当加大；正常不要过大，避免影响下一次触发节拍。");
            m_view.label_14->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_13) {
            m_view.label_13->setToolTip("相机收到 PLC 拍照信号后，再等待多久才真正曝光采图。\n通常在拍照距离基本正确后，用它做小范围微调。\n画面中产品还没到合适位置就加大；产品已经走过或喷码偏后就减小。");
            m_view.label_13->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_6) {
            m_view.label_6->setToolTip("检测拍照点到剔除机构中心的实际产线距离。\n剔除太早通常加大；剔除太晚通常减小。");
            m_view.label_6->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_10) {
            m_view.label_10->setToolTip("剔除机构保持动作的时长。\n不合格品剔不干净就加大；影响相邻合格品或动作拖尾就减小。");
            m_view.label_10->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_17) {
            m_view.label_17->setToolTip("选择第几路剔除输出或第几个剔除口。\n现场有多个气嘴、推杆或剔除工位时使用；填错会从错误位置剔除。");
            m_view.label_17->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_8) {
            m_view.label_8->setToolTip("上游传感器触发点到相机拍照中心的实际产线距离。\nPLC 根据这个距离判断产品走到相机位置后再发出拍照信号。\n画面中产品还没到拍照位置，说明触发偏早，适当加大；产品已经走过拍照位置，说明触发偏晚，适当减小。");
            m_view.label_8->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.comboBox_3) {
            m_view.comboBox_3->setToolTip("PLC触发工作模式。\n连续触发模式：产线连续经过时，PLC按连续节拍触发相机采图和检测。\n间歇触发模式：产品分批、停顿或按间隔到位时，PLC按间歇方式触发采图和检测。");
            m_view.comboBox_3->installEventFilter(m_view.eventFilterTarget);
        }
    }

    if (m_view.VideoShoot) {
        m_view.VideoShoot->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.batchTextsure_btn) {
        m_view.batchTextsure_btn->hide();
    }
    if (m_view.batchImageThresholdButton) {
        m_view.batchImageThresholdButton->setToolTip(
                    "把当前图像合格阈值保存到所有已选择的产品模板。");
        m_view.batchImageThresholdButton->installEventFilter(m_view.eventFilterTarget);
        m_view.batchImageThresholdButton->hide();
    }

    if (m_wordTemplateEditComboBox || !m_view.dateEdit
            || !m_view.lineEdit_yuzhi) {
        return;
    }

    QWidget *parentWidget = m_view.dateEdit->parentWidget();
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
                &TemplateEditorPage::selectPublishedRecipe);

        m_wordTemplateEditComboBox->hide();
        m_wordTemplateEditWidget->hide();
    }

    if (m_callbacks.updateTissueVisibility) {
        m_callbacks.updateTissueVisibility();
    }
}



void TemplateEditorPage::clearWordMultiTemplateState()
{
    clearBarcodeTemplateValidation();
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    m_currentTemplateDisplayName.clear();
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    {
        QSignalBlocker targetBlocker(m_view.dateEdit);
        QSignalBlocker thresholdBlocker(m_view.lineEdit_yuzhi);
        m_view.dateEdit->clear();
        m_view.lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

void TemplateEditorPage::clearSingleTemplateRecipeState()
{
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    m_currentTemplateDisplayName.clear();
    {
        QSignalBlocker targetBlocker(m_view.dateEdit);
        QSignalBlocker thresholdBlocker(m_view.lineEdit_yuzhi);
        m_view.dateEdit->clear();
        m_view.lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

QString TemplateEditorPage::detectModeIdForIndex(int index) const
{
    return TemplateModeMemory::modeIdForIndex(index);
}

QString TemplateEditorPage::currentDetectModeId() const
{
    return detectModeIdForIndex(m_view.comboBox_4 ? m_view.comboBox_4->currentIndex() : 1);
}

void TemplateEditorPage::restoreTemplatesForMode(
        const QString &modeId,
        bool showMessage)
{
    clearWordMultiTemplateState();
    clearSingleTemplateRecipeState();
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    const QString recipeId = m_templateService->modeMemory()
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

    m_templateService->forgetPublishedRecipe(modeId);
    saveSettings(false);
    updateCurrentTemplateName();
    if (showMessage) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("上次产品配方无法恢复，记录已清除；不会读取旧模板目录：\n%1")
                    .arg(errorMessage));
    }
}

void TemplateEditorPage::refreshWordTemplateEditorCombo()
{
    const bool isWordMode = isWordFamilyMode(currentDetectModeId());
    const bool isSingleMode = isSingleTemplateRecipeMode(
                currentDetectModeId());
    const bool isStampMode =
            currentDetectModeId() == QStringLiteral("stamp_detection");
    const bool isTemplateMode = isWordMode || isSingleMode;
    DetectionMode selectedMode = DetectionMode::Word;
    const bool isRecipeMode = detectionModeFromUiId(
                currentDetectModeId(), &selectedMode);
    const bool hasWordProfiles = isWordMode && !m_templateService->wordProfiles().empty();

    if (m_view.batchTextsure_btn) {
        m_view.batchTextsure_btn->setVisible(isWordMode && m_templateService->wordProfiles().size() > 1);
    }
    if (m_view.batchImageThresholdButton) {
        m_view.batchImageThresholdButton->setVisible(
                    isWordMode && m_templateService->wordProfiles().size() > 1);
    }

    if (m_manualCharacterCropButton) {
        m_manualCharacterCropButton->setVisible(isWordMode || isStampMode);
    }

    if (m_publishTemplateGroupButton) {
        const bool canPublishWordGroup =
                isWordMode
                && !m_templateService->wordProfiles().empty()
                && m_templateService->isActive();
        const bool canPublishSingleTemplate =
                isSingleMode
                && m_templateService->isActive();
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
            for (int i = 0; i < static_cast<int>(m_templateService->wordProfiles().size()); ++i) {
                const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(i)];
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
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
            profileIndex = 0;
        }
        setCurrentWordTemplateEditIndex(profileIndex);
    } else {
        m_currentWordTemplateEditIndex = -1;
    }
}

void TemplateEditorPage::applyWordTemplateEditorSelection(int comboIndex)
{
    if (!m_wordTemplateEditComboBox || comboIndex < 0) {
        return;
    }

    bool ok = false;
    const int profileIndex = m_wordTemplateEditComboBox->itemData(comboIndex).toInt(&ok);
    if (!ok || profileIndex < 0 || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
        return;
    }

    setCurrentWordTemplateEditIndex(profileIndex);
}

void TemplateEditorPage::setCurrentWordTemplateEditIndex(int profileIndex)
{
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
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

    const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
    {
        QSignalBlocker blocker(m_view.dateEdit);
        m_view.dateEdit->setPlainText(profile.settings.targetText);
    }
    {
        QSignalBlocker blocker(m_view.lineEdit_yuzhi);
        m_view.lineEdit_yuzhi->setText(QString::number(static_cast<int>(profile.settings.imageThresholdPercent)));
    }
    refreshRecipeProfileDirty();

    qDebug() << "[WORD_TEMPLATE_PROFILE] editing profile:"
             << profileIndex
             << profile.name
             << "threshold:" << profile.settings.imageThresholdPercent;

    displayWordTemplateRawImage(profile);
}

void TemplateEditorPage::publishCurrentWordTemplateGroup()
{
    publishCurrentRecipeSession();
}

void TemplateEditorPage::publishCurrentSingleTemplateRecipe()
{
    publishCurrentRecipeSession();
}

void TemplateEditorPage::publishCurrentRecipeSession()
{
    if (!m_templateService->isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先通过【保存模板】创建产品配方。"));
        return;
    }
    QString errorMessage;
    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(
            &prepared, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("产品配方事务保存失败，原配方保持不变：\n%1")
                    .arg(errorMessage));
        return;
    }
    m_templateService->setActivePreparedRecipe(prepared);
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("当前产品配方已完整保存。"));
}

void TemplateEditorPage::selectPublishedRecipe()
{
    if (isInspectionBusy() || templateOperationActive()) {
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

    RecipeCatalog catalog;
    QString catalogError;
    if (!m_templateService->listRecipes(&catalog, &catalogError)) {
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

    RecipeSelectionDialog dialog(matchingRecipes, dialogParent());
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
            m_templateService->draft();
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

bool TemplateEditorPage::activatePublishedTissueRecipe(
        const QString &recipeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_templateService->loadPreparedRecipe(
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
    if (!m_templateService->beginEdit(
                recipeId,
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
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName =
            prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    m_view.lineEdit_tissueRoughnessThreshold->setText(
                QString::number(
                    prepared->tissue.roughnessThreshold,
                    'f', 3));
    const QString modeId =
            detectionModeUiId(DetectionMode::Tissue);
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorPage::activatePublishedWordRecipe(
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
    if (!m_templateService->loadPreparedRecipe(
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
    if (!m_templateService->beginEdit(
                recipeId, &sessionError)) {
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
    m_templateService->replaceWordProfiles(loadedProfiles);
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    if (!m_templateService->wordProfiles().empty()) {
        setCurrentWordTemplateEditIndex(0);
    }
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();

    qDebug() << "[RECIPE_SELECT] selected prepared word recipe:"
             << prepared->recipe->recipeId
             << prepared->recipe->displayName
             << "profiles:" << m_templateService->wordProfiles().size();
    return true;
}


bool TemplateEditorPage::activatePublishedSingleTemplateRecipe(
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
    if (!m_templateService->loadPreparedRecipe(
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

    QString sessionError;
    if (!m_templateService->beginEdit(

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
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    applyRecipeProfileToUi(profile.definition);
    m_currentTemplateNameVisible = true;
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorPage::republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()
            || m_templateService->draft().profiles.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前已发布单模板没有有效编辑会话。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_templateService->draft().profiles.first().name;
    updated.assetKeys =
            m_templateService->draft().profiles.first().assetKeys;
    if (!m_templateService->updateProfile(
                0, updated, errorMessage)) {
        return false;
    }

    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(

                &prepared,
                errorMessage)) {
        return false;
    }
    m_templateService->setActivePreparedRecipe(prepared);
    m_templateService->rememberPublishedRecipe(
                currentDetectModeId(),
                prepared->recipe->recipeId);
    saveSettings(false);
    return true;
}

int TemplateEditorPage::currentWordTemplateProfileIndex() const
{
    if (m_currentWordTemplateEditIndex < 0
            || m_currentWordTemplateEditIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
        return -1;
    }
    return m_currentWordTemplateEditIndex;
}

void TemplateEditorPage::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    if (!m_view.image_undetected || profile.rawImage.empty()) {
        return;
    }
    const QImage image = imageFromBgrMat(profile.rawImage);
    if (image.isNull()) {
        return;
    }
    m_view.image_undetected->setScaledContents(false);
    m_view.image_undetected->setAlignment(Qt::AlignCenter);
    m_view.image_undetected->setAutoFitPixmap(
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

bool TemplateEditorPage::loadWordDigitTemplatesFromProfile(
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

void TemplateEditorPage::refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const
{
    if (profile) {
        profile->settings.name = profile->name;
    }
}

bool TemplateEditorPage::saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()
            || profileIndex < 0
            || profileIndex >= static_cast<int>(
                m_templateService->wordProfiles().size())
            || profileIndex
               >= m_templateService->draft().profiles.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方Profile无效。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_templateService->draft()
            .profiles.at(profileIndex).name;
    updated.assetKeys =
            m_templateService->draft()
            .profiles.at(profileIndex).assetKeys;
    return m_templateService->updateProfile(
                profileIndex, updated, errorMessage);
}

bool TemplateEditorPage::publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage)
{
    QVector<int> profileIndexes;
    profileIndexes.append(profileIndex);
    return publishWordTemplateRecipeEdits(
                profileIndexes, errorMessage);
}

bool TemplateEditorPage::publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方没有有效编辑会话。");
        }
        return false;
    }

    ProductRecipe candidate = m_templateService->draft();
    for (int profileIndex : profileIndexes) {
        if (profileIndex < 0
                || profileIndex >= static_cast<int>(
                    m_templateService->wordProfiles().size())
                || profileIndex >= candidate.profiles.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "当前配方编辑Profile无效。");
            }
            return false;
        }
        RecipeProfile updated =
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(
                        profileIndex)].settings;
        updated.name = candidate.profiles.at(profileIndex).name;
        updated.assetKeys =
                candidate.profiles.at(profileIndex).assetKeys;
        candidate.profiles[profileIndex] = updated;
    }

    if (!m_templateService->replaceDraft(
                candidate,
                m_templateService->assetSourcePaths(),
                errorMessage)) {
        return false;
    }
    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(

                &prepared,
                errorMessage)) {
        return false;
    }
    m_templateService->setActivePreparedRecipe(prepared);
    m_templateService->rememberPublishedRecipe(
                currentDetectModeId(),
                prepared->recipe->recipeId);
    saveSettings(false);
    return true;
}

void TemplateEditorPage::applyCurrentTargetText()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改目标字符。"));
        return;
    }
    if (!m_templateService->isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先创建或加载产品配方。"));
        return;
    }

    const QString targetText =
            m_view.dateEdit->toPlainText();
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

        const WordTemplateProfile previous =
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile profile = previous;
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
        if (!m_templateService->replaceWordProfile(
                    profileIndex, profile)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }

        QString publishError;
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            m_templateService->replaceWordProfile(
                        profileIndex, previous);
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
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
                m_templateService->draft()
                .profiles.first();
        settings.targetText = targetText;
        QString publishError;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
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
                        "目标字符已事务保存。"));
        return;
    }

    showParameterInfoAsError(
                QStringLiteral("提示"),
                QStringLiteral("当前模式不使用目标字符。"));
}

void TemplateEditorPage::applyBatchTargetText()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentTargetText();
        return;
    }
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板字符。"));
        return;
    }
    if (!m_templateService->isActive()
            || m_templateService->wordProfiles().empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const QString targetText =
            m_view.dateEdit->toPlainText();
    const QStringList targetUnits =
            preparedRecipeTargetUnits(targetText);
    if (targetUnits.isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("目标字符不能为空。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_templateService->wordProfiles();
    std::vector<WordTemplateProfile> updated = previous;
    for (WordTemplateProfile &profile : updated) {
        std::vector<cv::Mat> templates;
        std::vector<int> indexes;
        QString validationError;
        if (!loadWordDigitTemplatesFromProfile(
                    profile,
                    targetUnits,
                    &templates,
                    &indexes,
                    &validationError)) {
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
    m_templateService->replaceWordProfiles(updated);

    QVector<int> indexes;
    for (int i = 0;
         i < static_cast<int>(m_templateService->wordProfiles().size());
         ++i) {
        indexes.append(i);
    }
    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                indexes, &publishError)) {
        m_templateService->replaceWordProfiles(previous);
        if (m_templateService->activePreparedRecipe()
                && m_templateService->activePreparedRecipe()->recipe) {
            m_templateService->beginEdit(

                        m_templateService->activePreparedRecipe()->recipe->recipeId,
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

void TemplateEditorPage::applyCurrentImageThreshold()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                m_view.lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_templateService->isActive()) {
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
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile profile = previous;
        profile.settings.imageThresholdPercent = threshold;
        if (!m_templateService->replaceWordProfile(
                    profileIndex, profile)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            m_templateService->replaceWordProfile(
                        profileIndex, previous);
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
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
                m_templateService->draft()
                .profiles.first();
        settings.imageThresholdPercent = threshold;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
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


    clearTemplateImageThresholdDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("图像阈值已事务保存：%1")
                .arg(threshold));
}

void TemplateEditorPage::applyCurrentTissueThreshold()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改纸巾配方阈值。"));
        return;
    }
    bool valid = false;
    const double threshold =
            m_view.lineEdit_tissueRoughnessThreshold
            ->text().trimmed().toDouble(&valid);
    if (!valid || !std::isfinite(threshold) || threshold <= 0.0) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral("纸巾粗糙度阈值必须是大于0的有限数字。"));
        return;
    }
    if (!m_templateService->isActive()
            || !m_templateService->activePreparedRecipe()
            || !m_templateService->activePreparedRecipe()->recipe
            || m_templateService->activePreparedRecipe()->recipe->detectionMode
               != DetectionMode::Tissue) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先使用【保存模板】创建或加载纸巾产品配方。"));
        return;
    }

    const QString recipeId =
            m_templateService->activePreparedRecipe()->recipe->recipeId;
    ProductRecipe candidate = m_templateService->draft();
    candidate.tissueParameters.roughnessThreshold = threshold;
    QString errorMessage;
    PreparedRecipeSnapshot updated;
    if (!m_templateService->replaceDraft(
            candidate, QMap<QString, QString>(), &errorMessage)
            || !m_templateService->publish(
                &updated, &errorMessage)) {
        m_templateService->beginEdit(
                    recipeId, nullptr);
        m_view.lineEdit_tissueRoughnessThreshold->setText(
                    QString::number(
                        m_templateService->activePreparedRecipe()->tissue
                        .roughnessThreshold,
                        'f', 3));
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("纸巾阈值未生效，正式配方保持不变。"),
                    errorMessage);
        return;
    }

    m_templateService->setActivePreparedRecipe(updated);
    m_view.lineEdit_tissueRoughnessThreshold->setText(
                QString::number(threshold, 'f', 3));
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("纸巾粗糙度阈值已事务保存到当前产品配方。"));
}

void TemplateEditorPage::applyBatchImageThreshold()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentImageThreshold();
        return;
    }
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                m_view.lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_templateService->isActive()
            || m_templateService->wordProfiles().empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_templateService->wordProfiles();
    std::vector<WordTemplateProfile> updated = previous;
    QVector<int> profileIndexes;
    for (int i = 0;
         i < static_cast<int>(updated.size());
         ++i) {
        updated[static_cast<std::size_t>(i)]
                .settings.imageThresholdPercent = threshold;
        profileIndexes.append(i);
    }
    m_templateService->replaceWordProfiles(updated);

    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                profileIndexes, &publishError)) {
        m_templateService->replaceWordProfiles(previous);
        if (m_templateService->activePreparedRecipe()
                && m_templateService->activePreparedRecipe()->recipe) {
            m_templateService->beginEdit(

                        m_templateService->activePreparedRecipe()->recipe->recipeId,
                        nullptr);
        }
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "批量图像阈值未生效，正式配方保持不变。"),
                    publishError);
        return;
    }


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
