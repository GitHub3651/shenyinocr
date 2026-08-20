// 文件作用：实现统一模板选择、当前编辑模板、取景和参数保存。
#include "ui/pages/template_editor_page.h"

#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "contracts/detection_mode.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui/dialogs/character_template_editor_dialog.h"
#include "ui/dialogs/template_selection_dialog.h"
#include "ui/pages/template_editor_support.h"
#include "ui/widgets/image_label.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

using namespace TemplateEditorSupport;

namespace {

void applyWidgetAccess(QWidget *widget,
                       const OperationUiSnapshot::Access &access)
{
    if (!widget) {
        return;
    }
    widget->setEnabled(access.enabled);
    if (!access.enabled) {
        widget->setToolTip(access.disabledReason);
    }
}

bool isMultiTemplateMode(DetectionMode mode)
{
    return detectionModeDescriptor(mode).trackingKind
            == DetectionTrackingKind::MultipleTemplates;
}

}

TemplateEditorPage::TemplateEditorPage(
        const TemplateEditorViewBindings &view,
        TemplateApplicationService *templateService,
        InspectionApplicationService *inspectionService,
        SettingsApplicationService *settingsService,
        SettingsEditState *settingsEditState,
        const TemplateEditorPageCallbacks &callbacks,
        QObject *parent)
    : QObject(parent),
      m_view(view),
      m_templateService(templateService),
      m_inspectionService(inspectionService),
      m_settingsService(settingsService),
      m_settingsEditState(settingsEditState),
      m_callbacks(callbacks),
      imageLabel(view.imageLabel_templateCanvas)
{
    if (!m_view.parentWidget || !imageLabel || !m_templateService
            || !m_inspectionService || !m_settingsService
            || !m_settingsEditState) {
        throw std::invalid_argument(
                    "TemplateEditorPage requires complete bindings");
    }
    connect(m_inspectionService,
            &InspectionApplicationService::templatePreviewFrameReady,
            this, [this](quint64 sessionId, cv::Mat image) {
        handlePreviewFrame(sessionId, image);
    }, Qt::QueuedConnection);
    connect(m_inspectionService,
            &InspectionApplicationService::templatePreviewFailed,
            this, [this](quint64 sessionId, const QString &reason) {
        handlePreviewFailure(sessionId, reason);
    }, Qt::QueuedConnection);
    connect(m_inspectionService,
            &InspectionApplicationService::captureStopped,
            this, [this](bool preview) {
        if (preview && m_captureState == CaptureState::Previewing) {
            ++m_previewSessionId;
            m_captureState = CaptureState::Idle;
            m_lastPreviewFrame.release();
            if (m_callbacks.updateOperationUiState) {
                m_callbacks.updateOperationUiState();
            }
        }
    }, Qt::QueuedConnection);
}

void TemplateEditorPage::applyOperationState(
        const OperationUiSnapshot &snapshot)
{
    OperationUiSnapshot::Access editAccess = snapshot.templateEditing;
    if (m_selectedTemplateInvalid) {
        editAccess.enabled = false;
        editAccess.disabledReason = QStringLiteral(
                    "当前模板状态异常，请移除或重新选择模板。");
    }
    applyWidgetAccess(m_currentTemplateEditComboBox,
                      snapshot.templateEditing);
    applyWidgetAccess(m_removeCurrentTemplateButton,
                      snapshot.templateEditing);
    applyWidgetAccess(m_manualCharacterCropButton,
                      editAccess);
    applyWidgetAccess(m_view.textEdit_targetText,
                      editAccess);
    applyWidgetAccess(m_view.lineEdit_imageThreshold,
                      editAccess);
    applyWidgetAccess(m_view.lineEdit_tissueRoughnessThreshold,
                      snapshot.templateEditing);
    applyWidgetAccess(m_view.pushButton_applyTargetText,
                      editAccess);
    applyWidgetAccess(m_view.pushButton_applyBatchTargetText,
                      editAccess);
    applyWidgetAccess(m_view.pushButton_applyImageThreshold,
                      editAccess);
    applyWidgetAccess(m_view.pushButton_applyBatchImageThreshold,
                      editAccess);
    applyWidgetAccess(m_view.pushButton_applyTissueRoughnessThreshold,
                      snapshot.templateEditing);
}

QFrame *TemplateEditorPage::guideFrame() const
{
    return m_templateGuideFrame;
}

QPushButton *TemplateEditorPage::manualCharacterCropButton() const
{
    return m_manualCharacterCropButton;
}

