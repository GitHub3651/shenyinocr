#include "result_presentation_mailbox.h"

UiCompletionMailbox::UiCompletionMailbox()
    : m_cancelled(true),
      m_hasPendingWork(false),
      m_processing(false)
{
}

UiCompletionMailbox::~UiCompletionMailbox()
{
    cancel();
}

bool UiCompletionMailbox::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hasPendingWork || m_processing) {
        return false;
    }

    m_cancelled = false;
    return true;
}

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

bool UiCompletionMailbox::hasPendingWork() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hasPendingWork;
}

bool UiCompletionMailbox::isProcessing() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processing;
}
