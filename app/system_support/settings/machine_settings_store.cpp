#include "system_support/settings/machine_settings_store.h"
// 文件作用：本文件用于负责机器设置JSON的读取、校验、事务保存、清空和默认恢复。
// 主要职责：负责机器设置JSON的读取、校验、事务保存、清空和默认恢复。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。

#include "contracts/detection_mode.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QUuid>

#include <cmath>
#include <limits>

#pragma execution_character_set("utf-8")

namespace {

// 函数说明：clearError 函数停止流程、清理状态或释放对应资源。
void clearError(MachineSettingsStoreError *error)
{
    if (error) {
        *error = MachineSettingsStoreError();
    }
}

// 函数说明：fail 函数实现名称所表示的处理步骤。
bool fail(MachineSettingsStoreError *error,
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

// 函数说明：hasOnlyKeys 函数检查相关状态并返回判断结果。
bool hasOnlyKeys(const QJsonObject &object,
                 const QStringList &allowed,
                 const QString &context,
                 MachineSettingsStoreError *error)
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

// 函数说明：readObject 函数读取、等待或计算对应的数据。
bool readObject(const QJsonObject &parent,
                const char *key,
                QJsonObject *value,
                MachineSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (field.isUndefined()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_MISSING"),
                    QStringLiteral("设置文件缺少必填字段。"),
                    QStringLiteral("Missing object: %1")
                    .arg(QLatin1String(key)));
    }
    if (!field.isObject()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件字段类型不正确。"),
                    QStringLiteral("Expected object: %1")
                    .arg(QLatin1String(key)));
    }
    *value = field.toObject();
    return true;
}

// 函数说明：readString 函数读取、等待或计算对应的数据。
bool readString(const QJsonObject &parent,
                const char *key,
                QString *value,
                MachineSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (field.isUndefined()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_MISSING"),
                    QStringLiteral("设置文件缺少必填字段。"),
                    QStringLiteral("Missing string: %1")
                    .arg(QLatin1String(key)));
    }
    if (!field.isString()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件字段类型不正确。"),
                    QStringLiteral("Expected string: %1")
                    .arg(QLatin1String(key)));
    }
    *value = field.toString();
    return true;
}

// 函数说明：readInt 函数读取、等待或计算对应的数据。
bool readInt(const QJsonObject &parent,
             const char *key,
             int *value,
             MachineSettingsStoreError *error)
{
    const QJsonValue field = parent.value(QLatin1String(key));
    if (field.isUndefined()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_MISSING"),
                    QStringLiteral("设置文件缺少必填字段。"),
                    QStringLiteral("Missing int: %1")
                    .arg(QLatin1String(key)));
    }
    if (!field.isDouble()
            || !std::isfinite(field.toDouble())
            || std::floor(field.toDouble()) != field.toDouble()
            || field.toDouble()
               < static_cast<double>(std::numeric_limits<int>::min())
            || field.toDouble()
               > static_cast<double>(std::numeric_limits<int>::max())) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                    QStringLiteral("设置文件字段类型不正确。"),
                    QStringLiteral("Expected int: %1")
                    .arg(QLatin1String(key)));
    }
    *value = static_cast<int>(field.toDouble());
    return true;
}

// 函数说明：isCanonicalUuid 函数检查相关状态并返回判断结果。
bool isCanonicalUuid(const QString &value)
{
    if (value.isEmpty()) {
        return true;
    }
    const QUuid uuid(value);
    return !uuid.isNull()
            && uuid.toString(QUuid::WithoutBraces)
               == value.toLower();
}

// 函数说明：detectionModeJsonIds 函数执行对应事件或业务处理。
QStringList detectionModeJsonIds()
{
    QStringList ids;
    for (const DetectionModeDescriptor &descriptor :
         detectionModeDescriptors()) {
        ids.append(QLatin1String(descriptor.recipeId));
    }
    return ids;
}

// 函数说明：imageSaveRangeJsonIds 函数实现名称所表示的处理步骤。
QStringList imageSaveRangeJsonIds()
{
    return QStringList()
            << QStringLiteral("none")
            << QStringLiteral("ngOnly")
            << QStringLiteral("okOnly")
            << QStringLiteral("all");
}

