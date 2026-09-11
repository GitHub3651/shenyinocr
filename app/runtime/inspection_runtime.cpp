#include "runtime/inspection_runtime.h"

#include "runtime/inspection_presentation_renderer.h"
#include "system_support/logging/log_categories.h"

#include <QMetaObject>
#include <QUuid>

#include <chrono>
#include <stdexcept>

// 一次正式运行的不可变快照只属于InspectionRuntime实现，不暴露为公共模块。
struct InspectionRunContext
{
    InspectionRunContext(
        const QString &runIdValue,
        const QDateTime &startedAtUtcValue,
        const AppSettings &machineSettingsValue,
        DetectionMode modeValue,
        const QVector<PreparedTemplateSnapshot> &preparedTemplatesValue,
        const MultiTemplateRuntimeSnapshot &multiTemplateSnapshotValue,
        double tissueRoughnessThresholdValue,
        const FramePreprocessSettings &framePreprocessValue)
        : runId(runIdValue),
          startedAtUtc(startedAtUtcValue),
          machineSettings(machineSettingsValue),
          mode(modeValue),
          preparedTemplates(preparedTemplatesValue),
          multiTemplateSnapshot(multiTemplateSnapshotValue),
          tissueRoughnessThreshold(tissueRoughnessThresholdValue),
          framePreprocess(framePreprocessValue)
    {
    }

    const QString runId;
    const QDateTime startedAtUtc;
    const AppSettings machineSettings;
    const DetectionMode mode;
    const QVector<PreparedTemplateSnapshot> preparedTemplates;
    const MultiTemplateRuntimeSnapshot multiTemplateSnapshot;
    const double tissueRoughnessThreshold;
    const FramePreprocessSettings framePreprocess;
};

namespace {

QString faultReasonName(InspectionFaultReason reason)
{
    switch (reason) {
    case InspectionFaultReason::None:
        return QStringLiteral("none");
    case InspectionFaultReason::CameraDisconnected:
        return QStringLiteral("camera_disconnected");
    case InspectionFaultReason::PlcDisconnected:
        return QStringLiteral("plc_disconnected");
    case InspectionFaultReason::HardTriggerQueueOverflow:
        return QStringLiteral("hard_trigger_queue_overflow");
    case InspectionFaultReason::ProductIdentityAmbiguous:
        return QStringLiteral("product_identity_ambiguous");
    case InspectionFaultReason::RuntimeInvariantViolation:
        return QStringLiteral("runtime_invariant_violation");
    case InspectionFaultReason::BarcodeCsvUnavailable:
        return QStringLiteral("barcode_csv_unavailable");
    }
    return QStringLiteral("unknown");
}

QString plcRunSettingFieldName(InspectionPlcRunSettingField field)
{
    switch (field) {
    case InspectionPlcRunSettingField::None:
        return QStringLiteral("none");
    case InspectionPlcRunSettingField::RejectTime:
        return QStringLiteral("reject_time");
    case InspectionPlcRunSettingField::RejectDistance:
        return QStringLiteral("reject_distance");
    case InspectionPlcRunSettingField::PhotoTime:
        return QStringLiteral("photo_time");
    case InspectionPlcRunSettingField::PhotoDistance:
        return QStringLiteral("photo_distance");
    }
    return QStringLiteral("unknown");
}

} // namespace

InspectionRuntime::InspectionRuntime(
    const RunIdFactory &runIdFactory,
    const std::shared_ptr<InspectionPlcController> &plcController,
    const std::shared_ptr<DetectionRegistry> &detectionRegistry)
    : QObject(nullptr),
      m_runIdFactory(runIdFactory),
      m_plcController(plcController),
      m_detectionRegistry(detectionRegistry)
{
    if (!m_detectionRegistry) {
        throw std::invalid_argument("DetectionRegistry is required");
    }
    m_resultService.reset(new ResultService(*this));
}

InspectionRuntime::~InspectionRuntime()
{
    requestDetectionWorkerStop();
    waitForDetectionWorkerStop();
}

QString InspectionRuntime::createRunId() const
{
    QString runId = m_runIdFactory ? m_runIdFactory().trimmed() : QString();
    if (runId.isEmpty()) {
        runId = QUuid::createUuid()
                .toString(QUuid::WithoutBraces)
                .toLower();
    }
    return runId;
}

