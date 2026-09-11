#include "templates/template_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

#include <opencv2/imgcodecs.hpp>

const int TemplateSettings::CurrentSchemaVersion;
const int TemplateSettings::DefaultImageThresholdPercent;

namespace {

const char kSettingsFile[] = "template_settings.json";
const char kRawImageFile[] = "template_raw.png";
const char kTrackingImageFile[] = "tracking_template.bmp";
const char kStampRingFile[] = "template_ring.bmp";
const char kCharactersDirectory[] = "character_templates";

void clearError(TemplateStoreError *error)
{
    if (error) {
        *error = TemplateStoreError();
    }
}

bool fail(TemplateStoreError *error,
          const QString &code,
          const QString &message,
          const QString &diagnostic,
          const QString &path = QString())
{
    if (error) {
        error->code = code;
        error->userMessage = message;
        error->diagnostic = diagnostic;
        error->path = path;
    }
    return false;
}

bool hasOnlyKeys(const QJsonObject &object,
                 const QStringList &allowed,
                 const QString &context,
                 TemplateStoreError *error,
                 const QString &path)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                        QStringLiteral("模板文件内容不完整或格式不正确。"),
                        QStringLiteral("%1.%2").arg(context, it.key()),
                        path);
        }
    }
    return true;
}

QString normalizedDirectoryPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

bool finiteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
            && std::isfinite(rect.width()) && std::isfinite(rect.height())
            && rect.width() >= 0.0 && rect.height() >= 0.0;
}

bool finitePolygon(const QVector<QPointF> &polygon)
{
    for (const QPointF &point : polygon) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            return false;
        }
    }
    return true;
}

bool validateStructure(const TemplateSettings &settings,
                       TemplateStoreError *error,
                       const QString &path)
{
    if (settings.schemaVersion != TemplateSettings::CurrentSchemaVersion) {
        return fail(error, QStringLiteral("TEMPLATE_SCHEMA_UNSUPPORTED"),
                    QStringLiteral("模板版本与当前软件不兼容。"),
                    QStringLiteral("schemaVersion=%1")
                    .arg(settings.schemaVersion), path);
    }
    if (settings.detectionMode == DetectionMode::Tissue) {
        return fail(error, QStringLiteral("TEMPLATE_MODE_MISMATCH"),
                    QStringLiteral("纸巾检测不使用模板。"),
                    QStringLiteral("detectionMode=tissue"), path);
    }
    if (!finiteRect(settings.trackingRoi)
            || settings.trackingRoi.x() < 0.0
            || settings.trackingRoi.y() < 0.0
            || settings.imageThresholdPercent < 0
            || settings.imageThresholdPercent > 100
            || settings.characterSourceSize.width() < 0
            || settings.characterSourceSize.height() < 0) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    QStringLiteral("模板参数或检测区域无效，请重新制作模板。"),
                    QStringLiteral("Invalid ROI, threshold, or character size."),
                    path);
    }
    if (!finitePolygon(settings.datePolygon)
            || !finitePolygon(settings.barcodePolygon)
            || !finitePolygon(settings.stampPolygon)) {
        return fail(error, QStringLiteral("TEMPLATE_REGION_INVALID"),
                    QStringLiteral("模板参数或检测区域无效，请重新制作模板。"),
                    QStringLiteral("Non-finite polygon point."), path);
    }
    if (settings.detectionMode != DetectionMode::BarcodeWord
            && !settings.barcodePolygon.isEmpty()) {
        return fail(error, QStringLiteral("TEMPLATE_REGION_INVALID"),
                    QStringLiteral("当前模式不能保存二维码区域。"),
                    QStringLiteral("Unexpected barcodePolygon."), path);
    }
    if (settings.detectionMode != DetectionMode::Stamp
            && !settings.stampPolygon.isEmpty()) {
        return fail(error, QStringLiteral("TEMPLATE_REGION_INVALID"),
                    QStringLiteral("当前模式不能保存钢印区域。"),
                    QStringLiteral("Unexpected stampPolygon."), path);
    }
    if (settings.detectionMode == DetectionMode::BarcodeWord
            && (settings.barcodeParameters.formatMask == 0
                || settings.barcodeParameters.roiPaddingPercent < 0
                || settings.barcodeParameters.maxDecodeTimeMs <= 0)) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    QStringLiteral("二维码参数无效。"),
                    QStringLiteral("Invalid barcode parameters."), path);
    }
    for (const TemplateCharacterBox &box : settings.characterBoxes) {
        if (box.name.trimmed().isEmpty() || box.rect.width() <= 0
                || box.rect.height() <= 0 || box.rect.x() < 0
                || box.rect.y() < 0
                || (!settings.characterSourceSize.isEmpty()
                    && !QRect(QPoint(0, 0), settings.characterSourceSize)
                        .contains(box.rect))) {
            return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                        QStringLiteral("字符框名称或范围无效。"),
                        box.name, path);
        }
    }
    return true;
}

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject value;
    value.insert(QStringLiteral("x"), rect.x());
    value.insert(QStringLiteral("y"), rect.y());
    value.insert(QStringLiteral("width"), rect.width());
    value.insert(QStringLiteral("height"), rect.height());
    return value;
}

QJsonArray polygonToJson(const QVector<QPointF> &polygon)
{
    QJsonArray values;
    for (const QPointF &point : polygon) {
        QJsonObject value;
        value.insert(QStringLiteral("x"), point.x());
        value.insert(QStringLiteral("y"), point.y());
        values.append(value);
    }
    return values;
}

