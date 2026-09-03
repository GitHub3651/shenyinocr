// 文件作用：本文件用于跟踪界面设置项的已应用值和未应用修改。
// 主要职责：跟踪界面设置项的已应用值和未应用修改。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "settings_edit_state.h"

namespace {

QString normalizedDisplayName(const QString &displayName,
                              const QString &fallback)
{
    QString normalized = displayName.trimmed();
    normalized.remove(QLatin1Char(':'));
    normalized.remove(QChar(0xff1a));
    return normalized.isEmpty() ? fallback : normalized;
}

} // namespace

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

bool SettingsEditState::isGlobalDirty(const QString &key) const
{
    const auto it = m_globalSettings.constFind(key);
    return it != m_globalSettings.constEnd() && it.value().dirty;
}

void SettingsEditState::setGlobalDirty(const QString &key, bool dirty)
{
    auto it = m_globalSettings.find(key);
    if (it != m_globalSettings.end()) {
        it.value().dirty = dirty;
    }
}

void SettingsEditState::clearAllGlobalDirty()
{
    for (auto it = m_globalSettings.begin();
         it != m_globalSettings.end();
         ++it) {
        it.value().dirty = false;
    }
}

void SettingsEditState::setTemplateTargetDirty(bool dirty)
{
    m_templateTargetDirty = dirty;
}

void SettingsEditState::setTemplateThresholdDirty(bool dirty)
{
    m_templateThresholdDirty = dirty;
}

bool SettingsEditState::isTemplateTargetDirty() const
{
    return m_templateTargetDirty;
}

bool SettingsEditState::isTemplateThresholdDirty() const
{
    return m_templateThresholdDirty;
}

void SettingsEditState::clearTemplateDirty()
{
    m_templateTargetDirty = false;
    m_templateThresholdDirty = false;
}

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

bool SettingsEditState::hasDirtySettings() const
{
    return !dirtyNames().isEmpty();
}

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
