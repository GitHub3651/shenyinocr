#ifndef PLC_DEVICE_H
#define PLC_DEVICE_H

enum class PlcDataWidth
{
    Byte,
    Word,
    DWord
};

struct PlcOperationResult
{
    explicit PlcOperationResult(int errorCode = 0)
        : nativeErrorCode(errorCode)
    {
    }

    bool isSuccess() const
    {
        return nativeErrorCode == 0;
    }

    int nativeErrorCode;
};

class IPlcDevice
{
public:
    virtual ~IPlcDevice() = default;

    virtual PlcOperationResult connectTo(
        const char *address,
        int rack,
        int slot) = 0;
    virtual PlcOperationResult disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual PlcOperationResult writeDbArea(
        int dbNumber,
        int start,
        int amount,
        PlcDataWidth dataWidth,
        void *data) = 0;
};

#endif // PLC_DEVICE_H
