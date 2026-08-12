#include <QtTest/QtTest>

#include "detection/tissue/tissue_detection_pipeline.h"

class TissueRollDetectorBaselineTest : public QObject
{
    Q_OBJECT

private slots:
    void recipeDefaultIsSixPointZero();
    void explicitRecipeThresholdIsRetainedByPipeline();
    void emptyImageIsRejectedWithCurrentDiagnostic();
    void blankImageIsRejectedWithoutAFalseRoll();
};

void TissueRollDetectorBaselineTest::recipeDefaultIsSixPointZero()
{
    const TissueRecipeParameters parameters;
    QCOMPARE(parameters.roughnessThreshold, 6.0);
}

void TissueRollDetectorBaselineTest::explicitRecipeThresholdIsRetainedByPipeline()
{
    TissueRecipeParameters parameters;
    parameters.roughnessThreshold = 9.25;
    const TissueDetectionPipeline pipeline(parameters);

    QCOMPARE(pipeline.roughnessThreshold(), 9.25);
}

void TissueRollDetectorBaselineTest::emptyImageIsRejectedWithCurrentDiagnostic()
{
    const TissueRecipeParameters parameters;
    const TissueDetectionPipeline pipeline(parameters);
    const TissueRollResult result = pipeline.detect(cv::Mat());

    QVERIFY(!result.isOk);
    QVERIFY(!result.rollFound);
    QCOMPARE(result.imageWidth, 0);
    QCOMPARE(result.imageHeight, 0);
    QCOMPARE(QString::fromStdString(result.message), QStringLiteral("empty image"));
}

void TissueRollDetectorBaselineTest::blankImageIsRejectedWithoutAFalseRoll()
{
    const TissueRecipeParameters parameters;
    const TissueDetectionPipeline pipeline(parameters);
    const cv::Mat image = cv::Mat::zeros(128, 128, CV_8UC3);
    const TissueRollResult result = pipeline.detect(image);

    QVERIFY(!result.isOk);
    QVERIFY(!result.rollFound);
    QCOMPARE(result.imageWidth, image.cols);
    QCOMPARE(result.imageHeight, image.rows);
    QVERIFY(QString::fromStdString(result.message).contains(QStringLiteral("rollFound=false")));
    QVERIFY(QString::fromStdString(result.message).contains(
                QStringLiteral("thresholds(rough<6.000)")));
}

QTEST_APPLESS_MAIN(TissueRollDetectorBaselineTest)

#include "tissue_roll_detector_baseline_test.moc"
