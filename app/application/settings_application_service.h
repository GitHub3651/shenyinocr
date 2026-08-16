#pragma once

#include "application/application_result.h"
#include "system_support/settings/machine_settings_store.h"

#include <memory>

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
