# OCRGangYin 深度 OCR 启动前同步初始化精简方案

## 1. 状态与范围

- 状态：代码和文档已实施，静态检查通过，待用户通过 Qt Creator 构建并在主程序统一验证。
- 目标：点击启动识别后，OCR 模式先同步创建本次运行独立的 `DeepOcrEngine`，创建 DET/REC predictor 并完成一次预热，成功后再创建检测线程，随后才启动图像采集。
- 权威范围：OCR 引擎创建时机、单次运行所有权、检测线程职责、初始化失败返回、相关冗余代码清理和当前说明文档同步。
- 保持项：PP-OCRv6 tiny DET/REC、`DeepOcrEngine::recognize()`、OCR Pipeline、模板定位、文本清洗与判定、Overlay、统计、存图、PLC、软硬触发和运行期 Fault 行为不变。
- 约束：只做完成本目标所需的原位修改，不增加工厂、状态包装、回调框架、重试、回退、兼容重载、预测器池、预热配置或公开预热接口。
- 净结果：不新增生产文件、类、结构体、接口、成员状态或通用辅助函数；预热直接放在现有引擎构造流程中。

PP-OCRv6 tiny 的模型、算法、配置、依赖和部署继续服从 `PP-OCRv6_tiny_深度OCR重构实施方案.md`；本方案只规定 OCR 引擎启动生命周期。

字符模板编辑窗口按 `OCR公共引擎与字符辅助功能分层隔离实施方案.md` 持有独立的 `CharacterOcrEngine`。该实例不进入检测 Worker，不改变本方案规定的正式检测启动和释放顺序。

## 2. 当前实现

- `ApplicationStartup` 只向 `DetectionRegistry` 提供 `<exe>/config_ocr.txt` 路径。
- `InspectionApplicationService::start()` 先准备相机和 PLC，再调用 `InspectionRuntime::startDetection()`；只有该调用成功后才执行 `CameraSession::startInspection()`。
- `DetectionRegistry::create()` 负责按模式生成检测执行器，并仅在 OCR 分支同步创建本次运行的 `DeepOcrEngine`。
- `IOcrEngine` 公共构造过程创建 DET/REC predictor 后，以固定三通道图分别执行一次 DET 和 REC 预热，预热结果不进入业务 Pipeline。
- `DetectionWorker` 是所有模式共用的单线程执行器，队列容量、停止、运行期异常和结果回调已经稳定。
- 每次启动识别都会创建新的 `DetectionWorker`，停止后等待线程退出并释放 Worker。

本次不改变相机准备和 PLC 参数应用的既有顺序。这里的“启动前”是指 `startDetection()` 内部先完成 OCR 引擎创建，再创建检测 Worker；实际图像采集仍在 `startDetection()` 成功返回后开始。

## 3. 最终启动流程

```text
点击启动识别
  → 校验设置、模式和模板
  → beginStart()
  → 准备相机，但不开始采集
  → 应用 PLC 启动参数
  → InspectionRuntime::startDetection()
      → DetectionRegistry::create()
          → 非 OCR 模式：直接创建原模式执行器
          → OCR 模式：同步创建本次运行的 DeepOcrEngine
              → 读取 config_ocr.txt
              → 创建 DET predictor
              → 创建 REC predictor
              → 使用固定三通道图执行一次 DET 预热
              → 使用固定四点框执行一次 REC 预热
              → 创建成功后把引擎捕获到 OCR 检测执行器
      → 创建并启动 DetectionWorker
  → CameraSession::startInspection()
  → commitStart()
  → 检测线程逐帧串行复用本次运行的 OCR 引擎
```

OCR 引擎的固定生命周期为：

```text
每次 OCR 启动创建并预热一个新实例
  → 仅交给本次运行的唯一检测 Worker 使用
  → 停止时先等待检测线程退出
  → Worker 及其执行器释放时释放 OCR 引擎和 DET/REC predictor
```

同一个正式检测 OCR 引擎不跨检测运行复用，不被多个线程并发调用。暂停后再次启动必须重新创建完整的 DET/REC predictor。字符模板编辑窗口实例只归对应 Dialog 所有。

## 4. 最终实现

### 4.1 DetectionRegistry

- `DetectionPipelineCreationResult` 只保留 `executor`、`errorMessage` 和 `startFailureMessage`。
- OCR 分支在验证模板和定位配置后，同步创建：

