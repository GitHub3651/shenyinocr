#include <QtTest/QtTest>

#include "detection/stamp/stamp_detection_pipeline.h"

namespace {

StampOverlapResult clearStampResult()
{
    StampOverlapResult result;
    result.isOk = true;
    result.finalStampPoly.push_back(cv::Point(1, 2));
    result.finalStampPoly.push_back(cv::Point(3, 4));
    result.finalStampPoly.push_back(cv::Point(5, 6));
    return result;
}

StampDetectionResult detectWithCounts(
        const QString &targetText,
        int detectedCount,
        const StampDetectionPipeline::OverlapDetectionFunction &detectOverlap)
{
    cv::Mat dateRoi = cv::Mat::zeros(2, 2, CV_8UC3);
    const cv::Mat sourceImage = cv::Mat::zeros(4, 4, CV_8UC3);
    const std::vector<cv::Point> datePoly(3, cv::Point(0, 0));
    const StampDetectionPipeline pipeline;
    return pipeline.detect(
                dateRoi,
                sourceImage,
                datePoly,
                targetText,
                [detectedCount](cv::Mat &) {
        return detectedCount;
    },
                detectOverlap);
}

} // namespace

class StampDetectionPipelineTest : public QObject
{
    Q_OBJECT

private slots:
    void targetCountPreservesVariantAndFallbackRules();
    void matchingCharactersAndClearStampAreOk();
    void characterMismatchIsNgEvenWhenStampIsClear();
    void overlapFailureOrMissingConfigurationIsNg();
};

void StampDetectionPipelineTest::targetCountPreservesVariantAndFallbackRules()
{
    const StampDetectionPipeline::OverlapDetectionFunction clearStamp =
            [](const cv::Mat &, const std::vector<cv::Point> &) {
        return clearStampResult();
    };

    const StampDetectionResult variantResult =
            detectWithCounts("A(2)B3", 3, clearStamp);
    QCOMPARE(variantResult.targetCharacterCount, 3);
    QVERIFY(variantResult.isOk);

    const StampDetectionResult fallbackResult =
            detectWithCounts("---", 3, clearStamp);
    QCOMPARE(fallbackResult.targetCharacterCount, 3);
    QVERIFY(fallbackResult.isOk);
}

void StampDetectionPipelineTest::matchingCharactersAndClearStampAreOk()
{
    const StampDetectionResult result =
            detectWithCounts(
                "AB12",
                4,
                [](const cv::Mat &, const std::vector<cv::Point> &) {
        return clearStampResult();
    });

    QVERIFY(result.characterIsOk);
    QVERIFY(result.overlapIsOk);
    QVERIFY(result.isOk);
    QCOMPARE(static_cast<int>(result.finalStampPoly.size()), 3);
}

void StampDetectionPipelineTest::characterMismatchIsNgEvenWhenStampIsClear()
{
    const StampDetectionResult result =
            detectWithCounts(
                "AB12",
                3,
                [](const cv::Mat &, const std::vector<cv::Point> &) {
        return clearStampResult();
    });

    QVERIFY(!result.characterIsOk);
    QVERIFY(result.overlapIsOk);
    QVERIFY(!result.isOk);
}

void StampDetectionPipelineTest::overlapFailureOrMissingConfigurationIsNg()
{
    StampDetectionResult overlapResult =
            detectWithCounts(
                "AB12",
                4,
                [](const cv::Mat &, const std::vector<cv::Point> &) {
        StampOverlapResult result;
        result.isOk = false;
        return result;
    });
    QVERIFY(overlapResult.characterIsOk);
    QVERIFY(!overlapResult.overlapIsOk);
    QVERIFY(!overlapResult.isOk);

    overlapResult = detectWithCounts(
                "AB12",
                4,
                StampDetectionPipeline::OverlapDetectionFunction());
    QVERIFY(overlapResult.characterIsOk);
    QVERIFY(!overlapResult.overlapIsOk);
    QVERIFY(!overlapResult.isOk);
}

QTEST_APPLESS_MAIN(StampDetectionPipelineTest)

#include "stamp_detection_pipeline_test.moc"
