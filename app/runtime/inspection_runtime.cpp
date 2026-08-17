#include "runtime/inspection_runtime.h"

#include <QDebug>
#include <QUuid>

#include <stdexcept>

InspectionRuntime::InspectionRuntime(
    const RunIdFactory &runIdFactory,
    const std::shared_ptr<InspectionPlcController> &plcController,
    const std::shared_ptr<PipelineRegistry> &pipelineRegistry)
    : m_runIdFactory(runIdFactory),
      m_plcController(plcController),
      m_pipelineRegistry(pipelineRegistry)
{
    if (!m_pipelineRegistry) {
        throw std::invalid_argument("PipelineRegistry is required");
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
    const MachineSettings &machineSettings,
    const PreparedRecipeSnapshot &preparedRecipe,
    const InspectionProfileSnapshot &profileSnapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Idle
            || !preparedRecipe
            || !preparedRecipe->recipe) {
        return QString();
    }

    const QString newRunId = createRunId();
    m_state = InspectionRuntimeState::Starting;
    m_runContext.reset(new InspectionRunContext(
        newRunId,
        QDateTime::currentDateTimeUtc(),
        machineSettings,
        preparedRecipe,
        profileSnapshot));
    m_acceptedProductSequence = 0;
    m_completedProductCount = 0;
    m_lastCompletedProductSequence = 0;
    m_faultUnconfirmedProductCount = 0;
    m_faultSnapshot = InspectionFaultSnapshot();
    m_acceptedFrames.clear();
    m_products.clear();
    return newRunId;
}

bool InspectionRuntime::commitStart()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Starting
            || !m_detectionWorkerActive.load()) {
        return false;
    }
    m_state = InspectionRuntimeState::Running;
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
    finishStop();
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
        requestDetectionWorkerStop();
    }
    return accepted;
}

void InspectionRuntime::waitForStop()
{
    waitForDetectionWorkerStop();
}

void InspectionRuntime::finishStop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault) {
        m_state = InspectionRuntimeState::Idle;
        m_runContext.reset();
        m_products.clear();
        m_acceptedFrames.clear();
    }
}

bool InspectionRuntime::enterFault(
    InspectionFaultReason reason,
    const QString &diagnostic,
    const QDateTime &occurredAtUtc)
{
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
    }

    m_resultService->recordSystemFault();
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
        m_faultUnconfirmedProductCount += count;
        m_products.clear();
        m_acceptedFrames.clear();
    }
    m_resultService->recordUnconfirmedProducts(count);
    return count;
}

bool InspectionRuntime::acknowledgeFault()
{
    waitForDetectionWorkerStop();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Fault
            || !m_products.empty()
            || m_detectionWorkerActive.load()) {
        return false;
    }
    m_faultSnapshot = InspectionFaultSnapshot();
    m_state = InspectionRuntimeState::Idle;
    m_runContext.reset();
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

quint64 InspectionRuntime::acceptedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_acceptedProductSequence;
}

quint64 InspectionRuntime::completedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedProductCount;
}

int InspectionRuntime::unresolvedFaultProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_products.size());
}

int InspectionRuntime::faultUnconfirmedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultUnconfirmedProductCount;
}

bool InspectionRuntime::hasPlcController() const
{
    return m_plcController && m_plcController->hasDevice();
}

bool InspectionRuntime::isPlcConnected() const
{
    return m_plcController && m_plcController->isConnected();
}

