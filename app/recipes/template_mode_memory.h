#pragma once

#include <QMap>
#include <QString>

class TemplateModeMemory
{
public:
    static QString modeIdForIndex(int index);

    QMap<QString, QString> &publishedRecipeIdsByMode();
    const QMap<QString, QString> &publishedRecipeIdsByMode() const;

private:
    QMap<QString, QString> m_publishedRecipeIdsByMode;
};
