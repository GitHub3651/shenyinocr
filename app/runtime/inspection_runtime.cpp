// 文件作用：本文件用于管理正式检测生命周期、线程、队列、故障状态和结果链的唯一运行实例。
// 主要职责：管理正式检测生命周期、线程、队列、故障状态和结果链的唯一运行实例。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/inspection_runtime.h"

#include <QDebug>
#include <QUuid>

#include <stdexcept>

// 函数说明：InspectionRuntime 构造函数创建组件并初始化其依赖和初始状态。
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

// 函数说明：~InspectionRuntime 析构函数按生命周期要求释放组件持有的资源。
InspectionRuntime::~InspectionRuntime()
{
    requestDetectionWorkerStop();
    waitForDetectionWorkerStop();
}

// 函数说明：createRunId 函数创建、准备或启动对应流程。
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

// 函数说明：beginStart 函数创建、准备或启动对应流程。
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

// 函数说明：commitStart 函数保存或发布对应的数据和资源。
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

// 函数说明：rollbackStart 函数实现名称所表示的处理步骤。
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

// 函数说明：beginStop 函数创建、准备或启动对应流程。
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

// 函数说明：waitForStop 函数读取、等待或计算对应的数据。
void InspectionRuntime::waitForStop()
{
    waitForDetectionWorkerStop();
}

// 函数说明：finishStop 函数实现名称所表示的处理步骤。
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

// 函数说明：enterFault 函数实现名称所表示的处理步骤。
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

// 函数说明：reconcileFaultProducts 函数实现名称所表示的处理步骤。
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

// 函数说明：acknowledgeFault 函数停止流程、清理状态或释放对应资源。
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

// 函数说明：state 函数实现名称所表示的处理步骤。
InspectionRuntimeState InspectionRuntime::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

// 函数说明：faultSnapshot 函数实现名称所表示的处理步骤。
InspectionFaultSnapshot InspectionRuntime::faultSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultSnapshot;
}

// 函数说明：isBusy 函数检查相关状态并返回判断结果。
bool InspectionRuntime::isBusy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state != InspectionRuntimeState::Idle;
}

// 函数说明：isRunning 函数检查相关状态并返回判断结果。
bool InspectionRuntime::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Running;
}

// 函数说明：runId 函数执行对应事件或业务处理。
QString InspectionRuntime::runId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runContext ? m_runContext->runId : QString();
}

// 函数说明：acceptedProductCount 函数实现名称所表示的处理步骤。
quint64 InspectionRuntime::acceptedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_acceptedProductSequence;
}

// 函数说明：completedProductCount 函数实现名称所表示的处理步骤。
quint64 InspectionRuntime::completedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedProductCount;
}

// 函数说明：unresolvedFaultProductCount 函数实现名称所表示的处理步骤。
int InspectionRuntime::unresolvedFaultProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_products.size());
}

// 函数说明：faultUnconfirmedProductCount 函数实现名称所表示的处理步骤。
int InspectionRuntime::faultUnconfirmedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_faultUnconfirmedProductCount;
}

// 函数说明：isPlcConnected 函数检查相关状态并返回判断结果。
bool InspectionRuntime::isPlcConnected() const
{
    return m_plcController && m_plcController->isConnected();
}

// 函数说明：connectPlc 函数建立或断开对应外部连接。
PlcOperationResult InspectionRuntime::connectPlc(
    const QString &address,
    int rack,
    int slot)
{
    return m_plcController
            ? m_plcController->connectTo(address, rack, slot)
            : PlcOperationResult(-1);
}

// 函数说明：disconnectPlc 函数建立或断开对应外部连接。
PlcOperationResult InspectionRuntime::disconnectPlc()
{
    return m_plcController
            ? m_plcController->disconnect()
            : PlcOperationResult(-1);
}

// 函数说明：writePlcTriggerMode 函数保存或发布对应的数据和资源。
PlcOperationResult InspectionRuntime::writePlcTriggerMode(int modeIndex)
{
    return m_plcController
            ? m_plcController->writeTriggerMode(modeIndex)
            : PlcOperationResult(-1);
}

// 函数说明：applyPlcRunSettings 函数更新或应用对应的配置和状态。
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

// 函数说明：writePlcPhotoDistance 函数保存或发布对应的数据和资源。
PlcOperationResult InspectionRuntime::writePlcPhotoDistance(
    std::uint32_t photoDistance)
{
    return m_plcController
            ? m_plcController->writePhotoDistance(photoDistance)
            : PlcOperationResult(-1);
}

// 函数说明：writePlcResultValue 函数保存或发布对应的数据和资源。
PlcOperationResult InspectionRuntime::writePlcResultValue(std::uint8_t value)
{
    return m_plcController
            ? m_plcController->writeResultValue(value)
            : PlcOperationResult(-1);
}

// 函数说明：preparePipeline 函数创建、准备或启动对应流程。
BarcodeRuntimeReadiness InspectionRuntime::preparePipeline(
    DetectionMode mode) const
{
    return m_pipelineRegistry->prepare(mode);
}