PlcOperationResult InspectionRuntime::connectPlc(
    const QString &address,
    int rack,
    int slot)
{
    return m_plcController
            ? m_plcController->connectTo(address, rack, slot)
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntime::disconnectPlc()
{
    return m_plcController
            ? m_plcController->disconnect()
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntime::writePlcTriggerMode(int modeIndex)
{
    return m_plcController
            ? m_plcController->writeTriggerMode(modeIndex)
            : PlcOperationResult(-1);
}

InspectionPlcRunSettingsResult InspectionRuntime::applyPlcRunSettings(
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

PlcOperationResult InspectionRuntime::writePlcPhotoDistance(
    std::uint32_t photoDistance)
{
    return m_plcController
            ? m_plcController->writePhotoDistance(photoDistance)
            : PlcOperationResult(-1);
}

PlcOperationResult InspectionRuntime::writePlcResultValue(std::uint8_t value)
{
    return m_plcController
            ? m_plcController->writeResultValue(value)
            : PlcOperationResult(-1);
}

BarcodeRuntimeReadiness InspectionRuntime::preparePipeline(
    DetectionMode mode) const
{
    return m_pipelineRegistry->prepare(mode);
}

bool InspectionRuntime::startPipeline(
    const ResultServiceRunConfiguration &resultConfiguration,
    QString *errorMessage)
{
    PipelineRegistryRequest request;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting
                || !m_runContext
                || !m_runContext->preparedRecipe
                || !m_runContext->preparedRecipe->recipe) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("检测运行时未处于启动状态。");
            }
            return false;
        }
        request.mode = m_runContext->preparedRecipe->recipe->detectionMode;
        request.preparedRecipe = m_runContext->preparedRecipe;
        request.profileSnapshot = m_runContext->profileSnapshot;
    }

    const DetectionWorker::FailureConsumer failureConsumer =
            [this](const QString &message) {
        enterFault(
                    InspectionFaultReason::RuntimeInvariantViolation,
                    message);
    };
    const PipelineCreationResult creation = m_pipelineRegistry->create(
                request,
                m_resultService->pipelineConsumers(),
                failureConsumer);
    if (!creation.isAccepted()) {
        if (errorMessage) {
            *errorMessage = creation.errorMessage;
        }
        return false;
    }

    waitForDetectionWorkerStop();
    {
        std::lock_guard<std::mutex> workerLock(m_detectionWorkerMutex);
        std::lock_guard<std::mutex> stateLock(m_mutex);
        if (m_state != InspectionRuntimeState::Starting
                || !m_uiCompletionMailbox.reopen()
                || !creation.worker->start()) {
            m_uiCompletionMailbox.cancel();
            if (errorMessage) {
                *errorMessage = creation.startFailureMessage;
            }
            return false;
        }
        m_detectionWorker = creation.worker;
        m_detectionWorkerMode.store(request.mode);
        m_detectionWorkerActive.store(true);
        m_resultService->configureRun(resultConfiguration);
    }
    qDebug() << "[DETECTION_WORKER]" << creation.workerLogName
             << "worker started queueCapacity="
             << static_cast<qulonglong>(creation.worker->queueCapacity());
    return true;
}

std::shared_ptr<const FrameData> InspectionRuntime::acceptFrame(
    const cv::Mat &image,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    bool droppedAfterFault = false;
    std::shared_ptr<const FrameData> frame;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (image.empty()) {
            return frame;
        }
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
                        timestampUtc.isValid()
                            ? timestampUtc
                            : QDateTime::currentDateTimeUtc(),
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
    if (!acceptedFrame || acceptedFrame.get() != frame.get()) {
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
    qDebug() << "[DETECTION_WORKER] worker stopped processed="
             << worker->processedFrameCount()
             << "cancelled=" << worker->cancelledFrameCount();
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

std::shared_ptr<const InspectionRunContext>
InspectionRuntime::runContext() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runContext;
}

bool InspectionRuntime::isDetectionWorkerActive() const
{
    return m_detectionWorkerActive.load();
}

bool InspectionRuntime::isDetectionWorkerActiveForMode(
    DetectionMode mode) const
{
    return m_detectionWorkerActive.load()
            && m_detectionWorkerMode.load() == mode;
}

DetectionMode InspectionRuntime::detectionWorkerMode() const
{
    return m_detectionWorkerMode.load();
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

bool InspectionRuntime::submitDetectionWorkItem(
    const DetectionWorkItem &item)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return m_detectionWorkerActive.load()
            && worker
            && worker->submit(item);
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

DetectionWorkSubmissionResult InspectionRuntime::trySubmitDetectionWorkItem(
    const DetectionWorkItem &item)
{
    std::shared_ptr<DetectionWorker> worker;
    {
        std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
        worker = m_detectionWorker;
    }
    return !m_detectionWorkerActive.load() || !worker
            ? DetectionWorkSubmissionResult::NotRunning
            : worker->trySubmit(item);
}

bool InspectionRuntime::submitUiCompletion(
    const UiCompletionMailbox::Work &work)
{
    return m_uiCompletionMailbox.submit(work);
}

bool InspectionRuntime::processOneUiCompletion()
{
    return m_uiCompletionMailbox.processOne();
}

void InspectionRuntime::cancelUiCompletion()
{
    m_uiCompletionMailbox.cancel();
}

ResultService &InspectionRuntime::resultService()
{
    return *m_resultService;
}

const ResultService &InspectionRuntime::resultService() const
{
    return *m_resultService;
}

DetectionResultStatistics InspectionRuntime::statistics() const
{
    return m_resultService->statistics();
}

DetectionAbnormalStatistics InspectionRuntime::abnormalStatistics() const
{
    return m_resultService->abnormalStatistics();
}

int InspectionRuntime::totalCount() const
{
    return m_resultService->totalCount();
}

int InspectionRuntime::ngCount() const
{
    return m_resultService->ngCount();
}

int InspectionRuntime::pendingDelayedNgCount() const
{
    return m_resultService->pendingDelayedNgCount();
}

void InspectionRuntime::resetStatistics()
{
    m_resultService->resetStatistics();
}

void InspectionRuntime::resetAbnormalStatistics()
{
    m_resultService->resetAbnormalStatistics();
}

void InspectionRuntime::resetNgCount()
{
    m_resultService->resetNgCount();
}

void InspectionRuntime::clearPendingDelayedNgRequests()
{
    m_resultService->clearPendingDelayedNgRequests();
}
