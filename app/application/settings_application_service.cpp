// 文件作用：本文件用于组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 主要职责：组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/settings_application_service.h"

// 函数说明：SettingsApplicationService 构造函数创建组件并初始化其依赖和初始状态。
SettingsApplicationService::SettingsApplicationService(
    const std::shared_ptr<MachineSettingsStore> &store,
    const MachineSettings &loadedSettings)
    : m_store(store),
      m_current(loadedSettings),
      m_draft(loadedSettings)
{
}

// 函数说明：current 函数读取、等待或计算对应的数据。
const MachineSettings &SettingsApplicationService::current() const
{
    return m_current;
}

// 函数说明：draft 函数实现名称所表示的处理步骤。
const MachineSettings &SettingsApplicationService::draft() const
{
    return m_draft;
}

// 函数说明：editableDraft 函数实现名称所表示的处理步骤。
MachineSettings &SettingsApplicationService::editableDraft()
{
    return m_draft;
}

// 函数说明：updateDraft 函数更新或应用对应的配置和状态。
void SettingsApplicationService::updateDraft(
    const MachineSettings &draft)
{
    m_draft = draft;
}

// 函数说明：applyDraft 函数更新或应用对应的配置和状态。
OperationResult SettingsApplicationService::applyDraft()
{
    MachineSettingsStoreError error;
    if (!m_store || !m_store->save(m_draft, &error)) {
        return storeFailure(error);
    }
    m_current = m_draft;
    return OperationResult::accepted();
}

// 函数说明：discardDraft 函数停止流程、清理状态或释放对应资源。
void SettingsApplicationService::discardDraft()
{
    m_draft = m_current;
}

// 函数说明：restoreDefaults 函数校验、转换或恢复对应数据。
OperationResult SettingsApplicationService::restoreDefaults()
{
    MachineSettings defaults;
    MachineSettingsStoreError error;
    if (!m_store || !m_store->restoreDefaults(&defaults, &error)) {
        return storeFailure(error);
    }
    m_current = defaults;
    m_draft = defaults;
    return OperationResult::accepted();
}

// 函数说明：clearSettings 函数停止流程、清理状态或释放对应资源。
OperationResult SettingsApplicationService::clearSettings()
{
    MachineSettingsStoreError error;
    if (!m_store || !m_store->clear(&error)) {
        return storeFailure(error);
    }
    m_current = MachineSettings::defaults();
    m_draft = m_current;
    return OperationResult::accepted();
}

// 函数说明：hasUnappliedChanges 函数检查相关状态并返回判断结果。
bool SettingsApplicationService::hasUnappliedChanges() const
{
    return m_current != m_draft;
}

// 函数说明：applicationDataRoot 函数实现名称所表示的处理步骤。
QString SettingsApplicationService::applicationDataRoot() const
{
    return m_store ? m_store->applicationDataRoot() : QString();
}

// 函数说明：editorWorkspacesRootPath 函数实现名称所表示的处理步骤。
QString SettingsApplicationService::editorWorkspacesRootPath() const
{
    return m_store ? m_store->editorWorkspacesRootPath() : QString();
}

// 函数说明：storeFailure 函数保存或发布对应的数据和资源。
OperationResult SettingsApplicationService::storeFailure(
    const MachineSettingsStoreError &error)
{
    return OperationResult::rejected(
                error.code.isEmpty()
                ? QStringLiteral("MACHINE_SETTINGS_STORE_UNAVAILABLE")
                : error.code,
                error.userMessage.isEmpty()
                ? QStringLiteral("机器设置保存失败。")
                : error.userMessage,
                error.diagnostic);
}
