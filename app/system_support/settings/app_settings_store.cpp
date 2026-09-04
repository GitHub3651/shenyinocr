// 文件作用：实现 AppSettings Schema 7 的唯一磁盘入口。
#include "system_support/settings/app_settings_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <cmath>
#include <limits>

namespace {

void clearError(AppSettingsStoreError *error)
{
    if (error) {
        *error = AppSettingsStoreError();
    }
}

bool fail(AppSettingsStoreError *error,
          const QString &code,
          const QString &userMessage,
          const QString &diagnostic)
{
    if (error) {
        error->code = code;
        error->userMessage = userMessage;
        error->diagnostic = diagnostic;
    }
    return false;
}

bool hasOnlyKeys(const QJsonObject &object,
                 const QStringList &allowed,
                 const QString &context,
                 AppSettingsStoreError *error)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            return fail(error,
                        QStringLiteral("SETTINGS_SCHEMA_UNSUPPORTED"),
                        QStringLiteral("设置文件包含不支持的字段。"),
                        QStringLiteral("Unsupported %1 field: %2")
                        .arg(context, it.key()));
        }
    }
    return true;
}

bool readObject(const QJsonObject &parent,
                const char *key,
                QJsonObject *value,
                AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (!field.isObject()) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少对象或对象类型不正确。"),
                    QStringLiteral("Expected object: %1")
                    .arg(QLatin1String(key)));
    }
    *value = field.toObject();
    return true;
}

bool readString(const QJsonObject &parent,
                const char *key,
                QString *value,
                AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (!field.isString()) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少文本字段或字段类型不正确。"),
                    QStringLiteral("Expected string: %1")
                    .arg(QLatin1String(key)));
    }
    *value = field.toString();
    return true;
}

bool readBool(const QJsonObject &parent,
              const char *key,
              bool *value,
              AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (!field.isBool()) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少布尔字段或字段类型不正确。"),
                    QStringLiteral("Expected bool: %1")
                    .arg(QLatin1String(key)));
    }
    *value = field.toBool();
    return true;
}

bool readInt(const QJsonObject &parent,
             const char *key,
             int *value,
             AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    const double number = field.toDouble(
                std::numeric_limits<double>::quiet_NaN());
    if (!field.isDouble() || !std::isfinite(number)
            || std::floor(number) != number
            || number < std::numeric_limits<int>::min()
            || number > std::numeric_limits<int>::max()) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少整数或字段类型不正确。"),
                    QStringLiteral("Expected int: %1")
                    .arg(QLatin1String(key)));
    }
    *value = static_cast<int>(number);
    return true;
}

bool readDouble(const QJsonObject &parent,
                const char *key,
                double *value,
                AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    const double number = field.toDouble(
                std::numeric_limits<double>::quiet_NaN());
    if (!field.isDouble() || !std::isfinite(number)) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少数值或字段类型不正确。"),
                    QStringLiteral("Expected number: %1")
                    .arg(QLatin1String(key)));
    }
    *value = number;
    return true;
}

bool readStringList(const QJsonObject &parent,
                    const char *key,
                    QStringList *value,
                    AppSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (!field.isArray()) {
        return fail(error,
                    field.isUndefined()
                    ? QStringLiteral("SETTINGS_FIELD_MISSING")
                    : QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件缺少路径列表或字段类型不正确。"),
                    QStringLiteral("Expected array: %1")
                    .arg(QLatin1String(key)));
    }
    QStringList result;
    for (const QJsonValue &item : field.toArray()) {
        if (!item.isString()) {
            return fail(error,
                        QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                        QStringLiteral("模板路径列表中存在非文本值。"),
                        QStringLiteral("Non-string item in %1")
                        .arg(QLatin1String(key)));
        }
        result.append(item.toString());
    }
    *value = result;
    return true;
}

QStringList imageSaveRangeJsonIds()
{
    return QStringList() << QStringLiteral("none")
                         << QStringLiteral("ngOnly")
                         << QStringLiteral("okOnly")
                         << QStringLiteral("all");
}

QStringList imageSaveContentJsonIds()
{
    return QStringList() << QStringLiteral("annotatedAndRaw")
                         << QStringLiteral("annotatedOnly")
                         << QStringLiteral("rawOnly");
}

QStringList rotationJsonIds()
{
    return QStringList() << QStringLiteral("none")
                         << QStringLiteral("clockwise90")
                         << QStringLiteral("counterclockwise90")
                         << QStringLiteral("rotate180");
}