QJsonObject settingsToJson(const TemplateSettings &settings)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), settings.schemaVersion);
    root.insert(QStringLiteral("detectionMode"),
                detectionModeId(settings.detectionMode));
    root.insert(QStringLiteral("targetText"), settings.targetText);
    if (settings.detectionMode != DetectionMode::Ocr) {
        root.insert(QStringLiteral("imageThresholdPercent"),
                    settings.imageThresholdPercent);
    }
    root.insert(QStringLiteral("trackingRoi"),
                rectToJson(settings.trackingRoi));

    QJsonObject regions;
    regions.insert(QStringLiteral("datePolygon"),
                   polygonToJson(settings.datePolygon));
    if (settings.detectionMode == DetectionMode::BarcodeWord) {
        regions.insert(QStringLiteral("barcodePolygon"),
                       polygonToJson(settings.barcodePolygon));
    }
    if (settings.detectionMode == DetectionMode::Stamp) {
        regions.insert(QStringLiteral("stampPolygon"),
                       polygonToJson(settings.stampPolygon));
    }
    root.insert(QStringLiteral("regions"), regions);

    if (settings.detectionMode == DetectionMode::Stamp
            || settings.detectionMode == DetectionMode::Word
            || settings.detectionMode == DetectionMode::BarcodeWord) {
        QJsonObject size;
        size.insert(QStringLiteral("width"),
                    settings.characterSourceSize.width());
        size.insert(QStringLiteral("height"),
                    settings.characterSourceSize.height());
        root.insert(QStringLiteral("characterSourceSize"), size);
        QJsonArray boxes;
        for (const TemplateCharacterBox &box : settings.characterBoxes) {
            QJsonObject value;
            value.insert(QStringLiteral("name"), box.name);
            value.insert(QStringLiteral("rect"),
                         rectToJson(QRectF(box.rect)));
            boxes.append(value);
        }
        root.insert(QStringLiteral("characterBoxes"), boxes);
    }
    if (settings.detectionMode == DetectionMode::BarcodeWord) {
        QJsonObject barcode;
        barcode.insert(QStringLiteral("formatMask"),
                       static_cast<double>(settings.barcodeParameters.formatMask));
        barcode.insert(QStringLiteral("roiPaddingPercent"),
                       settings.barcodeParameters.roiPaddingPercent);
        barcode.insert(QStringLiteral("maxDecodeTimeMs"),
                       settings.barcodeParameters.maxDecodeTimeMs);
        barcode.insert(QStringLiteral("enableFallback"),
                       settings.barcodeParameters.enableFallback);
        root.insert(QStringLiteral("barcodeParameters"), barcode);
    }
    return root;
}

bool readNumber(const QJsonObject &object,
                const char *key,
                double *value)
{
    const QJsonValue field = object.value(QLatin1String(key));
    if (!field.isDouble() || !std::isfinite(field.toDouble())) {
        return false;
    }
    *value = field.toDouble();
    return true;
}

bool readInt(const QJsonObject &object,
             const char *key,
             int *value)
{
    double number = 0.0;
    if (!readNumber(object, key, &number)
            || std::floor(number) != number
            || number < std::numeric_limits<int>::min()
            || number > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(number);
    return true;
}

bool readRect(const QJsonValue &value, QRectF *rect)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    if (object.size() != 4 || !readNumber(object, "x", &x)
            || !readNumber(object, "y", &y)
            || !readNumber(object, "width", &width)
            || !readNumber(object, "height", &height)) {
        return false;
    }
    *rect = QRectF(x, y, width, height);
    return true;
}

bool readPolygon(const QJsonValue &value, QVector<QPointF> *polygon)
{
    if (!value.isArray()) {
        return false;
    }
    QVector<QPointF> result;
    for (const QJsonValue &item : value.toArray()) {
        if (!item.isObject()) {
            return false;
        }
        const QJsonObject object = item.toObject();
        double x = 0.0, y = 0.0;
        if (object.size() != 2 || !readNumber(object, "x", &x)
                || !readNumber(object, "y", &y)) {
            return false;
        }
        result.append(QPointF(x, y));
    }
    *polygon = result;
    return true;
}

