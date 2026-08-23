// 文件作用：本文件用于提供容量受控的正式帧队列，并支持阻塞提交、取出和协作取消。
// 主要职责：提供容量受控的正式帧队列，并支持阻塞提交、取出和协作取消。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/common/detection_pose.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

// 组件说明：FrameQueueSubmitResult 枚举列出该组件允许使用的稳定状态和选项。
enum class FrameQueueSubmitResult
{
    Accepted,
    InvalidItem,
    Cancelled,
    Full
};

// 组件说明：FrameQueue 组件封装本文件中与其名称对应的单一职责。
class FrameQueue
{
public:
    explicit FrameQueue(std::size_t capacity);
    ~FrameQueue();

    bool submit(const std::shared_ptr<const FrameData> &frame);
    FrameQueueSubmitResult trySubmit(
        const std::shared_ptr<const FrameData> &frame);
    bool waitAndTake(std::shared_ptr<const FrameData> *frame);

    std::size_t cancel();
    bool reopen();

    std::size_t capacity() const;
    std::size_t size() const;

private:
    const std::size_t m_capacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_frameAvailable;
    std::condition_variable m_spaceAvailable;
    std::deque<std::shared_ptr<const FrameData> > m_items;
    bool m_cancelled = false;
};
