#include <QtTest>
#include <QColor>
#include <QFileInfo>
#include <QRegularExpression>

#include "TrackingTypes.h"
#include "detection/common/detection_roi_geometry.h"
#include "devices/barcode/barcode_decoder_adapter.h"
#include "devices/ocr/ocr_engine.h"
#include "runtime/detection_mode_worker_factory.h"
#include "runtime/detection_shadow_comparator.h"
#include "runtime/detection_worker.h"
#include "runtime/detection_session.h"
#include "runtime/frame_queue.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_acquisition_stop_coordinator.h"
#include "runtime/inspection_camera_recovery_transition.h"
#include "runtime/inspection_camera_start_transition.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_start_preflight.h"
#include "runtime/inspection_runtime_controller.h"
#include "runtime/inspection_runtime_start_transaction.h"
#include "runtime/inspection_runtime_stop_transaction.h"
#include "runtime/result_handler.h"
#include "runtime/result_presentation_mailbox.h"
#include "ui/controllers/detection_completion_controller.h"
#include "ui/controllers/operation_ui_policy.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui/presenters/detection_result_presenter.h"
#include "ui/presenters/inspection_fault_presenter.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

namespace {

class FactoryFakeOcrEngine : public IOcrEngine
{
public:
    std::vector<std::string> recognize(cv::Mat &) override
    {
        ++recognizeCalls;
        return std::vector<std::string>(1, "UNEXPECTED");
    }

    int recognizeCalls = 0;
};

class FactoryFakeBarcodeDecoder : public IBarcodeDecoder
{
public:
    bool ensureLoaded() override
    {
        ++ensureLoadedCalls;
        return true;
    }

    QString lastError() const override
    {
        return QString();
    }

    BarcodeReadResult decode(
            const cv::Mat &,
            const BarcodeDecodeOptions &,
            int preferredStrategyId,
            unsigned int preferredOptionFlags,
            int *,
            unsigned int *) override
    {
        ++decodeCalls;
        receivedStrategyIds.push_back(preferredStrategyId);
        receivedOptionFlags.push_back(preferredOptionFlags);
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::NotFound;
        return result;
    }

    int ensureLoadedCalls = 0;
    int decodeCalls = 0;
    std::vector<int> receivedStrategyIds;
    std::vector<unsigned int> receivedOptionFlags;
};

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

void bindTestResultView(
    DetectionResultPresenter *presenter,
    QStringList *events = nullptr)
{
    DetectionResultViewBindings bindings;
    bindings.showImage = [events](const QImage &) {
        if (events) {
            events->append(QStringLiteral("present"));
        }
    };
    bindings.showVerdictStyle = [](DetectionVerdictViewStyle) {
    };
    bindings.showVerdictText = [](const QString &) {
    };
    bindings.showRecognitionText = [](const QString &) {
    };
    bindings.showTemplateName = [](const QString &) {
    };
    bindings.showTotalCount = [](int) {
    };
    bindings.showNgCount = [](int) {
    };
    bindings.showPassRate = [](double) {
    };
    bindings.showElapsedText = [](const QString &) {
    };
    presenter->bindView(bindings);
}

DetectionCompletion acceptedControllerCompletion(
    InspectionRuntimeController *controller,
    AlgorithmVerdict verdict)
{
    const std::shared_ptr<const FrameData> frame =
            controller->acceptFrame(
                cv::Mat(3, 4, CV_8UC3, cv::Scalar(10, 20, 30)));
    DetectionResult result;
    result.modeId = QStringLiteral("test");
    result.verdict = verdict;
    result.status = DetectionStatus::Completed;
    return controller->complete(frame, result);
}

DetectionCompletionProcessRequest completionProcessRequest(
    const DetectionCompletion &completion)
{
    DetectionCompletionProcessRequest request;
    request.completion = completion;
    request.preparePresentation = [completion]() {
        DetectionResultViewSnapshot snapshot;
        snapshot.image = QImage(4, 3, QImage::Format_RGB32);
        snapshot.image.fill(Qt::white);
        snapshot.verdictStyle =
                completion.result.verdict == AlgorithmVerdict::Ok
                ? DetectionVerdictViewStyle::Correct
                : DetectionVerdictViewStyle::Error;
        return snapshot;
    };
    return request;
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

DetectionWorkItem factoryPositionedWorkItem(
        quint64 sequence,
        bool poseValid,
        int profileIndex)
{
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                testProductKey(sequence),
                sequence,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(100, 140, CV_8UC3, cv::Scalar(0, 0, 0)));
    DetectionPose pose;
    pose.valid = poseValid;
    pose.wordTemplateProfileIndex = profileIndex;
    pose.anchorCenter = cv::Point2f(70.0f, 50.0f);
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

class StartTransitionFakeCamera : public ICameraDevice
{
public:
    CameraOperationResult enumerateDevices(int *deviceCount) override
    {
        if (deviceCount) {
            *deviceCount = 1;
        }
        return CameraOperationResult();
    }

    CameraOperationResult openDevice(int deviceIndex) override
    {
        calls.append(QStringLiteral("open:%1").arg(deviceIndex));
        return CameraOperationResult(openErrorCode);
    }

    CameraOperationResult close() override
    {
        calls.append(QStringLiteral("close"));
        return CameraOperationResult();
    }

    CameraOperationResult registerImageCallback() override
    {
        calls.append(QStringLiteral("callback"));
        return CameraOperationResult();
    }

    CameraOperationResult startGrabbing() override
    {
        calls.append(QStringLiteral("start"));
        return CameraOperationResult();
    }

    CameraOperationResult stopGrabbing() override
    {
        calls.append(QStringLiteral("stop"));
        return CameraOperationResult();
    }

    CameraOperationResult setEnumValue(
        const char *key,
        unsigned int value) override
    {
        calls.append(QStringLiteral("enum:%1=%2")
                     .arg(QString::fromLatin1(key))
                     .arg(value));
        return CameraOperationResult();
    }

    CameraOperationResult setFloatValue(
        const char *key,
        float value) override
    {
        calls.append(QStringLiteral("float:%1=%2")
                     .arg(QString::fromLatin1(key))
                     .arg(value, 0, 'f', 1));
        return CameraOperationResult();
    }

    CameraOperationResult getFloatValue(
        const char *,
        CameraFloatValue *) override
    {
        return CameraOperationResult();
    }

    CameraOperationResult getBoolValue(
        const char *,
        bool *) override
    {
        return CameraOperationResult();
    }

    CameraOperationResult executeCommand(const char *) override
    {
        return CameraOperationResult();
    }

    CameraOperationResult readBuffer(cv::Mat &) override
    {
        return CameraOperationResult();
    }

    cv::Mat latestImage() override
    {
        return cv::Mat();
    }

    cv::Mat waitForImage() override
    {
        return cv::Mat();
    }

    bool takeImageForMainIfReady(cv::Mat &) override
    {
        return false;
    }

    std::uint64_t frameSequence() const override
    {
        return 0;
    }

    bool isImageReadyForMain() override
    {
        return false;
    }

    void setNonBlocking(bool) override
    {
    }

    void deferSwitchToBlockingAfterNextFrame() override
    {
    }

    void requestStop() override
    {
    }

    QStringList calls;
    int openErrorCode = 0;
};

std::shared_ptr<DetectionWorker> idleDetectionWorker()
{
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    1,
                    [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [](const DetectionCompletion &) {
    }));
}
}

class DetectionCompletionTest : public QObject
{
    Q_OBJECT

private slots:
    void operationUiPolicyPreservesClosedCameraControls();
    void operationUiPolicyPreservesReadyCameraControls();
    void operationUiPolicyPreservesDetectionAndStoppingControls();
    void operationUiPolicyPreservesTemplateCaptureControls();
    void operationUiPolicyFaultLocksOperationsUntilAcknowledged();
    void faultPresenterWarnsThatConveyorStateIsUnknown();
    void settingsEditStateIgnoresUnknownKeys();
    void settingsEditStateDeduplicatesSharedDisplayNames();
    void settingsEditStateIncludesTemplatePrivateChanges();
    void settingsEditStateClearsGlobalAndTemplateScopesSeparately();
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
    void faultStatePreservesFirstCauseAndSnapshot();
    void runtimeControllerRejectsFaultEntryOutsideActiveRun();
    void runtimeControllerFaultRejectsNewFramesWithoutProductKey();
    void runtimeControllerFaultStopsWorkerAndSurvivesStopCommit();
    void runtimeControllerTracksAbnormalStatisticsOutsidePassRate();
    void runtimeControllerRejectsInFlightCompletionWhileFaulted();
    void runtimeControllerAcknowledgementClearsActiveFaultOnly();
    void runtimeControllerNormalRecordLeavesNoFaultProductActions();
    void runtimeControllerSingleAcceptedFaultPlansOneFallbackNg();
    void runtimeControllerCompletedFaultProductIsNeverFallbackNg();
    void runtimeControllerMultipleFaultProductsCannotUseBlindFallbackNg();
    void runtimeControllerFaultedPlcOutputIsUnconfirmedExactlyOnce();
    void runtimeControllerFaultResolutionReleasesFrameOwnership();
    void runtimeControllerNewRunStartsWithEmptyReconciliation();
    void runtimeControllerOwnsCompletionAndResultHandling();
    void runtimeControllerRejectsDuplicateAndForeignCompletions();
    void runtimeControllerStartsNewRunWithoutResettingStatistics();
    void runtimeControllerPreservesSeparateResetScopes();
    void runtimeControllerStopsAdmissionBeforeDrainingAcceptedFrames();
    void cameraStartTransitionPreservesHardwareCallOrder();
    void cameraStartTransitionPreservesSoftwareCallOrder();
    void cameraStartTransitionStopsWhenExposureIsRejected();
    void runtimeStartTransactionCommitsRunningState();
    void runtimeStartTransactionRollsBackStartedWorker();
    void runtimeStartTransactionDestructorRollsBackStartingState();
    void acquisitionStopCoordinatorPreservesSoftwareAndHardwareOrder();
    void acquisitionStopCoordinatorSkipsIdleSoftwareWait();
    void acquisitionStopCoordinatorReportsTimeoutWithoutAfterStop();
    void cameraRecoveryTransitionPreservesCallOrder();
    void cameraRecoveryTransitionReportsOpenFailure();
    void cameraRecoveryTransitionClosesAfterExposureFailure();
    void runtimeStopTransactionPreservesStoppingUntilCommit();
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
    void frameQueueTrySubmitReportsFullWithoutBlocking();
    void detectionWorkerProcessesFramesSeriallyInOrder();
    void detectionWorkerReceivesPositionedDetectionWorkItem();
    void detectionWorkerTrySubmitExposesHardTriggerOverflow();
    void runtimeControllerAcceptedFramesFlowThroughDetectionWorker();
    void runtimeControllerOwnsDetectionWorkerLifecycle();
    void runtimeControllerSerializesAndCancelsUiCompletion();
    void modeWorkerFactoryCreatesTissueWorker();
    void modeWorkerFactoryPreservesOcrPose();
    void modeWorkerFactoryReturnsTypedStampOutput();
    void modeWorkerFactoryRejectsInvalidWordProfileIndex();
    void modeWorkerFactoryCarriesBarcodeStrategyAcrossFrames();
    void profileSnapshotPreservesOrderAndOwnsImages();
    void profileSnapshotUsesLegacyThresholdFallback();
    void modeWorkerDispatcherRejectsInvalidRequests();
    void modeWorkerDispatcherCreatesRequestedWorker();
    void modeWorkerDispatcherLoadsBarcodeDecoderBeforeCreation();
    void detectionWorkerRejectsSubmissionOutsideRun();
    void detectionWorkerCancellationSuppressesPendingResults();
    void detectionWorkerCanRestartAfterWait();
    void uiCompletionMailboxSerializesWholeProductWork();
    void uiCompletionMailboxCancellationReleasesProducer();
    void resultPresenterRendersDetectionOverlays();
    void resultPresenterMovesDetailsWithPose();
    void resultPresenterRendersAndClearsTissueOverlay();
    void resultPresenterAppliesWholeViewSnapshotInOrder();
    void resultPresenterPreservesPartialRefreshRules();
    void completionControllerRejectsInvalidRequestWithoutSideEffects();
    void completionControllerPreservesDelayedNgAndCurrentPlcOrder();
    void completionControllerSavesAnnotatedThenRaw();
    void completionControllerPreservesAnnotatedAndRawSelections();
    void completionControllerPreservesOcrRawOnlyLayout();
    void completionControllerWarnsForMissingAnnotatedImage();
    void completionControllerRejectsDuplicateBeforeRepeatedSideEffects();
    void roiPaddingIsClampedToImageBounds();
    void outsidePolygonIsClampedToNearestImageEdge();
    void orientedDateRoiClampsPaddingAtImageEdge();
    void saveTaskRequiresProductAndAllItems();
    void saveServicePreservesTaskAndItemOrder();
    void fullQueueWaitsForSpaceWithoutDroppingTask();
    void writeFailureIsCountedAndReported();
    void shutdownRejectsNewTasks();
};

