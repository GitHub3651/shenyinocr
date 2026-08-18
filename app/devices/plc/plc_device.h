// 文件作用：本文件用于定义PLC连接、写入和错误结果端口，隔离具体通信库。
// 主要职责：定义PLC连接、写入和错误结果端口，隔离具体通信库。
// 模块位置：设备层；通过统一端口隔离相机和PLC供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef PLC_DEVICE_H
#define PLC_DEVICE_H

// 组件说明：PlcDataWidth 枚举列出该组件允许使用的稳定状态和选项。
enum class PlcDataWidth
{
    Byte,
    Word,
    DWord
};

// 组件说明：PlcOperationResult 数据结构保存一次操作的结果、状态和错误信息。
struct PlcOperationResult
{
    // 函数说明：PlcOperationResult 构造函数创建组件并初始化其依赖和初始状态。
    explicit PlcOperationResult(int errorCode = 0)
        : nativeErrorCode(errorCode)
    {
    }

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return nativeErrorCode == 0;
    }

    int nativeErrorCode;
};

// 组件说明：IPlcDevice 组件提供对应硬件设备能力的统一实现。
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
