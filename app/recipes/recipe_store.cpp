// 文件作用：本文件用于负责产品配方目录的校验、加载、事务保存和已发布配方枚举。
// 主要职责：负责产品配方目录的校验、加载、事务保存和已发布配方枚举。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "recipe_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

// 函数说明：setError 函数更新或应用对应的配置和状态。
void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message.startsWith(QLatin1String("RECIPE_"))
                ? message
                : QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: %1")
                  .arg(message);
    }
}

// 函数说明：isCanonicalRecipeId 函数检查相关状态并返回判断结果。
bool isCanonicalRecipeId(const QString &recipeId)
{
    const QString trimmed = recipeId.trimmed();
    const QUuid uuid(trimmed);
    return !uuid.isNull()
            && uuid.toString(QUuid::WithoutBraces) == trimmed.toLower();
}

// 函数说明：removeDirectoryIfExists 函数停止流程、清理状态或释放对应资源。
bool removeDirectoryIfExists(const QString &directoryPath,
                             QString *errorMessage)
{
    const QFileInfo info(directoryPath);
    if (!info.exists()) {
        return true;
    }
    if (info.isSymLink() || !info.isDir()) {
        setError(errorMessage,
                 QStringLiteral("Recipe work path is not a regular directory: %1")
                 .arg(directoryPath));
        return false;
    }
    QDir directory(directoryPath);
    if (!directory.removeRecursively()) {
        setError(errorMessage,
                 QStringLiteral("Unable to remove recipe work directory: %1")
                 .arg(directoryPath));
        return false;
    }
    return true;
}

// 函数说明：ensureRecipesRoot 函数实现名称所表示的处理步骤。
bool ensureRecipesRoot(const QString &rootPath, QString *errorMessage)
{
    if (rootPath.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root path is empty."));
        return false;
    }

    const QFileInfo rootInfo(rootPath);
    if (rootInfo.exists() && (rootInfo.isSymLink() || !rootInfo.isDir())) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root is not a regular directory: %1")
                 .arg(rootPath));
        return false;
    }
    if (!rootInfo.exists() && !QDir().mkpath(rootPath)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: unable to create recipe root directory: %1")
                 .arg(rootPath));
        return false;
    }
    return true;
}

// 函数说明：writeRecipeJson 函数保存或发布对应的数据和资源。
bool writeRecipeJson(const QString &directoryPath,
                     const ProductRecipe &recipe,
                     QString *errorMessage)
{
    const QString jsonPath = QDir(directoryPath).filePath(
                QStringLiteral("recipe.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_VERIFY_FAILED: unable to write recipe.json: %1")
                 .arg(jsonPath));
        return false;
    }

    const QByteArray bytes = QJsonDocument(productRecipeToJson(recipe))
            .toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_VERIFY_FAILED: unable to complete recipe.json: %1")
                 .arg(jsonPath));
        file.close();
        return false;
    }
    file.close();
    return true;
}

// 函数说明：copyRecipeAssets 函数实现名称所表示的处理步骤。
bool copyRecipeAssets(const QString &directoryPath,
                      const ProductRecipe &recipe,
                      const QMap<QString, QString> &assetSourcePaths,
                      QString *errorMessage)
{
    if (assetSourcePaths.size() != recipe.assets.size()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_MISSING: asset source keys do not match recipe assets."));
        return false;
    }

    QSet<QString> destinationPaths;
    for (auto it = recipe.assets.constBegin(); it != recipe.assets.constEnd(); ++it) {
        if (!assetSourcePaths.contains(it.key())) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_ASSET_MISSING: missing asset source for key: %1")
                     .arg(it.key()));
            return false;
        }

        const QString relativePath = QDir::cleanPath(
                    QDir::fromNativeSeparators(it.value()));
        const QString collisionKey = relativePath.toLower();
        if (destinationPaths.contains(collisionKey)) {
            setError(errorMessage,
                     QStringLiteral("Duplicate recipe asset destination: %1")
                     .arg(relativePath));
            return false;
        }
        destinationPaths.insert(collisionKey);

        const QString sourcePath = assetSourcePaths.value(it.key());
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists()
                || sourceInfo.isSymLink()
                || !sourceInfo.isFile()
                || sourceInfo.size() <= 0) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_ASSET_MISSING: recipe asset source is missing or invalid: %1")
                     .arg(sourcePath));
            return false;
        }

        const QString destinationPath = QDir(directoryPath).filePath(relativePath);
        const QString destinationParent = QFileInfo(destinationPath).absolutePath();
        if (!QDir().mkpath(destinationParent)
                || !QFile::copy(sourceInfo.absoluteFilePath(), destinationPath)) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_VERIFY_FAILED: unable to copy recipe asset %1 to %2")
                     .arg(sourceInfo.absoluteFilePath(), destinationPath));
            return false;
        }
    }
    return true;
}

} // namespace

