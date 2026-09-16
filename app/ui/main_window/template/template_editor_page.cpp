#include "ui/main_window/template/template_editor_page.h"

#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "contracts/detection_mode.h"
#include "system_support/logging/log_categories.h"
#include "ui/main_window/settings/settings_edit_state.h"
#include "ui/main_window/template/character_editor/character_template_editor_dialog.h"
#include "ui/main_window/template/save/template_save_dialog.h"
#include "ui/main_window/template/selection/template_selection_dialog.h"
#include "ui/main_window/inspection_image_canvas.h"
#include "ui_detection_settings_page.h"
#include "ui_image_settings_page.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QPolygonF>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

namespace {

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

QImage imageFromBgrMat(const cv::Mat &image)
{
    if (image.empty()) {
        return QImage();
    }
    cv::Mat converted;
    if (image.channels() == 1) {
        cv::cvtColor(image, converted, cv::COLOR_GRAY2RGB);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, converted, cv::COLOR_BGRA2RGBA);
        return QImage(converted.data, converted.cols, converted.rows,
                      static_cast<int>(converted.step),
                      QImage::Format_RGBA8888).copy();
    } else {
        cv::cvtColor(image, converted, cv::COLOR_BGR2RGB);
    }
    return QImage(converted.data, converted.cols, converted.rows,
                  static_cast<int>(converted.step),
                  QImage::Format_RGB888).copy();
}

bool isMultiTemplateMode(DetectionMode mode)
{
    return detectionModeDescriptor(mode).trackingKind
            == DetectionTrackingKind::MultipleTemplates;
}

QString templateGuideTitle(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp:
        return QStringLiteral("钢印检测模板制作");
    case DetectionMode::Word:
        return QStringLiteral("字库匹配模板制作");
    case DetectionMode::Ocr:
        return QStringLiteral("深度 OCR 模板制作");
    case DetectionMode::BarcodeWord:
        return QStringLiteral("二维码+三期模板制作");
    case DetectionMode::Tissue:
        return QStringLiteral("纸巾检测");
    }
    return QStringLiteral("模板制作");
}

QString templateGuideDetectionRegionName(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp:
        return QStringLiteral("钢印检测区域");
    case DetectionMode::Word:
        return QStringLiteral("文字检测区域");
    case DetectionMode::Ocr:
        return QStringLiteral("OCR 检测区域");
    case DetectionMode::BarcodeWord:
        return QStringLiteral("日期检测区域");
    case DetectionMode::Tissue:
        break;
    }
    return QStringLiteral("检测区域");
}

int drawingStepCount(DetectionMode mode)
{
    return mode == DetectionMode::Stamp
            ? 4 : mode == DetectionMode::BarcodeWord ? 3 : 2;
}

int drawingStepNumber(DetectionMode mode, InspectionImageCanvas::DrawingStep step)
{
    switch (step) {
    case InspectionImageCanvas::DrawingStep::TrackingAnchor:
    case InspectionImageCanvas::DrawingStep::StampAnchor:
        return 1;
    case InspectionImageCanvas::DrawingStep::DetectionPolygon:
    case InspectionImageCanvas::DrawingStep::BarcodeRegion:
    case InspectionImageCanvas::DrawingStep::StampPolygon:
        return 2;
    case InspectionImageCanvas::DrawingStep::DateAnchor:
        return 3;
    case InspectionImageCanvas::DrawingStep::DatePolygon:
        return mode == DetectionMode::Stamp ? 4 : 3;
    case InspectionImageCanvas::DrawingStep::Idle:
    case InspectionImageCanvas::DrawingStep::Complete:
        break;
    }
    return 0;
}

QString drawingRegionName(DetectionMode mode, InspectionImageCanvas::DrawingStep step)
{
    switch (step) {
    case InspectionImageCanvas::DrawingStep::TrackingAnchor:
        switch (mode) {
        case DetectionMode::Word:
            return QStringLiteral("文字检测区域定位参考区域");
        case DetectionMode::Ocr:
            return QStringLiteral("OCR 检测区域定位参考区域");
        case DetectionMode::BarcodeWord:
            return QStringLiteral("二维码与生产日期区域定位参考区域");
        case DetectionMode::Stamp:
        case DetectionMode::Tissue:
            break;
        }
        return QStringLiteral("定位参考区域");
    case InspectionImageCanvas::DrawingStep::DetectionPolygon:
        return templateGuideDetectionRegionName(mode);
    case InspectionImageCanvas::DrawingStep::BarcodeRegion:
        return QStringLiteral("二维码区域");
    case InspectionImageCanvas::DrawingStep::StampAnchor:
        return QStringLiteral("钢印区域定位参考区域（吸管口）");
    case InspectionImageCanvas::DrawingStep::StampPolygon:
        return QStringLiteral("钢印检测区域");
    case InspectionImageCanvas::DrawingStep::DateAnchor:
        return QStringLiteral("生产日期检测区域定位参考区域");
    case InspectionImageCanvas::DrawingStep::DatePolygon:
        return QStringLiteral("生产日期检测区域");
    case InspectionImageCanvas::DrawingStep::Idle:
    case InspectionImageCanvas::DrawingStep::Complete:
        break;
    }
    return QStringLiteral("检测区域");
}

QString emphasizedDrawingRegion(
        DetectionMode mode, InspectionImageCanvas::DrawingStep step)
{
    QString color = QStringLiteral("#157A3D");
    switch (step) {
    case InspectionImageCanvas::DrawingStep::TrackingAnchor:
    case InspectionImageCanvas::DrawingStep::DateAnchor:
        color = QStringLiteral("#0067C0");
        break;
    case InspectionImageCanvas::DrawingStep::BarcodeRegion:
    case InspectionImageCanvas::DrawingStep::StampAnchor:
        color = QStringLiteral("#B85C00");
        break;
    case InspectionImageCanvas::DrawingStep::StampPolygon:
        color = QStringLiteral("#C43D24");
        break;
    case InspectionImageCanvas::DrawingStep::DetectionPolygon:
    case InspectionImageCanvas::DrawingStep::DatePolygon:
    case InspectionImageCanvas::DrawingStep::Idle:
    case InspectionImageCanvas::DrawingStep::Complete:
        break;
    }
    return QStringLiteral(
                "<b><span style=\"color:%1;\">%2</span></b>")
            .arg(color, drawingRegionName(mode, step).toHtmlEscaped());
}

bool isDrawingPolygonStep(InspectionImageCanvas::DrawingStep step)
{
    return step == InspectionImageCanvas::DrawingStep::DetectionPolygon
            || step == InspectionImageCanvas::DrawingStep::DatePolygon
            || step == InspectionImageCanvas::DrawingStep::StampPolygon;
}

}

