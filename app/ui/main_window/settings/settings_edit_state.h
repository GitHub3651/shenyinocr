#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

class SettingsEditState
{
public:
    void registerGlobalSetting(const QString &key,
                               const QString &displayName);
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
    // 组件说明：Entry 数据结构集中保存该流程需要的一组相关数据。
    struct Entry
    {
        QString displayName;
        bool dirty = false;
    };

    QMap<QString, Entry> m_globalSettings;
    bool m_templateTargetDirty = false;
    bool m_templateThresholdDirty = false;
};
