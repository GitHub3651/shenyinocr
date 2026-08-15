#include "template_mode_memory.h"

QString TemplateModeMemory::modeIdForIndex(int index)
{
    static const QStringList modeIds = {
        QStringLiteral("stamp_detection"),
        QStringLiteral("word_detection"),
        QStringLiteral("ocr_detection"),
        QStringLiteral("tissue_detection"),
        QStringLiteral("barcode_word_detection")
    };
    return index >= 0 && index < modeIds.size()
            ? modeIds.at(index)
            : QStringLiteral("word_detection");
}

QMap<QString, QStringList> &TemplateModeMemory::templatePathsByMode()
{
    return m_templatePathsByMode;
}

const QMap<QString, QStringList> &
TemplateModeMemory::templatePathsByMode() const
{
    return m_templatePathsByMode;
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
