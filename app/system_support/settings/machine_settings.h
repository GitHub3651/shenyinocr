// 文件作用：本文件用于定义整机相机、触发、PLC、存图和界面布局等持久化设置。
// 主要职责：定义整机相机、触发、PLC、存图和界面布局等持久化设置。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

// 组件说明：MachineSettings 组件集中描述相关配置、规则和运行参数。
struct MachineSettings
{
    static const int CurrentSchemaVersion = 1;

    MachineSettings();

    int schemaVersion;

    QString detectModeId;
    QString imageSaveModeId;
    QString imageSaveTypeId;
    QString imageSavePath;
    int imageJpegQuality;

    int cameraExposure;
    int cameraGain;
    QString colorChannelId;
    QString imageRotationId;
    bool triggerEnabled;
    int cameraDelay;

    QString triggerModeId;
    QString plcIp;
    int plcRack;
    int plcSlot;
    int photoDistance;
    int photoTime;
    int rejectDistance;
    int rejectTime;
    int rejectPosition;

    int plcTriggerModeDb;
    int plcTriggerModeOffset;
    int plcResultDb;
    int plcResultOffset;
    int plcPhotoDistanceOffset;
    int plcPhotoTimeOffset;
    int plcRejectDistanceOffset;
    int plcRejectTimeOffset;

    QMap<QString, QString> publishedRecipeIdsByMode;
    QByteArray rightPanelSplitterState;

    static MachineSettings defaults();
};

bool operator==(const MachineSettings &left,
                const MachineSettings &right);
bool operator!=(const MachineSettings &left,
                const MachineSettings &right);

QStringList machineSettingsDetectionModeIds();
QStringList machineSettingsImageSaveModeIds();
QStringList machineSettingsImageSaveTypeIds();
QStringList machineSettingsColorChannelIds();
QStringList machineSettingsRotationIds();
QStringList machineSettingsTriggerModeIds();