QStringList triggerModeJsonIds()
{
    return QStringList() << QStringLiteral("continuous")
                         << QStringLiteral("intermittent");
}

QString mappedId(const QString &source,
                 const QStringList &sourceIds,
                 const QStringList &targetIds)
{
    const int index = sourceIds.indexOf(source);
    return index >= 0 && index < targetIds.size()
            ? targetIds.at(index) : QString();
}

bool mapId(const QString &source,
           const QStringList &sourceIds,
           const QStringList &targetIds,
           const QString &field,
           QString *target,
           AppSettingsStoreError *error)
{
    const QString mapped = mappedId(source, sourceIds, targetIds);
    if (mapped.isEmpty()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("设置文件包含不支持的选项。"),
                    QStringLiteral("%1=%2").arg(field, source));
    }
    *target = mapped;
    return true;
}

bool validTemplatePath(const QString &path)
{
    return path.isEmpty()
            || (QFileInfo(path).isAbsolute()
                && QDir::cleanPath(path) == path);
}

bool validTemplatePathList(const QStringList &paths)
{
    QSet<QString> seen;
    for (const QString &path : paths) {
        if (!validTemplatePath(path) || path.isEmpty()) {
            return false;
        }
        const QString key = path.toCaseFolded();
        if (seen.contains(key)) {
            return false;
        }
        seen.insert(key);
    }
    return true;
}

bool validateSettings(const AppSettings &settings,
                      AppSettingsStoreError *error)
{
    const AppSettings defaults = AppSettings::defaults();
    if (settings.schemaVersion != AppSettings::CurrentSchemaVersion) {
        return fail(error,
                    QStringLiteral("SETTINGS_SCHEMA_UNSUPPORTED"),
                    QStringLiteral("设置文件版本不受支持。"),
                    QStringLiteral("schemaVersion=%1")
                    .arg(settings.schemaVersion));
    }
    if (!appSettingsDetectionModeIds().contains(settings.detectModeId)
            || !appSettingsImageSaveModeIds().contains(settings.imageSaveModeId)
            || !appSettingsImageSaveTypeIds().contains(settings.imageSaveTypeId)
            || !appSettingsColorChannelIds().contains(settings.colorChannelId)
            || !appSettingsRotationIds().contains(settings.imageRotationId)
            || !appSettingsTriggerModeIds().contains(settings.triggerModeId)) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("设置文件包含不支持的选项。"),
                    QStringLiteral("One or more enum ids are invalid."));
    }
    const int nonNegativeValues[] = {
        settings.cameraExposure, settings.cameraGain, settings.cameraDelay,
        settings.plcRack, settings.plcSlot, settings.photoDistance,
        settings.photoTime, settings.rejectDistance, settings.rejectTime,
        settings.rejectPosition
    };
    for (int value : nonNegativeValues) {
        if (value < 0) {
            return fail(error,
                        QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                        QStringLiteral("设置数值超出合法范围。"),
                        QStringLiteral("Negative numeric field."));
        }
    }
    if (settings.photoTime > 65535 || settings.rejectTime > 65535
            || settings.plcIp.trimmed().isEmpty()) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("设置字段组合不符合设备合同。"),
                    QStringLiteral("PLC word or IP contract invalid."));
    }
    if (settings.imageSaveModeId != QLatin1String("save_none")
            && (settings.imageSavePath.trimmed().isEmpty()
                || !QFileInfo(settings.imageSavePath).isAbsolute())) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("启用存图时必须选择绝对输出目录。"),
                    QStringLiteral("imageSaving.outputDirectory invalid."));
    }
    const QString barcodeCsvDirectory =
            settings.barcodeCsvOutputDirectory.trimmed();
    if ((settings.barcodeCsvEnabled && barcodeCsvDirectory.isEmpty())
            || (!barcodeCsvDirectory.isEmpty()
                && !QFileInfo(barcodeCsvDirectory).isAbsolute())) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("启用本机 CSV 时必须选择绝对输出目录。"),
                    QStringLiteral("barcodeCsv.outputDirectory invalid."));
    }
    if (!validTemplatePath(settings.templateSaveDirectory)) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("模板保存目录必须是规范化绝对路径。"),
                    QStringLiteral("ui.templateSaveDirectory invalid."));
    }
    if (settings.plcTriggerModeDb != defaults.plcTriggerModeDb
            || settings.plcTriggerModeOffset != defaults.plcTriggerModeOffset
            || settings.plcResultDb != defaults.plcResultDb
            || settings.plcResultOffset != defaults.plcResultOffset
            || settings.plcPhotoDistanceOffset != defaults.plcPhotoDistanceOffset
            || settings.plcPhotoTimeOffset != defaults.plcPhotoTimeOffset
            || settings.plcRejectDistanceOffset != defaults.plcRejectDistanceOffset
            || settings.plcRejectTimeOffset != defaults.plcRejectTimeOffset) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("PLC 地址与当前设备合同不一致。"),
                    QStringLiteral("Fixed PLC address contract changed."));
    }
    const DetectionSchemes &schemes = settings.detectionSchemes;
    if (!validTemplatePath(schemes.stampTemplatePath)
            || !validTemplatePathList(schemes.wordTemplatePaths)
            || !validTemplatePath(schemes.ocrTemplatePath)
            || !validTemplatePathList(schemes.barcodeWordTemplatePaths)
            || !std::isfinite(schemes.tissueRoughnessThreshold)
            || schemes.tissueRoughnessThreshold < 0.0) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("检测方案中的模板路径或纸巾阈值无效。"),
                    QStringLiteral("Invalid detectionSchemes value."));
    }
    return true;
}