void DetectionCompletionTest::operationUiPolicyPreservesClosedCameraControls()
{
    const OperationUiSnapshot snapshot =
            OperationUiPolicy::create(OperationUiState::CameraClosed);
    QVERIFY(snapshot.enableAllOperations);
    QVERIFY(snapshot.settingsEnabled);
    QVERIFY(snapshot.openCameraEnabled);
    QVERIFY(!snapshot.startDetectionEnabled);
    QVERIFY(!snapshot.stopEnabled);
    QVERIFY(!snapshot.closeCameraEnabled);
    QVERIFY(!snapshot.templateCaptureEnabled);
    QVERIFY(!snapshot.saveTemplateEnabled);
    QCOMPARE(snapshot.startDetectionText,
             QStringLiteral("\u542f\u52a8\u8bc6\u522b"));
}

void DetectionCompletionTest::operationUiPolicyPreservesReadyCameraControls()
{
    const OperationUiSnapshot snapshot =
            OperationUiPolicy::create(OperationUiState::CameraReady);
    QVERIFY(snapshot.enableAllOperations);
    QVERIFY(!snapshot.openCameraEnabled);
    QVERIFY(snapshot.startDetectionEnabled);
    QVERIFY(!snapshot.stopEnabled);
    QVERIFY(snapshot.closeCameraEnabled);
    QVERIFY(snapshot.templateCaptureEnabled);
    QVERIFY(!snapshot.saveTemplateEnabled);
}

void DetectionCompletionTest::operationUiPolicyPreservesDetectionAndStoppingControls()
{
    const OperationUiSnapshot detecting =
            OperationUiPolicy::create(OperationUiState::Detecting);
    QVERIFY(!detecting.enableAllOperations);
    QVERIFY(!detecting.settingsEnabled);
    QVERIFY(!detecting.openCameraEnabled);
    QVERIFY(!detecting.startDetectionEnabled);
    QVERIFY(detecting.stopEnabled);
    QCOMPARE(detecting.startDetectionText,
             QStringLiteral("\u91c7\u96c6\u4e2d..."));

    const OperationUiSnapshot stopping =
            OperationUiPolicy::create(OperationUiState::Stopping);
    QVERIFY(!stopping.enableAllOperations);
    QVERIFY(stopping.stopEnabled);
    QCOMPARE(stopping.startDetectionText,
             QStringLiteral("\u505c\u6b62\u4e2d..."));
    QCOMPARE(stopping.stopText,
             QStringLiteral("\u505c\u6b62\u4e2d..."));
}

void DetectionCompletionTest::operationUiPolicyPreservesTemplateCaptureControls()
{
    const OperationUiSnapshot previewing =
            OperationUiPolicy::create(OperationUiState::TemplatePreviewing);
    QVERIFY(!previewing.enableAllOperations);
    QVERIFY(previewing.stopEnabled);
    QVERIFY(previewing.templateCaptureEnabled);
    QVERIFY(!previewing.saveTemplateEnabled);
    QVERIFY(!previewing.statusText.isEmpty());

    const OperationUiSnapshot frozen =
            OperationUiPolicy::create(OperationUiState::TemplateFrozen);
    QVERIFY(frozen.stopEnabled);
    QVERIFY(frozen.templateCaptureEnabled);
    QVERIFY(frozen.saveTemplateEnabled);
    QVERIFY(!frozen.statusText.isEmpty());
    QVERIFY(frozen.statusText != previewing.statusText);
}

void DetectionCompletionTest::operationUiPolicyFaultLocksOperationsUntilAcknowledged()
{
    const OperationUiSnapshot snapshot =
            OperationUiPolicy::create(OperationUiState::Fault);
    QVERIFY(!snapshot.enableAllOperations);
    QVERIFY(!snapshot.settingsEnabled);
    QVERIFY(!snapshot.openCameraEnabled);
    QVERIFY(!snapshot.startDetectionEnabled);
    QVERIFY(snapshot.stopEnabled);
    QVERIFY(!snapshot.closeCameraEnabled);
    QVERIFY(!snapshot.templateCaptureEnabled);
    QVERIFY(!snapshot.saveTemplateEnabled);
    QCOMPARE(snapshot.stopText,
             QStringLiteral("\u786e\u8ba4\u6545\u969c\u5e76\u6062\u590d"));
    QVERIFY(!snapshot.statusText.isEmpty());
}

void DetectionCompletionTest::faultPresenterWarnsThatConveyorStateIsUnknown()
{
    InspectionFaultSnapshot snapshot;
    snapshot.reason = InspectionFaultReason::PlcDisconnected;
    snapshot.diagnostic = QStringLiteral("native error 5");
    snapshot.runId = QStringLiteral("fault-run");
    snapshot.acceptedProductCount = 9;
    snapshot.completedProductCount = 7;

    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    QVERIFY(presentation.isValid());
    QVERIFY(presentation.statusText.contains(
                QStringLiteral("\u68c0\u6d4b\u5df2\u6682\u505c")));
    QVERIFY(presentation.operatorMessage.contains(
                QStringLiteral("native error 5")));
    QVERIFY(presentation.operatorMessage.contains(
                QStringLiteral("\u8f93\u9001\u7ebf\u72b6\u6001\u672a\u77e5")));
    QVERIFY(presentation.operatorMessage.contains(
                QStringLiteral("\u9694\u79bb\u6545\u969c\u671f\u95f4\u7684\u4ea7\u54c1")));
    QVERIFY(!presentation.operatorMessage.contains(
                QStringLiteral("\u8f93\u9001\u7ebf\u5df2\u505c\u6b62")));

    const InspectionFaultPresentation empty =
            InspectionFaultPresenter::create(
                InspectionFaultSnapshot());
    QVERIFY(!empty.isValid());
}

void DetectionCompletionTest::settingsEditStateIgnoresUnknownKeys()
{
    SettingsEditState state;
    state.setGlobalDirty(QStringLiteral("unknown"), true);
    QVERIFY(!state.hasDirtySettings());
    QVERIFY(state.dirtySettingsMessage().isEmpty());
}

void DetectionCompletionTest::settingsEditStateDeduplicatesSharedDisplayNames()
{
    SettingsEditState state;
    state.registerGlobalSetting(QStringLiteral("plc.ip"),
                                QStringLiteral("PLC\u8fde\u63a5\uff1a"));
    state.registerGlobalSetting(QStringLiteral("plc.rack"),
                                QStringLiteral("PLC\u8fde\u63a5\uff1a"));
    state.setGlobalDirty(QStringLiteral("plc.ip"), true);
    state.setGlobalDirty(QStringLiteral("plc.rack"), true);
    QCOMPARE(state.globalDirtyNames().size(), 1);
    QCOMPARE(state.globalDirtyNames().first(),
             QStringLiteral("PLC\u8fde\u63a5"));
    QVERIFY(state.dirtySettingsMessage().contains(
                QStringLiteral("PLC\u8fde\u63a5")));
}

void DetectionCompletionTest::settingsEditStateIncludesTemplatePrivateChanges()
{
    SettingsEditState state;
    state.registerGlobalSetting(QStringLiteral("camera.exposure"),
                                QStringLiteral("\u76f8\u673a\u66dd\u5149:"));
    state.setGlobalDirty(QStringLiteral("camera.exposure"), true);
    state.setTemplateTargetDirty(true);
    state.setTemplateThresholdDirty(true);
    const QStringList names = state.dirtyNames();
    QCOMPARE(names.size(), 3);
    QVERIFY(names.contains(QStringLiteral("\u76ee\u6807\u5b57\u7b26\u5185\u5bb9")));
    QVERIFY(names.contains(QStringLiteral("\u56fe\u50cf\u5408\u683c\u9608\u503c")));
}

void DetectionCompletionTest::settingsEditStateClearsGlobalAndTemplateScopesSeparately()
{
    SettingsEditState state;
    state.registerGlobalSetting(QStringLiteral("camera.gain"),
                                QStringLiteral("gain"));
    state.setGlobalDirty(QStringLiteral("camera.gain"), true);
    state.setTemplateTargetDirty(true);
    state.clearAllGlobalDirty();
    QVERIFY(!state.isGlobalDirty(QStringLiteral("camera.gain")));
    QVERIFY(state.isTemplateTargetDirty());
    QVERIFY(state.hasDirtySettings());
    state.clearTemplateDirty();
    QVERIFY(!state.hasDirtySettings());
}

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
    ProductKey delayedProductKey;
    QVERIFY(!handler.consumeDueDelayedNgRequest(
                &delayedProductKey));
    QVERIFY(!delayedProductKey.isValid());

    const DetectionResultHandlingOutcome okOutcome = handler.record(
                testCompletion(2, AlgorithmVerdict::Ok),
                0,
                2);
    QVERIFY(okOutcome.plcAction == DetectionPlcAction::RequestOk);
    QVERIFY(handler.consumeDueDelayedNgRequest(
                &delayedProductKey));
    QCOMPARE(delayedProductKey.runId, QStringLiteral("test-run"));
    QCOMPARE(delayedProductKey.sequence, quint64(1));
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
    QVERIFY(controller.enterFault(
                InspectionFaultReason::RuntimeInvariantViolation,
                QStringLiteral("test fault")));
    QVERIFY(controller.state() == InspectionRuntimeState::Fault);
    QVERIFY(controller.beginStart().isEmpty());

    controller.acknowledgeFault();
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QCOMPARE(controller.beginStart(), QStringLiteral("run-2"));
}

void DetectionCompletionTest::faultStatePreservesFirstCauseAndSnapshot()
{
    InspectionFaultState state;
    const QDateTime occurredAtUtc = QDateTime::fromMSecsSinceEpoch(
                123456,
                Qt::UTC);

    QVERIFY(!state.enter(
                InspectionFaultReason::None,
                QStringLiteral("ignored"),
                QStringLiteral("run-1"),
                2,
                1,
                occurredAtUtc));
    QVERIFY(state.enter(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("  plc link lost  "),
                QStringLiteral("run-1"),
                2,
                1,
                occurredAtUtc));
    QVERIFY(state.isActive());
    QVERIFY(!state.enter(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("must not replace first fault"),
                QStringLiteral("run-2"),
                9,
                8));
    QVERIFY(state.notePostFaultDroppedFrame());
    QVERIFY(state.notePostFaultDroppedFrame());

    const InspectionFaultSnapshot snapshot = state.snapshot();
    QVERIFY(snapshot.reason
            == InspectionFaultReason::PlcDisconnected);
    QCOMPARE(snapshot.diagnostic, QStringLiteral("plc link lost"));
    QCOMPARE(snapshot.runId, QStringLiteral("run-1"));
    QCOMPARE(snapshot.acceptedProductCount, quint64(2));
    QCOMPARE(snapshot.completedProductCount, quint64(1));
    QCOMPARE(snapshot.postFaultDroppedFrameCount, quint64(2));
    QCOMPARE(snapshot.occurredAtUtc, occurredAtUtc);

    QVERIFY(state.clear());
    QVERIFY(!state.isActive());
    QVERIFY(!state.notePostFaultDroppedFrame());
    QVERIFY(!state.clear());
}

void DetectionCompletionTest::runtimeControllerRejectsFaultEntryOutsideActiveRun()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("fault-entry-run");
    });

    QVERIFY(!controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("idle disconnect")));
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.faultSnapshot().isActive());

    QCOMPARE(controller.beginStart(), QStringLiteral("fault-entry-run"));
    QVERIFY(!controller.enterFault(InspectionFaultReason::None));
    QVERIFY(controller.state() == InspectionRuntimeState::Starting);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("runtime disconnect")));
    QVERIFY(controller.state() == InspectionRuntimeState::Fault);
    QVERIFY(controller.isBusy());
    QVERIFY(controller.beginStart().isEmpty());
    QVERIFY(!controller.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("later fault")));

    controller.finishStop();
    QVERIFY(controller.state() == InspectionRuntimeState::Fault);
}

void DetectionCompletionTest::runtimeControllerFaultRejectsNewFramesWithoutProductKey()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("fault-admission-run");
    });
    const cv::Mat image(2, 2, CV_8UC1, cv::Scalar(17));

    controller.beginStart();
    QVERIFY(controller.markRunning());
    const std::shared_ptr<const FrameData> accepted =
            controller.acceptFrame(image);
    QVERIFY(accepted);
    QCOMPARE(accepted->productKey.sequence, quint64(1));
    QVERIFY(controller.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("queue full")));

    QVERIFY(!controller.acceptFrame(cv::Mat()));
    QVERIFY(!controller.acceptFrame(image));
    QVERIFY(!controller.acceptFrame(image));
    QCOMPARE(controller.acceptedProductCount(), quint64(1));

    const InspectionFaultSnapshot snapshot = controller.faultSnapshot();
    QCOMPARE(snapshot.runId, QStringLiteral("fault-admission-run"));
    QCOMPARE(snapshot.acceptedProductCount, quint64(1));
    QCOMPARE(snapshot.completedProductCount, quint64(0));
    QCOMPARE(snapshot.postFaultDroppedFrameCount, quint64(2));
    QCOMPARE(
                controller.abnormalStatistics()
                .postFaultDroppedFrameCount,
                quint64(2));
}