// 函数说明：imageSaveContentJsonIds 函数实现名称所表示的处理步骤。
QStringList imageSaveContentJsonIds()
{
    return QStringList()
            << QStringLiteral("annotatedAndRaw")
            << QStringLiteral("annotatedOnly")
            << QStringLiteral("rawOnly");
}

// 函数说明：rotationJsonIds 函数实现名称所表示的处理步骤。
QStringList rotationJsonIds()
{
    return QStringList()
            << QStringLiteral("none")
            << QStringLiteral("clockwise90")
            << QStringLiteral("counterclockwise90")
            << QStringLiteral("rotate180");
}

// 函数说明：triggerModeJsonIds 函数执行对应事件或业务处理。
QStringList triggerModeJsonIds()
{
    return QStringList()
            << QStringLiteral("continuous")
            << QStringLiteral("intermittent");
}

// 函数说明：jsonIdForInternalId 函数实现名称所表示的处理步骤。
QString jsonIdForInternalId(const QString &internalId,
                            const QStringList &internalIds,
                            const QStringList &jsonIds)
{
    const int index = internalIds.indexOf(internalId);
    return index >= 0 && index < jsonIds.size()
            ? jsonIds.at(index)
            : QString();
}

// 函数说明：internalIdFromJsonId 函数实现名称所表示的处理步骤。
bool internalIdFromJsonId(const QString &jsonId,
                          const QStringList &jsonIds,
                          const QStringList &internalIds,
                          const QString &field,
                          QString *internalId,
                          MachineSettingsStoreError *error)
{
    const int index = jsonIds.indexOf(jsonId);
    if (index < 0 || index >= internalIds.size()) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("设置文件包含不支持的选项。"),
                    QStringLiteral("%1=%2")
                    .arg(field, jsonId));
    }
    *internalId = internalIds.at(index);
    return true;
}

// 函数说明：validateSettings 函数校验、转换或恢复对应数据。
bool validateSettings(const MachineSettings &settings,
                      MachineSettingsStoreError *error)
{
    const MachineSettings defaults = MachineSettings::defaults();
    if (settings.schemaVersion
            != MachineSettings::CurrentSchemaVersion) {
        return fail(error,
                    QStringLiteral("SETTINGS_SCHEMA_UNSUPPORTED"),
                    QStringLiteral("设置文件版本不受支持。"),
                    QStringLiteral("schemaVersion=%1")
                    .arg(settings.schemaVersion));
    }
    if (!machineSettingsDetectionModeIds()
            .contains(settings.detectModeId)
            || !machineSettingsImageSaveModeIds()
               .contains(settings.imageSaveModeId)
            || !machineSettingsImageSaveTypeIds()
               .contains(settings.imageSaveTypeId)
            || !machineSettingsColorChannelIds()
               .contains(settings.colorChannelId)
            || !machineSettingsRotationIds()
               .contains(settings.imageRotationId)
            || !machineSettingsTriggerModeIds()
               .contains(settings.triggerModeId)) {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("设置文件包含不支持的选项。"),
                    QStringLiteral("One or more enum ids are invalid."));
    }
    const int nonNegativeValues[] = {
        settings.cameraExposure,
        settings.cameraGain,
        settings.cameraDelay,
        settings.plcRack,
        settings.plcSlot,
        settings.photoDistance,
        settings.photoTime,
        settings.rejectDistance,
        settings.rejectTime,
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
    if (settings.photoTime > 65535
            || settings.rejectTime > 65535
            || settings.plcIp.trimmed().isEmpty()
            || settings.imageJpegQuality
               != defaults.imageJpegQuality) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("设置字段组合不符合设备合同。"),
                    QStringLiteral("PLC word, IP, or JPEG contract invalid."));
    }
    if (settings.imageSaveModeId
            != QLatin1String("save_none")) {
        const QFileInfo output(settings.imageSavePath);
        if (settings.imageSavePath.trimmed().isEmpty()
                || !output.isAbsolute()) {
            return fail(error,
                        QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                        QStringLiteral("启用存图时必须选择绝对输出目录。"),
                        QStringLiteral("imageSaving.outputDirectory invalid."));
        }
    }
    if (settings.plcTriggerModeDb != defaults.plcTriggerModeDb
            || settings.plcTriggerModeOffset
               != defaults.plcTriggerModeOffset
            || settings.plcResultDb != defaults.plcResultDb
            || settings.plcResultOffset != defaults.plcResultOffset
            || settings.plcPhotoDistanceOffset
               != defaults.plcPhotoDistanceOffset
            || settings.plcPhotoTimeOffset
               != defaults.plcPhotoTimeOffset
            || settings.plcRejectDistanceOffset
               != defaults.plcRejectDistanceOffset
            || settings.plcRejectTimeOffset
               != defaults.plcRejectTimeOffset) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("PLC地址与当前设备合同不一致。"),
                    QStringLiteral("Fixed PLC address contract changed."));
    }
    for (auto it = settings.publishedRecipeIdsByMode.constBegin();
         it != settings.publishedRecipeIdsByMode.constEnd(); ++it) {
        if (!machineSettingsDetectionModeIds().contains(it.key())
                || !isCanonicalUuid(it.value())) {
            return fail(error,
                        QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                        QStringLiteral("最近配方记录无效。"),
                        QStringLiteral("Invalid mode/recipe id: %1=%2")
                        .arg(it.key(), it.value()));
        }
    }
    return true;
}

