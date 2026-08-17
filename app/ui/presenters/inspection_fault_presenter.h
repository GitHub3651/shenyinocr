#pragma once

#include "runtime/inspection_runtime.h"

#include <QString>

struct InspectionFaultPresentation
{
    QString statusText;
    QString resultText;
    QString operatorMessage;
    QString statusStyleSheet;
    QString resultStyleSheet;

    bool isValid() const;
};

class InspectionFaultPresenter
{
public:
    static InspectionFaultPresentation create(
        const InspectionFaultSnapshot &snapshot);
};
