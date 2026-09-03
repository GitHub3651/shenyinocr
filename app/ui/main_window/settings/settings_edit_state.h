// 文件作用：本文件用于跟踪界面设置项的已应用值和未应用修改。
// 主要职责：跟踪界面设置项的已应用值和未应用修改。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

// 组件说明：SettingsEditState 组件封装本文件中与其名称对应的单一职责。
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