QWidget *TemplateEditorPage::dialogParent() const
{
    return m_view.parentWidget;
}

bool TemplateEditorPage::isCameraOpen() const
{
    return m_inspectionService->isCameraOpen();
}

void TemplateEditorPage::showInfo(
        const QString &title, const QString &message)
{
    QMessageBox::information(dialogParent(), title, message);
}

void TemplateEditorPage::showWarning(
        const QString &title, const QString &message)
{
    QMessageBox::warning(dialogParent(), title, message);
}

void TemplateEditorPage::showCritical(
        const QString &title, const QString &message)
{
    QMessageBox::critical(dialogParent(), title, message);
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
    return m_inspectionService->stopTemplatePreview().isSuccess();
}

void TemplateEditorPage::resetTemplateCaptureState()
{
    if (!stopTemplatePreview()) {
        return;
    }
    ++m_previewSessionId;
    m_captureState = CaptureState::Idle;
    m_lastPreviewFrame.release();
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
}

bool TemplateEditorPage::startTemplatePreview()
{
    ++m_previewSessionId;
    const OperationResult result = m_inspectionService->startTemplatePreview(
                m_previewSessionId,
                m_view.comboBox_imageRotation
                ? m_view.comboBox_imageRotation->currentIndex() : 0,
                m_view.comboBox_colorChannel
                ? m_view.comboBox_colorChannel->currentIndex() : 0);
    if (!result.isSuccess()) {
        showWarning(QStringLiteral("实时取景失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("实时取景线程启动失败。")
                    : result.error.userMessage);
        return false;
    }
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    m_captureState = CaptureState::Previewing;
    m_lastPreviewFrame.release();
    updateImageDisplayStatusText(
                QStringLiteral("实时取景中，调整产品位置后点击拍照并开始框选。"));
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
    return true;
}

bool TemplateEditorPage::freezeTemplatePreview()
{
    if (m_captureState != CaptureState::Previewing
            || m_lastPreviewFrame.empty()) {
        showInfo(QStringLiteral("提示"),
                 QStringLiteral("相机尚未返回有效画面，请稍候再点击。"));
        return false;
    }
    if (!stopTemplatePreview()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("实时取景尚未停止，请稍后重试。"));
        return false;
    }
    m_captureState = CaptureState::Frozen;
    m_inspectionService->replaceCurrentCameraImage(m_lastPreviewFrame);
    if (m_callbacks.displayPreviewFrame) {
        m_callbacks.displayPreviewFrame(m_lastPreviewFrame);
    }
    DetectionMode mode = DetectionMode::Stamp;
    detectionModeFromUiId(currentDetectModeId(), &mode);
    imageLabel->setBarcodeRegionRequired(mode == DetectionMode::BarcodeWord);
    imageLabel->setTemplateDrawingEnabled(mode != DetectionMode::Tissue);
    if (mode != DetectionMode::Tissue) {
        imageLabel->resetDrawingStep();
        showTemplateGuideForCurrentMode();
    }
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
    return true;
}

