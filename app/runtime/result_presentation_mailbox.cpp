// 文件作用：本文件用于保存容量为一的完整呈现快照，并在生产和界面消费之间提供反压。
// 主要职责：保存容量为一的完整呈现快照，并在生产和界面消费之间提供反压。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "result_presentation_mailbox.h"

// 函数说明：UiCompletionMailbox 构造函数创建组件并初始化其依赖和初始状态。
UiCompletionMailbox::UiCompletionMailbox()
    : m_cancelled(true),
      m_hasPendingWork(false),
      m_processing(false)
{
}

// 函数说明：~UiCompletionMailbox 析构函数按生命周期要求释放组件持有的资源。
UiCompletionMailbox::~UiCompletionMailbox()
{
    cancel();
}

// 函数说明：reopen 函数实现名称所表示的处理步骤。
bool UiCompletionMailbox::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hasPendingWork || m_processing) {
        return false;
    }

    m_cancelled = false;
    return true;
}

// 函数说明：submit 函数执行对应事件或业务处理。
bool UiCompletionMailbox::submit(const Work &work)
{
    if (!work) {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_spaceAvailable.wait(lock, [this]() {
        return m_cancelled
                || (!m_hasPendingWork && !m_processing);
    });
    if (m_cancelled) {
        return false;
    }

    m_work = work;
    m_hasPendingWork = true;
    return true;
}

// 函数说明：processOne 函数执行对应事件或业务处理。
bool UiCompletionMailbox::processOne()
{
    Work work;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cancelled || !m_hasPendingWork || m_processing) {
            return false;
        }

        work = m_work;
        m_work = Work();
        m_hasPendingWork = false;
        m_processing = true;
    }

    bool succeeded = true;
    try {
        work();
    } catch (...) {
        succeeded = false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processing = false;
    }
    m_spaceAvailable.notify_all();
    return succeeded;
}

// 函数说明：cancel 函数检查相关状态并返回判断结果。
void UiCompletionMailbox::cancel()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = true;
        m_work = Work();
        m_hasPendingWork = false;
    }
    m_spaceAvailable.notify_all();
}
