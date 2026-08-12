#include "template_recipe_edit_session.h"

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

void TemplateRecipeEditSession::reset()
{
    m_isActive = false;
    m_assembly = TemplateRecipeAssembly();
}

bool TemplateRecipeEditSession::begin(const RecipeSelection &selection,
                                      QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    TemplateRecipeAssembly candidate;
    if (!assembleSelectedTemplateRecipe(selection,
                                        &candidate,
                                        errorMessage)) {
        return false;
    }

    m_assembly = candidate;
    m_isActive = true;
    return true;
}

bool TemplateRecipeEditSession::isActive() const
{
    return m_isActive;
}

const ProductRecipe &TemplateRecipeEditSession::recipe() const
{
    return m_assembly.recipe;
}

bool TemplateRecipeEditSession::updateProfile(
        int profileIndex,
        const RecipeProfile &profile,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_isActive) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit session is not active."));
        return false;
    }
    if (profileIndex < 0
            || profileIndex >= m_assembly.recipe.profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit profile index is invalid."));
        return false;
    }

    const RecipeProfile &currentProfile =
            m_assembly.recipe.profiles.at(profileIndex);
    if (profile.name != currentProfile.name
            || profile.assetKeys != currentProfile.assetKeys) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit cannot change profile identity or asset bindings."));
        return false;
    }

    ProductRecipe candidate = m_assembly.recipe;
    candidate.profiles[profileIndex] = profile;
    if (!validateProductRecipe(candidate, errorMessage)) {
        return false;
    }

    m_assembly.recipe = candidate;
    return true;
}

bool TemplateRecipeEditSession::updateProfiles(
        const QVector<RecipeProfile> &profiles,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_isActive) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit session is not active."));
        return false;
    }
    if (profiles.size() != m_assembly.recipe.profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit profile count does not match the current recipe."));
        return false;
    }

    for (int profileIndex = 0; profileIndex < profiles.size(); ++profileIndex) {
        const RecipeProfile &currentProfile =
                m_assembly.recipe.profiles.at(profileIndex);
        const RecipeProfile &updatedProfile = profiles.at(profileIndex);
        if (updatedProfile.name != currentProfile.name
                || updatedProfile.assetKeys != currentProfile.assetKeys) {
            setError(errorMessage,
                     QStringLiteral("Template recipe edit cannot change profile identity or asset bindings at index %1.")
                     .arg(profileIndex));
            return false;
        }
    }

    ProductRecipe candidate = m_assembly.recipe;
    candidate.profiles = profiles;
    if (!validateProductRecipe(candidate, errorMessage)) {
        return false;
    }

    m_assembly.recipe = candidate;
    return true;
}

bool TemplateRecipeEditSession::publish(
        const RecipeStore &store,
        RecipeSelection *publishedSelection,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_isActive) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit session is not active."));
        return false;
    }
    if (!publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Template recipe edit published selection output is null."));
        return false;
    }

    if (!store.saveRecipe(m_assembly.recipe,
                          m_assembly.assetSourcePaths,
                          errorMessage)) {
        return false;
    }

    RecipeSelection candidateSelection;
    if (!loadRecipeSelection(store,
                             m_assembly.recipe.recipeId,
                             m_assembly.recipe.detectionMode,
                             &candidateSelection,
                             errorMessage)) {
        return false;
    }

    TemplateRecipeAssembly candidateAssembly;
    if (!assembleSelectedTemplateRecipe(candidateSelection,
                                        &candidateAssembly,
                                        errorMessage)) {
        return false;
    }

    m_assembly = candidateAssembly;
    *publishedSelection = candidateSelection;
    return true;
}
