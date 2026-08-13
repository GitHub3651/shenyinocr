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

QTEST_APPLESS_MAIN(OcrDetectionPipelineTest)

#include "ocr_detection_pipeline_test.moc"
