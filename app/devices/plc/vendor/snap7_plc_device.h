// 文件作用：本文件用于把Snap7通信包装为项目统一PLC设备端口。
// 主要职责：把Snap7通信包装为项目统一PLC设备端口。
// 模块位置：设备层；通过统一端口隔离相机和PLC供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef SNAP7_PLC_DEVICE_H
#define SNAP7_PLC_DEVICE_H

#include "devices/plc/plc_device.h"

#include <functional>
#include <memory>

// 组件说明：Snap7PlcFunctions 数据结构集中保存该流程需要的一组相关数据。
struct Snap7PlcFunctions
{
    std::function<int(const char *, int, int)> connectTo;
    std::function<int()> disconnect;
    std::function<bool()> isConnected;
    std::function<int(int, int, int, int, int, void *)> writeArea;
};

// 组件说明：Snap7PlcDevice 组件提供对应硬件设备能力的统一实现。
class Snap7PlcDevice final : public IPlcDevice
{
public:
    Snap7PlcDevice();
    explicit Snap7PlcDevice(const Snap7PlcFunctions &functions);
    ~Snap7PlcDevice() override;

    PlcOperationResult connectTo(
        const char *address,
        int rack,
        int slot) override;
    PlcOperationResult disconnect() override;
    bool isConnected() override;
    PlcOperationResult writeDbArea(
        int dbNumber,
        int start,
        int amount,
        PlcDataWidth dataWidth,
        void *data) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // SNAP7_PLC_DEVICE_H
