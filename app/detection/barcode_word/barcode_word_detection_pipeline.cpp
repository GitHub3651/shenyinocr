#include "barcode_word_detection_pipeline.h"

BarcodeWordDetectionResult BarcodeWordDetectionPipeline::detect(
        bool barcodeIsReadable,
        const DateDetectionFunction &detectDate) const
{
    BarcodeWordDetectionResult result;
    result.barcodeIsReadable = barcodeIsReadable;
    if (!result.barcodeIsReadable || !detectDate) {
        return result;
    }

    result.dateDetectionExecuted = true;
    const BarcodeWordDateDetectionResult dateResult = detectDate();
    result.dateResultProduced = dateResult.resultProduced;
    result.dateIsOk = dateResult.isOk;
    result.isOk = result.dateResultProduced && result.dateIsOk;
    return result;
}
