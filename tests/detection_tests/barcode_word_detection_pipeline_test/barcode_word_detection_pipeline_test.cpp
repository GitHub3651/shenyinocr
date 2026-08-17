#include <QtTest/QtTest>

#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "devices/barcode/barcode_decoder.h"

namespace {

class FakeBarcodeDecoder : public IBarcodeDecoder
{
public:
    bool loaded = true;
    QString loadError;
    BarcodeReadResult nextResult;
    int nextStrategyId = 6;
    unsigned int nextOptionFlags = 3u;
    int ensureLoadedCalls = 0;
    int decodeCalls = 0;
    int receivedPreferredStrategyId = -2;
    unsigned int receivedPreferredOptionFlags = 0u;

    bool ensureLoaded() override
    {
        ++ensureLoadedCalls;
        return loaded;
    }

    QString lastError() const override
    {
        return loadError;
    }

    BarcodeReadResult decode(
            const cv::Mat &grayRoi,
            const BarcodeDecodeOptions &,
            int preferredStrategyId,
            unsigned int preferredOptionFlags,
            int *successfulStrategyId,
            unsigned int *successfulOptionFlags) override
    {
        ++decodeCalls;
        receivedPreferredStrategyId = preferredStrategyId;
        receivedPreferredOptionFlags = preferredOptionFlags;
        if (successfulStrategyId) {
            *successfulStrategyId = nextStrategyId;
        }
        if (successfulOptionFlags) {
            *successfulOptionFlags = nextOptionFlags;
        }
        if (grayRoi.empty()) {
            BarcodeReadResult invalid;
            invalid.status = BarcodeReadStatus::InvalidRoi;
            return invalid;
        }
        return nextResult;
    }
};

DetectionWorkItem makeBarcodeWordItem(cv::Mat *characterTemplate)
{
    cv::Mat source = cv::Mat::zeros(100, 140, CV_8UC3);
    cv::Mat pattern(16, 12, CV_8UC3);
    for (int y = 0; y < pattern.rows; ++y) {
        for (int x = 0; x < pattern.cols; ++x) {
            pattern.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uchar>((x * 19 + y * 3) % 255),
                        static_cast<uchar>((x * 5 + y * 31) % 255),
                        static_cast<uchar>((x * 13 + y * 17) % 255));
        }
    }
    // The production matcher downsamples the full date ROI and each character
    // template independently at 0.5.  Keep this synthetic character on an
    // even offset relative to the padded date ROI origin (35, 15), so the
    // fixture tests orchestration instead of an artificial sampling-phase
    // mismatch.
    pattern.copyTo(source(cv::Rect(79, 49,
                                   pattern.cols,
                                   pattern.rows)));
    if (characterTemplate) {
        *characterTemplate = pattern.clone();
    }

    ProductKey key;
    key.runId = QStringLiteral("barcode-word-run");
    key.sequence = 1;
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                key,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                source);
    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = cv::Point2f(70.0f, 50.0f);
    pose.wordTemplateProfileIndex = 0;
    pose.trackingPoly = {
        cv::Point(55, 35),
        cv::Point(85, 35),
        cv::Point(85, 65),
        cv::Point(55, 65)
    };
    pose.barcodePoly = {
        cv::Point(8, 8),
        cv::Point(38, 8),
        cv::Point(38, 38),
        cv::Point(8, 38)
    };
    pose.datePoly = {
        cv::Point(55, 35),
        cv::Point(130, 35),
        cv::Point(130, 90),
        cv::Point(55, 90)
    };
    pose.score = 0.95f;
    pose.trackingElapsedMs = 2.0;
    return makeDetectionWorkItem(frame, pose);
}

BarcodeWordDetectionWorkOutput runPositionedWork(
        const DetectionWorkItem &item,
        const cv::Mat &characterTemplate,
        const QString &targetText,
        FakeBarcodeDecoder *decoder,
        const BarcodeWordDecodeStrategyState &strategy =
            BarcodeWordDecodeStrategyState())
{
    const BarcodeWordDetectionPipeline pipeline;
    BarcodeDecodeOptions options;
    options.roiPaddingPercent = 8;
    return pipeline.detect(
                item,
                targetText,
                QStringLiteral("profile-a"),
                CharacterGlyphMatcher::prepare({characterTemplate}),
                std::vector<int>(1, 0),
                80,
                options,
                strategy,
                decoder);
}

} // namespace

