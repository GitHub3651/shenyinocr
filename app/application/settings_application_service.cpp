// 文件作用：本文件用于组织软件设置读取与保存用例。
// 主要职责：维护当前软件设置，并协调设置与模板配置持久化。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/settings_application_service.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

// 函数说明：SettingsApplicationService 构造函数创建组件并初始化其依赖和初始状态。
SettingsApplicationService::SettingsApplicationService(
    const std::shared_ptr<AppSettingsStore> &store,
    const AppSettings &loadedSettings)
    : m_store(store),
      m_current(loadedSettings)
{
}

// 函数说明：current 函数读取、等待或计算对应的数据。
const AppSettings &SettingsApplicationService::current() const
{
    return m_current;
}

OperationResult SettingsApplicationService::saveConfiguration(
    const AppSettings &candidate)
{
    AppSettingsStoreError error;
    if (!m_store || !m_store->save(candidate, &error)) {
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
        return storeFailure(error);
    }
    return OperationResult::accepted();
}

// 函数说明：applicationDataRoot 函数实现名称所表示的处理步骤。
QString SettingsApplicationService::applicationDataRoot() const
{
    return m_store ? m_store->applicationDataRoot() : QString();
}

// 函数说明：storeFailure 函数保存或发布对应的数据和资源。
OperationResult SettingsApplicationService::storeFailure(
    const AppSettingsStoreError &error)
{
    return OperationResult::rejected(
                error.code.isEmpty()
                ? QStringLiteral("APP_SETTINGS_STORE_UNAVAILABLE")
                : error.code,
                error.userMessage.isEmpty()
                ? QStringLiteral("应用设置保存失败。")
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
