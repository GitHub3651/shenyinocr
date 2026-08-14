#pragma once

#include "runtime/frame_queue.h"

#include <QString>

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>

class DetectionWorker
{
public:
    typedef std::function<DetectionResult(
        const std::shared_ptr<const FrameData> &)> Detector;
    typedef std::function<DetectionResult(
        const DetectionWorkItem &)> WorkItemDetector;
    typedef std::function<void(
        const DetectionCompletion &)> CompletionConsumer;
    typedef std::function<void(const QString &)> FailureConsumer;

    DetectionWorker(
        std::size_t queueCapacity,
        const Detector &detector,
        const CompletionConsumer &completionConsumer,
        const FailureConsumer &failureConsumer = FailureConsumer());
    DetectionWorker(
        std::size_t queueCapacity,
        const WorkItemDetector &detector,
        const CompletionConsumer &completionConsumer,
        const FailureConsumer &failureConsumer = FailureConsumer());
    ~DetectionWorker();

    bool start();
    bool submit(const DetectionWorkItem &item);
    bool submit(const std::shared_ptr<const FrameData> &frame);
    void requestStop();
    void wait();

    bool isRunning() const;
    std::size_t queueCapacity() const;
    std::size_t queuedFrameCount() const;
    quint64 processedFrameCount() const;
    quint64 cancelledFrameCount() const;

private:
    void run();
    void reportFailure(const QString &message) const;

    FrameQueue m_queue;
    WorkItemDetector m_detector;
    CompletionConsumer m_completionConsumer;
    FailureConsumer m_failureConsumer;

    mutable std::mutex m_lifecycleMutex;
    std::thread m_thread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRequested;
    std::atomic<quint64> m_processedFrameCount;
    std::atomic<quint64> m_cancelledFrameCount;
};
