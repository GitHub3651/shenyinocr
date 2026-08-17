#include <QtTest>

#include "DetectionModes.h"
#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "devices/barcode/barcode_decoder.h"
#include "devices/ocr/ocr_engine.h"
#include "recipes/recipe_store.h"
#include "runtime/inspection_runtime.h"
#include "runtime/pipeline_registry.h"
#include "system_support/settings/machine_settings_store.h"

#include <QDir>
#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QVector>

#include <opencv2/imgcodecs.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

namespace {

class ApplicationFakeOcrEngine : public IOcrEngine
{
public:
    std::vector<std::string> recognize(cv::Mat &) override
    {
        return std::vector<std::string>(1, "1");
    }
};

class ApplicationFakeBarcodeDecoder : public IBarcodeDecoder
{
public:
    bool ensureLoaded() override
    {
        return true;
    }

    QString lastError() const override
    {
        return QString();
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
};

struct PlcWrite
{
    int start = 0;
    QByteArray data;
};

class PlcCommandFakeDevice : public IPlcDevice
{
public:
    PlcOperationResult connectTo(
        const char *address,
        int rack,
        int slot) override
    {
        connected = true;
        connectedAddress = QString::fromUtf8(address);
        connectedRack = rack;
        connectedSlot = slot;
        return PlcOperationResult();
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
        int,
        int start,
        int amount,
        PlcDataWidth,
        void *data) override
    {
        PlcWrite write;
        write.start = start;
        write.data = QByteArray(
                    static_cast<const char *>(data), amount);
        writes.append(write);
        return PlcOperationResult();
    }

    bool connected = false;
    QString connectedAddress;
    int connectedRack = -1;
    int connectedSlot = -1;
    QVector<PlcWrite> writes;
};

class ApplicationFakeCamera : public ICameraDevice
{
public:
    CameraResult enumerate(int *deviceCount) override
    {
        if (deviceCount) {
            *deviceCount = 1;
        }
        ++enumerateCount;
        return CameraResult();
    }

    CameraResult openFirst() override
    {
        ++openCount;
        opened = true;
        return CameraResult();
    }

    CameraResult applySettings(const CameraSettings &settings) override
    {
        CameraResult result;
        result.exposureRange.minimum = 100.0f;
        result.exposureRange.maximum = 100000.0f;
        result.gainRange.minimum = 0.0f;
        result.gainRange.maximum = 24.0f;
        if (settings.updateExposure) {
            if (settings.exposure < result.exposureRange.minimum
                    || settings.exposure
                       > result.exposureRange.maximum) {
                result.code = CameraResultCode::InvalidSettings;
            } else {
                exposure = settings.exposure;
            }
        }
        if (settings.updateGain) {
            if (settings.gain < result.gainRange.minimum
                    || settings.gain > result.gainRange.maximum) {
                result.code = CameraResultCode::InvalidSettings;
            } else {
                gain = settings.gain;
            }
        }
        result.exposureRange.current = exposure;
        result.gainRange.current = gain;
        return result;
    }

    CameraResult setTriggerMode(CameraTriggerMode mode) override
    {
        triggerMode = mode;
        triggerModes.append(mode);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_interrupted = false;
        }
        return CameraResult();
    }

    CameraResult startGrabbing() override
    {
        grabbing = true;
        return CameraResult();
    }

    CameraResult triggerSoftware() override
    {
        ++softwareTriggerCount;
        return CameraResult();
    }

    CameraFrameResult waitNextFrame(int timeoutMs) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait_for(
                    lock,
                    std::chrono::milliseconds(timeoutMs),
                    [this]() { return m_interrupted; });
        CameraFrameResult result;
        result.status = m_interrupted
                ? CameraFrameStatus::Interrupted
                : CameraFrameStatus::Timeout;
        return result;
    }

