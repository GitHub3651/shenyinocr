#pragma once

#include <QString>

class ApplicationLogger
{
public:
    static bool install(const QString &applicationDirectory);
    static void appendCrashInformation(const QString &information);
    static QString logDirectoryPath();
    static void shutdown();
};