TemplateEditorPage::TemplateEditorPage(
        QWidget &dialogParent,
        Ui::MainWindow &mainWindowUi,
        Ui::DetectionSettingsPage &detectionSettingsUi,
        Ui::ImageSettingsPage &imageSettingsUi,
        TemplateApplicationService &templateService,
        InspectionApplicationService &inspectionService,
        SettingsApplicationService &settingsService,
        SettingsEditState &settingsEditState)
    : m_dialogParent(dialogParent),
      m_mainWindowUi(mainWindowUi),
      m_detectionSettingsUi(detectionSettingsUi),
      m_imageSettingsUi(imageSettingsUi),
      m_templateService(templateService),
      m_inspectionService(inspectionService),
      m_settingsService(settingsService),
      m_settingsEditState(settingsEditState)
{
    m_targetTextLabelText = m_detectionSettingsUi.label_targetText->text();
    m_imageThresholdLabelText = m_detectionSettingsUi.label_imageThreshold->text();
    m_detectionSettingsUi.lineEdit_imageThreshold->setValidator(
                new QIntValidator(0, 100,
                                  m_detectionSettingsUi.lineEdit_imageThreshold));
    connect(&m_inspectionService,
            &InspectionApplicationService::templatePreviewFailed,
            this, &TemplateEditorPage::handlePreviewFailure,
            Qt::QueuedConnection);
    connect(&m_inspectionService,
            &InspectionApplicationService::templatePreviewStopped,
            this, [this]() {
        if (m_captureState == CaptureState::Previewing) {
            m_captureState = CaptureState::Idle;
            emit operationUiRefreshRequested();
        }
    }, Qt::QueuedConnection);
    setupCurrentTemplateEditor();
    connect(m_mainWindowUi.inspectionImageCanvas, &InspectionImageCanvas::templateDrawingChanged,
            this, &TemplateEditorPage::handleTemplateDrawingChanged);
    setupManualCharacterCropUi();
    setupTemplateDirtyTracking();
    connectPageActions();
}

void TemplateEditorPage::applyOperationState(
        OperationUiState requestedState,
        const OperationUiSnapshot &snapshot)
{
    OperationUiSnapshot::Access editAccess = snapshot.templateEditing;
    if (m_selectedTemplateInvalid) {
        editAccess.enabled = false;
        editAccess.disabledReason = QStringLiteral(
                    "当前模板状态异常，请移除或重新选择模板。");
    }
    applyOperationUiAccess(m_detectionSettingsUi.comboBox_currentEditTemplate,
                           snapshot.templateEditing);
    applyOperationUiAccess(m_detectionSettingsUi.toolButton_removeCurrentTemplate,
                           snapshot.templateEditing);
    applyOperationUiAccess(m_mainWindowUi.toolButton_editCharacterTemplates,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.textEdit_targetText,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.lineEdit_imageThreshold,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.pushButton_applyTargetText,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.pushButton_applyBatchTargetText,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.pushButton_applyImageThreshold,
                           editAccess);
    applyOperationUiAccess(m_detectionSettingsUi.pushButton_applyBatchImageThreshold,
                           editAccess);
    applyOperationUiAccess(m_mainWindowUi.toolButton_saveTemplate,
                           snapshot.saveTemplate);
    if (requestedState == OperationUiState::Detecting) {
        showInspectionStatus();
    } else if (requestedState == OperationUiState::Stopping) {
        cancelTemplateDrawing();
    }
}

bool TemplateEditorPage::isCameraOpen() const
{
    return m_inspectionService.isCameraOpen();
}

void TemplateEditorPage::showInfo(
        const QString &title, const QString &message)
{
    QMessageBox::information(&m_dialogParent, title, message);
}

void TemplateEditorPage::showWarning(
        const QString &title, const QString &message)
{
    QMessageBox::warning(&m_dialogParent, title, message);
}

void TemplateEditorPage::showCritical(
        const QString &title, const QString &message)
{
    QMessageBox::critical(&m_dialogParent, title, message);
}

bool TemplateEditorPage::templateOperationActive() const
{
    return m_captureState != CaptureState::Idle;
}

TemplateEditorPage::CaptureState TemplateEditorPage::captureState() const
{
    return m_captureState;
}

bool TemplateEditorPage::stopTemplatePreview(bool writeLog)
{
    if (m_captureState != CaptureState::Previewing) {
        return true;
    }
    const OperationResult result = m_inspectionService.stopTemplatePreview();
    if (writeLog) {
        if (result.isSuccess()) {
            qCInfo(logTemplate).noquote()
                    << "event=template.preview_stopped";
        } else {
            qCWarning(logTemplate).noquote()
                    << QStringLiteral(
                        "event=template.preview_stop_failed code=%1 reason=%2")
                       .arg(result.error.code, result.error.userMessage);
        }
    }
    return result.isSuccess();
}

void TemplateEditorPage::resetTemplateCaptureState(bool writePreviewStopLog)
{
    if (!stopTemplatePreview(writePreviewStopLog)) {
        return;
    }
    m_captureState = CaptureState::Idle;
    emit operationUiRefreshRequested();
}

bool TemplateEditorPage::startTemplatePreview()
{
    const OperationResult result = m_inspectionService.startTemplatePreview(
                m_imageSettingsUi.comboBox_imageRotation->currentIndex(),
                m_imageSettingsUi.comboBox_colorChannel->currentIndex());
    if (!result.isSuccess()) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.preview_start_failed code=%1 reason=%2")
                   .arg(result.error.code, result.error.userMessage);
        showWarning(QStringLiteral("实时取景失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("无法开始实时取景，请重试。")
                    : result.error.userMessage);
        return false;
    }
    cancelTemplateDrawing();
    m_captureState = CaptureState::Previewing;
    showTemplateCaptureStatus(
                QStringLiteral("实时取景中，调整产品位置后点击拍照并开始框选。"));
    emit operationUiRefreshRequested();
    qCInfo(logTemplate).noquote()
            << "event=template.preview_started";
    return true;
}

bool TemplateEditorPage::freezeTemplatePreview()
{
    if (m_captureState != CaptureState::Previewing
            || !m_inspectionService.hasCurrentCameraImage()) {
        showInfo(QStringLiteral("提示"),
                 QStringLiteral("相机尚未返回有效画面，请稍候再点击。"));
        return false;
    }
    if (!stopTemplatePreview()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("实时取景尚未停止，请稍后重试。"));
        return false;
    }
    const cv::Mat image = m_inspectionService.currentCameraImageClone();
    m_captureState = CaptureState::Frozen;
    emit previewFramePresentationRequested(image);
    DetectionMode mode = DetectionMode::Stamp;
    detectionModeFromUiId(currentDetectModeId(), &mode);
    clearBarcodeTemplateValidation();
    m_mainWindowUi.inspectionImageCanvas->beginTemplateDrawing(mode);
    if (mode == DetectionMode::Tissue) {
        hideTemplateGuide();
    }
    emit operationUiRefreshRequested();
    return true;
}

