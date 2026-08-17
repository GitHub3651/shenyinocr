#pragma once

#include "BarcodeTypes.h"
#include "application/template_application_service.h"
#include "recipes/prepared_recipe.h"
#include "runtime/template_runtime_profile.h"

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
class MachineSettingsPageController;
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
    ImageLabel *imageLabel = nullptr;
    QTextEdit *dateEdit = nullptr;
    QLineEdit *lineEdit_yuzhi = nullptr;
    QLineEdit *lineEdit_tissueRoughnessThreshold = nullptr;
    QComboBox *comboBox_4 = nullptr;
    QComboBox *comboBox_3 = nullptr;
    QComboBox *comboBox_2 = nullptr;
    QComboBox *comboBox_5 = nullptr;
    QLineEdit *currentTemplateName = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *label = nullptr;
    QLabel *label_4 = nullptr;
    QLabel *label_6 = nullptr;
    QLabel *label_8 = nullptr;
    QLabel *label_10 = nullptr;
    QLabel *label_13 = nullptr;
    QLabel *label_14 = nullptr;
    QLabel *label_16 = nullptr;
    QLabel *label_17 = nullptr;
    QLabel *label_27 = nullptr;
    ImageLabel *image_undetected = nullptr;
    QGroupBox *imagedisplayBox = nullptr;
    QVBoxLayout *verticalLayout_InnerImg = nullptr;
    QPushButton *manualCharacterCropButton = nullptr;
    QPushButton *textsure_btn = nullptr;
    QPushButton *batchTextsure_btn = nullptr;
    QPushButton *batchImageThresholdButton = nullptr;
    QPushButton *WriteVDpushButton = nullptr;
    QPushButton *pushButton_3 = nullptr;
    QPushButton *pushButton_7 = nullptr;
    QPushButton *pushButton_8 = nullptr;
    QPushButton *pushButton_9 = nullptr;
    QPushButton *pushButton_10 = nullptr;
    QPushButton *pushButton_12 = nullptr;
    QPushButton *sureButton = nullptr;
    QPushButton *pushButton_tissueRoughnessThreshold = nullptr;
    QPushButton *plcmodebtn = nullptr;
    QPushButton *ConnectpushButton = nullptr;
    QPushButton *DisconnectpushButton = nullptr;
    QPushButton *pushButton_browseImageSavePath = nullptr;
    QToolButton *VideoShoot = nullptr;
    QCheckBox *checkBox = nullptr;
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
        MachineSettingsPageController *settingsPageController,
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
        const BarcodeDecodeOptions &options,
        BarcodeReadResult *barcode,
        QString *failureReason);
    BarcodeDecodeOptions barcodeTemplateValidationOptions() const;

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
    void markTemplateTargetTextDirty();
    void markTemplateImageThresholdDirty();
    void clearTemplateTargetTextDirty();
    void clearTemplateImageThresholdDirty();
    void clearRecipeProfileDirty();
    void updateRecipeProfileDirtyUi();

    void showManualCharacterTemplateEditorDialog();
    void selectPublishedRecipeForCurrentMode();
    void saveCurrentTemplate();
    void showStampCharacterTemplateEditorDialog();
    void showPublishedRecipeCharacterTemplateEditorDialog(int profileIndex);
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
    void refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const;
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
    QString currentTemplateDisplayName() const;
    void setCurrentTemplateDisplayName(const QString &displayName);
    void setCurrentTemplateNameVisible(bool visible);
    int currentWordTemplateEditIndex() const;
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
    MachineSettingsPageController *m_settingsPageController = nullptr;
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
