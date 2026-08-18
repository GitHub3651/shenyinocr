// 文件作用：本文件用于记录各检测模式最近使用的配方，并在模式切换时恢复选择。
// 主要职责：记录各检测模式最近使用的配方，并在模式切换时恢复选择。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QMap>
#include <QString>

// 组件说明：TemplateModeMemory 组件封装本文件中与其名称对应的单一职责。
class TemplateModeMemory
{
public:
    static QString modeIdForIndex(int index);

    QMap<QString, QString> &publishedRecipeIdsByMode();
    const QMap<QString, QString> &publishedRecipeIdsByMode() const;

private:
    QMap<QString, QString> m_publishedRecipeIdsByMode;
};
