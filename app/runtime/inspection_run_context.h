#pragma once

#include "recipes/prepared_recipe.h"
#include "runtime/inspection_profile_snapshot.h"
#include "system_support/settings/machine_settings.h"

#include <QDateTime>
#include <QString>

// Immutable snapshot created exactly once for a production run. Runtime,
// detection and result handling all observe the same settings and recipe.
struct InspectionRunContext
{
    InspectionRunContext(
        const QString &runIdValue,
        const QDateTime &startedAtUtcValue,
        const MachineSettings &machineSettingsValue,
        const PreparedRecipeSnapshot &preparedRecipeValue,
        const InspectionProfileSnapshot &profileSnapshotValue)
        : runId(runIdValue),
          startedAtUtc(startedAtUtcValue),
          machineSettings(machineSettingsValue),
          preparedRecipe(preparedRecipeValue),
          profileSnapshot(profileSnapshotValue)
    {
    }

    const QString runId;
    const QDateTime startedAtUtc;
    const MachineSettings machineSettings;
    const PreparedRecipeSnapshot preparedRecipe;
    const InspectionProfileSnapshot profileSnapshot;
};
