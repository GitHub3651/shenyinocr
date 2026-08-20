# devices：相机与 PLC 的硬件端口和供应商适配器

## 一句话理解

`devices` 只负责“如何与真实物理设备说话”。上层面向稳定接口编程，海康 MVS 和 Snap7 的具体 API 被限制在 `vendor` 子目录中。

## 当前结构

```text
devices/
├─ camera/
│  ├─ camera_device.h
│  └─ vendor/
│     ├─ hikvision_camera_device.h
│     └─ hikvision_camera_device.cpp
├─ plc/
│  ├─ plc_device.h
│  └─ vendor/
│     ├─ snap7_plc_device.h
│     ├─ snap7_plc_device.cpp
│     ├─ snap7.h
│     └─ snap7.cpp
└─ README.md
```

本目录只有两类设备，不是检测模式目录：

- `camera`：获取独立图像帧、设置曝光/增益/触发模式。
- `plc`：建立网络连接并按地址读写原始字节。

## 依赖方向

```text
startup
  ├─ new HikvisionCameraDevice
  └─ new Snap7PlcDevice
          ↓ 注入接口
runtime/CameraSession      → ICameraDevice
runtime/PlcController      → IPlcDevice
```

只有 `startup` 构造具体实现。Runtime 只依赖 `ICameraDevice` 和 `IPlcDevice`，所以更换品牌时上层流程不用认识新的 SDK 类型。

## camera 文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `camera/camera_device.h` | 定义相机稳定端口：结果码、参数范围、触发模式、帧状态、枚举、打开、参数、触发、取帧、中断、停止和关闭。 | 不能暴露 SDK 句柄或回调缓冲区；`waitNextFrame/interruptWait` 必须支持协作停止。 |
| `camera/vendor/hikvision_camera_device.h` | 声明海康 MVS 适配器，使用 PImpl 隐藏 SDK 类型。 | 公共头只依赖通用相机端口，防止 MVS 头传播。 |
| `camera/vendor/hikvision_camera_device.cpp` | 调用 MVS 完成枚举、句柄、打开、取流、软触发、曝光/增益/TriggerDelay、像素转换和等待中断。 | SDK 回调内必须形成独立 `CameraFrame`；不做旋转、检测、统计或 UI 更新。 |

### 相机数据流

```text
MVS 回调临时缓冲区
→ HikvisionCameraDevice 转换并 clone
→ CameraFrame（独立图像所有权）
→ CameraSession
→ CaptureWorker
→ InspectionRuntime
```

不能把 MVS 回调给出的临时指针直接交给后台检测线程。

## plc 文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `plc/plc_device.h` | 定义数据宽度、操作结果和 `IPlcDevice` 的连接/断开/原始字节读写端口。 | 不认识产品 OK/NG、工艺字段、DB 地址表或 0/49。 |
| `plc/vendor/snap7_plc_device.h` | 声明 Snap7 适配器和可注入函数表，使用 PImpl 持有 `TS7Client`。 | 上层不能依赖 `TS7Client`。 |
| `plc/vendor/snap7_plc_device.cpp` | 把稳定 PLC 端口映射为 Snap7 连接、断开和 DB 区域写入，并转换原生错误。 | 不决定地址、字节序和结果值；这些属于 Runtime 的 PLC Controller。 |
| `plc/vendor/snap7.h/.cpp` | Snap7 官方 C++ 包装及原生类型实现。 | 视为供应商代码，不做业务重构、格式化或拆分；升级需与 DLL/LIB 整体匹配。 |

### PLC 调用分工

```text
ResultService 决定“本产品需要 OK/NG 动作”
→ InspectionPlcController 决定地址、顺序、Word/DWord、大端编码和 0/49
→ IPlcDevice 只执行指定地址的字节读写
→ Snap7PlcDevice 调用供应商 API
```

所以“NG 写 49，约 100 ms 后写 0”不应写进 `Snap7PlcDevice`。

## 允许放什么

- 稳定硬件端口和与供应商 API 的一对一适配。
- 原生错误码到稳定错误结构的转换。
- 设备缓冲区到应用自有数据的安全拷贝。
- 设备连接、触发、等待和中断等基础能力。

## 禁止放什么

- 产品判定、模式分支、模板和业务默认值。
- UI、弹窗、统计、存图和结果呈现。
- PLC 工艺地址、剔除策略和正常结果时序。
- 正式图像旋转、通道选择、ROI 或识别算法。
- 在适配器内部偷偷创建长期后台业务线程。

## 更换硬件的正确方式

更换相机时，实现新的 `ICameraDevice`，然后只在 `ApplicationStartup` 替换构造。更换 PLC 时，实现新的 `IPlcDevice`，同样只在组合根替换。不要复制 CameraSession、Runtime 或 UI。

## 维护与验证重点

- 相机：枚举/开关、软触发、硬触发、连续超时、曝光/增益范围、中断等待、停止后重开。
- 图像：像素格式、stride、深拷贝、回调锁和帧序号。
- PLC：连接恢复、地址/宽度/字节序、错误码、真实 PLC 写入和机械现场结果。
- 生命周期：停止后线程必须退出，再销毁设备；禁止 detach 或强杀。

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单、部署依赖和开发者维护指南。