void TemplateEditorPage::handleTemplateCaptureButton()
{
    if (m_captureState == CaptureState::Previewing) {
        freezeTemplatePreview();
        return;
    }
    if (m_captureState == CaptureState::Frozen
            && (!m_mainWindowUi.inspectionImageCanvas->trackingAnchorRect().isNull()
                || !m_mainWindowUi.inspectionImageCanvas->barcodeRect().isNull()
                || !m_mainWindowUi.inspectionImageCanvas->stampAnchorRect().isNull()
                || !m_mainWindowUi.inspectionImageCanvas->datePolygon().isEmpty()
                || !m_mainWindowUi.inspectionImageCanvas->stampPolygon().isEmpty())) {
        if (QMessageBox::question(
                &m_dialogParent, QStringLiteral("重新取景"),
                QStringLiteral("重新取景会清空当前框线，是否继续？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
    }
    startTemplatePreview();
}

void TemplateEditorPage::handlePreviewFailure(
        const QString &reason)
{
    if (m_captureState != CaptureState::Previewing) {
        return;
    }
    qCWarning(logTemplate).noquote()
            << QStringLiteral("event=template.preview_failed reason=%1")
               .arg(reason);
    resetTemplateCaptureState(false);
    cancelTemplateDrawing();
    showWarning(QStringLiteral("实时取景失败"), reason);
}

void TemplateEditorPage::setupCurrentTemplateEditor()
{
    connect(m_detectionSettingsUi.comboBox_currentEditTemplate,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this, [this](int index) {
        loadTemplateAtIndex(index, true);
    });
    connect(m_detectionSettingsUi.toolButton_removeCurrentTemplate,
            &QToolButton::clicked,
            this, &TemplateEditorPage::removeCurrentTemplate);
    refreshCurrentTemplateEditor();
}

QString TemplateEditorPage::detectModeIdForIndex(int index) const
{
    if (index < 0
            || index >= m_detectionSettingsUi.comboBox_detectionMode->count()) {
        return QString();
    }
    const QVariant data = m_detectionSettingsUi.comboBox_detectionMode->itemData(index);
    return data.isValid() && !data.toString().isEmpty()
            ? data.toString()
            : detectionModeUiId(detectionModeDescriptors()
                                .value(index).mode);
}

QString TemplateEditorPage::currentDetectModeId() const
{
    return detectModeIdForIndex(
                m_detectionSettingsUi.comboBox_detectionMode->currentIndex());
}

QStringList TemplateEditorPage::currentModeTemplatePaths() const
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        return QStringList();
    }
    return m_settingsService.current()
            .detectionSchemes.templatePaths(mode);
}

void TemplateEditorPage::refreshCurrentTemplateEditor()
{
    DetectionMode mode = DetectionMode::Tissue;
    detectionModeFromUiId(currentDetectModeId(), &mode);
    const bool usesTemplate = mode != DetectionMode::Tissue;
    const QStringList paths = currentModeTemplatePaths();
    const bool hasMultipleTemplates = paths.size() > 1;
    m_detectionSettingsUi.pushButton_applyBatchTargetText->setVisible(
                hasMultipleTemplates);
    m_detectionSettingsUi.pushButton_applyBatchImageThreshold->setVisible(
                hasMultipleTemplates);
    if (!usesTemplate) {
        QSignalBlocker blocker(m_detectionSettingsUi.comboBox_currentEditTemplate);
        m_detectionSettingsUi.comboBox_currentEditTemplate->clear();
        return;
    }
    const QString previous = m_detectionSettingsUi.comboBox_currentEditTemplate->currentData()
            .toString();
    QSignalBlocker blocker(m_detectionSettingsUi.comboBox_currentEditTemplate);
    m_detectionSettingsUi.comboBox_currentEditTemplate->clear();
    for (const QString &path : paths) {
        TemplateStoreError error;
        const TemplateSummary summary = m_templateService.readSummary(
                    path, mode, &error);
        const QString name = QFileInfo(path).fileName();
        m_detectionSettingsUi.comboBox_currentEditTemplate->addItem(
                    summary.valid ? name
                                  : name + QStringLiteral("（状态异常）"),
                    path);
        m_detectionSettingsUi.comboBox_currentEditTemplate->setItemData(
                    m_detectionSettingsUi.comboBox_currentEditTemplate->count() - 1,
                    path, Qt::ToolTipRole);
    }
    int index = m_detectionSettingsUi.comboBox_currentEditTemplate->findData(previous);
    if (index < 0 && !paths.isEmpty()) {
        index = 0;
    }
    m_detectionSettingsUi.comboBox_currentEditTemplate->setCurrentIndex(index);
    m_detectionSettingsUi.toolButton_removeCurrentTemplate->setEnabled(index >= 0);
}

bool TemplateEditorPage::loadTemplateAtIndex(
        int index, bool showMessage)
{
    if (index < 0
            || index >= m_detectionSettingsUi.comboBox_currentEditTemplate->count()) {
        clearTemplateState();
        return false;
    }
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        hideTemplateGuide();
        return false;
    }
    const QString path = m_detectionSettingsUi.comboBox_currentEditTemplate
            ->itemData(index).toString();
    const QString templateName = QFileInfo(path).fileName();
    QString errorMessage;
    if (!m_templateService.beginEdit(path, mode, &errorMessage)) {
        m_templateService.cancel();
        m_templateService.setActivePreparedTemplate(
                    PreparedTemplateSnapshot());
        m_selectedTemplateInvalid = true;
        hideTemplateGuide();
        emit operationUiRefreshRequested();
        if (showMessage) {
            showWarning(QStringLiteral("模板加载失败"), errorMessage);
        }
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.apply_failed name=%1 path=%2 reason=%3")
                   .arg(templateName,
                        QDir::toNativeSeparators(path), errorMessage);
        return false;
    }
    m_selectedTemplateInvalid = false;
    emit operationUiRefreshRequested();
    PreparedTemplateSnapshot prepared;
    if (!m_templateService.loadPreparedTemplate(
            path, mode, &prepared, &errorMessage)) {
        if (showMessage) {
            showWarning(QStringLiteral("模板状态异常"),
                        QStringLiteral("模板可以继续编辑，但目前不能用于检测：\n%1")
                        .arg(errorMessage));
        }
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.apply_failed name=%1 path=%2 reason=%3")
                   .arg(templateName,
                        QDir::toNativeSeparators(path), errorMessage);
        prepared.reset();
    }
    m_templateService.setActivePreparedTemplate(prepared);
    const EditableTemplate &editable = m_templateService.draft();
    applyTemplateSettingsToUi(editable.settings);
    if (!editable.rawImage.empty()) {
        emit previewFramePresentationRequested(editable.rawImage);
        showTemplateImageSource(templateName);
    } else {
        hideTemplateGuide();
    }
    clearTemplateDirty();
    if (prepared) {
        qCInfo(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.applied name=%1 path=%2")
                   .arg(templateName, QDir::toNativeSeparators(path));
    }
    return true;
}

void TemplateEditorPage::restoreTemplatesForMode(
        const QString &modeId, bool showMessage)
{
    refreshCurrentTemplateEditor();
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        clearTemplateState();
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.restore_failed mode=%1 reason=invalid_mode")
                   .arg(modeId.trimmed().isEmpty()
                        ? QStringLiteral("-") : modeId.trimmed());
        return;
    }
    const QString stableMode = detectionModeId(mode);
    if (mode == DetectionMode::Tissue) {
        clearTemplateState();
        qCInfo(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.restored mode=%1 count=0")
                   .arg(stableMode);
        return;
    }
    const int count = currentModeTemplatePaths().size();
    const bool loaded = loadTemplateAtIndex(
                m_detectionSettingsUi.comboBox_currentEditTemplate
                ->currentIndex(),
                showMessage);
    if (loaded || count == 0) {
        qCInfo(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.restored mode=%1 count=%2")
                   .arg(stableMode)
                   .arg(count);
    } else {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.restore_failed mode=%1 count=%2 reason=selected_template_invalid")
                   .arg(stableMode)
                   .arg(count);
    }
}

void TemplateEditorPage::clearTemplateState()
{
    m_templateService.cancel();
    m_templateService.setActivePreparedTemplate(
                PreparedTemplateSnapshot());
    m_selectedTemplateInvalid = false;
    clearTemplateDirty();
    hideTemplateGuide();
}

int TemplateEditorPage::currentTemplateIndex() const
{
    return m_detectionSettingsUi.comboBox_currentEditTemplate->currentIndex();
}

void TemplateEditorPage::selectTemplatesForCurrentMode()
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)
            || mode == DetectionMode::Tissue) {
        return;
    }
    TemplateSelectionDialog dialog(
                mode, currentModeTemplatePaths(),
                m_templateService, m_settingsService, &m_dialogParent);
    const bool applied = dialog.exec() == QDialog::Accepted;
    restoreTemplatesForMode(currentDetectModeId(), applied);
}