void DetectionCompletionTest::runtimeControllerFaultStopsWorkerAndSurvivesStopCommit()
{
    InspectionRuntimeController controller;
    const std::shared_ptr<DetectionWorker> worker =
            idleDetectionWorker();

    QVERIFY(!controller.beginStart().isEmpty());
    QVERIFY(controller.startDetectionWorker(2, worker));
    QVERIFY(controller.markRunning());
    QVERIFY(controller.isDetectionWorkerActiveForMode(2));
    QVERIFY(controller.enterFault(
                InspectionFaultReason::ProductIdentityAmbiguous,
                QStringLiteral("identity cannot be guaranteed")));
    QVERIFY(!controller.isDetectionWorkerActive());

    controller.finishStop();
    QVERIFY(controller.state() == InspectionRuntimeState::Fault);
    QVERIFY(controller.acknowledgeFault());
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!worker->isRunning());
}

void DetectionCompletionTest::runtimeControllerTracksAbnormalStatisticsOutsidePassRate()
{
    int runNumber = 0;
    InspectionRuntimeController controller([&runNumber]() {
        return QStringLiteral("statistics-run-%1").arg(++runNumber);
    });
    DetectionResult okResult;
    okResult.verdict = AlgorithmVerdict::Ok;
    okResult.status = DetectionStatus::Completed;

    controller.beginStart();
    QVERIFY(controller.markRunning());
    const DetectionCompletion normalCompletion = controller.complete(
                    cv::Mat(1, 1, CV_8UC1, cv::Scalar(3)),
                    okResult);
    const DetectionResultHandlingOutcome normalOutcome = controller.record(
                normalCompletion,
                0,
                0);
    QVERIFY(normalOutcome.resultRecorded);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::RuntimeInvariantViolation,
                QStringLiteral("runtime invariant")));
    QVERIFY(controller.recordFaultedPlcOutput(
                normalCompletion.frame->productKey));
    QVERIFY(!controller.recordFaultedPlcOutput(
                normalCompletion.frame->productKey));

    DetectionResultStatistics statistics = controller.statistics();
    DetectionAbnormalStatistics abnormalStatistics =
            controller.abnormalStatistics();
    QCOMPARE(statistics.totalCount, 1);
    QCOMPARE(statistics.ngCount, 0);
    QCOMPARE(statistics.passRatePercent(), 100.0);
    QCOMPARE(abnormalStatistics.systemFaultCount, quint64(1));
    QCOMPARE(abnormalStatistics.cancelledProductCount, quint64(0));
    QCOMPARE(abnormalStatistics.unconfirmedProductCount, quint64(1));
    QCOMPARE(abnormalStatistics.postFaultDroppedFrameCount, quint64(0));

    QVERIFY(controller.acknowledgeFault());
    QCOMPARE(controller.beginStart(), QStringLiteral("statistics-run-2"));
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("second run fault")));
    statistics = controller.statistics();
    abnormalStatistics = controller.abnormalStatistics();
    QCOMPARE(statistics.totalCount, 1);
    QCOMPARE(statistics.ngCount, 0);
    QCOMPARE(abnormalStatistics.systemFaultCount, quint64(2));
    QCOMPARE(abnormalStatistics.cancelledProductCount, quint64(0));
    QCOMPARE(abnormalStatistics.unconfirmedProductCount, quint64(1));

    controller.resetStatistics();
    QCOMPARE(controller.statistics().totalCount, 0);
    QCOMPARE(
                controller.abnormalStatistics().systemFaultCount,
                quint64(2));
    controller.resetAbnormalStatistics();
    QCOMPARE(
                controller.abnormalStatistics().systemFaultCount,
                quint64(0));
    QCOMPARE(controller.statistics().totalCount, 0);
}

void DetectionCompletionTest::runtimeControllerRejectsInFlightCompletionWhileFaulted()
{
    InspectionRuntimeController controller;
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(11));
    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;

    controller.beginStart();
    QVERIFY(controller.markRunning());
    const std::shared_ptr<const FrameData> frame =
            controller.acceptFrame(image);
    QVERIFY(frame);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected));

    const DetectionCompletion completion =
            controller.complete(frame, result);
    QVERIFY(!completion.isValid());
    const DetectionResultHandlingOutcome outcome =
            controller.record(completion, 3, 0);
    QVERIFY(!outcome.resultRecorded);
    QVERIFY(outcome.imageSaveAction
            == DetectionResultSaveAction::DoNotSave);
    QVERIFY(outcome.plcAction == DetectionPlcAction::NoRequest);
    QCOMPARE(controller.totalCount(), 0);
    QCOMPARE(controller.ngCount(), 0);
    QCOMPARE(
                controller.abnormalStatistics().systemFaultCount,
                quint64(1));
}

void DetectionCompletionTest::runtimeControllerAcknowledgementClearsActiveFaultOnly()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("fault-ack-run");
    });

    QVERIFY(!controller.acknowledgeFault());
    controller.beginStart();
    QVERIFY(controller.markRunning());
    QVERIFY(controller.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("overflow")));
    QVERIFY(controller.faultSnapshot().isActive());
    QCOMPARE(
                controller.abnormalStatistics().systemFaultCount,
                quint64(1));

    QVERIFY(controller.acknowledgeFault());
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.faultSnapshot().isActive());
    QVERIFY(controller.faultSnapshot().diagnostic.isEmpty());
    QCOMPARE(
                controller.abnormalStatistics().systemFaultCount,
                quint64(1));
    QVERIFY(!controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerNormalRecordLeavesNoFaultProductActions()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("normal-close-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());

    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion completion = controller.complete(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(7)),
                result);
    QVERIFY(controller.record(completion, 0, 0).resultRecorded);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("fault after normal record")));

    QVERIFY(controller.faultProductActions(true).empty());
    QCOMPARE(controller.unresolvedFaultProductCount(), 0);
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerSingleAcceptedFaultPlansOneFallbackNg()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("single-fallback-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());
    const std::shared_ptr<const FrameData> frame = controller.acceptFrame(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(8)));
    QVERIFY(frame);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("single unresolved product")));

    const std::vector<InspectionFaultProductAction> disconnectedActions =
            controller.faultProductActions(false);
    QCOMPARE(static_cast<int>(disconnectedActions.size()), 1);
    QVERIFY(disconnectedActions.front().type
            == InspectionFaultProductActionType::RecordUnconfirmed);

    const std::vector<InspectionFaultProductAction> writableActions =
            controller.faultProductActions(true);
    QCOMPARE(static_cast<int>(writableActions.size()), 1);
    QCOMPARE(writableActions.front().productKey.runId,
             QStringLiteral("single-fallback-run"));
    QCOMPARE(writableActions.front().productKey.sequence, quint64(1));
    QVERIFY(writableActions.front().type
            == InspectionFaultProductActionType::RequestFallbackNg);
    QVERIFY(!controller.acknowledgeFault());

    QVERIFY(controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::FallbackNgRequested));
    QVERIFY(!controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::FallbackNgRequested));
    QCOMPARE(
                controller.abnormalStatistics().cancelledProductCount,
                quint64(1));
    QCOMPARE(controller.faultFallbackNgResolutionCount(), 1);
    QCOMPARE(controller.faultUnconfirmedProductCount(), 0);
    QCOMPARE(controller.totalCount(), 0);
    QCOMPARE(controller.ngCount(), 0);
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerCompletedFaultProductIsNeverFallbackNg()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("completed-fault-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());

    const std::shared_ptr<const FrameData> frame = controller.acceptFrame(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(9)));
    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ok;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion completion = controller.complete(frame, result);
    QVERIFY(completion.isValid());
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("fault before result record")));

    const std::vector<InspectionFaultProductAction> actions =
            controller.faultProductActions(true);
    QCOMPARE(static_cast<int>(actions.size()), 1);
    QVERIFY(actions.front().type
            == InspectionFaultProductActionType::RecordUnconfirmed);
    QVERIFY(!controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::FallbackNgRequested));
    QVERIFY(controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::Unconfirmed));
    QCOMPARE(
                controller.abnormalStatistics().unconfirmedProductCount,
                quint64(1));
    QCOMPARE(controller.faultUnconfirmedProductCount(), 1);
    QCOMPARE(controller.totalCount(), 0);
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerMultipleFaultProductsCannotUseBlindFallbackNg()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("multiple-fault-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());
    const cv::Mat image(2, 2, CV_8UC1, cv::Scalar(10));
    const std::shared_ptr<const FrameData> first =
            controller.acceptFrame(image);
    const std::shared_ptr<const FrameData> second =
            controller.acceptFrame(image);
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::ProductIdentityAmbiguous,
                QStringLiteral("two products without PLC identity")));

    const std::vector<InspectionFaultProductAction> actions =
            controller.faultProductActions(true);
    QCOMPARE(static_cast<int>(actions.size()), 2);
    QCOMPARE(actions[0].productKey.sequence, quint64(1));
    QCOMPARE(actions[1].productKey.sequence, quint64(2));
    QVERIFY(actions[0].type
            == InspectionFaultProductActionType::RecordUnconfirmed);
    QVERIFY(actions[1].type
            == InspectionFaultProductActionType::RecordUnconfirmed);
    QVERIFY(!controller.resolveFaultProduct(
                first->productKey,
                InspectionFaultProductResolution::FallbackNgRequested));
    QVERIFY(controller.resolveFaultProduct(
                first->productKey,
                InspectionFaultProductResolution::Unconfirmed));
    QVERIFY(controller.resolveFaultProduct(
                second->productKey,
                InspectionFaultProductResolution::Unconfirmed));
    QCOMPARE(
                controller.abnormalStatistics().unconfirmedProductCount,
                quint64(2));
    QCOMPARE(controller.faultUnconfirmedProductCount(), 2);
    QCOMPARE(controller.unresolvedFaultProductCount(), 0);
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerFaultedPlcOutputIsUnconfirmedExactlyOnce()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("plc-output-fault-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());

    DetectionResult result;
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion completion = controller.complete(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(11)),
                result);
    QVERIFY(controller.record(completion, 0, 0).resultRecorded);
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected,
                QStringLiteral("NG output failed")));

    QVERIFY(controller.recordFaultedPlcOutput(
                completion.frame->productKey));
    QVERIFY(!controller.recordFaultedPlcOutput(
                completion.frame->productKey));
    QCOMPARE(controller.faultedPlcOutputCount(), 1);
    QCOMPARE(controller.faultUnconfirmedProductCount(), 1);
    QCOMPARE(controller.totalCount(), 1);
    QCOMPARE(controller.ngCount(), 1);
    QCOMPARE(
                controller.abnormalStatistics().unconfirmedProductCount,
                quint64(1));
    QVERIFY(controller.faultProductActions(true).empty());
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerFaultResolutionReleasesFrameOwnership()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("frame-release-run");
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());

    std::shared_ptr<const FrameData> frame = controller.acceptFrame(
                cv::Mat(64, 64, CV_8UC3, cv::Scalar(1, 2, 3)));
    QVERIFY(frame);
    const std::weak_ptr<const FrameData> weakFrame = frame;
    QVERIFY(controller.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral("release accepted frame")));
    QVERIFY(controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::Unconfirmed));

    frame.reset();
    QVERIFY(weakFrame.expired());
    QVERIFY(controller.acknowledgeFault());
}

void DetectionCompletionTest::runtimeControllerNewRunStartsWithEmptyReconciliation()
{
    int runNumber = 0;
    InspectionRuntimeController controller([&runNumber]() {
        return QStringLiteral("reconciliation-run-%1")
                .arg(++runNumber);
    });
    controller.beginStart();
    QVERIFY(controller.markRunning());
    const std::shared_ptr<const FrameData> frame = controller.acceptFrame(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(12)));
    QVERIFY(controller.enterFault(
                InspectionFaultReason::PlcDisconnected));
    QVERIFY(controller.resolveFaultProduct(
                frame->productKey,
                InspectionFaultProductResolution::Unconfirmed));
    QVERIFY(controller.acknowledgeFault());

    QCOMPARE(controller.beginStart(),
             QStringLiteral("reconciliation-run-2"));
    QVERIFY(controller.markRunning());
    QVERIFY(controller.enterFault(
                InspectionFaultReason::RuntimeInvariantViolation));
    QCOMPARE(controller.unresolvedFaultProductCount(), 0);
    QVERIFY(controller.faultProductActions(true).empty());
    QVERIFY(controller.acknowledgeFault());
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

