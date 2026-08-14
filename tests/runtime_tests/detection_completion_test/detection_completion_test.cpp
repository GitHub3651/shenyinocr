#include <QtTest>

#include "TrackingTypes.h"
#include "detection/common/detection_roi_geometry.h"
#include "runtime/detection_shadow_comparator.h"
#include "runtime/detection_worker.h"
#include "runtime/detection_session.h"
#include "runtime/frame_queue.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_start_preflight.h"
#include "runtime/inspection_runtime_controller.h"
#include "runtime/result_handler.h"
#include "runtime/result_presentation_mailbox.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

namespace {
ProductKey testProductKey(quint64 sequence)
{
    ProductKey key;
    key.runId = QStringLiteral("test-run");
    key.sequence = sequence;
    return key;
}

ImageSaveTask testSaveTask(
    quint64 sequence,
    const QStringList &itemNames)
{
    ImageSaveTask task;
    task.productKey = testProductKey(sequence);
    for (const QString &itemName : itemNames) {
        ImageSaveItem item;
        item.image = QImage(2, 2, QImage::Format_RGB32);
        item.image.fill(Qt::white);
        item.filePath = itemName;
        item.format = QByteArrayLiteral("PNG");
        task.items.push_back(item);
    }
    return task;
}

DetectionCompletion testCompletion(
    quint64 sequence,
    AlgorithmVerdict verdict)
{
    DetectionCompletion completion;
    completion.frame = makeFrameData(
                testProductKey(sequence),
                sequence,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
    completion.result.modeId = QStringLiteral("test");
    completion.result.verdict = verdict;
    completion.result.status = DetectionStatus::Completed;
    return completion;
}

DetectionResult shadowSampleResult()
{
    DetectionResult result;
    result.modeId = QStringLiteral("word_matching");
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("8135");
    result.diagnostic = QStringLiteral("primary diagnostic");
    result.elapsedMs = 12.5;

    DetectionOverlayPolygon polygon;
    polygon.role = QStringLiteral("date_roi");
    polygon.points.push_back(cv::Point(10, 20));
    polygon.points.push_back(cv::Point(30, 20));
    polygon.points.push_back(cv::Point(30, 40));
    polygon.points.push_back(cv::Point(10, 40));
    polygon.score = 0.875;
    result.overlay.polygons.push_back(polygon);
    return result;
}

std::shared_ptr<const FrameData> workerTestFrame(quint64 sequence)
{
    return makeFrameData(
                testProductKey(sequence),
                sequence,
                0,
                QDateTime::fromMSecsSinceEpoch(
                    static_cast<qint64>(sequence)),
                cv::Mat(
                    2,
                    2,
                    CV_8UC1,
                    cv::Scalar(static_cast<int>(sequence % 255))));
}
}

class DetectionCompletionTest : public QObject
{
    Q_OBJECT

private slots:
    void productKeyRequiresRunAndPositiveSequence();
    void frameFactoryOwnsIndependentImage();
    void completionRequiresFrameAndValidProductKey();
    void resultCarriesVerdictStatusTextAndTimingWithoutImage();
    void overlayPreservesOrderedBusinessPolygons();
    void immutableFrameCanBeSharedForShortLivedConsumers();
    void sessionBeginCreatesNewRunAndResetsProductSequence();
    void sessionCompletionOwnsFrameAndCopiesDetectionResult();
    void sessionAcceptsFrameBeforeDetectionAndCompletesSameFrame();
    void sessionRejectsInvalidForeignAndRepeatedFrameCompletion();
    void resultHandlerRejectsInvalidCompletionWithoutSideEffects();
    void resultHandlerRecordsOkAndRequestsOkOutput();
    void resultHandlerRecordsImmediateNgAndRequestsNgOutput();
    void resultHandlerDelaysNgUntilConfiguredProductOffset();
    void resultHandlerPreservesFourImageSaveModes();
    void resultHandlerResetsStatisticsAndPendingOutputsSeparately();
    void runtimeControllerTransitionsFromStartToStop();
    void runtimeControllerRejectsConcurrentAndFaultedStarts();
    void runtimeControllerOwnsCompletionAndResultHandling();
    void runtimeControllerRejectsDuplicateAndForeignCompletions();
    void runtimeControllerStartsNewRunWithoutResettingStatistics();
    void runtimeControllerPreservesSeparateResetScopes();
    void runtimeControllerStopsAdmissionBeforeDrainingAcceptedFrames();
    void startAccessAcceptsIdleOpenCamera();
    void startAccessPreservesGuardOrder();
    void dirtySettingsPrecedePlcConnectivity();
    void tissueStartNeedsNoTemplateAssets();
    void singleTemplateStartReportsOrderedMissingAssets();
    void wordStartRequiresProfilesAndCompleteCharacters();
    void barcodeStartAggregatesDecoderAndProfileErrors();
    void validWordAndBarcodeStartsAreAccepted();
    void runPlanSelectsSoftwareSingleTemplate();
    void runPlanSelectsHardwareBarcodeProfiles();
    void runPlanSelectsWholeFrameAndWordTracking();
    void runtimeSettingsRetainValidValues();
    void runtimeSettingsUseLegacyDefaultsForUnknownIndexes();
    void runtimeSettingsRejectInvalidImageThreshold();
    void runtimeSettingsRejectInvalidTissueThreshold();
    void shadowComparisonAcceptsEquivalentResults();
    void shadowComparisonIgnoresTimingAndDiagnosticByDefault();
    void shadowComparisonReportsBusinessResultDifferences();
    void shadowComparisonAppliesGeometryTolerance();
    void shadowComparisonReportsOrderedOverlayDifferences();
    void shadowComparisonCanIncludeDiagnostic();
    void frameQueueRequiresValidFramesAndPositiveCapacity();
    void frameQueuePreservesSubmissionOrder();
    void frameQueueWaitsForSpaceWithoutDroppingFrame();
    void frameQueueCancellationReleasesFramesAndSubmitter();
    void frameQueuePreservesPositionedDetectionWorkItem();
    void detectionWorkerProcessesFramesSeriallyInOrder();
    void detectionWorkerReceivesPositionedDetectionWorkItem();
    void runtimeControllerAcceptedFramesFlowThroughDetectionWorker();
    void detectionWorkerRejectsSubmissionOutsideRun();
    void detectionWorkerCancellationSuppressesPendingResults();
    void detectionWorkerCanRestartAfterWait();
    void uiCompletionMailboxSerializesWholeProductWork();
    void uiCompletionMailboxCancellationReleasesProducer();
    void roiPaddingIsClampedToImageBounds();
    void outsidePolygonIsClampedToNearestImageEdge();
    void orientedDateRoiClampsPaddingAtImageEdge();
    void saveTaskRequiresProductAndAllItems();
    void saveServicePreservesTaskAndItemOrder();
    void fullQueueWaitsForSpaceWithoutDroppingTask();
    void writeFailureIsCountedAndReported();
    void shutdownRejectsNewTasks();
};

void DetectionCompletionTest::productKeyRequiresRunAndPositiveSequence()
{
    ProductKey key;
    QVERIFY(!key.isValid());

    key.runId = QStringLiteral("run-1");
    QVERIFY(!key.isValid());

    key.sequence = 1;
    QVERIFY(key.isValid());
}

void DetectionCompletionTest::frameFactoryOwnsIndependentImage()
{
    ProductKey key;
    key.runId = QStringLiteral("run-1");
    key.sequence = 7;

    cv::Mat source(2, 2, CV_8UC1, cv::Scalar(31));
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(
                123456,
                Qt::UTC);
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                key,
                19,
                2,
                timestamp,
                source);

    source.setTo(cv::Scalar(99));

    QVERIFY(frame);
    QCOMPARE(frame->productKey.runId, QStringLiteral("run-1"));
    QCOMPARE(frame->productKey.sequence, quint64(7));
    QCOMPARE(frame->frameNumber, quint64(19));
    QCOMPARE(frame->cameraIndex, 2);
    QCOMPARE(frame->timestampUtc, timestamp);
    QCOMPARE(frame->originalImage.at<uchar>(0, 0), uchar(31));
}

