#ifndef RECIPES_TEMPLATE_RECIPE_WORKFLOW_H
#define RECIPES_TEMPLATE_RECIPE_WORKFLOW_H

#include "template_recipe_draft_session.h"
#include "template_recipe_edit_session.h"

enum class TemplateRecipeWorkflowFailureStage
{
    None,
    Validation,
    Publish,
    EditSession
};

class TemplateRecipeWorkflow
{
public:
    static bool validateProfileUpdate(
            const TemplateRecipeEditSession &editSession,
            int profileIndex,
            const RecipeProfile &profile,
            QString *errorMessage = nullptr);

    static bool publishDraftAndBeginEdit(
            TemplateRecipeDraftSession *draftSession,
            TemplateRecipeEditSession *editSession,
            const RecipeStore &store,
            const QString &sourceDirectoryPath,
            const QVector<TemplateRecipeProfileSource> &profileSources,
            RecipeSelection *publishedSelection,
            TemplateRecipeWorkflowFailureStage *failureStage = nullptr,
            QString *errorMessage = nullptr);

    static bool republishProfiles(
            TemplateRecipeEditSession *editSession,
            const RecipeStore &store,
            const QVector<RecipeProfile> &profiles,
            RecipeSelection *publishedSelection,
            TemplateRecipeWorkflowFailureStage *failureStage = nullptr,
            QString *errorMessage = nullptr);

    static bool republishProfileAssets(
            TemplateRecipeEditSession *editSession,
            const RecipeStore &store,
            int profileIndex,
            const RecipeProfile &profile,
            const TemplateProfileAssetManifest &assetManifest,
            RecipeSelection *publishedSelection,
            TemplateRecipeWorkflowFailureStage *failureStage = nullptr,
            QString *errorMessage = nullptr);
};

#endif // RECIPES_TEMPLATE_RECIPE_WORKFLOW_H
