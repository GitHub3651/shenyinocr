#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

class SettingsEditState
{
public:
    void registerGlobalSetting(const QString &key,
                               const QString &displayName);
    bool hasGlobalSetting(const QString &key) const;
    bool isGlobalDirty(const QString &key) const;
    void setGlobalDirty(const QString &key, bool dirty);
    void clearAllGlobalDirty();

    void setTemplateTargetDirty(bool dirty);
    void setTemplateThresholdDirty(bool dirty);
    bool isTemplateTargetDirty() const;
    bool isTemplateThresholdDirty() const;
    void clearTemplateDirty();

    QStringList globalDirtyNames() const;
    QStringList templateDirtyNames() const;
    QStringList dirtyNames() const;
    bool hasDirtySettings() const;
    QString dirtySettingsMessage() const;

private:
    struct Entry
    {
        QString displayName;
        bool dirty = false;
    };

    QMap<QString, Entry> m_globalSettings;
    bool m_templateTargetDirty = false;
    bool m_templateThresholdDirty = false;
};
