# OCRGangYin 正式产品失效安全 A2 实施计划

## 1. 状态与范围

- 方案日期：2026-08-30。
- 当前状态：生产代码已按本计划完成实施，等待用户统一验证。
- 当前授权：用户已授权实施生产代码；不修改测试工程、Schema、UI 布局或 PLC 协议。
- 唯一目标：已取得正式 `ProductKey` 的可检测产品必须产生明确 OK 或 NG；系统无法可信检测时必须进入现有 Runtime Fault。
- 最新决定：删除产品结果中的 `DetectionStatus` 和 `AlgorithmVerdict::NotEvaluated`，不再保留 `Completed`、`Cancelled`、`SystemFault` 等产品结果状态；所有模式的 ROI 无效只形成普通 NG 或按执行故障进入 Runtime Fault，不发布 ROI 专用警告。
- 本文是阶段 A2 的唯一实施权威，不包含字符算法、定位算法、阈值、模板格式或 UI 布局改造；允许删除现有 ROI 专用警告代码通路，但保留 `label_runtimeStatus` 的运行中、停止、Fault 和其他既有警告显示。

## 2. 最终结果合同

### 2.1 产品结果只有 OK 和 NG

```text
DetectionResult
  └─ AlgorithmVerdict::Ok / AlgorithmVerdict::Ng
```

- `AlgorithmVerdict` 只保留 `Ok` 和 `Ng`。
- `DetectionResult` 不再包含 `status`。
- 默认判定为 `Ng`；只有完整通过当前模式全部检测条件时才明确改为 `Ok`。
- 无定位、ROI 无效、OCR 空文本、二维码不可读和正常解码超时都属于可执行后的产品失败，返回 NG。
- ROI 无效不改变 `label_runtimeStatus` 的运行状态，也不显示“识别区域超出原图范围”之类的模式专用警告。

### 2.2 故障不生成产品结果

```text
系统无法可信执行检测
  → 执行器抛出标准异常，或返回无效 DetectionCompletion
  → DetectionWorker::failureConsumer
  → RuntimeInvariantViolation
  → Runtime Fault，停止接收新产品
```

故障不伪装成 NG，不进入普通统计、存图、PLC 或结果呈现事务；故障产品保留在现有未确认产品对账中。

### 2.3 取消只属于生命周期

`FrameQueueSubmitResult::Cancelled`、`DetectionWorkSubmissionResult::Cancelled` 和 Worker 取消计数继续保留，它们只描述停止、队列关闭和未执行工作：

- 不属于 `DetectionResult`；
- 不生成正式 `DetectionCompletion`；
- 不进入 `ResultService`。

## 3. NG 与 Fault 边界

### 3.1 判单件 NG

| 模式 | `AlgorithmVerdict::Ng` 情况 |
|---|---|
| 钢印 | 无定位、日期 ROI 无效或越界、字符不符、现有钢印算法判 NG |
| OCR | 无定位、日期 ROI 无效或越界、识别为空或与目标不一致 |
| 字库 | 无定位、日期 ROI 无效或越界、字符匹配失败 |
| 二维码+三期 | 无定位、二维码/日期 ROI 无效、二维码不可读、正常解码超时、日期字符失败 |
| 纸巾 | 未找到纸卷/轮廓、粗糙度超限以及当前已有普通算法 NG |

每个 NG 必须完成一次总数/NG 统计、按设置存图、一次 NG PLC 请求或现有延迟 NG 入队，以及一次结果呈现。

二维码+三期的每个 NG 如果 `DetectionResult::diagnostic` 非空，必须把同一原因追加到 `presentationText`，保证日志与 `label_recognitionText` 显示一致。

### 3.2 进入系统 Fault

以下情况表示系统无法可信检测，不得返回产品 NG：

- 已受理 `FrameData` 或 `DetectionWorkItem` 无效；
- `DetectionPose::valid == true`，但定位给出的模板下标小于 `0` 或超出当前运行模板数组范围；
- 运行模板快照或内部结构不一致；
- OCR 引擎或二维码解码器在执行期不可用；
- 二维码解码器报告 `InternalError`；
- 图像预处理无法产生有效正式帧；
- Pipeline 或供应商引擎抛出异常；
- 执行器返回无效 `DetectionCompletion`。

运行启动前发现模板、定位资源、OCR 引擎或二维码解码器不可用时，继续使用现有启动拒绝，不创建正式产品；只有正式运行后的执行故障进入 Runtime Fault。

