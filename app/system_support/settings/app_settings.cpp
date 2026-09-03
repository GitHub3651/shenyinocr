// 文件作用：实现应用设置默认值、检测方案路径规则和相等比较。
#include "system_support/settings/app_settings.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

const int AppSettings::CurrentSchemaVersion;

namespace {

QStringList normalizedTemplatePaths(const QStringList &paths)
{
    QStringList normalized;
    QSet<QString> seen;
    for (const QString &path : paths) {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QString absolute = QDir::cleanPath(
                    QFileInfo(trimmed).absoluteFilePath());
        const QString key = absolute.toCaseFolded();
        if (!seen.contains(key)) {
            seen.insert(key);
            normalized.append(absolute);
        }
    }
    return normalized;
}

}

QStringList DetectionSchemes::templatePaths(DetectionMode mode) const
{
    switch (mode) {
    case DetectionMode::Stamp:
        return stampTemplatePath.trimmed().isEmpty()
                ? QStringList() : QStringList() << stampTemplatePath;
    case DetectionMode::Word:
        return wordTemplatePaths;
    case DetectionMode::Ocr:
        return ocrTemplatePath.trimmed().isEmpty()
                ? QStringList() : QStringList() << ocrTemplatePath;
    case DetectionMode::BarcodeWord:
        return barcodeWordTemplatePaths;
    case DetectionMode::Tissue:
        return QStringList();
    }
    return QStringList();
}

bool DetectionSchemes::setTemplatePaths(DetectionMode mode,
                                        const QStringList &paths,
                                        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const QStringList normalized = normalizedTemplatePaths(paths);
    const DetectionTrackingKind trackingKind =
            detectionModeDescriptor(mode).trackingKind;
    if (trackingKind == DetectionTrackingKind::SingleTemplate
            && normalized.size() > 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前检测模式最多只能选择一个模板。");
        }
        return false;
    }

    switch (mode) {
    case DetectionMode::Stamp:
        stampTemplatePath = normalized.value(0);
        return true;
    case DetectionMode::Word:
        wordTemplatePaths = normalized;
        return true;
    case DetectionMode::Ocr:
        ocrTemplatePath = normalized.value(0);
        return true;
    case DetectionMode::BarcodeWord:
        barcodeWordTemplatePaths = normalized;
        return true;
    case DetectionMode::Tissue:
        if (errorMessage) {
            *errorMessage = QStringLiteral("纸巾检测不使用模板。");
        }
        return false;
    }
    return false;
}

bool operator==(const DetectionSchemes &left,
                const DetectionSchemes &right)
{
    return left.stampTemplatePath == right.stampTemplatePath
            && left.wordTemplatePaths == right.wordTemplatePaths
            && left.ocrTemplatePath == right.ocrTemplatePath
            && left.tissueRoughnessThreshold
               == right.tissueRoughnessThreshold
            && left.barcodeWordTemplatePaths
               == right.barcodeWordTemplatePaths;
}

bool operator!=(const DetectionSchemes &left,
                const DetectionSchemes &right)
{
    return !(left == right);
}

AppSettings::AppSettings()
    : schemaVersion(CurrentSchemaVersion),
      detectModeId(detectionModeUiId(DetectionMode::Word)),
      imageSaveModeId(QStringLiteral("save_none")),
      imageSaveTypeId(QStringLiteral("save_annotated_only")),
      imageJpegQuality(92),
      cameraExposure(800),
      cameraGain(1),
      colorChannelId(QStringLiteral("color")),
      imageRotationId(QStringLiteral("rotate_none")),
      triggerEnabled(true),
      cameraDelay(300),
      triggerModeId(QStringLiteral("trigger_interval")),
      plcIp(QStringLiteral("192.168.10.10")),
      plcRack(0),
      plcSlot(1),
      photoDistance(50),
      photoTime(300),
      rejectDistance(500),
      rejectTime(300),
      rejectPosition(0),
      plcTriggerModeDb(1),
      plcTriggerModeOffset(1032),
      plcResultDb(1),
      plcResultOffset(1033),
      plcPhotoDistanceOffset(924),
      plcPhotoTimeOffset(982),
      plcRejectDistanceOffset(920),
      plcRejectTimeOffset(980),
      resultExportEnabled(false)
{
    resultExportReceiverIp = QStringLiteral("192.168.10.20");
    resultExportReceiverPort = 35680;
}

