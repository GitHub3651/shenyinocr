// 文件作用：本文件用于保存容量为一的完整呈现快照，并在生产和界面消费之间提供反压。
// 主要职责：保存容量为一的完整呈现快照，并在生产和界面消费之间提供反压。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef RESULT_PRESENTATION_MAILBOX_H
#define RESULT_PRESENTATION_MAILBOX_H

#include "contracts/inspection_presentation.h"

#include <mutex>

// 组件说明：UiCompletionMailbox 以容量一邮箱把完整产品结果安全交给UI线程。
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
