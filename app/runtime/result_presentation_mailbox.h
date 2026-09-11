#ifndef RESULT_PRESENTATION_MAILBOX_H
#define RESULT_PRESENTATION_MAILBOX_H

#include "contracts/inspection_presentation.h"

#include <mutex>

class UiCompletionMailbox
{
public:
    UiCompletionMailbox();
    ~UiCompletionMailbox() = default;

    bool reopen();
    bool submit(const InspectionPresentation &presentation);
    bool processOne(InspectionPresentation *presentation);
    bool hasPending() const;
    void cancel();

private:
    UiCompletionMailbox(const UiCompletionMailbox &) = delete;
    UiCompletionMailbox &operator=(const UiCompletionMailbox &) = delete;

    mutable std::mutex m_mutex;
    InspectionPresentation m_pendingPresentation;
    bool m_cancelled;
    bool m_hasPendingPresentation;
};

#endif // RESULT_PRESENTATION_MAILBOX_H