void DetectionCompletionTest::cameraStartTransitionPreservesHardwareCallOrder()
{
    StartTransitionFakeCamera camera;
    InspectionCameraStartRequest request;
    request.cameraDevice = &camera;
    request.acquisitionKind =
            InspectionAcquisitionKind::HardwareTrigger;
    request.gain = 12.5f;
    request.applyExposure = [&camera](QString *) {
        camera.calls.append(QStringLiteral("exposure"));
        return true;
    };
    request.delayMilliseconds = [&camera](unsigned long milliseconds) {
        camera.calls.append(QStringLiteral("delay:%1").arg(
                                static_cast<qulonglong>(milliseconds)));
    };

    const InspectionCameraStartResult result =
            InspectionCameraStartTransition::apply(request);

    QVERIFY(result.isAccepted());
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("stop")
                << QStringLiteral("delay:200")
                << QStringLiteral("enum:TriggerMode=1")
                << QStringLiteral("enum:TriggerSource=0")
                << QStringLiteral("exposure")
                << QStringLiteral("float:Gain=12.5")
                << QStringLiteral("float:TriggerDelay=0.0")
                << QStringLiteral("callback")
                << QStringLiteral("start")
                << QStringLiteral("enum:LineDebouncerTime=5000")
                << QStringLiteral("delay:100"));
}

void DetectionCompletionTest::cameraStartTransitionPreservesSoftwareCallOrder()
{
    StartTransitionFakeCamera camera;
    InspectionCameraStartRequest request;
    request.cameraDevice = &camera;
    request.acquisitionKind =
            InspectionAcquisitionKind::SoftwareTrigger;
    request.gain = 8.0f;
    request.applyExposure = [&camera](QString *) {
        camera.calls.append(QStringLiteral("exposure"));
        return true;
    };

    const InspectionCameraStartResult result =
            InspectionCameraStartTransition::apply(request);

    QVERIFY(result.isAccepted());
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("enum:TriggerSource=7")
                << QStringLiteral("exposure")
                << QStringLiteral("float:Gain=8.0"));
}

void DetectionCompletionTest::cameraStartTransitionStopsWhenExposureIsRejected()
{
    StartTransitionFakeCamera camera;
    InspectionCameraStartRequest request;
    request.cameraDevice = &camera;
    request.acquisitionKind =
            InspectionAcquisitionKind::HardwareTrigger;
    request.applyExposure = [&camera](QString *errorMessage) {
        camera.calls.append(QStringLiteral("exposure"));
        *errorMessage = QStringLiteral("exposure rejected");
        return false;
    };
    request.delayMilliseconds = [&camera](unsigned long milliseconds) {
        camera.calls.append(QStringLiteral("delay:%1").arg(
                                static_cast<qulonglong>(milliseconds)));
    };

    const InspectionCameraStartResult result =
            InspectionCameraStartTransition::apply(request);

    QVERIFY(!result.isAccepted());
    QVERIFY(result.issue
            == InspectionCameraStartIssue::ExposureRejected);
    QCOMPARE(result.errorMessage, QStringLiteral("exposure rejected"));
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("stop")
                << QStringLiteral("delay:200")
                << QStringLiteral("enum:TriggerMode=1")
                << QStringLiteral("enum:TriggerSource=0")
                << QStringLiteral("exposure"));
}

void DetectionCompletionTest::runtimeStartTransactionCommitsRunningState()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("transaction-run");
    });
    InspectionRuntimeStartTransaction transaction(controller);

    QVERIFY(transaction.begin());
    QCOMPARE(transaction.runId(), QStringLiteral("transaction-run"));
    QVERIFY(transaction.hasBegun());
    QVERIFY(transaction.commit());
    QVERIFY(transaction.isCommitted());
    QVERIFY(controller.state() == InspectionRuntimeState::Running);

    QVERIFY(controller.requestStop());
    controller.finishStop();
}

void DetectionCompletionTest::runtimeStartTransactionRollsBackStartedWorker()
{
    InspectionRuntimeController controller;
    InspectionRuntimeStartTransaction transaction(controller);
    const std::shared_ptr<DetectionWorker> worker =
            idleDetectionWorker();

    QVERIFY(transaction.begin());
    QVERIFY(transaction.startDetectionWorker(3, worker));
    QVERIFY(controller.isDetectionWorkerActiveForMode(3));

    transaction.rollback();

    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.isDetectionWorkerActive());
    QVERIFY(!worker->isRunning());
    QVERIFY(!transaction.hasBegun());
}

void DetectionCompletionTest::runtimeStartTransactionDestructorRollsBackStartingState()
{
    InspectionRuntimeController controller;
    {
        InspectionRuntimeStartTransaction transaction(controller);
        QVERIFY(transaction.begin());
        QVERIFY(controller.state() == InspectionRuntimeState::Starting);
    }

    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
    QVERIFY(!controller.isDetectionWorkerActive());
}

void DetectionCompletionTest::acquisitionStopCoordinatorPreservesSoftwareAndHardwareOrder()
{
    QStringList calls;
    InspectionAcquisitionStopRequest request;
    request.software.isPresent = true;
    request.software.isRunning = []() {
        return true;
    };
    request.software.requestStop = [&calls]() {
        calls.append(QStringLiteral("software-request"));
    };
    request.software.stopLoop = [&calls]() {
        calls.append(QStringLiteral("software-stop"));
    };
    request.software.wait = [&calls](unsigned long milliseconds) {
        calls.append(QStringLiteral("software-wait:%1").arg(
                         static_cast<qulonglong>(milliseconds)));
        return true;
    };
    request.software.afterStopped = [&calls]() {
        calls.append(QStringLiteral("software-after"));
    };
    request.hardware.isPresent = true;
    request.hardware.waitWhenPresent = true;
    request.hardware.repeatStopRequestAfterPrepare = true;
    request.hardware.isRunning = []() {
        return true;
    };
    request.hardware.requestStop = [&calls]() {
        calls.append(QStringLiteral("hardware-request"));
    };
    request.hardware.prepareForWait = [&calls]() {
        calls.append(QStringLiteral("hardware-prepare"));
    };
    request.hardware.wait = [&calls](unsigned long milliseconds) {
        calls.append(QStringLiteral("hardware-wait:%1").arg(
                         static_cast<qulonglong>(milliseconds)));
        return true;
    };
    request.hardware.afterStopped = [&calls]() {
        calls.append(QStringLiteral("hardware-after"));
    };

    const InspectionAcquisitionStopResult result =
            InspectionAcquisitionStopCoordinator::stop(request);

    QVERIFY(result.softwareWasRunning);
    QVERIFY(result.hardwareWasPresent);
    QVERIFY(result.allStopped());
    QVERIFY(result.shouldRestoreCamera());
    QCOMPARE(
                calls,
                QStringList()
                << QStringLiteral("software-request")
                << QStringLiteral("hardware-request")
                << QStringLiteral("software-stop")
                << QStringLiteral("software-wait:3000")
                << QStringLiteral("software-after")
                << QStringLiteral("hardware-prepare")
                << QStringLiteral("hardware-request")
                << QStringLiteral("hardware-wait:3000")
                << QStringLiteral("hardware-after"));
}

void DetectionCompletionTest::acquisitionStopCoordinatorSkipsIdleSoftwareWait()
{
    QStringList calls;
    InspectionAcquisitionStopRequest request;
    request.software.isPresent = true;
    request.software.isRunning = []() {
        return false;
    };
    request.software.requestStop = [&calls]() {
        calls.append(QStringLiteral("software-request"));
    };
    request.software.stopLoop = [&calls]() {
        calls.append(QStringLiteral("software-stop"));
    };
    request.software.wait = [&calls](unsigned long) {
        calls.append(QStringLiteral("software-wait"));
        return true;
    };
    request.software.afterStopped = [&calls]() {
        calls.append(QStringLiteral("software-after"));
    };

    const InspectionAcquisitionStopResult result =
            InspectionAcquisitionStopCoordinator::stop(request);

    QVERIFY(!result.softwareWasRunning);
    QVERIFY(!result.hardwareWasPresent);
    QVERIFY(result.allStopped());
    QVERIFY(!result.shouldRestoreCamera());
    QCOMPARE(calls,
             QStringList() << QStringLiteral("software-request"));
}

void DetectionCompletionTest::acquisitionStopCoordinatorReportsTimeoutWithoutAfterStop()
{
    QStringList calls;
    InspectionAcquisitionStopRequest request;
    request.hardware.isPresent = true;
    request.hardware.waitWhenPresent = true;
    request.hardware.requestStop = [&calls]() {
        calls.append(QStringLiteral("hardware-request"));
    };
    request.hardware.wait = [&calls](unsigned long milliseconds) {
        calls.append(QStringLiteral("hardware-wait:%1").arg(
                         static_cast<qulonglong>(milliseconds)));
        return false;
    };
    request.hardware.afterStopped = [&calls]() {
        calls.append(QStringLiteral("hardware-after"));
    };

    const InspectionAcquisitionStopResult result =
            InspectionAcquisitionStopCoordinator::stop(request);

    QVERIFY(result.softwareStopped);
    QVERIFY(!result.hardwareStopped);
    QVERIFY(!result.allStopped());
    QVERIFY(result.shouldRestoreCamera());
    QCOMPARE(
                calls,
                QStringList()
                << QStringLiteral("hardware-request")
                << QStringLiteral("hardware-wait:3000"));
}

void DetectionCompletionTest::cameraRecoveryTransitionPreservesCallOrder()
{
    StartTransitionFakeCamera camera;
    InspectionCameraRecoveryRequest request;
    request.cameraDevice = &camera;
    request.recoveryRequired = true;
    request.cameraWasOpen = true;
    request.applySavedExposure = [&camera](
            QString *adjustmentMessage,
            QString *) {
        camera.calls.append(QStringLiteral("exposure"));
        *adjustmentMessage = QStringLiteral("exposure adjusted");
        return true;
    };
    request.delayMilliseconds = [&camera](unsigned long milliseconds) {
        camera.calls.append(QStringLiteral("delay:%1").arg(
                                static_cast<qulonglong>(milliseconds)));
    };

    const InspectionCameraRecoveryResult result =
            InspectionCameraRecoveryTransition::apply(request);

    QVERIFY(result.isRecovered());
    QVERIFY(result.recoveryAttempted);
    QVERIFY(result.cameraOpen);
    QCOMPARE(result.adjustmentMessage,
             QStringLiteral("exposure adjusted"));
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("close")
                << QStringLiteral("delay:100")
                << QStringLiteral("open:0")
                << QStringLiteral("enum:TriggerMode=1")
                << QStringLiteral("enum:TriggerSource=7")
                << QStringLiteral("exposure")
                << QStringLiteral("float:TriggerDelay=0.0")
                << QStringLiteral("callback")
                << QStringLiteral("start"));
}

void DetectionCompletionTest::cameraRecoveryTransitionReportsOpenFailure()
{
    StartTransitionFakeCamera camera;
    camera.openErrorCode = -7;
    InspectionCameraRecoveryRequest request;
    request.cameraDevice = &camera;
    request.recoveryRequired = true;
    request.cameraWasOpen = true;
    request.delayMilliseconds = [&camera](unsigned long milliseconds) {
        camera.calls.append(QStringLiteral("delay:%1").arg(
                                static_cast<qulonglong>(milliseconds)));
    };

    const InspectionCameraRecoveryResult result =
            InspectionCameraRecoveryTransition::apply(request);

    QVERIFY(!result.isRecovered());
    QVERIFY(result.issue
            == InspectionCameraRecoveryIssue::OpenFailed);
    QVERIFY(result.recoveryAttempted);
    QVERIFY(!result.cameraOpen);
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("close")
                << QStringLiteral("delay:100")
                << QStringLiteral("open:0"));
}

void DetectionCompletionTest::cameraRecoveryTransitionClosesAfterExposureFailure()
{
    StartTransitionFakeCamera camera;
    InspectionCameraRecoveryRequest request;
    request.cameraDevice = &camera;
    request.recoveryRequired = true;
    request.cameraWasOpen = true;
    request.applySavedExposure = [&camera](
            QString *,
            QString *errorMessage) {
        camera.calls.append(QStringLiteral("exposure"));
        *errorMessage = QStringLiteral("exposure rejected");
        return false;
    };
    request.delayMilliseconds = [&camera](unsigned long milliseconds) {
        camera.calls.append(QStringLiteral("delay:%1").arg(
                                static_cast<qulonglong>(milliseconds)));
    };

    const InspectionCameraRecoveryResult result =
            InspectionCameraRecoveryTransition::apply(request);

    QVERIFY(!result.isRecovered());
    QVERIFY(result.issue
            == InspectionCameraRecoveryIssue::ExposureRejected);
    QVERIFY(!result.cameraOpen);
    QCOMPARE(result.errorMessage,
             QStringLiteral("exposure rejected"));
    QCOMPARE(
                camera.calls,
                QStringList()
                << QStringLiteral("close")
                << QStringLiteral("delay:100")
                << QStringLiteral("open:0")
                << QStringLiteral("enum:TriggerMode=1")
                << QStringLiteral("enum:TriggerSource=7")
                << QStringLiteral("exposure")
                << QStringLiteral("close"));
}

