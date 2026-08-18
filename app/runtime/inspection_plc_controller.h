// 文件作用：本文件用于把检测结果转换为PLC输出请求，并维护延迟剔除和复位时序。
// 主要职责：把检测结果转换为PLC输出请求，并维护延迟剔除和复位时序。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "devices/plc/plc_device.h"

#include <QString>
#include <cstdint>
#include <memory>

// 组件说明：InspectionPlcRunSettingField 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionPlcRunSettingField
{
    None,
    RejectTime,
    RejectDistance,
    PhotoTime,
    PhotoDistance
};

// 组件说明：InspectionPlcRunSettings 组件集中描述相关配置、规则和运行参数。
struct InspectionPlcRunSettings
{
    std::uint16_t rejectTime = 0;
    std::uint32_t rejectDistance = 0;
    std::uint16_t photoTime = 0;
    std::uint32_t photoDistance = 0;
};

// 组件说明：InspectionPlcRunSettingsResult 数据结构保存一次操作的结果、状态和错误信息。
struct InspectionPlcRunSettingsResult
{
    InspectionPlcRunSettingField failedField =
            InspectionPlcRunSettingField::None;
    PlcOperationResult operation;

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return failedField == InspectionPlcRunSettingField::None
                && operation.isSuccess();
    }
};

// 组件说明：InspectionPlcAddressMap 数据结构集中保存该流程需要的一组相关数据。
struct InspectionPlcAddressMap
{
    int triggerModeDb = 0;
    int triggerModeOffset = 0;
    int resultDb = 0;
    int resultOffset = 0;
    int rejectTimeOffset = 0;
    int rejectDistanceOffset = 0;
    int photoTimeOffset = 0;
    int photoDistanceOffset = 0;
};

// 组件说明：InspectionPlcController 组件封装对应业务职责和生命周期边界。
class InspectionPlcController
{
public:
    explicit InspectionPlcController(
        std::unique_ptr<IPlcDevice> device,
        const InspectionPlcAddressMap &addresses);
    ~InspectionPlcController();

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
    PlcOperationResult writeByte(int db, int start, std::uint8_t value);
    PlcOperationResult writeWord(int db, int start, std::uint16_t value);
    PlcOperationResult writeDWord(int db, int start, std::uint32_t value);

    std::unique_ptr<IPlcDevice> m_device;
    InspectionPlcAddressMap m_addresses;
};
