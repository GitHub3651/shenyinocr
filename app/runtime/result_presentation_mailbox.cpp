#include "result_presentation_mailbox.h"

UiCompletionMailbox::UiCompletionMailbox()
    : m_cancelled(true),
      m_hasPendingPresentation(false)
{
}

bool UiCompletionMailbox::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hasPendingPresentation) {
        return false;
    }

    m_cancelled = false;
    return true;
}

bool UiCompletionMailbox::submit(
    const InspectionPresentation &presentation)
{
    if (!presentation.isValid()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return false;
    }

    m_pendingPresentation = presentation;
    m_hasPendingPresentation = true;
    return true;
}

bool UiCompletionMailbox::processOne(
    InspectionPresentation *presentation)
{
    if (!presentation) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled || !m_hasPendingPresentation) {
        return false;
    }
    *presentation = m_pendingPresentation;
    m_pendingPresentation = InspectionPresentation();
    m_hasPendingPresentation = false;
    return true;
}

bool UiCompletionMailbox::hasPending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_cancelled && m_hasPendingPresentation;
}

void UiCompletionMailbox::cancel()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = true;
        m_pendingPresentation = InspectionPresentation();
        m_hasPendingPresentation = false;
    }
}
