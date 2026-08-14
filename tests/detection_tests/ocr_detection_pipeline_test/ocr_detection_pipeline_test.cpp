#include <QtTest/QtTest>

#include "detection/ocr/ocr_detection_pipeline.h"

class FakeOcrEngine : public IOcrEngine
{
public:
    explicit FakeOcrEngine(
        const std::vector<std::string> &rawText)
        : m_rawText(rawText)
    {
    }

    std::vector<std::string> recognize(cv::Mat &image) override
    {
        ++recognizeCalls;
        receivedImageWasEmpty = image.empty();
        return m_rawText;
    }

    int recognizeCalls = 0;
    bool receivedImageWasEmpty = true;

private:
    std::vector<std::string> m_rawText;
};

class OcrDetectionPipelineTest : public QObject
{
    Q_OBJECT

private slots:
    void cleaningPreservesCurrentByteRulesAndLineOrder();
    void exactNonEmptyMatchIsOk();
    void emptyCleanedTextIsNg();
    void differentTextIsNg();
    void genericResultPreservesTextVerdictAndPoseOverlay();
    void positionedWorkItemRunsRoiAndReturnsGenericResult();
};

void OcrDetectionPipelineTest::cleaningPreservesCurrentByteRulesAndLineOrder()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    const std::string utf8Text("\xE4\xB8\xAD");
    std::vector<std::string> rawText;
    rawText.push_back(" A@-1.2:3! ");
    rawText.push_back(utf8Text + "#");
    FakeOcrEngine ocrEngine(rawText);
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                std::string(),
                ocrEngine);

    const std::string expected = "A-1.2:3\n" + utf8Text;
    QVERIFY(result.recognizedText == expected);
    QVERIFY(!result.isOk);
    QCOMPARE(ocrEngine.recognizeCalls, 1);
    QVERIFY(!ocrEngine.receivedImageWasEmpty);
}

void OcrDetectionPipelineTest::exactNonEmptyMatchIsOk()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    FakeOcrEngine ocrEngine(
                std::vector<std::string>(1, "AB-12:30"));
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                "AB-12:30",
                ocrEngine);

    QVERIFY(result.recognizedText == "AB-12:30");
    QVERIFY(result.isOk);
}

void OcrDetectionPipelineTest::emptyCleanedTextIsNg()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    FakeOcrEngine ocrEngine(
                std::vector<std::string>(1, " @! "));
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                std::string(),
                ocrEngine);

    QVERIFY(result.recognizedText.empty());
    QVERIFY(!result.isOk);
}

void OcrDetectionPipelineTest::differentTextIsNg()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    FakeOcrEngine ocrEngine(
                std::vector<std::string>(1, "OTHER"));
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                "TARGET",
                ocrEngine);

    QVERIFY(result.recognizedText == "OTHER");
    QVERIFY(!result.isOk);
}

void OcrDetectionPipelineTest::genericResultPreservesTextVerdictAndPoseOverlay()
{
    OcrDetectionResult ocrResult;
    ocrResult.recognizedText = "OTHER";
    ocrResult.isOk = false;
    DetectionPose pose;
    pose.valid = true;
    pose.score = 0.875f;
    pose.trackingPoly = {
        cv::Point(1, 2),
        cv::Point(5, 2),
        cv::Point(5, 6),
        cv::Point(1, 6)
    };
    pose.datePoly = {
        cv::Point(10, 20),
        cv::Point(30, 20),
        cv::Point(30, 40),
        cv::Point(10, 40)
    };

    const DetectionResult result =
            OcrDetectionPipeline::toDetectionResult(
                ocrResult,
                pose,
                12.5);

    QCOMPARE(result.modeId, QStringLiteral("ocr_detection"));
    QVERIFY(result.verdict == AlgorithmVerdict::Ng);
    QVERIFY(result.status == DetectionStatus::Completed);
    QCOMPARE(result.recognizedText, QStringLiteral("OTHER"));
    QCOMPARE(result.diagnostic,
             QStringLiteral(
                 "\u004f\u0043\u0052\u6587\u672c\u4e0e\u76ee\u6807\u4e0d\u4e00\u81f4"));
    QCOMPARE(result.elapsedMs, 12.5);
    QCOMPARE(static_cast<int>(result.overlay.polygons.size()), 2);
    QCOMPARE(result.overlay.polygons[0].role,
             QStringLiteral("tracking"));
    QCOMPARE(result.overlay.polygons[0].points[2].x, 5);
    QCOMPARE(result.overlay.polygons[1].role,
             QStringLiteral("date"));
    QCOMPARE(result.overlay.polygons[1].points[2].y, 40);
}

void OcrDetectionPipelineTest::positionedWorkItemRunsRoiAndReturnsGenericResult()
{
    ProductKey productKey;
    productKey.runId = QStringLiteral("ocr-run");
    productKey.sequence = 1;
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                productKey,
                10,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(40, 60, CV_8UC3, cv::Scalar(20, 40, 60)));
    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = cv::Point2f(10.0f, 10.0f);
    pose.trackingPoly = {
        cv::Point(2, 2),
        cv::Point(18, 2),
        cv::Point(18, 18),
        cv::Point(2, 18)
    };
    pose.datePoly = {
        cv::Point(0, 5),
        cv::Point(20, 5),
        cv::Point(20, 15),
        cv::Point(0, 15)
    };
    FakeOcrEngine ocrEngine(
                std::vector<std::string>(1, "AB-12"));
    const OcrDetectionPipeline pipeline;

    const DetectionResult result = pipeline.detect(
                makeDetectionWorkItem(frame, pose),
                "AB-12",
                ocrEngine);

    QCOMPARE(ocrEngine.recognizeCalls, 1);
    QVERIFY(!ocrEngine.receivedImageWasEmpty);
    QVERIFY(result.status == DetectionStatus::Completed);
    QVERIFY(result.verdict == AlgorithmVerdict::Ok);
    QCOMPARE(result.recognizedText, QStringLiteral("AB-12"));
    QCOMPARE(static_cast<int>(result.overlay.polygons.size()), 2);
}

QTEST_APPLESS_MAIN(OcrDetectionPipelineTest)

#include "ocr_detection_pipeline_test.moc"
