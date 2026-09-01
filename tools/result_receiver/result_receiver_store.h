#pragma once

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class ResultReceiverStore
{
public:
    ResultReceiverStore();

    void setOutputDirectory(const QString &directory);
    QString outputDirectory() const;

    bool acceptProduct(const QJsonObject &object,
                       bool *duplicate,
                       QString *errorMessage);
    bool synchronizeAllCsv(QStringList *successDates,
                           QStringList *failedDates,
                           QString *errorMessage);

private:
    QString dateForObject(const QJsonObject &object,
                          QString *errorMessage) const;
    QString jsonlPath(const QString &date) const;
    QString csvPath(const QString &date) const;
    bool loadDateIndex(const QString &date, QString *errorMessage);
    bool readDateRecords(const QString &date,
                         QList<QJsonObject> *records,
                         QString *errorMessage,
                         bool repairTrailingLine);
    bool materializeDate(const QString &date, QString *errorMessage);
    static QString csvEscape(const QString &value);

    QString m_outputDirectory;
    QString m_loadedDate;
    QSet<QString> m_loadedIds;
};
