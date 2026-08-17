#include <QtTest>

#include <QCoreApplication>
#include <QDirIterator>
#include <QTemporaryDir>

#include "devices/barcode/barcode_decoder.h"
#include "devices/ocr/ocr_engine.h"
#include "devices/plc/plc_device.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_runtime.h"
#include "runtime/pipeline_registry.h"
#include "system_support/settings/machine_settings.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

namespace {

class FakeOcrEngine : public IOcrEngine
{
public:
    std::vector<std::string> recognize(cv::Mat &) override
    {
        ++recognizeCalls;
        return std::vector<std::string>(1, "1");
    }

    int recognizeCalls = 0;
};

class FakeBarcodeDecoder : public IBarcodeDecoder
{
public:
    bool ensureLoaded() override
    {
        ++ensureLoadedCalls;
        return loadSucceeds;
    }

    QString lastError() const override
    {
        return loadSucceeds
                ? QString()
                : QStringLiteral("decoder unavailable");
    }

    BarcodeReadResult decode(
        const cv::Mat &,
        const BarcodeDecodeOptions &,
        int,
        unsigned int,
        int *,
        unsigned int *) override
    {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::NotFound;
        return result;
    }

    bool loadSucceeds = true;
    int ensureLoadedCalls = 0;
};

struct PlcWrite
{
    int dbNumber = 0;
    int start = 0;
    PlcDataWidth width = PlcDataWidth::Byte;
    QByteArray bytes;
};

class FakePlcDevice : public IPlcDevice
{
public:
    PlcOperationResult connectTo(const char *, int, int) override
    {
        connected = connectError == 0;
        return PlcOperationResult(connectError);
    }

    PlcOperationResult disconnect() override
    {
        connected = false;
        return PlcOperationResult();
    }

    bool isConnected() override
    {
        return connected;
    }

    PlcOperationResult writeDbArea(
        int dbNumber,
        int start,
        int amount,
        PlcDataWidth width,
        void *data) override
    {
        PlcWrite write;
        write.dbNumber = dbNumber;
        write.start = start;
        write.width = width;
        write.bytes = QByteArray(
                    static_cast<const char *>(data), amount);
        writes.push_back(write);
        return PlcOperationResult(
                    writes.size() == failWriteCall
                    ? failWriteError
                    : 0);
    }