QStringList appSettingsDetectionModeIds()
{
    QStringList ids;
    for (const DetectionModeDescriptor &descriptor : detectionModeDescriptors()) {
        ids.append(QLatin1String(descriptor.uiId));
    }
    return ids;
}

QStringList appSettingsImageSaveModeIds()
{
    return QStringList() << QStringLiteral("save_none")
                         << QStringLiteral("save_ng")
                         << QStringLiteral("save_ok")
                         << QStringLiteral("save_all");
}

QStringList appSettingsImageSaveTypeIds()
{
    return QStringList() << QStringLiteral("save_both")
                         << QStringLiteral("save_annotated_only")
                         << QStringLiteral("save_raw_only");
}

QStringList appSettingsColorChannelIds()
{
    return QStringList() << QStringLiteral("color")
                         << QStringLiteral("red")
                         << QStringLiteral("green")
                         << QStringLiteral("blue");
}

QStringList appSettingsRotationIds()
{
    return QStringList() << QStringLiteral("rotate_none")
                         << QStringLiteral("rotate_clockwise_90")
                         << QStringLiteral("rotate_counterclockwise_90")
                         << QStringLiteral("rotate_180");
}

QStringList appSettingsTriggerModeIds()
{
    return QStringList() << QStringLiteral("trigger_continuous")
                         << QStringLiteral("trigger_interval");
}

AppSettings AppSettings::defaults()
{
    return AppSettings();
}

bool operator==(const AppSettings &left,
                const AppSettings &right)
{
    return left.schemaVersion == right.schemaVersion
            && left.detectModeId == right.detectModeId
            && left.imageSaveModeId == right.imageSaveModeId
            && left.imageSaveTypeId == right.imageSaveTypeId
            && left.imageSavePath == right.imageSavePath
            && left.imageJpegQuality == right.imageJpegQuality
            && left.cameraExposure == right.cameraExposure
            && left.cameraGain == right.cameraGain
            && left.colorChannelId == right.colorChannelId
            && left.imageRotationId == right.imageRotationId
            && left.triggerEnabled == right.triggerEnabled
            && left.cameraDelay == right.cameraDelay
            && left.triggerModeId == right.triggerModeId
            && left.plcIp == right.plcIp
            && left.plcRack == right.plcRack
            && left.plcSlot == right.plcSlot
            && left.photoDistance == right.photoDistance
            && left.photoTime == right.photoTime
            && left.rejectDistance == right.rejectDistance
            && left.rejectTime == right.rejectTime
            && left.rejectPosition == right.rejectPosition
            && left.plcTriggerModeDb == right.plcTriggerModeDb
            && left.plcTriggerModeOffset == right.plcTriggerModeOffset
            && left.plcResultDb == right.plcResultDb
            && left.plcResultOffset == right.plcResultOffset
            && left.plcPhotoDistanceOffset == right.plcPhotoDistanceOffset
            && left.plcPhotoTimeOffset == right.plcPhotoTimeOffset
            && left.plcRejectDistanceOffset == right.plcRejectDistanceOffset
            && left.plcRejectTimeOffset == right.plcRejectTimeOffset
            && left.templateSaveDirectory == right.templateSaveDirectory
            && left.resultExportEnabled == right.resultExportEnabled
            && left.resultExportReceiverIp == right.resultExportReceiverIp
            && left.resultExportReceiverPort == right.resultExportReceiverPort
            && left.detectionSchemes == right.detectionSchemes;
}

bool operator!=(const AppSettings &left,
                const AppSettings &right)
{
    return !(left == right);
}
