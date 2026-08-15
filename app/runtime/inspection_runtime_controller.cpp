#include "runtime/inspection_runtime_controller.h"

#include <QDebug>

InspectionRuntimeController::InspectionRuntimeController(
    const RunIdFactory &runIdFactory,
    const std::shared_ptr<InspectionPlcController> &plcController)
    : m_session(runIdFactory),
      m_plcController(plcController),
      m_detectionWorkerModeIndex(-1)
{
}

InspectionRuntimeController::~InspectionRuntimeController()
{
    requestDetectionWorkerStop();
    waitForDetectionWorkerStop();
}

QString InspectionRuntimeController::beginStart()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Idle) {
        return QString();
    }

    m_state = InspectionRuntimeState::Starting;
    m_lastRecordedRunId.clear();
    m_lastRecordedProductSequence = 0;
    m_faultedPlcOutputSequences.clear();
    m_faultFallbackNgResolutionCount = 0;
    m_faultUnconfirmedProductCount = 0;
    const QString runId = m_session.begin();
    m_productReconciler.beginRun(runId);
    return runId;
}

bool InspectionRuntimeController::markRunning()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Starting) {
        return false;
    }

    m_state = InspectionRuntimeState::Running;
    return true;
}

bool InspectionRuntimeController::requestStop()
{
    bool stopAccepted = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == InspectionRuntimeState::Stopping) {
            stopAccepted = true;
        } else if (m_state == InspectionRuntimeState::Starting
                   || m_state == InspectionRuntimeState::Running) {
            m_state = InspectionRuntimeState::Stopping;
            stopAccepted = true;
        }
    }

    if (stopAccepted) {
        requestDetectionWorkerStop();
    }
    return stopAccepted;
}

void InspectionRuntimeController::finishStop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault) {
        m_state = InspectionRuntimeState::Idle;
    }
}

bool InspectionRuntimeController::enterFault(
    InspectionFaultReason reason,
    const QString &diagnostic,
    const QDateTime &occurredAtUtc)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if ((m_state != InspectionRuntimeState::Starting
             && m_state != InspectionRuntimeState::Running
             && m_state != InspectionRuntimeState::Stopping)
                || !m_faultState.enter(
                    reason,
                    diagnostic,
                    m_session.runId(),
                    m_session.acceptedProductCount(),
                    m_session.completedProductCount(),
                    occurredAtUtc)) {
            return false;
        }

        m_resultHandler.recordSystemFault();
        m_state = InspectionRuntimeState::Fault;
    }

    requestDetectionWorkerStop();
    return true;
}

bool InspectionRuntimeController::acknowledgeFault()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Fault) {
            return false;
        }
    }

    waitForDetectionWorkerStop();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault) {
        return false;
    }

    if (m_productReconciler.hasUnresolvedProducts()) {
        return false;
    }

    m_faultState.clear();
    m_state = InspectionRuntimeState::Idle;
    return true;
}

InspectionRuntimeState InspectionRuntimeController::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

InspectionFaultSnapshot InspectionRuntimeController::faultSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultState.snapshot();
}

bool InspectionRuntimeController::isBusy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Starting
            || m_state == InspectionRuntimeState::Running
            || m_state == InspectionRuntimeState::Stopping
            || m_state == InspectionRuntimeState::Fault;
}

bool InspectionRuntimeController::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Running;
}

QString InspectionRuntimeController::runId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.runId();
}

quint64 InspectionRuntimeController::completedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.completedProductCount();
}

quint64 InspectionRuntimeController::acceptedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.acceptedProductCount();
}

bool InspectionRuntimeController::hasPlcController() const
{
    return static_cast<bool>(m_plcController)
            && m_plcController->hasDevice();
}

bool InspectionRuntimeController::isPlcConnected() const
{
    return m_plcController
            && m_plcController->isConnected();
}

