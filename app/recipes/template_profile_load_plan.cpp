#include "template_profile_load_plan.h"

#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>

namespace {

const QString kCharacterRolePrefix = QStringLiteral("character/");

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool validatedAssetPath(const QMap<QString, QString> &assetPathsByRole,
                        const QString &role,
                        bool required,
                        QString *absolutePath,
                        QString *errorMessage)
{
    const QString sourcePath = assetPathsByRole.value(role).trimmed();
    if (sourcePath.isEmpty()) {
        if (!required) {
            absolutePath->clear();
            return true;
        }
        setError(errorMessage,
                 QStringLiteral("Recipe profile asset role is missing: %1")
                 .arg(role));
        return false;
    }

    const QFileInfo assetInfo(sourcePath);
    if (!assetInfo.exists()
            || assetInfo.isSymLink()
            || !assetInfo.isFile()
            || assetInfo.size() <= 0) {
        setError(errorMessage,
                 QStringLiteral("Recipe profile asset is missing or invalid: %1")
                 .arg(role));
        return false;
    }

    *absolutePath = assetInfo.absoluteFilePath();
    return true;
}

struct CharacterAssetCandidate
{
    QString fileName;
    QString absoluteFilePath;
    QString normalizedBaseName;
};

bool characterAssetLess(const CharacterAssetCandidate &left,
                        const CharacterAssetCandidate &right)
{
    const int caseInsensitiveOrder =
            QString::compare(left.fileName,
                             right.fileName,
                             Qt::CaseInsensitive);
    if (caseInsensitiveOrder != 0) {
        return caseInsensitiveOrder < 0;
    }
    return left.fileName < right.fileName;
}

bool collectCharacterAssets(
        const QMap<QString, QString> &assetPathsByRole,
        QVector<CharacterAssetCandidate> *characterAssets,
        QString *errorMessage)
{
    for (auto it = assetPathsByRole.constBegin();
         it != assetPathsByRole.constEnd();
         ++it) {
        const QString role = it.key().trimmed();
        if (!role.startsWith(kCharacterRolePrefix)) {
            continue;
        }

        const QString fileName = role.mid(kCharacterRolePrefix.size()).trimmed();
        if (fileName.isEmpty()
                || QFileInfo(fileName).fileName() != fileName) {
            setError(errorMessage,
                     QStringLiteral("Recipe character asset role is invalid: %1")
                     .arg(role));
            return false;
        }

        QString absoluteFilePath;
        if (!validatedAssetPath(assetPathsByRole,
                                it.key(),
                                true,
                                &absoluteFilePath,
                                errorMessage)) {
            return false;
        }

        CharacterAssetCandidate candidate;
        candidate.fileName = fileName;
        candidate.absoluteFilePath = absoluteFilePath;
        candidate.normalizedBaseName =
                QFileInfo(fileName).completeBaseName().trimmed().toLower();
        if (candidate.normalizedBaseName.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("Recipe character asset name is invalid: %1")
                     .arg(fileName));
            return false;
        }
        characterAssets->append(candidate);
    }

    std::sort(characterAssets->begin(),
              characterAssets->end(),
              characterAssetLess);
    return true;
}

QVector<CharacterAssetCandidate> matchingCharacterAssets(
        const QVector<CharacterAssetCandidate> &characterAssets,
        const QString &targetUnit)
{
    QVector<CharacterAssetCandidate> exactAssets;
    QVector<CharacterAssetCandidate> variantAssets;
    for (const CharacterAssetCandidate &asset : characterAssets) {
        if (asset.normalizedBaseName == targetUnit) {
            exactAssets.append(asset);
            continue;
        }
        if (!asset.normalizedBaseName.startsWith(targetUnit)) {
            continue;
        }

        const QString suffix = asset.normalizedBaseName.mid(targetUnit.size());
        if (suffix.startsWith(QLatin1Char('_'))
                || suffix.startsWith(QLatin1Char('-'))
                || suffix.startsWith(QLatin1Char('('))) {
            variantAssets.append(asset);
        }
    }

    exactAssets += variantAssets;
    return exactAssets;
}

} // namespace

QStringList parseTemplateTargetUnits(const QString &targetText)
{
    QStringList targetUnits;
    const QRegularExpression expression(
                R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matches =
            expression.globalMatch(targetText);

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        QString targetUnit;
        if (!match.captured(1).isEmpty()) {
            targetUnit = match.captured(1);
        } else if (!match.captured(2).isEmpty()) {
            targetUnit = match.captured(2);
        } else if (!match.captured(3).isEmpty()) {
            targetUnit = match.captured(3);
        } else if (!match.captured(4).isEmpty()) {
            targetUnit = match.captured(4);
        }

        if (!targetUnit.isEmpty()) {
            targetUnits.append(targetUnit.toLower());
        }
    }
    return targetUnits;
}

