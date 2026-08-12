#include "template_profile_load_plan.h"

#include <QFileInfo>
#include <QMap>

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
