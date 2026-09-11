#include "startup/runtime_guard.h"

#include "system_support/license/license_codec.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>

bool RuntimeGuard::check()
{
    const QString licensePath = QDir(
                QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("license.ini"));
    const LicenseReadResult license = LicenseCodec::readFile(licensePath);
    return license.succeeded()
            && QDate::currentDate() <= license.expiresDate;
}