```cpp
const std::shared_ptr<DeepOcrEngine> ocrEngine(
    new DeepOcrEngine(m_ocrConfigPath));
```

- OCR 检测执行器按值捕获 `ocrEngine`，每帧直接把 `*ocrEngine` 传给现有 `OcrDetectionPipeline::detect()`。
- 非 OCR 分支不创建、不持有也不检查 OCR 引擎。
- 不在 `DetectionPipelineCreationResult`、`DetectionRegistry` 或 `InspectionRuntime` 增加 OCR 引擎字段；正式检测引擎所有权只存在于 OCR 执行器捕获值中。

初始化失败处理：

- 正式检测初始化只在 `DetectionRegistry` 的 OCR 引擎构造位置捕获 `std::exception`。
- 使用现有 `logDetection` 记录一次 `event=detection.prepare_failed mode=ocr` 和原始异常诊断。
- predictor 创建或预热失败时，`creation.errorMessage` 固定为“深度 OCR 初始化失败，请检查配置、模型和运行库。”，不创建检测 Worker。
- `InspectionRuntime::startDetection()` 沿现有失败返回把错误交给 `InspectionApplicationService`；现有流程停止检测准备、恢复相机预览并回滚到 Idle。
- 不重试、不创建空引擎、不回退旧模型，也不把初始化失败送入运行期 Fault。
- 不增加 `catch (...)`、引擎空指针检查、重复配置检查或析构兜底；构造成功即得到有效引擎，构造失败即走唯一的启动失败返回。

### 4.2 公共 OCR 引擎构造

- 构造函数在 DET/REC predictor 创建完成后，创建一张固定的 `48×320` BGR 图像。
- DET 使用该图像执行一次完整推理；REC 使用覆盖该图像的一个有效四点框执行一次完整推理。
- 两次预热结果直接丢弃，不进入 OCR Pipeline，不生成产品结果、Overlay、统计、存图或 PLC 动作。
- `ocr.engine_loaded` 只在两次预热均成功后记录；预热抛出的异常继续由 `DetectionRegistry` 的现有启动失败处理接收。
- 本生命周期方案不增加配置项、预热接口、成员状态或辅助封装；字符辅助业务接口由字符分割方案单独规定。

### 4.3 DetectionWorker

`DetectionWorker` 恢复为纯检测执行线程：

```cpp
DetectionWorker(
    std::size_t queueCapacity,
    const Executor &executor,
    const CompletionConsumer &completionConsumer,
    const FailureConsumer &failureConsumer);
```

- `start()` 只负责打开队列和创建检测线程。
- `run()` 直接进入现有取帧、执行、完成和运行期异常流程。
- DET/REC 推理期间抛出的异常仍通过 `FailureConsumer` 进入现有 Runtime Fault。
- 不增加新的启动状态、同步原语或 OCR 专用分支。
- 最终只保留一个四参数构造函数和一个无参数 `start()`；不保留其他构造重载、默认转发或兼容入口。

### 4.4 InspectionRuntime

- 继续同步调用 `DetectionRegistry::create()`。
- `create()` 成功后才构造 `DetectionWorker`。
- Worker 构造只传队列容量、执行器、完成回调和故障回调。
- Worker 启动失败继续使用现有 `startFailureMessage` 返回。
- `m_detectionWorkerActive`、结果服务配置、停止和等待顺序保持不变。
- `InspectionRuntime` 直接使用 `creation.executor`，不增加 OCR 专用方法、包装 lambda、适配对象或生命周期成员。

### 4.5 ApplicationStartup

- 继续只计算 `<exe>/config_ocr.txt` 路径并传给 `DetectionRegistry`。
- 不创建、不持有 `IOcrEngine`、`DeepOcrEngine` 或 `CharacterOcrEngine`。
- 不包含任何 OCR 引擎头文件，只向 `DetectionRegistry` 传递配置路径。

## 5. 文件范围

生产代码只修改：

- `app/detection/detection_registry.h`
- `app/detection/detection_registry.cpp`
- `app/runtime/detection_worker.h`
- `app/runtime/detection_worker.cpp`
- `app/runtime/inspection_runtime.cpp`
- `app/startup/application_startup.cpp`
- `app/engines/ocr/ocr_engine.cpp`
- `app/detection/detectionmode/ocr/deep_ocr_engine.h`