QString InspectionRuntime::beginStart(
    const AppSettings &machineSettings,
    DetectionMode mode,
    const QVector<PreparedTemplateSnapshot> &preparedTemplates,
    const MultiTemplateRuntimeSnapshot &multiTemplateSnapshot,
    double tissueRoughnessThreshold,
    const FramePreprocessSettings &framePreprocess)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Idle
            || (mode != DetectionMode::Tissue
                && preparedTemplates.isEmpty())) {
        return QString();
    }

    const QString newRunId = createRunId();
    m_state = InspectionRuntimeState::Starting;
    m_runContext.reset(new InspectionRunContext(
        newRunId,
        QDateTime::currentDateTimeUtc(),
        machineSettings,
        mode,
        preparedTemplates,
        multiTemplateSnapshot,
        tissueRoughnessThreshold,
        framePreprocess));
    m_acceptedProductSequence = 0;
    m_completedProductCount = 0;
    m_lastCompletedProductSequence = 0;
    m_faultSnapshot = InspectionFaultSnapshot();
    m_acceptedFrames.clear();
    m_products.clear();
    m_uiCompletionMailbox.reopen();
    return newRunId;
}

bool InspectionRuntime::commitStart()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Starting
            || !m_detectionWorkerActive.load()
            || !m_runContext) {
        return false;
    }
    m_state = InspectionRuntimeState::Running;
    qCInfo(logRuntime).noquote()
            << QStringLiteral(
                "event=run.started mode=%1 templates=%2 trigger=%3 saveMode=%4")
               .arg(detectionModeId(m_runContext->mode))
               .arg(m_runContext->preparedTemplates.size())
               .arg(m_runContext->machineSettings.triggerEnabled
                    ? QStringLiteral("hardware")
                    : QStringLiteral("software"))
               .arg(m_runContext->machineSettings.imageSaveModeId);
    return true;
}

void InspectionRuntime::rollbackStart()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting) {
            return;
        }
        m_state = InspectionRuntimeState::Stopping;
    }
    requestDetectionWorkerStop();
    waitForDetectionWorkerStop();
    finishStop(false);
}

bool InspectionRuntime::beginStop()
{
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == InspectionRuntimeState::Fault
                || m_state == InspectionRuntimeState::Stopping) {
            accepted = true;
        } else if (m_state == InspectionRuntimeState::Starting
                   || m_state == InspectionRuntimeState::Running) {
            m_state = InspectionRuntimeState::Stopping;
            accepted = true;
        }
    }
    if (accepted) {
        m_uiCompletionMailbox.cancel();
        m_wakePosted.store(false);
        requestDetectionWorkerStop();
    }
    return accepted;
}

void InspectionRuntime::waitForStop()
{
    waitForDetectionWorkerStop();
}

void InspectionRuntime::finishStop(bool writeRunSummary)
{
    m_uiCompletionMailbox.cancel();
    m_wakePosted.store(false);
    const DetectionResultStatistics statistics = m_resultService->statistics();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault) {
        const quint64 acceptedCount = m_acceptedProductSequence;
        const quint64 completedCount = m_completedProductCount;
        m_state = InspectionRuntimeState::Idle;
        m_runContext.reset();
        m_products.clear();
        m_acceptedFrames.clear();
        if (writeRunSummary) {
            qCInfo(logRuntime).noquote()
                    << QStringLiteral(
                        "event=run.stopped accepted=%1 completed=%2 cancelled=%3 total=%4 ng=%5")
                       .arg(acceptedCount)
                       .arg(completedCount)
                       .arg(acceptedCount - completedCount)
                       .arg(statistics.totalCount)
                       .arg(statistics.ngCount);
        }
    }
}

