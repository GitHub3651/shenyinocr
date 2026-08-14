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
    void genericResultPreservesVerdictTextAndOverlay();
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

void TissueRollDetectorBaselineTest::genericResultPreservesVerdictTextAndOverlay()
{
    TissueRollResult tissueResult;
    tissueResult.isOk = false;
    tissueResult.rollFound = true;
    tissueResult.processingTimeMs = 17;
    tissueResult.message = "roughness rejected";
    tissueResult.roll.roughnessScore = 8.125;
    tissueResult.roll.outerBbox = cv::Rect(10, 20, 30, 40);

    const DetectionResult result =
            TissueDetectionPipeline::toDetectionResult(
                tissueResult);

    QCOMPARE(result.modeId, QStringLiteral("tissue_detection"));
    QVERIFY(result.verdict == AlgorithmVerdict::Ng);
    QVERIFY(result.status == DetectionStatus::Completed);
    QCOMPARE(result.recognizedText,
             QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a8.125"));
    QCOMPARE(result.diagnostic,
             QStringLiteral("roughness rejected"));
    QCOMPARE(result.elapsedMs, 17.0);
    QCOMPARE(static_cast<int>(result.overlay.polygons.size()), 1);
    QCOMPARE(result.overlay.polygons[0].role,
             QStringLiteral("tissue_roll"));
    QCOMPARE(result.overlay.polygons[0].score, 8.125);
    QCOMPARE(result.overlay.polygons[0].points[0].x, 10);
    QCOMPARE(result.overlay.polygons[0].points[0].y, 20);
    QCOMPARE(result.overlay.polygons[0].points[2].x, 40);
    QCOMPARE(result.overlay.polygons[0].points[2].y, 60);
}

QTEST_APPLESS_MAIN(TissueRollDetectorBaselineTest)

#include "tissue_roll_detector_baseline_test.moc"
