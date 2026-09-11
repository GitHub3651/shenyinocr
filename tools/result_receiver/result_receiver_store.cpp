#include "result_receiver_store.h"

#include <QDateTime>
#include <QDebug>
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
        qWarning().noquote()
                << QStringLiteral("result record open failed: path=%1 error=%2")
                   .arg(path, file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "结果记录保存失败，请检查保存文件夹权限和磁盘空间。");
        }
        return false;
    }
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact)
            + '\n';
    if (file.write(line) != line.size() || !file.flush()) {
        qWarning().noquote()
                << QStringLiteral("result record write failed: path=%1 error=%2")
                   .arg(path, file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "结果记录保存失败，请检查保存文件夹权限和磁盘空间。");
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
            *errorMessage = QStringLiteral("请先选择结果保存文件夹。" );
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
        qWarning().noquote()
                << QStringLiteral("invalid result time type: type=%1")
                   .arg(static_cast<int>(timeValue.type()));
        if (errorMessage) {
            *errorMessage = QStringLiteral("结果记录中的时间格式无效。" );
        }
        return QString();
    }
    const QString text = timeValue.toString();
    if (!text.endsWith(QLatin1Char('Z'))) {
        qWarning().noquote()
                << QStringLiteral("invalid result time zone: value=%1")
                   .arg(text);
        if (errorMessage) {
            *errorMessage = QStringLiteral("结果记录中的时间格式无效。" );
        }
        return QString();
    }
    const QDateTime utc = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!utc.isValid()) {
        qWarning().noquote()
                << QStringLiteral("invalid result time value: value=%1")
                   .arg(text);
        if (errorMessage) {
            *errorMessage = QStringLiteral("结果记录中的时间格式无效。" );
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
        qWarning().noquote()
                << QStringLiteral("result record read failed: path=%1 error=%2")
                   .arg(file.fileName(), file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "结果记录保存失败，请检查保存文件夹权限和磁盘空间。");
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
            qWarning().noquote()
                    << QStringLiteral("result record tail backup failed: source=%1 backup=%2")
                       .arg(file.fileName(), backup);
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法备份不完整的结果记录。" );
            }
            return false;
        }
        file.close();
        QFile repair(file.fileName());
        if (!repair.open(QIODevice::ReadWrite) || !repair.resize(lastLf + 1)) {
            qWarning().noquote()
                    << QStringLiteral("result record tail repair failed: path=%1 error=%2")
                       .arg(repair.fileName(), repair.errorString());
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法修复不完整的结果记录。" );
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
            qWarning().noquote()
                    << QStringLiteral(
                        "result record parse failed: path=%1 line=%2 offset=%3 error=%4 object=%5 idString=%6")
                       .arg(file.fileName())
                       .arg(i + 1)
                       .arg(parseError.offset)
                       .arg(parseError.errorString())
                       .arg(document.isObject())
                       .arg(document.object()
                            .value(QStringLiteral("id")).isString());
            if (errorMessage) {
                *errorMessage = QStringLiteral("结果记录已损坏，无法生成 CSV 文件。" );
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
        qWarning().noquote()
                << QStringLiteral("CSV open failed: path=%1 error=%2")
                   .arg(file.fileName(), file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "CSV 文件保存失败，请检查保存文件夹权限和磁盘空间。");
        }
        return false;
    }
    const QByteArray bom("\xEF\xBB\xBF", 3);
    if (file.write(bom) != bom.size()) {
        qWarning().noquote()
                << QStringLiteral("CSV BOM write failed: path=%1 error=%2")
                   .arg(file.fileName(), file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "CSV 文件保存失败，请检查保存文件夹权限和磁盘空间。");
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
            qWarning().noquote()
                    << QStringLiteral("CSV row write failed: path=%1 error=%2")
                       .arg(file.fileName(), file.errorString());
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "CSV 文件保存失败，请检查保存文件夹权限和磁盘空间。");
            }
            file.cancelWriting();
            return false;
        }
    }
    if (!file.flush() || !file.commit()) {
        qWarning().noquote()
                << QStringLiteral("CSV atomic replace failed: path=%1 error=%2")
                   .arg(file.fileName(), file.errorString());
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "CSV 文件保存失败，原文件保持不变。");
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