void TemplateEditorPage::removeCurrentTemplate()
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)
            || mode == DetectionMode::Tissue) {
        return;
    }
    const int index = currentTemplateIndex();
    if (index < 0) {
        return;
    }
    const OperationResult saved = m_settingsService.removeTemplatePaths(
                mode, QStringList()
                << m_detectionSettingsUi.comboBox_currentEditTemplate
                   ->itemData(index).toString());
    if (!saved.isSuccess()) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.selection_update_failed mode=%1 reason=%2")
                   .arg(detectionModeId(mode), saved.error.userMessage);
        showCritical(QStringLiteral("移除失败"), saved.error.userMessage);
        return;
    }
    restoreTemplatesForMode(currentDetectModeId(), false);
    qCInfo(logTemplate).noquote()
            << QStringLiteral(
                "event=templates.selection_updated mode=%1 count=%2")
               .arg(detectionModeId(mode))
               .arg(currentModeTemplatePaths().size());
}

void TemplateEditorPage::saveCurrentTemplate()
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("当前检测模式无效。"));
        return;
    }
    if (mode == DetectionMode::Tissue) {
        return;
    }
    if (m_captureState != CaptureState::Frozen
            || !m_inspectionService.hasCurrentCameraImage()) {
        if (m_templateService.isActive()
                && !m_templateService.currentDirectoryPath().isEmpty()) {
            saveCurrentDraft(true);
            return;
        }
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先制作并冻结模板画面。"));
        return;
    }
    if (m_mainWindowUi.inspectionImageCanvas->templateDrawingMode() != mode
            || !m_mainWindowUi.inspectionImageCanvas->isTemplateDrawingComplete()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先完成当前模式的全部模板框选步骤。"));
        return;
    }
    TemplateDrawingInput drawingInput;
    drawingInput.mode = mode;
    drawingInput.trackingAnchorRect =
            m_mainWindowUi.inspectionImageCanvas->trackingAnchorRect().normalized();
    drawingInput.barcodeRect = m_mainWindowUi.inspectionImageCanvas->barcodeRect().normalized();
    drawingInput.stampAnchorRect =
            m_mainWindowUi.inspectionImageCanvas->stampAnchorRect().normalized();
    drawingInput.datePolygon = m_mainWindowUi.inspectionImageCanvas->datePolygon();
    drawingInput.stampPolygon = m_mainWindowUi.inspectionImageCanvas->stampPolygon();
    if (mode == DetectionMode::BarcodeWord
            && (!m_barcodeTemplateReadable
                || m_validatedBarcodeRect
                   != drawingInput.barcodeRect)) {
        QString reason;
        if (!validateBarcodeTemplateRect(
                drawingInput.barcodeRect,
                barcodeTemplateValidationOptions(), &reason)) {
            showWarning(QStringLiteral("二维码扫描失败"), reason);
            return;
        }
        acceptBarcodeTemplateValidation(drawingInput.barcodeRect);
    }
    int threshold = TemplateSettings::DefaultImageThresholdPercent;
    if (mode != DetectionMode::Ocr
            && (!parseIntValue(m_detectionSettingsUi.lineEdit_imageThreshold->text(),
                               &threshold)
                || threshold < 0 || threshold > 100)) {
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("图像阈值必须是 0 到 100 的整数。"));
        return;
    }
    QString parentDirectory =
            m_settingsService.current().templateSaveDirectory;
    if (!QFileInfo(parentDirectory).isDir()) {
        parentDirectory = QStandardPaths::writableLocation(
                    QStandardPaths::DesktopLocation);
        if (!QFileInfo(parentDirectory).isDir()) {
            parentDirectory = QDir::homePath();
        }
    }
    parentDirectory = QDir(parentDirectory).absolutePath();

    TemplateSaveDialog saveDialog(parentDirectory, &m_dialogParent);
    if (saveDialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString name = saveDialog.templateName();
    parentDirectory = saveDialog.parentDirectory();
    const QString target = QDir(parentDirectory).filePath(name);
    const bool targetExists = QFileInfo::exists(target);
    if (targetExists
            && QMessageBox::question(
                &m_dialogParent, QStringLiteral("模板已存在"),
                QStringLiteral("模板“%1”已存在，是否完全覆盖？\n"
                               "确认后原模板文件夹中的全部内容都会被替换。")
                .arg(name), QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString errorMessage;
    if (!m_templateService.beginNew(mode, &errorMessage)) {
        showCritical(QStringLiteral("新建模板失败"), errorMessage);
        return;
    }
    const cv::Mat rawImage =
            m_inspectionService.currentCameraImageClone();
    const QPixmap *pixmap = m_mainWindowUi.inspectionImageCanvas->pixmap();
    const QSize imageSize(rawImage.cols, rawImage.rows);
    const TemplateDisplayGeometry display = {
        m_mainWindowUi.inspectionImageCanvas->size(),
        pixmap && !pixmap->isNull()
        ? pixmap->size()
        : imageSize.scaled(m_mainWindowUi.inspectionImageCanvas->size(), Qt::KeepAspectRatio),
        imageSize
    };
    const TemplateGeometryResult geometry =
            m_templateService.buildGeometry(drawingInput, display);
    if (!geometry.valid) {
        showWarning(QStringLiteral("模板区域无效"),
                    geometry.errorMessage);
        return;
    }
    TemplateSettings settings;
    settings.detectionMode = mode;
    settings.targetText =
            m_detectionSettingsUi.textEdit_targetText->toPlainText().trimmed();
    settings.imageThresholdPercent = threshold;
    settings.trackingRoi = geometry.trackingRoi;
    InitialTemplateAssets assets;
    assets.rawImage = rawImage;
    assets.trackingImageRect = geometry.trackingImageRect;
    assets.datePolygon = geometry.datePolygon;
    assets.barcodePolygon = geometry.barcodePolygon;
    if (mode == DetectionMode::Stamp) {
        const cv::Rect ring = geometry.stampAnchorImageRect;
        if (ring.width <= 5 || ring.height <= 5
                || ring.x < 0 || ring.y < 0
                || ring.x + ring.width > rawImage.cols
            || ring.y + ring.height > rawImage.rows) {
            showWarning(QStringLiteral("钢印标定无效"),
                        QStringLiteral(
                            "请重新选择有效的钢印区域定位参考区域（吸管口）。"));
            return;
        }
        assets.stampRing = rawImage(ring).clone();
        assets.stampPolygon = geometry.stampPolygon;
    }
    EditableTemplate editable;
    if (!m_templateService.stageInitialAssets(
            assets, &settings, &editable, &errorMessage)
            || !m_templateService.replaceDraft(
                editable, &errorMessage)) {
        showCritical(QStringLiteral("模板准备失败"), errorMessage);
        return;
    }
    PreparedTemplateSnapshot prepared;
    if (!m_templateService.save(
            target, false, &prepared, &errorMessage)) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.save_failed name=%1 path=%2 reason=%3")
                   .arg(name, QDir::toNativeSeparators(target), errorMessage);
        showCritical(QStringLiteral("模板保存失败"), errorMessage);
        return;
    }
    qCInfo(logTemplate).noquote()
            << QStringLiteral("event=template.saved name=%1 path=%2")
               .arg(name, QDir::toNativeSeparators(target));
    QStringList paths = currentModeTemplatePaths();
    if (isMultiTemplateMode(mode)) {
        if (!paths.contains(target, Qt::CaseInsensitive)) {
            paths.append(target);
        }
    } else {
        paths = QStringList() << target;
    }
    const OperationResult selected =
            m_settingsService.saveTemplatePathsAndDirectory(
                mode, paths, parentDirectory);
    if (!selected.isSuccess()) {
        showWarning(QStringLiteral("模板已保存但未应用"),
                    selected.error.userMessage
                    + QStringLiteral("\n\n保存目录也未记住。"));
        return;
    }
    cancelTemplateDrawing();
    resetTemplateCaptureState();
    refreshCurrentTemplateEditor();
    const int savedIndex =
            m_detectionSettingsUi.comboBox_currentEditTemplate->findData(
                QDir::cleanPath(QFileInfo(target).absoluteFilePath()));
    if (savedIndex >= 0) {
        const QSignalBlocker blocker(m_detectionSettingsUi.comboBox_currentEditTemplate);
        m_detectionSettingsUi.comboBox_currentEditTemplate->setCurrentIndex(savedIndex);
        loadTemplateAtIndex(savedIndex, false);
    }
    if (mode == DetectionMode::Stamp
            || mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord) {
        QMessageBox characterMessageBox(&m_dialogParent);
        characterMessageBox.setIcon(QMessageBox::Question);
        characterMessageBox.setWindowTitle(QStringLiteral("保存成功"));
        characterMessageBox.setText(
                    QStringLiteral("产品模板已保存成功。\n\n"
                                   "是否立即切割字符模板？"));
        QPushButton *confirmButton = characterMessageBox.addButton(
                    QStringLiteral("确定"), QMessageBox::AcceptRole);
        characterMessageBox.addButton(
                    QStringLiteral("取消"), QMessageBox::RejectRole);
        characterMessageBox.setDefaultButton(confirmButton);
        characterMessageBox.exec();
        if (characterMessageBox.clickedButton() == confirmButton) {
            showManualCharacterTemplateEditorDialog();
        }
        return;
    }
    showInfo(QStringLiteral("保存成功"),
             targetExists
             ? QStringLiteral("模板已完全覆盖并应用。")
             : QStringLiteral("模板已新建并应用。"));
}

bool TemplateEditorPage::saveCurrentDraft(bool showSuccessMessage)
{
    if (!m_templateService.isActive()
            || m_templateService.currentDirectoryPath().isEmpty()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("当前没有可编辑模板。"));
        return false;
    }
    PreparedTemplateSnapshot prepared;
    QString errorMessage;
    if (!m_templateService.save(
            m_templateService.currentDirectoryPath(), true,
            &prepared, &errorMessage)) {
        const QString path = m_templateService.currentDirectoryPath();
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=template.save_failed name=%1 path=%2 reason=%3")
                   .arg(QFileInfo(path).fileName(),
                        QDir::toNativeSeparators(path), errorMessage);
        showCritical(QStringLiteral("模板保存失败"), errorMessage);
        return false;
    }
    m_templateService.setActivePreparedTemplate(prepared);
    refreshTemplateDirty();
    const QString path = m_templateService.currentDirectoryPath();
    qCInfo(logTemplate).noquote()
            << QStringLiteral("event=template.saved name=%1 path=%2")
               .arg(QFileInfo(path).fileName(),
                    QDir::toNativeSeparators(path));
    if (showSuccessMessage) {
        showInfo(QStringLiteral("保存成功"),
                 QStringLiteral("当前模板已更新。"));
    }
    return true;
}

