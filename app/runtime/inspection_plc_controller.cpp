#include "runtime/inspection_plc_controller.h"

#include <QByteArray>
#include <utility>

namespace {
const int kMissingDeviceError = -1;
const int kInvalidTriggerModeError = -2;
}

InspectionPlcController::InspectionPlcController(
    std::unique_ptr<IPlcDevice> device,
    const InspectionPlcAddressMap &addresses)
    : m_device(std::move(device)),
      m_addresses(addresses)
{
}

InspectionPlcController::~InspectionPlcController()
{
    if (m_device && m_device->isConnected()) {
        m_device->disconnect();
    }
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
                m_addresses.triggerModeDb,
                m_addresses.triggerModeOffset,
                static_cast<std::uint8_t>(modeIndex));
}

InspectionPlcRunSettingsResult
InspectionPlcController::applyRunSettings(
    const InspectionPlcRunSettings &settings)
{
    InspectionPlcRunSettingsResult result;
    result.operation = writeWord(m_addresses.resultDb,
                                 m_addresses.rejectTimeOffset,
                                 settings.rejectTime);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::RejectTime;
        return result;
    }

    result.operation = writeDWord(
                m_addresses.resultDb,
                m_addresses.rejectDistanceOffset,
                settings.rejectDistance);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::RejectDistance;
        return result;
    }

    result.operation = writeWord(m_addresses.resultDb,
                                 m_addresses.photoTimeOffset,
                                 settings.photoTime);
    if (!result.operation.isSuccess()) {
        result.failedField = InspectionPlcRunSettingField::PhotoTime;
        return result;
    }

    result.operation = writeDWord(
                m_addresses.resultDb,
                m_addresses.photoDistanceOffset,
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
    return writeDWord(m_addresses.resultDb,
                      m_addresses.photoDistanceOffset,
                      photoDistance);
}

PlcOperationResult InspectionPlcController::writeResultValue(
    std::uint8_t value)
{
    return writeByte(m_addresses.resultDb,
                     m_addresses.resultOffset,
                     value);
}

PlcOperationResult InspectionPlcController::missingDeviceResult() const
{
    return PlcOperationResult(kMissingDeviceError);
}

PlcOperationResult InspectionPlcController::writeByte(
    int db,
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
                db,
                start,
                1,
                PlcDataWidth::Byte,
                data);
}

PlcOperationResult InspectionPlcController::writeWord(
    int db,
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
                db,
                start,
                2,
                PlcDataWidth::Word,
                data);
}

PlcOperationResult InspectionPlcController::writeDWord(
    int db,
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
                db,
                start,
                4,
                PlcDataWidth::DWord,
                data);
}