    void interruptWait() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_interrupted = true;
        }
        m_condition.notify_all();
    }

    CameraResult stopGrabbing() override
    {
        grabbing = false;
        return CameraResult();
    }

    CameraResult close() override
    {
        ++closeCount;
        opened = false;
        grabbing = false;
        interruptWait();
        return CameraResult();
    }

    int enumerateCount = 0;
    int openCount = 0;
    int closeCount = 0;
    std::atomic<int> softwareTriggerCount{0};
    bool opened = false;
    bool grabbing = false;
    float exposure = 800.0f;
    float gain = 1.0f;
    CameraTriggerMode triggerMode = CameraTriggerMode::Software;
    QVector<CameraTriggerMode> triggerModes;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_interrupted = false;
};

bool writeImage(const QString &path,
                const cv::Mat &image,
                const char *extension)
{
    std::vector<uchar> bytes;
    if (!cv::imencode(extension, image, bytes)) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(
                reinterpret_cast<const char *>(bytes.data()),
                static_cast<qint64>(bytes.size()))
               == static_cast<qint64>(bytes.size());
}

bool writeCalibration(const QString &path,
                      bool includeStamp,
                      bool includeBarcode)
{
    std::vector<cv::Point2f> stamp;
    if (includeStamp) {
        stamp.push_back(cv::Point2f(-3, -3));
        stamp.push_back(cv::Point2f(3, -3));
        stamp.push_back(cv::Point2f(0, 3));
    }
    std::vector<cv::Point2f> date;
    date.push_back(cv::Point2f(-5, -2));
    date.push_back(cv::Point2f(5, -2));
    date.push_back(cv::Point2f(0, 4));
    std::vector<cv::Point2f> barcode;
    if (includeBarcode) {
        barcode.push_back(cv::Point2f(-6, -6));
        barcode.push_back(cv::Point2f(6, -6));
        barcode.push_back(cv::Point2f(6, 6));
        barcode.push_back(cv::Point2f(-6, 6));
    }
    cv::FileStorage storage(
                "calibrate_config.yaml",
                cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    storage << "stamp_poly" << stamp;
    storage << "date_poly" << date;
    storage << "barcode_poly" << barcode;
    const std::string yaml = storage.releaseAndGetString();
    QFile file(path);
    return !yaml.empty()
            && file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(yaml.data(), static_cast<qint64>(yaml.size()))
               == static_cast<qint64>(yaml.size());
}

void addAsset(ProductRecipe *recipe,
              RecipeProfile *profile,
              QMap<QString, QString> *sources,
              const QString &key,
              const QString &role,
              const QString &relativePath,
              const QString &sourcePath)
{
    recipe->assets.insert(key, relativePath);
    profile->assetKeys.insert(role, key);
    sources->insert(key, sourcePath);
}

bool addProfile(ProductRecipe *recipe,
                const QString &sourceRoot,
                QMap<QString, QString> *sources)
{
    const QString profileRoot = QDir(sourceRoot).filePath(
                detectionModeId(recipe->detectionMode));
    if (!QDir().mkpath(profileRoot)) {
        return false;
    }
    const QString relativeRoot = QStringLiteral("assets/profiles/0/");
    const QString rawPath = QDir(profileRoot).filePath("raw.png");
    const QString trackingPath = QDir(profileRoot).filePath("tracking.bmp");
    const QString calibrationPath = QDir(profileRoot).filePath(
                "calibrate_config.yaml");
    if (!writeImage(rawPath,
                    cv::Mat(80, 80, CV_8UC3,
                            cv::Scalar(10, 20, 30)),
                    ".png")
            || !writeImage(trackingPath,
                           cv::Mat(20, 20, CV_8UC3,
                                   cv::Scalar(40, 50, 60)),
                           ".bmp")
            || !writeCalibration(
                calibrationPath,
                recipe->detectionMode == DetectionMode::Stamp,
                recipe->detectionMode == DetectionMode::BarcodeWord)) {
        return false;
    }

    RecipeProfile profile;
    profile.name = QStringLiteral("Profile-1");
    profile.targetText = QStringLiteral("1");
    profile.trackingRoi = QRectF(10, 10, 20, 20);
    addAsset(recipe, &profile, sources,
             QStringLiteral("profile0.rawImage"),
             QStringLiteral("rawImage"),
             relativeRoot + QStringLiteral("template_raw.png"),
             rawPath);
    addAsset(recipe, &profile, sources,
             QStringLiteral("profile0.trackingTemplate"),
             QStringLiteral("trackingTemplate"),
             relativeRoot + QStringLiteral("tracking_template.bmp"),
             trackingPath);
    addAsset(recipe, &profile, sources,
             QStringLiteral("profile0.calibration"),
             QStringLiteral("calibration"),
             relativeRoot + QStringLiteral("calibrate_config.yaml"),
             calibrationPath);

    if (recipe->detectionMode == DetectionMode::Stamp) {
        const QString ringPath = QDir(profileRoot).filePath("ring.bmp");
        if (!writeImage(ringPath,
                        cv::Mat(16, 16, CV_8UC1, cv::Scalar(80)),
                        ".bmp")) {
            return false;
        }
        addAsset(recipe, &profile, sources,
                 QStringLiteral("profile0.stampRing"),
                 QStringLiteral("stampRing"),
                 relativeRoot + QStringLiteral("template_ring.bmp"),
                 ringPath);
    }

    if (recipe->detectionMode == DetectionMode::Stamp
            || recipe->detectionMode == DetectionMode::Word
            || recipe->detectionMode == DetectionMode::BarcodeWord) {
        profile.characterSourceSize = QSize(11, 7);
        RecipeCharacterBox box;
        box.name = QStringLiteral("1");
        box.rect = QRect(1, 1, 6, 5);
        profile.characterBoxes.append(box);
        const QString characterPath = QDir(profileRoot).filePath("1.png");
        if (!writeImage(characterPath,
                        cv::Mat(5, 6, CV_8UC1, cv::Scalar(120)),
                        ".png")) {
            return false;
        }
        addAsset(recipe, &profile, sources,
                 QStringLiteral("profile0.character0"),
                 QStringLiteral("character/1.png"),
                 relativeRoot
                 + QStringLiteral("character_templates/1.png"),
                 characterPath);
    }
    recipe->profiles.append(profile);
    return true;
}

ProductRecipe makeRecipe(DetectionMode mode,
                         const QString &sourceRoot,
                         QMap<QString, QString> *sources)
{
    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("Application-%1")
                .arg(detectionModeId(mode)),
                mode);
    if (mode != DetectionMode::Tissue
            && !addProfile(&recipe, sourceRoot, sources)) {
        return ProductRecipe();
    }
    return recipe;
}

