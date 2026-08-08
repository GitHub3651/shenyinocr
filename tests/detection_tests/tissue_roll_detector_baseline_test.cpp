#include <QtTest/QtTest>

#include "TissueRollDetector.h"

class TissueRollDetectorBaselineTest : public QObject
{
    Q_OBJECT

private slots:
    void currentInternalDefaultIsFivePointTwo();
    void explicitRecipeThresholdIsRetained();
    void emptyImageIsRejectedWithCurrentDiagnostic();
    void blankImageIsRejectedWithoutAFalseRoll();
};

void TissueRollDetectorBaselineTest::currentInternalDefaultIsFivePointTwo()
{
    QCOMPARE(TissueRollDetector::defaultRoughnessThreshold(), 5.2);
    const TissueRollConfig currentDefaultConfig;
    QCOMPARE(currentDefaultConfig.roughnessThreshold, 5.2);
}

void TissueRollDetectorBaselineTest::explicitRecipeThresholdIsRetained()
{
    const TissueRollConfig config(6.0);
    QCOMPARE(config.roughnessThreshold, 6.0);
}

void TissueRollDetectorBaselineTest::emptyImageIsRejectedWithCurrentDiagnostic()
{
    const TissueRollDetector detector(TissueRollConfig(6.0));
    const TissueRollResult result = detector.processImage(cv::Mat());

    QVERIFY(!result.isOk);
    QVERIFY(!result.rollFound);
    QCOMPARE(result.imageWidth, 0);
    QCOMPARE(result.imageHeight, 0);
    QCOMPARE(QString::fromStdString(result.message), QStringLiteral("empty image"));
}

void TissueRollDetectorBaselineTest::blankImageIsRejectedWithoutAFalseRoll()
{
    const TissueRollDetector detector(TissueRollConfig(6.0));
    const cv::Mat image = cv::Mat::zeros(128, 128, CV_8UC3);
    const TissueRollResult result = detector.processImage(image);

    QVERIFY(!result.isOk);
    QVERIFY(!result.rollFound);
    QCOMPARE(result.imageWidth, image.cols);
    QCOMPARE(result.imageHeight, image.rows);
    QVERIFY(QString::fromStdString(result.message).contains(QStringLiteral("rollFound=false")));
}

QTEST_APPLESS_MAIN(TissueRollDetectorBaselineTest)

#include "tissue_roll_detector_baseline_test.moc"
