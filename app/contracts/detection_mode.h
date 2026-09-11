#ifndef CONTRACTS_DETECTION_MODE_H
#define CONTRACTS_DETECTION_MODE_H

#include <QString>
#include <QVector>

enum class DetectionMode
{
    Stamp,
    Word,
    Ocr,
    Tissue,
    BarcodeWord
};

enum class DetectionTrackingKind
{
    WholeFrame,
    SingleTemplate,
    MultipleTemplates
};

// 检测模式的唯一元数据。各层只查询这个描述，不再维护第二套模式分类。
struct DetectionModeDescriptor
{
    DetectionMode mode;
    const char *modeId;
    const char *uiId;
    const char *displayName;
    const char *startFailureMessage;
    DetectionTrackingKind trackingKind;
    bool requiresTargetText;
    bool requiresCharacterTemplates;
    bool requiresBarcodeDecoder;
    bool clearImageLabelRects;
    bool saveRawOnly;
};

const QVector<DetectionModeDescriptor> &detectionModeDescriptors();
const DetectionModeDescriptor &detectionModeDescriptor(DetectionMode mode);
const DetectionModeDescriptor *detectionModeDescriptorFromId(
    const QString &modeId);
const DetectionModeDescriptor *detectionModeDescriptorFromUiId(
    const QString &modeId);

QString detectionModeId(DetectionMode mode);
bool detectionModeFromId(const QString &modeId, DetectionMode *mode);
QString detectionModeUiId(DetectionMode mode);
bool detectionModeFromUiId(const QString &modeId, DetectionMode *mode);
bool isWordFamilyMode(const QString &modeId);

#endif // CONTRACTS_DETECTION_MODE_H
