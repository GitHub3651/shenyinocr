#pragma once

#include "application/template_editor_contract.h"
#include "application/template_geometry_service.h"
#include "recipes/prepared_recipe.h"
#include "recipes/recipe_asset_service.h"
#include "recipes/recipe_editor_session.h"
#include "recipes/template_mode_memory.h"

#include <QMap>
#include <QString>

#include <memory>
#include <vector>

class IBarcodeDecoder;
class RecipeStore;

// The only application boundary for the complete template editing use case.
// UI code supplies user intent and immutable images; this service owns the
// draft/session, recipe catalog access, asset staging and transactional publish.
class TemplateApplicationService
{
public:
    TemplateApplicationService(
        const std::shared_ptr<RecipeStore> &store,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
        const QString &editorWorkspacesRootPath);

    bool beginNew(const ProductRecipe &recipe,
                  QString *errorMessage = nullptr);
    bool beginEdit(const QString &recipeId,
                   QString *errorMessage = nullptr);
    void cancel();
    bool isActive() const;
    const ProductRecipe &draft() const;
    QString workspacePath() const;
    QMap<QString, QString> assetSourcePaths() const;
    bool replaceDraft(
        const ProductRecipe &recipe,
        const QMap<QString, QString> &assetSourcePaths,
        QString *errorMessage = nullptr);
    bool updateProfile(int profileIndex,
                       const RecipeProfile &profile,
                       QString *errorMessage = nullptr);
    bool publish(PreparedRecipeSnapshot *preparedRecipe,
                 QString *errorMessage = nullptr);

    bool listRecipes(TemplateRecipeCatalog *catalog,
                     QString *errorMessage = nullptr) const;
    bool loadPreparedRecipe(
        const QString &recipeId,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage = nullptr) const;

    bool stageInitialProfileAssets(
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage = nullptr) const;
    bool stageCharacterAssets(
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage = nullptr) const;

    QRect mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const;
    TemplateProfileGeometry buildProfileGeometry(
        const QRect &trackingDisplayRect,
        const QRect &barcodeDisplayRect,
        const QPolygon &dateDisplayPolygon,
        bool includeBarcode,
        const TemplateDisplayGeometry &geometry) const;
    bool validateBarcodeTemplate(
        const cv::Mat &sourceImage,
        const QRect &sourceRect,
        const TemplateBarcodeValidationOptions &options,
        TemplateBarcodeValidationResult *result,
        QString *failureReason) const;

    const TemplateModeMemory &modeMemory() const;
    void replacePublishedRecipeIdsByMode(
        const QMap<QString, QString> &recipeIds);
    void rememberPublishedRecipe(
        const QString &modeId,
        const QString &recipeId);
    void forgetPublishedRecipe(const QString &modeId);
    const PreparedRecipeSnapshot &activePreparedRecipe() const;
    void setActivePreparedRecipe(
        const PreparedRecipeSnapshot &preparedRecipe);
    const std::vector<WordTemplateProfile> &wordProfiles() const;
    void clearWordProfiles();
    void replaceWordProfiles(
        const std::vector<WordTemplateProfile> &profiles);
    bool replaceWordProfile(
        int profileIndex,
        const WordTemplateProfile &profile);
    void clearActiveRecipe();

private:
    std::shared_ptr<RecipeStore> m_store;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
    RecipeEditorSession m_session;
    RecipeAssetService m_assetService;
    TemplateGeometryService m_geometryService;
    TemplateModeMemory m_modeMemory;
    PreparedRecipeSnapshot m_activePreparedRecipe;
    std::vector<WordTemplateProfile> m_wordProfiles;
};
