#pragma once

#include "application/inspection_ui_contract.h"

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
        const ApplicationFaultSnapshot &snapshot);
};
