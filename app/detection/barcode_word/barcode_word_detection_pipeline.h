#ifndef DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
#define DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H

#include <functional>

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

class BarcodeWordDetectionPipeline
{
public:
    typedef std::function<BarcodeWordDateDetectionResult()>
            DateDetectionFunction;

    BarcodeWordDetectionResult detect(
            bool barcodeIsReadable,
            const DateDetectionFunction &detectDate) const;
};

#endif // DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