bool parseSettings(const QByteArray &bytes,
                   TemplateSettings *settings,
                   TemplateStoreError *error,
                   const QString &path)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                    QStringLiteral("模板文件已损坏。"),
                    parseError.errorString(), path);
    }
    const QJsonObject root = document.object();
    int schemaVersion = 0;
    if (!readInt(root, "schemaVersion", &schemaVersion)) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                    QStringLiteral("模板文件内容不完整或格式不正确。"),
                    QStringLiteral("schemaVersion"), path);
    }
    if (schemaVersion != TemplateSettings::CurrentSchemaVersion) {
        return fail(error, QStringLiteral("TEMPLATE_SCHEMA_UNSUPPORTED"),
                    QStringLiteral("模板版本与当前软件不兼容。"),
                    QStringLiteral("schemaVersion=%1").arg(schemaVersion), path);
    }
    const QJsonValue modeValue = root.value(QStringLiteral("detectionMode"));
    const QJsonValue targetValue = root.value(QStringLiteral("targetText"));
    if (!modeValue.isString() || !targetValue.isString()) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                    QStringLiteral("模板文件内容不完整或格式不正确。"),
                    QStringLiteral("detectionMode/targetText"), path);
    }
    TemplateSettings candidate;
    candidate.schemaVersion = schemaVersion;
    if (!detectionModeFromId(modeValue.toString(), &candidate.detectionMode)
            || candidate.detectionMode == DetectionMode::Tissue) {
        return fail(error, QStringLiteral("TEMPLATE_MODE_MISMATCH"),
                    QStringLiteral("模板检测模式无效。"),
                    modeValue.toString(), path);
    }
    candidate.targetText = targetValue.toString();
    QStringList rootKeys = QStringList()
            << QStringLiteral("schemaVersion")
            << QStringLiteral("detectionMode")
            << QStringLiteral("targetText")
            << QStringLiteral("trackingRoi")
            << QStringLiteral("regions");
    if (candidate.detectionMode != DetectionMode::Ocr) {
        rootKeys << QStringLiteral("imageThresholdPercent");
    }
    const bool characterMode = candidate.detectionMode == DetectionMode::Stamp
            || candidate.detectionMode == DetectionMode::Word
            || candidate.detectionMode == DetectionMode::BarcodeWord;
    if (characterMode) {
        rootKeys << QStringLiteral("characterSourceSize")
                 << QStringLiteral("characterBoxes");
    }
    if (candidate.detectionMode == DetectionMode::BarcodeWord) {
        rootKeys << QStringLiteral("barcodeParameters");
    }
    if (!hasOnlyKeys(root, rootKeys, QStringLiteral("root"), error, path)) {
        return false;
    }
    if (candidate.detectionMode != DetectionMode::Ocr
            && !readInt(root, "imageThresholdPercent",
                        &candidate.imageThresholdPercent)) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                    QStringLiteral("模板文件内容不完整或格式不正确。"),
                    QStringLiteral("imageThresholdPercent"), path);
    }
    if (!readRect(root.value(QStringLiteral("trackingRoi")),
                  &candidate.trackingRoi)) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    QStringLiteral("模板文件内容不完整或格式不正确。"),
                    QStringLiteral("trackingRoi"), path);
    }
    const QJsonValue regionsValue = root.value(QStringLiteral("regions"));
    if (!regionsValue.isObject()) {
        return fail(error, QStringLiteral("TEMPLATE_REGION_INVALID"),
                    QStringLiteral("模板缺少区域设置。"),
                    QStringLiteral("regions"), path);
    }
    const QJsonObject regions = regionsValue.toObject();
    QStringList regionKeys = QStringList()
            << QStringLiteral("datePolygon");
    if (candidate.detectionMode == DetectionMode::BarcodeWord) {
        regionKeys << QStringLiteral("barcodePolygon");
    }
    if (candidate.detectionMode == DetectionMode::Stamp) {
        regionKeys << QStringLiteral("stampPolygon");
    }
    if (!hasOnlyKeys(regions, regionKeys, QStringLiteral("regions"),
                     error, path)) {
        return false;
    }
    if (!readPolygon(regions.value(QStringLiteral("datePolygon")),
                     &candidate.datePolygon)
            || (candidate.detectionMode == DetectionMode::BarcodeWord
                && !readPolygon(regions.value(QStringLiteral("barcodePolygon")),
                                &candidate.barcodePolygon))
            || (candidate.detectionMode == DetectionMode::Stamp
                && !readPolygon(regions.value(QStringLiteral("stampPolygon")),
                                &candidate.stampPolygon))) {
        return fail(error, QStringLiteral("TEMPLATE_REGION_INVALID"),
                    QStringLiteral("模板文件内容不完整或格式不正确。"),
                    QStringLiteral("regions"), path);
    }
    if (characterMode) {
        const QJsonValue sizeValue = root.value(
                    QStringLiteral("characterSourceSize"));
        const QJsonValue boxesValue = root.value(
                    QStringLiteral("characterBoxes"));
        if (!sizeValue.isObject() || !boxesValue.isArray()) {
            return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                        QStringLiteral("模板文件内容不完整或格式不正确。"),
                        QStringLiteral("characterSourceSize/characterBoxes"), path);
        }
        const QJsonObject size = sizeValue.toObject();
        int width = 0, height = 0;
        if (!hasOnlyKeys(size,
                         QStringList() << QStringLiteral("width")
                                       << QStringLiteral("height"),
                         QStringLiteral("characterSourceSize"), error, path)
                || !readInt(size, "width", &width)
                || !readInt(size, "height", &height)) {
            return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                        QStringLiteral("模板文件内容不完整或格式不正确。"),
                        QStringLiteral("characterSourceSize"), path);
        }
        candidate.characterSourceSize = QSize(width, height);
        for (const QJsonValue &item : boxesValue.toArray()) {
            if (!item.isObject()) {
                return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                            QStringLiteral("模板文件内容不完整或格式不正确。"),
                            QStringLiteral("characterBoxes"), path);
            }
            const QJsonObject object = item.toObject();
            const QJsonValue nameValue = object.value(QStringLiteral("name"));
            QRectF rect;
            if (!hasOnlyKeys(object,
                             QStringList() << QStringLiteral("name")
                                           << QStringLiteral("rect"),
                             QStringLiteral("characterBoxes"), error, path)
                    || !nameValue.isString()
                    || !readRect(object.value(QStringLiteral("rect")), &rect)
                    || rect.x() != std::floor(rect.x())
                    || rect.y() != std::floor(rect.y())
                    || rect.width() != std::floor(rect.width())
                    || rect.height() != std::floor(rect.height())) {
                return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                            QStringLiteral("模板文件内容不完整或格式不正确。"),
                            QStringLiteral("characterBoxes"), path);
            }
            TemplateCharacterBox box;
            box.name = nameValue.toString();
            box.rect = rect.toRect();
            candidate.characterBoxes.append(box);
        }
    }
    if (candidate.detectionMode == DetectionMode::BarcodeWord) {
        const QJsonValue barcodeValue = root.value(
                    QStringLiteral("barcodeParameters"));
        if (!barcodeValue.isObject()) {
            return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                        QStringLiteral("模板文件内容不完整或格式不正确。"),
                        QStringLiteral("barcodeParameters"), path);
        }
        const QJsonObject barcode = barcodeValue.toObject();
        int formatMask = 0;
        if (!hasOnlyKeys(barcode,
                         QStringList() << QStringLiteral("formatMask")
                                       << QStringLiteral("roiPaddingPercent")
                                       << QStringLiteral("maxDecodeTimeMs")
                                       << QStringLiteral("enableFallback"),
                         QStringLiteral("barcodeParameters"), error, path)
                || !readInt(barcode, "formatMask", &formatMask)
                || formatMask < 0
                || !readInt(barcode, "roiPaddingPercent",
                            &candidate.barcodeParameters.roiPaddingPercent)
                || !readInt(barcode, "maxDecodeTimeMs",
                            &candidate.barcodeParameters.maxDecodeTimeMs)
                || !barcode.value(QStringLiteral("enableFallback")).isBool()) {
            return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                        QStringLiteral("模板文件内容不完整或格式不正确。"),
                        QStringLiteral("barcodeParameters"), path);
        }
        candidate.barcodeParameters.formatMask =
                static_cast<unsigned int>(formatMask);
        candidate.barcodeParameters.enableFallback =
                barcode.value(QStringLiteral("enableFallback")).toBool();
    }
    if (!validateStructure(candidate, error, path)) {
        return false;
    }
    *settings = candidate;
    return true;
}