void DetectionCompletionTest::runtimeStopTransactionPreservesStoppingUntilCommit()
{
    InspectionRuntimeController controller;
    QVERIFY(!controller.beginStart().isEmpty());
    QVERIFY(controller.markRunning());

    InspectionRuntimeStopTransaction transaction(controller);
    transaction.begin();
    QVERIFY(transaction.hasBegun());
    QVERIFY(!transaction.isCommitted());
    QVERIFY(controller.state() == InspectionRuntimeState::Stopping);

    transaction.waitForDetectionWorker();
    QVERIFY(transaction.hasWaitedForDetectionWorker());
    QVERIFY(controller.state() == InspectionRuntimeState::Stopping);

    transaction.commit();
    QVERIFY(transaction.isCommitted());
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
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

void DetectionCompletionTest::frameQueueTrySubmitReportsFullWithoutBlocking()
{
    FrameQueue queue(1);
    QCOMPARE(static_cast<int>(queue.trySubmit(
                                  std::shared_ptr<const FrameData>())),
             static_cast<int>(FrameQueueSubmitResult::InvalidItem));
    QCOMPARE(static_cast<int>(queue.trySubmit(workerTestFrame(1))),
             static_cast<int>(FrameQueueSubmitResult::Accepted));
    QCOMPARE(static_cast<int>(queue.trySubmit(workerTestFrame(2))),
             static_cast<int>(FrameQueueSubmitResult::Full));
    QCOMPARE(static_cast<int>(queue.size()), 1);

    std::shared_ptr<const FrameData> frame;
    QVERIFY(queue.waitAndTake(&frame));
    QCOMPARE(frame->productKey.sequence, quint64(1));
    QCOMPARE(static_cast<int>(queue.trySubmit(workerTestFrame(2))),
             static_cast<int>(FrameQueueSubmitResult::Accepted));
    QCOMPARE(static_cast<int>(queue.cancel()), 1);
    QCOMPARE(static_cast<int>(queue.trySubmit(workerTestFrame(3))),
             static_cast<int>(FrameQueueSubmitResult::Cancelled));
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

void DetectionCompletionTest::detectionWorkerTrySubmitExposesHardTriggerOverflow()
{
    std::mutex detectorMutex;
    std::condition_variable detectorEntered;
    std::condition_variable releaseDetector;
    bool firstDetectorEntered = false;
    bool detectorReleased = false;
    std::mutex completionMutex;
    std::condition_variable completionAvailable;
    int completionCount = 0;

    DetectionWorker worker(
                1,
                [&detectorMutex,
                 &detectorEntered,
                 &releaseDetector,
                 &firstDetectorEntered,
                 &detectorReleased](
                    const std::shared_ptr<const FrameData> &) {
        std::unique_lock<std::mutex> lock(detectorMutex);
        if (!firstDetectorEntered) {
            firstDetectorEntered = true;
            detectorEntered.notify_one();
        }
        releaseDetector.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&detectorReleased]() {
            return detectorReleased;
        });
        DetectionResult result;
        result.status = DetectionStatus::Completed;
        result.verdict = AlgorithmVerdict::Ok;
        return result;
    },
    [&completionMutex,
     &completionAvailable,
     &completionCount](const DetectionCompletion &) {
        std::lock_guard<std::mutex> lock(completionMutex);
        ++completionCount;
        completionAvailable.notify_one();
    });

    QVERIFY(worker.start());
    QCOMPARE(static_cast<int>(worker.trySubmit(workerTestFrame(1))),
             static_cast<int>(DetectionWorkSubmissionResult::Accepted));
    {
        std::unique_lock<std::mutex> lock(detectorMutex);
        QVERIFY(detectorEntered.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&firstDetectorEntered]() {
            return firstDetectorEntered;
        }));
    }

    QCOMPARE(static_cast<int>(worker.trySubmit(workerTestFrame(2))),
             static_cast<int>(DetectionWorkSubmissionResult::Accepted));
    QCOMPARE(static_cast<int>(worker.trySubmit(workerTestFrame(3))),
             static_cast<int>(DetectionWorkSubmissionResult::QueueFull));
    QCOMPARE(static_cast<int>(worker.queuedFrameCount()), 1);

    {
        std::lock_guard<std::mutex> lock(detectorMutex);
        detectorReleased = true;
    }
    releaseDetector.notify_all();
    {
        std::unique_lock<std::mutex> lock(completionMutex);
        QVERIFY(completionAvailable.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&completionCount]() {
            return completionCount == 2;
        }));
    }
    worker.requestStop();
    worker.wait();

    QCOMPARE(worker.processedFrameCount(), quint64(2));
    QCOMPARE(worker.cancelledFrameCount(), quint64(0));
    QCOMPARE(static_cast<int>(worker.trySubmit(workerTestFrame(4))),
             static_cast<int>(DetectionWorkSubmissionResult::NotRunning));
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