    bool connected = false;
    int connectError = 0;
    int failWriteCall = -1;
    int failWriteError = 91;
    QVector<PlcWrite> writes;
};

InspectionPlcAddressMap plcAddresses()
{
    InspectionPlcAddressMap addresses;
    addresses.triggerModeDb = 1;
    addresses.triggerModeOffset = 1032;
    addresses.resultDb = 1;
    addresses.resultOffset = 1033;
    addresses.rejectTimeOffset = 980;
    addresses.rejectDistanceOffset = 920;
    addresses.photoTimeOffset = 982;
    addresses.photoDistanceOffset = 924;
    return addresses;
}

std::shared_ptr<PipelineRegistry> registry(
    const std::shared_ptr<FakeOcrEngine> &ocr =
        std::shared_ptr<FakeOcrEngine>(new FakeOcrEngine),
    const std::shared_ptr<FakeBarcodeDecoder> &barcode =
        std::shared_ptr<FakeBarcodeDecoder>(new FakeBarcodeDecoder))
{
    return std::shared_ptr<PipelineRegistry>(
                new PipelineRegistry(ocr, barcode));
}

std::vector<cv::Point2f> square(float left, float top, float size)
{
    std::vector<cv::Point2f> points;
    points.push_back(cv::Point2f(left, top));
    points.push_back(cv::Point2f(left + size, top));
    points.push_back(cv::Point2f(left + size, top + size));
    points.push_back(cv::Point2f(left, top + size));
    return points;
}

PreparedRecipeSnapshot preparedRecipe(DetectionMode mode)
{
    ProductRecipe recipe;
    recipe.recipeId = QStringLiteral("11111111-1111-4111-8111-111111111111");
    recipe.displayName = QStringLiteral("runtime-test");
    recipe.detectionMode = mode;

    PreparedRecipe prepared;
    prepared.recipe = ProductRecipeSnapshot(new ProductRecipe(recipe));
    prepared.tissue.roughnessThreshold = 6.0;
    if (mode != DetectionMode::Tissue) {
        PreparedRecipeProfile profile;
        profile.definition.name = QStringLiteral("profile-1");
        profile.definition.targetText = QStringLiteral("1");
        profile.definition.imageThresholdPercent = 70;
        profile.rawImage = cv::Mat(
                    120, 160, CV_8UC3, cv::Scalar(30, 40, 50));
        profile.trackingTemplate = cv::Mat(
                    24, 24, CV_8UC1, cv::Scalar(80));
        profile.stampRingTemplate = cv::Mat(
                    24, 24, CV_8UC1, cv::Scalar(160));
        profile.datePolygon = square(50.0f, 40.0f, 40.0f);
        profile.barcodePolygon = square(5.0f, 5.0f, 25.0f);
        profile.stampPolygon = square(10.0f, 10.0f, 20.0f);
        profile.characterTemplates.push_back(cv::Mat(
            12, 10, CV_8UC3, cv::Scalar(220, 220, 220)));
        profile.characterTemplateTargetIndexes.push_back(0);
        prepared.profiles.push_back(profile);
    }
    return PreparedRecipeSnapshot(new PreparedRecipe(prepared));
}

InspectionProfileSnapshot profileSnapshot()
{
    InspectionProfileSource source;
    source.name = QStringLiteral("profile-1");
    source.trackingTemplate = cv::Mat(
                24, 24, CV_8UC1, cv::Scalar(80));
    source.barcodePoly = square(5.0f, 5.0f, 25.0f);
    source.datePoly = square(50.0f, 40.0f, 40.0f);
    source.targetText = QStringLiteral("1");
    source.imageThreshold = 70;
    source.digitTemplates.push_back(cv::Mat(
        12, 10, CV_8UC3, cv::Scalar(220, 220, 220)));
    source.digitTemplateTargetIndexes.push_back(0);
    return InspectionProfileSnapshotBuilder::create(
                std::vector<InspectionProfileSource>(1, source));
}

bool startRun(
    InspectionRuntime *runtime,
    DetectionMode mode,
    const ResultServiceRunConfiguration &configuration =
        ResultServiceRunConfiguration())
{
    const PreparedRecipeSnapshot prepared = preparedRecipe(mode);
    const InspectionProfileSnapshot profiles =
            mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord
            ? profileSnapshot()
            : InspectionProfileSnapshot();
    MachineSettings settings;
    if (runtime->beginStart(settings, prepared, profiles).isEmpty()) {
        return false;
    }
    QString error;
    return runtime->startPipeline(configuration, &error)
            && runtime->commitStart();
}

DetectionCompletion completionFor(
    InspectionRuntime *runtime,
    AlgorithmVerdict verdict,
    quint64 frameNumber = 0)
{
    DetectionCompletion completion;
    completion.frame = runtime->acceptFrame(
                cv::Mat(32, 48, CV_8UC3, cv::Scalar(10, 20, 30)),
                frameNumber);
    completion.result.modeId = QStringLiteral("tissue_detection");
    completion.result.verdict = verdict;
    completion.result.status = DetectionStatus::Completed;
    completion.result.recognizedText = verdict == AlgorithmVerdict::Ok
            ? QStringLiteral("OK-TEXT")
            : QStringLiteral("NG-TEXT");
    completion.result.elapsedMs = 3.0;
    return completion;
}

void submitTissueResult(
    InspectionRuntime *runtime,
    const DetectionCompletion &completion)
{
    TissueRollResult output;
    output.isOk = completion.result.verdict == AlgorithmVerdict::Ok;
    output.rollFound = true;
    output.roll.isOk = output.isOk;
    output.roll.roughnessScore = output.isOk ? 2.0 : 8.0;
    output.roll.center = cv::Point2f(16.0f, 16.0f);
    output.roll.outerAxes = cv::Size2f(8.0f, 8.0f);
    output.roll.innerCenter = cv::Point2f(16.0f, 16.0f);
    output.roll.innerAxes = cv::Size2f(3.0f, 3.0f);
    runtime->resultService().pipelineConsumers().tissue(completion, output);
}

InspectionPresentationViewBindings viewBindings(
    QImage *presentedImage,
    QString *recognition,
    int *total,
    int *ng,
    QString *elapsed)
{
    InspectionPresentationViewBindings bindings;
    bindings.showImage = [presentedImage](const QImage &image) {
        *presentedImage = image;
    };
    bindings.showVerdictStyle = [](DetectionVerdictViewStyle) {};
    bindings.showVerdictText = [](const QString &) {};
    bindings.showRecognitionText = [recognition](const QString &text) {
        *recognition = text;
    };
    bindings.showTemplateName = [](const QString &) {};
    bindings.showTotalCount = [total](int value) { *total = value; };
    bindings.showNgCount = [ng](int value) { *ng = value; };
    bindings.showPassRate = [](double) {};
    bindings.showElapsedText = [elapsed](const QString &text) {
        *elapsed = text;
    };
    return bindings;
}

int byteValue(const PlcWrite &write)
{
    return write.bytes.isEmpty()
            ? -1
            : static_cast<unsigned char>(write.bytes.at(0));
}

ImageSaveTask saveTask(quint64 sequence)
{
    ImageSaveTask task;
    task.productKey.runId = QStringLiteral("save-run");
    task.productKey.sequence = sequence;
    ImageSaveItem item;
    item.image = QImage(2, 2, QImage::Format_RGB32);
    item.image.fill(Qt::white);
    item.filePath = QStringLiteral("unused-%1.jpg").arg(sequence);
    item.format = QByteArrayLiteral("JPG");
    item.quality = 92;
    task.items.push_back(item);
    return task;
}

int fileCountBelow(const QString &root)
{
    int count = 0;
    QDirIterator iterator(
                root,
                QDir::Files,
                QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

} // namespace

class DetectionCompletionTest : public QObject
{
    Q_OBJECT

private slots:
    void runtimeRequiresExplicitImmutableRunContext();
    void registrySelectsAllFiveStableModes();
    void duplicateProductResultHasOneSetOfSideEffects();
    void presentationFieldsComeFromOneProductSnapshot();
    void okWritesOneZeroToPlc();
    void ngWrites49ThenResetsToZero();
    void faultAfterIssuedNgStillCompletesZeroReset();
    void plcWriteFailurePreservesVerdictAndEntersFault();
    void faultMarksOnlyUnfinishedProductsUnconfirmed();
    void faultPreservesFirstCauseAndRejectsNewFrames();
    void delayedNgUsesConfiguredProductOffset();
    void duplicateResultSubmitsOneImageSaveProduct();
    void imageSaveServiceKeepsCapacity32AndTwoWorkers();
    void fullImageSaveQueueBackpressuresWithoutLoss();
};

void DetectionCompletionTest::runtimeRequiresExplicitImmutableRunContext()
{
    InspectionRuntime runtime(
                []() { return QStringLiteral("immutable-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    QVERIFY(!runtime.acceptFrame(
        cv::Mat(4, 4, CV_8UC1, cv::Scalar(1))));

    MachineSettings settings;
    settings.cameraDelay = 123;
    const PreparedRecipeSnapshot prepared =
            preparedRecipe(DetectionMode::Tissue);
    QCOMPARE(runtime.beginStart(
                 settings, prepared, InspectionProfileSnapshot()),
             QStringLiteral("immutable-run"));
    const std::shared_ptr<const InspectionRunContext> context =
            runtime.runContext();
    QVERIFY(context);
    QCOMPARE(context->runId, QStringLiteral("immutable-run"));
    QCOMPARE(context->machineSettings.cameraDelay, 123);
    QCOMPARE(context->preparedRecipe.get(), prepared.get());

    settings.cameraDelay = 999;
    QCOMPARE(context->machineSettings.cameraDelay, 123);
    QVERIFY(runtime.beginStart(
        settings, prepared, InspectionProfileSnapshot()).isEmpty());
    runtime.rollbackStart();
    QCOMPARE(runtime.state(), InspectionRuntimeState::Idle);
    QVERIFY(!runtime.runContext());
}

void DetectionCompletionTest::registrySelectsAllFiveStableModes()
{
    const std::shared_ptr<FakeOcrEngine> ocr(new FakeOcrEngine);
    const std::shared_ptr<FakeBarcodeDecoder> barcode(
                new FakeBarcodeDecoder);
    PipelineRegistry pipelineRegistry(ocr, barcode);
    PipelineResultConsumers consumers;
    consumers.tissue = [](const DetectionCompletion &,
                          const TissueRollResult &) {};
    consumers.ocr = [](const DetectionCompletion &,
                       const DetectionPose &) {};
    consumers.stamp = [](const DetectionCompletion &,
                         const StampDetectionWorkOutput &) {};
    consumers.word = [](const DetectionCompletion &,
                        const WordDetectionWorkOutput &) {};
    consumers.barcodeWord = [](const DetectionCompletion &,
                               const BarcodeWordDetectionWorkOutput &) {};

    const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    for (DetectionMode mode : modes) {
        PipelineRegistryRequest request;
        request.mode = mode;
        request.preparedRecipe = preparedRecipe(mode);
        if (mode == DetectionMode::Word
                || mode == DetectionMode::BarcodeWord) {
            request.profileSnapshot = profileSnapshot();
        }
        const PipelineCreationResult created = pipelineRegistry.create(
                    request, consumers);
        QVERIFY2(created.isAccepted(),
                 qPrintable(created.errorMessage));
        QCOMPARE(created.worker->queueCapacity(),
                 static_cast<std::size_t>(1));
    }
    QVERIFY(barcode->ensureLoadedCalls > 0);
}

void DetectionCompletionTest::duplicateProductResultHasOneSetOfSideEffects()
{
    InspectionRuntime runtime(
                []() { return QStringLiteral("duplicate-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    QVERIFY(startRun(&runtime, DetectionMode::Tissue));
    const DetectionCompletion completion = completionFor(
                &runtime, AlgorithmVerdict::Ng);
    QVERIFY(completion.isValid());
    submitTissueResult(&runtime, completion);
    submitTissueResult(&runtime, completion);

    QCOMPARE(runtime.totalCount(), 1);
    QCOMPARE(runtime.ngCount(), 1);
    QCOMPARE(runtime.completedProductCount(), static_cast<quint64>(1));
    QCOMPARE(runtime.unresolvedFaultProductCount(), 0);
}

void DetectionCompletionTest::presentationFieldsComeFromOneProductSnapshot()
{
    InspectionRuntime runtime(
                []() { return QStringLiteral("presentation-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    QVERIFY(startRun(&runtime, DetectionMode::Tissue));

    QImage image;
    QString recognition;
    QString elapsed;
    int total = -1;
    int ng = -1;
    runtime.resultService().bindView(viewBindings(
        &image, &recognition, &total, &ng, &elapsed));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ok, 7));
    QTRY_COMPARE(total, 1);
    QCOMPARE(runtime.resultService().lastPresentedProductKey().runId,
             QStringLiteral("presentation-run"));
    QCOMPARE(runtime.resultService().lastPresentedProductKey().sequence,
             static_cast<quint64>(1));
    QCOMPARE(ng, 0);
    QVERIFY(!image.isNull());
    QCOMPARE(recognition, QStringLiteral("粗糙度：2.000"));
    QVERIFY(!elapsed.isEmpty());
}

void DetectionCompletionTest::okWritesOneZeroToPlc()
{
    FakePlcDevice *device = new FakePlcDevice;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::unique_ptr<IPlcDevice>(device), plcAddresses()));
    InspectionRuntime runtime(
                []() { return QStringLiteral("plc-ok-run"); },
                plc,
                registry());
    QVERIFY(runtime.connectPlc(QStringLiteral("127.0.0.1"), 0, 1)
            .isSuccess());
    ResultServiceRunConfiguration configuration;
    configuration.plcOutputEnabled = true;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ok));
    QCOMPARE(device->writes.size(), 1);
    QCOMPARE(byteValue(device->writes.at(0)), 0);
    QCOMPARE(runtime.state(), InspectionRuntimeState::Running);
}

void DetectionCompletionTest::ngWrites49ThenResetsToZero()
{
    FakePlcDevice *device = new FakePlcDevice;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::unique_ptr<IPlcDevice>(device), plcAddresses()));
    InspectionRuntime runtime(
                []() { return QStringLiteral("plc-ng-run"); },
                plc,
                registry());
    QVERIFY(runtime.connectPlc(QStringLiteral("127.0.0.1"), 0, 1)
            .isSuccess());
    ResultServiceRunConfiguration configuration;
    configuration.plcOutputEnabled = true;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ng));
    QCOMPARE(device->writes.size(), 1);
    QCOMPARE(byteValue(device->writes.at(0)), 49);
    QTRY_COMPARE(device->writes.size(), 2);
    QCOMPARE(byteValue(device->writes.at(1)), 0);
    QCOMPARE(runtime.state(), InspectionRuntimeState::Running);
}

