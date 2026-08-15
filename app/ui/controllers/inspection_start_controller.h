#pragma once

class Widget;

/**
 * Owns the complete operator command that starts an inspection run.
 *
 * The controller deliberately keeps the existing start sequence intact while
 * removing runtime orchestration from the main window event handler.
 */
class InspectionStartController
{
public:
    explicit InspectionStartController(Widget *host);

    void startInspection();

private:
    Widget *m_host = nullptr;
};
