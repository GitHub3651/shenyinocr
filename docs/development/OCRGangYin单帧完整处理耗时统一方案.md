# OCRGangYin 单帧完整处理耗时统一方案

## 1. 文档状态

- 方案日期：2026-08-24。
- 当前状态：代码实施完成，静态门禁通过，待用户统一验证。
- 权威范围：五种检测模式在检测页面显示的“检测耗时”定义、计时起止点、数据传递、格式化规则，以及旧算法计时字段和逻辑的删除范围。
- 当前进度：四个阶段的生产代码修改和静态检查均已完成；五种模式旧 UI 计时、旧墙钟接口和无消费者的相机时间戳已删除，统一 `steady_clock` 起点和 `ResultService` 终点已经落地；等待用户在 Qt Creator 中构建并统一验证五种模式。
- 替代关系：本方案实施后，统一替代钢印、字库、深度 OCR、纸巾、二维码+三期五种模式各自维护的 UI 检测耗时口径；二维码解码器内部用于预算、超时和日志的耗时不属于替代范围。
- 实施门禁：本轮代码实施已完成，不再继续修改生产代码；完整 Qt/qmake/MSVC 构建、五种模式人工验证、真实相机和 PLC 验证由用户统一执行。验证完成前不得标记为已完成基线。

## 2. 目标

检测页面中的“检测耗时”统一表示：

> Runtime 接收到一张有效图片后，到该图片的同步生产处理已经完成、即将把呈现数据投递给 UI 之前的总时间。

该时间用于尽量真实地反映“一张图片在后台完成生产处理需要多久”，而不是只反映某个算法函数的运行时间。

五种模式统一显示：

```text
检测耗时 123.45 ms
```

固定保留两位小数，不再由模式决定小数位数，也不提供设置项。

## 3. 当前问题

### 3.1 五种模式的起止点不一致

当前耗时由各算法分别计算：

- 钢印模式主要统计钢印 Pipeline 内部算法时间；
- 字库模式分别统计定位、ROI 准备和字符检测时间后再相加；
- 深度 OCR 模式统计 OCR Pipeline 内部时间；
- 纸巾模式由 `DetectionRegistry` 在纸巾检测器外层计时；
- 二维码+三期模式把多模板定位时间和 Pipeline 内部时间相加。

因此当前数值不能稳定代表相同范围，也不能直接比较五种模式的完整处理效率。

### 3.2 当前算法耗时没有覆盖完整生产处理

分散在算法层的计时通常没有完整覆盖：

- Runtime 接收图片和克隆原图；
- 检测工作队列等待；
- 公共预处理；
- Runtime 对检测结果的验收；
- 统计更新；
- 标注结果图生成；
- 异步存图任务提交；
- 当前产品的 PLC 结果请求。

这些步骤都是处理一张图片的同步后台流程，应纳入统一耗时。

### 3.3 当前最终耗时混用了两种时钟

`ResultService::presentationElapsedMs()` 当前会比较：

```text
算法层记录的 elapsedMs
与
FrameData::timestampUtc 到当前 QDateTime 的墙钟时间
```

然后取较大值。该实现存在三个问题：

1. 算法计时和墙钟计时的起点不同；
2. 系统时间被人工修改或自动校时时，`QDateTime` 不适合计算持续时间；
3. 最终先取整为整数毫秒，再按模式决定显示整数或两位小数，二维码模式显示的 `.00` 不是真实的小数毫秒。

## 4. 最终计时边界

### 4.1 唯一起点

起点固定在：

```text
InspectionRuntime::acceptFrame()
    → 已确认输入图片非空
    → 尚未获取 Runtime 状态锁
    → 立即记录 processingStartedAt
```

计时必须发生在以下操作之前：

- 等待 `InspectionRuntime` 状态锁；
- 生成 `ProductKey`；
- 克隆原图；
- 登记已接收产品；
- 把帧提交到检测工作队列。

这样锁等待、原图复制和排队时间都会被统计。

空图片直接返回，不创建无意义的起始时间。Fault 或非运行状态下未被接收为产品的图片不产生检测耗时。

### 4.2 唯一终点

终点固定在：

