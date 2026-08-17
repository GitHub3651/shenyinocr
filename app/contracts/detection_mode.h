#ifndef CONTRACTS_DETECTION_MODE_H
#define CONTRACTS_DETECTION_MODE_H

#include <QString>

enum class DetectionMode
{
    Stamp,
    Word,
    Ocr,
    Tissue,
    BarcodeWord
};

extern const QString BarcodeWordDetectionMode;

QString detectionModeId(DetectionMode mode);
bool detectionModeFromId(const QString &modeId, DetectionMode *mode);
QString detectionModeUiId(DetectionMode mode);
bool detectionModeFromUiId(const QString &modeId, DetectionMode *mode);
bool isWordFamilyMode(const QString &modeId);

#endif // CONTRACTS_DETECTION_MODE_H
