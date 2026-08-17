#include "ui/pages/template_editor_page.h"
#include "ui/pages/template_editor_support.h"

#include "contracts/detection_mode.h"
#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "ui/dialogs/character_template_editor_dialog.h"
#include "ui/widgets/image_label.h"
#include "ui/pages/machine_settings_page.h"
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

using namespace TemplateEditorSupport;

TemplateEditorPage::TemplateEditorPage(
    const TemplateEditorViewBindings &view,
    TemplateApplicationService *templateService,
    InspectionApplicationService *inspectionService,
    SettingsApplicationService *settingsService,
    MachineSettingsPage *settingsPageController,
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
    const CameraParameterResultDto exposure =
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
            TemplateBarcodeValidationResult barcode;
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

void TemplateEditorPage::setCurrentTemplateNameVisible(bool visible)
{
    m_currentTemplateNameVisible = visible;
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
    const TemplateBarcodeValidationOptions &options,
    TemplateBarcodeValidationResult *barcode,
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

TemplateBarcodeValidationOptions
TemplateEditorPage::barcodeTemplateValidationOptions() const
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
    TemplateBarcodeValidationOptions options;
    options.formatMask = parameters.formatMask;
    options.roiPaddingPercent = parameters.roiPaddingPercent;
    options.maxDecodeTimeMs = parameters.maxDecodeTimeMs;
    options.enableFallback = parameters.enableFallback;
    return options;
}