bool readSettings(const QString &directory,
                  TemplateSettings *settings,
                  TemplateStoreError *error)
{
    const QString path = QDir(directory).filePath(QLatin1String(kSettingsFile));
    QFile file(path);
    if (!file.exists()) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MISSING"),
                    QStringLiteral("模板文件内容不完整。"),
                    path, path);
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("TEMPLATE_SETTINGS_MALFORMED"),
                    QStringLiteral("无法读取模板文件。"),
                    file.errorString(), path);
    }
    return parseSettings(file.readAll(), settings, error, path);
}

bool readImage(const QString &path,
               int flags,
               bool required,
               cv::Mat *image,
               TemplateStoreError *error)
{
    QFile file(path);
    if (!file.exists() && !required) {
        image->release();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("TEMPLATE_RESOURCE_MISSING"),
                    QStringLiteral("模板缺少检测所需图片。"),
                    file.errorString(), path);
    }
    const QByteArray bytes = file.readAll();
    try {
        const std::vector<uchar> buffer(bytes.begin(), bytes.end());
        *image = cv::imdecode(buffer, flags);
    } catch (const cv::Exception &exception) {
        return fail(error, QStringLiteral("TEMPLATE_IMAGE_CORRUPT"),
                    QStringLiteral("模板图片已损坏。"),
                    QString::fromStdString(exception.what()), path);
    }
    if (image->empty()) {
        return fail(error, QStringLiteral("TEMPLATE_IMAGE_CORRUPT"),
                    QStringLiteral("模板图片已损坏。"), path, path);
    }
    return true;
}

bool loadCharacters(const QString &directory,
                    bool required,
                    QVector<TemplateCharacterAsset> *assets,
                    TemplateStoreError *error)
{
    const QString path = QDir(directory).filePath(
                QLatin1String(kCharactersDirectory));
    const QDir characterDirectory(path);
    if (!characterDirectory.exists()) {
        if (!required) {
            assets->clear();
            return true;
        }
        return fail(error, QStringLiteral("TEMPLATE_RESOURCE_MISSING"),
                    QStringLiteral("模板缺少字符图片。"), path, path);
    }
    const QFileInfoList files = characterDirectory.entryInfoList(
                QStringList() << QStringLiteral("*.png")
                              << QStringLiteral("*.bmp")
                              << QStringLiteral("*.jpg")
                              << QStringLiteral("*.jpeg"),
                QDir::Files | QDir::NoSymLinks, QDir::Name);
    QVector<TemplateCharacterAsset> result;
    const QRegularExpression validLetterStem(
                R"(^(?:upper_[A-Z]|lower_[a-z])(?:\(\d+\))?$)");
    const QRegularExpression legacyLetterStem(
                R"(^[A-Za-z](?:\(\d+\))?$)");
    for (const QFileInfo &file : files) {
        TemplateCharacterAsset asset;
        asset.fileName = file.fileName();
        asset.storageStem = file.completeBaseName().trimmed();
        const bool letterStem = legacyLetterStem.match(
                    asset.storageStem).hasMatch()
                || asset.storageStem.startsWith(
                    QStringLiteral("upper_"), Qt::CaseInsensitive)
                || asset.storageStem.startsWith(
                    QStringLiteral("lower_"), Qt::CaseInsensitive);
        if (letterStem && !validLetterStem.match(
                asset.storageStem).hasMatch()) {
            return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                        QStringLiteral(
                            "字母字符模板文件名无效，请重新分割并保存字符模板。"),
                        file.fileName(), file.absoluteFilePath());
        }
        if (asset.storageStem.isEmpty()
                || !readImage(file.absoluteFilePath(), cv::IMREAD_GRAYSCALE,
                              true, &asset.image, error)) {
            if (error && error->isEmpty()) {
                fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                     QStringLiteral("字符模板文件名无效。"),
                     file.fileName(), file.absoluteFilePath());
            }
            return false;
        }
        result.append(asset);
    }
    if (required && result.isEmpty()) {
        return fail(error, QStringLiteral("TEMPLATE_RESOURCE_MISSING"),
                    QStringLiteral("模板没有可用字符图片。"), path, path);
    }
    *assets = result;
    return true;
}

