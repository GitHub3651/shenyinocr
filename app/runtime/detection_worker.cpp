#include "runtime/detection_worker.h"

#include <exception>

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

DetectionWorker::~DetectionWorker()
{
    requestStop();
    wait();
}

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

bool DetectionWorker::submit(
    const std::shared_ptr<const FrameData> &frame)
{
    if (!m_running.load() || m_stopRequested.load()) {
        return false;
    }
    return m_queue.submit(frame);
}

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

void DetectionWorker::requestStop()
{
    m_stopRequested.store(true);
    m_cancelledFrameCount.fetch_add(
                static_cast<quint64>(m_queue.cancel()));
}

void DetectionWorker::wait()
{
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (!m_thread.joinable()
            || m_thread.get_id() == std::this_thread::get_id()) {
        return;
    }
    m_thread.join();
}

bool DetectionWorker::isRunning() const
{
    return m_running.load();
}

std::size_t DetectionWorker::queueCapacity() const
{
    return m_queue.capacity();
}

quint64 DetectionWorker::processedFrameCount() const
{
    return m_processedFrameCount.load();
}

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