```text
ResultService::process()
    → 完成统计、呈现图生成、存图任务提交和当前产品 PLC 请求
    → 使用 steady_clock::now() 计算耗时
    → 写入 InspectionPresentation::elapsedText
    → 调用 InspectionRuntime::publishPresentation()
```

必须在 `publishPresentation()` 调用之前取结束时间。UI mailbox 投递和 UI 刷新不属于检测耗时。

### 4.3 纳入耗时的步骤

统一耗时包含：

```text
接收有效图片
→ Runtime 状态锁等待和产品登记
→ 原图克隆
→ 检测队列等待
→ 公共图像预处理
→ 单模板定位或多模板定位
→ 当前模式算法
→ DetectionResult 与 Overlay 生成
→ Runtime 完成结果验收与认领
→ 统计更新
→ 标注结果图生成
→ 异步存图任务提交
→ 当前产品 PLC 结果请求
→ 生成固定两位小数的耗时文本
```

五种模式全部使用这一条边界，不允许某个模式重新定义自己的 UI 检测耗时。

### 4.4 明确排除的步骤

统一耗时不包含：

- PLC 触发相机；
- 相机曝光、传感器采集和相机 SDK 像素转换；
- 图片实际异步压缩和磁盘写入，只包含把存图任务成功或失败地提交给异步服务；
- `publishPresentation()` 内部的 UI mailbox 投递；
- UI mailbox 中等待显示的时间；
- Qt 主线程事件调度；
- `InspectionPage` 控件更新和屏幕重绘；
- NG 脉冲后续复位；
- PLC 收到结果后的机械动作。

排除 UI 时间是必要的：UI 允许覆盖尚未显示的旧结果，UI 调度速度不能改变生产处理耗时。排除实际磁盘写入是必要的：现有异步存图不能反向阻塞检测线程。

## 5. 最终数据设计

### 5.1 `FrameData` 保存唯一开始时间

`FrameData` 删除：

```cpp
QDateTime timestampUtc;
```

改为：

```cpp
std::chrono::steady_clock::time_point processingStartedAt;
```

`processingStartedAt` 的职责只有一个：随同一张图片从 Runtime 一直传到 `ResultService`，用于计算完整同步处理时间。

它不是相机拍摄时间，不用于排序、日志时间戳或持久化。

### 5.2 `makeFrameData()` 只传递同一个开始时间

`makeFrameData()` 的 `timestampUtc` 参数替换为 `processingStartedAt`。

两处调用固定为：

1. `InspectionRuntime::acceptFrame()` 创建首个 `FrameData` 时传入刚记录的起始时间；
2. `DetectionRegistry::preprocessFrame()` 创建预处理后的 `FrameData` 时原样传递源帧的 `processingStartedAt`。

预处理后不得重新取时间，否则会丢失原图克隆和队列等待时间。

### 5.3 `InspectionRuntime::acceptFrame()` 删除墙钟参数

删除：

```cpp
const QDateTime &timestampUtc = QDateTime()
```

最终公开接口只接收图片、帧号和相机索引。当前调用方 `CameraSession::submitFrame()` 本来就只传图片，不需要兼容重载或默认墙钟参数。

### 5.4 删除 `CameraFrame` 中无消费者的墙钟时间

设备层当前还存在：

```cpp
QDateTime CameraFrame::timestampUtc;
```

它只在海康相机回调完成像素转换和图像克隆后赋值，`CameraSession` 及其他代码没有任何读取位置，也没有参与相机控制、帧排序、检测、日志或存图。

本次直接删除：

- `CameraFrame::timestampUtc` 字段；
- 海康相机回调中的 `frame.timestampUtc = QDateTime::currentDateTimeUtc();`；
- `camera_device.h` 和 `hikvision_camera_device.cpp` 因此变为无用的 `QDateTime` include。

不新增替代字段，不把 `processingStartedAt` 下沉到相机设备层，不保留兼容接口。`CameraFrame` 继续只传递实际使用的帧序号和图像。

### 5.5 算法结果不再携带 UI 耗时

从 `DetectionResult` 删除：

```cpp
double elapsedMs;
int elapsedDecimals;
```

算法层只负责检测状态、判定、识别内容、诊断和 Overlay，不再负责决定页面耗时。

从 `DetectionPose` 删除：

```cpp
double trackingElapsedMs;
```

定位结果只保存定位几何和匹配信息，不再为了 UI 耗时携带局部计时。

