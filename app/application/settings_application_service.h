// 文件作用：本文件用于组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 主要职责：组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/application_result.h"
#include "system_support/settings/app_settings_store.h"

#include <memory>

// 组件说明：SettingsApplicationService 组件封装对应业务职责和生命周期边界。
class SettingsApplicationService
{
public:
    SettingsApplicationService(
        const std::shared_ptr<AppSettingsStore> &store,
        const AppSettings &loadedSettings);

    const AppSettings &current() const;
    const AppSettings &draft() const;
    AppSettings &editableDraft();
    void updateDraft(const AppSettings &draft);
    OperationResult applyDraft();
    void discardDraft();
    OperationResult saveTemplatePaths(DetectionMode mode,
                                      const QStringList &paths);
    OperationResult saveTissueThreshold(double value);
    OperationResult restoreDefaults();
    OperationResult clearSettings();
    bool hasUnappliedChanges() const;
    QString applicationDataRoot() const;

private:
    static OperationResult storeFailure(
        const AppSettingsStoreError &error);

    OperationResult saveCandidate(const AppSettings &candidate,
                                  bool preserveMachineDraft = false);

    std::shared_ptr<AppSettingsStore> m_store;
    AppSettings m_current;
    AppSettings m_draft;
};