struct ServiceFixture
{
    QTemporaryDir root;
    std::shared_ptr<MachineSettingsStore> settingsStore;
    std::shared_ptr<SettingsApplicationService> settings;
    std::shared_ptr<RecipeStore> recipes;
    std::shared_ptr<InspectionRuntime> runtime;
    std::shared_ptr<InspectionPlcController> plcController;
    std::shared_ptr<PipelineRegistry> pipelineRegistry;
    PlcCommandFakeDevice *plcDevice = nullptr;
    std::shared_ptr<ApplicationFakeCamera> camera;
    std::shared_ptr<CameraSession> cameraSession;
    std::shared_ptr<InspectionApplicationService> service;

    explicit ServiceFixture(bool providePlc = false)
    {
        settingsStore.reset(new MachineSettingsStore(root.path()));
        settings.reset(new SettingsApplicationService(
                           settingsStore, MachineSettings::defaults()));
        recipes.reset(new RecipeStore(settingsStore->recipesRootPath()));
        pipelineRegistry.reset(new PipelineRegistry(
            std::shared_ptr<IOcrEngine>(new ApplicationFakeOcrEngine),
            std::shared_ptr<IBarcodeDecoder>(
                new ApplicationFakeBarcodeDecoder)));
        if (providePlc) {
            plcDevice = new PlcCommandFakeDevice;
            std::unique_ptr<IPlcDevice> device(plcDevice);
            plcController.reset(new InspectionPlcController(
                                    std::move(device),
                                    InspectionPlcAddressMap()));
            runtime.reset(new InspectionRuntime(
                              InspectionRuntime::RunIdFactory(),
                              plcController,
                              pipelineRegistry));
        } else {
            runtime.reset(new InspectionRuntime(
                InspectionRuntime::RunIdFactory(),
                std::shared_ptr<InspectionPlcController>(),
                pipelineRegistry));
        }
        camera.reset(new ApplicationFakeCamera);
        cameraSession.reset(new CameraSession(camera, runtime.get()));
        service.reset(new InspectionApplicationService(
                          runtime, cameraSession,
                          settings, recipes));
    }

