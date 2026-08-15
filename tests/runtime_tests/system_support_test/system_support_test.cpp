#include <QtTest>

#include "system_support/license/license_codec.h"
#include "startup/single_instance_guard.h"

#include <QFile>
#include <QTemporaryDir>
#include <QUuid>

class SystemSupportTest : public QObject
{
    Q_OBJECT

private slots:
    void writeAndReadLicenseRoundTrip();
    void expiredLicenseRemainsReadable();
    void legacyLicensePayloadRemainsCompatible();
    void missingLicenseIsReportedAsReadFailure();
    void malformedLicenseIsRejected();
    void invalidExpirationDateIsRejectedBeforeWrite();
    void secondInstanceWithSameKeyIsRejected();
};

void SystemSupportTest::writeAndReadLicenseRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("license.ini"));
    const QDate expiresDate(2032, 8, 15);

    QVERIFY(LicenseCodec::writeFile(expiresDate, path)
            == LicenseFileError::None);
    const LicenseReadResult result = LicenseCodec::readFile(path);
    QVERIFY(result.succeeded());
    QCOMPARE(result.expiresDate, expiresDate);
}

void SystemSupportTest::expiredLicenseRemainsReadable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("license.ini"));
    const QDate expiresDate(2020, 1, 2);

    QVERIFY(LicenseCodec::writeFile(expiresDate, path)
            == LicenseFileError::None);
    const LicenseReadResult result = LicenseCodec::readFile(path);
    QVERIFY(result.succeeded());
    QCOMPARE(result.expiresDate, expiresDate);
}

void SystemSupportTest::legacyLicensePayloadRemainsCompatible()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("license.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[License]\n"
               "data=6lpkgd0pJK50ikRNG1CJItjj9g==\n");
    file.close();

    const LicenseReadResult result = LicenseCodec::readFile(path);
    QVERIFY(result.succeeded());
    QCOMPARE(result.expiresDate, QDate(2032, 8, 15));
}

void SystemSupportTest::missingLicenseIsReportedAsReadFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const LicenseReadResult result = LicenseCodec::readFile(
                directory.filePath(QStringLiteral("missing.ini")));
    QVERIFY(!result.succeeded());
    QVERIFY(result.error == LicenseFileError::FileReadFailed);
}

void SystemSupportTest::malformedLicenseIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("license.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[License]\ndata=not-a-valid-license\n");
    file.close();

    const LicenseReadResult result = LicenseCodec::readFile(path);
    QVERIFY(!result.succeeded());
    QVERIFY(result.error == LicenseFileError::InvalidFormat);
}

void SystemSupportTest::invalidExpirationDateIsRejectedBeforeWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("license.ini"));
    QVERIFY(LicenseCodec::writeFile(QDate(), path)
            == LicenseFileError::InvalidDate);
    QVERIFY(!QFile::exists(path));
}

void SystemSupportTest::secondInstanceWithSameKeyIsRejected()
{
    const QString key = QStringLiteral("system-support-test-%1").arg(
                QUuid::createUuid().toString());
    SingleInstanceGuard first(key);
    SingleInstanceGuard second(key);

    QVERIFY(first.acquire());
    QVERIFY(!second.acquire());
}

QTEST_GUILESS_MAIN(SystemSupportTest)
#include "system_support_test.moc"