PlcOperationResult InspectionRuntimeController::connectPlc(
    const QString &address,
    int rack,
    int slot)
{
    return m_plcController
            ? m_plcController->connectTo(address, rack, slot)
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntimeController::disconnectPlc()
{
    return m_plcController
            ? m_plcController->disconnect()
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntimeController::writePlcTriggerMode(
    int modeIndex)
{
    return m_plcController
            ? m_plcController->writeTriggerMode(modeIndex)
            : PlcOperationResult(-1);
}

InspectionPlcRunSettingsResult
InspectionRuntimeController::applyPlcRunSettings(
    const InspectionPlcRunSettings &settings)
{
    if (m_plcController) {
        return m_plcController->applyRunSettings(settings);
    }

    InspectionPlcRunSettingsResult result;
    result.failedField = InspectionPlcRunSettingField::RejectTime;
    result.operation = PlcOperationResult(-1);
    return result;
}

PlcOperationResult InspectionRuntimeController::writePlcPhotoDistance(
    std::uint32_t photoDistance)
{
    return m_plcController
            ? m_plcController->writePhotoDistance(photoDistance)
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntimeController::writePlcResultValue(
    std::uint8_t value)
{
    return m_plcController
            ? m_plcController->writeResultValue(value)
            : PlcOperationResult(-1);
}

std::shared_ptr<const FrameData> InspectionRuntimeController::acceptFrame(
    const cv::Mat &image,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Fault) {
        if (!image.empty()
                && m_faultState.notePostFaultDroppedFrame()) {
            m_resultHandler.recordPostFaultDroppedFrame();
        }
        return std::shared_ptr<const FrameData>();
    }
    if (m_state != InspectionRuntimeState::Starting
            && m_state != InspectionRuntimeState::Running) {
        return std::shared_ptr<const FrameData>();
    }

    const std::shared_ptr<const FrameData> frame = m_session.acceptFrame(
                image,
                frameNumber,
                cameraIndex,
                timestampUtc);
    if (frame) {
        m_productReconciler.noteAccepted(frame->productKey);
    }
    return frame;
}

DetectionCompletion InspectionRuntimeController::complete(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionResult &result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault) {
        return DetectionCompletion();
    }

    const DetectionCompletion completion = m_session.complete(frame, result);
    if (completion.isValid()) {
        m_productReconciler.noteAlgorithmCompleted(
                    completion.frame->productKey);
    }
    return completion;
}

DetectionCompletion InspectionRuntimeController::complete(
    const cv::Mat &image,
    const DetectionResult &result,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault) {
        return DetectionCompletion();
    }

    const std::shared_ptr<const FrameData> frame = m_session.acceptFrame(
                image,
                frameNumber,
                cameraIndex,
                timestampUtc);
    if (!frame
            || !m_productReconciler.noteAccepted(
                frame->productKey)) {
        return DetectionCompletion();
    }
    const DetectionCompletion completion = m_session.complete(frame, result);
    if (completion.isValid()) {
        m_productReconciler.noteAlgorithmCompleted(
                    completion.frame->productKey);
    }
    return completion;
}

DetectionResultHandlingOutcome InspectionRuntimeController::record(
    const DetectionCompletion &completion,
    int imageSaveModeIndex,
    int delayedNgOffset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    DetectionResultHandlingOutcome rejected;
    rejected.statistics = m_resultHandler.statistics();
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault
            || !completion.isValid()) {
        return rejected;
    }

    const ProductKey &productKey = completion.frame->productKey;
    if (productKey.runId != m_session.runId()
            || (productKey.runId == m_lastRecordedRunId
                && productKey.sequence
                   <= m_lastRecordedProductSequence)
            || !m_productReconciler.canRecordResult(productKey)) {
        return rejected;
    }

    const DetectionResultHandlingOutcome outcome = m_resultHandler.record(
                completion,
                imageSaveModeIndex,
                delayedNgOffset);
    if (outcome.resultRecorded) {
        m_lastRecordedRunId = productKey.runId;
        m_lastRecordedProductSequence = productKey.sequence;
        m_productReconciler.noteResultRecorded(productKey);
    }
    return outcome;
}

bool InspectionRuntimeController::consumeDueDelayedNgRequest(
    ProductKey *productKey)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.consumeDueDelayedNgRequest(productKey);
}

std::vector<InspectionFaultProductAction>
InspectionRuntimeController::faultProductActions(bool plcWritable) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault) {
        return std::vector<InspectionFaultProductAction>();
    }
    return m_productReconciler.faultActions(plcWritable);
}

bool InspectionRuntimeController::resolveFaultProduct(
    const ProductKey &productKey,
    InspectionFaultProductResolution resolution)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault
            || !m_productReconciler.resolve(
                productKey,
                resolution)) {
        return false;
    }

    if (resolution
            == InspectionFaultProductResolution::FallbackNgRequested) {
        m_resultHandler.recordCancelledProduct();
        ++m_faultFallbackNgResolutionCount;
    } else {
        m_resultHandler.recordUnconfirmedProduct();
        ++m_faultUnconfirmedProductCount;
    }
    return true;
}

bool InspectionRuntimeController::recordFaultedPlcOutput(
    const ProductKey &productKey)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault
            || !productKey.isValid()
            || productKey.runId != m_session.runId()
            || productKey.sequence > m_lastRecordedProductSequence
            || !m_faultedPlcOutputSequences.insert(
                productKey.sequence).second) {
        return false;
    }

    m_resultHandler.recordUnconfirmedProduct();
    ++m_faultUnconfirmedProductCount;
    return true;
}

