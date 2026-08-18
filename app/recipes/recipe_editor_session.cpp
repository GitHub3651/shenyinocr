// 文件作用：本文件用于维护模板编辑事务、草稿状态和提交取消边界。
// 主要职责：维护模板编辑事务、草稿状态和提交取消边界。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "recipes/recipe_editor_session.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace {

// 函数说明：setError 函数更新或应用对应的配置和状态。
void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

// 函数说明：validSessionHeader 函数实现名称所表示的处理步骤。
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

// 函数说明：RecipeEditorSession 构造函数创建组件并初始化其依赖和初始状态。
RecipeEditorSession::RecipeEditorSession(
    const QString &editorWorkspacesRootPath)
    : m_workspacesRoot(editorWorkspacesRootPath.trimmed().isEmpty()
                       ? QString()
                       : QDir(editorWorkspacesRootPath).absolutePath())
{
}

// 函数说明：~RecipeEditorSession 析构函数按生命周期要求释放组件持有的资源。
RecipeEditorSession::~RecipeEditorSession()
{
    reset();
}

// 函数说明：reset 函数停止流程、清理状态或释放对应资源。
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

// 函数说明：beginNew 函数创建、准备或启动对应流程。
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

// 函数说明：beginEdit 函数创建、准备或启动对应流程。
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

// 函数说明：isActive 函数检查相关状态并返回判断结果。
bool RecipeEditorSession::isActive() const
{
    return m_state != RecipeEditorSessionState::Inactive;
}

// 函数说明：state 函数实现名称所表示的处理步骤。
RecipeEditorSessionState RecipeEditorSession::state() const
{
    return m_state;
}

// 函数说明：sessionId 函数实现名称所表示的处理步骤。
QString RecipeEditorSession::sessionId() const
{
    return m_sessionId;
}

// 函数说明：workspacePath 函数实现名称所表示的处理步骤。
QString RecipeEditorSession::workspacePath() const
{
    return m_workspacesRoot.isEmpty() || m_sessionId.isEmpty()
            ? QString()
            : QDir(m_workspacesRoot).filePath(m_sessionId);
}

// 函数说明：recipe 函数实现名称所表示的处理步骤。
const ProductRecipe &RecipeEditorSession::recipe() const
{
    return m_recipe;
}

QMap<QString, QString> RecipeEditorSession::assetSourcePaths() const
{
    return m_assetSourcePaths;
}

// 函数说明：replaceDraft 函数更新或应用对应的配置和状态。
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

// 函数说明：updateProfile 函数更新或应用对应的配置和状态。
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

// 函数说明：publish 函数保存或发布对应的数据和资源。
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
