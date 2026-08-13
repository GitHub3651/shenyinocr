#include "template_recipe_workflow.h"

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

void setStage(TemplateRecipeWorkflowFailureStage *failureStage,
              TemplateRecipeWorkflowFailureStage stage)
{
    if (failureStage) {
        *failureStage = stage;
    }
}

} // namespace

bool TemplateRecipeWorkflow::validateProfileUpdate(
        const TemplateRecipeEditSession &editSession,
        int profileIndex,
        const RecipeProfile &profile,
        QString *errorMessage)
{
    TemplateRecipeEditSession candidateSession = editSession;
    return candidateSession.updateProfile(profileIndex,
                                          profile,
                                          errorMessage);
}

bool TemplateRecipeWorkflow::publishDraftAndBeginEdit(
        TemplateRecipeDraftSession *draftSession,
        TemplateRecipeEditSession *editSession,
        const RecipeStore &store,
        const QString &sourceDirectoryPath,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        RecipeSelection *publishedSelection,
        TemplateRecipeWorkflowFailureStage *failureStage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    setStage(failureStage, TemplateRecipeWorkflowFailureStage::None);
    if (!draftSession || !editSession || !publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Template recipe workflow draft inputs are invalid."));
        return false;
    }

    TemplateRecipeDraftSession candidateDraftSession = *draftSession;
    RecipeSelection candidateSelection;
    if (!candidateDraftSession.publish(store,
                                       sourceDirectoryPath,
                                       profileSources,
                                       &candidateSelection,
                                       errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::Publish);
        return false;
    }

    TemplateRecipeEditSession candidateEditSession;
    if (!candidateEditSession.begin(candidateSelection,
                                    errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::EditSession);
        return false;
    }

    *draftSession = candidateDraftSession;
    *editSession = candidateEditSession;
    *publishedSelection = candidateSelection;
    return true;
}

bool TemplateRecipeWorkflow::republishProfiles(
        TemplateRecipeEditSession *editSession,
        const RecipeStore &store,
        const QVector<RecipeProfile> &profiles,
        RecipeSelection *publishedSelection,
        TemplateRecipeWorkflowFailureStage *failureStage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    setStage(failureStage, TemplateRecipeWorkflowFailureStage::None);
    if (!editSession || !publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Template recipe workflow profile inputs are invalid."));
        return false;
    }

    TemplateRecipeEditSession candidateSession = *editSession;
    if (!candidateSession.updateProfiles(profiles, errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::Validation);
        return false;
    }

    RecipeSelection candidateSelection;
    if (!candidateSession.publish(store,
                                  &candidateSelection,
                                  errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::Publish);
        return false;
    }

    *editSession = candidateSession;
    *publishedSelection = candidateSelection;
    return true;
}

bool TemplateRecipeWorkflow::republishProfileAssets(
        TemplateRecipeEditSession *editSession,
        const RecipeStore &store,
        int profileIndex,
        const RecipeProfile &profile,
        const TemplateProfileAssetManifest &assetManifest,
        RecipeSelection *publishedSelection,
        TemplateRecipeWorkflowFailureStage *failureStage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    setStage(failureStage, TemplateRecipeWorkflowFailureStage::None);
    if (!editSession || !publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Template recipe workflow asset inputs are invalid."));
        return false;
    }

    TemplateRecipeEditSession candidateSession = *editSession;
    if (!candidateSession.replaceProfileAssets(profileIndex,
                                               profile,
                                               assetManifest,
                                               errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::Validation);
        return false;
    }

    RecipeSelection candidateSelection;
    if (!candidateSession.publish(store,
                                  &candidateSelection,
                                  errorMessage)) {
        setStage(failureStage,
                 TemplateRecipeWorkflowFailureStage::Publish);
        return false;
    }

    *editSession = candidateSession;
    *publishedSelection = candidateSelection;
    return true;
}
