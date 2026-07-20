#include "appsettingsmanager.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

const int AppSettingsManager::GlobalConfigVersion;
const int AppSettingsManager::TemplateConfigVersion;

namespace SettingsKeys {
namespace Meta {
const char ConfigVersion[] = "Meta/config_version";
const char ConfigType[] = "Meta/config_type";
}

namespace Global {
const char DetectMode[] = "Global/detect_mode";
const char ImageSaveMode[] = "Global/image_save_mode";
const char ImageSaveType[] = "Global/image_save_type";
const char ImageSavePath[] = "Global/image_save_path";
const char TemplateBaseDir[] = "Global/template_base_dir";
const char CameraExposure[] = "Global/camera_exposure";
const char CameraGain[] = "Global/camera_gain";
const char ColorChannel[] = "Global/color_channel";
const char ImageRotation[] = "Global/image_rotation";
const char TriggerEnabled[] = "Global/trigger_enabled";
const char TriggerMode[] = "Global/trigger_mode";
const char PlcMode[] = "Global/plc_mode";
const char PlcIp[] = "Global/plc_ip";
const char PlcRack[] = "Global/plc_rack";
const char PlcSlot[] = "Global/plc_slot";
const char PhotoDistance[] = "Global/photo_distance";
const char PhotoTime[] = "Global/photo_time";
const char CameraDelay[] = "Global/camera_delay";
const char RejectDistance[] = "Global/reject_distance";
const char RejectTime[] = "Global/reject_time";
const char RejectPosition[] = "Global/reject_position";
const char TissueRoughnessThreshold[] = "Global/tissue_roughness_threshold";

QString templatePathsKey(const QString &modeId)
{
    return QString("TemplatePaths/%1").arg(modeId);
}
}

namespace Template {
const char TargetText[] = "Template/target_text";
const char ImageThreshold[] = "Template/image_threshold";
const char TrackingBoxX[] = "Template/tracking_box_x";
const char TrackingBoxY[] = "Template/tracking_box_y";
const char TrackingBoxWidth[] = "Template/tracking_box_width";
const char TrackingBoxHeight[] = "Template/tracking_box_height";
const char HasValidBoxes[] = "Template/has_valid_boxes";
}

namespace CharacterBoxes {
const char Count[] = "CharacterTemplateBoxes/count";
const char SourceWidth[] = "CharacterTemplateBoxes/source_width";
const char SourceHeight[] = "CharacterTemplateBoxes/source_height";

QString nameKey(int index) { return QString("CharacterTemplateBoxes/box_%1_name").arg(index); }
QString xKey(int index) { return QString("CharacterTemplateBoxes/box_%1_x").arg(index); }
QString yKey(int index) { return QString("CharacterTemplateBoxes/box_%1_y").arg(index); }
QString widthKey(int index) { return QString("CharacterTemplateBoxes/box_%1_w").arg(index); }
QString heightKey(int index) { return QString("CharacterTemplateBoxes/box_%1_h").arg(index); }
}
}

