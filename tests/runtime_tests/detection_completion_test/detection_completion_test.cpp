#include <QtTest>

#include "TrackingTypes.h"
#include "detection/common/detection_roi_geometry.h"
#include "runtime/detection_session.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_start_preflight.h"
#include "runtime/inspection_runtime_controller.h"
#include "runtime/result_handler.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>

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
    void startAccessAcceptsIdleOpenCamera();
    void startAccessPreservesGuardOrder();
    void dirtySettingsPrecedePlcConnectivity();
    void tissueStartNeedsNoTemplateAssets();
    void singleTemplateStartReportsOrderedMissingAssets();
    void wordStartRequiresProfilesAndCompleteCharacters();
    void barcodeStartAggregatesDecoderAndProfileErrors();
    void validWordAndBarcodeStartsAreAccepted();
    void roiPaddingIsClampedToImageBounds();
    void outsidePolygonIsClampedToNearestImageEdge();
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