必须先判断 `pose.valid`：`pose.valid == false` 表示当前产品无定位，返回 NG；只有定位已经声明成功后模板下标仍非法，才表示运行快照结构损坏并进入 Fault。

## 4. 实施修改

### A2-1：精简公共结果合同

1. 删除 `DetectionStatus` 枚举和 `DetectionResult::status`。
2. 删除 `AlgorithmVerdict::NotEvaluated`，只保留 `Ok/Ng`。
3. 将 `DetectionResult::verdict` 默认值设为 `Ng`。
4. 删除 `saveNotEvaluatedAsNg` 在 `DetectionResult`、模式描述和 `ResultService` 中的全部字段与逻辑。
5. 保留 `DetectionCompletion::isValid()` 的帧、`ProductKey` 和图像有效性检查，不增加结果状态矩阵。

不新增结果工厂、失败原因枚举、自定义异常类型、结果 Manager 或新状态机。

### A2-2：修正五个 Pipeline

1. 钢印：无定位和日期 ROI 无效返回 NG；无效工作项抛出标准异常；字符匹配和重叠算法不改。
2. OCR：无定位、日期 ROI 无效、识别为空或不一致返回 NG；无效工作项和引擎异常进入 Worker Fault。
3. 字库：无定位、日期 ROI 无效和字符不匹配返回 NG；无效工作项进入 Worker Fault。
4. 二维码+三期：无定位、ROI 无效、不可读、正常超时和日期失败返回 NG；解码器不可用或 `InternalError` 抛出标准异常。
5. 纸巾：只删除 `status` 赋值，现有 OK/NG 算法和诊断保持不变。
6. 人可读原因继续写入现有 `diagnostic`，业务逻辑不解析诊断字符串。

OCR 边界固定为：`IOcrEngine::recognize()` 正常返回时，即使返回空集合或清洗后文本为空，也必须判为 NG；只有该调用抛出异常时才进入 Fault。不得根据空识别结果推断 OCR 引擎失效，也不扩展 OCR 接口。

二维码+三期日期子检测正常返回时必然具有 `Ok/Ng`，异常则直接退出 Pipeline，因此删除整层无实际延迟执行作用的中间包装：

- 删除 `BarcodeWordDateDetectionResult`；
- 删除 `DateDetectionFunction`；
- 删除接收 `bool barcodeIsReadable` 和日期回调的 `BarcodeWordDetectionPipeline::detect()` 重载；
- 删除 `BarcodeWordDetectionResult::dateResultProduced`；
- 删除“日期检测未产生有效结果”的普通 NG 分支；
- 日期字库 Pipeline 正常返回后，直接由其 `verdict` 设置 `dateDetectionExecuted`、`dateIsOk` 和组合 `isOk`；日期字库 Pipeline 抛异常时不构造组合结果。

二维码文本、日期识别文本、字符匹配结果、Overlay 和 `diagnostic` 均继续来自现有真实检测输出，不通过新的中间结构复制或重新判定。

### A2-3：修正 Registry、结果文本与故障出口

1. 字库和二维码+三期先处理 `!pose.valid` 为普通 NG；仅在 `pose.valid == true` 后检查模板下标，非法时抛出 `std::logic_error`，不构造产品结果。
2. 二维码+三期以最终 `DetectionResult::diagnostic` 作为唯一 NG 原因；删除重复的 `BarcodeWordDetectionWorkOutput::reason`。
3. 最终 `verdict == Ng` 且 `diagnostic` 非空时，Registry 无条件向 `presentationText` 追加 `原因：<diagnostic>`；不再依赖 `barcodeIsReadable/dateDetectionExecuted` 决定是否显示原因。
4. 执行期引擎/解码器不可用或内部错误抛出 `std::runtime_error`，不新增自定义异常体系。
5. 预处理失败继续返回无效 `DetectionCompletion`，由 Worker 现有检查进入 Fault。
6. 启动前资源校验、Pipeline 异常捕获、`failureConsumer` 和 Runtime Fault 链保持现有实现。
7. 删除 `DetectionModeDescriptor`、`DetectionResult`、`ResultService`、`InspectionRuntime` 和 `InspectionPage` 中仅服务于 ROI 无效提示的字段、信号、方法与状态记忆；不修改 `label_runtimeStatus` 控件结构及运行中、停止、Fault 和存图失败等其他显示。
8. `DetectionWorker`、`InspectionRuntime` 的队列、Worker 生命周期和 Fault 主链只读核对；除删除 ROI 专用警告出口外，没有证据证明现有链路缺失时不修改。

