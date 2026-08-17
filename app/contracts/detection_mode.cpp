#include "contracts/detection_mode.h"

const QString BarcodeWordDetectionMode =
    QStringLiteral("barcode_word_detection");

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

bool isWordFamilyMode(const QString &modeId)
{
    return modeId == QLatin1String("word_detection")
        || modeId == BarcodeWordDetectionMode;
}