void TemplateEditorPage::applyCurrentTargetText()
{
    if (!m_templateService.isActive()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择当前编辑模板。"));
        return;
    }
    const EditableTemplate original = m_templateService.draft();
    EditableTemplate value = original;
    value.settings.targetText = m_detectionSettingsUi.textEdit_targetText
            ->toPlainText().trimmed();
    QString validationMessage;
    if (detectionModeDescriptor(
            value.settings.detectionMode).requiresCharacterTemplates) {
        const QStringList targetUnits =
                TemplateStore::templateTargetUnits(
                    value.settings.targetText);
        if (!value.settings.targetText.trimmed().isEmpty()
                && targetUnits.isEmpty()) {
            validationMessage = QStringLiteral(
                        "目标文字不包含可检测字符。");
        } else {
            const QString missingTarget = missingTemplateTargetUnit(
                        value.settings.detectionMode,
                        targetUnits,
                        value.characterAssets);
            if (!missingTarget.isEmpty()) {
                validationMessage = QStringLiteral(
                            "模板缺少目标文字所需字符：“%1”。")
                        .arg(missingTarget);
            }
        }
    }
    if (!validationMessage.isEmpty()) {
        {
            QSignalBlocker blocker(m_detectionSettingsUi.textEdit_targetText);
            m_detectionSettingsUi.textEdit_targetText->setPlainText(
                        original.settings.targetText);
        }
        refreshTemplateDirty();
        showWarning(QStringLiteral("目标文字保存失败"),
                    validationMessage + QStringLiteral(
                        "\n目标文字未保存，已恢复为原内容。"));
        return;
    }
    QString errorMessage;
    if (!m_templateService.replaceDraft(value, &errorMessage)) {
        QSignalBlocker blocker(m_detectionSettingsUi.textEdit_targetText);
        m_detectionSettingsUi.textEdit_targetText->setPlainText(
                    original.settings.targetText);
        refreshTemplateDirty();
        showCritical(QStringLiteral("目标文字保存失败"), errorMessage);
        return;
    }
    if (!saveCurrentDraft(false)) {
        m_templateService.replaceDraft(original);
        {
            QSignalBlocker blocker(m_detectionSettingsUi.textEdit_targetText);
            m_detectionSettingsUi.textEdit_targetText->setPlainText(
                        original.settings.targetText);
        }
        refreshTemplateDirty();
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("当前模板的目标文字已保存。"));
}

