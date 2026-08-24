// 文件作用：本文件用于定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 主要职责：定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 模块位置：合同层；负责稳定枚举、默认值和跨模块轻量数据，不承载运行副作用。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef CONTRACTS_DETECTION_MODE_H
#define CONTRACTS_DETECTION_MODE_H

#include <QString>
#include <QVector>

// 组件说明：DetectionMode 枚举列出该组件允许使用的稳定状态和选项。
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
    const char *workerLogName;
    const char *startFailureMessage;
    DetectionTrackingKind trackingKind;
    bool requiresTargetText;
    bool requiresCharacterTemplates;
    bool requiresBarcodeDecoder;
    bool clearImageLabelRects;
    bool showRoiWarningOnCancelled;
    bool saveRawOnly;
    bool saveNotEvaluatedAsNg;
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
