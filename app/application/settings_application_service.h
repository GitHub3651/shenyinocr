// 文件作用：本文件用于组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 主要职责：组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/application_result.h"
#include "system_support/settings/machine_settings_store.h"

#include <memory>

// 组件说明：SettingsApplicationService 组件封装对应业务职责和生命周期边界。
class SettingsApplicationService
{
public:
    SettingsApplicationService(
        const std::shared_ptr<MachineSettingsStore> &store,
        const MachineSettings &loadedSettings);

    const MachineSettings &current() const;
    const MachineSettings &draft() const;
    MachineSettings &editableDraft();
    void updateDraft(const MachineSettings &draft);
    OperationResult applyDraft();
    void discardDraft();
    OperationResult restoreDefaults();
    OperationResult clearSettings();
    bool hasUnappliedChanges() const;
    QString applicationDataRoot() const;
    QString editorWorkspacesRootPath() const;

private:
    static OperationResult storeFailure(
        const MachineSettingsStoreError &error);

    std::shared_ptr<MachineSettingsStore> m_store;
    MachineSettings m_current;
    MachineSettings m_draft;
};
