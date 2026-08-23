// 文件作用：维护模板取景、统一选择、当前编辑模板和模板参数应用。
#pragma once

#include "application/template_application_service.h"
#include "ui/controllers/operation_ui_policy.h"
#include "ui/widgets/image_label.h"

#include <QObject>
#include <QRect>
#include <QStringList>

#include <functional>

class InspectionApplicationService;
class SettingsApplicationService;
class SettingsEditState;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QWidget;

struct TemplateEditorViewBindings
{
    QWidget *parentWidget = nullptr;
    ImageLabel *imageLabel_templateCanvas = nullptr;
    QLabel *label_targetText = nullptr;
    QLabel *label_imageThreshold = nullptr;
    QTextEdit *textEdit_targetText = nullptr;
    QLineEdit *lineEdit_imageThreshold = nullptr;
    QComboBox *comboBox_detectionMode = nullptr;
    QComboBox *comboBox_imageRotation = nullptr;
    QComboBox *comboBox_colorChannel = nullptr;
    QLineEdit *lineEdit_currentTemplateName = nullptr;
    QLabel *label_runtimeStatus = nullptr;
    QPushButton *pushButton_editCharacterTemplates = nullptr;
    QPushButton *pushButton_applyTargetText = nullptr;
    QPushButton *pushButton_applyBatchTargetText = nullptr;
    QPushButton *pushButton_applyBatchImageThreshold = nullptr;
    QPushButton *pushButton_applyImageThreshold = nullptr;
    QPushButton *pushButton_saveTemplate = nullptr;
    QToolButton *toolButton_selectTemplate = nullptr;
    QWidget *widget_currentTemplateEditor = nullptr;
    QLabel *label_currentEditTemplate = nullptr;
    QComboBox *comboBox_currentEditTemplate = nullptr;
    QPushButton *pushButton_removeCurrentTemplate = nullptr;
    QFrame *frame_templateGuide = nullptr;
    QLabel *label_templateGuideTitle = nullptr;
    QLabel *label_templateGuideBody = nullptr;
};

struct TemplateEditorPageCallbacks
{
    std::function<void()> updateOperationUiState;
    std::function<void(const cv::Mat &)> displayPreviewFrame;
};

class TemplateEditorPage : public QObject
{
public:
    enum class CaptureState { Idle, Previewing, Frozen };

    TemplateEditorPage(
        const TemplateEditorViewBindings &view,
        TemplateApplicationService *templateService,
        InspectionApplicationService *inspectionService,
        SettingsApplicationService *settingsService,
        SettingsEditState *settingsEditState,
        const TemplateEditorPageCallbacks &callbacks,
        QObject *parent = nullptr);

    void applyOperationState(const OperationUiSnapshot &snapshot);
    bool templateOperationActive() const;
    CaptureState captureState() const;
    bool startTemplatePreview();
    bool freezeTemplatePreview();
    bool stopTemplatePreview();
    void resetTemplateCaptureState();
    void handleTemplateCaptureButton();
    void handlePreviewFrame(quint64 sessionId, const cv::Mat &image);
    void handlePreviewFailure(quint64 sessionId, const QString &reason);

    void updateCurrentTemplateName();
    void updateTemplateGuideText(const QString &title,
                                 const QString &body);
    void hideTemplateGuide();
    void updateImageDisplayStatusText(const QString &body);
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
    void setCurrentTemplateNameVisible(bool visible);

private:
    QWidget *dialogParent() const;
    bool isCameraOpen() const;
    void showInfo(const QString &title, const QString &message);
    void showWarning(const QString &title, const QString &message);
    void showCritical(const QString &title, const QString &message);
    void applyTemplateSettingsToUi(const TemplateSettings &settings);
    void setupCurrentTemplateEditor();
    void setupTemplateGuide();
    void adjustTemplateGuideHeight();
    void handleTemplateDrawingChanged(
        ImageLabel::DrawingStep step,
        ImageLabel::DrawingEvent event,
        int pointCount);
    bool validateCompletedBarcode();
    void updateDrawingGuide(ImageLabel::DrawingStep step,
                            ImageLabel::DrawingEvent event,
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

    TemplateEditorViewBindings m_view;
    TemplateApplicationService *m_templateService = nullptr;
    InspectionApplicationService *m_inspectionService = nullptr;
    SettingsApplicationService *m_settingsService = nullptr;
    SettingsEditState *m_settingsEditState = nullptr;
    TemplateEditorPageCallbacks m_callbacks;
    ImageLabel *imageLabel = nullptr;

    CaptureState m_captureState = CaptureState::Idle;
    quint64 m_previewSessionId = 0;
    cv::Mat m_lastPreviewFrame;
    bool m_currentTemplateNameVisible = false;
    bool m_selectedTemplateInvalid = false;
    QWidget *m_currentTemplateEditWidget = nullptr;
    QLabel *m_currentTemplateEditLabel = nullptr;
    QComboBox *m_currentTemplateEditComboBox = nullptr;
    QPushButton *m_removeCurrentTemplateButton = nullptr;
    QFrame *m_templateGuideFrame = nullptr;
    QLabel *m_templateGuideTitleLabel = nullptr;
    QLabel *m_templateGuideBodyLabel = nullptr;
    QPushButton *m_manualCharacterCropButton = nullptr;
    QString m_currentTemplateDisplayName;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_targetTextLabelText;
    QString m_imageThresholdLabelText;
};
