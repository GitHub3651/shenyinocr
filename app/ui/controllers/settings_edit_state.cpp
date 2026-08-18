// 文件作用：本文件用于跟踪界面设置项的已应用值和未应用修改。
// 主要职责：跟踪界面设置项的已应用值和未应用修改。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "settings_edit_state.h"

namespace {

// 函数说明：normalizedDisplayName 函数校验、转换或恢复对应数据。
QString normalizedDisplayName(const QString &displayName,
                              const QString &fallback)
{
    QString normalized = displayName.trimmed();
    normalized.remove(QLatin1Char(':'));
    normalized.remove(QChar(0xff1a));
    return normalized.isEmpty() ? fallback : normalized;
}

} // namespace

// 函数说明：registerGlobalSetting 函数实现名称所表示的处理步骤。
void SettingsEditState::registerGlobalSetting(
        const QString &key,
        const QString &displayName)
{
    const QString normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return;
    }
    Entry entry = m_globalSettings.value(normalizedKey);
    entry.displayName = normalizedDisplayName(displayName, normalizedKey);
    m_globalSettings.insert(normalizedKey, entry);
}

// 函数说明：isGlobalDirty 函数检查相关状态并返回判断结果。
bool SettingsEditState::isGlobalDirty(const QString &key) const
{
    const auto it = m_globalSettings.constFind(key);
    return it != m_globalSettings.constEnd() && it.value().dirty;
}

// 函数说明：setGlobalDirty 函数更新或应用对应的配置和状态。
void SettingsEditState::setGlobalDirty(const QString &key, bool dirty)
{
    auto it = m_globalSettings.find(key);
    if (it != m_globalSettings.end()) {
        it.value().dirty = dirty;
    }
}

// 函数说明：clearAllGlobalDirty 函数停止流程、清理状态或释放对应资源。
void SettingsEditState::clearAllGlobalDirty()
{
    for (auto it = m_globalSettings.begin();
         it != m_globalSettings.end();
         ++it) {
        it.value().dirty = false;
    }
}

// 函数说明：setTemplateTargetDirty 函数更新或应用对应的配置和状态。
void SettingsEditState::setTemplateTargetDirty(bool dirty)
{
    m_templateTargetDirty = dirty;
}

// 函数说明：setTemplateThresholdDirty 函数更新或应用对应的配置和状态。
void SettingsEditState::setTemplateThresholdDirty(bool dirty)
{
    m_templateThresholdDirty = dirty;
}

// 函数说明：isTemplateTargetDirty 函数检查相关状态并返回判断结果。
bool SettingsEditState::isTemplateTargetDirty() const
{
    return m_templateTargetDirty;
}

// 函数说明：isTemplateThresholdDirty 函数检查相关状态并返回判断结果。
bool SettingsEditState::isTemplateThresholdDirty() const
{
    return m_templateThresholdDirty;
}

// 函数说明：clearTemplateDirty 函数停止流程、清理状态或释放对应资源。
void SettingsEditState::clearTemplateDirty()
{
    m_templateTargetDirty = false;
    m_templateThresholdDirty = false;
}

// 函数说明：globalDirtyNames 函数实现名称所表示的处理步骤。
QStringList SettingsEditState::globalDirtyNames() const
{
    QStringList names;
    for (auto it = m_globalSettings.constBegin();
         it != m_globalSettings.constEnd();
         ++it) {
        if (it.value().dirty && !names.contains(it.value().displayName)) {
            names.append(it.value().displayName);
        }
    }
    return names;
}

// 函数说明：templateDirtyNames 函数实现名称所表示的处理步骤。
QStringList SettingsEditState::templateDirtyNames() const
{
    QStringList names;
    if (m_templateTargetDirty) {
        names.append(QStringLiteral(
                         "目标字符内容"));
    }
    if (m_templateThresholdDirty) {
        names.append(QStringLiteral(
                         "图像合格阈值"));
    }
    return names;
}

// 函数说明：dirtyNames 函数实现名称所表示的处理步骤。
QStringList SettingsEditState::dirtyNames() const
{
    QStringList names = globalDirtyNames();
    const QStringList templateNames = templateDirtyNames();
    for (const QString &name : templateNames) {
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

// 函数说明：hasDirtySettings 函数检查相关状态并返回判断结果。
bool SettingsEditState::hasDirtySettings() const
{
    return !dirtyNames().isEmpty();
}

// 函数说明：dirtySettingsMessage 函数实现名称所表示的处理步骤。
QString SettingsEditState::dirtySettingsMessage() const
{
    const QStringList names = dirtyNames();
    if (names.isEmpty()) {
        return QString();
    }
    QStringList lines;
    for (const QString &name : names) {
        lines.append(QStringLiteral("- %1").arg(name));
    }
    return QStringLiteral(
                "存在未应用参数：\n\n%1\n\n"
                "继续运行将放弃以上未应用修改，"
                "并使用之前已设置的参数。")
            .arg(lines.join(QLatin1Char('\n')));
}