从 `TissueRollResult` 删除：

```cpp
int processingTimeMs;
```

纸巾算法结果只保存检测结果和诊断数据。

### 5.6 模式描述不再配置小数位

从 `DetectionModeDescriptor` 删除：

```cpp
int elapsedDecimals;
```

同步删除五种模式描述表末尾的小数位参数，以及 `DetectionRegistry::applyDescriptorPolicy()` 对该字段的赋值。

固定两位小数是统一呈现规则，不是模式能力，不需要配置。

## 6. `ResultService` 最终处理顺序

`ResultService::process()` 的最终同步顺序固定为：

```text
1. 验证 completion
2. Runtime 认领本产品结果
3. 处理到期的延迟 NG 请求
4. 更新识别总数、不合格数和延迟 NG 队列
5. 生成 InspectionPresentation 和标注结果图
6. 提交本产品异步存图任务
7. 把最新统计写入 InspectionPresentation
8. Runtime 未处于 Fault 时，请求本产品 PLC 结果
9. steady_clock::now() - frame.processingStartedAt
10. 直接生成“检测耗时 %.2f ms”文本
11. Runtime::publishPresentation(presentation)
```

第 9 步在 PLC 请求之后执行，因此正常同步 PLC 请求耗时被纳入。若 Runtime 已进入 Fault 而跳过当前产品 PLC 请求，则只统计实际执行的流程。

耗时计算固定使用：

```cpp
const double processingElapsedMs =
    std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now()
        - request.completion.frame->processingStartedAt).count();

presentation.elapsedText = QStringLiteral("检测耗时 %1 ms")
    .arg(processingElapsedMs, 0, 'f', 2);
```

不先转换为整数，不使用 `duration_cast<std::chrono::milliseconds>` 截断小数，不使用 `QDateTime`，不再取两个耗时的最大值。

### 6.1 删除 `finalizePresentation`

删除 `ResultService::ProcessRequest` 中的：

```cpp
std::function<void(InspectionPresentation *)> finalizePresentation;
```

删除 `handleCompletion()` 中只为耗时创建的 Lambda，以及 `process()` 中对该 Lambda 的调用。

耗时文本直接在 `process()` 的唯一终点生成。无需为了写一个字段保存回调。

### 6.2 删除旧最终耗时函数

删除：

```cpp
ResultService::presentationElapsedMs()
```

同时删除其声明、定义和调用，不保留兼容包装函数。

## 7. 五种模式的具体精简

### 7.1 钢印模式

`stamp_detection_pipeline.cpp` 删除 Pipeline 内部计时起点、结束计算和 `DetectionResult::elapsedMs` 赋值；其判定、诊断和 Overlay 生成保持不变。

### 7.2 字库模式

`word_detection_pipeline.cpp` 删除：

- ROI 准备阶段 `prepareElapsedMs`；
- 字符检测阶段局部计时；
- 对 `item.pose.trackingElapsedMs` 的读取；
- 各段耗时相加及 `DetectionResult::elapsedMs` 赋值。

字符匹配、缺字判定、识别文本、诊断和 Overlay 保持不变。

### 7.3 深度 OCR 模式

`ocr_detection_pipeline.h/.cpp` 删除：

- Pipeline 内部开始和结束计时；
- `toDetectionResult(..., double elapsedMs)` 的耗时参数；
- 无效 ROI 分支传入的占位耗时；
- `DetectionResult::elapsedMs` 赋值。

OCR 引擎调用和结果判定保持不变。

### 7.4 纸巾模式

`detection_registry.cpp` 删除包围纸巾 `detect()` 的局部计时和 `TissueRollResult::processingTimeMs` 赋值。

`tissue_roll_detector.h` 删除 `processingTimeMs` 字段，`tissue_detection_pipeline.cpp` 删除向 `DetectionResult::elapsedMs` 的复制。纸巾定位、粗糙度和判定保持不变。

### 7.5 二维码+三期模式

`barcode_word_detection_pipeline.cpp` 删除：

- Pipeline 内部计时起点；
- `elapsedMs` 局部 Lambda；
- 对 `item.pose.trackingElapsedMs` 的读取；
- 正常完成和各 NG 提前返回分支中的 `DetectionResult::elapsedMs` 赋值。