void DetectionCompletionTest::completionRequiresFrameAndValidProductKey()
{
    DetectionCompletion completion;
    QVERIFY(!completion.isValid());

    ProductKey invalidKey;
    invalidKey.runId = QStringLiteral("run-1");
    completion.frame = makeFrameData(
                invalidKey,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
    QVERIFY(!completion.isValid());

    ProductKey validKey;
    validKey.runId = QStringLiteral("run-1");
    validKey.sequence = 1;
    completion.frame = makeFrameData(
                validKey,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
    QVERIFY(completion.isValid());
}

void DetectionCompletionTest::resultCarriesVerdictStatusTextAndTimingWithoutImage()
{
    DetectionResult result;
    result.modeId = QStringLiteral("ocr");
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("2026-08-13");
    result.diagnostic = QStringLiteral("target mismatch");
    result.elapsedMs = 12.5;

    QCOMPARE(result.modeId, QStringLiteral("ocr"));
    QVERIFY(result.verdict == AlgorithmVerdict::Ng);
    QVERIFY(result.status == DetectionStatus::Completed);
    QCOMPARE(result.recognizedText, QStringLiteral("2026-08-13"));
    QCOMPARE(result.diagnostic, QStringLiteral("target mismatch"));
    QCOMPARE(result.elapsedMs, 12.5);
}

void DetectionCompletionTest::overlayPreservesOrderedBusinessPolygons()
{
    DetectionResult result;
    DetectionOverlayPolygon tracking;
    tracking.role = QStringLiteral("tracking");
    tracking.points.push_back(cv::Point(1, 2));
    tracking.score = 0.8;
    result.overlay.polygons.push_back(tracking);

    DetectionOverlayPolygon date;
    date.role = QStringLiteral("date");
    date.points.push_back(cv::Point(3, 4));
    result.overlay.polygons.push_back(date);

    QCOMPARE(static_cast<int>(result.overlay.polygons.size()), 2);
    QCOMPARE(result.overlay.polygons[0].role, QStringLiteral("tracking"));
    QCOMPARE(result.overlay.polygons[0].score, 0.8);
    QCOMPARE(result.overlay.polygons[1].role, QStringLiteral("date"));
    QCOMPARE(result.overlay.polygons[1].points[0].x, 3);
}

void DetectionCompletionTest::immutableFrameCanBeSharedForShortLivedConsumers()
{
    ProductKey key;
    key.runId = QStringLiteral("run-2");
    key.sequence = 3;

    DetectionCompletion completion;
    completion.frame = makeFrameData(
                key,
                3,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(3, 4, CV_8UC3, cv::Scalar(4, 5, 6)));
    const std::shared_ptr<const FrameData> saveConsumer = completion.frame;

    QCOMPARE(completion.frame.get(), saveConsumer.get());
    QCOMPARE(saveConsumer->originalImage.cols, 4);
    QCOMPARE(saveConsumer->originalImage.rows, 3);
    QCOMPARE(saveConsumer.use_count(), 2L);
}

void DetectionCompletionTest::sessionBeginCreatesNewRunAndResetsProductSequence()
{
    int runNumber = 0;
    DetectionSession session([&runNumber]() {
        return QStringLiteral("run-%1").arg(++runNumber);
    });
    QVERIFY(!session.isActive());

    QCOMPARE(session.begin(), QStringLiteral("run-1"));
    QCOMPARE(session.completedProductCount(), quint64(0));

    DetectionResult result;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion first = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                result);
    const DetectionCompletion second = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(2)),
                result);
    QCOMPARE(first.frame->productKey.runId, QStringLiteral("run-1"));
    QCOMPARE(first.frame->productKey.sequence, quint64(1));
    QCOMPARE(second.frame->productKey.sequence, quint64(2));
    QCOMPARE(session.completedProductCount(), quint64(2));

    QCOMPARE(session.begin(), QStringLiteral("run-2"));
    QCOMPARE(session.completedProductCount(), quint64(0));
    const DetectionCompletion restarted = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(3)),
                result);
    QCOMPARE(restarted.frame->productKey.runId, QStringLiteral("run-2"));
    QCOMPARE(restarted.frame->productKey.sequence, quint64(1));
}

void DetectionCompletionTest::sessionCompletionOwnsFrameAndCopiesDetectionResult()
{
    DetectionSession session([]() {
        return QStringLiteral("fixed-run");
    });
    cv::Mat source(2, 3, CV_8UC1, cv::Scalar(17));
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(
                987654,
                Qt::UTC);

    DetectionResult result;
    result.modeId = QStringLiteral("word");
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("123");
    result.diagnostic = QStringLiteral("count mismatch");
    result.elapsedMs = 4.5;
    DetectionOverlayPolygon polygon;
    polygon.role = QStringLiteral("character");
    polygon.points.push_back(cv::Point(2, 3));
    result.overlay.polygons.push_back(polygon);

    const DetectionCompletion completion = session.complete(
                source,
                result,
                41,
                2,
                timestamp);
    source.setTo(cv::Scalar(99));
    result.recognizedText = QStringLiteral("changed");

    QVERIFY(completion.isValid());
    QCOMPARE(completion.frame->productKey.runId, QStringLiteral("fixed-run"));
    QCOMPARE(completion.frame->productKey.sequence, quint64(1));
    QCOMPARE(completion.frame->frameNumber, quint64(41));
    QCOMPARE(completion.frame->cameraIndex, 2);
    QCOMPARE(completion.frame->timestampUtc, timestamp);
    QCOMPARE(completion.frame->originalImage.at<uchar>(0, 0), uchar(17));
    QCOMPARE(completion.result.recognizedText, QStringLiteral("123"));
    QCOMPARE(completion.result.overlay.polygons[0].role,
             QStringLiteral("character"));
}

void DetectionCompletionTest::sessionAcceptsFrameBeforeDetectionAndCompletesSameFrame()
{
    DetectionSession session([]() {
        return QStringLiteral("accepted-run");
    });
    session.begin();

    cv::Mat source(2, 3, CV_8UC1, cv::Scalar(17));
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(
                1234567,
                Qt::UTC);
    const std::shared_ptr<const FrameData> frame = session.acceptFrame(
                source,
                91,
                2,
                timestamp);
    source.setTo(cv::Scalar(99));

    QVERIFY(frame);
    QCOMPARE(frame->productKey.runId, QStringLiteral("accepted-run"));
    QCOMPARE(frame->productKey.sequence, quint64(1));
    QCOMPARE(frame->frameNumber, quint64(91));
    QCOMPARE(frame->cameraIndex, 2);
    QCOMPARE(frame->timestampUtc, timestamp);
    QCOMPARE(frame->originalImage.at<uchar>(0, 0), uchar(17));
    QCOMPARE(session.acceptedProductCount(), quint64(1));
    QCOMPARE(session.completedProductCount(), quint64(0));

    DetectionResult result;
    result.modeId = QStringLiteral("word_matching");
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion completion = session.complete(frame, result);

    QVERIFY(completion.isValid());
    QCOMPARE(completion.frame.get(), frame.get());
    QCOMPARE(completion.result.modeId, QStringLiteral("word_matching"));
    QCOMPARE(session.acceptedProductCount(), quint64(1));
    QCOMPARE(session.completedProductCount(), quint64(1));
}

void DetectionCompletionTest::sessionRejectsInvalidForeignAndRepeatedFrameCompletion()
{
    DetectionSession session([]() {
        return QStringLiteral("current-run");
    });
    session.begin();

    QVERIFY(!session.acceptFrame(cv::Mat()));
    QCOMPARE(session.acceptedProductCount(), quint64(0));

    const std::shared_ptr<const FrameData> accepted = session.acceptFrame(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(7)));
    QVERIFY(accepted);
    QCOMPARE(session.acceptedProductCount(), quint64(1));

    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;

    const std::shared_ptr<const FrameData> forged = makeFrameData(
                accepted->productKey,
                accepted->frameNumber,
                accepted->cameraIndex,
                accepted->timestampUtc,
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(9)));
    QVERIFY(!session.complete(forged, result).isValid());
    QVERIFY(session.complete(accepted, result).isValid());
    QVERIFY(!session.complete(accepted, result).isValid());

    ProductKey foreignKey;
    foreignKey.runId = QStringLiteral("foreign-run");
    foreignKey.sequence = 1;
    const std::shared_ptr<const FrameData> foreign = makeFrameData(
                foreignKey,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(8)));
    QVERIFY(!session.complete(foreign, result).isValid());
    QCOMPARE(session.acceptedProductCount(), quint64(1));
    QCOMPARE(session.completedProductCount(), quint64(1));
}

