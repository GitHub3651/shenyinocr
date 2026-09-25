#include "activation_protocol.h"

#include <QCryptographicHash>
#include <QFile>
#include <QTextStream>

namespace {

const QDate activationDateBase(2020, 1, 1);
const QString requestPrefix = QStringLiteral("41");
const QString activationPrefix = QStringLiteral("42");

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.license.expire.v1.20260706");
}

QByteArray cryptData(const QByteArray &data)
{
    const QByteArray key = QCryptographicHash::hash(
                secretKey(), QCryptographicHash::Sha256);
    QByteArray result;
    result.reserve(data.size());
    for (int index = 0; index < data.size(); ++index) {
        result.append(static_cast<char>(
                          data.at(index) ^ key.at(index % key.size())));
    }
    return result;
}

QString decryptText(const QString &text)
{
    return QString::fromUtf8(cryptData(
                QByteArray::fromHex(text.toLatin1())));
}

bool isAsciiDigits(const QString &value)
{
    if (value.isEmpty()) {
        return false;
    }
    for (const QChar character : value) {
        if (character < QLatin1Char('0')
                || character > QLatin1Char('9')) {
            return false;
        }
    }
    return true;
}

bool isUpperHex(const QString &value)
{
    if (value.isEmpty() || value.size() % 2 != 0) {
        return false;
    }
    for (const QChar character : value) {
        const bool digit = character >= QLatin1Char('0')
                && character <= QLatin1Char('9');
        const bool upperHex = character >= QLatin1Char('A')
                && character <= QLatin1Char('F');
        if (!digit && !upperHex) {
            return false;
        }
    }
    return true;
}

QByteArray numericKey(const QByteArray &label)
{
    const QByteArray baseKey = QCryptographicHash::hash(
                secretKey(), QCryptographicHash::Sha256);
    return QCryptographicHash::hash(
                baseKey + label, QCryptographicHash::Sha256);
}

QString transformDigits(const QString &digits,
                        const QByteArray &label,
                        bool encrypt)
{
    const QByteArray key = numericKey(label);
    QString transformed;
    transformed.reserve(digits.size());
    for (int index = 0; index < digits.size(); ++index) {
        const int digit = digits.at(index).unicode() - QLatin1Char('0').unicode();
        const int keyDigit = static_cast<unsigned char>(
                    key.at(index)) % 10;
        const int value = encrypt
                ? (digit + keyDigit) % 10
                : (digit - keyDigit + 10) % 10;
        transformed.append(QChar(QLatin1Char('0').unicode() + value));
    }
    return transformed;
}

QChar luhnCheckDigit(const QString &payload)
{
    int sum = 0;
    bool doubled = true;
    for (int index = payload.size() - 1; index >= 0; --index) {
        int digit = payload.at(index).unicode()
                - QLatin1Char('0').unicode();
        if (doubled) {
            digit *= 2;
            if (digit > 9) {
                digit -= 9;
            }
        }
        sum += digit;
        doubled = !doubled;
    }
    return QChar(QLatin1Char('0').unicode() + (10 - sum % 10) % 10);
}

bool hasValidLuhnCheck(const QString &code)
{
    if (!isAsciiDigits(code)) {
        return false;
    }
    int sum = 0;
    bool doubled = false;
    for (int index = code.size() - 1; index >= 0; --index) {
        int digit = code.at(index).unicode()
                - QLatin1Char('0').unicode();
        if (doubled) {
            digit *= 2;
            if (digit > 9) {
                digit -= 9;
            }
        }
        sum += digit;
        doubled = !doubled;
    }
    return sum % 10 == 0;
}

bool readEncryptedField(const QString &line,
                        const QString &expectedName,
                        QString *value)
{
    const int separator = line.indexOf(QLatin1Char('='));
    if (separator <= 0
            || separator != line.lastIndexOf(QLatin1Char('='))) {
        return false;
    }
    const QString encryptedName = line.left(separator);
    const QString encryptedValue = line.mid(separator + 1);
    if (!isUpperHex(encryptedName)
            || !isUpperHex(encryptedValue)
            || decryptText(encryptedName) != expectedName) {
        return false;
    }
    *value = decryptText(encryptedValue);
    return true;
}

} // namespace

const QVector<LicenseToolModeDescriptor> &licenseToolModeDescriptors()
{
    static const QVector<LicenseToolModeDescriptor> descriptors = {
        { "stamp", "钢印检测" },
        { "word", "字库匹配" },
        { "ocr", "深度 OCR" },
        { "tissue", "纸巾检测" },
        { "barcodeWord", "二维码+三期" }
    };
    return descriptors;
}

bool parseActivationRequestCode(
        const QString &code,
        ActivationRequestData *request)
{
    QString compact = code;
    compact.remove(QLatin1Char(' '));
    if (compact.size() != 16
            || !compact.startsWith(requestPrefix)
            || !hasValidLuhnCheck(compact)) {
        return false;
    }
    const QString plainBody = transformDigits(
                compact.mid(2, 13),
                QByteArrayLiteral("REQ4-NUMERIC"), false);
    QString binding;
    if (plainBody.at(0) == QLatin1Char('1')) {
        binding = QStringLiteral("S1");
    } else if (plainBody.at(0) == QLatin1Char('2')) {
        binding = QStringLiteral("S2");
    }
    if (binding.isEmpty()) {
        return false;
    }
    request->deviceBinding = binding;
    request->deviceDigest = plainBody.mid(1, 12);
    return true;
}

