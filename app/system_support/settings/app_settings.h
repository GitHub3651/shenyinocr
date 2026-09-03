// 文件作用：定义整机、界面和五种检测方案的唯一持久化设置。
#pragma once

#include "contracts/detection_mode.h"

#include <QString>
#include <QStringList>

struct DetectionSchemes
{
    QString stampTemplatePath;
    QStringList wordTemplatePaths;
    QString ocrTemplatePath;
    double tissueRoughnessThreshold = 6.0;
    QStringList barcodeWordTemplatePaths;

    QStringList templatePaths(DetectionMode mode) const;
    bool setTemplatePaths(DetectionMode mode,
                          const QStringList &paths,
                          QString *errorMessage = nullptr);
};

bool operator==(const DetectionSchemes &left,
                const DetectionSchemes &right);
bool operator!=(const DetectionSchemes &left,
                const DetectionSchemes &right);

struct AppSettings
{
    static const int CurrentSchemaVersion = 6;

    AppSettings();

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

    QString templateSaveDirectory;
    bool barcodeCsvEnabled;
    QString barcodeCsvOutputDirectory;
    DetectionSchemes detectionSchemes;

    static AppSettings defaults();
};

bool operator==(const AppSettings &left,
                const AppSettings &right);
bool operator!=(const AppSettings &left,
                const AppSettings &right);

QStringList appSettingsDetectionModeIds();
QStringList appSettingsImageSaveModeIds();
QStringList appSettingsImageSaveTypeIds();
QStringList appSettingsColorChannelIds();
QStringList appSettingsRotationIds();
QStringList appSettingsTriggerModeIds();
