#include "application/settings_application_service.h"

SettingsApplicationService::SettingsApplicationService(
    const std::shared_ptr<MachineSettingsStore> &store,
    const MachineSettings &loadedSettings)
    : m_store(store),
      m_current(loadedSettings),
      m_draft(loadedSettings)
{
}

const MachineSettings &SettingsApplicationService::current() const
{
    return m_current;
}

const MachineSettings &SettingsApplicationService::draft() const
{
    return m_draft;
}

MachineSettings &SettingsApplicationService::editableDraft()
{
    return m_draft;
}

void SettingsApplicationService::updateDraft(
    const MachineSettings &draft)
{
    m_draft = draft;
}

OperationResult SettingsApplicationService::applyDraft()
{
    MachineSettingsStoreError error;
    if (!m_store || !m_store->save(m_draft, &error)) {
        return storeFailure(error);
    }
    m_current = m_draft;
    return OperationResult::accepted();
}

void SettingsApplicationService::discardDraft()
{
    m_draft = m_current;
}

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

bool SettingsApplicationService::hasUnappliedChanges() const
{
    return m_current != m_draft;
}

QString SettingsApplicationService::applicationDataRoot() const
{
    return m_store ? m_store->applicationDataRoot() : QString();
}

QString SettingsApplicationService::editorWorkspacesRootPath() const
{
    return m_store ? m_store->editorWorkspacesRootPath() : QString();
}

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