void TemplateEditorPage::handleTemplateCaptureButton()
{
    if (m_captureState == CaptureState::Previewing) {
        freezeTemplatePreview();
        return;
    }
    if (m_captureState == CaptureState::Frozen
            && (!imageLabel->getTrackingRect().isNull()
                || !imageLabel->getBarcodeRect().isNull()
                || !imageLabel->getDetectionPoly().isEmpty())) {
        if (QMessageBox::question(
                dialogParent(), QStringLiteral("重新取景"),
                QStringLiteral("重新取景会清空当前框线，是否继续？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
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
}

void TemplateEditorPage::handlePreviewFailure(
        quint64 sessionId, const QString &reason)
{
    if (m_captureState != CaptureState::Previewing
            || sessionId != m_previewSessionId) {
        return;
    }
    resetTemplateCaptureState();
    showWarning(QStringLiteral("实时取景失败"), reason);
}

void TemplateEditorPage::setupCurrentTemplateEditor()
{
    if (m_currentTemplateEditWidget || !m_view.textEdit_targetText) {
        return;
    }
    QWidget *parent = m_view.textEdit_targetText->parentWidget();
    QGridLayout *grid = parent
            ? qobject_cast<QGridLayout *>(parent->layout()) : nullptr;
    if (!parent || !grid) {
        return;
    }
    m_currentTemplateEditWidget = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(m_currentTemplateEditWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    m_currentTemplateEditLabel = new QLabel(
                QStringLiteral("当前编辑模板："),
                m_currentTemplateEditWidget);
    m_currentTemplateEditComboBox = new QComboBox(
                m_currentTemplateEditWidget);
    m_currentTemplateEditComboBox->setObjectName(
                QStringLiteral("comboBox_currentEditTemplate"));
    m_removeCurrentTemplateButton = new QPushButton(
                QStringLiteral("移除模板"),
                m_currentTemplateEditWidget);
    m_removeCurrentTemplateButton->setObjectName(
                QStringLiteral("toolButton_removeCurrentTemplate"));
    m_removeCurrentTemplateButton->setToolTip(
                QStringLiteral("从当前检测方案移除模板，不会删除模板文件夹。"));
    layout->addWidget(m_currentTemplateEditLabel);
    layout->addWidget(m_currentTemplateEditComboBox, 1);
    layout->addWidget(m_removeCurrentTemplateButton);
    grid->addWidget(m_currentTemplateEditWidget, 0, 0, 1, 3);
    connect(m_currentTemplateEditComboBox,
            static_cast<void (QComboBox::*)(int)>(
                &QComboBox::currentIndexChanged),
            this, [this](int index) {
        loadTemplateAtIndex(index, true);
    });
    connect(m_removeCurrentTemplateButton, &QPushButton::clicked,
            this, [this]() { removeCurrentTemplate(); });
    refreshCurrentTemplateEditor();
}

QString TemplateEditorPage::detectModeIdForIndex(int index) const
{
    if (!m_view.comboBox_detectionMode
            || index < 0
            || index >= m_view.comboBox_detectionMode->count()) {
        return QString();
    }
    const QVariant data = m_view.comboBox_detectionMode->itemData(index);
    return data.isValid() && !data.toString().isEmpty()
            ? data.toString()
            : detectionModeUiId(detectionModeDescriptors()
                                .value(index).mode);
}

QString TemplateEditorPage::currentDetectModeId() const
{
    return m_view.comboBox_detectionMode
            ? detectModeIdForIndex(
                m_view.comboBox_detectionMode->currentIndex())
            : QString();
}

QStringList TemplateEditorPage::currentModeTemplatePaths() const
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        return QStringList();
    }
    return m_settingsService->current()
            .detectionSchemes.templatePaths(mode);
}

void TemplateEditorPage::refreshCurrentTemplateEditor()
{
    if (!m_currentTemplateEditComboBox) {
        return;
    }
    DetectionMode mode = DetectionMode::Tissue;
    detectionModeFromUiId(currentDetectModeId(), &mode);
    const bool usesTemplate = mode != DetectionMode::Tissue;
    m_currentTemplateEditWidget->setVisible(usesTemplate);
    if (m_view.pushButton_applyBatchTargetText) {
        m_view.pushButton_applyBatchTargetText->setVisible(
                    isMultiTemplateMode(mode));
    }
    if (m_view.pushButton_applyBatchImageThreshold) {
        m_view.pushButton_applyBatchImageThreshold->setVisible(
                    isMultiTemplateMode(mode));
    }
    if (!usesTemplate) {
        QSignalBlocker blocker(m_currentTemplateEditComboBox);
        m_currentTemplateEditComboBox->clear();
        return;
    }
    const QString previous = m_currentTemplateEditComboBox->currentData()
            .toString();
    const QStringList paths = currentModeTemplatePaths();
    QSignalBlocker blocker(m_currentTemplateEditComboBox);
    m_currentTemplateEditComboBox->clear();
    for (const QString &path : paths) {
        TemplateStoreError error;
        const TemplateSummary summary = m_templateService->readSummary(
                    path, mode, &error);
        const QString name = QFileInfo(path).fileName();
        m_currentTemplateEditComboBox->addItem(
                    summary.valid ? name
                                  : name + QStringLiteral("（状态异常）"),
                    path);
        m_currentTemplateEditComboBox->setItemData(
                    m_currentTemplateEditComboBox->count() - 1,
                    path, Qt::ToolTipRole);
    }
    int index = m_currentTemplateEditComboBox->findData(previous);
    if (index < 0 && !paths.isEmpty()) {
        index = 0;
    }
    m_currentTemplateEditComboBox->setCurrentIndex(index);
    m_removeCurrentTemplateButton->setEnabled(index >= 0);
}

bool TemplateEditorPage::loadTemplateAtIndex(
        int index, bool showMessage)
{
    if (!m_currentTemplateEditComboBox || index < 0
            || index >= m_currentTemplateEditComboBox->count()) {
        m_templateService->cancel();
        m_templateService->setActivePreparedTemplate(
                    PreparedTemplateSnapshot());
        m_selectedTemplateInvalid = false;
        m_currentTemplateDisplayName.clear();
        m_currentTemplateNameVisible = false;
        updateCurrentTemplateName();
        return false;
    }
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        return false;
    }
    const QString path = m_currentTemplateEditComboBox
            ->itemData(index).toString();
    QString errorMessage;
    if (!m_templateService->beginEdit(path, mode, &errorMessage)) {
        m_templateService->cancel();
        m_templateService->setActivePreparedTemplate(
                    PreparedTemplateSnapshot());
        m_selectedTemplateInvalid = true;
        m_currentTemplateDisplayName = QFileInfo(path).fileName()
                + QStringLiteral("（状态异常）");
        m_currentTemplateNameVisible = true;
        updateCurrentTemplateName();
        if (m_callbacks.updateOperationUiState) {
            m_callbacks.updateOperationUiState();
        }
        if (showMessage) {
            showWarning(QStringLiteral("模板加载失败"), errorMessage);
        }
        return false;
    }
    m_selectedTemplateInvalid = false;
    if (m_callbacks.updateOperationUiState) {
        m_callbacks.updateOperationUiState();
    }
    PreparedTemplateSnapshot prepared;
    if (!m_templateService->loadPreparedTemplate(
            path, mode, &prepared, &errorMessage)) {
        if (showMessage) {
            showWarning(QStringLiteral("模板状态异常"),
                        QStringLiteral("模板可以继续编辑，但目前不能用于检测：\n%1")
                        .arg(errorMessage));
        }
        prepared.reset();
    }
    m_templateService->setActivePreparedTemplate(prepared);
    const EditableTemplate &editable = m_templateService->draft();
    applyTemplateSettingsToUi(editable.settings);
    if (!editable.rawImage.empty()
            && m_callbacks.displayPreviewFrame) {
        m_callbacks.displayPreviewFrame(editable.rawImage);
    }
    m_currentTemplateDisplayName = QFileInfo(path).fileName();
    m_currentTemplateNameVisible = true;
    updateCurrentTemplateName();
    clearTemplateDirty();
    return true;
}

void TemplateEditorPage::restoreTemplatesForMode(
        const QString &modeId, bool showMessage)
{
    Q_UNUSED(modeId)
    refreshCurrentTemplateEditor();
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)) {
        clearTemplateState();
        return;
    }
    if (mode == DetectionMode::Tissue) {
        clearTemplateState();
        if (m_view.lineEdit_tissueRoughnessThreshold) {
            m_view.lineEdit_tissueRoughnessThreshold->setText(
                        QString::number(
                            m_settingsService->current().detectionSchemes
                            .tissueRoughnessThreshold, 'f', 3));
        }
        return;
    }
    loadTemplateAtIndex(
                m_currentTemplateEditComboBox
                ? m_currentTemplateEditComboBox->currentIndex() : -1,
                showMessage);
}