bool runtimeFieldsComplete(const TemplateSettings &settings,
                           QString *message)
{
    const DetectionModeDescriptor &descriptor =
            detectionModeDescriptor(settings.detectionMode);
    if (descriptor.requiresTargetText
            && settings.targetText.trimmed().isEmpty()) {
        if (message) {
            *message = QStringLiteral("目标文字尚未设置。");
        }
        return false;
    }
    if (settings.trackingRoi.width() <= 0.0
            || settings.trackingRoi.height() <= 0.0
            || settings.datePolygon.size() < 3) {
        if (message) {
            *message = QStringLiteral("定位区域或日期区域尚未设置完整。");
        }
        return false;
    }
    if (settings.detectionMode == DetectionMode::BarcodeWord
            && settings.barcodePolygon.size() != 4) {
        if (message) {
            *message = QStringLiteral("二维码区域必须包含四个点。");
        }
        return false;
    }
    if (settings.detectionMode == DetectionMode::Stamp
            && settings.stampPolygon.size() < 3) {
        if (message) {
            *message = QStringLiteral("钢印区域尚未设置完整。");
        }
        return false;
    }
    if (descriptor.requiresCharacterTemplates
            && (settings.characterSourceSize.isEmpty()
                || settings.characterBoxes.isEmpty())) {
        if (message) {
            *message = QStringLiteral("字符源图尺寸或字符框尚未设置。");
        }
        return false;
    }
    return true;
}

std::vector<cv::Point2f> cvPolygon(const QVector<QPointF> &polygon)
{
    std::vector<cv::Point2f> result;
    result.reserve(static_cast<std::size_t>(polygon.size()));
    for (const QPointF &point : polygon) {
        result.emplace_back(static_cast<float>(point.x()),
                            static_cast<float>(point.y()));
    }
    return result;
}

bool writeBytes(const QString &path,
                const QByteArray &bytes,
                TemplateStoreError *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size()
            || !file.commit()) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("模板文件保存失败。"),
                    file.errorString(), path);
    }
    return true;
}

bool writeImage(const QString &path,
                const cv::Mat &image,
                TemplateStoreError *error)
{
    if (image.empty()) {
        return true;
    }
    std::vector<uchar> encoded;
    try {
        const std::string extension = QFileInfo(path).suffix().isEmpty()
                ? std::string(".png")
                : std::string(".")
                  + QFileInfo(path).suffix().toStdString();
        if (!cv::imencode(extension, image, encoded)) {
            return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                        QStringLiteral("模板图片保存失败。"),
                        QString::fromStdString(extension), path);
        }
    } catch (const cv::Exception &exception) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("模板图片保存失败。"),
                    QString::fromStdString(exception.what()), path);
    }
    return writeBytes(path,
                      QByteArray(reinterpret_cast<const char *>(encoded.data()),
                                 static_cast<int>(encoded.size())),
                      error);
}

bool copyDirectoryContents(const QString &source,
                           const QString &target,
                           TemplateStoreError *error)
{
    const QDir sourceDirectory(source);
    if (!sourceDirectory.exists()) {
        return true;
    }
    if (!QDir().mkpath(target)) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("模板保存失败，请检查保存文件夹权限和磁盘空间。"), target, target);
    }
    const QFileInfoList entries = sourceDirectory.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden
                | QDir::System, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString destination = QDir(target).filePath(entry.fileName());
        if (entry.isSymLink()) {
            return fail(error, QStringLiteral("TEMPLATE_PATH_INVALID"),
                        QStringLiteral("所选模板文件夹包含不支持的链接，请选择普通文件夹。"),
                        entry.absoluteFilePath(), entry.absoluteFilePath());
        }
        if (entry.isDir()) {
            if (!copyDirectoryContents(entry.absoluteFilePath(),
                                       destination, error)) {
                return false;
            }
        } else if (!QFile::copy(entry.absoluteFilePath(), destination)) {
            return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                        QStringLiteral("复制模板原有内容失败。"),
                        entry.absoluteFilePath(), destination);
        }
    }
    return true;
}