void DetectionCompletionTest::resultHandlerRejectsInvalidCompletionWithoutSideEffects()
{
    DetectionResultHandler handler;
    const DetectionResultHandlingOutcome outcome = handler.record(
                DetectionCompletion(),
                3,
                0);

    QVERIFY(!outcome.resultRecorded);
    QVERIFY(outcome.imageSaveAction
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(outcome.plcAction == DetectionPlcAction::NoRequest);
    QCOMPARE(handler.totalCount(), 0);
    QCOMPARE(handler.ngCount(), 0);
    QCOMPARE(handler.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::resultHandlerRecordsOkAndRequestsOkOutput()
{
    DetectionResultHandler handler;
    const DetectionResultHandlingOutcome outcome = handler.record(
                testCompletion(1, AlgorithmVerdict::Ok),
                2,
                7);

    QVERIFY(outcome.resultRecorded);
    QVERIFY(outcome.imageSaveAction
            == DetectionResultSaveAction::SaveOk);
    QVERIFY(outcome.plcAction == DetectionPlcAction::RequestOk);
    QCOMPARE(outcome.statistics.totalCount, 1);
    QCOMPARE(outcome.statistics.ngCount, 0);
    QCOMPARE(outcome.statistics.passRatePercent(), 100.0);
    QCOMPARE(handler.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::resultHandlerRecordsImmediateNgAndRequestsNgOutput()
{
    DetectionResultHandler handler;
    const DetectionResultHandlingOutcome outcome = handler.record(
                testCompletion(1, AlgorithmVerdict::Ng),
                1,
                0);

    QVERIFY(outcome.resultRecorded);
    QVERIFY(outcome.imageSaveAction
            == DetectionResultSaveAction::SaveNg);
    QVERIFY(outcome.plcAction == DetectionPlcAction::RequestNg);
    QCOMPARE(outcome.statistics.totalCount, 1);
    QCOMPARE(outcome.statistics.ngCount, 1);
    QCOMPARE(outcome.statistics.passRatePercent(), 0.0);
    QCOMPARE(handler.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::resultHandlerDelaysNgUntilConfiguredProductOffset()
{
    DetectionResultHandler handler;
    const DetectionResultHandlingOutcome ngOutcome = handler.record(
                testCompletion(1, AlgorithmVerdict::Ng),
                0,
                2);

    QVERIFY(ngOutcome.plcAction == DetectionPlcAction::NoRequest);
    QCOMPARE(handler.pendingDelayedNgCount(), 1);
    QVERIFY(!handler.consumeDueDelayedNgRequest());

    const DetectionResultHandlingOutcome okOutcome = handler.record(
                testCompletion(2, AlgorithmVerdict::Ok),
                0,
                2);
    QVERIFY(okOutcome.plcAction == DetectionPlcAction::RequestOk);
    QVERIFY(handler.consumeDueDelayedNgRequest());
    QVERIFY(!handler.consumeDueDelayedNgRequest());
    QCOMPARE(handler.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::resultHandlerPreservesFourImageSaveModes()
{
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ok, 0)
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ng, 0)
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ok, 1)
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ng, 1)
            == DetectionResultSaveAction::SaveNg);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ok, 2)
            == DetectionResultSaveAction::SaveOk);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ng, 2)
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::Ok, 3)
            == DetectionResultSaveAction::SaveOk);
    QVERIFY(DetectionResultHandler::imageSaveActionFor(
                AlgorithmVerdict::NotEvaluated, 3)
            == DetectionResultSaveAction::SaveNg);
}

void DetectionCompletionTest::resultHandlerResetsStatisticsAndPendingOutputsSeparately()
{
    DetectionResultHandler handler;
    handler.record(testCompletion(1, AlgorithmVerdict::Ng), 0, 3);
    handler.record(testCompletion(2, AlgorithmVerdict::Ok), 0, 0);

    handler.resetNgCount();
    QCOMPARE(handler.totalCount(), 2);
    QCOMPARE(handler.ngCount(), 0);
    QCOMPARE(handler.pendingDelayedNgCount(), 1);

    handler.resetStatistics();
    QCOMPARE(handler.totalCount(), 0);
    QCOMPARE(handler.ngCount(), 0);
    QCOMPARE(handler.pendingDelayedNgCount(), 1);

    handler.clearPendingDelayedNgRequests();
    QCOMPARE(handler.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::runtimeControllerTransitionsFromStartToStop()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("coordinated-run");
    });
    DetectionResult result;
    result.status = DetectionStatus::Completed;

    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.isBusy());
    QVERIFY(!controller.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                result).isValid());
    QCOMPARE(controller.beginStart(), QStringLiteral("coordinated-run"));
    QVERIFY(controller.state() == InspectionRuntimeState::Starting);
    QVERIFY(controller.isBusy());
    QVERIFY(!controller.isRunning());
    QVERIFY(controller.markRunning());
    QVERIFY(controller.state() == InspectionRuntimeState::Running);
    QVERIFY(controller.isRunning());
    QVERIFY(controller.requestStop());
    QVERIFY(controller.state() == InspectionRuntimeState::Stopping);
    QVERIFY(controller.requestStop());

    controller.finishStop();
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.isBusy());
    QVERIFY(!controller.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(2)),
                result).isValid());
}

void DetectionCompletionTest::runtimeControllerRejectsConcurrentAndFaultedStarts()
{
    int runNumber = 0;
    InspectionRuntimeController controller([&runNumber]() {
        return QStringLiteral("run-%1").arg(++runNumber);
    });

    QCOMPARE(controller.beginStart(), QStringLiteral("run-1"));
    QVERIFY(controller.beginStart().isEmpty());
    QCOMPARE(controller.runId(), QStringLiteral("run-1"));
    QVERIFY(controller.markRunning());
    QVERIFY(!controller.markRunning());
    controller.markFault();
    QVERIFY(controller.state() == InspectionRuntimeState::Fault);
    QVERIFY(controller.beginStart().isEmpty());

    controller.acknowledgeFault();
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QCOMPARE(controller.beginStart(), QStringLiteral("run-2"));
}

void DetectionCompletionTest::runtimeControllerOwnsCompletionAndResultHandling()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("owned-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());

    DetectionResult result;
    result.modeId = QStringLiteral("stamp");
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("12");
    const DetectionCompletion completion = controller.complete(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(23)),
                result);
    const DetectionResultHandlingOutcome outcome = controller.record(
                completion,
                1,
                0);

    QVERIFY(completion.isValid());
    QCOMPARE(completion.frame->productKey.runId,
             QStringLiteral("owned-run"));
    QCOMPARE(completion.frame->productKey.sequence, quint64(1));
    QCOMPARE(controller.completedProductCount(), quint64(1));
    QVERIFY(outcome.resultRecorded);
    QVERIFY(outcome.imageSaveAction
            == DetectionResultSaveAction::SaveNg);
    QVERIFY(outcome.plcAction == DetectionPlcAction::RequestNg);
    QCOMPARE(controller.totalCount(), 1);
    QCOMPARE(controller.ngCount(), 1);
}

void DetectionCompletionTest::runtimeControllerRejectsDuplicateAndForeignCompletions()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("current-run");
    });
    controller.beginStart();
    controller.markRunning();

    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion completion = controller.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                result);
    QVERIFY(controller.record(completion, 0, 0).resultRecorded);

    const DetectionResultHandlingOutcome duplicate = controller.record(
                completion,
                0,
                0);
    QVERIFY(!duplicate.resultRecorded);
    QCOMPARE(controller.totalCount(), 1);

    const DetectionResultHandlingOutcome foreign = controller.record(
                testCompletion(2, AlgorithmVerdict::Ng),
                0,
                0);
    QVERIFY(!foreign.resultRecorded);
    QCOMPARE(controller.totalCount(), 1);
    QCOMPARE(controller.ngCount(), 0);
}

void DetectionCompletionTest::runtimeControllerStartsNewRunWithoutResettingStatistics()
{
    int runNumber = 0;
    InspectionRuntimeController controller([&runNumber]() {
        return QStringLiteral("run-%1").arg(++runNumber);
    });

    controller.beginStart();
    controller.markRunning();
    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    controller.record(
                controller.complete(
                    cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                    result),
                0,
                0);
    controller.requestStop();
    controller.finishStop();

    QCOMPARE(controller.beginStart(), QStringLiteral("run-2"));
    QCOMPARE(controller.completedProductCount(), quint64(0));
    QCOMPARE(controller.totalCount(), 1);
    QCOMPARE(controller.ngCount(), 0);
    const DetectionCompletion restarted = controller.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(2)),
                result);
    QCOMPARE(restarted.frame->productKey.runId, QStringLiteral("run-2"));
    QCOMPARE(restarted.frame->productKey.sequence, quint64(1));
}