// 函数说明：RecipeStore 构造函数创建组件并初始化其依赖和初始状态。
RecipeStore::RecipeStore(const QString &recipesRootPath,
                         const DirectoryRenameFunction &directoryRenameFunction)
    : m_recipesRootPath(recipesRootPath.trimmed().isEmpty()
                        ? QString()
                        : QDir(recipesRootPath).absolutePath()),
      m_directoryRenameFunction(directoryRenameFunction)
{
}

// 函数说明：recipesRootPath 函数实现名称所表示的处理步骤。
QString RecipeStore::recipesRootPath() const
{
    return m_recipesRootPath;
}

// 函数说明：recipeDirectoryPath 函数实现名称所表示的处理步骤。
QString RecipeStore::recipeDirectoryPath(const QString &recipeId) const
{
    if (m_recipesRootPath.isEmpty() || !isCanonicalRecipeId(recipeId)) {
        return QString();
    }
    return QDir(m_recipesRootPath).filePath(recipeId.trimmed().toLower());
}

// 函数说明：loadRecipe 函数读取、等待或计算对应的数据。
bool RecipeStore::loadRecipe(const QString &recipeId,
                             ProductRecipe *recipe,
                             QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!recipe) {
        setError(errorMessage, QStringLiteral("ProductRecipe output is null."));
        return false;
    }
    if (m_recipesRootPath.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root path is empty."));
        return false;
    }

    const QString directoryPath = recipeDirectoryPath(recipeId);
    if (directoryPath.isEmpty()) {
        const QString candidate = recipeId.trimmed();
        const bool simpleDirectoryName = !candidate.isEmpty()
                && candidate != QLatin1String(".")
                && candidate != QLatin1String("..")
                && !candidate.contains(QLatin1Char(':'))
                && QFileInfo(candidate).fileName() == candidate;
        const bool legacyDirectoryExists = simpleDirectoryName
                && QFileInfo(QDir(m_recipesRootPath)
                             .filePath(candidate)).isDir();
        setError(errorMessage,
                 legacyDirectoryExists
                 ? QStringLiteral(
                     "RECIPE_LEGACY_FORMAT_REJECTED: legacy recipe directories are not supported.")
                 : QStringLiteral("RECIPE_ID_INVALID: recipe ID is invalid."));
        return false;
    }
    return loadRecipeFromDirectory(directoryPath,
                                   recipeId.trimmed().toLower(),
                                   recipe,
                                   errorMessage);
}

// 函数说明：loadPreparedRecipe 函数读取、等待或计算对应的数据。
bool RecipeStore::loadPreparedRecipe(
        const QString &recipeId,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!preparedRecipe) {
        setError(errorMessage,
                 QStringLiteral("PreparedRecipe output is null."));
        return false;
    }
    ProductRecipe recipe;
    if (!loadRecipe(recipeId, &recipe, errorMessage)) {
        return false;
    }
    return prepareRecipe(recipe,
                         recipeDirectoryPath(recipe.recipeId),
                         preparedRecipe,
                         errorMessage);
}

