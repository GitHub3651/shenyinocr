// 文件作用：本文件用于从正式帧队列取帧、调用检测流水线并把唯一结果交给结果服务。
// 主要职责：从正式帧队列取帧、调用检测流水线并把唯一结果交给结果服务。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/detection_worker.h"

#include <exception>

// 函数说明：DetectionWorker 构造函数创建组件并初始化其依赖和初始状态。
DetectionWorker::DetectionWorker(
    std::size_t queueCapacity,
    const Executor &executor,
    const CompletionConsumer &completionConsumer,
    const FailureConsumer &failureConsumer)
    : m_queue(queueCapacity),
      m_executor(executor),
      m_completionConsumer(completionConsumer),
      m_failureConsumer(failureConsumer),
      m_running(false),
      m_stopRequested(true),
      m_processedFrameCount(0),
      m_cancelledFrameCount(0)
{
}

// 函数说明：~DetectionWorker 析构函数按生命周期要求释放组件持有的资源。
DetectionWorker::~DetectionWorker()
{
    requestStop();
    wait();
}

// 函数说明：start 函数创建、准备或启动对应流程。
bool DetectionWorker::start()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (m_thread.joinable()
            || m_running.load()
            || !m_executor
            || !m_completionConsumer
            || !m_queue.reopen()) {
        return false;
    }

    m_stopRequested.store(false);
    m_running.store(true);
    try {
        m_thread = std::thread(&DetectionWorker::run, this);
    } catch (const std::exception &error) {
        m_running.store(false);
        m_stopRequested.store(true);
        m_queue.cancel();
        reportFailure(QString::fromLocal8Bit(error.what()));
        return false;
    } catch (...) {
        m_running.store(false);
        m_stopRequested.store(true);
        m_queue.cancel();
        reportFailure(QStringLiteral("Unable to start detection worker"));
        return false;
    }
    return true;
}

// 函数说明：submit 函数执行对应事件或业务处理。
bool DetectionWorker::submit(
    const std::shared_ptr<const FrameData> &frame)
{
    if (!m_running.load() || m_stopRequested.load()) {
        return false;
    }
    return m_queue.submit(frame);
}

// 函数说明：trySubmit 函数实现名称所表示的处理步骤。
DetectionWorkSubmissionResult DetectionWorker::trySubmit(
    const std::shared_ptr<const FrameData> &frame)
{
    if (!frame || !frame->productKey.isValid()
            || frame->originalImage.empty()) {
        return DetectionWorkSubmissionResult::InvalidItem;
    }
    if (!m_running.load() || m_stopRequested.load()) {
        return DetectionWorkSubmissionResult::NotRunning;
    }
    const FrameQueueSubmitResult result = m_queue.trySubmit(frame);
    return result == FrameQueueSubmitResult::Accepted
            ? DetectionWorkSubmissionResult::Accepted
            : result == FrameQueueSubmitResult::InvalidItem
              ? DetectionWorkSubmissionResult::InvalidItem
              : result == FrameQueueSubmitResult::Full
                ? DetectionWorkSubmissionResult::QueueFull
                : DetectionWorkSubmissionResult::Cancelled;
}

// 函数说明：requestStop 函数实现名称所表示的处理步骤。
void DetectionWorker::requestStop()
{
    m_stopRequested.store(true);
    m_cancelledFrameCount.fetch_add(
                static_cast<quint64>(m_queue.cancel()));
}

// 函数说明：wait 函数读取、等待或计算对应的数据。
void DetectionWorker::wait()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (!m_thread.joinable()
            || m_thread.get_id() == std::this_thread::get_id()) {
        return;
    }
    m_thread.join();
}

// 函数说明：isRunning 函数检查相关状态并返回判断结果。
bool DetectionWorker::isRunning() const
{
    return m_running.load();
}

// 函数说明：queueCapacity 函数实现名称所表示的处理步骤。
std::size_t DetectionWorker::queueCapacity() const
{
    return m_queue.capacity();
}

// 函数说明：processedFrameCount 函数执行对应事件或业务处理。
quint64 DetectionWorker::processedFrameCount() const
{
    return m_processedFrameCount.load();
}

// 函数说明：cancelledFrameCount 函数检查相关状态并返回判断结果。
quint64 DetectionWorker::cancelledFrameCount() const
{
    return m_cancelledFrameCount.load();
}

// 函数说明：run 函数执行对应事件或业务处理。
void DetectionWorker::run()
{
    while (!m_stopRequested.load()) {
        std::shared_ptr<const FrameData> frame;
        if (!m_queue.waitAndTake(&frame)) {
            break;
        }
        if (m_stopRequested.load()) {
            m_cancelledFrameCount.fetch_add(1);
            break;
        }

        DetectionCompletion completion;
        try {
            completion = m_executor(frame);
        } catch (const std::exception &error) {
            reportFailure(QString::fromLocal8Bit(error.what()));
            m_cancelledFrameCount.fetch_add(1);
            requestStop();
            break;
        } catch (...) {
            reportFailure(QStringLiteral("Detection worker failed"));
            m_cancelledFrameCount.fetch_add(1);
            requestStop();
            break;
        }

        if (m_stopRequested.load()) {
            m_cancelledFrameCount.fetch_add(1);
            break;
        }

        if (!completion.isValid()) {
            reportFailure(QStringLiteral("检测执行器未返回有效结果"));
            m_cancelledFrameCount.fetch_add(1);
            requestStop();
            break;
        }
        try {
            m_completionConsumer(completion);
            m_processedFrameCount.fetch_add(1);
        } catch (const std::exception &error) {
            reportFailure(QString::fromLocal8Bit(error.what()));
            m_cancelledFrameCount.fetch_add(1);
            requestStop();
            break;
        } catch (...) {
            reportFailure(QStringLiteral("Detection completion consumer failed"));
            m_cancelledFrameCount.fetch_add(1);
            requestStop();
            break;
        }
    }
    m_running.store(false);
}

// 函数说明：reportFailure 函数实现名称所表示的处理步骤。
void DetectionWorker::reportFailure(const QString &message) const
{
    if (!m_failureConsumer) {
        return;
    }
    try {
        m_failureConsumer(message);
    } catch (...) {
    }
}