二维码解码、日期字符检测、判定、诊断和 Overlay 保持不变。

### 7.6 公共定位

`inspection_positioner.cpp` 删除多模板定位使用的局部计时和 `DetectionPose::trackingElapsedMs` 赋值。单模板、多模板选择和并行匹配逻辑保持不变。

## 8. 保留的内部性能计时

保留二维码引擎内部：

```cpp
BarcodeReadResult::elapsedMs
```

以及 `barcode_decoder_adapter.cpp` 对该字段的计算和使用。它用于：

- 解码预算；
- 超时判断；
- 解码日志。

它不是检测页面显示的耗时，不能删除，也不能改为完整单帧耗时。

允许保留的旧局部计时采用唯一白名单：

- `app/engines/barcode/barcode_types.h` 中的 `BarcodeReadResult::elapsedMs`；
- `app/engines/barcode/vendor/barcode_decoder_adapter.cpp` 中计算该字段的 `QElapsedTimer` 和 `elapsedMilliseconds()`。

除这两项外，不保留任何旧算法层、定位层、Detection 层或 Runtime 层的局部检测耗时、计时字段、辅助函数、Lambda、兼容接口或无用 include。实施者不得自行增加其他保留项。

## 9. UI、统计、存图和 PLC 行为保持

### 9.1 UI

- `InspectionPage` 继续只显示 `InspectionPresentation::elapsedText`；
- 不在 UI 中重新计算或格式化耗时；
- 不修改容量为一的 UI mailbox；
- 不要求 UI 显示每一张中间图片；
- 停止识别后保留最后一次判定和耗时的现有行为不变。

### 9.2 统计

- 识别总数和不合格数的更新位置、顺序和准确性不变；
- 统计更新发生在耗时终点之前；
- UI 是否跳帧不影响统计。

### 9.3 存图

- 存图条件、目录、文件格式和任务内容不变；
- 只把同步的任务构建和提交时间计入检测耗时；
- 实际编码和磁盘写入继续异步执行；
- 不为获得“更完整”的时间而等待存图完成。

### 9.4 PLC

- 延迟 NG、OK/NG 请求、Fault 判断和脉冲复位逻辑不变；
- 当前产品同步 PLC 请求发生在计时终点之前；
- 后续定时复位和机械动作不纳入本次耗时。

## 10. 文件级修改清单

### 10.1 合同和公共数据

| 文件 | 确定修改 |
|---|---|
| `app/contracts/detection_mode.h` | 删除 `DetectionModeDescriptor::elapsedDecimals` |
| `app/contracts/detection_mode.cpp` | 删除五种模式描述中的小数位参数 |
| `app/detection/common/detection_pose.h` | 用 `processingStartedAt` 替换 `FrameData::timestampUtc`；调整 `makeFrameData()`；删除 `DetectionResult` 的两个耗时字段和 `DetectionPose::trackingElapsedMs`；按实际使用调整头文件 include |

### 10.2 Detection

| 文件 | 确定修改 |
|---|---|
| `app/detection/common/inspection_positioner.cpp` | 删除多模板定位 UI 耗时计时，清理无用 `<chrono>` include |
| `app/detection/detection_registry.cpp` | 预处理帧继续传递统一起始时间；删除模式小数位赋值和纸巾局部计时，清理无用 `<chrono>` include |
| `app/detection/detectionmode/stamp/stamp_detection_pipeline.cpp` | 删除钢印局部 UI 耗时计算 |
| `app/detection/detectionmode/word/word_detection_pipeline.cpp` | 删除定位、ROI 准备和字符检测耗时拼接 |
| `app/detection/detectionmode/ocr/ocr_detection_pipeline.h` | 删除 `toDetectionResult()` 的耗时参数 |
| `app/detection/detectionmode/ocr/ocr_detection_pipeline.cpp` | 删除 OCR 局部 UI 耗时计算并同步函数签名 |
| `app/detection/detectionmode/tissue/tissue_roll_detector.h` | 删除 `processingTimeMs` |
| `app/detection/detectionmode/tissue/tissue_detection_pipeline.cpp` | 删除纸巾耗时字段复制 |
| `app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.cpp` | 删除定位与 Pipeline 耗时拼接 |

### 10.3 Runtime