// 函数说明：listRecipes 函数实现名称所表示的处理步骤。
bool RecipeStore::listRecipes(RecipeCatalog *catalog,
                              QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!catalog) {
        setError(errorMessage, QStringLiteral("RecipeCatalog output is null."));
        return false;
    }
    if (m_recipesRootPath.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root path is empty."));
        return false;
    }

    const QFileInfo rootInfo(m_recipesRootPath);
    if (rootInfo.isSymLink()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root is not a readable regular directory: %1")
                 .arg(m_recipesRootPath));
        return false;
    }
    if (!rootInfo.exists()) {
        *catalog = RecipeCatalog();
        return true;
    }
    if (!rootInfo.isDir() || !rootInfo.isReadable()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ROOT_UNAVAILABLE: recipe root is not a readable regular directory: %1")
                 .arg(m_recipesRootPath));
        return false;
    }

    RecipeCatalog candidate;
    const QFileInfoList directories = QDir(m_recipesRootPath).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot,
                QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &directoryInfo : directories) {
        const QString directoryName = directoryInfo.fileName();
        if (directoryName != directoryName.toLower()
                || !isCanonicalRecipeId(directoryName)) {
            continue;
        }

        ProductRecipe recipe;
        QString loadError;
        if (!loadRecipeFromDirectory(directoryInfo.absoluteFilePath(),
                                     directoryName.toLower(),
                                     &recipe,
                                     &loadError)) {
            RecipeCatalogIssue issue;
            issue.directoryName = directoryName;
            issue.message = loadError;
            candidate.invalidRecipes.append(issue);
            continue;
        }

        RecipeCatalogEntry entry;
        entry.recipeId = recipe.recipeId;
        entry.displayName = recipe.displayName;
        entry.detectionMode = recipe.detectionMode;
        entry.profileCount = recipe.profiles.size();
        candidate.recipes.append(entry);
    }

    std::sort(candidate.recipes.begin(),
              candidate.recipes.end(),
              [](const RecipeCatalogEntry &left,
                 const RecipeCatalogEntry &right) {
        const int nameComparison = QString::compare(left.displayName,
                                                    right.displayName,
                                                    Qt::CaseInsensitive);
        if (nameComparison != 0) {
            return nameComparison < 0;
        }
        return left.recipeId < right.recipeId;
    });

    *catalog = candidate;
    return true;
}

// 函数说明：saveRecipe 函数保存或发布对应的数据和资源。
bool RecipeStore::saveRecipe(
        const ProductRecipe &recipe,
        const QMap<QString, QString> &assetSourcePaths,
        QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!validateProductRecipe(recipe, errorMessage)
            || !ensureRecipesRoot(m_recipesRootPath, errorMessage)) {
        return false;
    }

    const QString targetPath = recipeDirectoryPath(recipe.recipeId);
    if (targetPath.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ID_INVALID: recipe target path is invalid."));
        return false;
    }
    const QFileInfo targetInfo(targetPath);
    if (targetInfo.exists() && (targetInfo.isSymLink() || !targetInfo.isDir())) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_DIRECTORY_MISMATCH: recipe target is not a regular directory: %1")
                 .arg(targetPath));
        return false;
    }

    const QString transactionId = QUuid::createUuid().toString(
                QUuid::WithoutBraces);
    const QString targetName = QFileInfo(targetPath).fileName();
    const QString tempName = targetName + QStringLiteral(".tmp.") + transactionId;
    const QString backupName = targetName + QStringLiteral(".bak.") + transactionId;
    const QDir rootDirectory(m_recipesRootPath);
    const QString tempPath = rootDirectory.filePath(tempName);
    const QString backupPath = rootDirectory.filePath(backupName);

    if (!removeDirectoryIfExists(tempPath, errorMessage)
            || !removeDirectoryIfExists(backupPath, errorMessage)
            || !QDir().mkpath(tempPath)) {
        if (errorMessage && errorMessage->isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_VERIFY_FAILED: unable to create recipe transaction directory: %1")
                     .arg(tempPath));
        }
        return false;
    }

    const QString assetsDirectoryPath = QDir(tempPath).filePath(
                QStringLiteral("assets"));
    bool prepared = QDir().mkpath(assetsDirectoryPath);
    if (!prepared) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_VERIFY_FAILED: unable to create recipe assets directory: %1")
                 .arg(assetsDirectoryPath));
    }
    prepared = prepared
            && writeRecipeJson(tempPath, recipe, errorMessage)
            && copyRecipeAssets(tempPath,
                                recipe,
                                assetSourcePaths,
                                errorMessage);
    ProductRecipe reloadedRecipe;
    if (prepared) {
        prepared = loadRecipeFromDirectory(tempPath,
                                           recipe.recipeId,
                                           &reloadedRecipe,
                                           errorMessage)
                && productRecipeToJson(reloadedRecipe)
                == productRecipeToJson(recipe);
        if (!prepared && errorMessage && errorMessage->isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_VERIFY_FAILED: reloaded recipe does not match the saved recipe."));
        }
    }
    if (!prepared) {
        QString cleanupError;
        removeDirectoryIfExists(tempPath, &cleanupError);
        return false;
    }

    const bool targetExisted = targetInfo.exists();
    if (targetExisted && !renameDirectory(targetPath, backupPath)) {
        QString cleanupError;
        removeDirectoryIfExists(tempPath, &cleanupError);
        setError(errorMessage,
                 QStringLiteral(
                     "RECIPE_BACKUP_FAILED: unable to back up existing recipe directory: %1.\n"
                     "\u8BF7\u5173\u95ED\u6B63\u5728\u6D4F\u89C8\u8BE5\u914D\u65B9"
                     "\u76EE\u5F55\u6216\u5176\u5B50\u76EE\u5F55\u7684\u6587\u4EF6"
                     "\u8D44\u6E90\u7BA1\u7406\u5668\u7A97\u53E3\u53CA\u5176\u4ED6"
                     "\u5360\u7528\u7A0B\u5E8F\uFF0C\u7136\u540E\u91CD\u8BD5\u3002")
                 .arg(targetPath));
        return false;
    }

    if (!renameDirectory(tempPath, targetPath)) {
        const bool restored = !targetExisted
                || renameDirectory(backupPath, targetPath);
        QString cleanupError;
        removeDirectoryIfExists(tempPath, &cleanupError);
        setError(errorMessage,
                 restored
                 ? QStringLiteral("RECIPE_COMMIT_FAILED: unable to commit recipe; the previous recipe was restored.")
                 : QStringLiteral("RECIPE_ROLLBACK_FAILED: unable to commit recipe and restore the previous recipe."));
        return false;
    }

    if (targetExisted) {
        QString cleanupError;
        removeDirectoryIfExists(backupPath, &cleanupError);
    }
    return true;
}