当前说明同步：

- `app/engines/README.md`
- `docs/development/PP-OCRv6_tiny_深度OCR重构实施方案.md`
- `docs/development/OCRGangYin现有功能对照表.md`
- `docs/development/OCRGangYin计划索引.md`
- 本方案状态

保持：

- 正式检测继续只调用 `DeepOcrEngine::recognize()`；字符模板入口不进入 Detection Worker；
- Paddle DET/REC 模型、配置和后处理合同；
- OCR Pipeline、清洗、换行比较和 OK/NG 规则；
- 模板、Schema、Overlay、UI、相机、PLC、统计、存图和日志基础设施；
- qmake、依赖、模型、配置和部署资源。

## 6. 静态门禁

- 正式检测使用的 `DeepOcrEngine` 只在 `DetectionRegistry::create()` 的 OCR 分支构造；字符模板编辑窗口只构造其窗口独立的 `CharacterOcrEngine`。
- `IOcrEngine` 公共构造完成日志位于 DET/REC 预热之后，预热只调用现有 `DBDetector::Run()` 和正式 `CRNNRecognizer::Run()`。
- `DetectionPipelineCreationResult` 的数据成员严格只有 `executor`、`errorMessage` 和 `startFailureMessage`。
- `DetectionWorker` 构造只有四个业务参数。
- `DetectionWorker::start()` 只有无参数版本。
- `DetectionWorker::run()` 的首个业务动作是等待并取得帧。
- `ApplicationStartup` 不包含 OCR 引擎头文件，只传递配置路径。
- 非 OCR 模式的执行器创建路径不引用 `DeepOcrEngine`。
- `DetectionRegistry`、`InspectionRuntime` 和 `ApplicationStartup` 不增加 OCR 生命周期成员或转发函数。
- 正式检测引擎构造失败处理只有 `DetectionRegistry` 构造位置的一处 `std::exception` 捕获；字符模板窗口初始化失败只记录模板日志并保留手工流程。
- 生命周期相关文件不新增文件级类型或通用辅助函数；预热实现不新增接口、成员或配置。
- `git diff --check` 通过。
- 本生命周期方案的生产修改严格属于第 5 节范围；字符模板辅助功能使用其独立计划规定的文件范围。

## 7. 构建与统一验证

全部修改完成后统一执行，不设置独立 OCR 验收程序：

1. 使用当前 Qt Creator Kit 执行 Run qmake 和 Release Rebuild。
2. 启动主程序并选择 OCR 模式，确认点击启动后完成 DET/REC 预热，只记录一次 `ocr.engine_loaded`，随后才出现 `run.started` 和首帧结果。
3. 完成一次“启动 → 识别 → 停止 → 再次启动 → 识别”，确认每次运行各创建一次新引擎，第二次运行不出现 `runtime_invariant_violation`。
4. 确认每帧不再出现 `ocr.engine_loaded`，DET/REC predictor 在同一次运行内持续复用。
5. 令 OCR 配置或模型暂时不可用后通过主程序点击启动，确认启动失败、无检测线程、无采集帧并回到 Idle；恢复资源后正常启动。
6. 启动一个非 OCR 模式，确认没有 `ocr.engine_loaded`，原检测流程正常。
7. 核对 OCR 框、文字、最终组合文本、OK/NG、统计、存图、软硬触发和运行期 Fault 行为不变。
8. 核对首帧耗时不再包含 DET/REC predictor 的首次 `Run()` 冷启动开销。

引擎构造和 DET/REC 预热都发生在首帧受理之前，因此 predictor 创建和首次 `Predictor::Run()` 的冷启动时间不计入单帧处理耗时。固定预热尺寸与真实 ROI 尺寸不同时，Paddle 的形状相关准备仍可能使首帧存在少量额外波动。

## 8. 完成条件

- OCR 模式在检测 Worker 创建前完成本次运行独立引擎的同步创建和 DET/REC 预热。
- predictor 创建或预热失败通过现有启动失败链返回，不开始图像采集；逐帧推理异常仍进入 Runtime Fault。
- 同一次运行只创建一次引擎，每帧复用；停止后释放，再次启动创建新实例。
- 非 OCR 模式不创建 OCR 引擎。
- 生产代码整体为净删除，不存在新抽象、重复状态或转发层。
- Release 构建和第 7 节主程序统一验证通过。
