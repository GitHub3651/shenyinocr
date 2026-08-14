#pragma once

#include "TrackingTypes.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

class FrameQueue
{
public:
    explicit FrameQueue(std::size_t capacity);
    ~FrameQueue();

    bool submit(const DetectionWorkItem &item);
    bool submit(const std::shared_ptr<const FrameData> &frame);
    bool waitAndTake(DetectionWorkItem *item);
    bool waitAndTake(std::shared_ptr<const FrameData> *frame);

    std::size_t cancel();
    bool reopen();

    std::size_t capacity() const;
    std::size_t size() const;
    bool isCancelled() const;

private:
    const std::size_t m_capacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_frameAvailable;
    std::condition_variable m_spaceAvailable;
    std::deque<DetectionWorkItem> m_items;
    bool m_cancelled = false;
};