namespace {
const char GlobalConfigType[] = "global";
const char TemplateConfigType[] = "template_private";
const QStringList SupportedDetectModeIds = {
    "stamp_detection",
    "word_detection",
    "ocr_detection",
    "tissue_detection"
};
const QStringList SupportedImageSaveModeIds = {
    "save_none",
    "save_ng",
    "save_ok",
    "save_all"
};
const QStringList SupportedImageSaveTypeIds = {
    "save_both",
    "save_annotated_only",
    "save_raw_only"
};
const QStringList SupportedColorChannelIds = {
    "color",
    "red",
    "green",
    "blue"
};
const QStringList SupportedImageRotationIds = {
    "rotate_none",
    "rotate_clockwise_90",
    "rotate_counterclockwise_90",
    "rotate_180"
};
const QStringList SupportedTriggerModeIds = {
    "trigger_continuous",
    "trigger_interval"
};

QString normalizeId(const QString &value,
                    const QStringList &validIds,
                    const QString &defaultId)
{
    const QString trimmedValue = value.trimmed();
    return validIds.contains(trimmedValue) ? trimmedValue : defaultId;
}

bool normalizeGlobalSettingValues(GlobalSettings *settings)
{
    if (!settings) {
        return false;
    }

    const GlobalSettings defaults = AppSettingsManager::defaultGlobalSettings();
    bool changed = false;

    auto normalizeField = [&changed](QString *field,
                                     const QStringList &validIds,
                                     const QString &defaultId) {
        const QString normalizedValue = normalizeId(*field, validIds, defaultId);
        if (*field != normalizedValue) {
            *field = normalizedValue;
            changed = true;
        }
    };

    normalizeField(&settings->detectModeId,
                   SupportedDetectModeIds,
                   defaults.detectModeId);
    normalizeField(&settings->imageSaveModeId,
                   SupportedImageSaveModeIds,
                   defaults.imageSaveModeId);
    normalizeField(&settings->imageSaveTypeId,
                   SupportedImageSaveTypeIds,
                   defaults.imageSaveTypeId);
    normalizeField(&settings->colorChannelId,
                   SupportedColorChannelIds,
                   defaults.colorChannelId);
    normalizeField(&settings->imageRotationId,
                   SupportedImageRotationIds,
                   defaults.imageRotationId);

    QString normalizedTriggerMode = normalizeId(settings->triggerModeId,
                                                SupportedTriggerModeIds,
                                                QString());
    if (normalizedTriggerMode.isEmpty()) {
        normalizedTriggerMode = normalizeId(settings->plcModeId,
                                            SupportedTriggerModeIds,
                                            defaults.triggerModeId);
    }
    if (settings->triggerModeId != normalizedTriggerMode
            || settings->plcModeId != normalizedTriggerMode) {
        settings->triggerModeId = normalizedTriggerMode;
        settings->plcModeId = normalizedTriggerMode;
        changed = true;
    }

    QString normalizedPlcIp = settings->plcIp.trimmed();
    if (normalizedPlcIp.isEmpty()) {
        normalizedPlcIp = defaults.plcIp;
    }
    if (settings->plcIp != normalizedPlcIp) {
        settings->plcIp = normalizedPlcIp;
        changed = true;
    }

    return changed;
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString unicodeText(const wchar_t *text)
{
    return QString::fromWCharArray(text);
}

bool removeIfExists(const QString &path, QString *errorMessage)
{
    if (!QFileInfo::exists(path)) {
        return true;
    }
    if (QFile::remove(path)) {
        return true;
    }
    setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u5220\u9664\u4e34\u65f6\u914d\u7f6e\u6587\u4ef6\uff1a%1").arg(path));
    return false;
}

bool replaceWithTempFile(const QString &tempPath,
                         const QString &targetPath,
                         const QString &backupPath,
                         bool keepBackup,
                         QString *errorMessage)
{
    const bool targetExists = QFileInfo::exists(targetPath);
    if (targetExists) {
        if (!removeIfExists(backupPath, errorMessage)) {
            QFile::remove(tempPath);
            return false;
        }
        if (!QFile::copy(targetPath, backupPath)) {
            setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u5907\u4efd\u539f\u914d\u7f6e\u6587\u4ef6\uff1a%1").arg(targetPath));
            QFile::remove(tempPath);
            return false;
        }
    }

    if (targetExists && !QFile::remove(targetPath)) {
        setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u66ff\u6362\u539f\u914d\u7f6e\u6587\u4ef6\uff1a%1").arg(targetPath));
        QFile::remove(tempPath);
        return false;
    }

    if (!QFile::rename(tempPath, targetPath)) {
        bool restored = !targetExists;
        if (targetExists && QFileInfo::exists(backupPath)) {
            if (QFileInfo::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            restored = QFile::copy(backupPath, targetPath);
        }
        QFile::remove(tempPath);
        if (!targetExists) {
            setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u63d0\u4ea4\u65b0\u914d\u7f6e\u6587\u4ef6\uff1a%1").arg(targetPath));
        } else {
            setError(errorMessage,
                     restored
                     ? unicodeText(L"\u65e0\u6cd5\u63d0\u4ea4\u65b0\u914d\u7f6e\u6587\u4ef6\uff0c\u5df2\u6062\u590d\u539f\u914d\u7f6e\uff1a%1").arg(targetPath)
                     : unicodeText(L"\u65e0\u6cd5\u63d0\u4ea4\u65b0\u914d\u7f6e\u6587\u4ef6\uff0c\u5e76\u4e14\u539f\u914d\u7f6e\u6062\u590d\u5931\u8d25\uff1a%1").arg(targetPath));
        }
        return false;
    }

    if (!keepBackup && QFileInfo::exists(backupPath)) {
        QFile::remove(backupPath);
    }
    return true;
}

bool validateHeader(QSettings &settings,
                    const QString &expectedType,
                    int supportedVersion,
                    bool allowOlderVersion,
                    int *actualVersion,
                    QString *errorMessage)
{
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, unicodeText(L"\u914d\u7f6e\u6587\u4ef6\u89e3\u6790\u5931\u8d25\u3002\u8bf7\u68c0\u67e5\u6587\u4ef6\u662f\u5426\u5b8c\u6574\u3002") );
        return false;
    }
    if (!settings.contains(SettingsKeys::Meta::ConfigVersion)
            || !settings.contains(SettingsKeys::Meta::ConfigType)) {
        if (expectedType == QLatin1String(TemplateConfigType)) {
            setError(errorMessage, unicodeText(L"\u4ea7\u54c1\u6a21\u677f\u914d\u7f6e\u4e0d\u5b8c\u6574\uff0c\u65e0\u6cd5\u4f7f\u7528\u3002\n\u8bf7\u91cd\u65b0\u5236\u4f5c\u8be5\u4ea7\u54c1\u6a21\u677f\uff1a\n1. \u70b9\u51fb\u3010\u5236\u4f5c\u6a21\u677f\u3011\u62cd\u7167\n2. \u6846\u9009\u5b9a\u4f4d\u533a\u57df\u548c\u55b7\u7801\u68c0\u6d4b\u533a\u57df\n3. \u70b9\u51fb\u3010\u4fdd\u5b58\u6a21\u677f\u3011\n4. \u518d\u5206\u5272\u5b57\u7b26\u6a21\u677f\u5e76\u786e\u8ba4\u76ee\u6807\u5b57\u7b26") );
        } else {
            setError(errorMessage, unicodeText(L"\u8f6f\u4ef6\u516c\u5171\u914d\u7f6e\u4e0d\u5b8c\u6574\uff0c\u65e0\u6cd5\u8bfb\u53d6\u3002\u8bf7\u6e05\u7a7a\u5f53\u524d\u8f6f\u4ef6\u6570\u636e\u540e\u91cd\u65b0\u8bbe\u7f6e\u3002") );
        }
        return false;
    }

    const QString configType = settings.value(SettingsKeys::Meta::ConfigType).toString();
    const int configVersion = settings.value(SettingsKeys::Meta::ConfigVersion).toInt();
    if (actualVersion) {
        *actualVersion = configVersion;
    }
    if (configType != expectedType) {
        setError(errorMessage, unicodeText(L"\u914d\u7f6e\u7c7b\u578b\u9519\u8bef\uff1a%1").arg(configType));
        return false;
    }
    if (configVersion > supportedVersion) {
        setError(errorMessage, unicodeText(L"\u914d\u7f6e\u7248\u672c %1 \u9ad8\u4e8e\u5f53\u524d\u7a0b\u5e8f\u652f\u6301\u7684\u7248\u672c %2\u3002")
                 .arg(configVersion).arg(supportedVersion));
        return false;
    }
    if (configVersion < supportedVersion) {
        if (allowOlderVersion) {
            return true;
        }
        setError(errorMessage, unicodeText(L"\u914d\u7f6e\u7248\u672c %1 \u6682\u65e0\u8fc1\u79fb\u89c4\u5219\uff0c\u5f53\u524d\u7a0b\u5e8f\u9700\u8981\u7248\u672c %2\u3002")
                 .arg(configVersion).arg(supportedVersion));
        return false;
    }
    return true;
}

bool validateWrittenFile(const QString &path,
                         const QString &expectedType,
                         int expectedVersion,
                         const QStringList &requiredKeys,
                         QString *errorMessage)
{
    QSettings verify(path, QSettings::IniFormat);
    int actualVersion = 0;
    if (!validateHeader(verify, expectedType, expectedVersion, false, &actualVersion, errorMessage)) {
        return false;
    }
    for (const QString &key : requiredKeys) {
        if (!verify.contains(key)) {
            setError(errorMessage, unicodeText(L"\u914d\u7f6e\u5199\u5165\u540e\u7f3a\u5c11\u5b57\u6bb5\uff1a%1").arg(key));
            return false;
        }
    }
    verify.sync();
    if (verify.status() != QSettings::NoError) {
        setError(errorMessage, unicodeText(L"\u914d\u7f6e\u5199\u5165\u540e\u6821\u9a8c\u5931\u8d25\uff1a%1").arg(path));
        return false;
    }
    return true;
}
}

