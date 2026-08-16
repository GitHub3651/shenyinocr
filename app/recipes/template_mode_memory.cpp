#include "template_mode_memory.h"
#include "recipes/product_recipe.h"

QString TemplateModeMemory::modeIdForIndex(int index)
{
    static const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    const int modeCount = static_cast<int>(
                sizeof(modes) / sizeof(modes[0]));
    return detectionModeUiId(
                index >= 0 && index < modeCount
                ? modes[index]
                : DetectionMode::Word);
}

QMap<QString, QString> &TemplateModeMemory::publishedRecipeIdsByMode()
{
    return m_publishedRecipeIdsByMode;
}

const QMap<QString, QString> &
TemplateModeMemory::publishedRecipeIdsByMode() const
{
    return m_publishedRecipeIdsByMode;
}