void TemplateEditorPage::applyCurrentImageThreshold()
{
    if (!m_templateService.isActive()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择当前编辑模板。"));
        return;
    }
    const EditableTemplate original = m_templateService.draft();
    int threshold = 0;
    if (!parseIntValue(m_detectionSettingsUi.lineEdit_imageThreshold->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        {
            QSignalBlocker blocker(m_detectionSettingsUi.lineEdit_imageThreshold);
            m_detectionSettingsUi.lineEdit_imageThreshold->setText(QString::number(
                original.settings.imageThresholdPercent));
        }
        refreshTemplateDirty();
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("图像阈值必须是 0 到 100 的整数。"));
        return;
    }
    EditableTemplate value = original;
    value.settings.imageThresholdPercent = threshold;
    QString errorMessage;
    if (!m_templateService.replaceDraft(value, &errorMessage)) {
        QSignalBlocker blocker(m_detectionSettingsUi.lineEdit_imageThreshold);
        m_detectionSettingsUi.lineEdit_imageThreshold->setText(QString::number(
            original.settings.imageThresholdPercent));
        refreshTemplateDirty();
        showCritical(QStringLiteral("阈值保存失败"), errorMessage);
        return;
    }
    if (!saveCurrentDraft(false)) {
        m_templateService.replaceDraft(original);
        {
            QSignalBlocker blocker(m_detectionSettingsUi.lineEdit_imageThreshold);
            m_detectionSettingsUi.lineEdit_imageThreshold->setText(QString::number(
                original.settings.imageThresholdPercent));
        }
        refreshTemplateDirty();
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("当前模板的图像阈值已保存。"));
}

bool TemplateEditorPage::updateAllSelectedTemplates(
        const std::function<void(TemplateSettings *)> &update,
        QString *errorMessage)
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        if (errorMessage) *errorMessage = QStringLiteral("检测模式无效。");
        return false;
    }
    const bool updated = m_templateService.updateTemplates(
                currentModeTemplatePaths(), mode, update, errorMessage);
    restoreTemplatesForMode(currentDetectModeId(), false);
    return updated;
}

void TemplateEditorPage::applyBatchTargetText()
{
    const QString text = m_detectionSettingsUi.textEdit_targetText
            ->toPlainText().trimmed();
    QString error;
    if (!updateAllSelectedTemplates(
            [text](TemplateSettings *settings) {
        settings->targetText = text;
    }, &error)) {
        showCritical(QStringLiteral("批量保存失败"), error);
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("目标文字已保存到全部已选择模板。"));
}

void TemplateEditorPage::applyBatchImageThreshold()
{
    int threshold = 0;
    if (!parseIntValue(m_detectionSettingsUi.lineEdit_imageThreshold->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("图像阈值必须是 0 到 100 的整数。"));
        return;
    }
    QString error;
    if (!updateAllSelectedTemplates(
            [threshold](TemplateSettings *settings) {
        settings->imageThresholdPercent = threshold;
    }, &error)) {
        showCritical(QStringLiteral("批量保存失败"), error);
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("图像阈值已保存到全部已选择模板。"));
}

void TemplateEditorPage::showManualCharacterTemplateEditorDialog()
{
    if (!m_templateService.isActive()
            || m_templateService.draft().rawImage.empty()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择并加载一个模板。"));
        return;
    }
    EditableTemplate value = m_templateService.draft();
    if (value.settings.trackingRoi.width() <= 0.0
            || value.settings.trackingRoi.height() <= 0.0
            || value.settings.datePolygon.size() < 3) {
        showWarning(
                    QStringLiteral("日期/文字检测区域无效"),
                    QStringLiteral("当前模板的日期/文字检测区域无效，"
                                   "请重新制作或编辑模板。"));
        return;
    }
    const QPointF trackingCenter = value.settings.trackingRoi.center();
    QPolygonF absoluteDatePolygon;
    for (const QPointF &point : value.settings.datePolygon) {
        absoluteDatePolygon.append(point + trackingCenter);
    }
    const QRect imageBounds(
                0, 0, value.rawImage.cols, value.rawImage.rows);
    const QRect characterRegionRect = absoluteDatePolygon.boundingRect()
            .toAlignedRect().intersected(imageBounds);
    if (characterRegionRect.width() <= 0
            || characterRegionRect.height() <= 0) {
        showWarning(
                    QStringLiteral("日期/文字检测区域无效"),
                    QStringLiteral("当前模板的日期/文字检测区域无效，"
                                   "请重新制作或编辑模板。"));
        return;
    }
    const cv::Rect characterRegion(
                characterRegionRect.x(), characterRegionRect.y(),
                characterRegionRect.width(), characterRegionRect.height());
    CharacterTemplateEditorDialog dialog(
                imageFromBgrMat(value.rawImage(characterRegion)),
                value.settings, &m_dialogParent);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    value.settings = dialog.resultSettings();
    QString error;
    if (!m_templateService.stageCharacterAssets(
            dialog.characterImages(), &value, &error)
            || !m_templateService.replaceDraft(value, &error)
            || !saveCurrentDraft(false)) {
        if (!error.isEmpty()) {
            showCritical(QStringLiteral("字符模板保存失败"), error);
        }
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("字符模板已保存到当前模板。"));
}

void TemplateEditorPage::setupTemplateDirtyTracking()
{
    connect(m_detectionSettingsUi.textEdit_targetText,
            &QTextEdit::textChanged,
            this, &TemplateEditorPage::refreshTemplateDirty);
    connect(m_detectionSettingsUi.lineEdit_imageThreshold,
            &QLineEdit::textChanged,
            this, &TemplateEditorPage::refreshTemplateDirty);
}

void TemplateEditorPage::refreshTemplateDirty()
{
    if (!m_templateService.isActive()) {
        clearTemplateDirty();
        return;
    }
    const TemplateSettings &settings = m_templateService.draft().settings;
    const bool targetDirty =
            m_detectionSettingsUi.textEdit_targetText
            ->toPlainText().trimmed() != settings.targetText;
    m_settingsEditState.setTemplateTargetDirty(targetDirty);
    bool ok = false;
    const int threshold =
            m_detectionSettingsUi.lineEdit_imageThreshold->text().toInt(&ok);
    const bool thresholdDirty =
            settings.detectionMode != DetectionMode::Ocr
            && (!ok || threshold != settings.imageThresholdPercent);
    m_settingsEditState.setTemplateThresholdDirty(thresholdDirty);
    m_detectionSettingsUi.label_targetText->setText(
                targetDirty
                ? m_targetTextLabelText + QStringLiteral(" *")
                : m_targetTextLabelText);
    m_detectionSettingsUi.label_imageThreshold->setText(
                thresholdDirty
                ? m_imageThresholdLabelText + QStringLiteral(" *")
                : m_imageThresholdLabelText);
}

void TemplateEditorPage::clearTemplateDirty()
{
    m_settingsEditState.clearTemplateDirty();
    m_detectionSettingsUi.label_targetText->setText(m_targetTextLabelText);
    m_detectionSettingsUi.label_imageThreshold->setText(m_imageThresholdLabelText);
}

void TemplateEditorPage::applyTemplateSettingsToUi(
        const TemplateSettings &settings)
{
    QSignalBlocker targetBlocker(m_detectionSettingsUi.textEdit_targetText);
    QSignalBlocker thresholdBlocker(m_detectionSettingsUi.lineEdit_imageThreshold);
    m_detectionSettingsUi.textEdit_targetText->setPlainText(settings.targetText);
    m_detectionSettingsUi.lineEdit_imageThreshold->setText(
                QString::number(settings.imageThresholdPercent));
}

void TemplateEditorPage::adjustTemplateGuideHeight()
{
    m_mainWindowUi.frame_templateGuide->updateGeometry();
    m_mainWindowUi.frame_templateGuide->adjustSize();
}

