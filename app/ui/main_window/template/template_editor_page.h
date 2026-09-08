// 文件作用：维护模板取景、统一选择、当前编辑模板和模板参数应用。
#pragma once

#include "application/template_application_service.h"
#include "ui/main_window/inspection_image_canvas.h"
#include "ui/main_window/operation_ui_policy.h"

#include <QObject>
#include <QRect>
#include <QStringList>

#include <functional>

class InspectionApplicationService;
class SettingsApplicationService;
class SettingsEditState;
class QWidget;

namespace Ui {
class MainWindow;
class DetectionSettingsPage;
class ImageSettingsPage;
}

class TemplateEditorPage : public QObject
{
    Q_OBJECT

public:
    enum class CaptureState { Idle, Previewing, Frozen };

    TemplateEditorPage(
        QWidget &dialogParent,
        Ui::MainWindow &mainWindowUi,
        Ui::DetectionSettingsPage &detectionSettingsUi,
        Ui::ImageSettingsPage &imageSettingsUi,
        TemplateApplicationService &templateService,
        InspectionApplicationService &inspectionService,
        SettingsApplicationService &settingsService,
        SettingsEditState &settingsEditState);

    void applyOperationState(
        OperationUiState requestedState,
        const OperationUiSnapshot &snapshot);
    bool templateOperationActive() const;
    CaptureState captureState() const;
    bool startTemplatePreview();
    bool freezeTemplatePreview();
    bool stopTemplatePreview(bool writeLog = true);
    void resetTemplateCaptureState(bool writePreviewStopLog = true);
    void handleTemplateCaptureButton();
    void handlePreviewFailure(const QString &reason);

    void updateTemplateGuideText(const QString &title,
                                 const QString &body);
    void hideTemplateGuide();
    void cancelTemplateDrawing();
    void refreshTemplateDirty();
    void clearTemplateDirty();
    void showManualCharacterTemplateEditorDialog();

    void selectTemplatesForCurrentMode();
    void saveCurrentTemplate();
    void clearTemplateState();
    QString detectModeIdForIndex(int index) const;
    QString currentDetectModeId() const;
    void restoreTemplatesForMode(const QString &modeId,
                                 bool showMessage);
    void refreshCurrentTemplateEditor();
    int currentTemplateIndex() const;
    void removeCurrentTemplate();

    void applyCurrentTargetText();
    void applyCurrentImageThreshold();
    void applyBatchTargetText();
    void applyBatchImageThreshold();

    PreparedTemplateSnapshot activePreparedTemplate() const;

signals:
    void operationUiRefreshRequested();
    void previewFramePresentationRequested(const cv::Mat &image);

private:
    bool isCameraOpen() const;
    void showInfo(const QString &title, const QString &message);
    void showWarning(const QString &title, const QString &message);
    void showCritical(const QString &title, const QString &message);
    void applyTemplateSettingsToUi(const TemplateSettings &settings);
    void setupCurrentTemplateEditor();
    void adjustTemplateGuideHeight();
    void showInspectionStatus();
    void showTemplateImageSource(const QString &templateName);
    void showTemplateCaptureStatus(const QString &body);
    void handleTemplateDrawingChanged(
        InspectionImageCanvas::DrawingStep step,
        InspectionImageCanvas::DrawingEvent event,
        int pointCount);
    bool validateCompletedBarcode();
    void updateDrawingGuide(InspectionImageCanvas::DrawingStep step,
                            InspectionImageCanvas::DrawingEvent event,
                            int pointCount);
    void askToSaveCompletedTemplate(DetectionMode mode);
    void clearBarcodeTemplateValidation();
    bool validateBarcodeTemplateRect(
        const QRect &uiBarcodeRect,
        const TemplateBarcodeValidationOptions &options,
        QString *failureReason);
    TemplateBarcodeValidationOptions barcodeTemplateValidationOptions() const;
    void acceptBarcodeTemplateValidation(const QRect &barcodeRect);
    void setupManualCharacterCropUi();
    void setupTemplateDirtyTracking();
    void connectPageActions();
    QStringList currentModeTemplatePaths() const;
    bool saveCurrentDraft(bool showSuccessMessage);
    bool loadTemplateAtIndex(int index, bool showMessage);
    bool updateAllSelectedTemplates(
        const std::function<void(TemplateSettings *)> &update,
        QString *errorMessage);

    QWidget &m_dialogParent;
    Ui::MainWindow &m_mainWindowUi;
    Ui::DetectionSettingsPage &m_detectionSettingsUi;
    Ui::ImageSettingsPage &m_imageSettingsUi;
    TemplateApplicationService &m_templateService;
    InspectionApplicationService &m_inspectionService;
    SettingsApplicationService &m_settingsService;
    SettingsEditState &m_settingsEditState;

    CaptureState m_captureState = CaptureState::Idle;
    bool m_selectedTemplateInvalid = false;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_targetTextLabelText;
    QString m_imageThresholdLabelText;
};
