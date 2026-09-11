#pragma once

#include "detection/common/detection_pose.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

enum class FrameQueueSubmitResult
{
    Accepted,
    InvalidItem,
    Cancelled,
    Full
};

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