### A2-4：简化 ResultService

1. 删除 `Cancelled` 特殊分支和静默 `claimResult()`。
2. 删除 `SystemFault` 日志分支、status/verdict 组合门禁及相关状态字符串。
3. 合法的 `DetectionCompletion` 经 `acceptCompletion()` 验收后直接进入现有唯一 `process()`。
4. `process()` 只处理 `Ok/Ng`，保留一次 `claimResult()`、统计、存图、PLC 和 UI 发布顺序。
5. 删除 `NotEvaluated` 存图条件和 `saveNotEvaluatedAsNg` 传递。
6. 保留 `recordSystemFault()`；它属于 Runtime 异常统计和延迟 NG 清理，不是产品结果状态。

### A2-5：删除所有模式的 ROI 专用警告

所有模式统一执行以下规则：

- 无定位、日期/文字 ROI 无效、二维码 ROI 无效等可判定的产品失败返回 NG；
- 不发布 `roiWarningChanged`，不调用 `publishRoiWarning()`；
- 不在 `label_runtimeStatus` 显示 ROI 无效专用文本，也不把运行状态切换为 `warning`；
- `label_runtimeStatus` 继续显示既有“停止”“运行中”“Fault”等运行状态；
- 存图失败等其他既有警告保持原行为，不与 ROI 规则混用。

删除以下仅服务于 ROI 专用警告的旧链路：

- `DetectionModeDescriptor::showRoiWarningOnCancelled`；
- `DetectionResult::showRoiWarningOnCancelled`；
- `DetectionResult::clearRoiWarningOnCompleted`；
- `InspectionRuntime::roiWarningChanged` 与 `publishRoiWarning()`；
- `InspectionPage::showDetectionRoiWarning()`、`clearDetectionRoiWarning()` 及 `m_detectionRoiWarningActive`；
- `ResultService` 中根据 `Cancelled` 或完成结果发布、清除 ROI 警告的分支。

不新增替代的 ROI 警告字段、信号、状态机或弹窗；UI 布局和其他状态显示不变。

## 5. 文件边界

计划修改：

- `app/detection/common/detection_pose.h`
- `app/contracts/detection_mode.h/.cpp`
- `app/detection/detectionmode/stamp/stamp_detection_pipeline.cpp`
- `app/detection/detectionmode/ocr/ocr_detection_pipeline.cpp`
- `app/detection/detectionmode/word/word_detection_pipeline.cpp`
- `app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h/.cpp`
- `app/detection/detectionmode/tissue/tissue_detection_pipeline.cpp`
- `app/detection/detection_registry.cpp`
- `app/runtime/result_service.h/.cpp`
- `app/runtime/inspection_runtime.h/.cpp`
- `app/ui/main_window.cpp`
- `app/ui/pages/inspection_page.h/.cpp`
- 实施完成后的开发者指南、功能对照表、执行记录和计划索引。

只读核对：

- `app/runtime/detection_worker.cpp`
- `app/runtime/inspection_runtime.cpp`
- `app/runtime/frame_queue.cpp`
- OCR 和二维码供应商适配器。

明确不修改：字符算法、定位方法和阈值、钢印重叠算法、纸巾算法、OCR/二维码识别规则、模板 Schema、设置文件、UI 布局、PLC 地址和值、延迟 NG 业务含义。允许修改 UI/Runtime 的 ROI 专用警告代码通路，但不得新增任何 ROI 警告表现。

## 6. 验证门禁

### 6.1 静态门禁

生产代码中以下引用必须为零：

```text
DetectionStatus
AlgorithmVerdict::NotEvaluated
saveNotEvaluatedAsNg
showRoiWarningOnCancelled
clearRoiWarningOnCompleted
roiWarningChanged
publishRoiWarning
showDetectionRoiWarning
clearDetectionRoiWarning
m_detectionRoiWarningActive
BarcodeWordDetectionResult::dateResultProduced
BarcodeWordDateDetectionResult
DateDetectionFunction
BarcodeWordDetectionWorkOutput::reason
```

同时确认：

- 队列和 Worker 生命周期的 `Cancelled` 仍存在，且不构造 `DetectionResult`；
- 只有 `AlgorithmVerdict::Ok/Ng` 能进入 `ResultService::process()`；
- 所有模式的 ROI 无效用例均不发布 ROI 警告、不把 `label_runtimeStatus` 置为 `warning`；
- 二维码+三期不存在日期结果回调包装，日期子结果只读取一次现有 `verdict`；
- 不新增失败原因枚举、自定义异常体系、结果管理层、Fault 状态机或测试框架；
- 受保护算法、Schema、UI 和 PLC 文件无计划外差异；
- `git diff --check`、UTF-8 和文件末尾换行检查通过。