bool InspectionRuntime::enterFault(
    InspectionFaultReason reason,
    const QString &diagnostic,
    const QDateTime &occurredAtUtc)
{
    InspectionFaultSnapshot enteredFault;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (reason == InspectionFaultReason::None
                || m_faultSnapshot.isActive()
                || (m_state != InspectionRuntimeState::Starting
                    && m_state != InspectionRuntimeState::Running
                    && m_state != InspectionRuntimeState::Stopping)) {
            return false;
        }
        m_faultSnapshot.reason = reason;
        m_faultSnapshot.diagnostic = diagnostic.trimmed();
        m_faultSnapshot.runId = m_runContext
                ? m_runContext->runId
                : QString();
        m_faultSnapshot.acceptedProductCount = m_acceptedProductSequence;
        m_faultSnapshot.completedProductCount = m_completedProductCount;
        m_faultSnapshot.postFaultDroppedFrameCount = 0;
        m_faultSnapshot.occurredAtUtc = occurredAtUtc.isValid()
                ? occurredAtUtc.toUTC()
                : QDateTime::currentDateTimeUtc();
        m_state = InspectionRuntimeState::Fault;
        enteredFault = m_faultSnapshot;
    }

    qCCritical(logRuntime).noquote()
            << QStringLiteral(
                "event=run.fault reason=%1 accepted=%2 completed=%3 diagnostic=%4")
               .arg(faultReasonName(enteredFault.reason))
               .arg(enteredFault.acceptedProductCount)
               .arg(enteredFault.completedProductCount)
               .arg(enteredFault.diagnostic);
    m_resultService->recordSystemFault();
    m_uiCompletionMailbox.cancel();
    m_wakePosted.store(false);
    emit faultSnapshotChanged(faultSnapshot());
    requestDetectionWorkerStop();
    return true;
}

int InspectionRuntime::reconcileFaultProducts()
{
    int count = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Fault) {
            return 0;
        }
        count = static_cast<int>(m_products.size());
        m_products.clear();
        m_acceptedFrames.clear();
    }
    m_resultService->recordUnconfirmedProducts(count);
    return count;
}

bool InspectionRuntime::acknowledgeFault()
{
    waitForDetectionWorkerStop();
    m_uiCompletionMailbox.cancel();
    m_wakePosted.store(false);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault
            || !m_products.empty()
            || m_detectionWorkerActive.load()) {
        return false;
    }
    m_faultSnapshot = InspectionFaultSnapshot();
    m_state = InspectionRuntimeState::Idle;
    m_runContext.reset();
    qCInfo(logRuntime).noquote() << "event=run.fault_acknowledged";
    return true;
}

InspectionRuntimeState InspectionRuntime::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

InspectionFaultSnapshot InspectionRuntime::faultSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultSnapshot;
}

bool InspectionRuntime::isBusy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state != InspectionRuntimeState::Idle;
}

bool InspectionRuntime::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Running;
}

QString InspectionRuntime::runId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runContext ? m_runContext->runId : QString();
}

bool InspectionRuntime::isPlcConnected() const
{
    return m_plcController && m_plcController->isConnected();
}

bool InspectionRuntime::requiresPlcForRun() const
{
    return m_resultService->requiresPlcForRun();
}

QImage InspectionRuntime::renderPreviewFrame(const cv::Mat &image)
{
    return InspectionPresentationRenderer::renderRawFrame(image);
}

PlcOperationResult InspectionRuntime::connectPlc(
    const QString &address,
    int rack,
    int slot)
{
    const PlcOperationResult result = m_plcController
            ? m_plcController->connectTo(address, rack, slot)
            : PlcOperationResult(-1);
    if (result.isSuccess()) {
        qCInfo(logDevice).noquote()
                << QStringLiteral(
                    "event=plc.connected address=%1 rack=%2 slot=%3")
                   .arg(address).arg(rack).arg(slot);
    } else {
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=plc.connect_failed address=%1 rack=%2 slot=%3 nativeCode=%4")
                   .arg(address).arg(rack).arg(slot)
                   .arg(result.nativeErrorCode);
    }
    return result;
}

PlcOperationResult InspectionRuntime::disconnectPlc()
{
    const PlcOperationResult result = m_plcController
            ? m_plcController->disconnect()
            : PlcOperationResult(-1);
    if (result.isSuccess()) {
        qCInfo(logDevice).noquote() << "event=plc.disconnected";
    } else {
        qCCritical(logDevice).noquote()
                << QStringLiteral("event=plc.disconnect_failed nativeCode=%1")
                   .arg(result.nativeErrorCode);
    }
    return result;
}