void DetectionCompletionTest::runtimeControllerPreservesSeparateResetScopes()
{
    InspectionRuntimeController controller;
    controller.beginStart();
    DetectionResult ngResult;
    ngResult.verdict = AlgorithmVerdict::Ng;
    ngResult.status = DetectionStatus::Completed;
    controller.record(
                controller.complete(
                    cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                    ngResult),
                0,
                3);
    DetectionResult okResult;
    okResult.verdict = AlgorithmVerdict::Ok;
    okResult.status = DetectionStatus::Completed;
    controller.record(
                controller.complete(
                    cv::Mat(1, 1, CV_8UC1, cv::Scalar(2)),
                    okResult),
                0,
                0);

    controller.resetNgCount();
    QCOMPARE(controller.totalCount(), 2);
    QCOMPARE(controller.ngCount(), 0);
    QCOMPARE(controller.pendingDelayedNgCount(), 1);

    controller.resetStatistics();
    QCOMPARE(controller.totalCount(), 0);
    QCOMPARE(controller.ngCount(), 0);
    QCOMPARE(controller.pendingDelayedNgCount(), 1);

    controller.clearPendingDelayedNgRequests();
    QCOMPARE(controller.pendingDelayedNgCount(), 0);
}

void DetectionCompletionTest::runtimeControllerStopsAdmissionBeforeDrainingAcceptedFrames()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("drain-run");
    });
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(5));

    QVERIFY(!controller.acceptFrame(image));
    QCOMPARE(controller.beginStart(), QStringLiteral("drain-run"));
    const std::shared_ptr<const FrameData> startingFrame =
            controller.acceptFrame(image);
    QVERIFY(startingFrame);
    QVERIFY(controller.markRunning());
    const std::shared_ptr<const FrameData> runningFrame =
            controller.acceptFrame(image);
    QVERIFY(runningFrame);
    QCOMPARE(controller.acceptedProductCount(), quint64(2));
    QCOMPARE(controller.completedProductCount(), quint64(0));

    QVERIFY(controller.requestStop());
    QVERIFY(!controller.acceptFrame(image));

    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    QVERIFY(controller.complete(startingFrame, result).isValid());
    QVERIFY(controller.complete(runningFrame, result).isValid());
    QCOMPARE(controller.acceptedProductCount(), quint64(2));
    QCOMPARE(controller.completedProductCount(), quint64(2));

    controller.finishStop();
    QVERIFY(!controller.complete(runningFrame, result).isValid());
}

void DetectionCompletionTest::startAccessAcceptsIdleOpenCamera()
{
    InspectionStartAccessInput input;
    input.cameraOpen = true;

    const InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateAccess(input);

    QVERIFY(result.isAccepted());
    QVERIFY(result.details.isEmpty());
}

void DetectionCompletionTest::startAccessPreservesGuardOrder()
{
    InspectionStartAccessInput input;
    input.templateOperationActive = true;
    input.runtimeBusy = true;
    input.cameraOpen = false;
    input.dirtySettings = true;
    input.plcTriggerEnabled = true;
    input.plcConnected = false;

    InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateAccess(input);
    QVERIFY(result.issue
            == InspectionStartIssue::TemplateOperationActive);

    input.templateOperationActive = false;
    result = InspectionStartPreflight::evaluateAccess(input);
    QVERIFY(result.issue == InspectionStartIssue::RuntimeBusy);

    input.runtimeBusy = false;
    result = InspectionStartPreflight::evaluateAccess(input);
    QVERIFY(result.issue == InspectionStartIssue::CameraClosed);
}

void DetectionCompletionTest::dirtySettingsPrecedePlcConnectivity()
{
    InspectionStartAccessInput input;
    input.cameraOpen = true;
    input.dirtySettings = true;
    input.plcTriggerEnabled = true;
    input.plcConnected = false;

    InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateAccess(input);
    QVERIFY(result.issue
            == InspectionStartIssue::DirtySettingsConfirmationRequired);

    input.dirtySettings = false;
    result = InspectionStartPreflight::evaluateAccess(input);
    QVERIFY(result.issue == InspectionStartIssue::PlcDisconnected);
}

void DetectionCompletionTest::tissueStartNeedsNoTemplateAssets()
{
    InspectionStartResourceInput input;
    input.modeKind = InspectionStartModeKind::Tissue;

    const InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateResources(input);

    QVERIFY(result.isAccepted());
}

void DetectionCompletionTest::singleTemplateStartReportsOrderedMissingAssets()
{
    InspectionStartResourceInput input;
    input.modeKind = InspectionStartModeKind::SingleTemplate;

    const InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateResources(input);

    QVERIFY(result.issue
            == InspectionStartIssue::ProductTemplateIncomplete);
    QCOMPARE(result.details.size(), 3);
    QCOMPARE(result.details.at(0),
             QString::fromWCharArray(
                 L"\u672a\u9009\u62e9\u4ea7\u54c1\u6a21\u677f\u6587\u4ef6\u5939"));
    QCOMPARE(result.details.at(1),
             QString::fromWCharArray(
                 L"\u5b9a\u4f4d\u6a21\u677f\u56fe\u7247 tracking_template.bmp "
                 L"\u7f3a\u5931\u6216\u8bfb\u53d6\u5931\u8d25"));
    QCOMPARE(result.details.at(2),
             QString::fromWCharArray(
                 L"\u55b7\u7801\u68c0\u6d4b\u533a\u57df "
                 L"calibrate_config.yaml/date_poly "
                 L"\u7f3a\u5931\u6216\u8bfb\u53d6\u5931\u8d25"));
}

void DetectionCompletionTest::wordStartRequiresProfilesAndCompleteCharacters()
{
    InspectionStartResourceInput input;
    input.modeKind = InspectionStartModeKind::WordProfiles;

    InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateResources(input);
    QVERIFY(result.issue == InspectionStartIssue::WordProfilesMissing);

    InspectionStartProfileReadiness incomplete;
    incomplete.displayName = QStringLiteral("8135");
    incomplete.targetTextReady = true;
    input.profiles.push_back(incomplete);
    result = InspectionStartPreflight::evaluateResources(input);
    QVERIFY(result.issue
            == InspectionStartIssue::WordProfilesIncomplete);
    QCOMPARE(result.details,
             QStringList() << QStringLiteral("8135"));
}

void DetectionCompletionTest::barcodeStartAggregatesDecoderAndProfileErrors()
{
    InspectionStartResourceInput input;
    input.modeKind = InspectionStartModeKind::BarcodeWordProfiles;
    input.barcodeDecoderReady = false;
    input.barcodeDecoderError = QStringLiteral("DLL missing");

    InspectionStartProfileReadiness profile;
    profile.displayName = QStringLiteral("566");
    input.profiles.push_back(profile);

    const InspectionStartPreflightResult result =
            InspectionStartPreflight::evaluateResources(input);

    QVERIFY(result.issue
            == InspectionStartIssue::BarcodeResourcesInvalid);
    QCOMPARE(result.details.size(), 2);
    QCOMPARE(result.details.at(0),
             QString::fromWCharArray(
                 L"\u8bfb\u7801\u7ec4\u4ef6\u4e0d\u53ef\u7528\uff1aDLL missing"));
    QVERIFY(result.details.at(1).startsWith(
                QString::fromWCharArray(
                    L"\u6a21\u677f\u201c566\u201d\uff1a")));
    QVERIFY(result.details.at(1).contains(
                QString::fromWCharArray(
                    L"\u5b9a\u4f4d\u6a21\u677f tracking_template.bmp "
                    L"\u7f3a\u5931\u6216\u65e0\u6cd5\u8bfb\u53d6")));
    QVERIFY(result.details.at(1).contains(
                QString::fromWCharArray(
                    L"calibrate_config.yaml "
                    L"\u7f3a\u5931\u6216\u65e0\u6cd5\u8bfb\u53d6")));
    QVERIFY(result.details.at(1).contains(
                QString::fromWCharArray(
                    L"\u76ee\u6807\u5b57\u7b26\u5c1a\u672a\u8bbe\u7f6e")));
    QVERIFY(result.details.at(1).contains(
                QString::fromWCharArray(
                    L"\u5b57\u7b26\u6a21\u677f\u7f3a\u5931\u6216"
                    L"\u7d22\u5f15\u914d\u7f6e\u65e0\u6548")));
    QVERIFY(!result.details.at(1).contains(
                QStringLiteral("barcode_poly")));
}

