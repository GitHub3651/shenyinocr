#pragma once

#include "runtime/inspection_runtime_controller.h"

class InspectionRuntimeStopTransaction
{
public:
    explicit InspectionRuntimeStopTransaction(
        InspectionRuntimeController &controller);

    void begin();
    void waitForDetectionWorker();
    void commit();

    bool hasBegun() const;
    bool hasWaitedForDetectionWorker() const;
    bool isCommitted() const;

private:
    InspectionRuntimeController &m_controller;
    bool m_hasBegun = false;
    bool m_hasWaitedForDetectionWorker = false;
    bool m_committed = false;
};