QString AppSettingsManager::globalDataDirPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.trimmed().isEmpty()) {
        path = QDir::home().filePath("AppData/Roaming/ShengYin");
    }
    return QDir::cleanPath(path);
}

QString AppSettingsManager::globalSettingsFilePath()
{
    return QDir(globalDataDirPath()).filePath("settings.ini");
}

GlobalSettings AppSettingsManager::defaultGlobalSettings()
{
    GlobalSettings settings;
    settings.configVersion = GlobalConfigVersion;
    settings.detectModeId = "word_detection";
    settings.imageSaveModeId = "save_none";
    settings.imageSaveTypeId = "save_annotated_only";
    settings.colorChannelId = "color";
    settings.imageRotationId = "rotate_none";
    settings.triggerModeId = "trigger_interval";
    settings.plcModeId = "trigger_interval";
    settings.plcIp = "192.168.10.10";
    settings.plcRack = 0;
    settings.plcSlot = 1;
    return settings;
}

bool AppSettingsManager::loadGlobalSettings(GlobalSettings *settings, QString *errorMessage)
{
    if (!settings) {
        setError(errorMessage, unicodeText(L"\u516c\u5171\u914d\u7f6e\u63a5\u6536\u5bf9\u8c61\u4e3a\u7a7a\u3002") );
        return false;
    }

    *settings = defaultGlobalSettings();
    const QString path = globalSettingsFilePath();
    if (!QFileInfo::exists(path)) {
        return true;
    }

    QSettings ini(path, QSettings::IniFormat);
    int fileVersion = 0;
    if (!validateHeader(ini, GlobalConfigType, GlobalConfigVersion, true, &fileVersion, errorMessage)) {
        return false;
    }

    settings->configVersion = GlobalConfigVersion;
    settings->detectModeId = ini.value(SettingsKeys::Global::DetectMode, settings->detectModeId).toString();
    settings->imageSaveModeId = ini.value(SettingsKeys::Global::ImageSaveMode, settings->imageSaveModeId).toString();
    settings->imageSaveTypeId = ini.value(SettingsKeys::Global::ImageSaveType, settings->imageSaveTypeId).toString();
    settings->imageSavePath = ini.value(SettingsKeys::Global::ImageSavePath).toString();
    settings->templateBaseDirPath = ini.value(SettingsKeys::Global::TemplateBaseDir).toString();
    settings->cameraExposure = ini.value(SettingsKeys::Global::CameraExposure, settings->cameraExposure).toInt();
    settings->cameraGain = ini.value(SettingsKeys::Global::CameraGain, settings->cameraGain).toDouble();
    settings->colorChannelId = ini.value(SettingsKeys::Global::ColorChannel, settings->colorChannelId).toString();
    settings->imageRotationId = ini.value(SettingsKeys::Global::ImageRotation, settings->imageRotationId).toString();
    settings->triggerEnabled = ini.value(SettingsKeys::Global::TriggerEnabled, settings->triggerEnabled).toBool();
    settings->triggerModeId = ini.value(SettingsKeys::Global::TriggerMode, settings->triggerModeId).toString();
    settings->plcModeId = ini.value(SettingsKeys::Global::PlcMode, settings->plcModeId).toString();
    settings->plcIp = ini.value(SettingsKeys::Global::PlcIp).toString();
    settings->plcRack = ini.value(SettingsKeys::Global::PlcRack, settings->plcRack).toInt();
    settings->plcSlot = ini.value(SettingsKeys::Global::PlcSlot, settings->plcSlot).toInt();
    settings->photoDistance = ini.value(SettingsKeys::Global::PhotoDistance, settings->photoDistance).toInt();
    settings->photoTime = ini.value(SettingsKeys::Global::PhotoTime, settings->photoTime).toInt();
    settings->cameraDelay = ini.value(SettingsKeys::Global::CameraDelay, settings->cameraDelay).toInt();
    settings->rejectDistance = ini.value(SettingsKeys::Global::RejectDistance, settings->rejectDistance).toInt();
    settings->rejectTime = ini.value(SettingsKeys::Global::RejectTime, settings->rejectTime).toInt();
    settings->rejectPosition = ini.value(SettingsKeys::Global::RejectPosition, settings->rejectPosition).toInt();
    settings->tissueRoughnessThreshold = ini.value(SettingsKeys::Global::TissueRoughnessThreshold,
                                                   settings->tissueRoughnessThreshold).toDouble();
    settings->templateDirPathsByMode.clear();
    for (const QString &modeId : SupportedDetectModeIds) {
        settings->templateDirPathsByMode.insert(
                    modeId,
                    ini.value(SettingsKeys::Global::templatePathsKey(modeId)).toStringList());
    }
    if (ini.status() != QSettings::NoError) {
        setError(errorMessage, unicodeText(L"\u516c\u5171\u914d\u7f6e\u8bfb\u53d6\u5931\u8d25\uff1a%1").arg(path));
        *settings = defaultGlobalSettings();
        return false;
    }

    const bool repairedInvalidValues = normalizeGlobalSettingValues(settings);
    if (fileVersion < GlobalConfigVersion) {
        QString saveError;
        if (!saveGlobalSettings(*settings, &saveError)) {
            setError(errorMessage, unicodeText(L"\u516c\u5171\u914d\u7f6e\u81ea\u52a8\u5347\u7ea7\u5931\u8d25\uff1a%1").arg(saveError));
            return false;
        }
    } else if (repairedInvalidValues) {
        QString saveError;
        if (!saveGlobalSettings(*settings, &saveError)) {
            qWarning().noquote()
                    << "[GLOBAL_SETTINGS] failed to persist normalized values:"
                    << path
                    << saveError;
        }
    }
    return true;
}