// 函数说明：startPipeline 函数创建、准备或启动对应流程。
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
        m_detectionWorkerActive.store(true);
        m_resultService->configureRun(resultConfiguration);
    }
    qDebug() << "[DETECTION_WORKER]" << creation.workerLogName
             << "worker started queueCapacity="
             << static_cast<qulonglong>(creation.worker->queueCapacity());
    return true;
}

// 函数说明：acceptFrame 函数实现名称所表示的处理步骤。
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

// 函数说明：complete 函数实现名称所表示的处理步骤。
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

// 函数说明：claimResult 函数实现名称所表示的处理步骤。
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

// 函数说明：requestDetectionWorkerStop 函数实现名称所表示的处理步骤。
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

// 函数说明：waitForDetectionWorkerStop 函数读取、等待或计算对应的数据。
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

// 函数说明：belongsToCurrentRun 函数实现名称所表示的处理步骤。
bool InspectionRuntime::belongsToCurrentRun(
    const ProductKey &productKey) const
{
    return productKey.isValid()
            && m_runContext
            && productKey.runId == m_runContext->runId;
}

std::shared_ptr<const InspectionRunContext>
// 函数说明：runContext 函数执行对应事件或业务处理。
InspectionRuntime::runContext() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runContext;
}

// 函数说明：isDetectionWorkerActive 函数检查相关状态并返回判断结果。
bool InspectionRuntime::isDetectionWorkerActive() const
{
    return m_detectionWorkerActive.load();
}

// 函数说明：detectionWorkerQueueCapacity 函数执行对应事件或业务处理。
std::size_t InspectionRuntime::detectionWorkerQueueCapacity() const
{
    std::lock_guard<std::mutex> lock(m_detectionWorkerMutex);
    return m_detectionWorker ? m_detectionWorker->queueCapacity() : 0;
}

// 函数说明：submitDetectionFrame 函数执行对应事件或业务处理。
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

// 函数说明：submitDetectionWorkItem 函数执行对应事件或业务处理。
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

// 函数说明：trySubmitDetectionFrame 函数实现名称所表示的处理步骤。
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

// 函数说明：trySubmitDetectionWorkItem 函数实现名称所表示的处理步骤。
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

// 函数说明：submitUiCompletion 函数执行对应事件或业务处理。
bool InspectionRuntime::submitUiCompletion(
    const UiCompletionMailbox::Work &work)
{
    return m_uiCompletionMailbox.submit(work);
}

// 函数说明：processOneUiCompletion 函数执行对应事件或业务处理。
bool InspectionRuntime::processOneUiCompletion()
{
    return m_uiCompletionMailbox.processOne();
}

// 函数说明：cancelUiCompletion 函数检查相关状态并返回判断结果。
void InspectionRuntime::cancelUiCompletion()
{
    m_uiCompletionMailbox.cancel();
}

// 函数说明：resultService 函数实现名称所表示的处理步骤。
ResultService &InspectionRuntime::resultService()
{
    return *m_resultService;
}

// 函数说明：resultService 函数实现名称所表示的处理步骤。
const ResultService &InspectionRuntime::resultService() const
{
    return *m_resultService;
}

// 函数说明：statistics 函数实现名称所表示的处理步骤。
DetectionResultStatistics InspectionRuntime::statistics() const
{
    return m_resultService->statistics();
}

// 函数说明：abnormalStatistics 函数实现名称所表示的处理步骤。
DetectionAbnormalStatistics InspectionRuntime::abnormalStatistics() const
{
    return m_resultService->abnormalStatistics();
}

// 函数说明：totalCount 函数校验、转换或恢复对应数据。
int InspectionRuntime::totalCount() const
{
    return m_resultService->totalCount();
}

// 函数说明：ngCount 函数实现名称所表示的处理步骤。
int InspectionRuntime::ngCount() const
{
    return m_resultService->ngCount();
}

// 函数说明：pendingDelayedNgCount 函数实现名称所表示的处理步骤。
int InspectionRuntime::pendingDelayedNgCount() const
{
    return m_resultService->pendingDelayedNgCount();
}

// 函数说明：resetStatistics 函数停止流程、清理状态或释放对应资源。
void InspectionRuntime::resetStatistics()
{
    m_resultService->resetStatistics();
}

// 函数说明：resetAbnormalStatistics 函数停止流程、清理状态或释放对应资源。
void InspectionRuntime::resetAbnormalStatistics()
{
    m_resultService->resetAbnormalStatistics();
}

// 函数说明：resetNgCount 函数停止流程、清理状态或释放对应资源。
void InspectionRuntime::resetNgCount()
{
    m_resultService->resetNgCount();
}

// 函数说明：clearPendingDelayedNgRequests 函数停止流程、清理状态或释放对应资源。
void InspectionRuntime::clearPendingDelayedNgRequests()
{
    m_resultService->clearPendingDelayedNgRequests();
}
