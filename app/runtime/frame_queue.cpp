#include "runtime/frame_queue.h"

FrameQueue::FrameQueue(std::size_t capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
{
}

FrameQueue::~FrameQueue()
{
    cancel();
}

bool FrameQueue::submit(
    const std::shared_ptr<const FrameData> &frame)
{
    if (!frame || !frame->productKey.isValid()
            || frame->originalImage.empty()) {
        return false;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_spaceAvailable.wait(lock, [this]() {
        return m_cancelled || m_items.size() < m_capacity;
    });
    if (m_cancelled) {
        return false;
    }
    m_items.push_back(frame);
    lock.unlock();
    m_frameAvailable.notify_one();
    return true;
}

FrameQueueSubmitResult FrameQueue::trySubmit(
    const std::shared_ptr<const FrameData> &frame)
{
    if (!frame || !frame->productKey.isValid()
            || frame->originalImage.empty()) {
        return FrameQueueSubmitResult::InvalidItem;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return FrameQueueSubmitResult::Cancelled;
    }
    if (m_items.size() >= m_capacity) {
        return FrameQueueSubmitResult::Full;
    }
    m_items.push_back(frame);
    m_frameAvailable.notify_one();
    return FrameQueueSubmitResult::Accepted;
}

bool FrameQueue::waitAndTake(
    std::shared_ptr<const FrameData> *frame)
{
    if (!frame) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_frameAvailable.wait(lock, [this]() {
        return m_cancelled || !m_items.empty();
    });
    if (m_cancelled) {
        frame->reset();
        return false;
    }
    *frame = m_items.front();
    m_items.pop_front();
    lock.unlock();
    m_spaceAvailable.notify_one();
    return true;
}

std::size_t FrameQueue::cancel()
{
    std::deque<std::shared_ptr<const FrameData> > releasedItems;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = true;
        releasedItems.swap(m_items);
    }
    m_frameAvailable.notify_all();
    m_spaceAvailable.notify_all();
    return releasedItems.size();
}

bool FrameQueue::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_items.empty()) {
        return false;
    }
    m_cancelled = false;
    return true;
}

std::size_t FrameQueue::capacity() const
{
    return m_capacity;
}

std::size_t FrameQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items.size();
}