bool AppSettingsManager::saveGlobalSettings(const GlobalSettings &settings, QString *errorMessage)
{
    QDir dataDir(globalDataDirPath());
    if (!dataDir.exists() && !QDir().mkpath(dataDir.absolutePath())) {
        setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u521b\u5efa\u8f6f\u4ef6\u6570\u636e\u6587\u4ef6\u5939\uff1a%1").arg(dataDir.absolutePath()));
        return false;
    }

    const QString targetPath = globalSettingsFilePath();
    const QString tempPath = targetPath + ".tmp";
    const QString rollbackPath = targetPath + ".rollback";
    if (!removeIfExists(tempPath, errorMessage)) {
        return false;
    }

    {
        QSettings ini(tempPath, QSettings::IniFormat);
        ini.clear();
        ini.setValue(SettingsKeys::Meta::ConfigType, GlobalConfigType);
        ini.setValue(SettingsKeys::Meta::ConfigVersion, GlobalConfigVersion);
        ini.setValue(SettingsKeys::Global::DetectMode, settings.detectModeId);
        ini.setValue(SettingsKeys::Global::ImageSaveMode, settings.imageSaveModeId);
        ini.setValue(SettingsKeys::Global::ImageSaveType, settings.imageSaveTypeId);
        ini.setValue(SettingsKeys::Global::ImageSavePath, settings.imageSavePath);
        ini.setValue(SettingsKeys::Global::TemplateBaseDir, settings.templateBaseDirPath);
        ini.setValue(SettingsKeys::Global::CameraExposure, settings.cameraExposure);
        ini.setValue(SettingsKeys::Global::CameraGain, settings.cameraGain);
        ini.setValue(SettingsKeys::Global::ColorChannel, settings.colorChannelId);
        ini.setValue(SettingsKeys::Global::ImageRotation, settings.imageRotationId);
        ini.setValue(SettingsKeys::Global::TriggerEnabled, settings.triggerEnabled);
        ini.setValue(SettingsKeys::Global::TriggerMode, settings.triggerModeId);
        ini.setValue(SettingsKeys::Global::PlcMode, settings.plcModeId);
        ini.setValue(SettingsKeys::Global::PlcIp, settings.plcIp);
        ini.setValue(SettingsKeys::Global::PlcRack, settings.plcRack);
        ini.setValue(SettingsKeys::Global::PlcSlot, settings.plcSlot);
        ini.setValue(SettingsKeys::Global::PhotoDistance, settings.photoDistance);
        ini.setValue(SettingsKeys::Global::PhotoTime, settings.photoTime);
        ini.setValue(SettingsKeys::Global::CameraDelay, settings.cameraDelay);
        ini.setValue(SettingsKeys::Global::RejectDistance, settings.rejectDistance);
        ini.setValue(SettingsKeys::Global::RejectTime, settings.rejectTime);
        ini.setValue(SettingsKeys::Global::RejectPosition, settings.rejectPosition);
        ini.setValue(SettingsKeys::Global::TissueRoughnessThreshold, settings.tissueRoughnessThreshold);
        for (const QString &modeId : SupportedDetectModeIds) {
            ini.setValue(SettingsKeys::Global::templatePathsKey(modeId),
                         settings.templateDirPathsByMode.value(modeId));
        }
        ini.sync();
        if (ini.status() != QSettings::NoError) {
            setError(errorMessage, unicodeText(L"\u516c\u5171\u914d\u7f6e\u5199\u5165\u5931\u8d25\uff1a%1").arg(tempPath));
            QFile::remove(tempPath);
            return false;
        }
    }

    const QStringList requiredKeys = {
        SettingsKeys::Global::DetectMode,
        SettingsKeys::Global::ImageSaveMode,
        SettingsKeys::Global::ImageSaveType,
        SettingsKeys::Global::ImageSavePath,
        SettingsKeys::Global::TemplateBaseDir,
        SettingsKeys::Global::CameraExposure,
        SettingsKeys::Global::CameraGain,
        SettingsKeys::Global::ColorChannel,
        SettingsKeys::Global::ImageRotation,
        SettingsKeys::Global::TriggerEnabled,
        SettingsKeys::Global::TriggerMode,
        SettingsKeys::Global::PlcMode,
        SettingsKeys::Global::PlcIp,
        SettingsKeys::Global::PlcRack,
        SettingsKeys::Global::PlcSlot,
        SettingsKeys::Global::PhotoDistance,
        SettingsKeys::Global::PhotoTime,
        SettingsKeys::Global::CameraDelay,
        SettingsKeys::Global::RejectDistance,
        SettingsKeys::Global::RejectTime,
        SettingsKeys::Global::RejectPosition,
        SettingsKeys::Global::TissueRoughnessThreshold
    };
    if (!validateWrittenFile(tempPath,
                             GlobalConfigType,
                             GlobalConfigVersion,
                             requiredKeys,
                             errorMessage)) {
        QFile::remove(tempPath);
        return false;
    }
    return replaceWithTempFile(tempPath, targetPath, rollbackPath, false, errorMessage);
}

