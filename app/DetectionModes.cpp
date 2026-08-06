#include "DetectionModes.h"

const QString BarcodeWordDetectionMode =
    QStringLiteral("barcode_word_detection");

bool isWordFamilyMode(const QString &modeId)
{
    return modeId == QLatin1String("word_detection")
        || modeId == BarcodeWordDetectionMode;
}