QJsonArray toJsonArray(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

QJsonObject settingsToJson(const AppSettings &settings)
{
    QJsonObject camera;
    camera.insert(QStringLiteral("exposureMicroseconds"), settings.cameraExposure);
    camera.insert(QStringLiteral("gain"), settings.cameraGain);
    camera.insert(QStringLiteral("triggerSource"), settings.triggerEnabled
                  ? QStringLiteral("hardwareLine0") : QStringLiteral("software"));
    camera.insert(QStringLiteral("rotation"),
                  mappedId(settings.imageRotationId,
                           appSettingsRotationIds(), rotationJsonIds()));
    camera.insert(QStringLiteral("colorChannel"), settings.colorChannelId);

    QJsonObject inspection;
    inspection.insert(QStringLiteral("minimumIntervalMs"), settings.cameraDelay);

    QJsonObject connection;
    connection.insert(QStringLiteral("ip"), settings.plcIp);
    connection.insert(QStringLiteral("rack"), settings.plcRack);
    connection.insert(QStringLiteral("slot"), settings.plcSlot);
    QJsonObject process;
    process.insert(QStringLiteral("triggerMode"),
                   mappedId(settings.triggerModeId,
                            appSettingsTriggerModeIds(), triggerModeJsonIds()));
    process.insert(QStringLiteral("photoDistanceMm"), settings.photoDistance);
    process.insert(QStringLiteral("photoTimeMs"), settings.photoTime);
    process.insert(QStringLiteral("rejectDistanceMm"), settings.rejectDistance);
    process.insert(QStringLiteral("rejectTimeMs"), settings.rejectTime);
    process.insert(QStringLiteral("rejectPosition"), settings.rejectPosition);
    auto address = [](int db, int offset) {
        QJsonObject object;
        object.insert(QStringLiteral("db"), db);
        object.insert(QStringLiteral("byteOffset"), offset);
        return object;
    };
    QJsonObject addresses;
    addresses.insert(QStringLiteral("triggerMode"),
                     address(settings.plcTriggerModeDb, settings.plcTriggerModeOffset));
    addresses.insert(QStringLiteral("result"),
                     address(settings.plcResultDb, settings.plcResultOffset));
    addresses.insert(QStringLiteral("photoDistanceDb"), settings.plcPhotoDistanceOffset);
    addresses.insert(QStringLiteral("photoTimeDb"), settings.plcPhotoTimeOffset);
    addresses.insert(QStringLiteral("rejectDistanceDb"), settings.plcRejectDistanceOffset);
    addresses.insert(QStringLiteral("rejectTimeDb"), settings.plcRejectTimeOffset);
    QJsonObject plc;
    plc.insert(QStringLiteral("connection"), connection);
    plc.insert(QStringLiteral("process"), process);
    plc.insert(QStringLiteral("addresses"), addresses);

    QJsonObject imageSaving;
    imageSaving.insert(QStringLiteral("range"),
                       mappedId(settings.imageSaveModeId,
                                appSettingsImageSaveModeIds(), imageSaveRangeJsonIds()));
    imageSaving.insert(QStringLiteral("content"),
                       mappedId(settings.imageSaveTypeId,
                                appSettingsImageSaveTypeIds(), imageSaveContentJsonIds()));
    imageSaving.insert(QStringLiteral("outputDirectory"), settings.imageSavePath);

    DetectionMode selectedMode = DetectionMode::Word;
    detectionModeFromUiId(settings.detectModeId, &selectedMode);
    QJsonObject ui;
    ui.insert(QStringLiteral("selectedDetectionMode"), detectionModeId(selectedMode));
    ui.insert(QStringLiteral("templateSaveDirectory"),
              settings.templateSaveDirectory);

    const DetectionSchemes &schemes = settings.detectionSchemes;
    QJsonObject stamp;
    stamp.insert(QStringLiteral("templatePath"),
                 schemes.stampTemplatePath);
    QJsonObject word;
    word.insert(QStringLiteral("templatePaths"),
                toJsonArray(schemes.wordTemplatePaths));
    QJsonObject ocr;
    ocr.insert(QStringLiteral("templatePath"),
               schemes.ocrTemplatePath);
    QJsonObject tissue;
    tissue.insert(QStringLiteral("roughnessThreshold"),
                  schemes.tissueRoughnessThreshold);
    QJsonObject barcodeWord;
    barcodeWord.insert(QStringLiteral("templatePaths"),
                       toJsonArray(schemes.barcodeWordTemplatePaths));
    QJsonObject detectionSchemes;
    detectionSchemes.insert(QStringLiteral("stamp"), stamp);
    detectionSchemes.insert(QStringLiteral("word"), word);
    detectionSchemes.insert(QStringLiteral("ocr"), ocr);
    detectionSchemes.insert(QStringLiteral("tissue"), tissue);
    detectionSchemes.insert(QStringLiteral("barcodeWord"), barcodeWord);

    QJsonObject barcodeCsv;
    barcodeCsv.insert(QStringLiteral("enabled"),
                      settings.barcodeCsvEnabled);
    barcodeCsv.insert(QStringLiteral("outputDirectory"),
                      settings.barcodeCsvOutputDirectory);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), settings.schemaVersion);
    root.insert(QStringLiteral("camera"), camera);
    root.insert(QStringLiteral("inspection"), inspection);
    root.insert(QStringLiteral("plc"), plc);
    root.insert(QStringLiteral("imageSaving"), imageSaving);
    root.insert(QStringLiteral("ui"), ui);
    root.insert(QStringLiteral("detectionSchemes"), detectionSchemes);
    root.insert(QStringLiteral("barcodeCsv"), barcodeCsv);
    return root;
}

