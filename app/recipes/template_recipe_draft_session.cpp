#include "template_recipe_draft_session.h"

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

bool normalizedExistingDirectory(const QString &path,
                                 QString *normalizedPath,
                                 QString *errorMessage)
{
    const QFileInfo directoryInfo(path.trimmed());
    if (path.trimmed().isEmpty()
            || !directoryInfo.exists()
            || !directoryInfo.isDir()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft source directory is invalid."));
        return false;
    }

    const QString canonicalPath = directoryInfo.canonicalFilePath();
    if (canonicalPath.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft source directory cannot be resolved."));
        return false;
    }

    *normalizedPath = QDir::cleanPath(canonicalPath);
    return true;
}

bool isCanonicalRecipeId(const QString &recipeId)
{
    const QUuid parsed(QStringLiteral("{%1}").arg(recipeId.trimmed()));
    return !parsed.isNull()
            && parsed.toString(QUuid::WithoutBraces).compare(
                recipeId.trimmed(), Qt::CaseInsensitive) == 0;
}

} // namespace

void TemplateRecipeDraftSession::reset()
{
    m_isActive = false;
    m_sourceDirectoryPath.clear();
    m_recipeHeader = ProductRecipe();
}

bool TemplateRecipeDraftSession::begin(
        const ProductRecipe &recipeHeader,
        const QString &sourceDirectoryPath,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!isCanonicalRecipeId(recipeHeader.recipeId)) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft recipeId is invalid."));
        return false;
    }
    if (recipeHeader.displayName.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft displayName is empty."));
        return false;
    }
    if (!isTemplateRecipeMode(recipeHeader.detectionMode)) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft only supports template-based modes."));
        return false;
    }

    QString normalizedSourceDirectory;
    if (!normalizedExistingDirectory(sourceDirectoryPath,
                                     &normalizedSourceDirectory,
                                     errorMessage)) {
        return false;
    }

    m_recipeHeader = recipeHeader;
    m_sourceDirectoryPath = normalizedSourceDirectory;
    m_isActive = true;
    return true;
}

bool TemplateRecipeDraftSession::isActive() const
{
    return m_isActive;
}

QString TemplateRecipeDraftSession::sourceDirectoryPath() const
{
    return m_sourceDirectoryPath;
}

const ProductRecipe &TemplateRecipeDraftSession::recipeHeader() const
{
    return m_recipeHeader;
}

bool TemplateRecipeDraftSession::publish(
        const RecipeStore &store,
        const QString &sourceDirectoryPath,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        RecipeSelection *publishedSelection,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_isActive) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft session is not active."));
        return false;
    }
    if (!publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft published selection output is null."));
        return false;
    }

    QString normalizedSourceDirectory;
    if (!normalizedExistingDirectory(sourceDirectoryPath,
                                     &normalizedSourceDirectory,
                                     errorMessage)) {
        return false;
    }
    if (normalizedSourceDirectory.compare(m_sourceDirectoryPath,
                                          Qt::CaseInsensitive) != 0) {
        setError(errorMessage,
                 QStringLiteral("Template recipe draft source directory changed."));
        return false;
    }

    RecipeSelection candidate;
    if (!publishTemplateRecipe(store,
                               m_recipeHeader,
                               profileSources,
                               &candidate,
                               errorMessage)) {
        return false;
    }

    m_recipeHeader = *candidate.recipe;
    *publishedSelection = candidate;
    return true;
}