bool writeEditable(const QString &directory,
                   const EditableTemplate &value,
                   TemplateStoreError *error)
{
    if (!QDir().mkpath(directory)) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("无法创建模板目录。"), directory, directory);
    }
    const QByteArray json = QJsonDocument(settingsToJson(value.settings))
            .toJson(QJsonDocument::Indented);
    if (!writeBytes(QDir(directory).filePath(QLatin1String(kSettingsFile)),
                    json, error)
            || !writeImage(QDir(directory).filePath(QLatin1String(kRawImageFile)),
                           value.rawImage, error)
            || !writeImage(QDir(directory).filePath(QLatin1String(kTrackingImageFile)),
                           value.trackingTemplate, error)
            || (value.settings.detectionMode == DetectionMode::Stamp
                && !writeImage(QDir(directory).filePath(
                                   QLatin1String(kStampRingFile)),
                               value.stampRingTemplate, error))) {
        return false;
    }
    const QString charactersPath = QDir(directory).filePath(
                QLatin1String(kCharactersDirectory));
    if (value.replaceCharacterAssets
            && QDir(charactersPath).exists()
            && !QDir(charactersPath).removeRecursively()) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("无法更新字符模板目录。"),
                    charactersPath, charactersPath);
    }
    if (!value.characterAssets.isEmpty()) {
        if (!QDir().mkpath(charactersPath)) {
            return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                        QStringLiteral("无法创建字符模板目录。"),
                        charactersPath, charactersPath);
        }
        for (const TemplateCharacterAsset &asset : value.characterAssets) {
            if (asset.fileName.isEmpty()
                    || QFileInfo(asset.fileName).fileName() != asset.fileName
                    || !writeImage(QDir(charactersPath).filePath(asset.fileName),
                                   asset.image, error)) {
                if (error && error->isEmpty()) {
                    fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                         QStringLiteral("字符模板文件名无效。"),
                         asset.fileName, charactersPath);
                }
                return false;
            }
        }
    }
    return true;
}

bool removeDirectory(const QString &path)
{
    QDir directory(path);
    return !directory.exists() || directory.removeRecursively();
}

}

TemplateSummary TemplateStore::readSummary(
    const QString &directoryPath,
    DetectionMode expectedMode,
    TemplateStoreError *error) const
{
    clearError(error);
    TemplateSummary summary;
    summary.directoryPath = normalizedDirectoryPath(directoryPath);
    summary.displayName = QFileInfo(summary.directoryPath).fileName();
    summary.detectionMode = expectedMode;
    const QFileInfo directory(summary.directoryPath);
    if (summary.directoryPath.isEmpty() || !directory.isAbsolute()
            || !directory.exists() || !directory.isDir()
            || directory.isSymLink()) {
        fail(error, QStringLiteral("TEMPLATE_DIRECTORY_MISSING"),
             QStringLiteral("模板文件夹不存在或路径无效。"),
             summary.directoryPath, summary.directoryPath);
        summary.message = error ? error->userMessage
                                : QStringLiteral("模板文件夹无效。");
        return summary;
    }
    TemplateSettings settings;
    if (!readSettings(summary.directoryPath, &settings, error)) {
        summary.message = error ? error->userMessage
                                : QStringLiteral("模板文件内容无效。");
        return summary;
    }
    summary.detectionMode = settings.detectionMode;
    if (settings.detectionMode != expectedMode) {
        fail(error, QStringLiteral("TEMPLATE_MODE_MISMATCH"),
             QStringLiteral("模板与当前检测模式不匹配。"),
             QStringLiteral("expected=%1 actual=%2")
             .arg(detectionModeId(expectedMode),
                  detectionModeId(settings.detectionMode)),
             summary.directoryPath);
        summary.message = error ? error->userMessage : QString();
        return summary;
    }
    EditableTemplate editable;
    if (!loadEditable(summary.directoryPath, expectedMode,
                      &editable, error)) {
        summary.message = error ? error->userMessage
                                : QStringLiteral("模板文件已损坏。");
        return summary;
    }
    summary.valid = true;
    PreparedTemplateSnapshot prepared;
    summary.complete = loadPrepared(summary.directoryPath, expectedMode,
                                    &prepared, error) && prepared;
    summary.message = summary.complete
            ? QStringLiteral("模板可用于检测。")
            : (error && !error->userMessage.isEmpty()
               ? error->userMessage
               : QStringLiteral("模板尚未制作完整。"));
    return summary;
}

bool TemplateStore::loadEditable(const QString &directoryPath,
                                 DetectionMode expectedMode,
                                 EditableTemplate *value,
                                 TemplateStoreError *error) const
{
    clearError(error);
    if (!value) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    QStringLiteral("模板加载失败，请联系维护人员。"),
                    QStringLiteral("Null EditableTemplate output."));
    }
    const QString directory = normalizedDirectoryPath(directoryPath);
    const QFileInfo info(directory);
    if (directory.isEmpty() || !info.exists() || !info.isDir()
            || info.isSymLink()) {
        return fail(error, QStringLiteral("TEMPLATE_DIRECTORY_MISSING"),
                    QStringLiteral("模板文件夹不存在或路径无效。"),
                    directory, directory);
    }
    EditableTemplate candidate;
    if (!readSettings(directory, &candidate.settings, error)
            || candidate.settings.detectionMode != expectedMode) {
        if (error && error->isEmpty()) {
            fail(error, QStringLiteral("TEMPLATE_MODE_MISMATCH"),
                 QStringLiteral("模板与当前检测模式不匹配。"),
                 directory, directory);
        }
        return false;
    }
    if (!readImage(QDir(directory).filePath(QLatin1String(kRawImageFile)),
                   cv::IMREAD_COLOR, false, &candidate.rawImage, error)
            || !readImage(QDir(directory).filePath(
                              QLatin1String(kTrackingImageFile)),
                          cv::IMREAD_COLOR, false,
                          &candidate.trackingTemplate, error)
            || (expectedMode == DetectionMode::Stamp
                && !readImage(QDir(directory).filePath(
                                  QLatin1String(kStampRingFile)),
                              cv::IMREAD_GRAYSCALE, false,
                              &candidate.stampRingTemplate, error))
            || !loadCharacters(directory, false,
                               &candidate.characterAssets, error)) {
        return false;
    }
    *value = candidate;
    return true;
}