bool addressFromJson(const QJsonObject &parent,
                     const char *key,
                     int *db,
                     int *offset,
                     AppSettingsStoreError *error)
{
    QJsonObject object;
    return readObject(parent, key, &object, error)
            && hasOnlyKeys(object,
                           QStringList() << QStringLiteral("db")
                                         << QStringLiteral("byteOffset"),
                           QLatin1String(key), error)
            && readInt(object, "db", db, error)
            && readInt(object, "byteOffset", offset, error);
}

bool settingsFromJson(const QJsonObject &root,
                      AppSettings *settings,
                      AppSettingsStoreError *error)
{
    AppSettings candidate;
    QJsonObject camera, inspection, plc, connection, process, addresses;
    QJsonObject imageSaving, ui, detectionSchemes, barcodeCsv;
    QJsonObject stamp, word, ocr, tissue, barcodeWord;
    if (!hasOnlyKeys(root,
                     QStringList() << QStringLiteral("schemaVersion")
                                   << QStringLiteral("camera")
                                   << QStringLiteral("inspection")
                                   << QStringLiteral("plc")
                                   << QStringLiteral("imageSaving")
                                   << QStringLiteral("ui")
                                   << QStringLiteral("detectionSchemes")
                                   << QStringLiteral("barcodeCsv"),
                     QStringLiteral("root"), error)
            || !readInt(root, "schemaVersion", &candidate.schemaVersion, error)
            || !readObject(root, "camera", &camera, error)
            || !readObject(root, "inspection", &inspection, error)
            || !readObject(root, "plc", &plc, error)
            || !readObject(plc, "connection", &connection, error)
            || !readObject(plc, "process", &process, error)
            || !readObject(plc, "addresses", &addresses, error)
            || !readObject(root, "imageSaving", &imageSaving, error)
            || !readObject(root, "ui", &ui, error)
            || !readObject(root, "detectionSchemes", &detectionSchemes, error)
            || !readObject(root, "barcodeCsv", &barcodeCsv, error)
            || !readObject(detectionSchemes, "stamp", &stamp, error)
            || !readObject(detectionSchemes, "word", &word, error)
            || !readObject(detectionSchemes, "ocr", &ocr, error)
            || !readObject(detectionSchemes, "tissue", &tissue, error)
            || !readObject(detectionSchemes, "barcodeWord", &barcodeWord, error)) {
        return false;
    }
    if (!hasOnlyKeys(camera,
                     QStringList() << QStringLiteral("exposureMicroseconds")
                                   << QStringLiteral("gain")
                                   << QStringLiteral("triggerSource")
                                   << QStringLiteral("rotation")
                                   << QStringLiteral("colorChannel"),
                     QStringLiteral("camera"), error)
            || !hasOnlyKeys(inspection,
                            QStringList() << QStringLiteral("minimumIntervalMs"),
                            QStringLiteral("inspection"), error)
            || !hasOnlyKeys(plc,
                            QStringList() << QStringLiteral("connection")
                                          << QStringLiteral("process")
                                          << QStringLiteral("addresses"),
                            QStringLiteral("plc"), error)
            || !hasOnlyKeys(connection,
                            QStringList() << QStringLiteral("ip")
                                          << QStringLiteral("rack")
                                          << QStringLiteral("slot"),
                            QStringLiteral("plc.connection"), error)
            || !hasOnlyKeys(process,
                            QStringList() << QStringLiteral("triggerMode")
                                          << QStringLiteral("photoDistanceMm")
                                          << QStringLiteral("photoTimeMs")
                                          << QStringLiteral("rejectDistanceMm")
                                          << QStringLiteral("rejectTimeMs")
                                          << QStringLiteral("rejectPosition"),
                            QStringLiteral("plc.process"), error)
            || !hasOnlyKeys(addresses,
                            QStringList() << QStringLiteral("triggerMode")
                                          << QStringLiteral("result")
                                          << QStringLiteral("photoDistanceDb")
                                          << QStringLiteral("photoTimeDb")
                                          << QStringLiteral("rejectDistanceDb")
                                          << QStringLiteral("rejectTimeDb"),
                            QStringLiteral("plc.addresses"), error)
            || !hasOnlyKeys(imageSaving,
                            QStringList() << QStringLiteral("range")
                                          << QStringLiteral("content")
                                          << QStringLiteral("outputDirectory"),
                            QStringLiteral("imageSaving"), error)
            || !hasOnlyKeys(ui,
                            QStringList() << QStringLiteral("selectedDetectionMode")
                                          << QStringLiteral("templateSaveDirectory"),
                            QStringLiteral("ui"), error)
            || !hasOnlyKeys(barcodeCsv,
                            QStringList() << QStringLiteral("enabled")
                                          << QStringLiteral("outputDirectory"),
                            QStringLiteral("barcodeCsv"), error)
            || !hasOnlyKeys(detectionSchemes,
                            QStringList() << QStringLiteral("stamp")
                                          << QStringLiteral("word")
                                          << QStringLiteral("ocr")
                                          << QStringLiteral("tissue")
                                          << QStringLiteral("barcodeWord"),
                            QStringLiteral("detectionSchemes"), error)
            || !hasOnlyKeys(stamp,
                            QStringList() << QStringLiteral("templatePath"),
                            QStringLiteral("detectionSchemes.stamp"), error)
            || !hasOnlyKeys(word,
                            QStringList() << QStringLiteral("templatePaths"),
                            QStringLiteral("detectionSchemes.word"), error)
            || !hasOnlyKeys(ocr,
                            QStringList() << QStringLiteral("templatePath"),
                            QStringLiteral("detectionSchemes.ocr"), error)
            || !hasOnlyKeys(tissue,
                            QStringList() << QStringLiteral("roughnessThreshold"),
                            QStringLiteral("detectionSchemes.tissue"), error)
            || !hasOnlyKeys(barcodeWord,
                            QStringList() << QStringLiteral("templatePaths"),
                            QStringLiteral("detectionSchemes.barcodeWord"), error)) {
        return false;
    }

    QString triggerSource, rotation, colorChannel, triggerMode;
    QString imageRange, imageContent, selectedModeId;
    if (!readInt(camera, "exposureMicroseconds", &candidate.cameraExposure, error)
            || !readInt(camera, "gain", &candidate.cameraGain, error)
            || !readString(camera, "triggerSource", &triggerSource, error)
            || !readString(camera, "rotation", &rotation, error)
            || !readString(camera, "colorChannel", &colorChannel, error)
            || !readInt(inspection, "minimumIntervalMs", &candidate.cameraDelay, error)
            || !readString(connection, "ip", &candidate.plcIp, error)
            || !readInt(connection, "rack", &candidate.plcRack, error)
            || !readInt(connection, "slot", &candidate.plcSlot, error)
            || !readString(process, "triggerMode", &triggerMode, error)
            || !readInt(process, "photoDistanceMm", &candidate.photoDistance, error)
            || !readInt(process, "photoTimeMs", &candidate.photoTime, error)
            || !readInt(process, "rejectDistanceMm", &candidate.rejectDistance, error)
            || !readInt(process, "rejectTimeMs", &candidate.rejectTime, error)
            || !readInt(process, "rejectPosition", &candidate.rejectPosition, error)
            || !addressFromJson(addresses, "triggerMode", &candidate.plcTriggerModeDb,
                                &candidate.plcTriggerModeOffset, error)
            || !addressFromJson(addresses, "result", &candidate.plcResultDb,
                                &candidate.plcResultOffset, error)
            || !readInt(addresses, "photoDistanceDb", &candidate.plcPhotoDistanceOffset, error)
            || !readInt(addresses, "photoTimeDb", &candidate.plcPhotoTimeOffset, error)
            || !readInt(addresses, "rejectDistanceDb", &candidate.plcRejectDistanceOffset, error)
            || !readInt(addresses, "rejectTimeDb", &candidate.plcRejectTimeOffset, error)
            || !readString(imageSaving, "range", &imageRange, error)
            || !readString(imageSaving, "content", &imageContent, error)
            || !readString(imageSaving, "outputDirectory", &candidate.imageSavePath, error)
            || !readString(ui, "selectedDetectionMode", &selectedModeId, error)
            || !readString(ui, "templateSaveDirectory",
                           &candidate.templateSaveDirectory, error)
            || !readBool(barcodeCsv, "enabled",
                         &candidate.barcodeCsvEnabled, error)
            || !readString(barcodeCsv, "outputDirectory",
                           &candidate.barcodeCsvOutputDirectory, error)
            || !readString(stamp, "templatePath",
                           &candidate.detectionSchemes.stampTemplatePath, error)
            || !readStringList(word, "templatePaths",
                               &candidate.detectionSchemes.wordTemplatePaths, error)
            || !readString(ocr, "templatePath",
                           &candidate.detectionSchemes.ocrTemplatePath, error)
            || !readDouble(tissue, "roughnessThreshold",
                           &candidate.detectionSchemes.tissueRoughnessThreshold, error)
            || !readStringList(barcodeWord, "templatePaths",
                               &candidate.detectionSchemes.barcodeWordTemplatePaths, error)) {
        return false;
    }

    if (triggerSource == QLatin1String("hardwareLine0")) {
        candidate.triggerEnabled = true;
    } else if (triggerSource == QLatin1String("software")) {
        candidate.triggerEnabled = false;
    } else {
        return fail(error, QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("相机触发源不受支持。"), triggerSource);
    }
    DetectionMode selectedMode;
    if (!detectionModeFromId(selectedModeId, &selectedMode)) {
        return fail(error, QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("检测模式不受支持。"), selectedModeId);
    }
    candidate.detectModeId = detectionModeUiId(selectedMode);
    candidate.colorChannelId = colorChannel;
    if (!mapId(rotation, rotationJsonIds(), appSettingsRotationIds(),
               QStringLiteral("camera.rotation"), &candidate.imageRotationId, error)
            || !mapId(triggerMode, triggerModeJsonIds(), appSettingsTriggerModeIds(),
                      QStringLiteral("plc.process.triggerMode"), &candidate.triggerModeId, error)
            || !mapId(imageRange, imageSaveRangeJsonIds(), appSettingsImageSaveModeIds(),
                      QStringLiteral("imageSaving.range"), &candidate.imageSaveModeId, error)
            || !mapId(imageContent, imageSaveContentJsonIds(), appSettingsImageSaveTypeIds(),
                      QStringLiteral("imageSaving.content"), &candidate.imageSaveTypeId, error)) {
        return false;
    }
    if (!validateSettings(candidate, error)) {
        return false;
    }
    *settings = candidate;
    return true;
}

}