    bool publishRecipe(DetectionMode mode,
                       QString *errorMessage)
    {
        const QString sourceRoot = QDir(root.path()).filePath("sources");
        QDir().mkpath(sourceRoot);
        QMap<QString, QString> sources;
        const ProductRecipe recipe = makeRecipe(
                    mode, sourceRoot, &sources);
        if (!recipes->saveRecipe(recipe, sources, errorMessage)) {
            return false;
        }
        MachineSettings draft = settings->current();
        draft.detectModeId = detectionModeUiId(mode);
        draft.triggerEnabled = false;
        draft.publishedRecipeIdsByMode.insert(
                    draft.detectModeId, recipe.recipeId);
        settings->updateDraft(draft);
        const OperationResult applied = settings->applyDraft();
        if (!applied.isSuccess() && errorMessage) {
            *errorMessage = applied.error.userMessage;
        }
        return applied.isSuccess();
    }

    OpenCameraResult openCamera()
    {
        PlcConnectionCommand command;
        command.address = QStringLiteral("192.168.10.10");
        command.rack = 0;
        command.slot = 1;
        return service->openCamera(command);
    }
};

} // namespace

class ApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void settingsDraftApplyDiscardDefaultsAndClear();
    void fiveModesStartStopAndRestart();
    void preflightOrderDirtyAndDuplicateStart();
    void plcTriggerDisconnectedBlocksStart();
    void plcCommandsUseApplicationBoundary();
    void cameraCommandsUseApplicationState();
    void hardTriggerUsesSharedCameraSession();
};

void ApplicationServiceTest::settingsDraftApplyDiscardDefaultsAndClear()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const std::shared_ptr<MachineSettingsStore> store(
                new MachineSettingsStore(root.path()));
    SettingsApplicationService service(
                store, MachineSettings::defaults());

    MachineSettings draft = service.current();
    draft.cameraExposure = 4321;
    service.updateDraft(draft);
    QVERIFY(service.hasUnappliedChanges());
    service.discardDraft();
    QVERIFY(!service.hasUnappliedChanges());
    QCOMPARE(service.current().cameraExposure,
             MachineSettings::defaults().cameraExposure);

    service.updateDraft(draft);
    QVERIFY(service.applyDraft().isSuccess());
    QCOMPARE(service.current().cameraExposure, 4321);
    MachineSettings reloaded;
    MachineSettingsLoadStatus status;
    QVERIFY(store->load(&reloaded, &status));
    QCOMPARE(reloaded.cameraExposure, 4321);

    QVERIFY(service.restoreDefaults().isSuccess());
    QVERIFY(service.current() == MachineSettings::defaults());
    QVERIFY(service.clearSettings().isSuccess());
    QVERIFY(service.current() == MachineSettings::defaults());
}

void ApplicationServiceTest::fiveModesStartStopAndRestart()
{
    ServiceFixture fixture;
    QVERIFY(fixture.root.isValid());
    QVERIFY(fixture.openCamera().operation.isSuccess());
    const QVector<DetectionMode> modes = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    for (const DetectionMode mode : modes) {
        QString error;
        QVERIFY2(fixture.publishRecipe(mode, &error), qPrintable(error));
        const StartInspectionResult first =
                fixture.service->start(StartInspectionCommand());
        QVERIFY2(first.isAccepted(),
                 qPrintable(first.error.userMessage
                            + first.error.diagnostic));
        QCOMPARE(first.snapshot.state,
                 ApplicationRuntimeState::Running);
        const StopInspectionResult stopped = fixture.service->stop();
        QVERIFY(stopped.isAccepted());
        QCOMPARE(stopped.snapshot.state,
                 ApplicationRuntimeState::Idle);

        const StartInspectionResult restarted =
                fixture.service->start(StartInspectionCommand());
        QVERIFY(restarted.isAccepted());
        QVERIFY(fixture.service->stop().isAccepted());
    }
}

