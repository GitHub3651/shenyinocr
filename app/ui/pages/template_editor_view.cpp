// 文件作用：本文件用于构造模板编辑动态控件，并维护模板制作视图的显示和交互状态。
// 主要职责：构造模板编辑动态控件，并维护模板制作视图的显示和交互状态。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
/**
 * @file ui/pages/template_editor_view.cpp
 * @brief 模板引导、脏状态和字符资产编辑交互。
 */

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

// 函数说明：updateCurrentTemplateName 函数更新或应用对应的配置和状态。
void TemplateEditorPage::updateCurrentTemplateName()
{
    QString templateName = "--";

    if (m_currentTemplateNameVisible
            && !m_currentTemplateDisplayName.trimmed().isEmpty()) {
        templateName = m_currentTemplateDisplayName.trimmed();
    }

    m_view.lineEdit_currentRecipeName->setText(templateName);

}


// 函数说明：setupTemplateGuide 函数更新或应用对应的配置和状态。
void TemplateEditorPage::setupTemplateGuide()
{
    if (m_templateGuideFrame) {
        return;
    }

    m_templateGuideFrame = new QFrame(m_view.groupBox_imageDisplay);
    m_templateGuideFrame->setObjectName("frame_templateGuide");
    m_templateGuideFrame->setFrameShape(QFrame::NoFrame);
    m_templateGuideFrame->setStyleSheet(
                "#frame_templateGuide {"
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

    m_view.verticalLayout_imageDisplay->insertWidget(0, m_templateGuideFrame);
    hideTemplateGuide();
}

// 函数说明：adjustTemplateGuideHeight 函数更新或应用对应的配置和状态。
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

// 函数说明：updateTemplateGuideText 函数更新或应用对应的配置和状态。
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

// 函数说明：hideTemplateGuide 函数实现名称所表示的处理步骤。
void TemplateEditorPage::hideTemplateGuide()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->hide();
    }
}

// 函数说明：updateImageDisplayStatusText 函数更新或应用对应的配置和状态。
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

