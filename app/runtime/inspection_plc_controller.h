#pragma once

#include "devices/plc/plc_device.h"

#include <QString>
#include <cstdint>
#include <memory>

enum class InspectionPlcRunSettingField
{
    None,
    RejectTime,
    RejectDistance,
    PhotoTime,
    PhotoDistance
};

struct InspectionPlcRunSettings
{
    std::uint16_t rejectTime = 0;
    std::uint32_t rejectDistance = 0;
    std::uint16_t photoTime = 0;
    std::uint32_t photoDistance = 0;
};

struct InspectionPlcRunSettingsResult
{
    InspectionPlcRunSettingField failedField =
            InspectionPlcRunSettingField::None;
    PlcOperationResult operation;

    bool isSuccess() const
    {
        return failedField == InspectionPlcRunSettingField::None
                && operation.isSuccess();
    }
};

class InspectionPlcController
{
public:
    explicit InspectionPlcController(
        std::unique_ptr<IPlcDevice> device);
    ~InspectionPlcController();

    bool hasDevice() const;
    bool isConnected() const;
    PlcOperationResult connectTo(
        const QString &address,
        int rack,
        int slot);
    PlcOperationResult disconnect();

    PlcOperationResult writeTriggerMode(int modeIndex);
    InspectionPlcRunSettingsResult applyRunSettings(
        const InspectionPlcRunSettings &settings);
    PlcOperationResult writePhotoDistance(
        std::uint32_t photoDistance);
    PlcOperationResult writeResultValue(std::uint8_t value);

private:
    PlcOperationResult missingDeviceResult() const;
    PlcOperationResult writeByte(int start, std::uint8_t value);
    PlcOperationResult writeWord(int start, std::uint16_t value);
    PlcOperationResult writeDWord(int start, std::uint32_t value);

    std::unique_ptr<IPlcDevice> m_device;
};
