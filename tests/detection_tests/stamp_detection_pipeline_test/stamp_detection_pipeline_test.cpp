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

DetectionWorkItem makePositionedItem(cv::Mat *characterTemplate)
{
    cv::Mat source = cv::Mat::zeros(80, 100, CV_8UC3);
    cv::Mat pattern(16, 12, CV_8UC3);
    for (int y = 0; y < pattern.rows; ++y) {
        for (int x = 0; x < pattern.cols; ++x) {
            pattern.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uchar>((x * 17 + y * 11) % 255),
                        static_cast<uchar>((x * 7 + y * 23) % 255),
                        static_cast<uchar>((x * 29 + y * 5) % 255));
        }
    }
    pattern.copyTo(source(cv::Rect(30, 24, pattern.cols, pattern.rows)));
    if (characterTemplate) {
        *characterTemplate = pattern.clone();
    }

    ProductKey key;
    key.runId = QStringLiteral("stamp-run");
    key.sequence = 1;
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                key,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                source);
    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = cv::Point2f(50.0f, 40.0f);
    pose.datePoly = {
        cv::Point(0, 0),
        cv::Point(99, 0),
        cv::Point(99, 79),
        cv::Point(0, 79)
    };
    pose.trackingPoly = {
        cv::Point(40, 30),
        cv::Point(60, 30),
        cv::Point(60, 50),
        cv::Point(40, 50)
    };
    pose.score = 0.91f;
    return makeDetectionWorkItem(frame, pose);
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
    void positionedWorkProducesGenericResultAndOverlay();
    void positionedWorkRejectsInvalidDateRoiWithoutProductNg();
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

void StampDetectionPipelineTest::positionedWorkProducesGenericResultAndOverlay()
{
    cv::Mat characterTemplate;
    const DetectionWorkItem item = makePositionedItem(&characterTemplate);
    const TemplateMatchPreparedTemplates prepared =
            CharacterTemplateMatcher::prepare({characterTemplate});
    const StampDetectionPipeline pipeline;
    const StampDetectionWorkOutput output = pipeline.detect(
                item,
                QStringLiteral("A"),
                prepared,
                std::vector<int>(1, 0),
                80,
                [](const cv::Mat &, const std::vector<cv::Point> &) {
        StampOverlapResult overlap;
        overlap.isOk = true;
        overlap.finalStampPoly = {
            cv::Point(5, 5),
            cv::Point(15, 5),
            cv::Point(15, 15),
            cv::Point(5, 15)
        };
        return overlap;
    });

    QVERIFY(output.roiValid);
    QVERIFY(output.stampResult.isOk);
    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Completed));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ok));
    QVERIFY(output.detectionResult.overlay.polygons.size() >= 4);
}

void StampDetectionPipelineTest::positionedWorkRejectsInvalidDateRoiWithoutProductNg()
{
    cv::Mat characterTemplate;
    DetectionWorkItem item = makePositionedItem(&characterTemplate);
    item.pose.datePoly.clear();
    const StampDetectionPipeline pipeline;
    const StampDetectionWorkOutput output = pipeline.detect(
                item,
                QStringLiteral("A"),
                CharacterTemplateMatcher::prepare({characterTemplate}),
                std::vector<int>(1, 0),
                80,
                StampDetectionPipeline::OverlapDetectionFunction());

    QVERIFY(!output.roiValid);
    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Cancelled));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::NotEvaluated));
}

QTEST_APPLESS_MAIN(StampDetectionPipelineTest)

#include "stamp_detection_pipeline_test.moc"