PlcOperationResult InspectionRuntime::writePlcTriggerMode(int modeIndex)
{
    const PlcOperationResult result = m_plcController
            ? m_plcController->writeTriggerMode(modeIndex)
            : PlcOperationResult(-1);
    if (result.isSuccess()) {
        qCInfo(logDevice).noquote()
                << QStringLiteral("event=plc.trigger_mode_written value=%1")
                   .arg(modeIndex);
    } else {
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=plc.trigger_mode_write_failed value=%1 nativeCode=%2")
                   .arg(modeIndex).arg(result.nativeErrorCode);
    }
    return result;
}

InspectionPlcRunSettingsResult InspectionRuntime::applyPlcRunSettings(
    const InspectionPlcRunSettings &settings)
{
    if (m_plcController) {
        const InspectionPlcRunSettingsResult result =
                m_plcController->applyRunSettings(settings);
        if (result.isSuccess()) {
            qCInfo(logDevice).noquote()
                    << QStringLiteral(
                        "event=plc.run_settings_written rejectTime=%1 rejectDistance=%2 photoTime=%3 photoDistance=%4")
                       .arg(settings.rejectTime)
                       .arg(settings.rejectDistance)
                       .arg(settings.photoTime)
                       .arg(settings.photoDistance);
        } else {
            qCCritical(logDevice).noquote()
                    << QStringLiteral(
                        "event=plc.run_settings_write_failed field=%1 nativeCode=%2")
                       .arg(plcRunSettingFieldName(result.failedField))
                       .arg(result.operation.nativeErrorCode);
        }
        return result;
    }
    InspectionPlcRunSettingsResult result;
    result.failedField = InspectionPlcRunSettingField::RejectTime;
    result.operation = PlcOperationResult(-1);
    qCCritical(logDevice).noquote()
            << "event=plc.run_settings_write_failed field=reject_time nativeCode=-1";
    return result;
}

PlcOperationResult InspectionRuntime::writePlcPhotoDistance(
    std::uint32_t photoDistance)
{
    const PlcOperationResult result = m_plcController
            ? m_plcController->writePhotoDistance(photoDistance)
            : PlcOperationResult(-1);
    if (result.isSuccess()) {
        qCInfo(logDevice).noquote()
                << QStringLiteral("event=plc.photo_distance_written value=%1")
                   .arg(photoDistance);
    } else {
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=plc.photo_distance_write_failed value=%1 nativeCode=%2")
                   .arg(photoDistance).arg(result.nativeErrorCode);
    }
    return result;
}

PlcOperationResult InspectionRuntime::writePlcResultValue(std::uint8_t value)
{
    const PlcOperationResult result = m_plcController
            ? m_plcController->writeResultValue(value)
            : PlcOperationResult(-1);
    if (result.isSuccess()) {
        qCInfo(logDevice).noquote()
                << QStringLiteral("event=plc.result_written value=%1")
                   .arg(static_cast<int>(value));
    } else {
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=plc.result_write_failed value=%1 nativeCode=%2")
                   .arg(static_cast<int>(value))
                   .arg(result.nativeErrorCode);
    }
    return result;
}

DetectionRuntimeReadiness InspectionRuntime::prepareDetection(
    DetectionMode mode) const
{
    return m_detectionRegistry->prepare(mode);
}