void DetectionCompletionTest::runtimeControllerOwnsDetectionWorkerLifecycle()
{
    InspectionRuntimeController controller([]() {
        return QStringLiteral("owned-worker-run");
    });
    std::promise<DetectionCompletion> completionPromise;
    std::future<DetectionCompletion> completionFuture =
            completionPromise.get_future();
    const std::shared_ptr<DetectionWorker> worker(
                new DetectionWorker(
                    1,
                    [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.modeId = QStringLiteral("tissue_detection");
        result.verdict = AlgorithmVerdict::Ok;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [&completionPromise](const DetectionCompletion &completion) {
        completionPromise.set_value(completion);
    }));

    QVERIFY(!controller.startDetectionWorker(3, worker));
    QCOMPARE(controller.beginStart(),
             QStringLiteral("owned-worker-run"));
    QVERIFY(!controller.startDetectionWorker(-1, worker));
    QVERIFY(controller.startDetectionWorker(3, worker));
    QVERIFY(controller.isDetectionWorkerActive());
    QVERIFY(controller.isDetectionWorkerActiveForMode(3));
    QVERIFY(!controller.isDetectionWorkerActiveForMode(1));
    QCOMPARE(controller.detectionWorkerModeIndex(), 3);
    QCOMPARE(static_cast<qulonglong>(
                 controller.detectionWorkerQueueCapacity()),
             qulonglong(1));
    QVERIFY(controller.markRunning());

    const std::shared_ptr<const FrameData> frame =
            controller.acceptFrame(
                cv::Mat(2, 2, CV_8UC1, cv::Scalar(7)));
    QVERIFY(frame);
    QVERIFY(controller.submitDetectionFrame(frame));
    QVERIFY(completionFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const DetectionCompletion completion = completionFuture.get();
    QCOMPARE(completion.frame.get(), frame.get());
    QCOMPARE(completion.result.modeId,
             QStringLiteral("tissue_detection"));

    QVERIFY(controller.requestStop());
    QVERIFY(controller.state() == InspectionRuntimeState::Stopping);
    QVERIFY(!controller.isDetectionWorkerActive());
    QCOMPARE(controller.detectionWorkerModeIndex(), -1);
    QVERIFY(!controller.submitDetectionFrame(frame));
    controller.waitForDetectionWorkerStop();
    QVERIFY(!worker->isRunning());
    QCOMPARE(static_cast<qulonglong>(
                 controller.detectionWorkerQueueCapacity()),
             qulonglong(0));
    QCOMPARE(worker->processedFrameCount(), quint64(1));
    QCOMPARE(worker->cancelledFrameCount(), quint64(0));

    controller.finishStop();
    QVERIFY(controller.state() == InspectionRuntimeState::Idle);
}

void DetectionCompletionTest::runtimeControllerSerializesAndCancelsUiCompletion()
{
    InspectionRuntimeController controller;
    QVERIFY(!controller.beginStart().isEmpty());
    const std::shared_ptr<DetectionWorker> worker(
                new DetectionWorker(
                    1,
                    [](const std::shared_ptr<const FrameData> &) {
        DetectionResult result;
        result.status = DetectionStatus::Completed;
        return result;
    },
    [](const DetectionCompletion &) {
    }));
    QVERIFY(controller.startDetectionWorker(1, worker));
    QVERIFY(controller.markRunning());

    std::vector<int> presentedProducts;
    QVERIFY(controller.submitUiCompletion([&presentedProducts]() {
        presentedProducts.push_back(1);
    }));
    std::future<bool> secondSubmission = std::async(
                std::launch::async,
                [&controller, &presentedProducts]() {
        return controller.submitUiCompletion([&presentedProducts]() {
            presentedProducts.push_back(2);
        });
    });
    QVERIFY(secondSubmission.wait_for(
                std::chrono::milliseconds(30))
            == std::future_status::timeout);

    QVERIFY(controller.processOneUiCompletion());
    QCOMPARE(secondSubmission.get(), true);
    QVERIFY(controller.processOneUiCompletion());
    QCOMPARE(static_cast<int>(presentedProducts.size()), 2);
    QCOMPARE(presentedProducts.at(0), 1);
    QCOMPARE(presentedProducts.at(1), 2);

    QVERIFY(controller.submitUiCompletion([]() {}));
    std::future<bool> blockedSubmission = std::async(
                std::launch::async,
                [&controller]() {
        return controller.submitUiCompletion([]() {});
    });
    QVERIFY(blockedSubmission.wait_for(
                std::chrono::milliseconds(30))
            == std::future_status::timeout);

    QVERIFY(controller.requestStop());
    QCOMPARE(blockedSubmission.get(), false);
    QVERIFY(!controller.processOneUiCompletion());
    controller.waitForDetectionWorkerStop();
    QVERIFY(!worker->isRunning());
    controller.finishStop();
}

void DetectionCompletionTest::modeWorkerFactoryCreatesTissueWorker()
{
    std::promise<DetectionCompletion> completionPromise;
    std::future<DetectionCompletion> completionFuture =
            completionPromise.get_future();
    std::promise<TissueRollResult> resultPromise;
    std::future<TissueRollResult> resultFuture =
            resultPromise.get_future();
    TissueRecipeParameters parameters;
    parameters.roughnessThreshold = 6.25;
    const std::shared_ptr<DetectionWorker> worker =
            DetectionModeWorkerFactory::createTissueWorker(
                parameters,
                [&completionPromise, &resultPromise](
                    const DetectionCompletion &completion,
                    const TissueRollResult &result) {
        completionPromise.set_value(completion);
        resultPromise.set_value(result);
    });

    QVERIFY(worker);
    QCOMPARE(static_cast<qulonglong>(worker->queueCapacity()),
             qulonglong(1));
    QVERIFY(worker->start());
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                testProductKey(101),
                101,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat::zeros(128, 128, CV_8UC3));
    QVERIFY(worker->submit(frame));
    QVERIFY(completionFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    QVERIFY(resultFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const DetectionCompletion completion = completionFuture.get();
    const TissueRollResult result = resultFuture.get();
    worker->requestStop();
    worker->wait();

    QCOMPARE(completion.frame.get(), frame.get());
    QCOMPARE(completion.result.modeId,
             QStringLiteral("tissue_detection"));
    QVERIFY(completion.result.status == DetectionStatus::Completed);
    QCOMPARE(result.imageWidth, 128);
    QCOMPARE(result.imageHeight, 128);
    QVERIFY(result.processingTimeMs >= 0);
}

void DetectionCompletionTest::modeWorkerFactoryPreservesOcrPose()
{
    FactoryFakeOcrEngine ocrEngine;
    std::promise<DetectionCompletion> completionPromise;
    std::future<DetectionCompletion> completionFuture =
            completionPromise.get_future();
    std::promise<DetectionPose> posePromise;
    std::future<DetectionPose> poseFuture = posePromise.get_future();
    const std::shared_ptr<DetectionWorker> worker =
            DetectionModeWorkerFactory::createOcrWorker(
                "TARGET",
                &ocrEngine,
                [&completionPromise, &posePromise](
                    const DetectionCompletion &completion,
                    const DetectionPose &pose) {
        completionPromise.set_value(completion);
        posePromise.set_value(pose);
    });

    QVERIFY(worker);
    QVERIFY(worker->start());
    const DetectionWorkItem item =
            factoryPositionedWorkItem(102, false, -1);
    QVERIFY(worker->submit(item));
    QVERIFY(completionFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    QVERIFY(poseFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const DetectionCompletion completion = completionFuture.get();
    const DetectionPose pose = poseFuture.get();
    worker->requestStop();
    worker->wait();

    QCOMPARE(completion.frame.get(), item.frame.get());
    QCOMPARE(completion.result.modeId,
             QStringLiteral("ocr_detection"));
    QVERIFY(completion.result.status == DetectionStatus::Cancelled);
    QCOMPARE(pose.wordTemplateProfileIndex, -1);
    QCOMPARE(ocrEngine.recognizeCalls, 0);
}

void DetectionCompletionTest::modeWorkerFactoryReturnsTypedStampOutput()
{
    StampDetectionWorkerConfiguration configuration;
    configuration.targetText = QStringLiteral("1");
    configuration.thresholdPercent = 70;
    std::promise<DetectionCompletion> completionPromise;
    std::future<DetectionCompletion> completionFuture =
            completionPromise.get_future();
    std::promise<StampDetectionWorkOutput> outputPromise;
    std::future<StampDetectionWorkOutput> outputFuture =
            outputPromise.get_future();
    const std::shared_ptr<DetectionWorker> worker =
            DetectionModeWorkerFactory::createStampWorker(
                configuration,
                [&completionPromise, &outputPromise](
                    const DetectionCompletion &completion,
                    const StampDetectionWorkOutput &output) {
        completionPromise.set_value(completion);
        outputPromise.set_value(output);
    });

    QVERIFY(worker);
    QVERIFY(worker->start());
    const DetectionWorkItem item =
            factoryPositionedWorkItem(103, true, 0);
    QVERIFY(worker->submit(item));
    QVERIFY(completionFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    QVERIFY(outputFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const DetectionCompletion completion = completionFuture.get();
    const StampDetectionWorkOutput output = outputFuture.get();
    worker->requestStop();
    worker->wait();

    QCOMPARE(completion.result.modeId,
             QStringLiteral("stamp_detection"));
    QCOMPARE(output.detectionResult.modeId,
             QStringLiteral("stamp_detection"));
    QVERIFY(output.detectionResult.status
            == DetectionStatus::Completed);
    QVERIFY(output.roiValid);
    QVERIFY(!output.hasOverlapDetection);
    QCOMPARE(output.stampResult.targetCharacterCount, 1);
}

void DetectionCompletionTest::modeWorkerFactoryRejectsInvalidWordProfileIndex()
{
    DetectionModeWorkerProfile profile;
    profile.templateName = QStringLiteral("profile-a");
    profile.targetText = QStringLiteral("1");
    profile.thresholdPercent = 70;
    std::promise<WordDetectionWorkOutput> outputPromise;
    std::future<WordDetectionWorkOutput> outputFuture =
            outputPromise.get_future();
    const std::shared_ptr<DetectionWorker> worker =
            DetectionModeWorkerFactory::createWordWorker(
                std::vector<DetectionModeWorkerProfile>(1, profile),
                [&outputPromise](
                    const DetectionCompletion &,
                    const WordDetectionWorkOutput &output) {
        outputPromise.set_value(output);
    });

    QVERIFY(worker);
    QVERIFY(worker->start());
    const DetectionWorkItem item =
            factoryPositionedWorkItem(104, true, 3);
    QVERIFY(worker->submit(item));
    QVERIFY(outputFuture.wait_for(std::chrono::seconds(2))
            == std::future_status::ready);
    const WordDetectionWorkOutput output = outputFuture.get();
    worker->requestStop();
    worker->wait();

    QCOMPARE(output.pose.wordTemplateProfileIndex, 3);
    QCOMPARE(output.detectionResult.modeId,
             QStringLiteral("word_detection"));
    QVERIFY(output.detectionResult.status
            == DetectionStatus::Cancelled);
    QCOMPARE(output.detectionResult.diagnostic,
             QStringLiteral("Invalid word profile index"));
}

void DetectionCompletionTest::modeWorkerFactoryCarriesBarcodeStrategyAcrossFrames()
{
    FactoryFakeBarcodeDecoder decoder;
    DetectionModeWorkerProfile profile;
    profile.templateName = QStringLiteral("profile-a");
    profile.targetText = QStringLiteral("1");
    profile.thresholdPercent = 70;
    profile.decodeStrategy.preferredStrategyId = 9;
    profile.decodeStrategy.preferredOptionFlags = 7u;
    profile.decodeStrategy.consecutiveFailures = 2;

    std::mutex outputMutex;
    std::condition_variable outputAvailable;
    std::vector<BarcodeWordDetectionWorkOutput> outputs;
    const std::shared_ptr<DetectionWorker> worker =
            DetectionModeWorkerFactory::createBarcodeWordWorker(
                std::vector<DetectionModeWorkerProfile>(1, profile),
                &decoder,
                [&outputMutex, &outputAvailable, &outputs](
                    const DetectionCompletion &,
                    const BarcodeWordDetectionWorkOutput &output) {
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            outputs.push_back(output);
        }
        outputAvailable.notify_all();
    });

    QVERIFY(worker);
    QVERIFY(worker->start());
    QVERIFY(worker->submit(
                factoryPositionedWorkItem(105, true, 0)));
    QVERIFY(worker->submit(
                factoryPositionedWorkItem(106, true, 0)));
    {
        std::unique_lock<std::mutex> lock(outputMutex);
        QVERIFY(outputAvailable.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&outputs]() {
            return outputs.size() == 2;
        }));
    }
    worker->requestStop();
    worker->wait();

    QCOMPARE(decoder.decodeCalls, 2);
    QCOMPARE(static_cast<int>(decoder.receivedStrategyIds.size()), 2);
    QCOMPARE(decoder.receivedStrategyIds.at(0), 9);
    QCOMPARE(decoder.receivedOptionFlags.at(0), 7u);
    QCOMPARE(decoder.receivedStrategyIds.at(1), -1);
    QCOMPARE(decoder.receivedOptionFlags.at(1),
             static_cast<unsigned int>(BARCODE_DECODER_OPTION_NONE));
    QCOMPARE(outputs.at(0).nextDecodeStrategy.consecutiveFailures, 3);
    QCOMPARE(outputs.at(1).nextDecodeStrategy.consecutiveFailures, 3);
    QVERIFY(outputs.at(0).detectionResult.status
            == DetectionStatus::Completed);
    QVERIFY(outputs.at(0).detectionResult.verdict
            == AlgorithmVerdict::Ng);
}

void DetectionCompletionTest::profileSnapshotPreservesOrderAndOwnsImages()
{
    InspectionProfileSource first;
    first.directoryPath = QStringLiteral("C:/recipes/profile-a");
    first.trackingTemplate = cv::Mat(
                3, 4, CV_8UC1, cv::Scalar(17));
    first.datePoly.push_back(cv::Point2f(1.0f, 2.0f));
    first.targetText = QStringLiteral("12");
    first.imageThreshold = 73.0;
    first.digitTemplates.push_back(cv::Mat(
                2, 2, CV_8UC1, cv::Scalar(23)));
    first.digitTemplateTargetIndexes.push_back(0);

    InspectionProfileSource second;
    second.name = QStringLiteral("profile-b");
    second.directoryPath = QStringLiteral("C:/recipes/ignored");
    second.trackingTemplate = cv::Mat(
                2, 5, CV_8UC1, cv::Scalar(31));
    second.targetText = QStringLiteral("3");
    second.imageThreshold = 82.0;
    second.digitTemplates.push_back(cv::Mat(
                2, 2, CV_8UC1, cv::Scalar(41)));
    second.digitTemplateTargetIndexes.push_back(0);

    std::vector<InspectionProfileSource> sources;
    sources.push_back(first);
    sources.push_back(second);
    const InspectionProfileSnapshot snapshot =
            InspectionProfileSnapshotBuilder::create(
                sources,
                QStringLiteral("66"));

    sources[0].trackingTemplate.setTo(cv::Scalar(99));
    sources[0].digitTemplates[0].setTo(cv::Scalar(101));

    QVERIFY(snapshot.isValid());
    QCOMPARE(static_cast<int>(snapshot.trackingProfiles.size()), 2);
    QCOMPARE(static_cast<int>(snapshot.detectionProfiles.size()), 2);
    QCOMPARE(snapshot.trackingProfiles[0].name,
             QStringLiteral("profile-a"));
    QCOMPARE(snapshot.trackingProfiles[1].name,
             QStringLiteral("profile-b"));
    QCOMPARE(snapshot.trackingProfiles[0].profileIndex, 0);
    QCOMPARE(snapshot.trackingProfiles[1].profileIndex, 1);
    QCOMPARE(snapshot.trackingProfiles[0].trackingTemplate.at<uchar>(0, 0),
             uchar(17));
    QCOMPARE(snapshot.detectionProfiles[0]
             .preparedTemplates.grayTemplates[0].at<uchar>(0, 0),
             uchar(23));
    QCOMPARE(snapshot.detectionProfiles[0].targetText,
             QStringLiteral("12"));
    QCOMPARE(snapshot.detectionProfiles[1].thresholdPercent, 82);
}

void DetectionCompletionTest::profileSnapshotUsesLegacyThresholdFallback()
{
    InspectionProfileSource source;
    source.name = QStringLiteral("profile-a");
    source.trackingTemplate = cv::Mat::ones(2, 2, CV_8UC1);
    source.targetText = QStringLiteral("1");
    source.imageThreshold = 78.5;
    source.digitTemplates.push_back(
                cv::Mat::ones(2, 2, CV_8UC1));
    source.digitTemplateTargetIndexes.push_back(0);
    source.barcodeOptions.roiPaddingPercent = 11;
    source.decodeStrategy.preferredStrategyId = 4;
    source.decodeStrategy.preferredOptionFlags = 7u;
    source.decodeStrategy.consecutiveFailures = 2;

    const InspectionProfileSnapshot snapshot =
            InspectionProfileSnapshotBuilder::create(
                std::vector<InspectionProfileSource>(1, source),
                QStringLiteral("66"));

    QVERIFY(snapshot.isValid());
    QCOMPARE(snapshot.detectionProfiles[0].thresholdPercent, 66);
    QCOMPARE(snapshot.detectionProfiles[0]
             .barcodeOptions.roiPaddingPercent, 11);
    QCOMPARE(snapshot.detectionProfiles[0]
             .decodeStrategy.preferredStrategyId, 4);
    QCOMPARE(snapshot.detectionProfiles[0]
             .decodeStrategy.preferredOptionFlags, 7u);
    QCOMPARE(snapshot.detectionProfiles[0]
             .decodeStrategy.consecutiveFailures, 2);
}

void DetectionCompletionTest::modeWorkerDispatcherRejectsInvalidRequests()
{
    DetectionModeWorkerConsumers consumers;
    DetectionModeWorkerRequest request;
    request.modeIndex = 99;
    DetectionModeWorkerCreationResult result =
            DetectionModeWorkerDispatcher::create(request, consumers);
    QVERIFY(!result.isAccepted());
    QVERIFY(result.errorMessage.contains(
                QStringLiteral("\u4e0d\u652f\u6301")));

    request.modeIndex = 1;
    result = DetectionModeWorkerDispatcher::create(request, consumers);
    QVERIFY(!result.isAccepted());
    QVERIFY(result.errorMessage.contains(QStringLiteral("Profile")));

    request.modeIndex = 2;
    result = DetectionModeWorkerDispatcher::create(request, consumers);
    QVERIFY(!result.isAccepted());
    QVERIFY(result.errorMessage.contains(QStringLiteral("OCR")));

    request.modeIndex = 4;
    request.profiles.push_back(DetectionModeWorkerProfile());
    result = DetectionModeWorkerDispatcher::create(request, consumers);
    QVERIFY(!result.isAccepted());
    QCOMPARE(result.errorMessage,
             QStringLiteral("Barcode decoder is null"));
}

void DetectionCompletionTest::modeWorkerDispatcherCreatesRequestedWorker()
{
    DetectionModeWorkerRequest request;
    request.modeIndex = 3;
    request.tissueParameters.roughnessThreshold = 6.25;
    DetectionModeWorkerConsumers consumers;
    consumers.tissue = [](
            const DetectionCompletion &,
            const TissueRollResult &) {
    };

    const DetectionModeWorkerCreationResult result =
            DetectionModeWorkerDispatcher::create(request, consumers);

    QVERIFY(result.isAccepted());
    QVERIFY(result.worker);
    QCOMPARE(result.workerLogName, QStringLiteral("tissue"));
    QVERIFY(result.startFailureMessage.contains(
                QStringLiteral("\u7eb8\u5dfe")));
    QCOMPARE(static_cast<qulonglong>(result.worker->queueCapacity()),
             qulonglong(1));
}

void DetectionCompletionTest::modeWorkerDispatcherLoadsBarcodeDecoderBeforeCreation()
{
    FactoryFakeBarcodeDecoder decoder;
    DetectionModeWorkerProfile profile;
    profile.templateName = QStringLiteral("profile-a");
    profile.targetText = QStringLiteral("1");

    DetectionModeWorkerRequest request;
    request.modeIndex = 4;
    request.profiles.push_back(profile);
    request.barcodeDecoder = &decoder;
    DetectionModeWorkerConsumers consumers;
    consumers.barcodeWord = [](
            const DetectionCompletion &,
            const BarcodeWordDetectionWorkOutput &) {
    };

    const DetectionModeWorkerCreationResult result =
            DetectionModeWorkerDispatcher::create(request, consumers);

    QVERIFY(result.isAccepted());
    QCOMPARE(decoder.ensureLoadedCalls, 1);
    QCOMPARE(result.workerLogName, QStringLiteral("barcode-word"));
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

void DetectionCompletionTest::resultPresenterRendersDetectionOverlays()
{
    DetectionResult result;
    DetectionOverlayPolygon character;
    character.role = QStringLiteral("character");
    character.points = {
        cv::Point(20, 20),
        cv::Point(26, 20),
        cv::Point(26, 26),
        cv::Point(20, 26)
    };
    character.score = -1.0;
    result.overlay.polygons.push_back(character);

    DetectionOverlayPolygon stamp;
    stamp.role = QStringLiteral("stamp");
    stamp.points = {
        cv::Point(32, 32),
        cv::Point(38, 32),
        cv::Point(38, 38),
        cv::Point(32, 38)
    };
    result.overlay.polygons.push_back(stamp);

    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = cv::Point2f(8.0f, 8.0f);
    pose.trackingPoly = {
        cv::Point(2, 2),
        cv::Point(8, 2),
        cv::Point(8, 8),
        cv::Point(2, 8)
    };
    pose.barcodePoly = {
        cv::Point(42, 2),
        cv::Point(48, 2),
        cv::Point(48, 8),
        cv::Point(42, 8)
    };
    pose.datePoly = {
        cv::Point(2, 42),
        cv::Point(8, 42),
        cv::Point(8, 48),
        cv::Point(2, 48)
    };

    DetectionResultPresenter presenter;
    presenter.installDetectionResult(result, pose, true);
    const QImage rendered = presenter.renderFrame(
                cv::Mat(60, 60, CV_8UC3, cv::Scalar(0, 0, 0)),
                false);

    QVERIFY(!rendered.isNull());
    QCOMPARE(rendered.size(), QSize(60, 60));
    QCOMPARE(rendered.pixelColor(2, 2), QColor(Qt::blue));
    QCOMPARE(rendered.pixelColor(20, 20), QColor(Qt::green));
    QCOMPARE(rendered.pixelColor(32, 32), QColor(Qt::red));
    QCOMPARE(rendered.pixelColor(42, 2), QColor(Qt::yellow));

    const QImage fourChannelRendered = presenter.renderFrame(
                cv::Mat(60, 60, CV_8UC4, cv::Scalar(0, 0, 0, 255)),
                false);
    QVERIFY(!fourChannelRendered.isNull());
    QCOMPARE(fourChannelRendered.size(), QSize(60, 60));
}

void DetectionCompletionTest::resultPresenterMovesDetailsWithPose()
{
    DetectionResult result;
    DetectionOverlayPolygon character;
    character.role = QStringLiteral("character");
    character.points = {
        cv::Point(10, 10),
        cv::Point(14, 10),
        cv::Point(14, 14),
        cv::Point(10, 14)
    };
    result.overlay.polygons.push_back(character);

    DetectionPose initialPose;
    initialPose.valid = true;
    initialPose.anchorCenter = cv::Point2f(10.0f, 10.0f);

    DetectionResultPresenter presenter;
    presenter.installDetectionResult(result, initialPose);

    DetectionPose movedPose = initialPose;
    movedPose.anchorCenter = cv::Point2f(15.0f, 12.0f);
    presenter.updatePose(movedPose);

    QCOMPARE(
                presenter.state().detailPolygons.front().points.front().x,
                15);
    QCOMPARE(
                presenter.state().detailPolygons.front().points.front().y,
                12);
    QCOMPARE(presenter.state().pose.anchorCenter.x, 15.0f);
    QCOMPARE(presenter.state().pose.anchorCenter.y, 12.0f);

    presenter.updatePose(DetectionPose());
    QVERIFY(presenter.state().detailPolygons.empty());
    QVERIFY(!presenter.state().pose.valid);
}

void DetectionCompletionTest::resultPresenterRendersAndClearsTissueOverlay()
{
    TissueRollPresentation tissueRoll;
    tissueRoll.center = cv::Point2f(20.0f, 20.0f);
    tissueRoll.outerAxes = cv::Size2f(5.0f, 5.0f);
    tissueRoll.innerCenter = cv::Point2f(20.0f, 20.0f);
    tissueRoll.innerAxes = cv::Size2f(2.0f, 2.0f);

    DetectionResultPresenter presenter;
    presenter.installTissueRoll(tissueRoll, true);
    QVERIFY(presenter.state().hasTissueRoll);

    const QImage rendered = presenter.renderFrame(
                cv::Mat(50, 50, CV_8UC3, cv::Scalar(0, 0, 0)),
                true);
    QVERIFY(!rendered.isNull());
    QCOMPARE(rendered.pixelColor(25, 20), QColor(Qt::yellow));
    QCOMPARE(rendered.pixelColor(22, 20), QColor(Qt::blue));

    presenter.clear();
    QVERIFY(!presenter.state().hasTissueRoll);
    QVERIFY(presenter.state().detailPolygons.empty());
}

void DetectionCompletionTest::resultPresenterAppliesWholeViewSnapshotInOrder()
{
    QStringList applicationOrder;
    QImage presentedImage;
    DetectionVerdictViewStyle presentedStyle =
            DetectionVerdictViewStyle::Error;
    QString verdictText;
    QString recognitionText;
    QString templateName;
    int totalCount = -1;
    int ngCount = -1;
    double passRate = -1.0;
    QString elapsedText;

    DetectionResultViewBindings bindings;
    bindings.showImage = [&](const QImage &image) {
        applicationOrder.append(QStringLiteral("image"));
        presentedImage = image;
    };
    bindings.showVerdictStyle = [&](DetectionVerdictViewStyle style) {
        applicationOrder.append(QStringLiteral("style"));
        presentedStyle = style;
    };
    bindings.showVerdictText = [&](const QString &text) {
        applicationOrder.append(QStringLiteral("verdict"));
        verdictText = text;
    };
    bindings.showRecognitionText = [&](const QString &text) {
        applicationOrder.append(QStringLiteral("recognition"));
        recognitionText = text;
    };
    bindings.showTemplateName = [&](const QString &text) {
        applicationOrder.append(QStringLiteral("template"));
        templateName = text;
    };
    bindings.showTotalCount = [&](int count) {
        applicationOrder.append(QStringLiteral("total"));
        totalCount = count;
    };
    bindings.showNgCount = [&](int count) {
        applicationOrder.append(QStringLiteral("ng"));
        ngCount = count;
    };
    bindings.showPassRate = [&](double value) {
        applicationOrder.append(QStringLiteral("rate"));
        passRate = value;
    };
    bindings.showElapsedText = [&](const QString &text) {
        applicationOrder.append(QStringLiteral("elapsed"));
        elapsedText = text;
    };

    DetectionResultPresenter presenter;
    QVERIFY(!presenter.hasViewBindings());
    presenter.bindView(bindings);
    QVERIFY(presenter.hasViewBindings());

    DetectionResultViewSnapshot snapshot;
    snapshot.productKey.runId = QStringLiteral("view-run");
    snapshot.productKey.sequence = 9;
    snapshot.image = QImage(4, 3, QImage::Format_RGB888);
    snapshot.verdictStyle = DetectionVerdictViewStyle::Error;
    snapshot.recognitionText =
            QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a8.250");
    snapshot.updatesTemplateName = true;
    snapshot.templateName = QStringLiteral("profile-a");
    snapshot.statistics.totalCount = 5;
    snapshot.statistics.ngCount = 2;
    snapshot.elapsedText = QStringLiteral(
                "\u68c0\u6d4b\u8017\u65f6 12 \u6beb\u79d2");

    QVERIFY(presenter.present(snapshot));
    QStringList expectedOrder;
    expectedOrder
            << QStringLiteral("image")
            << QStringLiteral("style")
            << QStringLiteral("verdict")
            << QStringLiteral("recognition")
            << QStringLiteral("template")
            << QStringLiteral("total")
            << QStringLiteral("ng")
            << QStringLiteral("rate")
            << QStringLiteral("elapsed");
    QCOMPARE(applicationOrder, expectedOrder);
    QCOMPARE(presentedImage.size(), QSize(4, 3));
    QVERIFY(presentedStyle == DetectionVerdictViewStyle::Error);
    QCOMPARE(
                verdictText,
                QString::fromWCharArray(L"\u9519\u8bef"));
    QCOMPARE(
                recognitionText,
                QString::fromWCharArray(L"\u7c97\u7cd9\u5ea6\uff1a8.250"));
    QCOMPARE(templateName, QStringLiteral("profile-a"));
    QCOMPARE(totalCount, 5);
    QCOMPARE(ngCount, 2);
    QCOMPARE(passRate, 60.0);
    QCOMPARE(
                elapsedText,
                QString::fromWCharArray(
                    L"\u68c0\u6d4b\u8017\u65f6 12 \u6beb\u79d2"));
    QCOMPARE(
                presenter.lastPresentedProductKey().runId,
                QStringLiteral("view-run"));
    QCOMPARE(presenter.lastPresentedProductKey().sequence, quint64(9));

    applicationOrder.clear();
    snapshot.productKey.sequence = 10;
    snapshot.verdictStyle = DetectionVerdictViewStyle::Correct;
    snapshot.recognitionText.clear();
    snapshot.updatesTemplateName = false;
    snapshot.elapsedText = QStringLiteral(
                "\u68c0\u6d4b\u8017\u65f6 9 \u6beb\u79d2");
    QVERIFY(presenter.present(snapshot));
    QVERIFY(presentedStyle == DetectionVerdictViewStyle::Correct);
    QCOMPARE(
                verdictText,
                QString::fromWCharArray(L"\u6b63\u786e"));
    QCOMPARE(
                elapsedText,
                QString::fromWCharArray(
                    L"\u68c0\u6d4b\u8017\u65f6 9 \u6beb\u79d2"));
    QVERIFY(!applicationOrder.contains(QStringLiteral("template")));
    QCOMPARE(presenter.lastPresentedProductKey().sequence, quint64(10));
}

void DetectionCompletionTest::resultPresenterPreservesPartialRefreshRules()
{
    int imageCount = 0;
    int templateCount = 0;
    int totalCount = -1;
    int ngCount = -1;
    int passRateCount = 0;
    QString verdictText = QStringLiteral("unchanged");
    QString recognitionText = QStringLiteral("unchanged");
    QString elapsedText = QStringLiteral("unchanged");

    DetectionResultViewBindings bindings;
    bindings.showImage = [&](const QImage &) { ++imageCount; };
    bindings.showVerdictStyle = [](DetectionVerdictViewStyle) {};
    bindings.showVerdictText = [&](const QString &text) {
        verdictText = text;
    };
    bindings.showRecognitionText = [&](const QString &text) {
        recognitionText = text;
    };
    bindings.showTemplateName = [&](const QString &) { ++templateCount; };
    bindings.showTotalCount = [&](int count) { totalCount = count; };
    bindings.showNgCount = [&](int count) { ngCount = count; };
    bindings.showPassRate = [&](double) { ++passRateCount; };
    bindings.showElapsedText = [&](const QString &text) {
        elapsedText = text;
    };

    DetectionResultPresenter presenter;
    presenter.bindView(bindings);

    DetectionResultViewSnapshot invalidSnapshot;
    invalidSnapshot.image = QImage(2, 2, QImage::Format_RGB888);
    QVERIFY(!presenter.present(invalidSnapshot));
    QCOMPARE(imageCount, 0);

    QVERIFY(!presenter.presentFrame(QImage()));
    QVERIFY(presenter.presentFrame(QImage(2, 2, QImage::Format_RGB888)));
    QCOMPARE(imageCount, 1);

    presenter.clearTransientView();
    QVERIFY(verdictText.isEmpty());
    QVERIFY(recognitionText.isEmpty());
    QVERIFY(elapsedText.isEmpty());

    presenter.presentTotalAndNgCounts(7, 3);
    QCOMPARE(totalCount, 7);
    QCOMPARE(ngCount, 3);
    QCOMPARE(passRateCount, 0);

    presenter.presentNgCount(1);
    QCOMPARE(totalCount, 7);
    QCOMPARE(ngCount, 1);
    QCOMPARE(templateCount, 0);

    presenter.clear();
    QVERIFY(presenter.hasViewBindings());
    QVERIFY(!presenter.lastPresentedProductKey().isValid());
    QVERIFY(presenter.presentFrame(QImage(3, 3, QImage::Format_RGB888)));
    QCOMPARE(imageCount, 2);
}

void DetectionCompletionTest::completionControllerRejectsInvalidRequestWithoutSideEffects()
{
    InspectionRuntimeController runtimeController;
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter);
    int plcRequests = 0;
    int presentationPreparations = 0;
    DetectionCompletionControllerCallbacks callbacks;
    callbacks.requestPlc = [&plcRequests](
            DetectionPlcAction,
            const ProductKey &) {
        ++plcRequests;
    };
    DetectionCompletionController controller(
                &runtimeController,
                nullptr,
                &presenter,
                callbacks);

    DetectionCompletionProcessRequest request;
    request.preparePresentation = [&presentationPreparations]() {
        ++presentationPreparations;
        return DetectionResultViewSnapshot();
    };
    const DetectionCompletionProcessOutcome outcome =
            controller.process(request);

    QVERIFY(!outcome.resultRecorded);
    QCOMPARE(plcRequests, 0);
    QCOMPARE(presentationPreparations, 0);
    QCOMPARE(runtimeController.totalCount(), 0);
}

void DetectionCompletionTest::completionControllerPreservesDelayedNgAndCurrentPlcOrder()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-order-run");
    });
    QCOMPARE(runtimeController.beginStart(),
             QStringLiteral("completion-order-run"));
    QVERIFY(runtimeController.markRunning());

    QStringList events;
    QList<ProductKey> plcProductKeys;
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter, &events);
    DetectionCompletionControllerCallbacks callbacks;
    callbacks.requestPlc = [&events, &plcProductKeys](
            DetectionPlcAction action,
            const ProductKey &productKey) {
        events.append(action == DetectionPlcAction::RequestNg
                      ? QStringLiteral("plc-ng")
                      : QStringLiteral("plc-ok"));
        plcProductKeys.append(productKey);
    };
    DetectionCompletionController controller(
                &runtimeController,
                nullptr,
                &presenter,
                callbacks);

    DetectionCompletionProcessRequest first = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ng));
    first.delayedNgOffset = 1;
    first.preparePresentation = [&events]() {
        events.append(QStringLiteral("prepare-1"));
        DetectionResultViewSnapshot snapshot;
        snapshot.image = QImage(2, 2, QImage::Format_RGB32);
        snapshot.image.fill(Qt::white);
        return snapshot;
    };
    const DetectionCompletionProcessOutcome firstOutcome =
            controller.process(first);
    QVERIFY(firstOutcome.resultRecorded);
    QCOMPARE(events,
             QStringList()
             << QStringLiteral("prepare-1")
             << QStringLiteral("present"));

    events.clear();
    DetectionCompletionProcessRequest second = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ok));
    second.preparePresentation = [&events]() {
        events.append(QStringLiteral("prepare-2"));
        DetectionResultViewSnapshot snapshot;
        snapshot.image = QImage(2, 2, QImage::Format_RGB32);
        snapshot.image.fill(Qt::white);
        return snapshot;
    };
    second.finalizePresentation = [&events](
            DetectionResultViewSnapshot *) {
        events.append(QStringLiteral("finalize-2"));
    };
    const DetectionCompletionProcessOutcome secondOutcome =
            controller.process(second);

    QVERIFY(secondOutcome.resultRecorded);
    QVERIFY(secondOutcome.delayedNgRequested);
    QCOMPARE(secondOutcome.statistics.totalCount, 2);
    QCOMPARE(secondOutcome.statistics.ngCount, 1);
    QCOMPARE(events,
             QStringList()
             << QStringLiteral("plc-ng")
             << QStringLiteral("prepare-2")
             << QStringLiteral("finalize-2")
              << QStringLiteral("present")
              << QStringLiteral("plc-ok"));
    QCOMPARE(plcProductKeys.size(), 2);
    QCOMPARE(plcProductKeys[0].sequence, quint64(1));
    QCOMPARE(plcProductKeys[1].sequence, quint64(2));
}

