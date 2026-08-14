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
    if (!isValidFrame(frame)) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_spaceAvailable.wait(lock, [this]() {
        return m_cancelled || m_frames.size() < m_capacity;
    });
    if (m_cancelled) {
        return false;
    }

    m_frames.push_back(frame);
    lock.unlock();
    m_frameAvailable.notify_one();
    return true;
}

bool FrameQueue::waitAndTake(
    std::shared_ptr<const FrameData> *frame)
{
    if (!frame) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_frameAvailable.wait(lock, [this]() {
        return m_cancelled || !m_frames.empty();
    });
    if (m_cancelled) {
        frame->reset();
        return false;
    }

    *frame = m_frames.front();
    m_frames.pop_front();
    lock.unlock();
    m_spaceAvailable.notify_one();
    return true;
}

std::size_t FrameQueue::cancel()
{
    std::deque<std::shared_ptr<const FrameData>> releasedFrames;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = true;
        releasedFrames.swap(m_frames);
    }
    m_frameAvailable.notify_all();
    m_spaceAvailable.notify_all();
    return releasedFrames.size();
}

bool FrameQueue::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_frames.empty()) {
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
    return m_frames.size();
}

bool FrameQueue::isCancelled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelled;
}

bool FrameQueue::isValidFrame(
    const std::shared_ptr<const FrameData> &frame) const
{
    return frame
            && frame->productKey.isValid()
            && !frame->originalImage.empty();
}