bool InspectionRuntime::startDetection(
    const ResultServiceRunConfiguration &resultConfiguration,
    QString *errorMessage)
{
    DetectionRegistryRequest request;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting
                || !m_runContext) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("检测尚未准备好，无法开始。");
            }
            return false;
        }
        request.mode = m_runContext->mode;
        request.preparedTemplates = m_runContext->preparedTemplates;
        request.multiTemplateSnapshot =
                m_runContext->multiTemplateSnapshot;
        request.tissueRoughnessThreshold =
                m_runContext->tissueRoughnessThreshold;
        request.framePreprocess = m_runContext->framePreprocess;
    }

    const DetectionWorker::FailureConsumer failureConsumer =
            [this](const QString &message) {
        enterFault(
                    InspectionFaultReason::RuntimeInvariantViolation,
                    message);
    };
    const DetectionPipelineCreationResult creation =
            m_detectionRegistry->create(request);
    if (!creation.isAccepted()) {
        if (errorMessage) {
            *errorMessage = creation.errorMessage;
        }
        return false;
    }

    const std::shared_ptr<DetectionWorker> worker(new DetectionWorker(
        1, creation.executor, m_resultService->completionConsumer(),
        failureConsumer));
    waitForDetectionWorkerStop();
    {
        std::lock_guard<std::mutex> workerLock(m_detectionWorkerMutex);
        std::lock_guard<std::mutex> stateLock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting
                || !m_uiCompletionMailbox.reopen()
                || !worker->start()) {
            m_uiCompletionMailbox.cancel();
            if (errorMessage) {
                *errorMessage = creation.startFailureMessage;
            }
            return false;
        }
        m_detectionWorker = worker;
        m_detectionWorkerActive.store(true);
        m_resultService->configureRun(resultConfiguration);
    }
    return true;
}

std::shared_ptr<const FrameData> InspectionRuntime::acceptFrame(
    const cv::Mat &image,
    quint64 frameNumber,
    int cameraIndex)
{
    if (image.empty()) {
        return std::shared_ptr<const FrameData>();
    }
    const std::chrono::steady_clock::time_point processingStartedAt =
            std::chrono::steady_clock::now();
    bool droppedAfterFault = false;
    std::shared_ptr<const FrameData> frame;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == InspectionRuntimeState::Fault) {
            ++m_faultSnapshot.postFaultDroppedFrameCount;
            droppedAfterFault = true;
        } else if (m_state == InspectionRuntimeState::Starting
                   || m_state == InspectionRuntimeState::Running) {
            ProductKey productKey;
            productKey.runId = m_runContext
                    ? m_runContext->runId
                    : QString();
            productKey.sequence = ++m_acceptedProductSequence;
            frame = makeFrameData(
                        productKey,
                        frameNumber > 0 ? frameNumber : productKey.sequence,
                        cameraIndex,
                        processingStartedAt,
                        image);
            m_acceptedFrames[productKey.sequence] = frame;
            m_products[productKey.sequence] = ProductProgress::Accepted;
        }
    }
    if (droppedAfterFault) {
        m_resultService->recordPostFaultDroppedFrame();
    }
    return frame;
}

DetectionCompletion InspectionRuntime::complete(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionResult &result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if ((m_state != InspectionRuntimeState::Starting
         && m_state != InspectionRuntimeState::Running
         && m_state != InspectionRuntimeState::Stopping)
            || !frame
            || frame->originalImage.empty()
            || !belongsToCurrentRun(frame->productKey)
            || frame->productKey.sequence <= m_lastCompletedProductSequence) {
        return DetectionCompletion();
    }
    const std::map<quint64, std::weak_ptr<const FrameData>>::iterator accepted =
            m_acceptedFrames.find(frame->productKey.sequence);
    const std::map<quint64, ProductProgress>::iterator product =
            m_products.find(frame->productKey.sequence);
    if (accepted == m_acceptedFrames.end()
            || product == m_products.end()
            || product->second != ProductProgress::Accepted) {
        return DetectionCompletion();
    }
    const std::shared_ptr<const FrameData> acceptedFrame =
            accepted->second.lock();
    if (!acceptedFrame
            || acceptedFrame->productKey.runId != frame->productKey.runId
            || acceptedFrame->productKey.sequence
               != frame->productKey.sequence) {
        return DetectionCompletion();
    }

    DetectionCompletion completion;
    completion.frame = frame;
    completion.result = result;
    m_acceptedFrames.erase(accepted);
    product->second = ProductProgress::AlgorithmCompleted;
    m_lastCompletedProductSequence = frame->productKey.sequence;
    ++m_completedProductCount;
    return completion;
}

bool InspectionRuntime::claimResult(const ProductKey &productKey)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Idle
            || !belongsToCurrentRun(productKey)) {
        return false;
    }
    const std::map<quint64, ProductProgress>::iterator product =
            m_products.find(productKey.sequence);
    if (product == m_products.end()
            || product->second != ProductProgress::AlgorithmCompleted) {
        return false;
    }
    product->second = ProductProgress::Claimed;
    return true;
}

