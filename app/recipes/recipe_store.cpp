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

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool isCanonicalRecipeId(const QString &recipeId)
{
    const QString trimmed = recipeId.trimmed();
    const QUuid uuid(trimmed);
    return !uuid.isNull()
            && uuid.toString(QUuid::WithoutBraces) == trimmed.toLower();
}

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

bool ensureRecipesRoot(const QString &rootPath, QString *errorMessage)
{
    if (rootPath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Recipe root path is empty."));
        return false;
    }

    const QFileInfo rootInfo(rootPath);
    if (rootInfo.exists() && (rootInfo.isSymLink() || !rootInfo.isDir())) {
        setError(errorMessage,
                 QStringLiteral("Recipe root is not a regular directory: %1")
                 .arg(rootPath));
        return false;
    }
    if (!rootInfo.exists() && !QDir().mkpath(rootPath)) {
        setError(errorMessage,
                 QStringLiteral("Unable to create recipe root directory: %1")
                 .arg(rootPath));
        return false;
    }
    return true;
}

bool writeRecipeJson(const QString &directoryPath,
                     const ProductRecipe &recipe,
                     QString *errorMessage)
{
    const QString jsonPath = QDir(directoryPath).filePath(
                QStringLiteral("recipe.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage,
                 QStringLiteral("Unable to write recipe.json: %1")
                 .arg(jsonPath));
        return false;
    }

    const QByteArray bytes = QJsonDocument(productRecipeToJson(recipe))
            .toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        setError(errorMessage,
                 QStringLiteral("Unable to complete recipe.json: %1")
                 .arg(jsonPath));
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool copyRecipeAssets(const QString &directoryPath,
                      const ProductRecipe &recipe,
                      const QMap<QString, QString> &assetSourcePaths,
                      QString *errorMessage)
{
    if (assetSourcePaths.size() != recipe.assets.size()) {
        setError(errorMessage,
                 QStringLiteral("Asset source keys do not match recipe assets."));
        return false;
    }

    QSet<QString> destinationPaths;
    for (auto it = recipe.assets.constBegin(); it != recipe.assets.constEnd(); ++it) {
        if (!assetSourcePaths.contains(it.key())) {
            setError(errorMessage,
                     QStringLiteral("Missing asset source for key: %1")
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
                     QStringLiteral("Recipe asset source is missing or invalid: %1")
                     .arg(sourcePath));
            return false;
        }

        const QString destinationPath = QDir(directoryPath).filePath(relativePath);
        const QString destinationParent = QFileInfo(destinationPath).absolutePath();
        if (!QDir().mkpath(destinationParent)
                || !QFile::copy(sourceInfo.absoluteFilePath(), destinationPath)) {
            setError(errorMessage,
                     QStringLiteral("Unable to copy recipe asset %1 to %2")
                     .arg(sourceInfo.absoluteFilePath(), destinationPath));
            return false;
        }
    }
    return true;
}

} // namespace

RecipeStore::RecipeStore(const QString &recipesRootPath,
                         const AssetValidator &assetValidator,
                         const DirectoryRenameFunction &directoryRenameFunction)
    : m_recipesRootPath(recipesRootPath.trimmed().isEmpty()
                        ? QString()
                        : QDir(recipesRootPath).absolutePath()),
      m_assetValidator(assetValidator),
      m_directoryRenameFunction(directoryRenameFunction)
{
}

QString RecipeStore::recipesRootPath() const
{
    return m_recipesRootPath;
}

QString RecipeStore::recipeDirectoryPath(const QString &recipeId) const
{
    if (m_recipesRootPath.isEmpty() || !isCanonicalRecipeId(recipeId)) {
        return QString();
    }
    return QDir(m_recipesRootPath).filePath(recipeId.trimmed().toLower());
}

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

    const QString directoryPath = recipeDirectoryPath(recipeId);
    if (directoryPath.isEmpty()) {
        setError(errorMessage, QStringLiteral("Recipe ID is invalid."));
        return false;
    }
    return loadRecipeFromDirectory(directoryPath,
                                   recipeId.trimmed().toLower(),
                                   recipe,
                                   errorMessage);
}

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
        setError(errorMessage, QStringLiteral("Recipe root path is empty."));
        return false;
    }

    const QFileInfo rootInfo(m_recipesRootPath);
    if (rootInfo.isSymLink()) {
        setError(errorMessage,
                 QStringLiteral("Recipe root is not a readable regular directory: %1")
                 .arg(m_recipesRootPath));
        return false;
    }
    if (!rootInfo.exists()) {
        *catalog = RecipeCatalog();
        return true;
    }
    if (!rootInfo.isDir() || !rootInfo.isReadable()) {
        setError(errorMessage,
                 QStringLiteral("Recipe root is not a readable regular directory: %1")
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
        setError(errorMessage, QStringLiteral("Recipe target path is invalid."));
        return false;
    }
    const QFileInfo targetInfo(targetPath);
    if (targetInfo.exists() && (targetInfo.isSymLink() || !targetInfo.isDir())) {
        setError(errorMessage,
                 QStringLiteral("Recipe target is not a regular directory: %1")
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
                     QStringLiteral("Unable to create recipe transaction directory: %1")
                     .arg(tempPath));
        }
        return false;
    }

    const QString assetsDirectoryPath = QDir(tempPath).filePath(
                QStringLiteral("assets"));
    bool prepared = QDir().mkpath(assetsDirectoryPath);
    if (!prepared) {
        setError(errorMessage,
                 QStringLiteral("Unable to create recipe assets directory: %1")
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
                     QStringLiteral("Reloaded recipe does not match the saved recipe."));
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
                     "Unable to back up existing recipe directory: %1.\n"
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
                 ? QStringLiteral("Unable to commit recipe; the previous recipe was restored.")
                 : QStringLiteral("Unable to commit recipe and restore the previous recipe."));
        return false;
    }

    if (targetExisted) {
        QString cleanupError;
        removeDirectoryIfExists(backupPath, &cleanupError);
    }
    return true;
}

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
                 QStringLiteral("Recipe directory is missing or invalid: %1")
                 .arg(directoryPath));
        return false;
    }

    const QString jsonPath = QDir(directoryPath).filePath(
                QStringLiteral("recipe.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("Unable to read recipe.json: %1")
                 .arg(jsonPath));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(),
                                                            &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage,
                 QStringLiteral("recipe.json is invalid: %1")
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
                 QStringLiteral("Recipe ID does not match the requested directory."));
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
                     QStringLiteral("Recipe asset is missing or invalid: %1")
                     .arg(it.value()));
            return false;
        }
        if (m_assetValidator) {
            QString validatorError;
            if (!m_assetValidator(candidate,
                                  it.key(),
                                  assetInfo.absoluteFilePath(),
                                  &validatorError)) {
                setError(errorMessage,
                         validatorError.isEmpty()
                         ? QStringLiteral("Recipe asset validation failed: %1")
                           .arg(it.key())
                         : validatorError);
                return false;
            }
        }
    }

    *recipe = candidate;
    return true;
}
