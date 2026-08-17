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

DetectionWorkItem makePositionedItem(cv::Mat *characterTemplate)
{
    cv::Mat source = cv::Mat::zeros(80, 100, CV_8UC3);
    cv::Mat pattern(16, 12, CV_8UC3);
    for (int y = 0; y < pattern.rows; ++y) {
        for (int x = 0; x < pattern.cols; ++x) {
            pattern.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uchar>((x * 19 + y * 3) % 255),
                        static_cast<uchar>((x * 5 + y * 31) % 255),
                        static_cast<uchar>((x * 13 + y * 17) % 255));
        }
    }
    pattern.copyTo(source(cv::Rect(30, 24, pattern.cols, pattern.rows)));
    if (characterTemplate) {
        *characterTemplate = pattern.clone();
    }

    ProductKey key;
    key.runId = QStringLiteral("word-run");
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
    pose.score = 0.92f;
    pose.wordTemplateProfileIndex = 0;
    return makeDetectionWorkItem(frame, pose);
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
    void positionedWorkProducesGenericResultAndOverlay();
    void trackingFailureProducesOneGenericNg();
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

void WordDetectionPipelineTest::positionedWorkProducesGenericResultAndOverlay()
{
    cv::Mat characterTemplate;
    const DetectionWorkItem item = makePositionedItem(&characterTemplate);
    const WordDetectionPipeline pipeline;
    const WordDetectionWorkOutput output = pipeline.detect(
                item,
                QStringLiteral("A"),
                QStringLiteral("profile-a"),
                CharacterGlyphMatcher::prepare({characterTemplate}),
                std::vector<int>(1, 0),
                80);

    QVERIFY(output.roiValid);
    QVERIFY(output.wordResult.isOk);
    QCOMPARE(output.detectedUnits.join(QString()), QStringLiteral("a"));
    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Completed));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ok));
    QVERIFY(output.detectionResult.overlay.polygons.size() >= 3);
}

void WordDetectionPipelineTest::trackingFailureProducesOneGenericNg()
{
    cv::Mat characterTemplate;
    DetectionWorkItem item = makePositionedItem(&characterTemplate);
    item.pose.valid = false;
    const WordDetectionPipeline pipeline;
    const WordDetectionWorkOutput output = pipeline.detect(
                item,
                QString(),
                QString(),
                PreparedCharacterTemplates(),
                std::vector<int>(),
                0);

    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Completed));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ng));
    QCOMPARE(output.detectionResult.diagnostic,
             QStringLiteral(
                 "\u672a\u627e\u5230\u5b57\u5e93"
                 "\u5b9a\u4f4d\u533a\u57df"));
}

QTEST_APPLESS_MAIN(WordDetectionPipelineTest)

#include "word_detection_pipeline_test.moc"
