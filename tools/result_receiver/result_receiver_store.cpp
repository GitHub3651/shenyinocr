#include "result_receiver_store.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

ResultReceiverStore::ResultReceiverStore()
{
}

void ResultReceiverStore::setOutputDirectory(const QString &directory)
{
    m_outputDirectory = QDir::cleanPath(
                QFileInfo(directory.trimmed()).absoluteFilePath());
    m_loadedDate.clear();
    m_loadedIds.clear();
}

QString ResultReceiverStore::outputDirectory() const
{
    return m_outputDirectory;
}

bool ResultReceiverStore::acceptProduct(const QJsonObject &object,
                                        bool *duplicate,
                                        QString *errorMessage)
{
    if (duplicate) {
        *duplicate = false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    const QString date = dateForObject(object, errorMessage);
    if (date.isEmpty()) {
        return false;
    }
    if (!loadDateIndex(date, errorMessage)) {
        return false;
    }
    const QString id = object.value(QStringLiteral("id")).toString();
    if (m_loadedIds.contains(id)) {
        if (duplicate) {
            *duplicate = true;
        }
        return true;
    }
    const QString path = jsonlPath(date);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact)
            + '\n';
    if (file.write(line) != line.size() || !file.flush()) {
        if (errorMessage) {
            *errorMessage = file.errorString().isEmpty()
                    ? QStringLiteral("JSONL 写入失败。") : file.errorString();
        }
        return false;
    }
    m_loadedIds.insert(id);
    return true;
}

bool ResultReceiverStore::synchronizeAllCsv(QStringList *successDates,
                                            QStringList *failedDates,
                                            QString *errorMessage)
{
    if (successDates) {
        successDates->clear();
    }
    if (failedDates) {
        failedDates->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    if (m_outputDirectory.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("输出目录为空。" );
        }
        return false;
    }
    const QStringList files = QDir(m_outputDirectory).entryList(
                QStringList() << QStringLiteral("qr_results_*.jsonl"),
                QDir::Files, QDir::Name);
    bool allOk = true;
    for (const QString &fileName : files) {
        const QString date = fileName.mid(11, 8);
        QString error;
        if (date.size() != 8 || !materializeDate(date, &error)) {
            allOk = false;
            if (failedDates) {
                failedDates->append(date.isEmpty() ? fileName : date);
            }
            if (errorMessage && errorMessage->isEmpty()) {
                *errorMessage = error;
            }
        } else if (successDates) {
            successDates->append(date);
        }
    }
    return allOk;
}

QString ResultReceiverStore::dateForObject(const QJsonObject &object,
                                           QString *errorMessage) const
{
    const QJsonValue timeValue = object.value(QStringLiteral("time"));
    if (!timeValue.isString()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("time 字段无效。" );
        }
        return QString();
    }
    const QString text = timeValue.toString();
    if (!text.endsWith(QLatin1Char('Z'))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("time 必须使用 UTC Z。" );
        }
        return QString();
    }
    const QDateTime utc = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!utc.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("time 不是有效的 ISO-8601 时间。" );
        }
        return QString();
    }
    return utc.toUTC().toLocalTime().date().toString(QStringLiteral("yyyyMMdd"));
}

QString ResultReceiverStore::jsonlPath(const QString &date) const
{
    return QDir(m_outputDirectory).filePath(
                QStringLiteral("qr_results_%1.jsonl").arg(date));
}

QString ResultReceiverStore::csvPath(const QString &date) const
{
    return QDir(m_outputDirectory).filePath(
                QStringLiteral("qr_results_%1.csv").arg(date));
}

bool ResultReceiverStore::loadDateIndex(const QString &date,
                                        QString *errorMessage)
{
    if (m_loadedDate == date) {
        return true;
    }
    QList<QJsonObject> records;
    if (!readDateRecords(date, &records, errorMessage, true)) {
        return false;
    }
    QSet<QString> ids;
    for (const QJsonObject &record : records) {
        ids.insert(record.value(QStringLiteral("id")).toString());
    }
    m_loadedDate = date;
    m_loadedIds = ids;
    return true;
}

bool ResultReceiverStore::readDateRecords(const QString &date,
                                          QList<QJsonObject> *records,
                                          QString *errorMessage,
                                          bool repairTrailingLine)
{
    if (records) {
        records->clear();
    }
    QFile file(jsonlPath(date));
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    const QByteArray data = file.readAll();
    const bool completeFile = data.isEmpty() || data.endsWith('\n');
    QByteArray parseData = data;
    if (!completeFile && repairTrailingLine) {
        const int lastLf = data.lastIndexOf('\n');
        const QString backup = file.fileName() + QStringLiteral(".bak");
        if (!QFile::exists(backup) && !QFile::copy(file.fileName(), backup)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("JSONL 尾行备份失败。" );
            }
            return false;
        }
        file.close();
        QFile repair(file.fileName());
        if (!repair.open(QIODevice::ReadWrite) || !repair.resize(lastLf + 1)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("JSONL 尾行截断失败。" );
            }
            return false;
        }
        parseData = lastLf >= 0 ? data.left(lastLf + 1) : QByteArray();
    }
    const QList<QByteArray> lines = parseData.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError
                || !document.isObject()
                || !document.object().value(QStringLiteral("id")).isString()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("JSONL 中间记录损坏。" );
            }
            return false;
        }
        if (records) {
            records->append(document.object());
        }
    }
    return true;
}

bool ResultReceiverStore::materializeDate(const QString &date,
                                          QString *errorMessage)
{
    QList<QJsonObject> records;
    if (!readDateRecords(date, &records, errorMessage, true)) {
        return false;
    }
    QSaveFile file(csvPath(date));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    const QByteArray bom("\xEF\xBB\xBF", 3);
    if (file.write(bom) != bom.size()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    for (const QJsonObject &record : records) {
        const bool ok = record.value(QStringLiteral("overallOk")).toBool(false);
        const QString value = ok
                ? record.value(QStringLiteral("qrContent")).toString()
                : QStringLiteral("noQR");
        const QByteArray line = csvEscape(value).toUtf8() + '\n';
        if (file.write(line) != line.size()) {
            if (errorMessage) {
                *errorMessage = file.errorString();
            }
            file.cancelWriting();
            return false;
        }
    }
    if (!file.flush() || !file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString().isEmpty()
                    ? QStringLiteral("CSV 原子替换失败。") : file.errorString();
        }
        return false;
    }
    return true;
}

QString ResultReceiverStore::csvEscape(const QString &value)
{
    QString escaped = value;
    const bool needsQuotes = escaped.contains(QLatin1Char(','))
            || escaped.contains(QLatin1Char('\n'))
            || escaped.contains(QLatin1Char('\r'))
            || escaped.contains(QLatin1Char('"'));
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    if (needsQuotes) {
        escaped = QStringLiteral("\"") + escaped + QStringLiteral("\"");
    }
    return escaped;
}
