#pragma once

class Widget;

/**
 * Owns the operator stop command, including camera recovery and fault
 * reconciliation. The main window only forwards the button event.
 */
class InspectionStopController
{
public:
    explicit InspectionStopController(Widget *host);

    void stopInspection();

private:
    Widget *m_host = nullptr;
};