void TemplateEditorPage::clearTemplateState()
{
    m_templateService->cancel();
    m_templateService->setActivePreparedTemplate(
                PreparedTemplateSnapshot());
    m_currentTemplateDisplayName.clear();
    m_currentTemplateNameVisible = false;
    m_selectedTemplateInvalid = false;
    updateCurrentTemplateName();
    clearTemplateDirty();
}

int TemplateEditorPage::currentTemplateIndex() const
{
    return m_currentTemplateEditComboBox
            ? m_currentTemplateEditComboBox->currentIndex() : -1;
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
                m_templateService, m_settingsService, dialogParent());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    restoreTemplatesForMode(currentDetectModeId(), true);
}

void TemplateEditorPage::removeCurrentTemplate()
{
    DetectionMode mode;
    if (!detectionModeFromUiId(currentDetectModeId(), &mode)
            || mode == DetectionMode::Tissue) {
        return;
    }
    QStringList paths = currentModeTemplatePaths();
    const int index = currentTemplateIndex();
    if (index < 0 || index >= paths.size()) {
        return;
    }
    paths.removeAt(index);
    const OperationResult saved = m_settingsService->saveTemplatePaths(
                mode, paths);
    if (!saved.isSuccess()) {
        showCritical(QStringLiteral("移除失败"), saved.error.userMessage);
        return;
    }
    restoreTemplatesForMode(currentDetectModeId(), false);
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
        applyCurrentTissueThreshold();
        return;
    }
    if (m_captureState != CaptureState::Frozen
            || !m_inspectionService->hasCurrentCameraImage()) {
        if (m_templateService->isActive()
                && !m_templateService->currentDirectoryPath().isEmpty()) {
            saveCurrentDraft(true);
            return;
        }
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先制作并冻结模板画面。"));
        return;
    }
    const QRect trackingUi = imageLabel->getTrackingRect().normalized();
    const QRect barcodeUi = imageLabel->getBarcodeRect().normalized();
    const QPolygon dateUi = imageLabel->getDetectionPoly();
    if (trackingUi.width() <= 5 || trackingUi.height() <= 5
            || dateUi.size() < 3
            || !imageLabel->isDetectionPolyComplete()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请完成定位区域和闭合的检测区域。"));
        return;
    }
    if (mode == DetectionMode::BarcodeWord
            && (!m_barcodeTemplateReadable
                || m_validatedBarcodeRect != barcodeUi)) {
        QString reason;
        if (!validateBarcodeTemplateRect(
                barcodeUi, barcodeTemplateValidationOptions(), &reason)) {
            showWarning(QStringLiteral("二维码扫描失败"), reason);
            return;
        }
        acceptBarcodeTemplateValidation(barcodeUi);
    }
    int threshold = TemplateSettings::DefaultImageThresholdPercent;
    if (mode != DetectionMode::Ocr
            && (!parseIntValue(m_view.lineEdit_imageThreshold->text(),
                               &threshold)
                || threshold < 0 || threshold > 100)) {
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("图像阈值必须是 0 到 100 的整数。"));
        return;
    }
    const QString parentDirectory = QFileDialog::getExistingDirectory(
                dialogParent(), QStringLiteral("选择模板保存位置"));
    if (parentDirectory.isEmpty()) {
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(
                dialogParent(), QStringLiteral("新建模板"),
                QStringLiteral("模板名称："), QLineEdit::Normal,
                QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    if (QFileInfo(name).fileName() != name) {
        showWarning(QStringLiteral("模板名称无效"),
                    QStringLiteral("模板名称不能包含路径分隔符。"));
        return;
    }
    const QString target = QDir(parentDirectory).filePath(name);
    const bool targetExists = QFileInfo::exists(target);
    if (targetExists
            && QMessageBox::question(
                dialogParent(), QStringLiteral("模板已存在"),
                QStringLiteral("模板“%1”已存在，是否完全覆盖？\n"
                               "确认后原模板文件夹中的全部内容都会被替换。")
                .arg(name), QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString errorMessage;
    if (!m_templateService->beginNew(mode, &errorMessage)) {
        showCritical(QStringLiteral("新建模板失败"), errorMessage);
        return;
    }
    const cv::Mat rawImage =
            m_inspectionService->currentCameraImageClone();
    const QPixmap *pixmap = imageLabel->pixmap();
    const QSize imageSize(rawImage.cols, rawImage.rows);
    const TemplateDisplayGeometry display = {
        imageLabel->size(),
        pixmap && !pixmap->isNull()
        ? pixmap->size()
        : imageSize.scaled(imageLabel->size(), Qt::KeepAspectRatio),
        imageSize
    };
    const TemplateGeometryResult geometry =
            m_templateService->buildGeometry(
                trackingUi, barcodeUi, dateUi,
                mode == DetectionMode::BarcodeWord, display);
    if (!geometry.valid) {
        showWarning(QStringLiteral("模板区域无效"),
                    geometry.errorMessage);
        return;
    }
    TemplateSettings settings;
    settings.detectionMode = mode;
    settings.targetText = m_view.textEdit_targetText
            ? m_view.textEdit_targetText->toPlainText().trimmed()
            : QString();
    settings.imageThresholdPercent = threshold;
    settings.trackingRoi = geometry.trackingRoi;
    InitialTemplateAssets assets;
    assets.rawImage = rawImage;
    assets.trackingImageRect = geometry.trackingImageRect;
    assets.datePolygon = geometry.datePolygon;
    assets.barcodePolygon = geometry.barcodePolygon;
    if (mode == DetectionMode::Stamp) {
        const cv::Rect ring = getQuickRectROI(rawImage, "STAMP_RING");
        if (ring.width <= 5 || ring.height <= 5
                || ring.x < 0 || ring.y < 0
                || ring.x + ring.width > rawImage.cols
                || ring.y + ring.height > rawImage.rows) {
            showWarning(QStringLiteral("钢印标定无效"),
                        QStringLiteral("请重新选择有效的钢印环区域。"));
            return;
        }
        assets.stampRing = rawImage(ring).clone();
        const cv::Point2f center(ring.x + ring.width / 2.0f,
                                 ring.y + ring.height / 2.0f);
        const std::vector<cv::Point> polygon =
                getPolygonROI(rawImage, "STAMP_REGION");
        if (polygon.size() < 3u) {
            showWarning(QStringLiteral("钢印标定无效"),
                        QStringLiteral("钢印区域至少需要三个点。"));
            return;
        }
        for (const cv::Point &point : polygon) {
            assets.stampPolygon.emplace_back(
                        point.x - center.x, point.y - center.y);
        }
    }
    EditableTemplate editable;
    if (!m_templateService->stageInitialAssets(
            assets, &settings, &editable, &errorMessage)
            || !m_templateService->replaceDraft(
                editable, &errorMessage)) {
        showCritical(QStringLiteral("模板准备失败"), errorMessage);
        return;
    }
    PreparedTemplateSnapshot prepared;
    if (!m_templateService->save(
            target, false, &prepared, &errorMessage)) {
        showCritical(QStringLiteral("模板保存失败"), errorMessage);
        return;
    }
    QStringList paths = currentModeTemplatePaths();
    if (isMultiTemplateMode(mode)) {
        if (!paths.contains(target, Qt::CaseInsensitive)) {
            paths.append(target);
        }
    } else {
        paths = QStringList() << target;
    }
    const OperationResult selected = m_settingsService->saveTemplatePaths(
                mode, paths);
    if (!selected.isSuccess()) {
        showWarning(QStringLiteral("模板已保存但未应用"),
                    selected.error.userMessage);
    }
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    clearBarcodeTemplateValidation();
    hideTemplateGuide();
    resetTemplateCaptureState();
    refreshCurrentTemplateEditor();
    const int savedIndex = m_currentTemplateEditComboBox
            ? m_currentTemplateEditComboBox->findData(
                QDir::cleanPath(QFileInfo(target).absoluteFilePath()))
            : -1;
    if (m_currentTemplateEditComboBox && savedIndex >= 0) {
        const QSignalBlocker blocker(m_currentTemplateEditComboBox);
        m_currentTemplateEditComboBox->setCurrentIndex(savedIndex);
        loadTemplateAtIndex(savedIndex, false);
    }
    showInfo(QStringLiteral("保存成功"),
             targetExists
             ? QStringLiteral("模板已完全覆盖并应用。")
             : QStringLiteral("模板已新建并应用。"));
}

bool TemplateEditorPage::saveCurrentDraft(bool showSuccessMessage)
{
    if (!m_templateService->isActive()
            || m_templateService->currentDirectoryPath().isEmpty()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("当前没有可编辑模板。"));
        return false;
    }
    PreparedTemplateSnapshot prepared;
    QString errorMessage;
    if (!m_templateService->save(
            m_templateService->currentDirectoryPath(), true,
            &prepared, &errorMessage)) {
        showCritical(QStringLiteral("模板保存失败"), errorMessage);
        return false;
    }
    m_templateService->setActivePreparedTemplate(prepared);
    clearTemplateDirty();
    if (showSuccessMessage) {
        showInfo(QStringLiteral("保存成功"),
                 QStringLiteral("当前模板已更新。"));
    }
    return true;
}

void TemplateEditorPage::applyCurrentTargetText()
{
    if (!m_templateService->isActive()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择当前编辑模板。"));
        return;
    }
    EditableTemplate value = m_templateService->draft();
    value.settings.targetText = m_view.textEdit_targetText
            ->toPlainText().trimmed();
    QString errorMessage;
    if (!m_templateService->replaceDraft(value, &errorMessage)
            || !saveCurrentDraft(false)) {
        if (!errorMessage.isEmpty()) {
            showCritical(QStringLiteral("目标文字保存失败"), errorMessage);
        }
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("当前模板的目标文字已保存。"));
}

void TemplateEditorPage::applyCurrentImageThreshold()
{
    int threshold = 0;
    if (!parseIntValue(m_view.lineEdit_imageThreshold->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("图像阈值必须是 0 到 100 的整数。"));
        return;
    }
    if (!m_templateService->isActive()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择当前编辑模板。"));
        return;
    }
    EditableTemplate value = m_templateService->draft();
    value.settings.imageThresholdPercent = threshold;
    QString errorMessage;
    if (!m_templateService->replaceDraft(value, &errorMessage)
            || !saveCurrentDraft(false)) {
        if (!errorMessage.isEmpty()) {
            showCritical(QStringLiteral("阈值保存失败"), errorMessage);
        }
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("当前模板的图像阈值已保存。"));
}

void TemplateEditorPage::applyCurrentTissueThreshold()
{
    bool ok = false;
    const double value = m_view.lineEdit_tissueRoughnessThreshold
            ->text().trimmed().toDouble(&ok);
    if (!ok || value < 0.0) {
        showWarning(QStringLiteral("参数错误"),
                    QStringLiteral("纸巾粗糙度阈值必须是非负数。"));
        return;
    }
    const OperationResult saved =
            m_settingsService->saveTissueThreshold(value);
    if (!saved.isSuccess()) {
        showCritical(QStringLiteral("纸巾阈值保存失败"),
                     saved.error.userMessage);
        return;
    }
    showInfo(QStringLiteral("成功"),
             QStringLiteral("纸巾检测阈值已保存。"));
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
    if (!m_templateService->updateTemplates(
            currentModeTemplatePaths(), mode, update, errorMessage)) {
        return false;
    }
    restoreTemplatesForMode(currentDetectModeId(), false);
    return true;
}

void TemplateEditorPage::applyBatchTargetText()
{
    const QString text = m_view.textEdit_targetText
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
    if (!parseIntValue(m_view.lineEdit_imageThreshold->text(), &threshold)
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
    if (!m_templateService->isActive()
            || m_templateService->draft().rawImage.empty()) {
        showWarning(QStringLiteral("提示"),
                    QStringLiteral("请先选择并加载一个模板。"));
        return;
    }
    EditableTemplate value = m_templateService->draft();
    CharacterTemplateEditorDialog dialog(
                imageFromBgrMat(value.rawImage),
                value.settings, dialogParent());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    value.settings = dialog.resultSettings();
    QString error;
    if (!m_templateService->stageCharacterAssets(
            dialog.characterImages(), &value, &error)
            || !m_templateService->replaceDraft(value, &error)
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
    if (m_view.textEdit_targetText) {
        connect(m_view.textEdit_targetText, &QTextEdit::textChanged,
                this, [this]() { refreshTemplateDirty(); });
    }
    if (m_view.lineEdit_imageThreshold) {
        connect(m_view.lineEdit_imageThreshold, &QLineEdit::textChanged,
                this, [this](const QString &) { refreshTemplateDirty(); });
    }
}

void TemplateEditorPage::refreshTemplateDirty()
{
    if (!m_templateService->isActive()) {
        clearTemplateDirty();
        return;
    }
    const TemplateSettings &settings = m_templateService->draft().settings;
    m_settingsEditState->setTemplateTargetDirty(
                m_view.textEdit_targetText
                && m_view.textEdit_targetText->toPlainText().trimmed()
                   != settings.targetText);
    bool ok = false;
    const int threshold = m_view.lineEdit_imageThreshold
            ? m_view.lineEdit_imageThreshold->text().toInt(&ok) : 0;
    m_settingsEditState->setTemplateThresholdDirty(
                settings.detectionMode != DetectionMode::Ocr
                && (!ok || threshold != settings.imageThresholdPercent));
}

void TemplateEditorPage::clearTemplateDirty()
{
    m_settingsEditState->clearTemplateDirty();
}

void TemplateEditorPage::applyTemplateSettingsToUi(
        const TemplateSettings &settings)
{
    if (m_callbacks.applyTemplateSettingsToUi) {
        m_callbacks.applyTemplateSettingsToUi(settings);
        return;
    }
    QSignalBlocker targetBlocker(m_view.textEdit_targetText);
    QSignalBlocker thresholdBlocker(m_view.lineEdit_imageThreshold);
    m_view.textEdit_targetText->setPlainText(settings.targetText);
    m_view.lineEdit_imageThreshold->setText(
                QString::number(settings.imageThresholdPercent));
}

PreparedTemplateSnapshot TemplateEditorPage::activePreparedTemplate() const
{
    return m_templateService->activePreparedTemplate();
}

void TemplateEditorPage::setCurrentTemplateNameVisible(bool visible)
{
    m_currentTemplateNameVisible = visible;
    updateCurrentTemplateName();
}

void TemplateEditorPage::updateCurrentTemplateName()
{
    if (!m_view.lineEdit_currentTemplateName) {
        return;
    }
    m_view.lineEdit_currentTemplateName->setVisible(
                m_currentTemplateNameVisible);
    m_view.lineEdit_currentTemplateName->setText(
                m_currentTemplateNameVisible
                ? m_currentTemplateDisplayName : QString());
}

void TemplateEditorPage::setupTemplateGuide()
{
    if (m_templateGuideFrame || !m_view.verticalLayout_imageDisplay) {
        return;
    }
    m_templateGuideFrame = new QFrame(m_view.groupBox_imageDisplay);
    QVBoxLayout *layout = new QVBoxLayout(m_templateGuideFrame);
    m_templateGuideTitleLabel = new QLabel(
                QStringLiteral("模板制作向导"), m_templateGuideFrame);
    m_templateGuideBodyLabel = new QLabel(m_templateGuideFrame);
    m_templateGuideBodyLabel->setWordWrap(true);
    layout->addWidget(m_templateGuideTitleLabel);
    layout->addWidget(m_templateGuideBodyLabel);
    m_view.verticalLayout_imageDisplay->addWidget(m_templateGuideFrame);
    m_templateGuideFrame->hide();
}

void TemplateEditorPage::adjustTemplateGuideHeight()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->adjustSize();
    }
}

void TemplateEditorPage::updateTemplateGuideText(
        const QString &title, const QString &body)
{
    if (m_templateGuideTitleLabel) m_templateGuideTitleLabel->setText(title);
    if (m_templateGuideBodyLabel) m_templateGuideBodyLabel->setText(body);
    if (m_templateGuideFrame) m_templateGuideFrame->show();
}

void TemplateEditorPage::hideTemplateGuide()
{
    if (m_templateGuideFrame) m_templateGuideFrame->hide();
}

void TemplateEditorPage::updateImageDisplayStatusText(const QString &body)
{
    if (m_view.label_runtimeStatus && !body.isEmpty()) {
        m_view.label_runtimeStatus->setText(body);
    }
}

void TemplateEditorPage::showTemplateGuideForCurrentMode()
{
    DetectionMode mode = DetectionMode::Stamp;
    detectionModeFromUiId(currentDetectModeId(), &mode);
    updateTemplateGuideText(
                QStringLiteral("模板制作向导"),
                mode == DetectionMode::BarcodeWord
                ? QStringLiteral("依次框选定位区域、二维码区域和日期检测区域。")
                : QStringLiteral("依次框选定位区域和日期检测区域。"));
}

void TemplateEditorPage::handleTemplateGuideEvent(
        const QString &eventName, int pointCount)
{
    Q_UNUSED(eventName)
    Q_UNUSED(pointCount)
    showTemplateGuideForCurrentMode();
}

void TemplateEditorPage::setupManualCharacterCropUi()
{
    m_manualCharacterCropButton =
            m_view.pushButton_editCharacterTemplates;
    if (m_manualCharacterCropButton) {
        connect(m_manualCharacterCropButton, &QPushButton::clicked,
                this, [this]() {
            showManualCharacterTemplateEditorDialog();
        });
    }
}

void TemplateEditorPage::clearBarcodeTemplateValidation()
{
    m_barcodeTemplateReadable = false;
    m_validatedBarcodeRect = QRect();
}

TemplateBarcodeValidationOptions
TemplateEditorPage::barcodeTemplateValidationOptions() const
{
    if (m_templateService->isActive()) {
        const TemplateBarcodeParameters &parameters =
                m_templateService->draft().settings.barcodeParameters;
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
            m_inspectionService->currentCameraImageClone();
    const QPixmap *pixmap = imageLabel->pixmap();
    const QSize sourceSize(source.cols, source.rows);
    const TemplateDisplayGeometry geometry = {
        imageLabel->size(),
        pixmap && !pixmap->isNull()
        ? pixmap->size()
        : sourceSize.scaled(imageLabel->size(), Qt::KeepAspectRatio),
        sourceSize
    };
    const QRect imageRect = m_templateService->mapDisplayRectToImage(
                uiBarcodeRect, geometry);
    return m_templateService->validateBarcodeTemplate(
                source, imageRect, options, failureReason);
}

bool TemplateEditorPage::barcodeTemplateReadable() const
{
    return m_barcodeTemplateReadable;
}

QRect TemplateEditorPage::validatedBarcodeRect() const
{
    return m_validatedBarcodeRect;
}

void TemplateEditorPage::acceptBarcodeTemplateValidation(
        const QRect &barcodeRect)
{
    m_barcodeTemplateReadable = true;
    m_validatedBarcodeRect = barcodeRect;
}
