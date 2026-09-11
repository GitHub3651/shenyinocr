#ifndef DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
#define DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H

#include "engines/barcode/barcode_types.h"
#include "detection/detectionmode/word/word_detection_pipeline.h"

#include <QString>
#include <QStringList>

#include <vector>

class IBarcodeDecoder;

struct BarcodeWordDetectionResult
{
    bool barcodeIsReadable = false;
    bool dateDetectionExecuted = false;
    bool dateIsOk = false;
    bool isOk = false;
};

struct BarcodeWordDecodeStrategyState
{
    int preferredStrategyId = -1;
    unsigned int preferredOptionFlags =
            BarcodeDecodeOptionFlags::None;
    int consecutiveFailures = 0;
};

struct BarcodeWordDetectionWorkOutput
{
    DetectionResult detectionResult;
    BarcodeWordDetectionResult barcodeWordResult;
    WordDetectionWorkOutput wordOutput;
    DetectionPose pose;
    QString templateName;
    BarcodeReadResult barcode;
    BarcodeWordDecodeStrategyState nextDecodeStrategy;
    QString barcodeState;
    QString dateState;
    QString operatorReason;
    bool barcodeRoiValid = false;
    bool dateRoiValid = false;
};

class BarcodeWordDetectionPipeline
{
public:
    BarcodeWordDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QStringList &targetUnits,
            const QString &templateName,
            const PreparedCharacterTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent,
            const BarcodeDecodeOptions &decodeOptions,
            const BarcodeWordDecodeStrategyState &decodeStrategy,
            IBarcodeDecoder *decoder) const;
};

#endif // DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