// 函数说明：settingsToJson 函数更新或应用对应的配置和状态。
QJsonObject settingsToJson(const MachineSettings &settings)
{
    QJsonObject camera;
    camera.insert(QStringLiteral("exposureMicroseconds"),
                  settings.cameraExposure);
    camera.insert(QStringLiteral("gain"), settings.cameraGain);
    camera.insert(QStringLiteral("triggerSource"),
                  settings.triggerEnabled
                  ? QStringLiteral("hardwareLine0")
                  : QStringLiteral("software"));
    camera.insert(QStringLiteral("rotation"),
                  jsonIdForInternalId(
                      settings.imageRotationId,
                      machineSettingsRotationIds(),
                      rotationJsonIds()));
    camera.insert(QStringLiteral("colorChannel"),
                  jsonIdForInternalId(
                      settings.colorChannelId,
                      machineSettingsColorChannelIds(),
                      machineSettingsColorChannelIds()));

    QJsonObject inspection;
    inspection.insert(QStringLiteral("minimumIntervalMs"),
                      settings.cameraDelay);

    QJsonObject connection;
    connection.insert(QStringLiteral("ip"), settings.plcIp);
    connection.insert(QStringLiteral("rack"), settings.plcRack);
    connection.insert(QStringLiteral("slot"), settings.plcSlot);

    QJsonObject process;
    process.insert(QStringLiteral("triggerMode"),
                   jsonIdForInternalId(
                       settings.triggerModeId,
                       machineSettingsTriggerModeIds(),
                       triggerModeJsonIds()));
    process.insert(QStringLiteral("photoDistanceMm"),
                   settings.photoDistance);
    process.insert(QStringLiteral("photoTimeMs"), settings.photoTime);
    process.insert(QStringLiteral("rejectDistanceMm"),
                   settings.rejectDistance);
    process.insert(QStringLiteral("rejectTimeMs"), settings.rejectTime);
    process.insert(QStringLiteral("rejectPosition"),
                   settings.rejectPosition);

    auto address = [](int db, int offset) {
        QJsonObject value;
        value.insert(QStringLiteral("db"), db);
        value.insert(QStringLiteral("byteOffset"), offset);
        return value;
    };
    QJsonObject addresses;
    addresses.insert(QStringLiteral("triggerMode"),
                     address(settings.plcTriggerModeDb,
                             settings.plcTriggerModeOffset));
    addresses.insert(QStringLiteral("result"),
                     address(settings.plcResultDb,
                             settings.plcResultOffset));
    addresses.insert(QStringLiteral("photoDistanceDb"),
                     settings.plcPhotoDistanceOffset);
    addresses.insert(QStringLiteral("photoTimeDb"),
                     settings.plcPhotoTimeOffset);
    addresses.insert(QStringLiteral("rejectDistanceDb"),
                     settings.plcRejectDistanceOffset);
    addresses.insert(QStringLiteral("rejectTimeDb"),
                     settings.plcRejectTimeOffset);

    QJsonObject plc;
    plc.insert(QStringLiteral("connection"), connection);
    plc.insert(QStringLiteral("process"), process);
    plc.insert(QStringLiteral("addresses"), addresses);

    QJsonObject imageSaving;
    imageSaving.insert(QStringLiteral("range"),
                       jsonIdForInternalId(
                           settings.imageSaveModeId,
                           machineSettingsImageSaveModeIds(),
                           imageSaveRangeJsonIds()));
    imageSaving.insert(QStringLiteral("content"),
                       jsonIdForInternalId(
                           settings.imageSaveTypeId,
                           machineSettingsImageSaveTypeIds(),
                           imageSaveContentJsonIds()));
    imageSaving.insert(QStringLiteral("outputDirectory"),
                       settings.imageSavePath);
    imageSaving.insert(QStringLiteral("jpegQuality"),
                       settings.imageJpegQuality);

    QJsonObject recipeIds;
    for (auto it = settings.publishedRecipeIdsByMode.constBegin();
         it != settings.publishedRecipeIdsByMode.constEnd(); ++it) {
        recipeIds.insert(
                    jsonIdForInternalId(
                        it.key(),
                        machineSettingsDetectionModeIds(),
                        detectionModeJsonIds()),
                    it.value());
    }
    QJsonObject ui;
    ui.insert(QStringLiteral("selectedDetectionMode"),
              jsonIdForInternalId(
                  settings.detectModeId,
                  machineSettingsDetectionModeIds(),
                  detectionModeJsonIds()));
    ui.insert(QStringLiteral("lastRecipeIdByMode"), recipeIds);
    ui.insert(QStringLiteral("rightPanelSplitterStateBase64"),
              QString::fromLatin1(
                  settings.rightPanelSplitterState.toBase64()));

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"),
                settings.schemaVersion);
    root.insert(QStringLiteral("camera"), camera);
    root.insert(QStringLiteral("inspection"), inspection);
    root.insert(QStringLiteral("plc"), plc);
    root.insert(QStringLiteral("imageSaving"), imageSaving);
    root.insert(QStringLiteral("ui"), ui);
    return root;
}