bool TemplateStore::loadPrepared(const QString &directoryPath,
                                 DetectionMode expectedMode,
                                 PreparedTemplateSnapshot *value,
                                 TemplateStoreError *error) const
{
    clearError(error);
    if (!value) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    QStringLiteral("模板加载失败，请联系维护人员。"),
                    QStringLiteral("Null PreparedTemplate output."));
    }
    EditableTemplate editable;
    if (!loadEditable(directoryPath, expectedMode, &editable, error)) {
        return false;
    }
    QString incomplete;
    if (!runtimeFieldsComplete(editable.settings, &incomplete)) {
        return fail(error, QStringLiteral("TEMPLATE_FIELD_INVALID"),
                    incomplete, QStringLiteral("Runtime fields incomplete."),
                    normalizedDirectoryPath(directoryPath));
    }
    const QString directory = normalizedDirectoryPath(directoryPath);
    if (!readImage(QDir(directory).filePath(QLatin1String(kRawImageFile)),
                   cv::IMREAD_COLOR, true, &editable.rawImage, error)
            || !readImage(QDir(directory).filePath(
                              QLatin1String(kTrackingImageFile)),
                          cv::IMREAD_COLOR, true,
                          &editable.trackingTemplate, error)
            || (expectedMode == DetectionMode::Stamp
                && !readImage(QDir(directory).filePath(
                                  QLatin1String(kStampRingFile)),
                              cv::IMREAD_GRAYSCALE, true,
                              &editable.stampRingTemplate, error))) {
        return false;
    }
    const bool characterMode = detectionModeDescriptor(
                expectedMode).requiresCharacterTemplates;
    const bool charactersRequired = characterMode
            && !editable.settings.targetText.trimmed().isEmpty();
    if (!loadCharacters(directory, charactersRequired,
                        &editable.characterAssets, error)) {
        return false;
    }
    QStringList targetUnits;
    if (characterMode) {
        targetUnits = TemplateStore::templateTargetUnits(
                    editable.settings.targetText);
        if (!editable.settings.targetText.trimmed().isEmpty()
                && targetUnits.isEmpty()) {
            return fail(error, QStringLiteral("TEMPLATE_TARGET_INVALID"),
                        QStringLiteral("目标文字不包含可检测字符。"),
                        editable.settings.targetText, directory);
        }
        const QString missingTarget = missingTemplateTargetUnit(
                    expectedMode, targetUnits, editable.characterAssets);
        if (!missingTarget.isEmpty()) {
            return fail(error, QStringLiteral("TEMPLATE_CHARACTER_INVALID"),
                        QStringLiteral("模板缺少目标文字所需字符：“%1”。")
                        .arg(missingTarget),
                        editable.settings.targetText, directory);
        }
    }
    if (editable.trackingTemplate.cols
            != static_cast<int>(editable.settings.trackingRoi.width())
            || editable.trackingTemplate.rows
               != static_cast<int>(editable.settings.trackingRoi.height())) {
        return fail(error, QStringLiteral("TEMPLATE_RESOURCE_MISSING"),
                    QStringLiteral("定位模板尺寸与定位区域不一致。"),
                    QStringLiteral("tracking_template.bmp size mismatch"),
                    directory);
    }

    std::shared_ptr<PreparedTemplate> candidate(new PreparedTemplate);
    candidate->directoryPath = directory;
    candidate->displayName = QFileInfo(directory).fileName();
    candidate->settings = editable.settings;
    candidate->rawImage = editable.rawImage.clone();
    candidate->trackingTemplate = editable.trackingTemplate.clone();
    candidate->stampRingTemplate = editable.stampRingTemplate.clone();
    candidate->datePolygon = cvPolygon(editable.settings.datePolygon);
    candidate->barcodePolygon = cvPolygon(editable.settings.barcodePolygon);
    candidate->stampPolygon = cvPolygon(editable.settings.stampPolygon);
    candidate->targetUnits = targetUnits;
    for (const TemplateCharacterAsset &asset : editable.characterAssets) {
        candidate->characterAssets.push_back(asset);
    }
    for (int targetIndex = 0; targetIndex < targetUnits.size(); ++targetIndex) {
        for (const TemplateCharacterAsset &asset : editable.characterAssets) {
            if (!templateCharacterAssetMatchesTarget(
                    asset.storageStem, targetUnits.at(targetIndex))) {
                continue;
            }
            candidate->characterTemplates.push_back(asset.image);
            candidate->characterTemplateTargetIndexes.push_back(targetIndex);
        }
    }
    *value = candidate;
    return true;
}