void ApplicationServiceTest::preflightOrderDirtyAndDuplicateStart()
{
    ServiceFixture fixture;
    QString error;
    QVERIFY(fixture.publishRecipe(DetectionMode::Tissue, &error));

    StartInspectionCommand templateCommand;
    templateCommand.templateOperationActive = true;
    QCOMPARE(fixture.service->start(templateCommand).issue,
             InspectionStartIssue::TemplateOperationActive);
    QCOMPARE(fixture.service->start(StartInspectionCommand()).issue,
             InspectionStartIssue::CameraClosed);
    QVERIFY(fixture.openCamera().operation.isSuccess());

    MachineSettings draft = fixture.settings->current();
    ++draft.cameraGain;
    fixture.settings->updateDraft(draft);
    QCOMPARE(fixture.service->start(StartInspectionCommand()).issue,
             InspectionStartIssue::DirtySettingsConfirmationRequired);
    fixture.settings->discardDraft();

    QVERIFY(fixture.service->start(StartInspectionCommand()).isAccepted());
    QCOMPARE(fixture.service->start(StartInspectionCommand()).issue,
             InspectionStartIssue::RuntimeBusy);
    QVERIFY(fixture.service->stop().isAccepted());
}

void ApplicationServiceTest::plcTriggerDisconnectedBlocksStart()
{
    ServiceFixture fixture;
    QString error;
    QVERIFY(fixture.publishRecipe(DetectionMode::Tissue, &error));
    QVERIFY(fixture.openCamera().operation.isSuccess());
    MachineSettings settings = fixture.settings->current();
    settings.triggerEnabled = true;
    fixture.settings->updateDraft(settings);
    QVERIFY(fixture.settings->applyDraft().isSuccess());
    QCOMPARE(fixture.service->start(StartInspectionCommand()).issue,
             InspectionStartIssue::PlcDisconnected);
}

void ApplicationServiceTest::plcCommandsUseApplicationBoundary()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const std::shared_ptr<MachineSettingsStore> settingsStore(
                new MachineSettingsStore(root.path()));
    const std::shared_ptr<SettingsApplicationService> settings(
                new SettingsApplicationService(
                    settingsStore, MachineSettings::defaults()));
    const std::shared_ptr<RecipeStore> recipes(
                new RecipeStore(settingsStore->recipesRootPath()));
    PlcCommandFakeDevice *fake = new PlcCommandFakeDevice;
    std::unique_ptr<IPlcDevice> device(fake);
    InspectionPlcAddressMap addresses;
    addresses.triggerModeDb = 1;
    addresses.triggerModeOffset = 1032;
    addresses.resultDb = 1;
    addresses.rejectTimeOffset = 980;
    addresses.rejectDistanceOffset = 920;
    addresses.photoTimeOffset = 982;
    addresses.photoDistanceOffset = 924;
    const std::shared_ptr<InspectionPlcController> plc(
                new InspectionPlcController(
                    std::move(device), addresses));
    const std::shared_ptr<InspectionRuntime> runtime(
                new InspectionRuntime(
                    InspectionRuntime::RunIdFactory(),
                    plc,
                    std::shared_ptr<PipelineRegistry>(
                        new PipelineRegistry(
                            std::shared_ptr<IOcrEngine>(
                                new ApplicationFakeOcrEngine),
                            std::shared_ptr<IBarcodeDecoder>(
                                new ApplicationFakeBarcodeDecoder)))));
    const std::shared_ptr<ApplicationFakeCamera> camera(
                new ApplicationFakeCamera);
    const std::shared_ptr<CameraSession> cameraSession(
                new CameraSession(camera, runtime.get()));
    InspectionApplicationService service(
                runtime, cameraSession, settings, recipes);

    PlcConnectionCommand connection;
    connection.address = QStringLiteral("192.168.10.10");
    connection.rack = 0;
    connection.slot = 1;
    QVERIFY(service.connectPlc(connection).isSuccess());
    QCOMPARE(fake->connectedAddress, connection.address);
    QCOMPARE(fake->connectedRack, connection.rack);
    QCOMPARE(fake->connectedSlot, connection.slot);

    QVERIFY(service.applyPlcTriggerMode(
                machineSettingsTriggerModeIds().at(1)).isSuccess());
    InspectionPlcRunSettings runSettings;
    runSettings.rejectTime = 12;
    runSettings.rejectDistance = 345;
    runSettings.photoTime = 67;
    runSettings.photoDistance = 890;
    QVERIFY(service.applyPlcRunSettings(runSettings).isSuccess());
    QVERIFY(service.writePlcPhotoDistance(901).isSuccess());
    QCOMPARE(fake->writes.size(), 6);
    QCOMPARE(fake->writes.at(0).start, 1032);
    QCOMPARE(static_cast<unsigned char>(
                 fake->writes.at(0).data.at(0)),
             static_cast<unsigned char>(1));
    QCOMPARE(fake->writes.at(1).start, 980);
    QCOMPARE(fake->writes.at(2).start, 920);
    QCOMPARE(fake->writes.at(3).start, 982);
    QCOMPARE(fake->writes.at(4).start, 924);
    QCOMPARE(fake->writes.at(5).start, 924);
    QVERIFY(service.disconnectPlc().isSuccess());
    QVERIFY(!fake->connected);
}