// 函数说明：addressFromJson 函数实现名称所表示的处理步骤。
bool addressFromJson(const QJsonObject &parent,
                     const char *key,
                     int *db,
                     int *offset,
                     MachineSettingsStoreError *error)
{
    QJsonObject value;
    return readObject(parent, key, &value, error)
            && hasOnlyKeys(value,
                           QStringList() << "db" << "byteOffset",
                           QString::fromLatin1(key), error)
            && readInt(value, "db", db, error)
            && readInt(value, "byteOffset", offset, error);
}

// 函数说明：settingsFromJson 函数更新或应用对应的配置和状态。
bool settingsFromJson(const QJsonObject &root,
                      MachineSettings *settings,
                      MachineSettingsStoreError *error)
{
    MachineSettings candidate;
    QJsonObject camera;
    QJsonObject inspection;
    QJsonObject plc;
    QJsonObject connection;
    QJsonObject process;
    QJsonObject addresses;
    QJsonObject imageSaving;
    QJsonObject ui;
    if (!hasOnlyKeys(root,
                     QStringList() << "schemaVersion" << "camera"
                     << "inspection" << "plc" << "imageSaving" << "ui",
                     QStringLiteral("root"), error)
            || !readInt(root, "schemaVersion", &candidate.schemaVersion, error)
            || !readObject(root, "camera", &camera, error)
            || !readObject(root, "inspection", &inspection, error)
            || !readObject(root, "plc", &plc, error)
            || !readObject(plc, "connection", &connection, error)
            || !readObject(plc, "process", &process, error)
            || !readObject(plc, "addresses", &addresses, error)
            || !readObject(root, "imageSaving", &imageSaving, error)
            || !readObject(root, "ui", &ui, error)) {
        return false;
    }
    if (!hasOnlyKeys(camera,
                     QStringList() << "exposureMicroseconds" << "gain"
                     << "triggerSource" << "rotation" << "colorChannel",
                     QStringLiteral("camera"), error)
            || !hasOnlyKeys(inspection,
                            QStringList() << "minimumIntervalMs",
                            QStringLiteral("inspection"), error)
            || !hasOnlyKeys(plc,
                            QStringList() << "connection" << "process"
                            << "addresses",
                            QStringLiteral("plc"), error)
            || !hasOnlyKeys(connection,
                            QStringList() << "ip" << "rack" << "slot",
                            QStringLiteral("plc.connection"), error)
            || !hasOnlyKeys(process,
                            QStringList() << "triggerMode"
                            << "photoDistanceMm" << "photoTimeMs"
                            << "rejectDistanceMm" << "rejectTimeMs"
                            << "rejectPosition",
                            QStringLiteral("plc.process"), error)
            || !hasOnlyKeys(addresses,
                            QStringList() << "triggerMode" << "result"
                            << "photoDistanceDb" << "photoTimeDb"
                            << "rejectDistanceDb" << "rejectTimeDb",
                            QStringLiteral("plc.addresses"), error)
            || !hasOnlyKeys(imageSaving,
                            QStringList() << "range" << "content"
                            << "outputDirectory" << "jpegQuality",
                            QStringLiteral("imageSaving"), error)
            || !hasOnlyKeys(ui,
                            QStringList() << "selectedDetectionMode"
                            << "lastRecipeIdByMode"
                            << "rightPanelSplitterStateBase64",
                            QStringLiteral("ui"), error)) {
        return false;
    }
    QString triggerSource;
    QString rotationJsonId;
    QString colorChannelJsonId;
    QString triggerModeJsonId;
    QString imageSaveRangeJsonId;
    QString imageSaveContentJsonId;
    QString detectionModeJsonId;
    if (!readInt(camera, "exposureMicroseconds",
                 &candidate.cameraExposure, error)
            || !readInt(camera, "gain", &candidate.cameraGain, error)
            || !readString(camera, "triggerSource",
                           &triggerSource, error)
            || !readString(camera, "rotation",
                           &rotationJsonId, error)
            || !readString(camera, "colorChannel",
                           &colorChannelJsonId, error)
            || !readInt(inspection, "minimumIntervalMs",
                        &candidate.cameraDelay, error)
            || !readString(connection, "ip", &candidate.plcIp, error)
            || !readInt(connection, "rack", &candidate.plcRack, error)
            || !readInt(connection, "slot", &candidate.plcSlot, error)
            || !readString(process, "triggerMode",
                           &triggerModeJsonId, error)
            || !readInt(process, "photoDistanceMm",
                        &candidate.photoDistance, error)
            || !readInt(process, "photoTimeMs",
                        &candidate.photoTime, error)
            || !readInt(process, "rejectDistanceMm",
                        &candidate.rejectDistance, error)
            || !readInt(process, "rejectTimeMs",
                        &candidate.rejectTime, error)
            || !readInt(process, "rejectPosition",
                        &candidate.rejectPosition, error)
            || !addressFromJson(addresses, "triggerMode",
                                &candidate.plcTriggerModeDb,
                                &candidate.plcTriggerModeOffset,
                                error)
            || !addressFromJson(addresses, "result",
                                &candidate.plcResultDb,
                                &candidate.plcResultOffset,
                                error)
            || !readInt(addresses, "photoDistanceDb",
                        &candidate.plcPhotoDistanceOffset, error)
            || !readInt(addresses, "photoTimeDb",
                        &candidate.plcPhotoTimeOffset, error)
            || !readInt(addresses, "rejectDistanceDb",
                        &candidate.plcRejectDistanceOffset, error)
            || !readInt(addresses, "rejectTimeDb",
                        &candidate.plcRejectTimeOffset, error)
            || !readString(imageSaving, "range",
                           &imageSaveRangeJsonId, error)
            || !readString(imageSaving, "content",
                           &imageSaveContentJsonId, error)
            || !readString(imageSaving, "outputDirectory",
                           &candidate.imageSavePath, error)
            || !readInt(imageSaving, "jpegQuality",
                        &candidate.imageJpegQuality, error)
            || !readString(ui, "selectedDetectionMode",
                           &detectionModeJsonId, error)) {
        return false;
    }
    if (triggerSource == QLatin1String("hardwareLine0")) {
        candidate.triggerEnabled = true;
    } else if (triggerSource == QLatin1String("software")) {
        candidate.triggerEnabled = false;
    } else {
        return fail(error,
                    QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"),
                    QStringLiteral("相机触发源不受支持。"),
                    QStringLiteral("triggerSource=%1")
                    .arg(triggerSource));
    }
    if (!internalIdFromJsonId(
            rotationJsonId,
            rotationJsonIds(),
            machineSettingsRotationIds(),
            QStringLiteral("camera.rotation"),
            &candidate.imageRotationId,
            error)
            || !internalIdFromJsonId(
                colorChannelJsonId,
                machineSettingsColorChannelIds(),
                machineSettingsColorChannelIds(),
                QStringLiteral("camera.colorChannel"),
                &candidate.colorChannelId,
                error)
            || !internalIdFromJsonId(
                triggerModeJsonId,
                triggerModeJsonIds(),
                machineSettingsTriggerModeIds(),
                QStringLiteral("plc.process.triggerMode"),
                &candidate.triggerModeId,
                error)
            || !internalIdFromJsonId(
                imageSaveRangeJsonId,
                imageSaveRangeJsonIds(),
                machineSettingsImageSaveModeIds(),
                QStringLiteral("imageSaving.range"),
                &candidate.imageSaveModeId,
                error)
            || !internalIdFromJsonId(
                imageSaveContentJsonId,
                imageSaveContentJsonIds(),
                machineSettingsImageSaveTypeIds(),
                QStringLiteral("imageSaving.content"),
                &candidate.imageSaveTypeId,
                error)
            || !internalIdFromJsonId(
                detectionModeJsonId,
                detectionModeJsonIds(),
                machineSettingsDetectionModeIds(),
                QStringLiteral("ui.selectedDetectionMode"),
                &candidate.detectModeId,
                error)) {
        return false;
    }
    QJsonObject recipeIds;
    if (!readObject(ui, "lastRecipeIdByMode", &recipeIds, error)) {
        return false;
    }
    for (auto it = recipeIds.constBegin(); it != recipeIds.constEnd(); ++it) {
        if (!it.value().isString()) {
            return fail(error,
                        QStringLiteral("SETTINGS_FIELD_TYPE_INVALID"),
                        QStringLiteral("最近配方记录类型不正确。"),
                        QStringLiteral("lastRecipeIdByMode.%1")
                        .arg(it.key()));
        }
        QString internalModeId;
        if (!internalIdFromJsonId(
                it.key(),
                detectionModeJsonIds(),
                machineSettingsDetectionModeIds(),
                QStringLiteral("ui.lastRecipeIdByMode"),
                &internalModeId,
                error)) {
            return false;
        }
        candidate.publishedRecipeIdsByMode.insert(
                    internalModeId, it.value().toString());
    }
    QString splitter;
    if (!readString(ui, "rightPanelSplitterStateBase64",
                    &splitter, error)) {
        return false;
    }
    candidate.rightPanelSplitterState =
            QByteArray::fromBase64(splitter.toLatin1());
    if (!splitter.isEmpty()
            && QString::fromLatin1(
                candidate.rightPanelSplitterState.toBase64())
               != splitter) {
        return fail(error,
                    QStringLiteral("SETTINGS_CONSTRAINT_VIOLATION"),
                    QStringLiteral("界面布局数据已损坏。"),
                    QStringLiteral("Invalid base64 splitter state."));
    }
    if (!validateSettings(candidate, error)) {
        return false;
    }
    *settings = candidate;
    return true;
}

