// 文件作用：本文件用于定义整机相机、触发、PLC、存图和界面布局等持久化设置。
// 主要职责：定义整机相机、触发、PLC、存图和界面布局等持久化设置。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "system_support/settings/machine_settings.h"

#include "contracts/detection_mode.h"

const int MachineSettings::CurrentSchemaVersion;

// 函数说明：MachineSettings 构造函数创建组件并初始化其依赖和初始状态。
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

// 函数说明：machineSettingsDetectionModeIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsDetectionModeIds()
{
    return QStringList()
            << QStringLiteral("stamp_detection")
            << QStringLiteral("word_detection")
            << QStringLiteral("ocr_detection")
            << QStringLiteral("tissue_detection")
            << BarcodeWordDetectionMode;
}

// 函数说明：machineSettingsImageSaveModeIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsImageSaveModeIds()
{
    return QStringList()
            << QStringLiteral("save_none")
            << QStringLiteral("save_ng")
            << QStringLiteral("save_ok")
            << QStringLiteral("save_all");
}

// 函数说明：machineSettingsImageSaveTypeIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsImageSaveTypeIds()
{
    return QStringList()
            << QStringLiteral("save_both")
            << QStringLiteral("save_annotated_only")
            << QStringLiteral("save_raw_only");
}

// 函数说明：machineSettingsColorChannelIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsColorChannelIds()
{
    return QStringList()
            << QStringLiteral("color")
            << QStringLiteral("red")
            << QStringLiteral("green")
            << QStringLiteral("blue");
}

// 函数说明：machineSettingsRotationIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsRotationIds()
{
    return QStringList()
            << QStringLiteral("rotate_none")
            << QStringLiteral("rotate_clockwise_90")
            << QStringLiteral("rotate_counterclockwise_90")
            << QStringLiteral("rotate_180");
}

// 函数说明：machineSettingsTriggerModeIds 函数实现名称所表示的处理步骤。
QStringList machineSettingsTriggerModeIds()
{
    return QStringList()
            << QStringLiteral("trigger_continuous")
            << QStringLiteral("trigger_interval");
}

// 函数说明：defaults 函数实现名称所表示的处理步骤。
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
