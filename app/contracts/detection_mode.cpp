// 文件作用：本文件用于定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 主要职责：定义五种稳定检测模式及其界面、JSON和字符串转换规则。
// 模块位置：合同层；负责稳定枚举、默认值和跨模块轻量数据，不承载运行副作用。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "contracts/detection_mode.h"

const QString BarcodeWordDetectionMode =
    QStringLiteral("barcode_word_detection");

// 函数说明：detectionModeId 函数执行对应事件或业务处理。
QString detectionModeId(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp: return QStringLiteral("stamp");
    case DetectionMode::Word: return QStringLiteral("word");
    case DetectionMode::Ocr: return QStringLiteral("ocr");
    case DetectionMode::Tissue: return QStringLiteral("tissue");
    case DetectionMode::BarcodeWord: return QStringLiteral("barcodeWord");
    }
    return QString();
}

// 函数说明：detectionModeFromId 函数执行对应事件或业务处理。
bool detectionModeFromId(const QString &id, DetectionMode *mode)
{
    if (!mode) return false;
    if (id == QLatin1String("stamp")) *mode = DetectionMode::Stamp;
    else if (id == QLatin1String("word")) *mode = DetectionMode::Word;
    else if (id == QLatin1String("ocr")) *mode = DetectionMode::Ocr;
    else if (id == QLatin1String("tissue")) *mode = DetectionMode::Tissue;
    else if (id == QLatin1String("barcodeWord")) {
        *mode = DetectionMode::BarcodeWord;
    } else {
        return false;
    }
    return true;
}

// 函数说明：detectionModeUiId 函数执行对应事件或业务处理。
QString detectionModeUiId(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp: return QStringLiteral("stamp_detection");
    case DetectionMode::Word: return QStringLiteral("word_detection");
    case DetectionMode::Ocr: return QStringLiteral("ocr_detection");
    case DetectionMode::Tissue: return QStringLiteral("tissue_detection");
    case DetectionMode::BarcodeWord: return BarcodeWordDetectionMode;
    }
    return QString();
}

// 函数说明：detectionModeFromUiId 函数执行对应事件或业务处理。
bool detectionModeFromUiId(const QString &id, DetectionMode *mode)
{
    if (!mode) return false;
    if (id == QLatin1String("stamp_detection")) *mode = DetectionMode::Stamp;
    else if (id == QLatin1String("word_detection")) *mode = DetectionMode::Word;
    else if (id == QLatin1String("ocr_detection")) *mode = DetectionMode::Ocr;
    else if (id == QLatin1String("tissue_detection")) {
        *mode = DetectionMode::Tissue;
    } else if (id == BarcodeWordDetectionMode) {
        *mode = DetectionMode::BarcodeWord;
    } else {
        return false;
    }
    return true;
}

// 函数说明：isWordFamilyMode 函数检查相关状态并返回判断结果。
bool isWordFamilyMode(const QString &modeId)
{
    return modeId == QLatin1String("word_detection")
        || modeId == BarcodeWordDetectionMode;
}
