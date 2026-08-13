#include "snap7_plc_device.h"

#include "snap7.h"

namespace {

const int kBackendUnavailable = -1;

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

struct Snap7PlcDevice::Impl
{
    std::unique_ptr<TS7Client> client;
    Snap7PlcFunctions functions;
};

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

Snap7PlcDevice::Snap7PlcDevice(const Snap7PlcFunctions &functions)
    : m_impl(new Impl)
{
    m_impl->functions = functions;
}

Snap7PlcDevice::~Snap7PlcDevice() = default;

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

bool Snap7PlcDevice::isConnected()
{
    return m_impl->functions.isConnected
        && m_impl->functions.isConnected();
}

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
