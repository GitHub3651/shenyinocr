// 文件作用：本文件用于提供容量受控的正式帧队列，并支持阻塞提交、取出和协作取消。
// 主要职责：提供容量受控的正式帧队列，并支持阻塞提交、取出和协作取消。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/frame_queue.h"

// 函数说明：FrameQueue 构造函数创建组件并初始化其依赖和初始状态。
FrameQueue::FrameQueue(std::size_t capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
{
}

// 函数说明：~FrameQueue 析构函数按生命周期要求释放组件持有的资源。
FrameQueue::~FrameQueue()
{
    cancel();
}

// 函数说明：submit 函数执行对应事件或业务处理。
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

// 函数说明：trySubmit 函数实现名称所表示的处理步骤。
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

// 函数说明：waitAndTake 函数读取、等待或计算对应的数据。
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

// 函数说明：cancel 函数检查相关状态并返回判断结果。
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

// 函数说明：reopen 函数实现名称所表示的处理步骤。
bool FrameQueue::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_items.empty()) {
        return false;
    }
    m_cancelled = false;
    return true;
}

// 函数说明：capacity 函数实现名称所表示的处理步骤。
std::size_t FrameQueue::capacity() const
{
    return m_capacity;
}

// 函数说明：size 函数读取、等待或计算对应的数据。
std::size_t FrameQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_items.size();
}
