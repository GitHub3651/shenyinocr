#ifndef SNAP7_PLC_DEVICE_H
#define SNAP7_PLC_DEVICE_H

#include "devices/plc/plc_device.h"

#include <functional>
#include <memory>

struct Snap7PlcFunctions
{
    std::function<int(const char *, int, int)> connectTo;
    std::function<int()> disconnect;
    std::function<bool()> isConnected;
    std::function<int(int, int, int, int, int, void *)> writeArea;
};

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
