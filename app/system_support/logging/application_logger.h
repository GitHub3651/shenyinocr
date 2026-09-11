#pragma once

#include <QString>

class ApplicationLogger
{
public:
    static bool start(QString *errorMessage = nullptr);
    static void stop();
};