void DetectionCompletionTest::validWordAndBarcodeStartsAreAccepted()
{
    InspectionStartProfileReadiness profile;
    profile.displayName = QStringLiteral("566");
    profile.trackingTemplateReady = true;
    profile.calibrationReady = true;
    profile.barcodeRegionReady = true;
    profile.dateRegionReady = true;
    profile.targetTextReady = true;
    profile.characterTemplatesReady = true;

    InspectionStartResourceInput input;
    input.modeKind = InspectionStartModeKind::WordProfiles;
    input.profiles.push_back(profile);
    QVERIFY(InspectionStartPreflight::evaluateResources(input).isAccepted());

    input.modeKind = InspectionStartModeKind::BarcodeWordProfiles;
    input.barcodeDecoderReady = true;
    QVERIFY(InspectionStartPreflight::evaluateResources(input).isAccepted());
}

void DetectionCompletionTest::runPlanSelectsSoftwareSingleTemplate()
{
    const InspectionRunPlan plan =
            InspectionRunConfiguration::createPlan(
                InspectionStartModeKind::SingleTemplate,
                false);

    QVERIFY(plan.acquisitionKind
            == InspectionAcquisitionKind::SoftwareTrigger);
    QVERIFY(plan.trackingKind
            == InspectionTrackingKind::SingleTemplate);
    QVERIFY(!plan.barcodeWordHardTriggerMode);
}

void DetectionCompletionTest::runPlanSelectsHardwareBarcodeProfiles()
{
    const InspectionRunPlan plan =
            InspectionRunConfiguration::createPlan(
                InspectionStartModeKind::BarcodeWordProfiles,
                true);

    QVERIFY(plan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger);
    QVERIFY(plan.trackingKind
            == InspectionTrackingKind::WordProfiles);
    QVERIFY(plan.barcodeWordHardTriggerMode);
}

void DetectionCompletionTest::runPlanSelectsWholeFrameAndWordTracking()
{
    InspectionRunPlan plan =
            InspectionRunConfiguration::createPlan(
                InspectionStartModeKind::Tissue,
                false);
    QVERIFY(plan.trackingKind == InspectionTrackingKind::WholeFrame);

    plan = InspectionRunConfiguration::createPlan(
                InspectionStartModeKind::WordProfiles,
                true);
    QVERIFY(plan.trackingKind == InspectionTrackingKind::WordProfiles);
    QVERIFY(!plan.barcodeWordHardTriggerMode);
}

void DetectionCompletionTest::runtimeSettingsRetainValidValues()
{
    InspectionRuntimeSettingsInput input;
    input.imageThresholdText = QStringLiteral(" 70 ");
    input.tissueThresholdText = QStringLiteral(" 6.250 ");
    input.rotationIndex = 2;
    input.colorChannelIndex = 3;

    const InspectionRuntimeSettingsResult result =
            InspectionRunConfiguration::parseSettings(input);

    QVERIFY(result.isAccepted());
    QCOMPARE(result.settings.imageThreshold, 70);
    QCOMPARE(result.settings.tissueThreshold, 6.25);
    QCOMPARE(result.settings.rotationCode, 2);
    QCOMPARE(result.settings.colorChannelCode, 3);
}

void DetectionCompletionTest::runtimeSettingsUseLegacyDefaultsForUnknownIndexes()
{
    InspectionRuntimeSettingsInput input;
    input.imageThresholdText = QStringLiteral("0");
    input.tissueThresholdText = QStringLiteral("1");
    input.rotationIndex = 9;
    input.colorChannelIndex = -1;

    const InspectionRuntimeSettingsResult result =
            InspectionRunConfiguration::parseSettings(input);

    QVERIFY(result.isAccepted());
    QCOMPARE(result.settings.rotationCode, 0);
    QCOMPARE(result.settings.colorChannelCode, 0);
}

void DetectionCompletionTest::runtimeSettingsRejectInvalidImageThreshold()
{
    InspectionRuntimeSettingsInput input;
    input.tissueThresholdText = QStringLiteral("6");

    input.imageThresholdText = QStringLiteral("abc");
    InspectionRuntimeSettingsResult result =
            InspectionRunConfiguration::parseSettings(input);
    QVERIFY(result.issue
            == InspectionRuntimeSettingsIssue::InvalidImageThreshold);

    input.imageThresholdText = QStringLiteral("101");
    result = InspectionRunConfiguration::parseSettings(input);
    QVERIFY(result.issue
            == InspectionRuntimeSettingsIssue::InvalidImageThreshold);
}

void DetectionCompletionTest::runtimeSettingsRejectInvalidTissueThreshold()
{
    InspectionRuntimeSettingsInput input;
    input.imageThresholdText = QStringLiteral("70");

    input.tissueThresholdText = QStringLiteral("0");
    InspectionRuntimeSettingsResult result =
            InspectionRunConfiguration::parseSettings(input);
    QVERIFY(result.issue
            == InspectionRuntimeSettingsIssue::InvalidTissueThreshold);

    input.tissueThresholdText = QStringLiteral("invalid");
    result = InspectionRunConfiguration::parseSettings(input);
    QVERIFY(result.issue
            == InspectionRuntimeSettingsIssue::InvalidTissueThreshold);
}

void DetectionCompletionTest::shadowComparisonAcceptsEquivalentResults()
{
    const DetectionResult primary = shadowSampleResult();
    const DetectionResult shadow = primary;

    const DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow);

    QVERIFY(comparison.isEquivalent());
    QVERIFY(comparison.differences.isEmpty());
}

void DetectionCompletionTest::shadowComparisonIgnoresTimingAndDiagnosticByDefault()
{
    const DetectionResult primary = shadowSampleResult();
    DetectionResult shadow = primary;
    shadow.elapsedMs = 999.0;
    shadow.diagnostic = QStringLiteral("shadow diagnostic");

    const DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow);

    QVERIFY(comparison.isEquivalent());
}

void DetectionCompletionTest::shadowComparisonReportsBusinessResultDifferences()
{
    const DetectionResult primary = shadowSampleResult();
    DetectionResult shadow = primary;
    shadow.modeId = QStringLiteral("barcode_word");
    shadow.verdict = AlgorithmVerdict::Ng;
    shadow.status = DetectionStatus::SystemFault;
    shadow.recognizedText = QStringLiteral("8136");

    const DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow);

    QVERIFY(!comparison.isEquivalent());
    QVERIFY(comparison.differences.contains(
                QStringLiteral("result.modeId")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("result.verdict")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("result.status")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("result.recognizedText")));
    QVERIFY(!comparison.differences.contains(
                QStringLiteral("result.elapsedMs")));
}

void DetectionCompletionTest::shadowComparisonAppliesGeometryTolerance()
{
    const DetectionResult primary = shadowSampleResult();
    DetectionResult shadow = primary;
    shadow.overlay.polygons[0].points[0] += cv::Point(1, -1);
    shadow.overlay.polygons[0].score += 0.005;

    DetectionShadowComparisonOptions options;
    options.coordinateTolerance = 1;
    options.scoreTolerance = 0.01;
    DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow, options);
    QVERIFY(comparison.isEquivalent());

    options.coordinateTolerance = 0;
    options.scoreTolerance = 0.001;
    comparison = DetectionShadowComparator::compare(
                primary,
                shadow,
                options);
    QVERIFY(comparison.differences.contains(
                QStringLiteral("overlay[0].points[0]")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("overlay[0].score")));
}

void DetectionCompletionTest::shadowComparisonReportsOrderedOverlayDifferences()
{
    const DetectionResult primary = shadowSampleResult();
    DetectionResult shadow = primary;
    shadow.overlay.polygons[0].role = QStringLiteral("tracking_roi");
    shadow.overlay.polygons[0].points.pop_back();

    DetectionOverlayPolygon extra;
    extra.role = QStringLiteral("extra");
    shadow.overlay.polygons.push_back(extra);

    const DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow);

    QVERIFY(comparison.differences.contains(
                QStringLiteral("overlay.count")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("overlay[0].role")));
    QVERIFY(comparison.differences.contains(
                QStringLiteral("overlay[0].points.count")));
}