void ApplicationServiceTest::cameraCommandsUseApplicationState()
{
    ServiceFixture fixture;
    QString error;
    QVERIFY(fixture.publishRecipe(DetectionMode::Tissue, &error));
    const OpenCameraResult opened = fixture.openCamera();
    QVERIFY(opened.operation.isSuccess());
    QVERIFY(opened.plcConnectionFailed);
    QVERIFY(opened.snapshot.cameraOpen);
    QVERIFY(fixture.service->queryCameraExposureRange().success);
    QVERIFY(fixture.service->queryCameraGainRange().success);
    QVERIFY(fixture.service->applyCameraExposure(4321).success);
    QVERIFY(fixture.service->applyCameraGain(5).success);
    QString previewError;
    QVERIFY2(fixture.service->startTemplatePreview(
                 17, 0, 0, &previewError), qPrintable(previewError));
    QVERIFY(fixture.service->isCapturing());
    QVERIFY(fixture.service->stopTemplatePreview());
    QVERIFY(!fixture.service->isCapturing());
    QVERIFY(fixture.service->start(StartInspectionCommand()).isAccepted());
    QVERIFY(!fixture.service->closeCamera().isSuccess());
    QVERIFY(fixture.service->stop().isAccepted());
    QVERIFY(fixture.service->closeCamera().isSuccess());
    QCOMPARE(fixture.camera->closeCount, 2);
}

void ApplicationServiceTest::hardTriggerUsesSharedCameraSession()
{
    ServiceFixture fixture(true);
    QString error;
    QVERIFY(fixture.publishRecipe(DetectionMode::Tissue, &error));
    QVERIFY(fixture.openCamera().operation.isSuccess());
    QVERIFY(fixture.plcDevice && fixture.plcDevice->connected);

    MachineSettings settings = fixture.settings->current();
    settings.triggerEnabled = true;
    fixture.settings->updateDraft(settings);
    QVERIFY(fixture.settings->applyDraft().isSuccess());

    const StartInspectionResult started =
            fixture.service->start(StartInspectionCommand());
    QVERIFY2(started.isAccepted(),
             qPrintable(started.error.userMessage
                        + started.error.diagnostic));
    QCOMPARE(started.acquisitionKind,
             InspectionAcquisitionKind::HardwareTrigger);
    QVERIFY(fixture.camera->triggerModes.contains(
                CameraTriggerMode::HardwareLine0));
    QCOMPARE(fixture.camera->softwareTriggerCount.load(), 0);
    QVERIFY(fixture.service->stop().isAccepted());
    QCOMPARE(fixture.camera->softwareTriggerCount.load(), 0);
}

QTEST_GUILESS_MAIN(ApplicationServiceTest)
#include "application_service_test.moc"
