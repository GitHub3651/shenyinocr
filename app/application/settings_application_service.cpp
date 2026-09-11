#include "application/settings_application_service.h"
#include "system_support/logging/log_categories.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

SettingsApplicationService::SettingsApplicationService(
    const std::shared_ptr<AppSettingsStore> &store,
    const AppSettings &loadedSettings)
    : m_store(store),
      m_current(loadedSettings)
{
}

const AppSettings &SettingsApplicationService::current() const
{
    return m_current;
}

OperationResult SettingsApplicationService::saveConfiguration(
    const AppSettings &candidate)
{
    AppSettingsStoreError error;
    if (!m_store || !m_store->save(candidate, &error)) {
        qCWarning(logUi).noquote()
                << QStringLiteral(
                    "event=settings.save_failed code=%1 path=%2 diagnostic=%3")
                   .arg(error.code,
                        m_store ? m_store->settingsFilePath() : QString(),
                        error.diagnostic);
        return storeFailure(error);
    }
    m_current = candidate;
    return OperationResult::accepted();
}

OperationResult SettingsApplicationService::commitAppliedHardwareSettings(
    const AppSettings &appliedSettings)
{
    m_current = appliedSettings;
    AppSettingsStoreError error;
    if (!m_store || !m_store->save(m_current, &error)) {
        qCWarning(logUi).noquote()
                << QStringLiteral(
                    "event=settings.hardware_persist_failed code=%1 path=%2 diagnostic=%3")
                   .arg(error.code,
                        m_store ? m_store->settingsFilePath() : QString(),
                        error.diagnostic);
        return storeFailure(error);
    }
    return OperationResult::accepted();
}

QString SettingsApplicationService::applicationDataRoot() const
{
    return m_store ? m_store->applicationDataRoot() : QString();
}

OperationResult SettingsApplicationService::storeFailure(
    const AppSettingsStoreError &error)
{
    return OperationResult::rejected(
                error.code.isEmpty()
                ? QStringLiteral("APP_SETTINGS_STORE_UNAVAILABLE")
                : error.code,
                error.userMessage.isEmpty()
                ? QStringLiteral("软件设置保存失败。")
                : error.userMessage,
                error.diagnostic);
}

OperationResult SettingsApplicationService::saveTemplatePaths(
    DetectionMode mode,
    const QStringList &paths)
{
    AppSettings candidate = m_current;
    QString errorMessage;
    if (!candidate.detectionSchemes.setTemplatePaths(
            mode, paths, &errorMessage)) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_SELECTION_INVALID"),
                    errorMessage.isEmpty()
                    ? QStringLiteral("模板选择无效。") : errorMessage);
    }
    return saveConfiguration(candidate);
}

OperationResult SettingsApplicationService::removeTemplatePaths(
    DetectionMode mode,
    const QStringList &pathsToRemove)
{
    QStringList paths = m_current.detectionSchemes.templatePaths(mode);
    for (int index = paths.size() - 1; index >= 0; --index) {
        for (const QString &path : pathsToRemove) {
            if (QString::compare(paths.at(index), path,
                                 Qt::CaseInsensitive) == 0) {
                paths.removeAt(index);
                break;
            }
        }
    }
    return saveTemplatePaths(mode, paths);
}

OperationResult SettingsApplicationService::saveTemplatePathsAndDirectory(
    DetectionMode mode,
    const QStringList &paths,
    const QString &directoryPath)
{
    AppSettings candidate = m_current;
    QString errorMessage;
    if (!candidate.detectionSchemes.setTemplatePaths(
            mode, paths, &errorMessage)) {
        return OperationResult::rejected(
                    QStringLiteral("TEMPLATE_SELECTION_INVALID"),
                    errorMessage.isEmpty()
                    ? QStringLiteral("模板选择无效。") : errorMessage);
    }
    candidate.templateSaveDirectory = QDir::cleanPath(
                QFileInfo(directoryPath).absoluteFilePath());
    return saveConfiguration(candidate);
}

OperationResult SettingsApplicationService::saveTissueThreshold(double value)
{
    if (!std::isfinite(value) || value < 0.0) {
        return OperationResult::rejected(
                    QStringLiteral("TISSUE_THRESHOLD_INVALID"),
                    QStringLiteral("纸巾检测阈值必须是非负数。"));
    }
    AppSettings candidate = m_current;
    candidate.detectionSchemes.tissueRoughnessThreshold = value;
    return saveConfiguration(candidate);
}