// 函数说明：readSettingsFile 函数读取、等待或计算对应的数据。
bool readSettingsFile(const QString &path,
                      MachineSettings *settings,
                      MachineSettingsStoreError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error,
                    QStringLiteral("SETTINGS_READ_FAILED"),
                    QStringLiteral("无法读取设置文件。"),
                    file.errorString());
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        return fail(error,
                    QStringLiteral("SETTINGS_READ_FAILED"),
                    QStringLiteral("无法完整读取设置文件。"),
                    file.errorString());
    }
    QJsonParseError parseError;
    const QJsonDocument document =
            QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
        return fail(error,
                    QStringLiteral("SETTINGS_JSON_MALFORMED"),
                    QStringLiteral("设置文件已损坏，程序不会使用默认值继续运行。"),
                    parseError.errorString());
    }
    return settingsFromJson(document.object(), settings, error);
}

} // namespace

// 函数说明：MachineSettingsStore 构造函数创建组件并初始化其依赖和初始状态。
MachineSettingsStore::MachineSettingsStore(
    const QString &applicationDataRoot)
    : m_applicationDataRoot(
          applicationDataRoot.trimmed().isEmpty()
          ? QString()
          : QDir(applicationDataRoot).absolutePath())
{
}

// 函数说明：applicationDataRoot 函数实现名称所表示的处理步骤。
QString MachineSettingsStore::applicationDataRoot() const
{
    return m_applicationDataRoot;
}

