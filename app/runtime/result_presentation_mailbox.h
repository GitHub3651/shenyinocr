#ifndef RESULT_PRESENTATION_MAILBOX_H
#define RESULT_PRESENTATION_MAILBOX_H

#include <condition_variable>
#include <functional>
#include <mutex>

// Capacity-one handoff between a detection worker and the UI thread.
// A submitted work item owns the complete presentation of one product
// (image, text, statistics and timing). The next product cannot be handed
// off until the current UI work item has finished, so Qt's event queue never
// becomes an unbounded second result queue.
class UiCompletionMailbox
{
public:
    typedef std::function<void()> Work;

    UiCompletionMailbox();
    ~UiCompletionMailbox();

    bool reopen();
    bool submit(const Work &work);
    bool processOne();
    void cancel();

    bool hasPendingWork() const;
    bool isProcessing() const;

private:
    UiCompletionMailbox(const UiCompletionMailbox &) = delete;
    UiCompletionMailbox &operator=(const UiCompletionMailbox &) = delete;

    mutable std::mutex m_mutex;
    std::condition_variable m_spaceAvailable;
    Work m_work;
    bool m_cancelled;
    bool m_hasPendingWork;
    bool m_processing;
};

#endif // RESULT_PRESENTATION_MAILBOX_H