void TemplateEditorPage::updateTemplateGuideText(
        const QString &title, const QString &body)
{
    m_mainWindowUi.label_templateGuideTitle->setText(
                QStringLiteral("【%1】").arg(title.trimmed()));
    QString guideText = body.trimmed();
    if (!guideText.contains(QStringLiteral("Esc"))) {
        guideText.append(QStringLiteral("  按 Esc 清空框线并重新开始。"));
    }
    m_mainWindowUi.label_templateGuideBody->setText(guideText);
    m_mainWindowUi.frame_templateGuide->show();
    adjustTemplateGuideHeight();
}

void TemplateEditorPage::hideTemplateGuide()
{
    m_mainWindowUi.frame_templateGuide->hide();
}

void TemplateEditorPage::showInspectionStatus()
{
    m_mainWindowUi.label_templateGuideTitle->setText(QStringLiteral("【当前状态】"));
    m_mainWindowUi.label_templateGuideBody->setText(QStringLiteral("正在检测中..."));
    m_mainWindowUi.frame_templateGuide->show();
    adjustTemplateGuideHeight();
}

void TemplateEditorPage::showTemplateImageSource(
        const QString &templateName)
{
    const QString normalizedName = templateName.trimmed();
    if (normalizedName.isEmpty()
            ) {
        hideTemplateGuide();
        return;
    }
    m_mainWindowUi.label_templateGuideTitle->setText(QStringLiteral("【当前图像】"));
    m_mainWindowUi.label_templateGuideBody->setText(
                QStringLiteral("正在显示模板【%1】的产品图像。")
                .arg(normalizedName.toHtmlEscaped()));
    m_mainWindowUi.frame_templateGuide->show();
    adjustTemplateGuideHeight();
}

void TemplateEditorPage::showTemplateCaptureStatus(const QString &body)
{
    if (m_captureState != CaptureState::Previewing
            && m_captureState != CaptureState::Frozen) {
        hideTemplateGuide();
        return;
    }
    DetectionMode mode = DetectionMode::Stamp;
    const QString status = body.trimmed();
    if (status.isEmpty()
            || !detectionModeFromUiId(currentDetectModeId(), &mode)
            || mode == DetectionMode::Tissue) {
        hideTemplateGuide();
        return;
    }
    m_mainWindowUi.label_templateGuideTitle->setText(
                QStringLiteral("【%1】").arg(templateGuideTitle(mode)));
    m_mainWindowUi.label_templateGuideBody->setText(status);
    m_mainWindowUi.frame_templateGuide->show();
    adjustTemplateGuideHeight();
}

void TemplateEditorPage::cancelTemplateDrawing()
{
    m_mainWindowUi.inspectionImageCanvas->cancelTemplateDrawing();
    clearBarcodeTemplateValidation();
    hideTemplateGuide();
}

void TemplateEditorPage::handleTemplateDrawingChanged(
        InspectionImageCanvas::DrawingStep step,
        InspectionImageCanvas::DrawingEvent event,
        int pointCount)
{
    const DetectionMode mode = m_mainWindowUi.inspectionImageCanvas->templateDrawingMode();
    if (mode == DetectionMode::Tissue) {
        hideTemplateGuide();
        return;
    }

    if (event == InspectionImageCanvas::DrawingEvent::StepCompleted
            && step == InspectionImageCanvas::DrawingStep::BarcodeRegion
            && !validateCompletedBarcode()) {
        return;
    }
    if (event == InspectionImageCanvas::DrawingEvent::WorkflowCompleted) {
        updateDrawingGuide(step, event, pointCount);
        askToSaveCompletedTemplate(mode);
        return;
    }
    updateDrawingGuide(step, event, pointCount);
}

bool TemplateEditorPage::validateCompletedBarcode()
{
    const QRect barcode = m_mainWindowUi.inspectionImageCanvas->barcodeRect().normalized();
    QString failureReason;
    if (validateBarcodeTemplateRect(
                barcode,
                barcodeTemplateValidationOptions(),
                &failureReason)) {
        acceptBarcodeTemplateValidation(barcode);
        return true;
    }

    clearBarcodeTemplateValidation();
    m_mainWindowUi.inspectionImageCanvas->retryBarcodeRegion();
    const QString barcodeRegion = emphasizedDrawingRegion(
                DetectionMode::BarcodeWord,
                InspectionImageCanvas::DrawingStep::BarcodeRegion);
    const QString trackingAnchor = emphasizedDrawingRegion(
                DetectionMode::BarcodeWord,
                InspectionImageCanvas::DrawingStep::TrackingAnchor);
    updateTemplateGuideText(
                templateGuideTitle(DetectionMode::BarcodeWord),
                QStringLiteral(
                    "【步骤2/3】%1扫描失败，%2已保留，请重新完整框选%1。")
                .arg(barcodeRegion, trackingAnchor));
    QTimer::singleShot(0, this, [this, failureReason]() {
        showWarning(
                    QStringLiteral("二维码扫描失败"),
                    failureReason.trimmed().isEmpty()
                    ? QStringLiteral(
                          "二维码区域无法解码，请重新完整框选，四周保留少量背景，不要包含日期区域。")
                    : failureReason
                      + QStringLiteral(
                          "\n\n请重新完整框选二维码区域，四周保留少量背景，不要包含日期区域。"));
    });
    return false;
}

void TemplateEditorPage::updateDrawingGuide(
        InspectionImageCanvas::DrawingStep step,
        InspectionImageCanvas::DrawingEvent event,
        int pointCount)
{
    const DetectionMode mode = m_mainWindowUi.inspectionImageCanvas->templateDrawingMode();
    const QString title = templateGuideTitle(mode);
    const QString region = emphasizedDrawingRegion(mode, step);
    const QString stepPrefix = QStringLiteral("【步骤%1/%2】")
            .arg(drawingStepNumber(mode, step))
            .arg(drawingStepCount(mode));

    switch (event) {
    case InspectionImageCanvas::DrawingEvent::StepStarted: {
        if (step == InspectionImageCanvas::DrawingStep::TrackingAnchor
                || step == InspectionImageCanvas::DrawingStep::BarcodeRegion) {
            clearBarcodeTemplateValidation();
        }
        QString instruction;
        if (isDrawingPolygonStep(step)) {
            instruction = QStringLiteral(
                        "%1用鼠标左键依次点击%2边缘，右键闭合。")
                    .arg(stepPrefix, region);
            if (step == InspectionImageCanvas::DrawingStep::DatePolygon
                    && mode == DetectionMode::BarcodeWord
                    && m_barcodeTemplateReadable) {
                instruction.prepend(QStringLiteral("二维码扫描成功。"));
            }
        } else if (step == InspectionImageCanvas::DrawingStep::BarcodeRegion) {
            instruction = QStringLiteral(
                        "%1按住鼠标左键拖动，完整框选%2；松开后立即验证是否可读，四周保留少量背景。")
                    .arg(stepPrefix, region);
        } else {
            instruction = QStringLiteral(
                        "%1按住鼠标左键拖动，框选%2；松开左键完成。")
                    .arg(stepPrefix, region);
        }
        updateTemplateGuideText(title, instruction);
        return;
    }
    case InspectionImageCanvas::DrawingEvent::Reset:
        clearBarcodeTemplateValidation();
        updateTemplateGuideText(
                    title,
                    QStringLiteral("%1已清空当前模式全部框线，请重新框选%2。")
                    .arg(stepPrefix, region));
        return;
    case InspectionImageCanvas::DrawingEvent::RegionTooSmall:
        if (step == InspectionImageCanvas::DrawingStep::BarcodeRegion) {
            clearBarcodeTemplateValidation();
        }
        updateTemplateGuideText(
                    title,
                    QStringLiteral("%1%2太小，请重新框选。")
                    .arg(stepPrefix, region));
        return;
    case InspectionImageCanvas::DrawingEvent::PointAdded:
        updateTemplateGuideText(
                    title,
                    QStringLiteral(
                        "%1已选择%2个点，继续点击%3边缘或右键闭合。")
                    .arg(stepPrefix).arg(pointCount).arg(region));
        return;
    case InspectionImageCanvas::DrawingEvent::TooFewPoints:
        updateTemplateGuideText(
                    title,
                    QStringLiteral(
                        "%1至少需要3个点，当前%2个，请继续点击%3边缘。")
                    .arg(stepPrefix).arg(pointCount).arg(region));
        return;
    case InspectionImageCanvas::DrawingEvent::StepCompleted:
        return;
    case InspectionImageCanvas::DrawingEvent::WorkflowCompleted:
        break;
    }

    QString completedText;
    if (mode == DetectionMode::Stamp) {
        completedText = QStringLiteral(
                    "【步骤4/4】%1、%2、%3和%4均已完成，请点击【保存模板】。")
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::StampAnchor))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::StampPolygon))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::DateAnchor))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::DatePolygon));
    } else if (mode == DetectionMode::BarcodeWord) {
        completedText = QStringLiteral(
                    "【步骤3/3】%1、%2和%3均已完成，请点击【保存模板】。")
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::TrackingAnchor))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::BarcodeRegion))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::DatePolygon));
    } else {
        completedText = QStringLiteral(
                    "【步骤2/2】%1和%2均已完成，请点击【保存模板】。")
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::TrackingAnchor))
                .arg(emphasizedDrawingRegion(
                         mode, InspectionImageCanvas::DrawingStep::DetectionPolygon));
    }
    updateTemplateGuideText(title, completedText);
}

