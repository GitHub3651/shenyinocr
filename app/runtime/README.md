# runtime：正式检测的生命周期、线程、结果副作用和呈现

## 一句话理解

`runtime` 把“一次正式检测运行”从开始管理到停止：采集相机帧、排队、调用 Detection、保证每个产品只结算一次，然后统一完成统计、PLC、存图和 UI 呈现。它不实现五种具体算法。

## 为什么文件看起来多

Runtime 涉及真实线程、设备和不可重复副作用，必须把不同生命周期拆开。当前 21 个代码文件可折叠为 10 组头源配对/数据合同：

```text
runtime/
├─ 相机采集
│  ├─ camera_session.h/.cpp
│  └─ capture_worker.h/.cpp
├─ 正式检测线程
│  ├─ frame_queue.h/.cpp
│  ├─ detection_worker.h/.cpp
│  └─ inspection_runtime.h/.cpp
├─ 结果与副作用
│  ├─ result_service.h/.cpp
│  ├─ inspection_plc_controller.h/.cpp
│  └─ image_save_service.h/.cpp
├─ UI 整体呈现
│  ├─ inspection_presentation.h
│  ├─ inspection_presentation_renderer.h/.cpp
│  └─ result_presentation_mailbox.h/.cpp
└─ README.md
```

这些不是重复 Runtime，而是一条正式生产链上不同的所有权和阻塞边界。

## 主调用链

```text
CameraSession
→ CaptureWorker
→ InspectionRuntime::acceptFrame
→ FrameQueue（容量 1）
→ DetectionWorker（串行）
→ DetectionRegistry/Pipeline
→ DetectionCompletion
→ ResultService
   ├─ 统计
   ├─ InspectionPlcController
   ├─ ImageSaveService（容量 32，2 个线程）
   └─ InspectionPresentation
      → UiCompletionMailbox（容量 1）
      → UI 线程 Renderer
```

## 相机采集

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `camera_session.h` | 定义相机打开/参数/恢复 DTO、采集配置、回调和 Session 接口。 | 只依赖 `ICameraDevice`，不出现海康类型。 |
| `camera_session.cpp` | 打开首台相机、曝光/增益、软硬触发配置、TriggerDelay、预览、正式采集、停止恢复和原帧提交。 | 正式帧原样进入 Detection；不要在此做正式旋转、通道或定位。 |
| `capture_worker.h` | 定义 Preview、SoftwareTrigger、HardwareTrigger 三种模式和单一采集 Worker。 | 只有一个 `std::thread`，必须支持协作停止和重复启动。 |
| `capture_worker.cpp` | 软件模式主动触发并等待；硬件/预览模式等待帧；连续超时报告错误；停止时中断等待并 join。 | 禁止 detach、强杀或超时后泄漏对象。 |

模板预览也使用 CameraSession，但预览不是正式产品：不分配 ProductKey、不统计、不写 PLC、不正式存图。

## 帧队列与检测线程

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `frame_queue.h/.cpp` | 容量明确的 FIFO，支持阻塞/非阻塞提交、等待取出、取消和重新打开。 | 正式容量当前为 1；修改会改变反压与硬触发溢出语义。 |
| `detection_worker.h/.cpp` | 串行消费队列并调用注入的通用 DetectionExecutor，管理停止、join、计数和基础设施失败。 | 不识别五种模式，不统计、不写 PLC、不更新 UI。 |
| `inspection_runtime.h` | 定义 Idle/Starting/Running/Stopping/Fault 唯一状态机、Fault 快照和公开接口。 | Runtime 是运行状态唯一真源。 |
| `inspection_runtime.cpp` | 定义一次运行唯一且不可变的 RunContext；实现开始/回滚/停止/Fault、产品账本、Worker 生命周期、UI 邮箱和服务委托。 | 每个 ProductKey 只能完成/claim 一次；停止先禁止新受理，再取消等待，最后 join。 |

### 为什么有 FrameQueue

相机线程与检测线程速度不同，不能直接互相调用并无限堆积。容量 1 表示最多只允许一帧等待检测，从而形成明确背压；这不是普通性能参数。

