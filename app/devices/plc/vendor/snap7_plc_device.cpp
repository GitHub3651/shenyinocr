// 文件作用：本文件用于把Snap7通信包装为项目统一PLC设备端口。
// 主要职责：把Snap7通信包装为项目统一PLC设备端口。
// 模块位置：设备层；通过统一端口隔离相机、PLC、OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "snap7_plc_device.h"

#include "devices/plc/vendor/snap7.h"

namespace {

const int kBackendUnavailable = -1;

// 函数说明：snap7WordLength 函数实现名称所表示的处理步骤。
int snap7WordLength(PlcDataWidth dataWidth)
{
    switch (dataWidth) {
    case PlcDataWidth::Byte:
        return S7WLByte;
    case PlcDataWidth::Word:
        return S7WLWord;
    case PlcDataWidth::DWord:
        return S7WLDWord;
    }

    return 0;
}

} // namespace

// 组件说明：Snap7PlcDevice 组件提供对应设备或检测能力的统一实现。
struct Snap7PlcDevice::Impl
{
    std::unique_ptr<TS7Client> client;
    Snap7PlcFunctions functions;
};

// 函数说明：Snap7PlcDevice 构造函数创建组件并初始化其依赖和初始状态。
Snap7PlcDevice::Snap7PlcDevice()
    : m_impl(new Impl)
{
    m_impl->client.reset(new TS7Client);
    TS7Client *client = m_impl->client.get();

    m_impl->functions.connectTo =
        [client](const char *address, int rack, int slot) {
        return client->ConnectTo(address, rack, slot);
    };
    m_impl->functions.disconnect = [client]() {
        return client->Disconnect();
    };
    m_impl->functions.isConnected = [client]() {
        return client->Connected();
    };
    m_impl->functions.writeArea =
        [client](int area,
                 int dbNumber,
                 int start,
                 int amount,
                 int wordLength,
                 void *data) {
        return client->WriteArea(
            area,
            dbNumber,
            start,
            amount,
            wordLength,
            data);
    };
}

// 函数说明：Snap7PlcDevice 构造函数创建组件并初始化其依赖和初始状态。
Snap7PlcDevice::Snap7PlcDevice(const Snap7PlcFunctions &functions)
    : m_impl(new Impl)
{
    m_impl->functions = functions;
}

Snap7PlcDevice::~Snap7PlcDevice() = default;

// 函数说明：connectTo 函数建立或断开对应外部连接。
PlcOperationResult Snap7PlcDevice::connectTo(
    const char *address,
    int rack,
    int slot)
{
    if (!m_impl->functions.connectTo) {
        return PlcOperationResult(kBackendUnavailable);
    }

    return PlcOperationResult(
        m_impl->functions.connectTo(address, rack, slot));
}

PlcOperationResult Snap7PlcDevice::disconnect()
{
    if (!m_impl->functions.disconnect) {
        return PlcOperationResult(kBackendUnavailable);
    }

    return PlcOperationResult(m_impl->functions.disconnect());
}

// 函数说明：isConnected 函数检查相关状态并返回判断结果。
bool Snap7PlcDevice::isConnected()
{
    return m_impl->functions.isConnected
        && m_impl->functions.isConnected();
}

// 函数说明：writeDbArea 函数保存或发布对应的数据和资源。
PlcOperationResult Snap7PlcDevice::writeDbArea(
    int dbNumber,
    int start,
    int amount,
    PlcDataWidth dataWidth,
    void *data)
{
    if (!m_impl->functions.writeArea) {
        return PlcOperationResult(kBackendUnavailable);
    }

    const int wordLength = snap7WordLength(dataWidth);
    if (wordLength == 0) {
        return PlcOperationResult(kBackendUnavailable);
    }

    return PlcOperationResult(
        m_impl->functions.writeArea(
            S7AreaDB,
            dbNumber,
            start,
            amount,
            wordLength,
            data));
}
