// 文件作用：本文件用于记录各检测模式最近使用的配方，并在模式切换时恢复选择。
// 主要职责：记录各检测模式最近使用的配方，并在模式切换时恢复选择。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "template_mode_memory.h"
#include "recipes/product_recipe.h"

// 函数说明：modeIdForIndex 函数实现名称所表示的处理步骤。
QString TemplateModeMemory::modeIdForIndex(int index)
{
    const QVector<DetectionModeDescriptor> &descriptors =
            detectionModeDescriptors();
    return index >= 0 && index < descriptors.size()
            ? QLatin1String(descriptors.at(index).uiId)
            : detectionModeUiId(DetectionMode::Word);
}

QMap<QString, QString> &TemplateModeMemory::publishedRecipeIdsByMode()
{
    return m_publishedRecipeIdsByMode;
}

const QMap<QString, QString> &
// 函数说明：publishedRecipeIdsByMode 函数保存或发布对应的数据和资源。
TemplateModeMemory::publishedRecipeIdsByMode() const
{
    return m_publishedRecipeIdsByMode;
}