void TemplateEditorPage::askToSaveCompletedTemplate(DetectionMode mode)
{
    QTimer::singleShot(0, this, [this, mode]() {
        if (m_mainWindowUi.inspectionImageCanvas->templateDrawingMode() != mode
                || !m_mainWindowUi.inspectionImageCanvas->isTemplateDrawingComplete()) {
            return;
        }
        QMessageBox saveMessageBox(&m_dialogParent);
        saveMessageBox.setIcon(QMessageBox::Question);
        saveMessageBox.setWindowTitle(QStringLiteral("保存模板"));
        saveMessageBox.setText(
                    mode == DetectionMode::Stamp
                    ? QStringLiteral(
                          "钢印区域定位参考区域（吸管口）、钢印检测区域、生产日期检测区域定位参考区域和生产日期检测区域均已完成。\n\n是否立即保存当前模板？")
                    : mode == DetectionMode::BarcodeWord
                      ? QStringLiteral(
                            "二维码与生产日期区域定位参考区域、二维码区域和生产日期检测区域均已完成。\n\n是否立即保存当前模板？")
                      : QStringLiteral(
                            "%1和%2均已完成。\n\n是否立即保存当前模板？")
                        .arg(drawingRegionName(
                                 mode,
                                 InspectionImageCanvas::DrawingStep::TrackingAnchor),
                             drawingRegionName(
                                 mode,
                                 InspectionImageCanvas::DrawingStep::DetectionPolygon)));
        QPushButton *saveButton = saveMessageBox.addButton(
                    QStringLiteral("保存"), QMessageBox::AcceptRole);
        saveMessageBox.addButton(
                    QStringLiteral("取消"), QMessageBox::RejectRole);
        saveMessageBox.setDefaultButton(saveButton);
        saveMessageBox.exec();
        if (saveMessageBox.clickedButton() == saveButton) {
            saveCurrentTemplate();
        }
    });
}

void TemplateEditorPage::setupManualCharacterCropUi()
{
    connect(m_mainWindowUi.toolButton_editCharacterTemplates,
            &QToolButton::clicked,
            this,
            &TemplateEditorPage::showManualCharacterTemplateEditorDialog);
}

void TemplateEditorPage::connectPageActions()
{
    connect(m_mainWindowUi.toolButton_selectTemplate,
            &QToolButton::clicked,
            this, &TemplateEditorPage::selectTemplatesForCurrentMode);
    connect(m_mainWindowUi.toolButton_saveTemplate,
            &QToolButton::clicked,
            this, &TemplateEditorPage::saveCurrentTemplate);
    connect(m_detectionSettingsUi.pushButton_applyTargetText,
            &QPushButton::clicked,
            this, &TemplateEditorPage::applyCurrentTargetText);
    connect(m_detectionSettingsUi.pushButton_applyBatchTargetText,
            &QPushButton::clicked,
            this, &TemplateEditorPage::applyBatchTargetText);
    connect(m_detectionSettingsUi.pushButton_applyImageThreshold,
            &QPushButton::clicked,
            this, &TemplateEditorPage::applyCurrentImageThreshold);
    connect(m_detectionSettingsUi.pushButton_applyBatchImageThreshold,
            &QPushButton::clicked,
            this, &TemplateEditorPage::applyBatchImageThreshold);
}

void TemplateEditorPage::clearBarcodeTemplateValidation()
{
    m_barcodeTemplateReadable = false;
    m_validatedBarcodeRect = QRect();
}

TemplateBarcodeValidationOptions
TemplateEditorPage::barcodeTemplateValidationOptions() const
{
    if (m_templateService.isActive()) {
        const TemplateBarcodeParameters &parameters =
                m_templateService.draft().settings.barcodeParameters;
        TemplateBarcodeValidationOptions options;
        options.formatMask = parameters.formatMask;
        options.roiPaddingPercent = parameters.roiPaddingPercent;
        options.maxDecodeTimeMs = parameters.maxDecodeTimeMs;
        options.enableFallback = parameters.enableFallback;
        return options;
    }
    return TemplateBarcodeValidationOptions();
}

bool TemplateEditorPage::validateBarcodeTemplateRect(
        const QRect &uiBarcodeRect,
        const TemplateBarcodeValidationOptions &options,
        QString *failureReason)
{
    const cv::Mat source =
            m_inspectionService.currentCameraImageClone();
    const QPixmap *pixmap = m_mainWindowUi.inspectionImageCanvas->pixmap();
    const QSize sourceSize(source.cols, source.rows);
    const TemplateDisplayGeometry geometry = {
        m_mainWindowUi.inspectionImageCanvas->size(),
        pixmap && !pixmap->isNull()
        ? pixmap->size()
        : sourceSize.scaled(m_mainWindowUi.inspectionImageCanvas->size(), Qt::KeepAspectRatio),
        sourceSize
    };
    const QRect imageRect = m_templateService.mapDisplayRectToImage(
                uiBarcodeRect, geometry);
    return m_templateService.validateBarcodeTemplate(
                source, imageRect, options, failureReason);
}

void TemplateEditorPage::acceptBarcodeTemplateValidation(
        const QRect &barcodeRect)
{
    m_barcodeTemplateReadable = true;
    m_validatedBarcodeRect = barcodeRect;
}