bool buildTemplateProfileLoadPlan(
        const ResolvedRecipeProfile &resolvedProfile,
        const QStringList &targetUnits,
        TemplateProfileLoadPlan *loadPlan,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!loadPlan) {
        setError(errorMessage,
                 QStringLiteral("TemplateProfileLoadPlan output is null."));
        return false;
    }

    TemplateProfileLoadPlan candidate;
    candidate.profile = resolvedProfile.profile;
    if (!validatedAssetPath(resolvedProfile.assetPathsByRole,
                            QStringLiteral("trackingTemplate"),
                            true,
                            &candidate.trackingTemplatePath,
                            errorMessage)
            || !validatedAssetPath(resolvedProfile.assetPathsByRole,
                                   QStringLiteral("calibration"),
                                   true,
                                   &candidate.calibrationPath,
                                   errorMessage)
            || !validatedAssetPath(resolvedProfile.assetPathsByRole,
                                   QStringLiteral("rawImage"),
                                   false,
                                   &candidate.rawImagePath,
                                   errorMessage)) {
        return false;
    }

    QVector<CharacterAssetCandidate> characterAssets;
    if (!collectCharacterAssets(resolvedProfile.assetPathsByRole,
                                &characterAssets,
                                errorMessage)) {
        return false;
    }

    for (const QString &targetUnit : targetUnits) {
        const QString normalizedUnit = targetUnit.trimmed().toLower();
        if (!normalizedUnit.isEmpty()) {
            candidate.targetUnits.append(normalizedUnit);
        }
    }

    if (candidate.targetUnits.isEmpty()) {
        candidate.pendingTargetMessage =
                QStringLiteral("Target characters are empty or cannot be parsed.");
        *loadPlan = candidate;
        return true;
    }

    QStringList missingTargets;
    for (int targetIndex = 0;
         targetIndex < candidate.targetUnits.size();
         ++targetIndex) {
        const QString targetUnit = candidate.targetUnits.at(targetIndex);
        const QVector<CharacterAssetCandidate> matches =
                matchingCharacterAssets(characterAssets, targetUnit);
        if (matches.isEmpty()) {
            missingTargets.append(targetUnit);
            continue;
        }

        for (const CharacterAssetCandidate &match : matches) {
            TemplateCharacterLoadItem item;
            item.fileName = match.fileName;
            item.absoluteFilePath = match.absoluteFilePath;
            item.targetIndex = targetIndex;
            candidate.characterTemplates.append(item);
        }
    }

    if (!missingTargets.isEmpty()) {
        candidate.characterTemplates.clear();
        candidate.pendingTargetMessage =
                QStringLiteral("Character assets are incomplete for targets: %1")
                .arg(missingTargets.join(QLatin1Char(' ')));
    }

    *loadPlan = candidate;
    return true;
}

bool buildTemplateRecipeLoadPlan(
        const RecipeSelection &selection,
        TemplateRecipeLoadPlan *loadPlan,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!loadPlan) {
        setError(errorMessage,
                 QStringLiteral("TemplateRecipeLoadPlan output is null."));
        return false;
    }
    if (!selection.recipe
            || (selection.recipe->detectionMode != DetectionMode::Word
                && selection.recipe->detectionMode
                   != DetectionMode::BarcodeWord)) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe is not a word-family recipe."));
        return false;
    }
    if (selection.profiles.isEmpty()
            || selection.profiles.size()
               != selection.recipe->profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe profile collection is incomplete."));
        return false;
    }

    TemplateRecipeLoadPlan candidate;
    candidate.recipe = selection.recipe;
    candidate.recipeDirectoryPath = selection.recipeDirectoryPath;
    candidate.profiles.reserve(selection.profiles.size());
    for (int profileIndex = 0;
         profileIndex < selection.profiles.size();
         ++profileIndex) {
        const ResolvedRecipeProfile &resolvedProfile =
                selection.profiles.at(profileIndex);
        TemplateProfileLoadPlan profilePlan;
        QString profileError;
        if (!buildTemplateProfileLoadPlan(
                    resolvedProfile,
                    parseTemplateTargetUnits(
                        resolvedProfile.profile.targetText),
                    &profilePlan,
                    &profileError)) {
            const QString profileName =
                    resolvedProfile.profile.name.trimmed().isEmpty()
                    ? QString::number(profileIndex + 1)
                    : resolvedProfile.profile.name;
            setError(errorMessage,
                     QStringLiteral("Recipe profile %1 cannot be loaded: %2")
                     .arg(profileName, profileError));
            return false;
        }
        candidate.profiles.append(profilePlan);
    }

    *loadPlan = candidate;
    return true;
}
