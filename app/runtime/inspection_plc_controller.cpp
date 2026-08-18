// 文件作用：本文件用于把检测结果转换为PLC输出请求，并维护延迟剔除和复位时序。
// 主要职责：把检测结果转换为PLC输出请求，并维护延迟剔除和复位时序。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/inspection_plc_controller.h"

#include <QByteArray>
#include <utility>

namespace {
const int kMissingDeviceError = -1;
const int kInvalidTriggerModeError = -2;
}

// 函数说明：InspectionPlcController 构造函数创建组件并初始化其依赖和初始状态。
InspectionPlcController::InspectionPlcController(
    std::unique_ptr<IPlcDevice> device,
    const InspectionPlcAddressMap &addresses)
    : m_device(std::move(device)),
      m_addresses(addresses)
{
}

// 函数说明：~InspectionPlcController 析构函数按生命周期要求释放组件持有的资源。
InspectionPlcController::~InspectionPlcController()
{
    if (m_device && m_device->isConnected()) {
        m_device->disconnect();
    }
}

// 函数说明：isConnected 函数检查相关状态并返回判断结果。
bool InspectionPlcController::isConnected() const
{
    return m_device && m_device->isConnected();
}

// 函数说明：connectTo 函数建立或断开对应外部连接。
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

// 函数说明：writeTriggerMode 函数保存或发布对应的数据和资源。
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
// 函数说明：applyRunSettings 函数更新或应用对应的配置和状态。
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

// 函数说明：writePhotoDistance 函数保存或发布对应的数据和资源。
PlcOperationResult InspectionPlcController::writePhotoDistance(
    std::uint32_t photoDistance)
{
    return writeDWord(m_addresses.resultDb,
                      m_addresses.photoDistanceOffset,
                      photoDistance);
}

// 函数说明：writeResultValue 函数保存或发布对应的数据和资源。
PlcOperationResult InspectionPlcController::writeResultValue(
    std::uint8_t value)
{
    return writeByte(m_addresses.resultDb,
                     m_addresses.resultOffset,
                     value);
}

// 函数说明：missingDeviceResult 函数实现名称所表示的处理步骤。
PlcOperationResult InspectionPlcController::missingDeviceResult() const
{
    return PlcOperationResult(kMissingDeviceError);
}

// 函数说明：writeByte 函数保存或发布对应的数据和资源。
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

// 函数说明：writeWord 函数保存或发布对应的数据和资源。
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

// 函数说明：writeDWord 函数保存或发布对应的数据和资源。
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