void DetectionCompletionTest::faultAfterIssuedNgStillCompletesZeroReset()
{
    FakePlcDevice *device = new FakePlcDevice;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::unique_ptr<IPlcDevice>(device),
                    plcAddresses()));
    QVERIFY(plc->connectTo(QStringLiteral("127.0.0.1"), 0, 1)
            .isSuccess());
    InspectionRuntime runtime(
                []() { return QStringLiteral("fault-after-ng-run"); },
                plc,
                registry());
    ResultServiceRunConfiguration configuration;
    configuration.plcOutputEnabled = true;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ng));
    QCOMPARE(device->writes.size(), 1);
    QCOMPARE(byteValue(device->writes.at(0)), 49);
    QVERIFY(runtime.enterFault(
        InspectionFaultReason::CameraDisconnected,
        QStringLiteral("camera stopped")));

    QTRY_COMPARE(device->writes.size(), 2);
    QCOMPARE(byteValue(device->writes.at(1)), 0);
    QCOMPARE(runtime.state(), InspectionRuntimeState::Fault);
}

void DetectionCompletionTest::plcWriteFailurePreservesVerdictAndEntersFault()
{
    FakePlcDevice *device = new FakePlcDevice;
    device->failWriteCall = 1;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::unique_ptr<IPlcDevice>(device), plcAddresses()));
    InspectionRuntime runtime(
                []() { return QStringLiteral("plc-fault-run"); },
                plc,
                registry());
    QVERIFY(runtime.connectPlc(QStringLiteral("127.0.0.1"), 0, 1)
            .isSuccess());
    ResultServiceRunConfiguration configuration;
    configuration.plcOutputEnabled = true;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ok));
    QCOMPARE(runtime.totalCount(), 1);
    QCOMPARE(runtime.ngCount(), 0);
    QCOMPARE(runtime.state(), InspectionRuntimeState::Fault);
    QCOMPARE(runtime.faultSnapshot().reason,
             InspectionFaultReason::PlcDisconnected);
    QCOMPARE(runtime.reconcileFaultProducts(), 0);
    QCOMPARE(runtime.faultUnconfirmedProductCount(), 0);
    QCOMPARE(device->writes.size(), 1);
    QCOMPARE(byteValue(device->writes.at(0)), 0);
    QVERIFY(runtime.acknowledgeFault());
}

