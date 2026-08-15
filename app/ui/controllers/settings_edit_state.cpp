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

bool SettingsEditState::hasGlobalSetting(const QString &key) const
{
    return m_globalSettings.contains(key);
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
                         "\u76ee\u6807\u5b57\u7b26\u5185\u5bb9"));
    }
    if (m_templateThresholdDirty) {
        names.append(QStringLiteral(
                         "\u56fe\u50cf\u5408\u683c\u9608\u503c"));
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
                "\u5b58\u5728\u672a\u5e94\u7528\u53c2\u6570\uff1a\n\n%1\n\n"
                "\u7ee7\u7eed\u8fd0\u884c\u5c06\u653e\u5f03\u4ee5\u4e0a\u672a\u5e94\u7528\u4fee\u6539\uff0c"
                "\u5e76\u4f7f\u7528\u4e4b\u524d\u5df2\u8bbe\u7f6e\u7684\u53c2\u6570\u3002")
            .arg(lines.join(QLatin1Char('\n')));
}
