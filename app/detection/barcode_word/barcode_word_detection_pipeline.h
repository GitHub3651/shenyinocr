#ifndef DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
#define DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H

#include "BarcodeTypes.h"
#include "detection/word/word_detection_pipeline.h"

#include <QString>

#include <functional>
#include <vector>

class IBarcodeDecoder;

struct BarcodeWordDateDetectionResult
{
    bool resultProduced = false;
    bool isOk = false;
};

struct BarcodeWordDetectionResult
{
    bool barcodeIsReadable = false;
    bool dateDetectionExecuted = false;
    bool dateResultProduced = false;
    bool dateIsOk = false;
    bool isOk = false;
};

struct BarcodeWordDecodeStrategyState
{
    int preferredStrategyId = -1;
    unsigned int preferredOptionFlags =
            BARCODE_DECODER_OPTION_NONE;
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
    QString reason;
    bool barcodeRoiValid = false;
    bool dateRoiValid = false;
};

class BarcodeWordDetectionPipeline
{
public:
    typedef std::function<BarcodeWordDateDetectionResult()>
            DateDetectionFunction;

    BarcodeWordDetectionResult detect(
            bool barcodeIsReadable,
            const DateDetectionFunction &detectDate) const;

    BarcodeWordDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QString &targetText,
            const QString &templateName,
            const TemplateMatchPreparedTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent,
            const BarcodeDecodeOptions &decodeOptions,
            const BarcodeWordDecodeStrategyState &decodeStrategy,
            IBarcodeDecoder *decoder) const;
};

#endif // DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
