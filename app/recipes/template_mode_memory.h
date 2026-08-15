#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

class TemplateModeMemory
{
public:
    static QString modeIdForIndex(int index);

    QMap<QString, QStringList> &templatePathsByMode();
    const QMap<QString, QStringList> &templatePathsByMode() const;
    QMap<QString, QString> &publishedRecipeIdsByMode();
    const QMap<QString, QString> &publishedRecipeIdsByMode() const;

private:
    QMap<QString, QStringList> m_templatePathsByMode;
    QMap<QString, QString> m_publishedRecipeIdsByMode;
};
