// 文件作用：本文件用于定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 主要职责：定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 模块位置：合同层；负责稳定枚举、默认值和跨模块轻量数据，不承载运行副作用。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "contracts/detection_mode.h"

namespace {

const QVector<DetectionModeDescriptor> &descriptors()
{
    static const QVector<DetectionModeDescriptor> values = {
        { DetectionMode::Stamp, "stamp", "stamp_detection", "模板匹配",
          "stamp", "无法启动钢印检测工作线程。",
          DetectionTrackingKind::SingleTemplate, true, true, false,
          true, true, false, false, 0 },
        { DetectionMode::Word, "word", "word_detection", "字库匹配",
          "word", "无法启动字库检测工作线程。",
          DetectionTrackingKind::MultipleProfiles, true, true, false,
          true, false, false, false, 0 },
        { DetectionMode::Ocr, "ocr", "ocr_detection", "深度模型",
          "OCR", "无法启动深度OCR检测工作线程。",
          DetectionTrackingKind::SingleTemplate, true, false, false,
          false, false, true, true, 0 },
        { DetectionMode::Tissue, "tissue", "tissue_detection", "纸巾检测",
          "tissue", "无法启动纸巾检测工作线程。",
          DetectionTrackingKind::WholeFrame, false, false, false,
          false, false, false, true, 0 },
        { DetectionMode::BarcodeWord, "barcodeWord", "barcode_word_detection",
          "二维码+三期", "barcode-word", "无法启动二维码+三期检测工作线程。",
          DetectionTrackingKind::MultipleProfiles,
          true, true, true, true, false, false, false, 2 }
    };
    return values;
}

} // namespace

const QVector<DetectionModeDescriptor> &detectionModeDescriptors()
{
    return descriptors();
}

const DetectionModeDescriptor &detectionModeDescriptor(DetectionMode mode)
{
    for (const DetectionModeDescriptor &descriptor : descriptors()) {
        if (descriptor.mode == mode) {
            return descriptor;
        }
    }
    return descriptors().first();
}

const DetectionModeDescriptor *detectionModeDescriptorFromId(
    const QString &modeId)
{
    for (const DetectionModeDescriptor &descriptor : descriptors()) {
        if (modeId == QLatin1String(descriptor.recipeId)) {
            return &descriptor;
        }
    }
    return nullptr;
}

const DetectionModeDescriptor *detectionModeDescriptorFromUiId(
    const QString &modeId)
{
    for (const DetectionModeDescriptor &descriptor : descriptors()) {
        if (modeId == QLatin1String(descriptor.uiId)) {
            return &descriptor;
        }
    }
    return nullptr;
}

// 函数说明：detectionModeId 函数执行对应事件或业务处理。
QString detectionModeId(DetectionMode mode)
{
    return QLatin1String(detectionModeDescriptor(mode).recipeId);
}

// 函数说明：detectionModeFromId 函数执行对应事件或业务处理。
bool detectionModeFromId(const QString &id, DetectionMode *mode)
{
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromId(id);
    if (!mode || !descriptor) {
        return false;
    }
    *mode = descriptor->mode;
    return true;
}

// 函数说明：detectionModeUiId 函数执行对应事件或业务处理。
QString detectionModeUiId(DetectionMode mode)
{
    return QLatin1String(detectionModeDescriptor(mode).uiId);
}

// 函数说明：detectionModeFromUiId 函数执行对应事件或业务处理。
bool detectionModeFromUiId(const QString &id, DetectionMode *mode)
{
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromUiId(id);
    if (!mode || !descriptor) {
        return false;
    }
    *mode = descriptor->mode;
    return true;
}

// 函数说明：isWordFamilyMode 函数检查相关状态并返回判断结果。
bool isWordFamilyMode(const QString &modeId)
{
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromUiId(modeId);
    return descriptor
            && descriptor->trackingKind
               == DetectionTrackingKind::MultipleProfiles;
}