### 6.2 普通 NG

至少覆盖四个定位模式的无定位和 ROI 无效，以及 OCR 空文本、二维码不可读和正常超时。每件必须满足：

```text
一次 Ng
totalCount +1，ngCount +1
按现有设置提交 NG 存图
立即或延迟请求一次 NG PLC
发布一次 NG 结果
Runtime 保持 Running
不发布 ROI 警告，label_runtimeStatus 保持运行中状态
二维码+三期 diagnostic 非空时，presentationText 包含同一条“原因”
```

其中 OCR 空文本用例必须由 `IOcrEngine::recognize()` 正常返回空集合或清洗后空文本产生，并确认 Runtime 保持 Running；OCR 调用抛异常属于 6.3 的系统 Fault 用例。

### 6.3 系统 Fault

至少覆盖 `pose.valid == true` 后模板下标非法、执行期解码器不可用、解码器内部错误、预处理失败、Pipeline 异常和无效 completion。每件必须满足：

```text
不产生 DetectionResult
Runtime 进入 Fault
不增加普通 total/ng
不请求产品 OK/NG PLC
不按普通产品存图
故障产品进入未确认对账
```

### 6.4 生命周期与回归

- 主动停止和队列取消不生成正式 completion。
- `pose.valid == false` 的字库和二维码+三期产品返回 NG 并继续运行，不得进入 Fault。
- 正常 OK/NG 仍只有一个 `process()` 出口，不重复统计、存图或请求 PLC。
- 延迟 NG 顺序在无定位、ROI 无效等反例中不发生错位。
- 五种模式正常 OK/NG、Overlay、模板名和耗时保持原行为；所有模式 ROI 无效均不显示 ROI 专用警告，`label_runtimeStatus` 仍按既有运行状态显示。
- 仓库当前没有独立测试工程，A2 不为此搭建大型测试框架；优先执行聚焦静态检查和用户 Qt Creator/现场验证。

## 7. 实施顺序

1. 记录分支、HEAD、工作区和所有结果状态引用。
2. 删除公共结果状态、`NotEvaluated` 和相关兼容字段，使编译错误暴露全部调用点。
3. 修改五个 Pipeline，删除二维码日期回调包装，并完成 Registry 的 NG/Fault 分流和 NG 原因展示。
4. 简化 `ResultService`，删除 ResultService、Runtime 和 UI 的 ROI 专用警告出口。
5. 执行静态门禁和可运行的聚焦验证。
6. 用户在 Qt Creator 执行 qmake、Clean、Rebuild 和五模式软触发回归。
7. 用户使用真实硬触发和 PLC 核对 NG 脉冲、延迟剔除及 Fault 停线。

## 8. 完成定义

当前实施状态：A2-1～A2-5 已完成代码修改和静态门禁，Qt Creator 构建、软触发回归、硬触发及 PLC 现场验证待用户统一执行；在用户验证通过前，本计划仍保持“代码实施完成，待验证”。

A2 仅在以下条件全部满足后完成：

1. 产品结果模型只包含 `Ok/Ng`，生产代码不存在 `DetectionStatus` 和 `NotEvaluated`。
2. 无定位、ROI 无效、OCR/二维码不可读均形成一次明确 NG；二维码+三期非空 NG 原因进入 `presentationText`。
3. `pose.valid == false` 不触发 Fault；只有定位有效后模板下标非法，以及执行期引擎/解码器失效、运行结构损坏和非法帧进入现有 Worker Fault。
4. ResultService 不再接收或处理产品取消/系统故障状态，所有合法 completion 只进入一个 `process()`。
5. 真正停止和队列取消只保留为生命周期行为。
6. 每个合法产品只统计、存图、请求 PLC 和发布一次，延迟 NG 顺序正确。
7. Fault 不伪造产品 NG，故障产品进入未确认对账。
8. 未引入替代状态体系或第二套 Fault 机制，未修改计划外算法、Schema、UI 布局或 PLC 协议；所有模式均未新增 ROI 警告表现。
9. 二维码日期回调包装和重复 `reason` 字段已经删除，识别内容、Overlay 和最终判定保持原来源。
10. 静态门禁、用户 Qt Creator 构建及现场验证通过；未完成的现场项必须明确标记待验。
