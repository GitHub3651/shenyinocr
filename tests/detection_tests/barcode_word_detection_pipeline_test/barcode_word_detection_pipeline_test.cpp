#include <QtTest/QtTest>

#include "detection/barcode_word/barcode_word_detection_pipeline.h"

class BarcodeWordDetectionPipelineTest : public QObject
{
    Q_OBJECT

private slots:
    void unreadableBarcodeShortCircuitsDateDetection();
    void readableBarcodeWithoutDateStageDoesNotExecute();
    void readableBarcodeAndDateOkAreOk();
    void readableBarcodeAndDateNgAreNg();
};

void BarcodeWordDetectionPipelineTest::unreadableBarcodeShortCircuitsDateDetection()
{
    int dateCallCount = 0;
    const BarcodeWordDetectionPipeline pipeline;
    const BarcodeWordDetectionResult result = pipeline.detect(
                false,
                [&dateCallCount]() {
        ++dateCallCount;
        BarcodeWordDateDetectionResult dateResult;
        dateResult.resultProduced = true;
        dateResult.isOk = true;
        return dateResult;
    });

    QCOMPARE(dateCallCount, 0);
    QVERIFY(!result.barcodeIsReadable);
    QVERIFY(!result.dateDetectionExecuted);
    QVERIFY(!result.isOk);
}

void BarcodeWordDetectionPipelineTest::readableBarcodeWithoutDateStageDoesNotExecute()
{
    const BarcodeWordDetectionPipeline pipeline;
    const BarcodeWordDetectionResult result = pipeline.detect(
                true,
                BarcodeWordDetectionPipeline::DateDetectionFunction());

    QVERIFY(result.barcodeIsReadable);
    QVERIFY(!result.dateDetectionExecuted);
    QVERIFY(!result.dateResultProduced);
    QVERIFY(!result.isOk);
}

void BarcodeWordDetectionPipelineTest::readableBarcodeAndDateOkAreOk()
{
    const BarcodeWordDetectionPipeline pipeline;
    const BarcodeWordDetectionResult result = pipeline.detect(
                true,
                []() {
        BarcodeWordDateDetectionResult dateResult;
        dateResult.resultProduced = true;
        dateResult.isOk = true;
        return dateResult;
    });

    QVERIFY(result.dateDetectionExecuted);
    QVERIFY(result.dateResultProduced);
    QVERIFY(result.dateIsOk);
    QVERIFY(result.isOk);
}

void BarcodeWordDetectionPipelineTest::readableBarcodeAndDateNgAreNg()
{
    const BarcodeWordDetectionPipeline pipeline;
    const BarcodeWordDetectionResult result = pipeline.detect(
                true,
                []() {
        BarcodeWordDateDetectionResult dateResult;
        dateResult.resultProduced = true;
        dateResult.isOk = false;
        return dateResult;
    });

    QVERIFY(result.dateDetectionExecuted);
    QVERIFY(result.dateResultProduced);
    QVERIFY(!result.dateIsOk);
    QVERIFY(!result.isOk);
}

QTEST_APPLESS_MAIN(BarcodeWordDetectionPipelineTest)

#include "barcode_word_detection_pipeline_test.moc"