// 函数说明：settingsFilePath 函数更新或应用对应的配置和状态。
QString MachineSettingsStore::settingsFilePath() const
{
    return m_applicationDataRoot.isEmpty()
            ? QString()
            : QDir(m_applicationDataRoot).filePath(
                QStringLiteral("settings/app_settings.json"));
}

// 函数说明：recipesRootPath 函数实现名称所表示的处理步骤。
QString MachineSettingsStore::recipesRootPath() const
{
    return m_applicationDataRoot.isEmpty()
            ? QString()
            : QDir(m_applicationDataRoot).filePath(
                QStringLiteral("recipes"));
}

// 函数说明：editorWorkspacesRootPath 函数实现名称所表示的处理步骤。
QString MachineSettingsStore::editorWorkspacesRootPath() const
{
    return m_applicationDataRoot.isEmpty()
            ? QString()
            : QDir(m_applicationDataRoot).filePath(
                QStringLiteral("editor-workspaces"));
}

// 函数说明：load 函数读取、等待或计算对应的数据。
bool MachineSettingsStore::load(
    MachineSettings *settings,
    MachineSettingsLoadStatus *status,
    MachineSettingsStoreError *error) const
{
    clearError(error);
    if (!settings || !status) {
        return fail(error,
                    QStringLiteral("SETTINGS_READ_FAILED"),
                    QStringLiteral("设置读取目标无效。"),
                    QStringLiteral("Null load output."));
    }
    if (m_applicationDataRoot.isEmpty()) {
        return fail(error,
                    QStringLiteral("DATA_ROOT_UNAVAILABLE"),
                    QStringLiteral("无法确定当前用户的应用数据目录。"),
                    QStringLiteral("Application data root is empty."));
    }
    const QString path = settingsFilePath();
    if (!QFileInfo::exists(path)) {
        *settings = MachineSettings::defaults();
        *status = MachineSettingsLoadStatus::FirstRun;
        return true;
    }
    MachineSettings candidate;
    if (!readSettingsFile(path, &candidate, error)) {
        return false;
    }
    *settings = candidate;
    *status = MachineSettingsLoadStatus::Loaded;
    return true;
}

