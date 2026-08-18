// 文件作用：本文件用于保存一次运行的唯一标识、设置快照、配方快照和启动信息。
// 主要职责：保存一次运行的唯一标识、设置快照、配方快照和启动信息。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
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
    // 函数说明：InspectionRunContext 构造函数创建组件并初始化其依赖和初始状态。
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
