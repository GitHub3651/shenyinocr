#pragma once

#include "BarcodeTypes.h"
#include "Detector.h"
#include "recipes/recipe_selection.h"
#include "recipes/template_mode_memory.h"
#include "recipes/template_recipe_draft_session.h"
#include "recipes/template_recipe_edit_session.h"
#include "recipes/template_runtime_profile.h"
#include "runtime/inspection_profile_snapshot.h"

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
class InspectionAcquisitionController;
class MachineSettingsPageController;
class QComboBox;
class QDir;
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
        InspectionAcquisitionController *acquisitionController,
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

    void setupTemplatePrivateSettingDirtyTracking();
    void refreshTemplateTargetTextDirty();
    void refreshTemplateImageThresholdDirty();
    void refreshTemplatePrivateSettingDirty();
    void markTemplateTargetTextDirty();
    void markTemplateImageThresholdDirty();
    void clearTemplateTargetTextDirty();
    void clearTemplateImageThresholdDirty();
    void clearTemplatePrivateSettingDirty();
    void updateTemplatePrivateSettingDirtyUi();

    void showManualCharacterTemplateCropDialog();
    void showStampCharacterTemplateCropDialog();
    void showPublishedRecipeCharacterTemplateCropDialog(int profileIndex);
    void setupWordTemplateEditorCombo();

    void clearWordMultiTemplateState();
    void clearSingleTemplateRecipeState();
    QString detectModeIdForIndex(int index) const;
    QString currentDetectModeId() const;
    QStringList currentTemplatePathsForMode(const QString &modeId) const;
    void storeCurrentTemplatePathsForMode(const QString &modeId);
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
    bool loadSingleTemplateCharacterAssets(
        const QMap<QString, QString> &assetPathsByRole,
        const QStringList &targetUnits,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    bool activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage);
    bool republishSingleTemplateRecipeSettings(
        const TemplatePrivateSettings &settings,
        QString *errorMessage);
    int currentWordTemplateProfileIndex() const;
    QString wordTemplateProfileAssetPath(
        const WordTemplateProfile &profile,
        const QString &role,
        const QString &legacyFileName) const;
    void displayWordTemplateRawImage(const WordTemplateProfile &profile);
    void displayWordTemplateRawImage(const QString &dirPath);
    void displayWordTemplateRawImageFile(
        const QString &rawImagePath,
        const QString &templateName);
    QStringList wordTemplateImagePathsForKey(
        const QDir &directory,
        const QString &searchKey,
        bool includeVariants = true) const;
    bool loadWordDigitTemplatesFromDir(
        const QString &dirPath,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage,
        bool includeVariants = true) const;
    bool loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    bool loadWordTemplateProfileFromDir(
        const QString &dirPath,
        WordTemplateProfile *profile,
        QString *errorMessage);
    bool loadWordTemplateProfileFromRecipeSelection(
        const RecipeSelection &selection,
        int profileIndex,
        WordTemplateProfile *profile,
        QString *errorMessage);
    bool loadWordTemplateProfilesFromRecipeSelection(
        const RecipeSelection &selection,
        std::vector<WordTemplateProfile> *profiles,
        QStringList *pendingMessages,
        QString *errorMessage);
    void refreshWordTemplateProfileDigitCache(
        WordTemplateProfile *profile) const;
    InspectionProfileSnapshot createWordTemplateRunSnapshot() const;
    void refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const;
    bool saveWordTemplatePrivateSettings(
        int profileIndex,
        const TemplatePrivateSettings &settings,
        QString *errorMessage);
    void refreshWordTemplateRecipeAssets();
    void prepareWordTemplateRecipeDraft(
        const WordTemplateProfile &profile);
    bool publishWordTemplateRecipeDraft(QString *errorMessage);
    bool publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage);
    bool publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage);

    void applyCurrentTargetText();
    void applyCurrentImageThreshold();
    void applyBatchTargetText();
    void applyBatchImageThreshold();

    TemplateModeMemory &modeMemory();
    const TemplateModeMemory &modeMemory() const;
    std::vector<WordTemplateProfile> &wordTemplateProfiles();
    const std::vector<WordTemplateProfile> &wordTemplateProfiles() const;
    TemplateRecipeDraftSession &wordDraftSession();
    TemplateRecipeEditSession &wordEditSession();
    TemplateRecipeEditSession &singleTemplateEditSession();
    QMap<QString, QString> &singleTemplateResolvedAssets();
    const QMap<QString, QString> &singleTemplateResolvedAssets() const;
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
    bool loadSettingsFromDir(const QString &dirPath,
                             bool showErrorMessage);
    void applyTemplatePrivateSettingsToUi(
        const TemplatePrivateSettings &settings);
    void resetTemplateCaptureState();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void initOverlapDetectorFromCurrentDir();

    Widget *m_host = nullptr;
    Ui::Widget *ui = nullptr;
    QPointer<ImageLabel> imageLabel;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
    InspectionAcquisitionController *m_acquisitionController = nullptr;
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
    QString m_templateTargetLabelText;
    QString m_templateThresholdLabelText;

    std::vector<WordTemplateProfile> m_wordTemplateProfiles;
    TemplateRecipeDraftSession m_wordTemplateRecipeDraftSession;
    TemplateRecipeEditSession m_wordTemplateRecipeEditSession;
    TemplateRecipeEditSession m_singleTemplateRecipeEditSession;
    QMap<QString, QString> m_singleTemplateResolvedAssetPathsByRole;
    QString m_currentTemplateDisplayName;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_validatedBarcodeText;
};