int InspectionRuntimeController::unresolvedFaultProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_productReconciler.unresolvedProductCount();
}

int InspectionRuntimeController::faultedPlcOutputCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_faultedPlcOutputSequences.size());
}

int InspectionRuntimeController::faultFallbackNgResolutionCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultFallbackNgResolutionCount;
}

int InspectionRuntimeController::faultUnconfirmedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultUnconfirmedProductCount;
}

DetectionResultStatistics InspectionRuntimeController::statistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.statistics();
}

DetectionAbnormalStatistics
InspectionRuntimeController::abnormalStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.abnormalStatistics();
}

int InspectionRuntimeController::totalCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.totalCount();
}

int InspectionRuntimeController::ngCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.ngCount();
}

int InspectionRuntimeController::pendingDelayedNgCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.pendingDelayedNgCount();
}

void InspectionRuntimeController::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.resetStatistics();
}

void InspectionRuntimeController::resetAbnormalStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.resetAbnormalStatistics();
}

void InspectionRuntimeController::resetNgCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.resetNgCount();
}

void InspectionRuntimeController::clearPendingDelayedNgRequests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.clearPendingDelayedNgRequests();
}

bool InspectionRuntimeController::startDetectionWorker(
    int modeIndex,
    const std::shared_ptr<DetectionWorker> &worker)
{
    if (modeIndex < 0 || !worker) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting) {
            return false;
        }
    }

    waitForDetectionWorkerStop();
    {
        std::lock_guard<std::mutex> workerLock(
                    m_detectionWorkerMutex);
        std::lock_guard<std::mutex> stateLock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting) {
            return false;
        }
        if (!m_uiCompletionMailbox.reopen()
                || !worker->start()) {
            m_uiCompletionMailbox.cancel();
            return false;
        }
        m_detectionWorker = worker;
        m_detectionWorkerModeIndex.store(modeIndex);
    }
    return true;
}

void InspectionRuntimeController::requestDetectionWorkerStop()
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        m_detectionWorkerModeIndex.store(-1);
        m_uiCompletionMailbox.cancel();
        worker = m_detectionWorker;
    }
    if (worker) {
        worker->requestStop();
    }
}

void InspectionRuntimeController::waitForDetectionWorkerStop()
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
    qDebug() << "[DETECTION_WORKER] worker stopped"
             << "processed=" << worker->processedFrameCount()
             << "cancelled=" << worker->cancelledFrameCount();
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        if (m_detectionWorker == worker) {
            m_detectionWorker.reset();
        }
    }
}

bool InspectionRuntimeController::isDetectionWorkerActive() const
{
    return m_detectionWorkerModeIndex.load() >= 0;
}

bool InspectionRuntimeController::isDetectionWorkerActiveForMode(
    int modeIndex) const
{
    return modeIndex >= 0
            && m_detectionWorkerModeIndex.load() == modeIndex;
}

int InspectionRuntimeController::detectionWorkerModeIndex() const
{
    return m_detectionWorkerModeIndex.load();
}

std::size_t InspectionRuntimeController::detectionWorkerQueueCapacity() const
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return worker ? worker->queueCapacity() : 0;
}

bool InspectionRuntimeController::submitDetectionFrame(
    const std::shared_ptr<const FrameData> &frame)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return isDetectionWorkerActive()
            && worker
            && worker->submit(frame);
}

bool InspectionRuntimeController::submitDetectionWorkItem(
    const DetectionWorkItem &item)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return isDetectionWorkerActive()
            && worker
            && worker->submit(item);
}

DetectionWorkSubmissionResult
InspectionRuntimeController::trySubmitDetectionFrame(
    const std::shared_ptr<const FrameData> &frame)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    if (!isDetectionWorkerActive() || !worker) {
        return DetectionWorkSubmissionResult::NotRunning;
    }
    return worker->trySubmit(frame);
}

DetectionWorkSubmissionResult
InspectionRuntimeController::trySubmitDetectionWorkItem(
    const DetectionWorkItem &item)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    if (!isDetectionWorkerActive() || !worker) {
        return DetectionWorkSubmissionResult::NotRunning;
    }
    return worker->trySubmit(item);
}

bool InspectionRuntimeController::submitUiCompletion(
    const UiCompletionMailbox::Work &work)
{
    return m_uiCompletionMailbox.submit(work);
}

bool InspectionRuntimeController::processOneUiCompletion()
{
    return m_uiCompletionMailbox.processOne();
}

void InspectionRuntimeController::cancelUiCompletion()
{
    m_uiCompletionMailbox.cancel();
}
