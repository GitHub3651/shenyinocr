#pragma once

#include <QString>

struct ApplicationError
{
    QString code;
    QString userMessage;
    QString diagnostic;

    bool isEmpty() const
    {
        return code.isEmpty();
    }
};

struct OperationResult
{
    bool success = false;
    ApplicationError error;

    bool isSuccess() const
    {
        return success;
    }

    static OperationResult accepted()
    {
        OperationResult result;
        result.success = true;
        return result;
    }

    static OperationResult rejected(
        const QString &code,
        const QString &userMessage,
        const QString &diagnostic = QString())
    {
        OperationResult result;
        result.error.code = code;
        result.error.userMessage = userMessage;
        result.error.diagnostic = diagnostic;
        return result;
    }
};
