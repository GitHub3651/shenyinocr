#include "startup/runtime_guard.h"

#include "contracts/detection_mode.h"
#include "system_support/license/license_codec.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <Windows.h>
#include <Wbemidl.h>

namespace {

const QString unactivatedValue = QStringLiteral("UNACTIVATED");

QDate shanghaiDate()
{
    return QDateTime::currentDateTimeUtc().addSecs(8 * 60 * 60).date();
}

QByteArray cryptRuntimeData(const QByteArray &data)
{
    const QByteArray key = QByteArrayLiteral(
                "OCRGangYin.runtime.date.v1.20260925");
    QByteArray result;
    result.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result.append(static_cast<char>(
                          data.at(i) ^ key.at(i % key.size())));
    }
    return result;
}

bool writeRuntimeDate(const QString &filePath, const QDate &date)
{
    const QByteArray content = cryptRuntimeData(
                date.toString(QStringLiteral("yyyy-MM-dd")).toUtf8())
            .toHex().toUpper();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
            || file.write(content) != content.size()) {
        return false;
    }
    return file.commit();
}

RuntimeGuardStatus checkRuntimeDateValue(const QDate &currentDate,
                                        const QDate &issuedDate,
                                        const QDate &expiresDate)
{
    if (currentDate < issuedDate) {
        return RuntimeGuardStatus::TimeError;
    }

    const QString filePath = QDir(
                QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("runtime.dat"));
    QFile file(filePath);
    if (!file.exists()) {
        if (!writeRuntimeDate(filePath, currentDate)) {
            return RuntimeGuardStatus::TimeError;
        }
    } else {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return RuntimeGuardStatus::TimeError;
        }
        const QByteArray content = file.readAll().trimmed();
        file.close();
        const QString text = QString::fromUtf8(
                    cryptRuntimeData(QByteArray::fromHex(content)));
        const QDate recordedDate = QDate::fromString(
                    text, QStringLiteral("yyyy-MM-dd"));
        if (!recordedDate.isValid()
                || recordedDate.toString(
                    QStringLiteral("yyyy-MM-dd")) != text) {
            return RuntimeGuardStatus::TimeError;
        }
        if (currentDate < recordedDate) {
            return RuntimeGuardStatus::TimeError;
        }
        if (currentDate > recordedDate
                && !writeRuntimeDate(filePath, currentDate)) {
            return RuntimeGuardStatus::TimeError;
        }
    }

    return currentDate > expiresDate
            ? RuntimeGuardStatus::Expired
            : RuntimeGuardStatus::Valid;
}

class WmiReader
{
public:
    ~WmiReader()
    {
        if (m_services) {
            m_services->Release();
        }
        if (m_locator) {
            m_locator->Release();
        }
        if (m_uninitializeCom) {
            CoUninitialize();
        }
    }

    bool initialize()
    {
        const HRESULT initializeResult = CoInitializeEx(
                    nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(initializeResult)
                && initializeResult != RPC_E_CHANGED_MODE) {
            return false;
        }
        m_uninitializeCom = SUCCEEDED(initializeResult);

        const HRESULT securityResult = CoInitializeSecurity(
                    nullptr, -1, nullptr, nullptr,
                    RPC_C_AUTHN_LEVEL_DEFAULT,
                    RPC_C_IMP_LEVEL_IMPERSONATE,
                    nullptr, EOAC_NONE, nullptr);
        if (FAILED(securityResult)
                && securityResult != RPC_E_TOO_LATE) {
            return false;
        }

        if (FAILED(CoCreateInstance(
                       CLSID_WbemLocator, nullptr,
                       CLSCTX_INPROC_SERVER,
                       IID_IWbemLocator,
                       reinterpret_cast<void **>(&m_locator)))) {
            return false;
        }

        BSTR nameSpace = SysAllocString(L"ROOT\\CIMV2");
        const HRESULT connectResult = m_locator->ConnectServer(
                    nameSpace, nullptr, nullptr, nullptr, 0,
                    nullptr, nullptr, &m_services);
        SysFreeString(nameSpace);
        if (FAILED(connectResult)) {
            return false;
        }

        return SUCCEEDED(CoSetProxyBlanket(
                             m_services,
                             RPC_C_AUTHN_WINNT,
                             RPC_C_AUTHZ_NONE,
                             nullptr,
                             RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE,
                             nullptr,
                             EOAC_NONE));
    }

    QString value(const wchar_t *className,
                  const wchar_t *propertyName) const
    {
        const QString queryText = QStringLiteral("SELECT %1 FROM %2")
                .arg(QString::fromWCharArray(propertyName),
                     QString::fromWCharArray(className));
        BSTR language = SysAllocString(L"WQL");
        BSTR query = SysAllocStringLen(
                    reinterpret_cast<const OLECHAR *>(queryText.utf16()),
                    static_cast<UINT>(queryText.size()));
        IEnumWbemClassObject *enumerator = nullptr;
        const HRESULT queryResult = m_services->ExecQuery(
                    language, query,
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    nullptr, &enumerator);
        SysFreeString(language);
        SysFreeString(query);
        if (FAILED(queryResult) || !enumerator) {
            return QString();
        }

        IWbemClassObject *object = nullptr;
        ULONG returned = 0;
        const HRESULT nextResult = enumerator->Next(
                    WBEM_INFINITE, 1, &object, &returned);
        enumerator->Release();
        if (FAILED(nextResult) || returned == 0 || !object) {
            return QString();
        }

        VARIANT property;
        VariantInit(&property);
        const HRESULT propertyResult = object->Get(
                    propertyName, 0, &property, nullptr, nullptr);
        object->Release();
        QString result;
        if (SUCCEEDED(propertyResult)
                && property.vt == VT_BSTR
                && property.bstrVal) {
            result = QString::fromWCharArray(property.bstrVal);
        }
        VariantClear(&property);
        return result;
    }

private:
    bool m_uninitializeCom = false;
    IWbemLocator *m_locator = nullptr;
    IWbemServices *m_services = nullptr;
};

