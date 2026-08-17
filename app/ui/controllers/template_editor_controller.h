#pragma once

#include "BarcodeTypes.h"
#include "Detector.h"
#include "recipes/prepared_recipe.h"
#include "recipes/recipe_editor_session.h"
#include "recipes/template_mode_memory.h"
#include "runtime/template_runtime_profile.h"

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

class IBarcodeDecoder;
class ImageLabel;
class InspectionApplicationService;
class MachineSettingsPageController;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QWidget;
class Widget;

namespace Ui {
class Widget;
}

class TemplateEditorController : public QObject
{
public:
    TemplateEditorController(
        Widget *host,
        Ui::Widget *ui,
        ImageLabel *imageLabel,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
        QObject *parent = nullptr);

    void bindRuntimeDependencies(
        InspectionApplicationService *inspectionService,
        MachineSettingsPageController *settingsPageController);

    void setEditorsEnabled(bool enabled);
    QFrame *guideFrame() const;
    QPushButton *manualCharacterCropButton() const;

    void clearBarcodeTemplateValidation();
    QString barcodeTemplateValidationFailureText(
        const BarcodeReadResult &barcode) const;
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

    void showManualCharacterTemplateCropDialog();
    void selectPublishedRecipeForCurrentMode();
    void saveCurrentTemplate();
    void showStampCharacterTemplateCropDialog();
    void showPublishedRecipeCharacterTemplateCropDialog(int profileIndex);
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

    TemplateModeMemory &modeMemory();
    const TemplateModeMemory &modeMemory() const;
    std::vector<WordTemplateProfile> &wordTemplateProfiles();
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
    void applyRecipeProfileToUi(
        const RecipeProfile &settings);
    void resetTemplateCaptureState();
    void editActiveRecipeCharacterAssets(int profileIndex);
    void publishCurrentRecipeSession();
    bool activatePublishedTissueRecipe(
        const QString &recipeId,
        bool showErrorMessage,
        QString *errorMessage);

    Widget *m_host = nullptr;
    Ui::Widget *ui = nullptr;
    QPointer<ImageLabel> imageLabel;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
    InspectionApplicationService *m_inspectionService = nullptr;
    MachineSettingsPageController *m_settingsPageController = nullptr;

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
    TemplateModeMemory m_templateModeMemory;
    RecipeEditorSession m_recipeEditorSession;
    PreparedRecipeSnapshot m_activePreparedRecipe;
    QString m_templateTargetLabelText;
    QString m_templateThresholdLabelText;

    std::vector<WordTemplateProfile> m_wordTemplateProfiles;
    QString m_currentTemplateDisplayName;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_validatedBarcodeText;
};