bool InspectionRuntime::finalizeResultClaim(const ProductKey &productKey)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!belongsToCurrentRun(productKey)) {
        return false;
    }
    const std::map<quint64, ProductProgress>::iterator product =
            m_products.find(productKey.sequence);
    if (product == m_products.end()
            || product->second != ProductProgress::Claimed) {
        return false;
    }
    m_products.erase(product);
    return true;
}

void InspectionRuntime::requestDetectionWorkerStop()
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        m_detectionWorkerActive.store(false);
        m_uiCompletionMailbox.cancel();
        worker = m_detectionWorker;
    }
    if (worker) {
        worker->requestStop();
    }
}

void InspectionRuntime::waitForDetectionWorkerStop()
{
    requestDetectionWorkerStop();
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    if (!worker) {
        return;
    }
    worker->wait();
    std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
    if (m_detectionWorker == worker) {
        m_detectionWorker.reset();
    }
}

bool InspectionRuntime::belongsToCurrentRun(
    const ProductKey &productKey) const
{
    return productKey.isValid()
            && m_runContext
            && productKey.runId == m_runContext->runId;
}

bool InspectionRuntime::isDetectionWorkerActive() const
{
    return m_detectionWorkerActive.load();
}

std::size_t InspectionRuntime::detectionWorkerQueueCapacity() const
{
    std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
    return m_detectionWorker ? m_detectionWorker->queueCapacity() : 0;
}

bool InspectionRuntime::submitDetectionFrame(
    const std::shared_ptr<const FrameData> &frame)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return m_detectionWorkerActive.load()
            && worker
            && worker->submit(frame);
}

DetectionWorkSubmissionResult InspectionRuntime::trySubmitDetectionFrame(
    const std::shared_ptr<const FrameData> &frame)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return !m_detectionWorkerActive.load() || !worker
            ? DetectionWorkSubmissionResult::NotRunning
             : worker->trySubmit(frame);
}

bool InspectionRuntime::publishPresentation(
    const InspectionPresentation &presentation)
{
    if (!m_uiCompletionMailbox.submit(presentation)) {
        if (state() == InspectionRuntimeState::Running) {
            qCCritical(logRuntime).noquote()
                    << "event=presentation.submit_failed";
        }
        return false;
    }
    bool expected = false;
    if (!m_wakePosted.compare_exchange_strong(expected, true)) {
        return true;
    }
    if (!QMetaObject::invokeMethod(
                this,
                "drainPresentationMailbox",
                Qt::QueuedConnection)) {
        m_wakePosted.store(false);
        m_uiCompletionMailbox.cancel();
        if (state() == InspectionRuntimeState::Running) {
            qCCritical(logRuntime).noquote()
                    << "event=presentation.dispatch_failed";
        }
        return false;
    }
    return true;
}

void InspectionRuntime::publishImageSaveFailure(
    quint64 totalFailed,
    const QString &latestError)
{
    emit imageSaveFailed(totalFailed, latestError);
}

void InspectionRuntime::drainPresentationMailbox()
{
    m_wakePosted.store(false);
    InspectionPresentation presentation;
    if (!m_uiCompletionMailbox.processOne(&presentation)) {
        return;
    }
    emit presentationReady(presentation);
    if (m_uiCompletionMailbox.hasPending()) {
        bool expected = false;
        if (m_wakePosted.compare_exchange_strong(expected, true)) {
            if (!QMetaObject::invokeMethod(
                        this,
                        "drainPresentationMailbox",
                        Qt::QueuedConnection)) {
                m_wakePosted.store(false);
                m_uiCompletionMailbox.cancel();
            }
        }
    }
}

DetectionResultStatistics InspectionRuntime::statistics() const
{
    return m_resultService->statistics();
}

void InspectionRuntime::resetStatistics()
{
    m_resultService->resetStatistics();
}

void InspectionRuntime::resetNgCount()
{
    m_resultService->resetNgCount();
}

void InspectionRuntime::clearPendingDelayedNgRequests()
{
    m_resultService->clearPendingDelayedNgRequests();
}
