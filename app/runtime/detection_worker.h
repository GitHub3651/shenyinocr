#pragma once

#include "runtime/frame_queue.h"

#include <QString>

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>

enum class DetectionWorkSubmissionResult
{
    Accepted,
    InvalidItem,
    NotRunning,
    Cancelled,
    QueueFull
};

class DetectionWorker
{
public:
    typedef std::function<DetectionCompletion(
        const std::shared_ptr<const FrameData> &)> Executor;
    typedef std::function<void(
        const DetectionCompletion &)> CompletionConsumer;
    typedef std::function<void(const QString &)> FailureConsumer;

    DetectionWorker(
        std::size_t queueCapacity,
        const Executor &executor,
        const CompletionConsumer &completionConsumer,
        const FailureConsumer &failureConsumer);
    ~DetectionWorker();

    bool start();
    bool submit(const std::shared_ptr<const FrameData> &frame);
    DetectionWorkSubmissionResult trySubmit(
        const std::shared_ptr<const FrameData> &frame);
    void requestStop();
    void wait();

    bool isRunning() const;
    std::size_t queueCapacity() const;
    quint64 processedFrameCount() const;

private:
    void run();
    void reportFailure(const QString &message) const;

    FrameQueue m_queue;
    Executor m_executor;
    CompletionConsumer m_completionConsumer;
    FailureConsumer m_failureConsumer;

    mutable std::mutex m_lifecycleMutex;
    std::thread m_thread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRequested;
    std::atomic<quint64> m_processedFrameCount;
    std::atomic<quint64> m_cancelledFrameCount;
};
