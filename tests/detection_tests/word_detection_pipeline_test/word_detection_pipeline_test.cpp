#include <QtTest/QtTest>

#include "detection/word/word_detection_pipeline.h"

namespace {

WordDetectionResult detectCount(
        const QString &targetText,
        int detectedCount)
{
    cv::Mat dateRoi = cv::Mat::zeros(2, 2, CV_8UC3);
    const WordDetectionPipeline pipeline;
    return pipeline.detect(
                dateRoi,
                targetText,
                [detectedCount](cv::Mat &) {
        return detectedCount;
    });
}

} // namespace

class WordDetectionPipelineTest : public QObject
{
    Q_OBJECT

private slots:
    void targetParserPreservesVariantsAndFallbackCount();
    void exactDetectedCountIsOk();
    void fewerDetectedCharactersAreNg();
    void moreDetectedCharactersAreNg();
};

void WordDetectionPipelineTest::targetParserPreservesVariantsAndFallbackCount()
{
    WordDetectionResult result = detectCount("A(2)B3", 3);
    QCOMPARE(result.targetCharacterCount, 3);
    QCOMPARE(result.targetUnits.join("|"), QString("a(2)|b|3"));
    QVERIFY(result.isOk);

    result = detectCount("---", 3);
    QCOMPARE(result.targetCharacterCount, 3);
    QVERIFY(result.targetUnits.isEmpty());
    QVERIFY(result.isOk);
}

void WordDetectionPipelineTest::exactDetectedCountIsOk()
{
    const WordDetectionResult result = detectCount("AB12", 4);
    QCOMPARE(result.detectedCharacterCount, 4);
    QVERIFY(result.isOk);
}

void WordDetectionPipelineTest::fewerDetectedCharactersAreNg()
{
    const WordDetectionResult result = detectCount("AB12", 3);
    QVERIFY(result.detectedCharacterCount < result.targetCharacterCount);
    QVERIFY(!result.isOk);
}

void WordDetectionPipelineTest::moreDetectedCharactersAreNg()
{
    const WordDetectionResult result = detectCount("AB12", 5);
    QVERIFY(result.detectedCharacterCount > result.targetCharacterCount);
    QVERIFY(!result.isOk);
}

QTEST_APPLESS_MAIN(WordDetectionPipelineTest)

#include "word_detection_pipeline_test.moc"
