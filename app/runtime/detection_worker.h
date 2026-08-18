// 文件作用：本文件用于从正式帧队列取帧、调用检测流水线并把唯一结果交给结果服务。
// 主要职责：从正式帧队列取帧、调用检测流水线并把唯一结果交给结果服务。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "runtime/frame_queue.h"

#include <QString>

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>

// 组件说明：DetectionWorkSubmissionResult 枚举列出该组件允许使用的稳定状态和选项。
enum class DetectionWorkSubmissionResult
{
    Accepted,
    InvalidItem,
    NotRunning,
    Cancelled,
    QueueFull
};

// 组件说明：DetectionWorker 组件封装对应业务职责和生命周期边界。
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
    DetectionWorkSubmissionResult trySubmit(
        const DetectionWorkItem &item);
    DetectionWorkSubmissionResult trySubmit(
        const std::shared_ptr<const FrameData> &frame);
    void requestStop();
    void wait();

    bool isRunning() const;
    std::size_t queueCapacity() const;
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
