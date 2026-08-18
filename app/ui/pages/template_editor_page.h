#pragma once

#include "application/template_application_service.h"

#include <QMap>
#include <QObject>
#include <QRect>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <vector>

class ImageLabel;
class InspectionApplicationService;
class MachineSettingsPage;
class SettingsApplicationService;
class SettingsEditState;
class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QVBoxLayout;
class QWidget;

struct TemplateEditorViewBindings
{
    QWidget *parentWidget = nullptr;
    QObject *eventFilterTarget = nullptr;
    ImageLabel *imageLabel_templateCanvas = nullptr;
    QTextEdit *textEdit_targetText = nullptr;
    QLineEdit *lineEdit_imageThreshold = nullptr;
    QLineEdit *lineEdit_tissueRoughnessThreshold = nullptr;
    QComboBox *comboBox_detectionMode = nullptr;
    QComboBox *comboBox_plcTriggerMode = nullptr;
    QComboBox *comboBox_imageRotation = nullptr;
    QComboBox *comboBox_colorChannel = nullptr;
    QLineEdit *lineEdit_currentRecipeName = nullptr;
    QLabel *label_runtimeStatus = nullptr;
    QLabel *label_targetText = nullptr;
    QLabel *label_imageThreshold = nullptr;
    QLabel *label_rejectDistance = nullptr;
    QLabel *label_photoDistance = nullptr;
    QLabel *label_rejectTime = nullptr;
    QLabel *label_hardwareTriggerDelay = nullptr;
    QLabel *label_photoTime = nullptr;
    QLabel *label_cameraGain = nullptr;
    QLabel *label_rejectPosition = nullptr;
    QLabel *label_imageRotation = nullptr;
    ImageLabel *imageLabel_inspectionDisplay = nullptr;
    QGroupBox *groupBox_imageDisplay = nullptr;
    QVBoxLayout *verticalLayout_imageDisplay = nullptr;
    QPushButton *pushButton_editCharacterTemplates = nullptr;
    QPushButton *pushButton_applyTargetText = nullptr;
    QPushButton *pushButton_applyBatchTargetText = nullptr;
    QPushButton *pushButton_applyBatchImageThreshold = nullptr;
    QPushButton *pushButton_applyPhotoDistance = nullptr;
    QPushButton *pushButton_applyImageThreshold = nullptr;
    QPushButton *pushButton_applyColorChannel = nullptr;
    QPushButton *pushButton_applyPlcProcessParameters = nullptr;
    QPushButton *pushButton_applyImageRotation = nullptr;
    QPushButton *pushButton_resetRejectQueue = nullptr;
    QPushButton *pushButton_applyCameraGain = nullptr;
    QPushButton *pushButton_applyCameraExposure = nullptr;
    QPushButton *pushButton_applyTissueRoughnessThreshold = nullptr;
    QPushButton *pushButton_applyPlcTriggerMode = nullptr;
    QPushButton *pushButton_connectPlc = nullptr;
    QPushButton *pushButton_disconnectPlc = nullptr;
    QPushButton *pushButton_browseImageSavePath = nullptr;
    QToolButton *toolButton_createTemplate = nullptr;
    QCheckBox *checkBox_hardwareTriggerEnabled = nullptr;
};

struct TemplateEditorPageCallbacks
{
    std::function<void()> updateOperationUiState;
    std::function<void()> updateTissueVisibility;
    std::function<void()> clearTransientView;
    std::function<void(const cv::Mat &)> displayPreviewFrame;
    std::function<bool()> isApplyingSettings;
    std::function<bool()> isUpdatingSettingsUi;
    std::function<bool(bool)> saveSettings;
    std::function<void(const RecipeProfile &)> applyRecipeProfileToUi;
};

class TemplateEditorPage : public QObject
{
public:
    enum class CaptureState
    {
        Idle,
        Previewing,
        Frozen
    };

    TemplateEditorPage(
        const TemplateEditorViewBindings &view,
        TemplateApplicationService *templateService,
        InspectionApplicationService *inspectionService,
        SettingsApplicationService *settingsService,
        MachineSettingsPage *settingsPageController,
        SettingsEditState *settingsEditState,
        const TemplateEditorPageCallbacks &callbacks,
        QObject *parent = nullptr);

    void setEditorsEnabled(bool enabled);
    QFrame *guideFrame() const;
    QPushButton *manualCharacterCropButton() const;

    bool templateOperationActive() const;
    CaptureState captureState() const;
    bool startTemplatePreview();
    bool freezeTemplatePreview();
    bool stopTemplatePreview();
    void resetTemplateCaptureState();
    void handleTemplateCaptureButton();
    void handlePreviewFrame(quint64 sessionId, const cv::Mat &image);
    void handlePreviewFailure(quint64 sessionId, const QString &reason);

    void clearBarcodeTemplateValidation();
    bool validateBarcodeTemplateRect(
        const QRect &uiBarcodeRect,
        const TemplateBarcodeValidationOptions &options,
        TemplateBarcodeValidationResult *barcode,
        QString *failureReason);
    TemplateBarcodeValidationOptions
    barcodeTemplateValidationOptions() const;