QString normalizedIdentifier(const QString &identifier)
{
    QString value = identifier.trimmed().toUpper();
    if (value.size() >= 2
            && value.startsWith(QLatin1Char('{'))
            && value.endsWith(QLatin1Char('}'))) {
        value = value.mid(1, value.size() - 2);
    }
    value.remove(QLatin1Char(' '));
    value.remove(QLatin1Char('-'));
    return value;
}

bool isValidIdentifier(const QString &identifier)
{
    if (identifier.isEmpty()
            || identifier == QString(identifier.size(), QLatin1Char('0'))
            || identifier == QString(identifier.size(), QLatin1Char('F'))) {
        return false;
    }
    return identifier != QLatin1String("TOBEFILLEDBYOEM")
            && identifier != QLatin1String("DEFAULTSTRING")
            && identifier != QLatin1String("UNKNOWN")
            && identifier != QLatin1String("NONE")
            && identifier != QLatin1String("NOTSPECIFIED");
}

QString deviceCode(const QString &source)
{
    return QString::fromLatin1(
                QCryptographicHash::hash(
                    source.toUtf8(), QCryptographicHash::Sha256)
                .toHex().toUpper());
}

bool systemUuidDeviceCode(const WmiReader &reader, QString &code)
{
    const QString systemUuid = normalizedIdentifier(reader.value(
                L"Win32_ComputerSystemProduct", L"UUID"));
    if (!isValidIdentifier(systemUuid)) {
        return false;
    }
    code = deviceCode(systemUuid);
    return true;
}

bool baseboardBiosDeviceCode(const WmiReader &reader, QString &code)
{
    const QString baseboardSerial = normalizedIdentifier(reader.value(
                L"Win32_BaseBoard", L"SerialNumber"));
    const QString biosSerial = normalizedIdentifier(reader.value(
                L"Win32_BIOS", L"SerialNumber"));
    if (!isValidIdentifier(baseboardSerial)
            || !isValidIdentifier(biosSerial)) {
        return false;
    }
    code = deviceCode(baseboardSerial + QLatin1Char('|') + biosSerial);
    return true;
}

bool currentDeviceCode(const WmiReader &reader,
                       const QString &binding,
                       QString &code)
{
    if (binding == QLatin1String("S1")) {
        return systemUuidDeviceCode(reader, code);
    }
    if (binding == QLatin1String("S2")) {
        return baseboardBiosDeviceCode(reader, code);
    }
    return false;
}

bool selectDeviceBinding(const WmiReader &reader,
                         QString &binding,
                         QString &code)
{
    if (systemUuidDeviceCode(reader, code)) {
        binding = QStringLiteral("S1");
        return true;
    }
    if (baseboardBiosDeviceCode(reader, code)) {
        binding = QStringLiteral("S2");
        return true;
    }
    return false;
}

bool matchesCurrentDevice(const WmiReader &reader,
                          const LicenseData &license)
{
    QString code;
    return currentDeviceCode(reader, license.deviceBinding, code)
            && code == license.deviceCode;
}

} // namespace

RuntimeGuardResult RuntimeGuard::check()
{
    RuntimeGuardResult result;
    const QString licensePath = QDir(
                QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("license.ini"));
    LicenseDecodeResult decoded = LicenseCodec::readFile(licensePath);
    if (decoded.status != LicenseCodecStatus::Success) {
        return result;
    }
    QStringList authorizedModeIds;
    for (const DetectionModeDescriptor &descriptor
         : detectionModeDescriptors()) {
        const QString modeId = QLatin1String(descriptor.modeId);
        if (decoded.license.featureModeIds.contains(modeId)) {
            authorizedModeIds.append(modeId);
        }
    }

    if (!decoded.license.permanent) {
        const QDate currentDate = shanghaiDate();
        const RuntimeGuardStatus dateStatus = checkRuntimeDateValue(
                    currentDate,
                    decoded.license.issuedDate,
                    decoded.license.expiresDate);
        if (dateStatus != RuntimeGuardStatus::Valid) {
            result.status = dateStatus;
            return result;
        }
        result.remainingDays = currentDate.daysTo(
                    decoded.license.expiresDate);
    }

    result.status = RuntimeGuardStatus::DeviceBindingError;
    WmiReader reader;
    if (!reader.initialize()) {
        return result;
    }

    if (decoded.license.deviceBinding == unactivatedValue) {
        if (!selectDeviceBinding(
                    reader,
                    decoded.license.deviceBinding,
                    decoded.license.deviceCode)
                || LicenseCodec::writeFile(decoded.license, licensePath)
                   != LicenseCodecStatus::Success) {
            return result;
        }
        decoded = LicenseCodec::readFile(licensePath);
        if (decoded.status != LicenseCodecStatus::Success) {
            return result;
        }
    }

    if (!matchesCurrentDevice(reader, decoded.license)) {
        return result;
    }

    result.status = RuntimeGuardStatus::Valid;
    result.permanent = decoded.license.permanent;
    result.issuedDate = decoded.license.issuedDate;
    result.expiresDate = decoded.license.expiresDate;
    result.authorizedModeIds = authorizedModeIds;
    result.defaultModeId = decoded.license.defaultModeId;
    return result;
}

RuntimeGuardStatus RuntimeGuard::checkRuntimeDate(
        const QDate &issuedDate,
        const QDate &expiresDate)
{
    return checkRuntimeDateValue(shanghaiDate(), issuedDate, expiresDate);
}