## 结果、副作用和 PLC

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `result_service.h` | 定义保存动作、PLC 动作、运行保存配置、统计、延迟 NG 和处理结果。 | 所有产品结果副作用的唯一入口。 |
| `result_service.cpp` | 对唯一 Completion 进行统计、PLC、存图、延迟 NG/复位、异常计数和完整 Presentation 发布。 | 正常 OK=0；NG=49 后约 100ms 写 0。Fault 不得猜测补发 NG。 |
| `inspection_plc_controller.h/.cpp` | 维护 PLC 工艺字段、地址、Word/DWord 和大端编码，按稳定顺序委托 `IPlcDevice` 写入。 | 地址、宽度、字节序和顺序属于现场合同。 |
| `image_save_service.h/.cpp` | 以 2 个后台线程消费容量 32 的产品任务队列，保存原图/标注图并汇总写入错误。 | 一个产品提交一个任务；队满等待，不能丢弃本应保存的任务或每图开线程。 |

`DetectionResult` 只说明算法结果；只有 `ResultService` 能把结果变成统计、PLC 写入、图片文件和用户可见 Presentation。

## UI 整体呈现

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `inspection_presentation.h` | 一次产品的完整 UI 快照：判定、正常/异常统计、图像、Overlay、文字、模板名、耗时和错误。 | 新显示字段进入同一快照，不另发独立排队信号。 |
| `inspection_presentation_renderer.h/.cpp` | 绘制 Overlay，格式化判定、文字、统计、合格率和耗时，并一次性应用到显式 ViewBindings。 | 只改变显示，不改变结果或统计。 |
| `result_presentation_mailbox.h/.cpp` | 容量固定为 1 的 UI 工作邮箱；提交端等待空位，UI 侧一次取出完整工作，取消时唤醒。 | 防止无界 Qt 事件积压和跨产品画面/文字错配。 |

文件名 `result_presentation_mailbox` 保留历史命名，实际类型 `UiCompletionMailbox` 表达的是“容量 1 的 UI 完整结果工作槽”。

## 状态机

```text
Idle
  → Starting
  → Running
  → Stopping
  → Idle

Starting / Running / Stopping
  → Fault
  → 操作员确认且线程退出
  → Idle
```

- Fault 是系统基础设施故障，不等于产品 NG。
- 已完成算法结论保留。
- 未完成产品记 `Unconfirmed`，不能猜测为 NG。
- UI 只能说明视觉检测暂停、输送线状态未知，不能声称机械设备已经安全停止。

## 线程和容量

| 执行环境 | 所有者 | 阻塞点 | 停止方式 |
|---|---|---|---|
| UI 线程 | QApplication | Qt 事件循环、邮箱消费 | 正常关闭窗口 |
| CaptureWorker | CameraSession | `waitNextFrame`、正式反压 | stop flag + `interruptWait()` + join |
| DetectionWorker | InspectionRuntime | FrameQueue、结果/呈现背压 | queue cancel + requestStop + join |
| 2 个存图线程 | ImageSaveService | 容量 32 任务队列 | shutdown 唤醒并 join |

固定容量：正式 FrameQueue=1，UI Mailbox=1，ImageSaveService=32 个产品任务/2 个 Writer。调整容量会改变吞吐、内存和 Fault 语义，必须作为行为变化评审。

## 允许放什么

- 运行状态机、线程、队列、取消/join 和产品账本。
- 相机会话编排、结果结算、统计、PLC、存图和 UI 完整结果交付。
- 与运行生命周期直接相关的 Fault、快照和诊断。

## 禁止放什么

- 五种模式专用 Pipeline、阈值和算法分支。
- QWidget、QMessageBox、Ui::MainWindow 或按钮权限。
- 运行中直接读取配方 JSON、模板目录或 UI 控件。
- 供应商 SDK 类型。
- 多条结果旁路、无界队列、detach 或 `QThread::terminate()`。

## 常见维护入口

| 需求 | 首先修改 | 不能顺手修改 |
|---|---|---|
| 改软/硬触发采集 | `camera_session.*`、`capture_worker.*` | Detection 算法 |
| 改停止/Fault | `inspection_runtime.*` | 把 Fault 当 NG |
| 改统计/PLC/存图 | `result_service.*` | UI/Pipeline 增加旁路 |
| 改 PLC 地址/编码 | `inspection_plc_controller.*` | Snap7 适配器写业务值 |
| 改结果显示字段 | `inspection_presentation*` | 分散多个 Qt 队列信号 |
| 改队列容量 | 对应 Queue/Service | 作为“无行为影响”的普通优化 |

## 推荐阅读顺序

```text
camera_session.*
→ capture_worker.*
→ frame_queue.*
→ detection_worker.*
→ inspection_runtime.*
→ result_service.*
→ inspection_presentation* / mailbox
```

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单和开发者维护指南。