void DetectionCompletionTest::completionControllerSavesAnnotatedThenRaw()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-save-run");
    });
    QVERIFY(!runtimeController.beginStart().isEmpty());
    QVERIFY(runtimeController.markRunning());

    std::vector<ImageSaveItem> writtenItems;
    std::mutex writtenMutex;
    ImageSaveService saveService(
                4,
                [&writtenItems, &writtenMutex](
                    const ImageSaveItem &item,
                    QString *) {
        std::lock_guard<std::mutex> lock(writtenMutex);
        writtenItems.push_back(item);
        return true;
    },
    1);
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter);
    DetectionCompletionController controller(
                &runtimeController,
                &saveService,
                &presenter);

    DetectionCompletionProcessRequest request = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ok));
    request.imageSaveModeIndex = 3;
    request.saveOptions.rootDirectory = QStringLiteral("C:/capture");
    request.saveOptions.format = QStringLiteral(".PNG");
    request.saveOptions.imageContentModeIndex = 0;
    const DetectionCompletionProcessOutcome outcome =
            controller.process(request);

    QVERIFY(outcome.resultRecorded);
    QVERIFY(outcome.imageSaveRequested);
    QVERIFY(outcome.imageSaveSubmitted);
    QTRY_VERIFY(saveService.outstandingTaskCount() == 0);
    std::lock_guard<std::mutex> lock(writtenMutex);
    QCOMPARE(static_cast<int>(writtenItems.size()), 2);
    QVERIFY(!writtenItems[0].image.isNull());
    QVERIFY(!writtenItems[0].frame);
    QVERIFY(writtenItems[0].filePath.contains(QStringLiteral("/ok/")));
    QVERIFY(writtenItems[0].format == QByteArrayLiteral("PNG"));
    QVERIFY(writtenItems[1].image.isNull());
    QVERIFY(writtenItems[1].frame);
    QVERIFY(writtenItems[1].filePath.contains(QStringLiteral("/ok_raw/")));
    QCOMPARE(QFileInfo(writtenItems[0].filePath).fileName(),
             QFileInfo(writtenItems[1].filePath).fileName());
}