QString createActivationCode(const ActivationCodeData &activation)
{
    int featureMask = 0;
    int defaultModeNumber = 0;
    const QVector<LicenseToolModeDescriptor> &descriptors =
            licenseToolModeDescriptors();
    for (int index = 0; index < descriptors.size(); ++index) {
        const QString modeId = QLatin1String(descriptors.at(index).modeId);
        if (activation.featureModeIds.contains(modeId)) {
            featureMask |= 1 << index;
        }
        if (activation.defaultModeId == modeId) {
            defaultModeNumber = index + 1;
        }
    }
    const int authorizationFlags = featureMask
            | (activation.showExpiry ? 32 : 0);

    const QString expires = activation.permanent
            ? QStringLiteral("00000")
            : QStringLiteral("%1").arg(
                activationDateBase.daysTo(activation.expiresDate) + 1,
                5, 10, QLatin1Char('0'));
    const QString plainBody =
            (activation.deviceBinding == QLatin1String("S1")
             ? QStringLiteral("1") : QStringLiteral("2"))
            + activation.deviceDigest
            + expires
            + QStringLiteral("%1").arg(
                authorizationFlags,
                2, 10, QLatin1Char('0'))
            + QString::number(defaultModeNumber);
    const QString payload = activationPrefix
            + transformDigits(
                plainBody, QByteArrayLiteral("ACT4-NUMERIC"), true);
    return payload + luhnCheckDigit(payload);
}

bool parseActivationCode(
        const QString &code,
        ActivationCodeData *activation)
{
    QString compact = code;
    compact.remove(QLatin1Char(' '));
    if (compact.size() != 24
            || !compact.startsWith(activationPrefix)
            || !hasValidLuhnCheck(compact)) {
        return false;
    }

    const QString plainBody = transformDigits(
                compact.mid(2, 21),
                QByteArrayLiteral("ACT4-NUMERIC"), false);
    QString binding;
    if (plainBody.at(0) == QLatin1Char('1')) {
        binding = QStringLiteral("S1");
    } else if (plainBody.at(0) == QLatin1Char('2')) {
        binding = QStringLiteral("S2");
    }
    const int expiresValue = plainBody.mid(13, 5).toInt();
    const int authorizationFlags = plainBody.mid(18, 2).toInt();
    const int featureMask = authorizationFlags & 31;
    const int defaultModeNumber = plainBody.at(20).unicode()
            - QLatin1Char('0').unicode();
    const QVector<LicenseToolModeDescriptor> &descriptors =
            licenseToolModeDescriptors();
    if (binding.isEmpty()
            || authorizationFlags < 1
            || authorizationFlags > 63
            || featureMask == 0
            || defaultModeNumber < 1
            || defaultModeNumber > descriptors.size()
            || (featureMask & (1 << (defaultModeNumber - 1))) == 0) {
        return false;
    }

    activation->deviceBinding = binding;
    activation->deviceDigest = plainBody.mid(1, 12);
    activation->permanent = expiresValue == 0;
    activation->expiresDate = activation->permanent
            ? QDate()
            : activationDateBase.addDays(expiresValue - 1);
    activation->showExpiry = (authorizationFlags & 32) != 0;
    activation->featureModeIds.clear();
    for (int index = 0; index < descriptors.size(); ++index) {
        if ((featureMask & (1 << index)) != 0) {
            activation->featureModeIds.append(
                        QLatin1String(descriptors.at(index).modeId));
        }
    }
    activation->defaultModeId = QLatin1String(
                descriptors.at(defaultModeNumber - 1).modeId);
    return true;
}

bool readLicenseFile(const QString &filePath,
                     LicenseFileData *license)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QStringList lines;
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }
    if (lines.size() != 7
            || lines.at(0) != QStringLiteral("version=5")) {
        return false;
    }

    QString expires;
    QString features;
    QString showExpiry;
    if (!readEncryptedField(lines.at(1), QStringLiteral("expires"), &expires)
            || !readEncryptedField(lines.at(2), QStringLiteral("features"),
                                   &features)
            || !readEncryptedField(lines.at(3), QStringLiteral("defaultMode"),
                                   &license->defaultModeId)
            || !readEncryptedField(lines.at(4), QStringLiteral("deviceBinding"),
                                   &license->deviceBinding)
            || !readEncryptedField(lines.at(5), QStringLiteral("deviceCode"),
                                   &license->deviceCode)
            || !readEncryptedField(lines.at(6), QStringLiteral("showExpiry"),
                                   &showExpiry)) {
        return false;
    }

    if (showExpiry == QLatin1String("true")) {
        license->showExpiry = true;
    } else if (showExpiry == QLatin1String("false")) {
        license->showExpiry = false;
    } else {
        return false;
    }

    if (expires == QLatin1String("permanent")) {
        license->permanent = true;
        license->expiresDate = QDate();
    } else {
        license->permanent = false;
        license->expiresDate = QDate::fromString(
                    expires, QStringLiteral("yyyy-MM-dd"));
        if (!license->expiresDate.isValid()
                || license->expiresDate.toString(
                    QStringLiteral("yyyy-MM-dd")) != expires) {
            return false;
        }
    }

    license->featureModeIds = features.split(
                QLatin1Char(','), QString::KeepEmptyParts);
    if (license->deviceBinding != QLatin1String("S1")
            && license->deviceBinding != QLatin1String("S2")) {
        return false;
    }
    for (const QString &modeId : license->featureModeIds) {
        bool knownMode = false;
        for (const LicenseToolModeDescriptor &descriptor
             : licenseToolModeDescriptors()) {
            if (modeId == QLatin1String(descriptor.modeId)) {
                knownMode = true;
                break;
            }
        }
        if (!knownMode) {
            return false;
        }
    }
    return !license->featureModeIds.isEmpty()
            && license->featureModeIds.contains(license->defaultModeId);
}
