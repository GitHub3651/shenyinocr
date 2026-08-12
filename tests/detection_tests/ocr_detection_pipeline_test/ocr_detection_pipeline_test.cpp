#include <QtTest/QtTest>

#include "detection/ocr/ocr_detection_pipeline.h"

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
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                std::string(),
                [utf8Text](cv::Mat &) {
        std::vector<std::string> rawText;
        rawText.push_back(" A@-1.2:3! ");
        rawText.push_back(utf8Text + "#");
        return rawText;
    });

    const std::string expected = "A-1.2:3\n" + utf8Text;
    QVERIFY(result.recognizedText == expected);
    QVERIFY(!result.isOk);
}

void OcrDetectionPipelineTest::exactNonEmptyMatchIsOk()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                "AB-12:30",
                [](cv::Mat &) {
        return std::vector<std::string>(1, "AB-12:30");
    });

    QVERIFY(result.recognizedText == "AB-12:30");
    QVERIFY(result.isOk);
}

void OcrDetectionPipelineTest::emptyCleanedTextIsNg()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                std::string(),
                [](cv::Mat &) {
        return std::vector<std::string>(1, " @! ");
    });

    QVERIFY(result.recognizedText.empty());
    QVERIFY(!result.isOk);
}

void OcrDetectionPipelineTest::differentTextIsNg()
{
    cv::Mat image = cv::Mat::zeros(1, 1, CV_8UC1);
    const OcrDetectionPipeline pipeline;
    const OcrDetectionResult result = pipeline.detect(
                image,
                "TARGET",
                [](cv::Mat &) {
        return std::vector<std::string>(1, "OTHER");
    });

    QVERIFY(result.recognizedText == "OTHER");
    QVERIFY(!result.isOk);
}

QTEST_APPLESS_MAIN(OcrDetectionPipelineTest)

#include "ocr_detection_pipeline_test.moc"