void DetectionCompletionTest::completionControllerPreservesAnnotatedAndRawSelections()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-selection-run");
    });
    QVERIFY(!runtimeController.beginStart().isEmpty());
    QVERIFY(runtimeController.markRunning());

    std::vector<ImageSaveItem> writtenItems;
    std::mutex writtenMutex;
    ImageSaveService saveService(
                4,
                [&writtenItems, &writtenMutex](
                    const ImageSaveItem &item,
                    QString *) {
        std::lock_guard<std::mutex> lock(writtenMutex);
        writtenItems.push_back(item);
        return true;
    },
    1);
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter);
    DetectionCompletionController controller(
                &runtimeController,
                &saveService,
                &presenter);

    DetectionCompletionProcessRequest annotated = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ok));
    annotated.imageSaveModeIndex = 3;
    annotated.saveOptions.rootDirectory = QStringLiteral("C:/capture");
    annotated.saveOptions.imageContentModeIndex = 1;
    QVERIFY(controller.process(annotated).imageSaveSubmitted);

    DetectionCompletionProcessRequest raw = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ng));
    raw.imageSaveModeIndex = 3;
    raw.saveOptions.rootDirectory = QStringLiteral("C:/capture");
    raw.saveOptions.imageContentModeIndex = 2;
    QVERIFY(controller.process(raw).imageSaveSubmitted);

    QTRY_VERIFY(saveService.outstandingTaskCount() == 0);
    std::lock_guard<std::mutex> lock(writtenMutex);
    QCOMPARE(static_cast<int>(writtenItems.size()), 2);
    QVERIFY(!writtenItems[0].image.isNull());
    QVERIFY(!writtenItems[0].frame);
    QVERIFY(writtenItems[0].filePath.contains(QStringLiteral("/ok/")));
    QVERIFY(writtenItems[1].image.isNull());
    QVERIFY(writtenItems[1].frame);
    QVERIFY(writtenItems[1].filePath.contains(QStringLiteral("/ng_raw/")));
}

void DetectionCompletionTest::completionControllerPreservesOcrRawOnlyLayout()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-ocr-run");
    });
    QVERIFY(!runtimeController.beginStart().isEmpty());
    QVERIFY(runtimeController.markRunning());

    std::vector<ImageSaveItem> writtenItems;
    std::mutex writtenMutex;
    ImageSaveService saveService(
                2,
                [&writtenItems, &writtenMutex](
                    const ImageSaveItem &item,
                    QString *) {
        std::lock_guard<std::mutex> lock(writtenMutex);
        writtenItems.push_back(item);
        return true;
    },
    1);
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter);
    DetectionCompletionController controller(
                &runtimeController,
                &saveService,
                &presenter);

    DetectionCompletionProcessRequest request = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ng));
    request.imageSaveModeIndex = 3;
    request.saveOptions.layout = DetectionCompletionSaveLayout::RawOnly;
    request.saveOptions.rootDirectory = QStringLiteral("C:/ocr");
    request.saveOptions.format = QStringLiteral(".JPG");
    request.saveOptions.imageContentModeIndex = 1;
    QVERIFY(controller.process(request).imageSaveSubmitted);

    QTRY_VERIFY(saveService.outstandingTaskCount() == 0);
    std::lock_guard<std::mutex> lock(writtenMutex);
    QCOMPARE(static_cast<int>(writtenItems.size()), 1);
    QVERIFY(writtenItems[0].image.isNull());
    QVERIFY(writtenItems[0].frame);
    QVERIFY(writtenItems[0].filePath.contains(QStringLiteral("/ng/")));
    QVERIFY(!writtenItems[0].filePath.contains(QStringLiteral("_raw")));
    QVERIFY(writtenItems[0].format == QByteArrayLiteral("JPG"));
    QVERIFY(QRegularExpression(
                QStringLiteral("\\d{8}-\\d{6}-\\d{3}\\.jpg$"))
            .match(writtenItems[0].filePath)
            .hasMatch());
}

void DetectionCompletionTest::completionControllerWarnsForMissingAnnotatedImage()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-warning-run");
    });
    QVERIFY(!runtimeController.beginStart().isEmpty());
    QVERIFY(runtimeController.markRunning());

    ImageSaveService saveService(2);
    DetectionResultPresenter presenter;
    bindTestResultView(&presenter);
    int warnings = 0;
    QList<DetectionPlcAction> plcActions;
    DetectionCompletionControllerCallbacks callbacks;
    callbacks.warnMissingAnnotatedImage = [&warnings]() {
        ++warnings;
    };
    callbacks.requestPlc = [&plcActions](
            DetectionPlcAction action,
            const ProductKey &) {
        plcActions.append(action);
    };
    DetectionCompletionController controller(
                &runtimeController,
                &saveService,
                &presenter,
                callbacks);

    DetectionCompletionProcessRequest request;
    request.completion = acceptedControllerCompletion(
                &runtimeController,
                AlgorithmVerdict::Ok);
    request.imageSaveModeIndex = 3;
    request.saveOptions.rootDirectory = QStringLiteral("C:/capture");
    request.saveOptions.imageContentModeIndex = 1;
    request.preparePresentation = []() {
        return DetectionResultViewSnapshot();
    };
    const DetectionCompletionProcessOutcome outcome =
            controller.process(request);

    QVERIFY(outcome.resultRecorded);
    QVERIFY(outcome.imageSaveRequested);
    QVERIFY(!outcome.imageSaveSubmitted);
    QVERIFY(!outcome.presentationAccepted);
    QCOMPARE(warnings, 1);
    QCOMPARE(plcActions.size(), 1);
    QVERIFY(plcActions.front() == DetectionPlcAction::RequestOk);
}

void DetectionCompletionTest::completionControllerRejectsDuplicateBeforeRepeatedSideEffects()
{
    InspectionRuntimeController runtimeController([]() {
        return QStringLiteral("completion-duplicate-run");
    });
    QVERIFY(!runtimeController.beginStart().isEmpty());
    QVERIFY(runtimeController.markRunning());

    int presentations = 0;
    DetectionResultViewBindings bindings;
    bindings.showImage = [&presentations](const QImage &) {
        ++presentations;
    };
    bindings.showVerdictStyle = [](DetectionVerdictViewStyle) {
    };
    bindings.showVerdictText = [](const QString &) {
    };
    bindings.showRecognitionText = [](const QString &) {
    };
    bindings.showTemplateName = [](const QString &) {
    };
    bindings.showTotalCount = [](int) {
    };
    bindings.showNgCount = [](int) {
    };
    bindings.showPassRate = [](double) {
    };
    bindings.showElapsedText = [](const QString &) {
    };
    DetectionResultPresenter presenter;
    presenter.bindView(bindings);
    int plcRequests = 0;
    DetectionCompletionControllerCallbacks callbacks;
    callbacks.requestPlc = [&plcRequests](
            DetectionPlcAction,
            const ProductKey &) {
        ++plcRequests;
    };
    DetectionCompletionController controller(
                &runtimeController,
                nullptr,
                &presenter,
                callbacks);

    int preparations = 0;
    DetectionCompletionProcessRequest request = completionProcessRequest(
                acceptedControllerCompletion(
                    &runtimeController,
                    AlgorithmVerdict::Ok));
    request.preparePresentation = [&preparations]() {
        ++preparations;
        DetectionResultViewSnapshot snapshot;
        snapshot.image = QImage(2, 2, QImage::Format_RGB32);
        snapshot.image.fill(Qt::white);
        return snapshot;
    };
    QVERIFY(controller.process(request).resultRecorded);
    QVERIFY(!controller.process(request).resultRecorded);

    QCOMPARE(preparations, 2);
    QCOMPARE(presentations, 1);
    QCOMPARE(plcRequests, 1);
    QCOMPARE(runtimeController.totalCount(), 1);
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
