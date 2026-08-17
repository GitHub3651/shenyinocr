#include "recipes/recipe_editor_session.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool validSessionHeader(const ProductRecipe &recipe,
                        QString *errorMessage)
{
    const QUuid id(recipe.recipeId);
    if (id.isNull()
            || id.toString(QUuid::WithoutBraces)
               != recipe.recipeId.toLower()
            || recipe.displayName.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Recipe editor identity is invalid."));
        return false;
    }
    return true;
}

} // namespace

RecipeEditorSession::RecipeEditorSession(
    const QString &editorWorkspacesRootPath)
    : m_workspacesRoot(editorWorkspacesRootPath.trimmed().isEmpty()
                       ? QString()
                       : QDir(editorWorkspacesRootPath).absolutePath())
{
}

RecipeEditorSession::~RecipeEditorSession()
{
    reset();
}

void RecipeEditorSession::reset()
{
    const QString activeWorkspace = workspacePath();
    if (!activeWorkspace.isEmpty()) {
        const QFileInfo workspaceInfo(activeWorkspace);
        const QString expectedWorkspace =
                QDir(m_workspacesRoot).filePath(m_sessionId);
        if (workspaceInfo.absoluteFilePath()
                == QFileInfo(expectedWorkspace).absoluteFilePath()
                && workspaceInfo.exists()
                && !workspaceInfo.isSymLink()
                && workspaceInfo.isDir()) {
            QDir(activeWorkspace).removeRecursively();
        }
    }
    m_sessionId.clear();
    m_state = RecipeEditorSessionState::Inactive;
    m_recipe = ProductRecipe();
    m_assetSourcePaths.clear();
}

bool RecipeEditorSession::beginNew(const ProductRecipe &recipe,
                                   QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!validSessionHeader(recipe, errorMessage)) {
        return false;
    }
    const QString newSessionId = QUuid::createUuid()
            .toString(QUuid::WithoutBraces);
    if (!m_workspacesRoot.isEmpty()
            && !QDir().mkpath(QDir(m_workspacesRoot)
                              .filePath(newSessionId))) {
        setError(errorMessage,
                 QStringLiteral("Unable to create recipe editor workspace."));
        return false;
    }
    reset();
    m_sessionId = newSessionId;
    m_state = RecipeEditorSessionState::New;
    m_recipe = recipe;
    m_assetSourcePaths.clear();
    return true;
}

bool RecipeEditorSession::beginEdit(const RecipeStore &store,
                                    const QString &recipeId,
                                    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    ProductRecipe loaded;
    if (!store.loadRecipe(recipeId, &loaded, errorMessage)) {
        return false;
    }
    const QString newSessionId = QUuid::createUuid()
            .toString(QUuid::WithoutBraces);
    if (!m_workspacesRoot.isEmpty()
            && !QDir().mkpath(QDir(m_workspacesRoot)
                              .filePath(newSessionId))) {
        setError(errorMessage,
                 QStringLiteral("Unable to create recipe editor workspace."));
        return false;
    }
    reset();
    QMap<QString, QString> sources;
    const QDir recipeDirectory(store.recipeDirectoryPath(loaded.recipeId));
    for (auto it = loaded.assets.constBegin();
         it != loaded.assets.constEnd(); ++it) {
        sources.insert(it.key(), recipeDirectory.filePath(it.value()));
    }
    m_sessionId = newSessionId;
    m_state = RecipeEditorSessionState::Editing;
    m_recipe = loaded;
    m_assetSourcePaths = sources;
    return true;
}

bool RecipeEditorSession::isActive() const
{
    return m_state != RecipeEditorSessionState::Inactive;
}

RecipeEditorSessionState RecipeEditorSession::state() const
{
    return m_state;
}

QString RecipeEditorSession::sessionId() const
{
    return m_sessionId;
}

QString RecipeEditorSession::workspacePath() const
{
    return m_workspacesRoot.isEmpty() || m_sessionId.isEmpty()
            ? QString()
            : QDir(m_workspacesRoot).filePath(m_sessionId);
}

const ProductRecipe &RecipeEditorSession::recipe() const
{
    return m_recipe;
}

QMap<QString, QString> RecipeEditorSession::assetSourcePaths() const
{
    return m_assetSourcePaths;
}

bool RecipeEditorSession::replaceDraft(
        const ProductRecipe &recipe,
        const QMap<QString, QString> &assetSourcePaths,
        QString *errorMessage)
{
    if (!isActive()) {
        setError(errorMessage,
                 QStringLiteral("Recipe editor session is inactive."));
        return false;
    }
    if (recipe.recipeId != m_recipe.recipeId
            || recipe.detectionMode != m_recipe.detectionMode) {
        setError(errorMessage,
                 QStringLiteral("Recipe identity or mode cannot change in a session."));
        return false;
    }
    if (assetSourcePaths.size() != recipe.assets.size()) {
        setError(errorMessage,
                 QStringLiteral("Recipe editor asset sources are incomplete."));
        return false;
    }
    if (!validateProductRecipe(recipe, errorMessage)) {
        return false;
    }
    m_recipe = recipe;
    m_assetSourcePaths = assetSourcePaths;
    return true;
}

bool RecipeEditorSession::updateProfile(int profileIndex,
                                        const RecipeProfile &profile,
                                        QString *errorMessage)
{
    if (!isActive() || profileIndex < 0
            || profileIndex >= m_recipe.profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Recipe editor profile index is invalid."));
        return false;
    }
    ProductRecipe candidate = m_recipe;
    candidate.profiles[profileIndex] = profile;
    if (!validateProductRecipe(candidate, errorMessage)) {
        return false;
    }
    m_recipe = candidate;
    return true;
}

bool RecipeEditorSession::publish(
        const RecipeStore &store,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage)
{
    if (!isActive() || !preparedRecipe) {
        setError(errorMessage,
                 QStringLiteral("Recipe editor publish inputs are invalid."));
        return false;
    }
    if (!store.saveRecipe(m_recipe, m_assetSourcePaths, errorMessage)) {
        return false;
    }
    PreparedRecipeSnapshot prepared;
    if (!store.loadPreparedRecipe(m_recipe.recipeId,
                                  &prepared, errorMessage)) {
        return false;
    }
    m_recipe = *prepared->recipe;
    m_state = RecipeEditorSessionState::Editing;
    m_assetSourcePaths.clear();
    const QDir recipeDirectory(store.recipeDirectoryPath(m_recipe.recipeId));
    for (auto it = m_recipe.assets.constBegin();
         it != m_recipe.assets.constEnd(); ++it) {
        m_assetSourcePaths.insert(it.key(),
                                  recipeDirectory.filePath(it.value()));
    }
    *preparedRecipe = prepared;
    return true;
}