bool TemplateStore::save(const QString &directoryPath,
                         const EditableTemplate &value,
                         bool preserveExistingContents,
                         TemplateStoreError *error) const
{
    clearError(error);
    const QString target = normalizedDirectoryPath(directoryPath);
    const QFileInfo targetInfo(target);
    if (target.isEmpty() || !targetInfo.isAbsolute()
            || targetInfo.isSymLink()) {
        return fail(error, QStringLiteral("TEMPLATE_PATH_INVALID"),
                    QStringLiteral("请选择有效的模板保存文件夹"), target, target);
    }
    if (targetInfo.exists() && !targetInfo.isDir()) {
        return fail(error, QStringLiteral("TEMPLATE_PATH_INVALID"),
                    QStringLiteral("所选模板保存位置不是文件夹。"),
                    target, target);
    }
    if (preserveExistingContents && !targetInfo.exists()) {
        return fail(error, QStringLiteral("TEMPLATE_DIRECTORY_MISSING"),
                    QStringLiteral("要编辑的模板文件夹不存在。"),
                    target, target);
    }
    if (!validateStructure(value.settings, error, target)) {
        return false;
    }
    const QString parent = targetInfo.absolutePath();
    if (!QDir().mkpath(parent)) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("无法创建模板保存文件夹。"), parent, parent);
    }
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString temporary = QDir(parent).filePath(
                QStringLiteral(".%1.tmp-%2")
                .arg(targetInfo.fileName(), token));
    const QString backup = QDir(parent).filePath(
                QStringLiteral(".%1.bak-%2")
                .arg(targetInfo.fileName(), token));
    if (!QDir().mkdir(temporary)) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("模板保存失败，请检查保存文件夹权限和磁盘空间。"),
                    temporary, temporary);
    }
    if (preserveExistingContents && targetInfo.exists()
            && !copyDirectoryContents(target, temporary, error)) {
        removeDirectory(temporary);
        return false;
    }
    if (!writeEditable(temporary, value, error)) {
        removeDirectory(temporary);
        return false;
    }
    EditableTemplate verified;
    if (!loadEditable(temporary, value.settings.detectionMode,
                      &verified, error)) {
        removeDirectory(temporary);
        return false;
    }

    const bool hadPrevious = targetInfo.exists();
    if (hadPrevious && !QDir().rename(target, backup)) {
        removeDirectory(temporary);
        return fail(error, QStringLiteral("TEMPLATE_COMMIT_FAILED"),
                    QStringLiteral("模板保存失败，原模板仍可使用。"),
                    target, target);
    }
    if (!QDir().rename(temporary, target)) {
        const bool rolledBack = !hadPrevious || QDir().rename(backup, target);
        if (!rolledBack) {
            return fail(error, QStringLiteral("TEMPLATE_ROLLBACK_FAILED"),
                        QStringLiteral("模板保存失败，原模板可能不可用，请联系维护人员。"),
                        QStringLiteral("temporary=%1; backup=%2")
                        .arg(temporary, backup), target);
        }
        removeDirectory(temporary);
        return fail(error, QStringLiteral("TEMPLATE_COMMIT_FAILED"),
                    QStringLiteral("模板保存失败，原模板仍可使用。"),
                    temporary, target);
    }
    if (hadPrevious && !removeDirectory(backup)) {
        return fail(error, QStringLiteral("TEMPLATE_SAVE_FAILED"),
                    QStringLiteral("模板已保存，但旧备份文件未能清理，请联系维护人员。"),
                    backup, backup);
    }
    return true;
}

QStringList TemplateStore::templateTargetUnits(const QString &targetText)
{
    QStringList units;
    const QRegularExpression expression(
                R"((\d\(\d+\))|(\d)|([A-Za-z])|([一-龥]))");
    QRegularExpressionMatchIterator matches =
            expression.globalMatch(targetText);
    while (matches.hasNext()) {
        const QString value = matches.next().captured(0);
        if (!value.isEmpty()) {
            units.append(value);
        }
    }
    return units;
}

QString characterStorageStem(const QString &unit)
{
    const QString value = unit.trimmed();
    if (value.size() != 1) {
        return value;
    }
    const QChar character = value.at(0);
    if (character >= QLatin1Char('A') && character <= QLatin1Char('Z')) {
        return QStringLiteral("upper_") + character;
    }
    if (character >= QLatin1Char('a') && character <= QLatin1Char('z')) {
        return QStringLiteral("lower_") + character;
    }
    return value;
}

bool templateCharacterAssetMatchesTarget(
    const QString &storageStem,
    const QString &target)
{
    const QString base = storageStem.trimmed();
    const QString expected = characterStorageStem(target);
    if (base == expected) {
        return true;
    }
    if (!base.startsWith(expected)) {
        return false;
    }
    const QString suffix = base.mid(expected.size());
    static const QRegularExpression variantSuffix(
                R"(^\(\d+\)$)");
    return variantSuffix.match(suffix).hasMatch();
}

QString missingTemplateTargetUnit(
    DetectionMode mode,
    const QStringList &targetUnits,
    const QVector<TemplateCharacterAsset> &characterAssets)
{
    if (!detectionModeDescriptor(mode).requiresCharacterTemplates) {
        return QString();
    }
    for (const QString &target : targetUnits) {
        bool matched = false;
        for (const TemplateCharacterAsset &asset : characterAssets) {
            if (templateCharacterAssetMatchesTarget(
                    asset.storageStem, target)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return target;
        }
    }
    return QString();
}
