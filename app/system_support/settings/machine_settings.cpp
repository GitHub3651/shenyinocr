#include "system_support/settings/machine_settings.h"

#include "DetectionModes.h"

const int MachineSettings::CurrentSchemaVersion;

MachineSettings::MachineSettings()
    : schemaVersion(CurrentSchemaVersion),
      detectModeId(QStringLiteral("word_detection")),
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
      plcRejectTimeOffset(980)
{
}

QStringList machineSettingsDetectionModeIds()
{
    return QStringList()
            << QStringLiteral("stamp_detection")
            << QStringLiteral("word_detection")
            << QStringLiteral("ocr_detection")
            << QStringLiteral("tissue_detection")
            << BarcodeWordDetectionMode;
}

QStringList machineSettingsImageSaveModeIds()
{
    return QStringList()
            << QStringLiteral("save_none")
            << QStringLiteral("save_ng")
            << QStringLiteral("save_ok")
            << QStringLiteral("save_all");
}

QStringList machineSettingsImageSaveTypeIds()
{
    return QStringList()
            << QStringLiteral("save_both")
            << QStringLiteral("save_annotated_only")
            << QStringLiteral("save_raw_only");
}

QStringList machineSettingsColorChannelIds()
{
    return QStringList()
            << QStringLiteral("color")
            << QStringLiteral("red")
            << QStringLiteral("green")
            << QStringLiteral("blue");
}

QStringList machineSettingsRotationIds()
{
    return QStringList()
            << QStringLiteral("rotate_none")
            << QStringLiteral("rotate_clockwise_90")
            << QStringLiteral("rotate_counterclockwise_90")
            << QStringLiteral("rotate_180");
}

QStringList machineSettingsTriggerModeIds()
{
    return QStringList()
            << QStringLiteral("trigger_continuous")
            << QStringLiteral("trigger_interval");
}

MachineSettings MachineSettings::defaults()
{
    return MachineSettings();
}

bool operator==(const MachineSettings &left,
                const MachineSettings &right)
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
            && left.publishedRecipeIdsByMode
               == right.publishedRecipeIdsByMode
            && left.rightPanelSplitterState
               == right.rightPanelSplitterState;
}

bool operator!=(const MachineSettings &left,
                const MachineSettings &right)
{
    return !(left == right);
}
