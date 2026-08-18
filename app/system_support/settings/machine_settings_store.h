// 文件作用：本文件用于负责机器设置JSON的读取、校验、事务保存、清空和默认恢复。
// 主要职责：负责机器设置JSON的读取、校验、事务保存、清空和默认恢复。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "system_support/settings/machine_settings.h"

#include <QString>

// 组件说明：MachineSettingsLoadStatus 枚举列出该组件允许使用的稳定状态和选项。
enum class MachineSettingsLoadStatus
{
    Loaded,
    FirstRun
};

// 组件说明：MachineSettingsStoreError 数据结构集中保存该流程需要的一组相关数据。
struct MachineSettingsStoreError
{
    QString code;
    QString userMessage;
    QString diagnostic;

    // 函数说明：isEmpty 函数检查相关状态并返回判断结果。
    bool isEmpty() const { return code.isEmpty(); }
};

// 组件说明：MachineSettingsStore 组件封装对应业务职责和生命周期边界。
class MachineSettingsStore
{
public:
    explicit MachineSettingsStore(const QString &applicationDataRoot);

    QString applicationDataRoot() const;
    QString settingsFilePath() const;
    QString recipesRootPath() const;
    QString editorWorkspacesRootPath() const;

    bool load(MachineSettings *settings,
              MachineSettingsLoadStatus *status,
              MachineSettingsStoreError *error = nullptr) const;
    bool save(const MachineSettings &settings,
              MachineSettingsStoreError *error = nullptr) const;
    bool restoreDefaults(MachineSettings *settings,
                         MachineSettingsStoreError *error = nullptr) const;
    bool clear(MachineSettingsStoreError *error = nullptr) const;

private:
    QString m_applicationDataRoot;
};
