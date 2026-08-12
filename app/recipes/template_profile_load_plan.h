#ifndef RECIPES_TEMPLATE_PROFILE_LOAD_PLAN_H
#define RECIPES_TEMPLATE_PROFILE_LOAD_PLAN_H

#include "recipe_selection.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct TemplateCharacterLoadItem
{
    QString fileName;
    QString absoluteFilePath;
    int targetIndex = -1;
};

struct TemplateProfileLoadPlan
{
    RecipeProfile profile;
    QString trackingTemplatePath;
    QString calibrationPath;
    QString rawImagePath;
    QStringList targetUnits;
    QVector<TemplateCharacterLoadItem> characterTemplates;
    QString pendingTargetMessage;
};

bool buildTemplateProfileLoadPlan(
        const ResolvedRecipeProfile &resolvedProfile,
        const QStringList &targetUnits,
        TemplateProfileLoadPlan *loadPlan,
        QString *errorMessage = nullptr);

#endif // RECIPES_TEMPLATE_PROFILE_LOAD_PLAN_H