void DetectionCompletionTest::shadowComparisonCanIncludeDiagnostic()
{
    const DetectionResult primary = shadowSampleResult();
    DetectionResult shadow = primary;
    shadow.diagnostic = QStringLiteral("shadow diagnostic");

    DetectionShadowComparisonOptions options;
    options.compareDiagnostic = true;
    const DetectionShadowComparison comparison =
            DetectionShadowComparator::compare(primary, shadow, options);

    QVERIFY(comparison.differences.contains(
                QStringLiteral("result.diagnostic")));
}

void DetectionCompletionTest::frameQueueRequiresValidFramesAndPositiveCapacity()
{
    FrameQueue queue(0);
    QCOMPARE(static_cast<int>(queue.capacity()), 1);
    QVERIFY(!queue.submit(std::shared_ptr<const FrameData>()));

    std::shared_ptr<FrameData> invalidFrame(new FrameData);
    QVERIFY(!queue.submit(invalidFrame));

    const std::shared_ptr<const FrameData> validFrame = workerTestFrame(1);
    QVERIFY(queue.submit(validFrame));
    QCOMPARE(static_cast<int>(queue.size()), 1);

    std::shared_ptr<const FrameData> takenFrame;
    QVERIFY(queue.waitAndTake(&takenFrame));
    QCOMPARE(takenFrame->productKey.sequence, quint64(1));
    QCOMPARE(static_cast<int>(queue.size()), 0);
    queue.cancel();
}

void DetectionCompletionTest::frameQueuePreservesSubmissionOrder()
{
    FrameQueue queue(3);
    QVERIFY(queue.submit(workerTestFrame(1)));
    QVERIFY(queue.submit(workerTestFrame(2)));
    QVERIFY(queue.submit(workerTestFrame(3)));

    for (quint64 sequence = 1; sequence <= 3; ++sequence) {
        std::shared_ptr<const FrameData> frame;
        QVERIFY(queue.waitAndTake(&frame));
        QCOMPARE(frame->productKey.sequence, sequence);
    }
    queue.cancel();
}

void DetectionCompletionTest::frameQueueWaitsForSpaceWithoutDroppingFrame()
{
    FrameQueue queue(1);
    QVERIFY(queue.submit(workerTestFrame(1)));
    const std::shared_ptr<const FrameData> secondFrame = workerTestFrame(2);

    std::future<bool> blockedSubmit = std::async(
                std::launch::async,
                [&queue, secondFrame]() {
        return queue.submit(secondFrame);
    });
    QVERIFY(blockedSubmit.wait_for(std::chrono::milliseconds(20))
            == std::future_status::timeout);

    std::shared_ptr<const FrameData> frame;
    QVERIFY(queue.waitAndTake(&frame));
    QCOMPARE(frame->productKey.sequence, quint64(1));
    QVERIFY(blockedSubmit.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    QVERIFY(blockedSubmit.get());

    QVERIFY(queue.waitAndTake(&frame));
    QCOMPARE(frame->productKey.sequence, quint64(2));
    queue.cancel();
}

void DetectionCompletionTest::frameQueueCancellationReleasesFramesAndSubmitter()
{
    FrameQueue queue(1);
    std::shared_ptr<const FrameData> firstFrame = workerTestFrame(1);
    std::weak_ptr<const FrameData> firstWeak = firstFrame;
    QVERIFY(queue.submit(firstFrame));
    firstFrame.reset();

    const std::shared_ptr<const FrameData> secondFrame = workerTestFrame(2);
    std::future<bool> blockedSubmit = std::async(
                std::launch::async,
                [&queue, secondFrame]() {
        return queue.submit(secondFrame);
    });
    QVERIFY(blockedSubmit.wait_for(std::chrono::milliseconds(20))
            == std::future_status::timeout);

    QCOMPARE(static_cast<int>(queue.cancel()), 1);
    QVERIFY(blockedSubmit.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    QVERIFY(!blockedSubmit.get());
    QVERIFY(firstWeak.expired());
    QVERIFY(queue.isCancelled());
    QVERIFY(queue.reopen());
    QVERIFY(!queue.isCancelled());
    queue.cancel();
}

void DetectionCompletionTest::frameQueuePreservesPositionedDetectionWorkItem()
{
    FrameQueue queue(1);
    const std::shared_ptr<const FrameData> frame = workerTestFrame(9);
    DetectionPose pose;
    pose.valid = true;
    pose.wordTemplateProfileIndex = 3;
    pose.angleDeg = 17.5f;
    pose.datePoly = {
        cv::Point(1, 2),
        cv::Point(5, 2),
        cv::Point(5, 6),
        cv::Point(1, 6)
    };
    const DetectionWorkItem submitted =
            makeDetectionWorkItem(frame, pose);

    QVERIFY(queue.submit(submitted));
    DetectionWorkItem received;
    QVERIFY(queue.waitAndTake(&received));
    QVERIFY(received.isValid());
    QVERIFY(received.hasPose);
    QCOMPARE(received.frame.get(), frame.get());
    QCOMPARE(received.pose.wordTemplateProfileIndex, 3);
    QCOMPARE(received.pose.angleDeg, 17.5f);
    QCOMPARE(static_cast<int>(received.pose.datePoly.size()), 4);
    QCOMPARE(received.pose.datePoly[2].x, 5);
    QCOMPARE(received.pose.datePoly[2].y, 6);
    QCOMPARE(static_cast<int>(queue.cancel()), 0);
}

void DetectionCompletionTest::detectionWorkerProcessesFramesSeriallyInOrder()
{
    std::atomic<int> activeCount(0);
    std::atomic<int> maximumActiveCount(0);
    std::atomic<int> failureCount(0);
    std::mutex resultMutex;
    std::condition_variable resultAvailable;
    std::vector<quint64> resultSequences;

    DetectionWorker worker(
                2,
                [&activeCount, &maximumActiveCount](
                    const std::shared_ptr<const FrameData> &frame) {
        const int active = activeCount.fetch_add(1) + 1;
        int maximum = maximumActiveCount.load();
        while (active > maximum
               && !maximumActiveCount.compare_exchange_weak(
                   maximum,
                   active)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        activeCount.fetch_sub(1);

        DetectionResult result;
        result.modeId = QStringLiteral("serial-test");
        result.verdict = AlgorithmVerdict::Ok;
        result.status = DetectionStatus::Completed;
        result.recognizedText = QString::number(
                    frame->productKey.sequence);
        return result;
    },
    [&resultMutex, &resultAvailable, &resultSequences](
        const DetectionCompletion &completion) {
        std::lock_guard<std::mutex> lock(resultMutex);
        resultSequences.push_back(
                    completion.frame->productKey.sequence);
        resultAvailable.notify_all();
    },
    [&failureCount](const QString &) {
        failureCount.fetch_add(1);
    });

    QVERIFY(worker.start());
    for (quint64 sequence = 1; sequence <= 4; ++sequence) {
        QVERIFY(worker.submit(workerTestFrame(sequence)));
    }

    {
        std::unique_lock<std::mutex> lock(resultMutex);
        QVERIFY(resultAvailable.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&resultSequences]() {
            return resultSequences.size() == 4;
        }));
    }
    worker.requestStop();
    worker.wait();

    QCOMPARE(maximumActiveCount.load(), 1);
    QCOMPARE(failureCount.load(), 0);
    QCOMPARE(worker.processedFrameCount(), quint64(4));
    QCOMPARE(worker.cancelledFrameCount(), quint64(0));
    QCOMPARE(static_cast<int>(resultSequences.size()), 4);
    for (quint64 sequence = 1; sequence <= 4; ++sequence) {
        QCOMPARE(resultSequences[static_cast<std::size_t>(sequence - 1)],
                 sequence);
    }
}

void DetectionCompletionTest::detectionWorkerReceivesPositionedDetectionWorkItem()
{
    std::promise<DetectionCompletion> completionPromise;
    std::future<DetectionCompletion> completionFuture =
            completionPromise.get_future();
    const DetectionWorker::WorkItemDetector detector =
            [](const DetectionWorkItem &item) {
        DetectionResult result;
        result.modeId = QStringLiteral("ocr_detection");
        result.status = DetectionStatus::Completed;
        result.verdict = AlgorithmVerdict::Ok;
        result.recognizedText = QStringLiteral("%1:%2")
                .arg(item.pose.wordTemplateProfileIndex)
                .arg(item.pose.angleDeg, 0, 'f', 1);
        return result;
    };
    DetectionWorker worker(
                1,
                detector,
                [&completionPromise](
                    const DetectionCompletion &completion) {
        completionPromise.set_value(completion);
    });
    QVERIFY(worker.start());

    DetectionPose pose;
    pose.valid = true;
    pose.wordTemplateProfileIndex = 4;
    pose.angleDeg = 22.5f;
    const std::shared_ptr<const FrameData> frame = workerTestFrame(10);
    QVERIFY(worker.submit(makeDetectionWorkItem(frame, pose)));
    QVERIFY(completionFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const DetectionCompletion completion = completionFuture.get();
    worker.requestStop();
    worker.wait();

    QVERIFY(completion.isValid());
    QCOMPARE(completion.frame.get(), frame.get());
    QCOMPARE(completion.result.modeId,
             QStringLiteral("ocr_detection"));
    QCOMPARE(completion.result.recognizedText,
             QStringLiteral("4:22.5"));
    QCOMPARE(worker.processedFrameCount(), quint64(1));
    QCOMPARE(worker.cancelledFrameCount(), quint64(0));
}

void DetectionCompletionTest::runtimeControllerAcceptedFramesFlowThroughDetectionWorker()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("software-run");
    });
    QCOMPARE(controller.beginStart(), QStringLiteral("software-run"));
    QVERIFY(controller.markRunning());

    std::mutex outcomeMutex;
    std::condition_variable outcomeAvailable;
    bool resultRecorded = false;
    ProductKey recordedKey;

    DetectionWorker worker(
                1,
                [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.modeId = QStringLiteral("tissue_detection");
        result.verdict = AlgorithmVerdict::Ok;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [&controller,
     &outcomeMutex,
     &outcomeAvailable,
     &resultRecorded,
     &recordedKey](const DetectionCompletion &workerCompletion) {
        const DetectionCompletion acceptedCompletion =
                controller.complete(
                    workerCompletion.frame,
                    workerCompletion.result);
        const DetectionResultHandlingOutcome outcome =
                controller.record(acceptedCompletion, 0, 0);
        {
            std::lock_guard<std::mutex> lock(outcomeMutex);
            resultRecorded = outcome.resultRecorded;
            if (acceptedCompletion.frame) {
                recordedKey = acceptedCompletion.frame->productKey;
            }
        }
        outcomeAvailable.notify_all();
    });

    QVERIFY(worker.start());
    const std::shared_ptr<const FrameData> frame =
            controller.acceptFrame(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(4)));
    QVERIFY(frame);
    QVERIFY(worker.submit(frame));
    {
        std::unique_lock<std::mutex> lock(outcomeMutex);
        QVERIFY(outcomeAvailable.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&resultRecorded]() {
            return resultRecorded;
        }));
    }
    worker.requestStop();
    worker.wait();

    QCOMPARE(recordedKey.runId, QStringLiteral("software-run"));
    QCOMPARE(recordedKey.sequence, quint64(1));
    QCOMPARE(controller.acceptedProductCount(), quint64(1));
    QCOMPARE(controller.completedProductCount(), quint64(1));
    QCOMPARE(controller.totalCount(), 1);
    QCOMPARE(controller.ngCount(), 0);
}

