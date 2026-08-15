#include "runtime/inspection_plc_controller.h"

#include <QByteArray>
#include <utility>

namespace {
const int kMissingDeviceError = -1;
const int kInvalidTriggerModeError = -2;
const int kPlcDbNumber = 1;
}

InspectionPlcController::InspectionPlcController(
    std::unique_ptr<IPlcDevice> device)
    : m_device(std::move(device))
{
}

InspectionPlcController::~InspectionPlcController()
{
    if (m_device && m_device->isConnected()) {
        m_device->disconnect();
    }
}

bool InspectionPlcController::hasDevice() const
{
    return static_cast<bool>(m_device);
}

bool InspectionPlcController::isConnected() const
{
    return m_device && m_device->isConnected();
}

PlcOperationResult InspectionPlcController::connectTo(
    const QString &address,
    int rack,
    int slot)
{
    if (!m_device) {
        return missingDeviceResult();
    }
    const QByteArray encodedAddress = address.toUtf8();
    return m_device->connectTo(
                encodedAddress.constData(),
                rack,
                slot);
}

PlcOperationResult InspectionPlcController::disconnect()
{
    return m_device
            ? m_device->disconnect()
            : missingDeviceResult();
}

PlcOperationResult InspectionPlcController::writeTriggerMode(
    int modeIndex)
{
    if (modeIndex != 0 && modeIndex != 1) {
        return PlcOperationResult(kInvalidTriggerModeError);
    }
    return writeByte(
                1032,
                static_cast<std::uint8_t>(modeIndex));
}

InspectionPlcRunSettingsResult
InspectionPlcController::applyRunSettings(
    const InspectionPlcRunSettings &settings)
{
    InspectionPlcRunSettingsResult result;
    result.operation = writeWord(980, settings.rejectTime);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::RejectTime;
        return result;
    }

    result.operation = writeDWord(
                920,
                settings.rejectDistance);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::RejectDistance;
        return result;
    }

    result.operation = writeWord(982, settings.photoTime);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::PhotoTime;
        return result;
    }

    result.operation = writeDWord(
                924,
                settings.photoDistance);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::PhotoDistance;
        return result;
    }
    return result;
}

PlcOperationResult InspectionPlcController::writePhotoDistance(
    std::uint32_t photoDistance)
{
    return writeDWord(924, photoDistance);
}

PlcOperationResult InspectionPlcController::writeResultValue(
    std::uint8_t value)
{
    return writeByte(1033, value);
}

PlcOperationResult InspectionPlcController::missingDeviceResult() const
{
    return PlcOperationResult(kMissingDeviceError);
}

PlcOperationResult InspectionPlcController::writeByte(
    int start,
    std::uint8_t value)
{
    if (!m_device) {
        return missingDeviceResult();
    }
    unsigned char data[1] = {
        static_cast<unsigned char>(value)
    };
    return m_device->writeDbArea(
                kPlcDbNumber,
                start,
                1,
                PlcDataWidth::Byte,
                data);
}

PlcOperationResult InspectionPlcController::writeWord(
    int start,
    std::uint16_t value)
{
    if (!m_device) {
        return missingDeviceResult();
    }
    unsigned char data[2] = {
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)
    };
    return m_device->writeDbArea(
                kPlcDbNumber,
                start,
                2,
                PlcDataWidth::Word,
                data);
}

PlcOperationResult InspectionPlcController::writeDWord(
    int start,
    std::uint32_t value)
{
    if (!m_device) {
        return missingDeviceResult();
    }
    unsigned char data[4] = {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)
    };
    return m_device->writeDbArea(
                kPlcDbNumber,
                start,
                4,
                PlcDataWidth::DWord,
                data);
}
