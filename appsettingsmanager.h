#ifndef APPSETTINGSMANAGER_H
#define APPSETTINGSMANAGER_H

#include <QRect>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct GlobalSettings
{
    int configVersion = 1;

    QString detectModeId;
    QString imageSaveModeId;
    QString imageSaveTypeId;
    QString imageSavePath;
    QString templateBaseDirPath;

    int cameraExposure = 800;
    double cameraGain = 1.0;
    QString colorChannelId;
    QString imageRotationId;

    bool triggerEnabled = true;
    QString triggerModeId;
    QString plcModeId;
    QString plcIp;
    int plcRack = 0;
    int plcSlot = 1;

    int photoDistance = 50;
    int photoTime = 300;
    int cameraDelay = 300;
    int rejectDistance = 500;
    int rejectTime = 300;
    int rejectPosition = 0;

    double tissueRoughnessThreshold = 6.0;
    QMap<QString, QStringList> templateDirPathsByMode;
};

struct CharacterTemplateBox
{
    QString name;
    QRect rect;
};

struct TemplatePrivateSettings
{
    int configVersion = 1;
    QString targetText;
    double imageThreshold = 70.0;
    cv::Rect2d trackingBox;
    bool hasValidBoxes = false;
    QVector<CharacterTemplateBox> characterBoxes;
    QSize characterSourceImageSize;
};

namespace SettingsKeys {
namespace Meta {
extern const char ConfigVersion[];
extern const char ConfigType[];
}

namespace Global {
extern const char DetectMode[];
extern const char ImageSaveMode[];
extern const char ImageSaveType[];
extern const char ImageSavePath[];
extern const char TemplateBaseDir[];
extern const char CameraExposure[];
extern const char CameraGain[];
extern const char ColorChannel[];
extern const char ImageRotation[];
extern const char TriggerEnabled[];
extern const char TriggerMode[];
extern const char PlcMode[];
extern const char PlcIp[];
extern const char PlcRack[];
extern const char PlcSlot[];
extern const char PhotoDistance[];
extern const char PhotoTime[];
extern const char CameraDelay[];
extern const char RejectDistance[];
extern const char RejectTime[];
extern const char RejectPosition[];
extern const char TissueRoughnessThreshold[];
QString templatePathsKey(const QString &modeId);
}

namespace Template {
extern const char TargetText[];
extern const char ImageThreshold[];
extern const char TrackingBoxX[];
extern const char TrackingBoxY[];
extern const char TrackingBoxWidth[];
extern const char TrackingBoxHeight[];
extern const char HasValidBoxes[];
}

namespace CharacterBoxes {
extern const char Count[];
extern const char SourceWidth[];
extern const char SourceHeight[];
QString nameKey(int index);
QString xKey(int index);
QString yKey(int index);
QString widthKey(int index);
QString heightKey(int index);
}
}

class AppSettingsManager
{
public:
    static const int GlobalConfigVersion = 2;
    static const int TemplateConfigVersion = 1;

    static QString globalDataDirPath();
    static QString globalSettingsFilePath();

    static GlobalSettings defaultGlobalSettings();
    static bool loadGlobalSettings(GlobalSettings *settings, QString *errorMessage);
    static bool saveGlobalSettings(const GlobalSettings &settings, QString *errorMessage);

    static TemplatePrivateSettings defaultTemplatePrivateSettings();
    static bool loadTemplatePrivateSettings(const QString &templateDir,
                                            TemplatePrivateSettings *settings,
                                            QString *errorMessage);
    static bool saveTemplatePrivateSettings(const QString &templateDir,
                                            const TemplatePrivateSettings &settings,
                                            QString *errorMessage);

    static bool clearGlobalSettings(QString *errorMessage);
};

#endif // APPSETTINGSMANAGER_H