class BarcodeWordDetectionPipelineTest : public QObject
{
    Q_OBJECT

private slots:
    void unreadableBarcodeShortCircuitsDateDetection();
    void readableBarcodeWithoutDateStageDoesNotExecute();
    void readableBarcodeAndDateOkAreOk();
    void readableBarcodeAndDateNgAreNg();
    void positionedUnreadableBarcodeProducesOneNgWithoutDateMatch();
    void positionedReadableBarcodeAndMatchingDateAreOk();
    void positionedReadableBarcodeAndMismatchingDateAreNg();
    void invalidBarcodePolygonIsRejectedBeforeDecoderCall();
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

void BarcodeWordDetectionPipelineTest::positionedUnreadableBarcodeProducesOneNgWithoutDateMatch()
{
    cv::Mat characterTemplate;
    const DetectionWorkItem item = makeBarcodeWordItem(
                &characterTemplate);
    FakeBarcodeDecoder decoder;
    decoder.nextResult.status = BarcodeReadStatus::NotFound;
    decoder.nextResult.readable = false;
    BarcodeWordDecodeStrategyState strategy;
    strategy.preferredStrategyId = 4;
    strategy.preferredOptionFlags = 2u;
    strategy.consecutiveFailures = 1;

    const BarcodeWordDetectionWorkOutput output = runPositionedWork(
                item,
                characterTemplate,
                QStringLiteral("A"),
                &decoder,
                strategy);

    QCOMPARE(decoder.decodeCalls, 1);
    QCOMPARE(decoder.receivedPreferredStrategyId, 4);
    QCOMPARE(decoder.receivedPreferredOptionFlags, 2u);
    QCOMPARE(output.nextDecodeStrategy.consecutiveFailures, 2);
    QVERIFY(!output.barcodeWordResult.dateDetectionExecuted);
    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Completed));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ng));
}

void BarcodeWordDetectionPipelineTest::positionedReadableBarcodeAndMatchingDateAreOk()
{
    cv::Mat characterTemplate;
    const DetectionWorkItem item = makeBarcodeWordItem(
                &characterTemplate);
    FakeBarcodeDecoder decoder;
    decoder.nextResult.status = BarcodeReadStatus::Success;
    decoder.nextResult.readable = true;
    decoder.nextResult.text = QStringLiteral("CODE-123");
    decoder.nextResult.cornersInRoi = {
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(10.0f, 0.0f),
        cv::Point2f(10.0f, 10.0f),
        cv::Point2f(0.0f, 10.0f)
    };

    const BarcodeWordDetectionWorkOutput output = runPositionedWork(
                item,
                characterTemplate,
                QStringLiteral("A"),
                &decoder);

    QVERIFY(output.barcodeRoiValid);
    QVERIFY(output.dateRoiValid);
    QVERIFY(output.barcodeWordResult.barcodeIsReadable);
    QVERIFY(output.barcodeWordResult.dateDetectionExecuted);
    QVERIFY(output.barcodeWordResult.dateResultProduced);
    QVERIFY(output.barcodeWordResult.isOk);
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ok));
    QCOMPARE(output.nextDecodeStrategy.preferredStrategyId,
             decoder.nextStrategyId);
    QCOMPARE(output.nextDecodeStrategy.preferredOptionFlags,
             decoder.nextOptionFlags);
    QCOMPARE(output.nextDecodeStrategy.consecutiveFailures, 0);
    QCOMPARE(static_cast<int>(output.barcode.cornersInOriginal.size()),
             4);
}

void BarcodeWordDetectionPipelineTest::positionedReadableBarcodeAndMismatchingDateAreNg()
{
    cv::Mat characterTemplate;
    const DetectionWorkItem item = makeBarcodeWordItem(
                &characterTemplate);
    FakeBarcodeDecoder decoder;
    decoder.nextResult.status = BarcodeReadStatus::Success;
    decoder.nextResult.readable = true;
    decoder.nextResult.text = QStringLiteral("CODE-456");

    const BarcodeWordDetectionWorkOutput output = runPositionedWork(
                item,
                characterTemplate,
                QStringLiteral("AA"),
                &decoder);

    QVERIFY(output.barcodeWordResult.barcodeIsReadable);
    QVERIFY(output.barcodeWordResult.dateDetectionExecuted);
    QVERIFY(output.barcodeWordResult.dateResultProduced);
    QVERIFY(!output.barcodeWordResult.dateIsOk);
    QVERIFY(!output.barcodeWordResult.isOk);
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ng));
}

void BarcodeWordDetectionPipelineTest::invalidBarcodePolygonIsRejectedBeforeDecoderCall()
{
    cv::Mat characterTemplate;
    DetectionWorkItem item = makeBarcodeWordItem(
                &characterTemplate);
    item.pose.barcodePoly.clear();
    FakeBarcodeDecoder decoder;
    decoder.nextResult.status = BarcodeReadStatus::Success;
    decoder.nextResult.readable = true;

    const BarcodeWordDetectionWorkOutput output = runPositionedWork(
                item,
                characterTemplate,
                QStringLiteral("A"),
                &decoder);

    QCOMPARE(decoder.ensureLoadedCalls, 0);
    QCOMPARE(decoder.decodeCalls, 0);
    QCOMPARE(static_cast<int>(output.detectionResult.status),
             static_cast<int>(DetectionStatus::Completed));
    QCOMPARE(static_cast<int>(output.detectionResult.verdict),
             static_cast<int>(AlgorithmVerdict::Ng));
}

QTEST_APPLESS_MAIN(BarcodeWordDetectionPipelineTest)

#include "barcode_word_detection_pipeline_test.moc"