TemplatePrivateSettings AppSettingsManager::defaultTemplatePrivateSettings()
{
    TemplatePrivateSettings settings;
    settings.configVersion = TemplateConfigVersion;
    settings.characterSourceImageSize = QSize(0, 0);
    return settings;
}

bool AppSettingsManager::loadTemplatePrivateSettings(const QString &templateDir,
                                                     TemplatePrivateSettings *settings,
                                                     QString *errorMessage)
{
    if (!settings) {
        setError(errorMessage, unicodeText(L"\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u63a5\u6536\u5bf9\u8c61\u4e3a\u7a7a\u3002") );
        return false;
    }
    *settings = defaultTemplatePrivateSettings();

    const QString path = QDir(templateDir).filePath("app_settings.appset");
    if (!QFileInfo::exists(path)) {
        setError(errorMessage, unicodeText(L"\u7f3a\u5c11\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u6587\u4ef6\uff1a%1").arg(path));
        return false;
    }

    QSettings ini(path, QSettings::IniFormat);
    int fileVersion = 0;
    if (!validateHeader(ini, TemplateConfigType, TemplateConfigVersion, true, &fileVersion, errorMessage)) {
        return false;
    }

    const QStringList requiredKeys = {
        SettingsKeys::Template::TargetText,
        SettingsKeys::Template::ImageThreshold,
        SettingsKeys::Template::TrackingBoxX,
        SettingsKeys::Template::TrackingBoxY,
        SettingsKeys::Template::TrackingBoxWidth,
        SettingsKeys::Template::TrackingBoxHeight,
        SettingsKeys::Template::HasValidBoxes,
        SettingsKeys::CharacterBoxes::Count,
        SettingsKeys::CharacterBoxes::SourceWidth,
        SettingsKeys::CharacterBoxes::SourceHeight
    };
    if (fileVersion == TemplateConfigVersion) {
        for (const QString &key : requiredKeys) {
            if (!ini.contains(key)) {
                setError(errorMessage, unicodeText(L"\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u7f3a\u5c11\u5b57\u6bb5\uff1a%1").arg(key));
                return false;
            }
        }
    }

    settings->configVersion = TemplateConfigVersion;
    settings->targetText = ini.value(SettingsKeys::Template::TargetText, settings->targetText).toString();
    settings->imageThreshold = ini.value(SettingsKeys::Template::ImageThreshold, settings->imageThreshold).toDouble();
    settings->trackingBox = cv::Rect2d(
                ini.value(SettingsKeys::Template::TrackingBoxX, settings->trackingBox.x).toDouble(),
                ini.value(SettingsKeys::Template::TrackingBoxY, settings->trackingBox.y).toDouble(),
                ini.value(SettingsKeys::Template::TrackingBoxWidth, settings->trackingBox.width).toDouble(),
                ini.value(SettingsKeys::Template::TrackingBoxHeight, settings->trackingBox.height).toDouble());
    settings->hasValidBoxes = ini.value(SettingsKeys::Template::HasValidBoxes, settings->hasValidBoxes).toBool();
    settings->characterSourceImageSize = QSize(
                ini.value(SettingsKeys::CharacterBoxes::SourceWidth, settings->characterSourceImageSize.width()).toInt(),
                ini.value(SettingsKeys::CharacterBoxes::SourceHeight, settings->characterSourceImageSize.height()).toInt());

    const int count = ini.value(SettingsKeys::CharacterBoxes::Count, settings->characterBoxes.size()).toInt();
    if (count < 0) {
        setError(errorMessage, unicodeText(L"\u5b57\u7b26\u6846\u6570\u91cf\u65e0\u6548\u3002") );
        return false;
    }
    for (int i = 0; i < count; ++i) {
        CharacterTemplateBox box;
        box.name = ini.value(SettingsKeys::CharacterBoxes::nameKey(i)).toString();
        box.rect = QRect(
                    ini.value(SettingsKeys::CharacterBoxes::xKey(i)).toInt(),
                    ini.value(SettingsKeys::CharacterBoxes::yKey(i)).toInt(),
                    ini.value(SettingsKeys::CharacterBoxes::widthKey(i)).toInt(),
                    ini.value(SettingsKeys::CharacterBoxes::heightKey(i)).toInt());
        if (box.name.trimmed().isEmpty() || box.rect.width() <= 0 || box.rect.height() <= 0) {
            setError(errorMessage, unicodeText(L"\u7b2c %1 \u4e2a\u5b57\u7b26\u6846\u914d\u7f6e\u65e0\u6548\u3002").arg(i + 1));
            return false;
        }
        settings->characterBoxes.append(box);
    }
    if (ini.status() != QSettings::NoError) {
        setError(errorMessage, unicodeText(L"\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u8bfb\u53d6\u5931\u8d25\uff1a%1").arg(path));
        return false;
    }
    if (fileVersion < TemplateConfigVersion) {
        QString saveError;
        if (!saveTemplatePrivateSettings(templateDir, *settings, &saveError)) {
            setError(errorMessage, unicodeText(L"\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u81ea\u52a8\u5347\u7ea7\u5931\u8d25\uff1a%1").arg(saveError));
            return false;
        }
    }
    return true;
}