AppSettingsStore::AppSettingsStore(const QString &applicationDataRoot)
    : m_applicationDataRoot(applicationDataRoot.trimmed().isEmpty()
                            ? QString()
                            : QDir(applicationDataRoot).absolutePath())
{
}

QString AppSettingsStore::applicationDataRoot() const
{
    return m_applicationDataRoot;
}

QString AppSettingsStore::settingsFilePath() const
{
    return m_applicationDataRoot.isEmpty()
            ? QString()
            : QDir(m_applicationDataRoot).filePath(
                QStringLiteral("settings/app_settings.json"));
}

bool AppSettingsStore::load(AppSettings *settings,
                            AppSettingsLoadStatus *status,
                            AppSettingsStoreError *error) const
{
    clearError(error);
    if (!settings || !status) {
        return fail(error, QStringLiteral("SETTINGS_READ_FAILED"),
                    QStringLiteral("设置读取目标无效。"),
                    QStringLiteral("Null load output."));
    }
    if (m_applicationDataRoot.isEmpty()) {
        return fail(error, QStringLiteral("DATA_ROOT_UNAVAILABLE"),
                    QStringLiteral("无法确定当前用户的应用数据目录。"),
                    QStringLiteral("Application data root is empty."));
    }
    const QString path = settingsFilePath();
    if (!QFileInfo::exists(path)) {
        *settings = AppSettings::defaults();
        *status = AppSettingsLoadStatus::FirstRun;
        return true;
    }

    QFile versionFile(path);
    if (!versionFile.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("SETTINGS_READ_FAILED"),
                    QStringLiteral("无法读取设置文件。"), versionFile.errorString());
    }
    QJsonParseError parseError;
    const QJsonDocument versionDocument =
            QJsonDocument::fromJson(versionFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
            || !versionDocument.isObject()) {
        return fail(error, QStringLiteral("SETTINGS_JSON_MALFORMED"),
                    QStringLiteral("设置文件已损坏，程序不会自动覆盖原文件。"),
                    parseError.errorString());
    }
    int schemaVersion = 0;
    if (!readInt(versionDocument.object(), "schemaVersion", &schemaVersion, error)) {
        return false;
    }
    if (schemaVersion != AppSettings::CurrentSchemaVersion) {
        *status = AppSettingsLoadStatus::ResetRequired;
        return fail(error, QStringLiteral("SETTINGS_RESET_REQUIRED"),
                    QStringLiteral("旧版设置与当前版本不兼容。"),
                    QStringLiteral("schemaVersion=%1").arg(schemaVersion));
    }

    AppSettings candidate;
    if (!settingsFromJson(versionDocument.object(), &candidate, error)) {
        return false;
    }
    *settings = candidate;
    *status = AppSettingsLoadStatus::Loaded;
    return true;
}

bool AppSettingsStore::save(const AppSettings &settings,
                            AppSettingsStoreError *error) const
{
    clearError(error);
    if (m_applicationDataRoot.isEmpty()) {
        return fail(error, QStringLiteral("DATA_ROOT_UNAVAILABLE"),
                    QStringLiteral("无法确定当前用户的应用数据目录。"),
                    QStringLiteral("Application data root is empty."));
    }
    if (!validateSettings(settings, error)) {
        return false;
    }
    const QString path = settingsFilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return fail(error, QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("无法创建设置目录。"),
                    QFileInfo(path).absolutePath());
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("无法写入设置文件。"), file.errorString());
    }
    const QByteArray json = QJsonDocument(settingsToJson(settings))
            .toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        file.cancelWriting();
        return fail(error, QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("设置文件写入不完整。"), file.errorString());
    }
    if (!file.commit()) {
        return fail(error, QStringLiteral("SETTINGS_COMMIT_FAILED"),
                    QStringLiteral("设置文件原子提交失败，原文件保持不变。"),
                    file.errorString());
    }
    return true;
}