// 函数说明：save 函数保存或发布对应的数据和资源。
bool MachineSettingsStore::save(
    const MachineSettings &settings,
    MachineSettingsStoreError *error) const
{
    clearError(error);
    if (m_applicationDataRoot.isEmpty()) {
        return fail(error,
                    QStringLiteral("DATA_ROOT_UNAVAILABLE"),
                    QStringLiteral("无法确定当前用户的应用数据目录。"),
                    QStringLiteral("Application data root is empty."));
    }
    if (!validateSettings(settings, error)) {
        return false;
    }
    const QString finalPath = settingsFilePath();
    const QFileInfo finalInfo(finalPath);
    QDir directory;
    if (!directory.mkpath(finalInfo.absolutePath())) {
        return fail(error,
                    QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("无法创建设置目录。"),
                    finalInfo.absolutePath());
    }
    const QString token = QUuid::createUuid()
            .toString(QUuid::WithoutBraces);
    const QString temporaryPath = finalPath
            + QStringLiteral(".tmp-") + token;
    const QString backupPath = finalPath
            + QStringLiteral(".bak-") + token;
    QFile temporary(temporaryPath);
    if (!temporary.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(error,
                    QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("无法写入设置临时文件。"),
                    temporary.errorString());
    }
    const QByteArray json = QJsonDocument(settingsToJson(settings))
            .toJson(QJsonDocument::Indented);
    if (temporary.write(json) != json.size()
            || !temporary.flush()) {
        const QString diagnostic = temporary.errorString();
        temporary.close();
        QFile::remove(temporaryPath);
        return fail(error,
                    QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("设置临时文件写入不完整。"),
                    diagnostic);
    }
    temporary.close();

    MachineSettings verified;
    if (!readSettingsFile(temporaryPath, &verified, error)
            || verified != settings) {
        QFile::remove(temporaryPath);
        if (!error || error->isEmpty()) {
            fail(error,
                 QStringLiteral("SETTINGS_VERIFY_FAILED"),
                 QStringLiteral("设置写入校验失败。"),
                 QStringLiteral("Round-trip mismatch."));
        }
        return false;
    }

    const bool hadPrevious = QFileInfo::exists(finalPath);
    if (hadPrevious && !QFile::rename(finalPath, backupPath)) {
        QFile::remove(temporaryPath);
        return fail(error,
                    QStringLiteral("SETTINGS_COMMIT_FAILED"),
                    QStringLiteral("无法备份上一份设置。"),
                    finalPath);
    }
    if (!QFile::rename(temporaryPath, finalPath)) {
        bool rolledBack = true;
        if (hadPrevious) {
            rolledBack = QFile::rename(backupPath, finalPath);
        }
        QFile::remove(temporaryPath);
        return fail(error,
                    rolledBack
                    ? QStringLiteral("SETTINGS_COMMIT_FAILED")
                    : QStringLiteral("SETTINGS_ROLLBACK_FAILED"),
                    rolledBack
                    ? QStringLiteral("设置提交失败，上一份设置已保留。")
                    : QStringLiteral("设置提交和回滚均失败。"),
                    finalPath);
    }
    if (hadPrevious) {
        QFile::remove(backupPath);
    }
    return true;
}