| 文件 | 确定修改 |
|---|---|
| `app/runtime/inspection_runtime.h` | 删除 `acceptFrame()` 的 `timestampUtc` 参数 |
| `app/runtime/inspection_runtime.cpp` | 在有效图片进入 Runtime 时记录 `steady_clock` 起点并写入 `FrameData` |
| `app/runtime/result_service.h` | 删除 `finalizePresentation` 和 `presentationElapsedMs()` |
| `app/runtime/result_service.cpp` | 在 `publishPresentation()` 前计算完整同步耗时并固定格式化为两位小数；删除旧 Lambda、旧墙钟计算和取最大值逻辑 |

### 10.4 相机设备

| 文件 | 确定修改 |
|---|---|
| `app/devices/camera/camera_device.h` | 删除无消费者的 `CameraFrame::timestampUtc` 和无用 `QDateTime` include |
| `app/devices/camera/vendor/hikvision_camera_device.cpp` | 删除 `frame.timestampUtc` 赋值和无用 `QDateTime` include |

### 10.5 明确不修改

- 不新增生产代码文件；
- 不修改 `app/ui/pages/inspection_page.h/.cpp`；
- 不修改 `app/contracts/inspection_presentation.h`，继续使用现有 `elapsedText`；
- 不修改 `app/runtime/camera_session.h/.cpp` 的采集流程；
- 不修改 `app/engines/barcode/barcode_types.h` 和二维码解码器内部耗时；
- 不修改 `.pro` 文件，因为本方案不增删生产代码文件；
- 不新增配置项、设置页面控件或翻译资源。

## 11. 分阶段实施

### 阶段 1：建立唯一跨流程计时数据

1. 在 `FrameData` 中用 `processingStartedAt` 替换 `timestampUtc`；
2. 修改 `makeFrameData()`；
3. 修改 `InspectionRuntime::acceptFrame()`，在状态锁、产品登记和原图克隆前记录起点；
4. 修改 `DetectionRegistry::preprocessFrame()`，确保预处理帧继续携带原始起点；
5. 删除 `acceptFrame()` 已无用途的墙钟参数；
6. 删除 `CameraFrame::timestampUtc` 及海康相机回调中的赋值；
7. 删除相机设备头文件和实现文件中随之无用的 `QDateTime` include。

阶段门禁：所有被接受并进入检测的帧只记录一次起点，预处理和队列传递过程中不重置。

### 阶段 2：删除五种模式的分散 UI 计时

1. 删除模式描述中的小数位策略；
2. 删除 `DetectionResult`、`DetectionPose` 和 `TissueRollResult` 的 UI 耗时字段；
3. 删除公共定位和五种 Pipeline 中对应的局部计时与赋值；
4. 只保留白名单中的 `BarcodeReadResult::elapsedMs`、`QElapsedTimer` 和 `elapsedMilliseconds()`；
5. 清理因计时删除而不再使用的 `<chrono>` include 和函数参数。

阶段门禁：Detection 层不再决定 `InspectionPresentation::elapsedText` 的数值或小数位，算法判定输出保持不变。

### 阶段 3：在 `ResultService` 形成唯一最终耗时

1. 删除 `ProcessRequest::finalizePresentation`；
2. 删除 `presentationElapsedMs()`；
3. 保持统计、渲染、存图任务提交和 PLC 请求的既有业务顺序；
4. 在当前产品 PLC 请求之后、`publishPresentation()` 之前计算耗时；
5. 固定生成两位小数的 `elapsedText`。

阶段门禁：完整耗时只计算一次，UI 只消费字符串，不增加新回调或中间对象。

### 阶段 4：静态核对与用户统一验证

静态检查已完成；下一步由用户使用 Qt Creator 构建并统一验证五种模式。本方案不把静态检查表述为编译或现场验证通过。

## 12. 禁止的过度设计和范围扩张

实施时禁止：

