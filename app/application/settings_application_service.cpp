// 文件作用：本文件用于组织机器设置读取、保存、默认恢复和软件数据清理用例。
// 主要职责：组织机器设置读取、保存、默认恢复和软件数据清理用例。
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
      m_current(loadedSettings),
      m_draft(loadedSettings)
{
}

// 函数说明：current 函数读取、等待或计算对应的数据。
const AppSettings &SettingsApplicationService::current() const
{
    return m_current;
}

// 函数说明：draft 函数实现名称所表示的处理步骤。
const AppSettings &SettingsApplicationService::draft() const
{
    return m_draft;
}

// 函数说明：editableDraft 函数实现名称所表示的处理步骤。
AppSettings &SettingsApplicationService::editableDraft()
{
    return m_draft;
}

// 函数说明：updateDraft 函数更新或应用对应的配置和状态。
void SettingsApplicationService::updateDraft(
    const AppSettings &draft)
{
    m_draft = draft;
}

// 函数说明：applyDraft 函数更新或应用对应的配置和状态。
OperationResult SettingsApplicationService::applyDraft()
{
    AppSettings candidate = m_draft;
    candidate.detectionSchemes = m_current.detectionSchemes;
    candidate.templateSaveDirectory = m_current.templateSaveDirectory;
    return saveCandidate(candidate);
}

// 函数说明：discardDraft 函数停止流程、清理状态或释放对应资源。
void SettingsApplicationService::discardDraft()
{
    m_draft = m_current;
}

// 函数说明：restoreDefaults 函数校验、转换或恢复对应数据。
OperationResult SettingsApplicationService::restoreDefaults()
{
    AppSettings defaults = AppSettings::defaults();
    defaults.detectionSchemes = m_current.detectionSchemes;
    return saveCandidate(defaults);
}

// 函数说明：clearSettings 函数停止流程、清理状态或释放对应资源。
OperationResult SettingsApplicationService::clearSettings()
{
    return saveCandidate(AppSettings::defaults());
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
    return saveCandidate(candidate, true);
}

OperationResult SettingsApplicationService::removeTemplatePath(
    DetectionMode mode,
    const QString &path)
{
    QStringList paths = m_current.detectionSchemes.templatePaths(mode);
    for (int index = paths.size() - 1; index >= 0; --index) {
        if (QString::compare(paths.at(index), path,
                             Qt::CaseInsensitive) == 0) {
            paths.removeAt(index);
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
    return saveCandidate(candidate, true);
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
    return saveCandidate(candidate, true);
}

OperationResult SettingsApplicationService::saveCandidate(
    const AppSettings &candidate,
    bool preserveMachineDraft)
{
    AppSettingsStoreError error;
    if (!m_store || !m_store->save(candidate, &error)) {
        return storeFailure(error);
    }
    const AppSettings previousDraft = m_draft;
    m_current = candidate;
    if (preserveMachineDraft) {
        m_draft = previousDraft;
        m_draft.detectionSchemes = candidate.detectionSchemes;
        m_draft.templateSaveDirectory = candidate.templateSaveDirectory;
    } else {
        m_draft = candidate;
    }
    return OperationResult::accepted();
}