void DetectionCompletionTest::faultMarksOnlyUnfinishedProductsUnconfirmed()
{
    InspectionRuntime runtime(
                []() { return QStringLiteral("unconfirmed-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    QVERIFY(startRun(&runtime, DetectionMode::Tissue));
    const std::shared_ptr<const FrameData> unfinished = runtime.acceptFrame(
                cv::Mat(8, 8, CV_8UC1, cv::Scalar(1)));
    QVERIFY(unfinished);
    QVERIFY(runtime.enterFault(
        InspectionFaultReason::RuntimeInvariantViolation,
        QStringLiteral("test infrastructure fault")));
    QCOMPARE(runtime.reconcileFaultProducts(), 1);
    QCOMPARE(runtime.abnormalStatistics().unconfirmedProductCount,
             static_cast<quint64>(1));
    QCOMPARE(runtime.abnormalStatistics().systemFaultCount,
             static_cast<quint64>(1));
    QCOMPARE(runtime.totalCount(), 0);
    QCOMPARE(runtime.ngCount(), 0);
    QCOMPARE(runtime.pendingDelayedNgCount(), 0);
    QVERIFY(runtime.acknowledgeFault());
}

void DetectionCompletionTest::faultPreservesFirstCauseAndRejectsNewFrames()
{
    InspectionRuntime runtime(
                []() { return QStringLiteral("first-fault-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    QVERIFY(startRun(&runtime, DetectionMode::Tissue));
    QVERIFY(runtime.enterFault(
        InspectionFaultReason::HardTriggerQueueOverflow,
        QStringLiteral("queue full")));
    QVERIFY(!runtime.enterFault(
        InspectionFaultReason::PlcDisconnected,
        QStringLiteral("later fault")));
    QCOMPARE(runtime.faultSnapshot().reason,
             InspectionFaultReason::HardTriggerQueueOverflow);
    QVERIFY(!runtime.acceptFrame(
        cv::Mat(8, 8, CV_8UC1, cv::Scalar(1))));
    QCOMPARE(runtime.faultSnapshot().postFaultDroppedFrameCount,
             static_cast<quint64>(1));
    QCOMPARE(runtime.reconcileFaultProducts(), 0);
    QVERIFY(runtime.acknowledgeFault());
}

void DetectionCompletionTest::delayedNgUsesConfiguredProductOffset()
{
    FakePlcDevice *device = new FakePlcDevice;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::unique_ptr<IPlcDevice>(device), plcAddresses()));
    InspectionRuntime runtime(
                []() { return QStringLiteral("delayed-ng-run"); },
                plc,
                registry());
    QVERIFY(runtime.connectPlc(QStringLiteral("127.0.0.1"), 0, 1)
            .isSuccess());
    ResultServiceRunConfiguration configuration;
    configuration.plcOutputEnabled = true;
    configuration.delayedNgOffset = 1;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ng));
    QCOMPARE(device->writes.size(), 0);
    QCoreApplication::processEvents();
    submitTissueResult(&runtime, completionFor(
        &runtime, AlgorithmVerdict::Ok));
    QCOMPARE(device->writes.size(), 2);
    QCOMPARE(byteValue(device->writes.at(0)), 49);
    QCOMPARE(byteValue(device->writes.at(1)), 0);
    QTRY_COMPARE(device->writes.size(), 3);
    QCOMPARE(byteValue(device->writes.at(2)), 0);
}

void DetectionCompletionTest::duplicateResultSubmitsOneImageSaveProduct()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    InspectionRuntime runtime(
                []() { return QStringLiteral("save-once-run"); },
                std::shared_ptr<InspectionPlcController>(),
                registry());
    ResultServiceRunConfiguration configuration;
    configuration.imageSaveModeIndex = 2;
    configuration.saveOptions.rootDirectory = directory.path();
    configuration.saveOptions.format = QStringLiteral("png");
    configuration.saveOptions.quality = 92;
    configuration.saveOptions.imageContentModeIndex = 0;
    QVERIFY(startRun(&runtime, DetectionMode::Tissue, configuration));

    const DetectionCompletion completion = completionFor(
                &runtime, AlgorithmVerdict::Ok);
    submitTissueResult(&runtime, completion);
    submitTissueResult(&runtime, completion);
    QTRY_COMPARE(fileCountBelow(directory.path()), 2);
    QCOMPARE(runtime.totalCount(), 1);
}

void DetectionCompletionTest::imageSaveServiceKeepsCapacity32AndTwoWorkers()
{
    ImageSaveService service(
                32,
                [](const ImageSaveItem &, QString *) { return true; },
                2);
    QCOMPARE(service.capacity(), static_cast<std::size_t>(32));
    QCOMPARE(service.workerCount(), static_cast<std::size_t>(2));
    service.shutdown();
}

void DetectionCompletionTest::fullImageSaveQueueBackpressuresWithoutLoss()
{
    std::mutex mutex;
    std::condition_variable condition;
    bool firstWriteStarted = false;
    bool releaseFirstWrite = false;
    QVector<quint64> written;
    ImageSaveService service(
                1,
                [&](const ImageSaveItem &item, QString *) {
        const quint64 sequence = item.filePath
                .section(QLatin1Char('-'), 1, 1)
                .section(QLatin1Char('.'), 0, 0)
                .toULongLong();
        std::unique_lock<std::mutex> lock(mutex);
        written.push_back(sequence);
        if (sequence == 1) {
            firstWriteStarted = true;
            condition.notify_all();
            condition.wait(lock, [&]() { return releaseFirstWrite; });
        }
        return true;
    },
    1);

    QVERIFY(service.submit(saveTask(1)).isAccepted());
    {
        std::unique_lock<std::mutex> lock(mutex);
        QVERIFY(condition.wait_for(
            lock,
            std::chrono::seconds(2),
            [&]() { return firstWriteStarted; }));
    }
    std::future<ImageSaveSubmitResult> second = std::async(
                std::launch::async,
                [&service]() { return service.submit(saveTask(2)); });
    QVERIFY(second.wait_for(std::chrono::milliseconds(50))
            == std::future_status::timeout);
    {
        std::lock_guard<std::mutex> lock(mutex);
        releaseFirstWrite = true;
    }
    condition.notify_all();
    QVERIFY(second.get().isAccepted());
    service.shutdown();
    QCOMPARE(written.size(), 2);
    QCOMPARE(written.at(0), static_cast<quint64>(1));
    QCOMPARE(written.at(1), static_cast<quint64>(2));
}

QTEST_GUILESS_MAIN(DetectionCompletionTest)
#include "detection_completion_test.moc"
