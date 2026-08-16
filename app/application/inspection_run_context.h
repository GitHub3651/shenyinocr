#pragma once

#include "recipes/prepared_recipe.h"
#include "system_support/settings/machine_settings.h"

#include <QDateTime>
#include <QString>

struct InspectionRunContext
{
    QString runId;
    QDateTime startedAtUtc;
    MachineSettings machineSettings;
    PreparedRecipeSnapshot preparedRecipe;
};
