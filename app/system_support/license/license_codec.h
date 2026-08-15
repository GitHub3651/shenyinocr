#pragma once

#include <QDate>
#include <QString>

enum class LicenseFileError
{
    None,
    InvalidDate,
    FileReadFailed,
    InvalidFormat,
    FileWriteFailed
};

struct LicenseReadResult
{
    LicenseFileError error = LicenseFileError::FileReadFailed;
    QDate expiresDate;

    bool succeeded() const;
};

class LicenseCodec
{
public:
    static LicenseReadResult readFile(const QString &filePath);
    static LicenseFileError writeFile(const QDate &expiresDate,
                                      const QString &filePath);
};