- 新增 `TimerManager`、计时服务、计时策略类或通用性能框架；
- 新增耗时配置项、模式级精度字段或 UI 精度选择；
- 保留旧 `elapsedMs`、`elapsedDecimals` 或 `presentationElapsedMs()` 作为兼容接口；
- 同时保留算法耗时和完整耗时并让 UI 二选一；
- 使用 `QDateTime`、系统墙钟或多个局部耗时相加模拟完整耗时；
- 为默认构造的 `processingStartedAt` 增加无实际调用来源的兜底分支；
- 等待异步存图完成；
- 把 UI mailbox、控件刷新或屏幕重绘纳入检测线程计时；
- 修改检测阈值、算法判定、模板选择、图像保存规则或 PLC 业务时序；
- 为已经删除的字段提供兼容重载、别名或迁移分支。
- 保留 `CameraFrame::timestampUtc`、对应赋值或无用的 `QDateTime` include；
- 在二维码解码器白名单之外自行保留其他旧局部计时。

最终只增加一个随 `FrameData` 传递的 `steady_clock` 起点，在 `ResultService` 中做一次减法和一次格式化；其余工作以删除旧字段和旧计时代码为主。

## 13. 静态门禁

实施后必须完成以下检查：

### 13.1 旧 UI 耗时字段清零

在 `app/contracts`、`app/detection` 和 `app/runtime` 中搜索以下名称，除二维码引擎明确保留项外必须无结果：

```text
elapsedDecimals
DetectionResult::elapsedMs 对应字段和赋值
trackingElapsedMs
processingTimeMs
presentationElapsedMs
finalizePresentation
timestampUtc
```

其中 `timestampUtc` 应在 `app/detection`、`app/runtime` 和 `app/devices/camera` 中全部清零。`elapsedMs` 只允许存在于第 8 节列出的二维码解码器白名单中。

### 13.2 时间来源唯一

确认：

- `processingStartedAt` 只在 `InspectionRuntime::acceptFrame()` 记录；
- `DetectionRegistry::preprocessFrame()` 只传递、不重置该时间；
- 页面检测耗时只在 `ResultService::process()` 格式化；
- `InspectionPage` 只显示 `presentation.elapsedText`。

### 13.3 范围保持

确认：

- `BarcodeReadResult::elapsedMs`、`QElapsedTimer`、`elapsedMilliseconds()` 及解码超时逻辑仍存在，并且没有其他旧局部计时保留；
- 五种算法的 verdict、recognizedText、diagnostic 和 Overlay 生成逻辑没有被改写；
- 统计、存图、PLC 和 mailbox 的业务条件没有变化；
- 没有新增生产代码文件或 `.pro` 条目；
- 用户未跟踪文件未被修改、暂存或提交。

### 13.4 基础检查

执行：

```text
git diff --check
```

并核对计划范围内的实际差异。完整 Qt/qmake/MSVC 构建继续由用户在 Qt Creator 中执行。

实施结果（2026-08-24）：

- `git diff --check` 通过；
- `elapsedDecimals`、`trackingElapsedMs`、`processingTimeMs`、`presentationElapsedMs`、`finalizePresentation` 和 `timestampUtc` 在 `app/` 中全部清零；
- `elapsedMs`、`QElapsedTimer` 和 `elapsedMilliseconds()` 只存在于二维码解码器白名单；
- `processingStartedAt` 只在 `InspectionRuntime::acceptFrame()` 记录，预处理只传递，`ResultService::process()` 只读取；
- `InspectionPage`、二维码解码器和 `.pro` 文件保持零差异；
- 未执行 Qt/qmake/MSVC 构建、人工交互、真实相机或 PLC 验证。

## 14. 用户统一验证清单

用户构建成功后，对钢印、字库、深度 OCR、纸巾、二维码+三期五种模式分别验证：

1. 正常检测结果显示 `检测耗时 xx.xx ms`；
2. 五种模式始终保留两位小数，不出现整数“毫秒”格式或模式间精度差异；
3. OK、NG 的判定、识别文本、模板名称和标注图与修改前一致；
4. 识别总数和不合格数准确，UI 跳过中间显示时统计仍不丢失；
5. 应保存的原图和标注图不漏保存；
6. PLC OK/NG 和延迟 NG 行为与修改前一致；
7. 停止识别后仍保留最后一次判定结果和耗时；
8. 连续检测时耗时会随队列等待和完整后台处理负载合理变化，不再只反映某个算法函数；
9. 二维码解码超时、预算和日志行为保持正常。

通过标准：五种模式的页面耗时具有相同起止边界和相同两位小数格式，且算法判定、统计、存图、PLC、mailbox 和停止显示行为没有回归。