bool AppSettingsManager::saveTemplatePrivateSettings(const QString &templateDir,
                                                     const TemplatePrivateSettings &settings,
                                                     QString *errorMessage)
{
    QDir dir(templateDir);
    if (!dir.exists()) {
        setError(errorMessage, unicodeText(L"\u4ea7\u54c1\u6a21\u677f\u6587\u4ef6\u5939\u4e0d\u5b58\u5728\uff1a%1").arg(templateDir));
        return false;
    }

    const QString targetPath = dir.filePath("app_settings.appset");
    const QString tempPath = targetPath + ".tmp";
    const QString backupPath = targetPath + ".bak";
    if (!removeIfExists(tempPath, errorMessage)) {
        return false;
    }

    {
        QSettings ini(tempPath, QSettings::IniFormat);
        ini.clear();
        ini.setValue(SettingsKeys::Meta::ConfigType, TemplateConfigType);
        ini.setValue(SettingsKeys::Meta::ConfigVersion, TemplateConfigVersion);
        ini.setValue(SettingsKeys::Template::TargetText, settings.targetText);
        ini.setValue(SettingsKeys::Template::ImageThreshold, settings.imageThreshold);
        ini.setValue(SettingsKeys::Template::TrackingBoxX, settings.trackingBox.x);
        ini.setValue(SettingsKeys::Template::TrackingBoxY, settings.trackingBox.y);
        ini.setValue(SettingsKeys::Template::TrackingBoxWidth, settings.trackingBox.width);
        ini.setValue(SettingsKeys::Template::TrackingBoxHeight, settings.trackingBox.height);
        ini.setValue(SettingsKeys::Template::HasValidBoxes, settings.hasValidBoxes);
        ini.setValue(SettingsKeys::CharacterBoxes::SourceWidth, settings.characterSourceImageSize.width());
        ini.setValue(SettingsKeys::CharacterBoxes::SourceHeight, settings.characterSourceImageSize.height());
        ini.setValue(SettingsKeys::CharacterBoxes::Count, settings.characterBoxes.size());
        for (int i = 0; i < settings.characterBoxes.size(); ++i) {
            const CharacterTemplateBox &box = settings.characterBoxes.at(i);
            ini.setValue(SettingsKeys::CharacterBoxes::nameKey(i), box.name);
            ini.setValue(SettingsKeys::CharacterBoxes::xKey(i), box.rect.x());
            ini.setValue(SettingsKeys::CharacterBoxes::yKey(i), box.rect.y());
            ini.setValue(SettingsKeys::CharacterBoxes::widthKey(i), box.rect.width());
            ini.setValue(SettingsKeys::CharacterBoxes::heightKey(i), box.rect.height());
        }
        ini.sync();
        if (ini.status() != QSettings::NoError) {
            setError(errorMessage, unicodeText(L"\u6a21\u677f\u79c1\u6709\u914d\u7f6e\u5199\u5165\u5931\u8d25\uff1a%1").arg(tempPath));
            QFile::remove(tempPath);
            return false;
        }
    }

    const QStringList requiredKeys = {
        SettingsKeys::Template::TargetText,
        SettingsKeys::Template::ImageThreshold,
        SettingsKeys::Template::TrackingBoxX,
        SettingsKeys::Template::TrackingBoxY,
        SettingsKeys::Template::TrackingBoxWidth,
        SettingsKeys::Template::TrackingBoxHeight,
        SettingsKeys::Template::HasValidBoxes,
        SettingsKeys::CharacterBoxes::Count,
        SettingsKeys::CharacterBoxes::SourceWidth,
        SettingsKeys::CharacterBoxes::SourceHeight
    };
    if (!validateWrittenFile(tempPath,
                             TemplateConfigType,
                             TemplateConfigVersion,
                             requiredKeys,
                             errorMessage)) {
        QFile::remove(tempPath);
        return false;
    }
    return replaceWithTempFile(tempPath, targetPath, backupPath, true, errorMessage);
}

bool AppSettingsManager::clearGlobalSettings(QString *errorMessage)
{
    const QString path = globalSettingsFilePath();
    if (!QFileInfo::exists(path)) {
        return true;
    }
    if (!QFile::remove(path)) {
        setError(errorMessage, unicodeText(L"\u65e0\u6cd5\u5220\u9664\u8f6f\u4ef6\u516c\u5171\u914d\u7f6e\uff1a%1").arg(path));
        return false;
    }
    return true;
}
