#pragma once

#include "system_support/settings/machine_settings.h"

#include <QString>

enum class MachineSettingsLoadStatus
{
    Loaded,
    FirstRun
};

struct MachineSettingsStoreError
{
    QString code;
    QString userMessage;
    QString diagnostic;

    bool isEmpty() const { return code.isEmpty(); }
};

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
