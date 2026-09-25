#include "startup/runtime_guard.h"

#include "contracts/detection_mode.h"
#include "system_support/license/license_codec.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>

#include <Windows.h>
#include <Wbemidl.h>

namespace {

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
        if (!m_services) {
            return QString();
        }

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

bool systemUuidDeviceCode(const WmiReader &reader, QString *code)
{
    const QString systemUuid = normalizedIdentifier(reader.value(
                L"Win32_ComputerSystemProduct", L"UUID"));
    if (!isValidIdentifier(systemUuid)) {
        return false;
    }
    *code = deviceCode(systemUuid);
    return true;
}

bool baseboardBiosDeviceCode(const WmiReader &reader, QString *code)
{
    const QString baseboardSerial = normalizedIdentifier(reader.value(
                L"Win32_BaseBoard", L"SerialNumber"));
    const QString biosSerial = normalizedIdentifier(reader.value(
                L"Win32_BIOS", L"SerialNumber"));
    if (!isValidIdentifier(baseboardSerial)
            || !isValidIdentifier(biosSerial)) {
        return false;
    }
    *code = deviceCode(baseboardSerial + QLatin1Char('|') + biosSerial);
    return true;
}

bool currentDeviceCode(const WmiReader &reader,
                       const QString &binding,
                       QString *code)
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
                         QString *binding,
                         QString *code)
{
    if (systemUuidDeviceCode(reader, code)) {
        *binding = QStringLiteral("S1");
        return true;
    }
    if (baseboardBiosDeviceCode(reader, code)) {
        *binding = QStringLiteral("S2");
        return true;
    }
    return false;
}

QString licensePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("license.ini"));
}

bool authorizedModeIds(const LicenseData &license,
                       QStringList *orderedModeIds)
{
    if (license.featureModeIds.isEmpty()) {
        return false;
    }
    for (const QString &modeId : license.featureModeIds) {
        DetectionMode mode;
        if (!detectionModeFromId(modeId, &mode)) {
            return false;
        }
    }
    DetectionMode defaultMode;
    if (!detectionModeFromId(license.defaultModeId, &defaultMode)
            || !license.featureModeIds.contains(license.defaultModeId)) {
        return false;
    }

    orderedModeIds->clear();
    for (const DetectionModeDescriptor &descriptor
         : detectionModeDescriptors()) {
        const QString modeId = QLatin1String(descriptor.modeId);
        if (license.featureModeIds.contains(modeId)) {
            orderedModeIds->append(modeId);
        }
    }
    return true;
}

RuntimeGuardResult activationResult(RuntimeGuardStatus status,
                                    const WmiReader &reader)
{
    RuntimeGuardResult result;
    QString binding;
    QString code;
    if (!selectDeviceBinding(reader, &binding, &code)) {
        result.status = RuntimeGuardStatus::DeviceUnavailable;
        return result;
    }
    result.status = status;
    result.activationRequestCode =
            LicenseCodec::createActivationRequestCode(binding, code);
    return result;
}

RuntimeGuardResult validResult(const LicenseData &license,
                               const QStringList &modeIds)
{
    RuntimeGuardResult result;
    result.status = RuntimeGuardStatus::Valid;
    result.permanent = license.permanent;
    result.expiresDate = license.expiresDate;
    result.showExpiry = license.showExpiry;
    result.authorizedModeIds = modeIds;
    result.defaultModeId = license.defaultModeId;
    return result;
}

} // namespace

RuntimeGuardResult RuntimeGuard::check()
{
    const LicenseDecodeResult decoded = LicenseCodec::readFile(licensePath());
    if (decoded.status == LicenseCodecStatus::FileReadFailed) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::LicenseReadFailed;
        return result;
    }

    WmiReader reader;
    if (!reader.initialize()) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::DeviceUnavailable;
        return result;
    }

    if (decoded.status == LicenseCodecStatus::FileMissing) {
        return activationResult(RuntimeGuardStatus::LicenseMissing, reader);
    }
    if (decoded.status != LicenseCodecStatus::Success) {
        return activationResult(RuntimeGuardStatus::LicenseInvalid, reader);
    }

    QStringList modeIds;
    if (!authorizedModeIds(decoded.license, &modeIds)) {
        return activationResult(RuntimeGuardStatus::LicenseInvalid, reader);
    }
    if (decoded.license.deviceBinding != QLatin1String("S1")
            && decoded.license.deviceBinding != QLatin1String("S2")) {
        return activationResult(RuntimeGuardStatus::LicenseInvalid, reader);
    }

    QString code;
    if (!currentDeviceCode(reader, decoded.license.deviceBinding, &code)
            || code != decoded.license.deviceCode) {
        return activationResult(RuntimeGuardStatus::DeviceMismatch, reader);
    }
    if (!decoded.license.permanent
            && QDate::currentDate() > decoded.license.expiresDate) {
        return activationResult(RuntimeGuardStatus::LicenseExpired, reader);
    }
    return validResult(decoded.license, modeIds);
}

RuntimeGuardResult RuntimeGuard::activate(const QString &activationCode)
{
    ActivationCodeData activation;
    if (!LicenseCodec::parseActivationCode(activationCode, &activation)) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::LicenseInvalid;
        return result;
    }

    WmiReader reader;
    if (!reader.initialize()) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::DeviceUnavailable;
        return result;
    }

    QString code;
    if (!currentDeviceCode(reader, activation.deviceBinding, &code)
            || LicenseCodec::deviceDigest(activation.deviceBinding, code)
               != activation.deviceDigest) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::DeviceMismatch;
        return result;
    }
    if (!activation.permanent
            && QDate::currentDate() > activation.expiresDate) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::LicenseExpired;
        return result;
    }

    LicenseData license;
    license.permanent = activation.permanent;
    license.expiresDate = activation.expiresDate;
    license.showExpiry = activation.showExpiry;
    license.featureModeIds = activation.featureModeIds;
    license.defaultModeId = activation.defaultModeId;
    license.deviceBinding = activation.deviceBinding;
    license.deviceCode = code;
    if (!LicenseCodec::writeFile(license, licensePath())) {
        RuntimeGuardResult result;
        result.status = RuntimeGuardStatus::LicenseSaveFailed;
        return result;
    }
    return validResult(license, activation.featureModeIds);
}