void DetectionCompletionTest::detectionWorkerRejectsSubmissionOutsideRun()
{
    DetectionWorker worker(
                1,
                [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [](const DetectionCompletion &) {
    });

    QVERIFY(!worker.submit(workerTestFrame(1)));
    QVERIFY(worker.start());
    QVERIFY(!worker.start());
    worker.requestStop();
    worker.wait();
    QVERIFY(!worker.isRunning());
    QVERIFY(!worker.submit(workerTestFrame(2)));
}

void DetectionCompletionTest::detectionWorkerCancellationSuppressesPendingResults()
{
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool detectorEntered = false;
    bool releaseDetector = false;
    std::atomic<int> completionCount(0);

    DetectionWorker worker(
                3,
                [&gateMutex, &gateChanged, &detectorEntered, &releaseDetector](
                    const std::shared_ptr<const FrameData> &) {
        std::unique_lock<std::mutex> lock(gateMutex);
        detectorEntered = true;
        gateChanged.notify_all();
        gateChanged.wait(lock, [&releaseDetector]() {
            return releaseDetector;
        });
        DetectionResult result;
        result.verdict = AlgorithmVerdict::Ok;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [&completionCount](const DetectionCompletion &) {
        completionCount.fetch_add(1);
    });

    QVERIFY(worker.start());
    QVERIFY(worker.submit(workerTestFrame(1)));
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        QVERIFY(gateChanged.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&detectorEntered]() {
            return detectorEntered;
        }));
    }
    QVERIFY(worker.submit(workerTestFrame(2)));
    QVERIFY(worker.submit(workerTestFrame(3)));

    worker.requestStop();
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseDetector = true;
    }
    gateChanged.notify_all();
    worker.wait();

    QCOMPARE(completionCount.load(), 0);
    QCOMPARE(worker.processedFrameCount(), quint64(0));
    QCOMPARE(worker.cancelledFrameCount(), quint64(3));
    QCOMPARE(static_cast<int>(worker.queuedFrameCount()), 0);
}

void DetectionCompletionTest::detectionWorkerCanRestartAfterWait()
{
    std::mutex resultMutex;
    std::condition_variable resultAvailable;
    std::vector<quint64> resultSequences;

    DetectionWorker worker(
                1,
                [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.verdict = AlgorithmVerdict::Ok;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [&resultMutex, &resultAvailable, &resultSequences](
        const DetectionCompletion &completion) {
        std::lock_guard<std::mutex> lock(resultMutex);
        resultSequences.push_back(
                    completion.frame->productKey.sequence);
        resultAvailable.notify_all();
    });

    for (quint64 sequence = 1; sequence <= 2; ++sequence) {
        QVERIFY(worker.start());
        QVERIFY(worker.submit(workerTestFrame(sequence)));
        {
            std::unique_lock<std::mutex> lock(resultMutex);
            QVERIFY(resultAvailable.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&resultSequences, sequence]() {
                return resultSequences.size()
                        == static_cast<std::size_t>(sequence);
            }));
        }
        worker.requestStop();
        worker.wait();
        QVERIFY(!worker.isRunning());
    }

    QCOMPARE(worker.processedFrameCount(), quint64(2));
    QCOMPARE(worker.cancelledFrameCount(), quint64(0));
    QCOMPARE(resultSequences[0], quint64(1));
    QCOMPARE(resultSequences[1], quint64(2));
}

void DetectionCompletionTest::uiCompletionMailboxSerializesWholeProductWork()
{
    UiCompletionMailbox mailbox;
    QVERIFY(mailbox.reopen());

    std::vector<int> presentedProducts;
    bool firstObservedProcessing = false;
    QVERIFY(mailbox.submit([&]() {
        firstObservedProcessing = mailbox.isProcessing();
        presentedProducts.push_back(1);
    }));
    QVERIFY(mailbox.hasPendingWork());

    std::future<bool> secondSubmission = std::async(
                std::launch::async,
                [&]() {
        return mailbox.submit([&]() {
            presentedProducts.push_back(2);
        });
    });
    QVERIFY(secondSubmission.wait_for(
                std::chrono::milliseconds(30))
            == std::future_status::timeout);

    QVERIFY(mailbox.processOne());
    QVERIFY(firstObservedProcessing);
    QCOMPARE(secondSubmission.get(), true);
    QVERIFY(mailbox.hasPendingWork());
    QVERIFY(mailbox.processOne());
    QVERIFY(!mailbox.hasPendingWork());
    QCOMPARE(static_cast<int>(presentedProducts.size()), 2);
    QCOMPARE(presentedProducts.at(0), 1);
    QCOMPARE(presentedProducts.at(1), 2);
}

void DetectionCompletionTest::uiCompletionMailboxCancellationReleasesProducer()
{
    UiCompletionMailbox mailbox;
    QVERIFY(mailbox.reopen());
    QVERIFY(mailbox.submit([]() {}));

    std::future<bool> blockedSubmission = std::async(
                std::launch::async,
                [&]() {
        return mailbox.submit([]() {});
    });
    QVERIFY(blockedSubmission.wait_for(
                std::chrono::milliseconds(30))
            == std::future_status::timeout);

    mailbox.cancel();
    QCOMPARE(blockedSubmission.get(), false);
    QVERIFY(!mailbox.hasPendingWork());
    QVERIFY(!mailbox.processOne());

    int presentedCount = 0;
    QVERIFY(mailbox.reopen());
    QVERIFY(mailbox.submit([&presentedCount]() {
        ++presentedCount;
    }));
    QVERIFY(mailbox.processOne());
    QCOMPARE(presentedCount, 1);
}