// 函数说明：restoreDefaults 函数校验、转换或恢复对应数据。
bool MachineSettingsStore::restoreDefaults(
    MachineSettings *settings,
    MachineSettingsStoreError *error) const
{
    if (!settings) {
        return fail(error,
                    QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("默认设置写入目标无效。"),
                    QStringLiteral("Null defaults output."));
    }
    const MachineSettings defaults = MachineSettings::defaults();
    if (!save(defaults, error)) {
        return false;
    }
    *settings = defaults;
    return true;
}

// 函数说明：clear 函数停止流程、清理状态或释放对应资源。
bool MachineSettingsStore::clear(
    MachineSettingsStoreError *error) const
{
    clearError(error);
    if (m_applicationDataRoot.isEmpty()) {
        return fail(error,
                    QStringLiteral("DATA_ROOT_UNAVAILABLE"),
                    QStringLiteral("无法确定当前用户的应用数据目录。"),
                    QStringLiteral("Application data root is empty."));
    }
    const QString path = settingsFilePath();
    if (!QFileInfo::exists(path)) {
        return true;
    }
    if (!QFile::remove(path)) {
        return fail(error,
                    QStringLiteral("SETTINGS_WRITE_FAILED"),
                    QStringLiteral("无法清空当前软件设置。"),
                    path);
    }
    return true;
}