    void updateCurrentTemplateName();
    void setupTemplateGuide();
    void adjustTemplateGuideHeight();
    void updateTemplateGuideText(const QString &title,
                                 const QString &body);
    void hideTemplateGuide();
    void updateImageDisplayStatusText(const QString &body);
    void showTemplateGuideForCurrentMode();
    void handleTemplateGuideEvent(const QString &eventName,
                                  int pointCount);
    void setupManualCharacterCropUi();

    void setupRecipeProfileDirtyTracking();
    void refreshTemplateTargetTextDirty();
    void refreshTemplateImageThresholdDirty();
    void refreshRecipeProfileDirty();
    void clearTemplateTargetTextDirty();
    void clearTemplateImageThresholdDirty();
    void clearRecipeProfileDirty();
    void updateRecipeProfileDirtyUi();

    void showManualCharacterTemplateEditorDialog();
    void selectPublishedRecipeForCurrentMode();
    void saveCurrentTemplate();
    void setupWordTemplateEditorCombo();

    void clearWordMultiTemplateState();
    void clearSingleTemplateRecipeState();
    QString detectModeIdForIndex(int index) const;
    QString currentDetectModeId() const;
    void restoreTemplatesForMode(const QString &modeId, bool showMessage);
    void refreshWordTemplateEditorCombo();
    void applyWordTemplateEditorSelection(int comboIndex);
    void setCurrentWordTemplateEditIndex(int profileIndex);
    void publishCurrentWordTemplateGroup();
    void publishCurrentSingleTemplateRecipe();
    void selectPublishedRecipe();
    bool activatePublishedWordRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QStringList *pendingMessages,
        QString *errorMessage);
    bool activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage);
    bool republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage);
    int currentWordTemplateProfileIndex() const;
    void displayWordTemplateRawImage(const WordTemplateProfile &profile);
    bool loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    bool saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage);
    bool publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage);
    bool publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage);

    void applyCurrentTargetText();
    void applyCurrentImageThreshold();
    void applyCurrentTissueThreshold();
    void applyBatchTargetText();
    void applyBatchImageThreshold();

    const std::vector<WordTemplateProfile> &wordTemplateProfiles() const;
    PreparedRecipeSnapshot activePreparedRecipe() const;
    void setCurrentTemplateNameVisible(bool visible);
    bool barcodeTemplateReadable() const;
    QRect validatedBarcodeRect() const;
    QString validatedBarcodeText() const;
    void acceptBarcodeTemplateValidation(
        const QRect &barcodeRect,
        const QString &barcodeText);

private:
    QWidget *dialogParent() const;
    bool isInspectionBusy() const;
    bool isCameraOpen() const;
    void showParameterInfo(const QString &title,
                           const QString &message);
    void showParameterInfoWithRedWarning(
        const QString &title,
        const QString &message,
        const QString &warningMessage);
    void showParameterInfoAsError(const QString &title,
                                  const QString &message);
    void showParameterWarning(const QString &title,
                              const QString &message);
    void showParameterCritical(const QString &title,
                               const QString &message);
    bool saveSettings(bool showErrorMessage = true);
    void applyRecipeProfileToUi(const RecipeProfile &settings);
    void editActiveRecipeCharacterAssets(int profileIndex);
    void publishCurrentRecipeSession();
    bool activatePublishedTissueRecipe(
        const QString &recipeId,
        bool showErrorMessage,
        QString *errorMessage);

    TemplateEditorViewBindings m_view;
    TemplateApplicationService *m_templateService = nullptr;
    InspectionApplicationService *m_inspectionService = nullptr;
    SettingsApplicationService *m_settingsService = nullptr;
    MachineSettingsPage *m_settingsPageController = nullptr;
    SettingsEditState *m_settingsEditState = nullptr;
    TemplateEditorPageCallbacks m_callbacks;
    ImageLabel *imageLabel = nullptr;

    CaptureState m_captureState = CaptureState::Idle;
    quint64 m_previewSessionId = 0;
    cv::Mat m_lastPreviewFrame;
    bool m_currentTemplateNameVisible = false;
    QWidget *m_wordTemplateEditWidget = nullptr;
    QLabel *m_wordTemplateEditLabel = nullptr;
    QComboBox *m_wordTemplateEditComboBox = nullptr;
    QPushButton *m_publishTemplateGroupButton = nullptr;
    QFrame *m_templateGuideFrame = nullptr;
    QLabel *m_templateGuideTitleLabel = nullptr;
    QLabel *m_templateGuideBodyLabel = nullptr;
    QPushButton *m_manualCharacterCropButton = nullptr;
    QPushButton *m_publishedRecipeButton = nullptr;
    int m_currentWordTemplateEditIndex = -1;
    QString m_templateTargetLabelText;
    QString m_templateThresholdLabelText;
    QString m_currentTemplateDisplayName;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_validatedBarcodeText;
};
