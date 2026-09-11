#pragma once

#include "application/application_result.h"
#include "system_support/settings/app_settings_store.h"

#include <memory>

class SettingsApplicationService
{
public:
    SettingsApplicationService(
        const std::shared_ptr<AppSettingsStore> &store,
        const AppSettings &loadedSettings);

    const AppSettings &current() const;
    OperationResult saveConfiguration(const AppSettings &candidate);
    OperationResult commitAppliedHardwareSettings(
        const AppSettings &appliedSettings);
    OperationResult saveTemplatePaths(DetectionMode mode,
                                      const QStringList &paths);
    OperationResult removeTemplatePaths(
        DetectionMode mode,
        const QStringList &paths);
    OperationResult saveTemplatePathsAndDirectory(
        DetectionMode mode,
        const QStringList &paths,
        const QString &directoryPath);
    OperationResult saveTissueThreshold(double value);
    QString applicationDataRoot() const;

private:
    static OperationResult storeFailure(
        const AppSettingsStoreError &error);

    std::shared_ptr<AppSettingsStore> m_store;
    AppSettings m_current;
};