// 函数说明：showTemplateGuideForCurrentMode 函数实现名称所表示的处理步骤。
void TemplateEditorPage::showTemplateGuideForCurrentMode()
{
    const int modeIndex = m_view.comboBox_detectionMode->currentIndex();
    const QString modeId = detectModeIdForIndex(modeIndex);
    DetectionMode mode = DetectionMode::Word;
    detectionModeFromUiId(modeId, &mode);

    if (isSingleTemplateRecipeMode(modeId)) {
        updateTemplateGuideText(
                    mode == DetectionMode::Ocr
                        ? "深度模型模板制作"
                        : "模板匹配模板制作",
                    "请按住鼠标左键拖动，框选定位区域。");
        return;
    }

    if (isWordFamilyMode(modeId)) {
        const bool barcodeWordMode =
                modeId == detectionModeUiId(DetectionMode::BarcodeWord);
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

// 函数说明：handleTemplateGuideEvent 函数执行对应事件或业务处理。
void TemplateEditorPage::handleTemplateGuideEvent(const QString &eventName, int pointCount)
{
    if (!imageLabel) {
        return;
    }

    const int modeIndex = m_view.comboBox_detectionMode->currentIndex();
    const QString modeId = detectModeIdForIndex(modeIndex);
    DetectionMode mode = DetectionMode::Word;
    detectionModeFromUiId(modeId, &mode);
    if (!isSingleTemplateRecipeMode(modeId)
            && !isWordFamilyMode(modeId)) {
        if (m_templateGuideFrame && m_templateGuideFrame->isVisible()) {
            hideTemplateGuide();
        }
        return;
    }

    const bool barcodeWordMode =
            modeId == detectionModeUiId(DetectionMode::BarcodeWord);
    const bool guideVisible =
            m_templateGuideFrame && m_templateGuideFrame->isVisible();
    const QString title = barcodeWordMode
            ? "二维码+三期模板制作"
            : (mode == DetectionMode::Word
               ? "字库匹配模板制作"
               : (mode == DetectionMode::Ocr
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
        QString failureReason;
        if (!validateBarcodeTemplateRect(
                    barcodeRect,
                    barcodeTemplateValidationOptions(),
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
                        currentDetectModeId()
                        == detectionModeUiId(DetectionMode::BarcodeWord)
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

// 函数说明：setupManualCharacterCropUi 函数更新或应用对应的配置和状态。
void TemplateEditorPage::setupManualCharacterCropUi()
{
    if (m_manualCharacterCropButton) {
        return;
    }

    if (!m_view.pushButton_editCharacterTemplates) {
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

    m_manualCharacterCropButton = m_view.pushButton_editCharacterTemplates;
    m_manualCharacterCropButton->setStyleSheet(splitPushButtonStyle);
    m_manualCharacterCropButton->setMinimumHeight(42);
    m_manualCharacterCropButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_manualCharacterCropButton->setToolTip("打开当前产品模板的喷码区域图，手动框选字符并批量保存字符模板图片。");
    m_manualCharacterCropButton->installEventFilter(m_view.eventFilterTarget);

    connect(m_manualCharacterCropButton, &QPushButton::clicked,
            this, &TemplateEditorPage::showManualCharacterTemplateEditorDialog);
    refreshWordTemplateEditorCombo();
}



// 函数说明：setupRecipeProfileDirtyTracking 函数更新或应用对应的配置和状态。
void TemplateEditorPage::setupRecipeProfileDirtyTracking()
{
    m_templateTargetLabelText = m_view.label_targetText
            ? m_view.label_targetText->text()
            : QString("目标字符内容:");
    m_templateThresholdLabelText = m_view.label_imageThreshold
            ? m_view.label_imageThreshold->text()
            : QString("图像合格阈值:");

    if (m_view.textEdit_targetText) {
        connect(m_view.textEdit_targetText, &QTextEdit::textChanged, this, [this]() {
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
    if (m_view.lineEdit_imageThreshold) {
        connect(m_view.lineEdit_imageThreshold, &QLineEdit::textChanged, this, [this](const QString &) {
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

// 函数说明：refreshTemplateTargetTextDirty 函数更新或应用对应的配置和状态。
void TemplateEditorPage::refreshTemplateTargetTextDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_templateService->wordProfiles().size())) {
        const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
        dirty = (m_view.textEdit_targetText->toPlainText() != profile.settings.targetText);
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateService->isActive()
               && m_templateService->draft().profiles.size() == 1) {
        dirty = m_view.textEdit_targetText->toPlainText()
                != m_templateService->draft()
                   .profiles.first().targetText;
    }

    m_settingsEditState->setTemplateTargetDirty(dirty);
    updateRecipeProfileDirtyUi();
}

// 函数说明：refreshTemplateImageThresholdDirty 函数更新或应用对应的配置和状态。
void TemplateEditorPage::refreshTemplateImageThresholdDirty()
{
    bool dirty = false;
    const int profileIndex = currentWordTemplateProfileIndex();
    if (isWordFamilyMode(currentDetectModeId())
            && profileIndex >= 0
            && profileIndex < static_cast<int>(m_templateService->wordProfiles().size())) {
        int thresholdValue = 0;
        if (!parseIntValue(m_view.lineEdit_imageThreshold->text(), &thresholdValue)) {
            dirty = true;
        } else {
            const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
            dirty = (thresholdValue != static_cast<int>(profile.settings.imageThresholdPercent));
        }
    } else if (isSingleTemplateRecipeMode(currentDetectModeId())
               && m_templateService->isActive()
               && m_templateService->draft().profiles.size() == 1) {
        int thresholdValue = 0;
        if (!parseIntValue(m_view.lineEdit_imageThreshold->text(), &thresholdValue)) {
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

// 函数说明：refreshRecipeProfileDirty 函数更新或应用对应的配置和状态。
void TemplateEditorPage::refreshRecipeProfileDirty()
{
    refreshTemplateTargetTextDirty();
    refreshTemplateImageThresholdDirty();
}

// 函数说明：clearTemplateTargetTextDirty 函数停止流程、清理状态或释放对应资源。
void TemplateEditorPage::clearTemplateTargetTextDirty()
{
    m_settingsEditState->setTemplateTargetDirty(false);
    updateRecipeProfileDirtyUi();
}

// 函数说明：clearTemplateImageThresholdDirty 函数停止流程、清理状态或释放对应资源。
void TemplateEditorPage::clearTemplateImageThresholdDirty()
{
    m_settingsEditState->setTemplateThresholdDirty(false);
    updateRecipeProfileDirtyUi();
}

// 函数说明：clearRecipeProfileDirty 函数停止流程、清理状态或释放对应资源。
void TemplateEditorPage::clearRecipeProfileDirty()
{
    m_settingsEditState->clearTemplateDirty();
    updateRecipeProfileDirtyUi();
}

// 函数说明：updateRecipeProfileDirtyUi 函数更新或应用对应的配置和状态。
void TemplateEditorPage::updateRecipeProfileDirtyUi()
{
    if (m_view.label_targetText) {
        m_view.label_targetText->setText(
                    m_settingsEditState->isTemplateTargetDirty()
                           ? m_templateTargetLabelText + " *"
                           : m_templateTargetLabelText);
    }
    if (m_view.label_imageThreshold) {
        m_view.label_imageThreshold->setText(
                    m_settingsEditState->isTemplateThresholdDirty()
                             ? m_templateThresholdLabelText + " *"
                             : m_templateThresholdLabelText);
    }
}

// 函数说明：showManualCharacterTemplateEditorDialog 函数实现名称所表示的处理步骤。
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

// 函数说明：editActiveRecipeCharacterAssets 函数实现名称所表示的处理步骤。
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
