#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

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