// 函数说明：renameDirectory 函数实现名称所表示的处理步骤。
bool RecipeStore::renameDirectory(
        const QString &sourceDirectoryPath,
        const QString &destinationDirectoryPath) const
{
    if (m_directoryRenameFunction) {
        return m_directoryRenameFunction(sourceDirectoryPath,
                                         destinationDirectoryPath);
    }
    return QDir().rename(sourceDirectoryPath, destinationDirectoryPath);
}

// 函数说明：loadRecipeFromDirectory 函数读取、等待或计算对应的数据。
bool RecipeStore::loadRecipeFromDirectory(
        const QString &directoryPath,
        const QString &expectedRecipeId,
        ProductRecipe *recipe,
        QString *errorMessage) const
{
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.exists()
            || directoryInfo.isSymLink()
            || !directoryInfo.isDir()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_JSON_MISSING: recipe directory is missing or invalid: %1")
                 .arg(directoryPath));
        return false;
    }

    const QString jsonPath = QDir(directoryPath).filePath(
                QStringLiteral("recipe.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_JSON_MISSING: unable to read recipe.json: %1")
                 .arg(jsonPath));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(),
                                                            &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_JSON_MALFORMED: recipe.json is invalid: %1")
                 .arg(parseError.errorString()));
        return false;
    }

    ProductRecipe candidate;
    if (!productRecipeFromJson(document.object(), &candidate, errorMessage)) {
        return false;
    }
    if (!expectedRecipeId.isEmpty()
            && candidate.recipeId != expectedRecipeId.trimmed().toLower()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_DIRECTORY_MISMATCH: recipe ID does not match the requested directory."));
        return false;
    }

    for (auto it = candidate.assets.constBegin(); it != candidate.assets.constEnd(); ++it) {
        const QString absoluteAssetPath = QDir(directoryPath).filePath(
                    QDir::cleanPath(QDir::fromNativeSeparators(it.value())));
        const QFileInfo assetInfo(absoluteAssetPath);
        if (!assetInfo.exists()
                || assetInfo.isSymLink()
                || !assetInfo.isFile()
                || assetInfo.size() <= 0) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_ASSET_MISSING: recipe asset is missing or invalid: %1")
                     .arg(it.value()));
            return false;
        }
    }

    PreparedRecipeSnapshot prepared;
    if (!prepareRecipe(candidate, directoryPath,
                       &prepared, errorMessage)) {
        return false;
    }

    *recipe = candidate;
    return true;
}