void DetectionCompletionTest::roiPaddingIsClampedToImageBounds()
{
    const std::vector<cv::Point> polygon = {
        cv::Point(2, 3),
        cv::Point(20, 3),
        cv::Point(20, 12),
        cv::Point(2, 12)
    };
    std::vector<cv::Point> clampedPolygon;
    const cv::Rect roi =
            DetectionRoiGeometry::polygonRoiWithClampedPadding(
                polygon,
                20,
                cv::Size(100, 80),
                &clampedPolygon);

    QCOMPARE(roi.x, 0);
    QCOMPARE(roi.y, 0);
    QCOMPARE(roi.width, 41);
    QCOMPARE(roi.height, 33);
    QCOMPARE(static_cast<int>(clampedPolygon.size()), 4);
}

void DetectionCompletionTest::outsidePolygonIsClampedToNearestImageEdge()
{
    const std::vector<cv::Point> polygon = {
        cv::Point(110, 20),
        cv::Point(120, 20),
        cv::Point(120, 30),
        cv::Point(110, 30)
    };
    std::vector<cv::Point> clampedPolygon;
    const cv::Rect roi =
            DetectionRoiGeometry::polygonRoiWithClampedPadding(
                polygon,
                20,
                cv::Size(100, 80),
                &clampedPolygon);

    QCOMPARE(roi.x, 79);
    QCOMPARE(roi.y, 0);
    QCOMPARE(roi.width, 21);
    QCOMPARE(roi.height, 51);
    QCOMPARE(static_cast<int>(clampedPolygon.size()), 4);
    for (const cv::Point &point : clampedPolygon) {
        QCOMPARE(point.x, 99);
        QVERIFY(point.y >= 0 && point.y < 80);
    }
}

void DetectionCompletionTest::orientedDateRoiClampsPaddingAtImageEdge()
{
    const cv::Mat source(80, 100, CV_8UC3, cv::Scalar(20, 40, 60));
    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = cv::Point2f(5.0f, 7.0f);
    pose.datePoly = {
        cv::Point(-5, 2),
        cv::Point(10, 2),
        cv::Point(10, 12),
        cv::Point(-5, 12)
    };

    const OrientedDateRoi oriented =
            DetectionRoiGeometry::prepareOrientedDateRoi(
                source,
                pose,
                20);

    QVERIFY(oriented.valid);
    QCOMPARE(oriented.roi.x, 0);
    QCOMPARE(oriented.roi.y, 0);
    QVERIFY(oriented.roi.x + oriented.roi.width
            <= source.cols);
    QVERIFY(oriented.roi.y + oriented.roi.height
            <= source.rows);
    QCOMPARE(oriented.croppedImage.cols, oriented.roi.width);
    QCOMPARE(oriented.croppedImage.rows, oriented.roi.height);
    QCOMPARE(oriented.croppedImage.type(), CV_8UC3);
}

void DetectionCompletionTest::saveTaskRequiresProductAndAllItems()
{
    ImageSaveTask task;
    QVERIFY(!task.isValid());

    task.productKey = testProductKey(1);
    QVERIFY(!task.isValid());

    ImageSaveItem invalidItem;
    invalidItem.filePath = QStringLiteral("invalid.png");
    invalidItem.format = QByteArrayLiteral("PNG");
    task.items.push_back(invalidItem);
    QVERIFY(!task.isValid());

    task.items[0].image = QImage(1, 1, QImage::Format_RGB32);
    QVERIFY(task.isValid());
}

void DetectionCompletionTest::saveServicePreservesTaskAndItemOrder()
{
    QStringList writtenPaths;
    std::mutex pathsMutex;
    ImageSaveService service(
                8,
                [&writtenPaths, &pathsMutex](
                    const ImageSaveItem &item,
                    QString *) {
        std::lock_guard<std::mutex> lock(pathsMutex);
        writtenPaths.append(item.filePath);
        return true;
    },
    1);

    QVERIFY(service.submit(testSaveTask(
                               1,
                               QStringList()
                               << QStringLiteral("a-annotated.png")
                               << QStringLiteral("a-raw.png")))
            .isAccepted());
    QVERIFY(service.submit(testSaveTask(
                               2,
                               QStringList()
                               << QStringLiteral("b-annotated.png")))
            .isAccepted());

    QTRY_VERIFY(service.outstandingTaskCount() == 0);
    {
        std::lock_guard<std::mutex> lock(pathsMutex);
        QCOMPARE(
                    writtenPaths,
                    QStringList()
                    << QStringLiteral("a-annotated.png")
                    << QStringLiteral("a-raw.png")
                    << QStringLiteral("b-annotated.png"));
    }
}

void DetectionCompletionTest::fullQueueWaitsForSpaceWithoutDroppingTask()
{
    std::mutex gateMutex;
    std::condition_variable gateCondition;
    bool firstWriteStarted = false;
    bool releaseFirstWrite = false;
    int writeCount = 0;
    ImageSaveService service(
                2,
                [&](const ImageSaveItem &, QString *) {
        std::unique_lock<std::mutex> lock(gateMutex);
        ++writeCount;
        if (writeCount == 1) {
            firstWriteStarted = true;
            gateCondition.notify_all();
            gateCondition.wait(lock, [&releaseFirstWrite]() {
                return releaseFirstWrite;
            });
        }
        return true;
    },
    1);

    const bool firstAccepted = service.submit(testSaveTask(
                                                   1,
                                                   QStringList()
                                                   << QStringLiteral("1.png")))
            .isAccepted();
    bool firstStarted = false;
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        firstStarted = gateCondition.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&firstWriteStarted]() {
            return firstWriteStarted;
        });
    }

    const bool secondAccepted = service.submit(testSaveTask(
                                                   2,
                                                   QStringList()
                                                   << QStringLiteral("2.png")))
            .isAccepted();
    std::future<ImageSaveSubmitResult> thirdSubmission = std::async(
                std::launch::async,
                [&service]() {
        return service.submit(testSaveTask(
                                  3,
                                  QStringList()
                                  << QStringLiteral("3.png")));
    });
    const bool thirdWaitedForSpace = thirdSubmission.wait_for(
                std::chrono::milliseconds(100))
            == std::future_status::timeout;

    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseFirstWrite = true;
    }
    gateCondition.notify_all();
    const bool thirdCompleted = thirdSubmission.wait_for(
                std::chrono::seconds(2))
            == std::future_status::ready;
    ImageSaveSubmitResult thirdResult;
    if (thirdCompleted) {
        thirdResult = thirdSubmission.get();
    }
    QTRY_VERIFY(service.outstandingTaskCount() == 0);
    int finalWriteCount = 0;
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        finalWriteCount = writeCount;
    }

    QVERIFY(firstAccepted);
    QVERIFY(firstStarted);
    QVERIFY(secondAccepted);
    QVERIFY(thirdWaitedForSpace);
    QVERIFY(thirdCompleted);
    QVERIFY(thirdResult.isAccepted());
    QCOMPARE(static_cast<int>(service.capacity()), 2);
    QCOMPARE(static_cast<int>(service.workerCount()), 1);
    QCOMPARE(finalWriteCount, 3);
}

void DetectionCompletionTest::writeFailureIsCountedAndReported()
{
    ImageSaveService service(
                8,
                [](const ImageSaveItem &, QString *errorMessage) {
        *errorMessage = QStringLiteral("simulated disk failure");
        return false;
    },
    1);
    QSignalSpy failureSpy(&service, &ImageSaveService::taskFailed);

    QVERIFY(service.submit(testSaveTask(
                               1,
                               QStringList() << QStringLiteral("failure.png")))
            .isAccepted());
    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(service.failedTaskCount(), quint64(1));
    QCOMPARE(
                failureSpy.at(0).at(1).toString(),
                QStringLiteral("simulated disk failure"));
}

void DetectionCompletionTest::shutdownRejectsNewTasks()
{
    ImageSaveService service(
                8,
                [](const ImageSaveItem &, QString *) {
        return true;
    },
    1);
    service.shutdown();

    const ImageSaveSubmitResult result = service.submit(
                testSaveTask(
                    1,
                    QStringList() << QStringLiteral("after-stop.png")));
    QVERIFY(result.status == ImageSaveSubmitStatus::Stopping);
}

QTEST_APPLESS_MAIN(DetectionCompletionTest)

#include "detection_completion_test.moc"
