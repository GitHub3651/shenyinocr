#ifndef TEMPLATE_PROFILE_MAPPER_H
#define TEMPLATE_PROFILE_MAPPER_H

#include "product_recipe.h"

struct TemplatePrivateSettings;

RecipeProfile recipeProfileFromTemplatePrivateSettings(
        const QString &profileName,
        const TemplatePrivateSettings &settings,
        const QMap<QString, QString> &assetKeys = QMap<QString, QString>());

TemplatePrivateSettings templatePrivateSettingsFromRecipeProfile(
        const RecipeProfile &profile);

#endif // TEMPLATE_PROFILE_MAPPER_H
