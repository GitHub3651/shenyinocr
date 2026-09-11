#include "contracts/detection_mode.h"

namespace {

const QVector<DetectionModeDescriptor> &descriptors()
{
    static const QVector<DetectionModeDescriptor> values = {
        { DetectionMode::Stamp, "stamp", "stamp_detection", "钢印检测",
          "无法开始钢印检测，请重试。",
          DetectionTrackingKind::SingleTemplate, true, true, false,
          true, false },
        { DetectionMode::Word, "word", "word_detection", "字库匹配",
          "无法开始字库匹配检测，请重试。",
          DetectionTrackingKind::MultipleTemplates, true, true, false,
          true, false },
        { DetectionMode::Ocr, "ocr", "ocr_detection", "深度 OCR",
          "无法开始深度 OCR 检测，请重试。",
          DetectionTrackingKind::SingleTemplate, true, false, false,
          false, true },
        { DetectionMode::Tissue, "tissue", "tissue_detection", "纸巾检测",
          "无法开始纸巾检测，请重试。",
          DetectionTrackingKind::WholeFrame, false, false, false,
          false, true },
        { DetectionMode::BarcodeWord, "barcodeWord", "barcode_word_detection",
          "二维码+三期", "无法开始二维码+三期检测，请重试。",
          DetectionTrackingKind::MultipleTemplates,
          true, true, true, true, false }
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
        if (modeId == QLatin1String(descriptor.modeId)) {
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

QString detectionModeId(DetectionMode mode)
{
    return QLatin1String(detectionModeDescriptor(mode).modeId);
}

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

QString detectionModeUiId(DetectionMode mode)
{
    return QLatin1String(detectionModeDescriptor(mode).uiId);
}

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

bool isWordFamilyMode(const QString &modeId)
{
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromUiId(modeId);
    return descriptor
            && descriptor->trackingKind
               == DetectionTrackingKind::MultipleTemplates;
}
