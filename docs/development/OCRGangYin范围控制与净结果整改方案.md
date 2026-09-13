# OCRGangYin 范围控制与净结果整改方案

## 1. 文档状态

- 方案日期：2026-09-10。
- 当前状态：范围内代码与现行文档已实施，轻量静态门禁已执行，待用户统一构建、交互和现场验证。
- 编写基线：分支 `codex/ocrgangyin-refactor`，HEAD `abb424e`。
- 工作区基线：`docs/development/OCRGangYin计划索引.md` 存在用户已有未提交修改，本方案不得覆盖、回退、暂存或混入该差异。
- 事实优先级：当前行为以 `app/`、`tools/` 中的当前代码和真实调用链为准；本方案中用户已经明确指定的内容作为整改目标。计划索引不得在代码尚未达到目标时把目标状态写成“代码已实施”。
- 权威范围：只处理本方案列出的旧实现残留、零调用接口、重复数据合同、操作员提示文字、模板化注释和现行文档失真。
- 替代关系：本方案不替代五种检测模式、模板终局、软硬触发、PLC、Fault、存图、UI 布局、相机初始化等现有功能方案；操作员界面只修改第 9 节确认的文字及诊断信息显示边界。
- 交叉计划：运行时接口和相机合同整改必须保护《OCRGangYin应用运行时界面边界精简方案》的 Runtime 数据 signal、容量一 PresentationMailbox、ResultService 内部所有权和 UI 直连结果链。
- 实施授权：用户已于 2026-09-11 明确授权完整实施；实施前已重新记录分支、HEAD、工作区和暂存区。
- 项目限制：Agent 只执行轻量静态检查，不运行 qmake、编译、链接、测试程序、主程序、DLL、相机、PLC 或现场设备。

## 2. 整改目标

整改完成后，仓库必须满足：

1. 当前代码与当前计划索引、Schema、模块 README 和开发者指南表达同一最终状态。
2. 设置 Schema 不匹配时直接用当前默认设置覆盖原文件并继续启动，不提示、不确认、不迁移、不备份，也不读取旧 Schema 的其他字段。
3. 公开接口只保留真实调用者或当前已确认合同需要的能力，不保留为了调试、历史阶段或潜在未来调用而存在的 getter、转发和包装。
4. 相机操作结果只保留一套纯数据合同，不在 Runtime 和 Application 各维护一份同构类型及字段复制代码。
5. 参数页面保留现有集中绑定、字符串键和运行时控件类型分发，不因本方案改写已稳定运行的设置流程。
6. 代码注释只解释当前仍然存在且不易从代码本身得出的约束、所有权、线程、ABI、算法或兼容事实。
7. 面向操作员的提示只表达当前结果、影响和可执行动作，不直接显示内部路径、错误码、诊断字符串或实现术语；在错误产生处或结构化错误仍完整的边界先记录该路径实际可取得的技术信息，再生成操作员文字。
8. 界面文字按第 9.2—9.3 节统一，不扩大为全量改写、翻译重做或界面重构。
9. 不新增 Manager、Facade、Repository、Coordinator、通用绑定框架、文案框架、兼容层、迁移层、弃用别名或空壳包装。
10. 除本方案明确规定的行为修复和提示文字外，五种检测算法、模板流程、PLC 时序、软硬触发、统计、存图、Fault 和 UI 交互保持不变。

## 3. 当前事实基线

### 3.1 设置文件已具备旧 Schema 静默覆盖路径

当前 `app/startup/application_startup.cpp` 在 `AppSettingsStore::load()` 返回 `SETTINGS_RESET_REQUIRED` 后，直接执行：

```text
AppSettings::defaults()
→ AppSettingsStore::save()
→ 继续启动
```

该代码路径与最终行为一致：旧文件只用于读取 `schemaVersion`，随后由当前默认设置直接覆盖，不迁移旧字段，也不要求用户确认。以下现行说明与实际行为不一致，实施时必须同步修正：

- `OCRGangYin新架构数据Schema.md`：`schemaVersion != 8` 时仍描述为用户确认后替换；
- `app/startup/README.md`：旧版设置仍描述为启动层触发重置确认。

### 3.2 模板化注释进入生产代码

排除 PaddleOCR 上游源码 `app/engines/ocr/vendor/paddle/**`、Snap7 上游源码 `app/devices/plc/vendor/snap7.h`、`app/devices/plc/vendor/snap7.cpp`，以及本方案不处理的 `tools/barcode_decoder/**`、`app/engines/barcode/**` 和 `app/contracts/barcode_parameter_defaults.h` 后，当前 135 个纳入范围的第一方 `.h/.cpp` 文件中：

- 86 个文件包含“文件作用：本文件用于”；
- 88 个文件包含“主要职责”；
- 90 个文件包含“模块位置”；
- 87 个文件包含“协作说明”；
- 46 个文件共有 355 条“函数说明”；
- 48 个文件共有 130 条“组件说明”。

项目适配器 `app/engines/ocr/vendor/paddle_ocr_engine.h/.cpp` 和 `app/devices/plc/vendor/snap7_plc_device.h/.cpp` 属于第一方代码，纳入清理。当前大量注释只复述文件名、类型名或函数名；`MainWindow` 析构注释还描述了实际不存在的“删除临时文件”行为。

### 3.3 零调用接口与逐层转发

当前全仓符号检索确认以下接口只有声明和定义，没有真实调用者：

```text
InspectionRuntime::acceptedProductCount
InspectionRuntime::completedProductCount
InspectionRuntime::abnormalStatistics
InspectionRuntime::totalCount
InspectionRuntime::ngCount
InspectionRuntime::pendingDelayedNgCount
InspectionRuntime::resetAbnormalStatistics
ImageSaveService::workerCount
DetectionWorker::cancelledFrameCount
TissueDetectionPipeline::roughnessThreshold
TissueRollDetector::roughnessThreshold
TemplateEditorPage::activePreparedTemplate
TemplateApplicationService::activePreparedTemplate
InspectionCameraRecoveryResult::isRecovered
```

其中 `InspectionRuntime` 的部分方法只把调用继续转给 `ResultService`，没有形成新的业务语义。

### 3.4 相机结果合同重复

`app/runtime/camera_session.h` 与 `app/application/camera_application_contract.h` 分别维护相机打开、参数设置和恢复结果。两套枚举和结构字段基本一一对应，`inspection_application_service.cpp` 再通过多个转换函数逐字段复制。

Application 仍应负责打开相机、停止、恢复和 PLC 组合用例；需要删除的是重复数据表示和机械转换，不是应用用例边界。

### 3.5 参数页现有集中绑定保持

`MachineSettingsPage` 使用约二十个字符串键、`QMap<QString, GlobalSettingBinding>`、通用 `QWidget *` 和 `qobject_cast` 集中管理固定 Designer 控件。相同键集合在以下路径中使用：

```text
registerGlobalSetting
isDirtyByValue
copyUiValuesTo
settingValueText
restoreAppliedValues
MainWindow 各设置应用/失败回退调用点
```

该结构当前没有已知运行故障，并以较少代码集中保持设置登记、dirty、保存、失败恢复和权限控制。本方案保持该实现，不再为类型化改造增加重复分支。

### 3.6 现行文档未同步

- 根 `README.md` 仍把已经删除的 `app/recipes/`、`ProductRecipe`、`PreparedRecipe`、`RecipeStore` 和 `RecipeEditorSession` 描述为当前架构。
- `app/application/README.md` 仍列出不存在的 `inspection_ui_contract.h`。
- `app/contracts/README.md` 未登记现有 `inspection_presentation.h`，`app/runtime/README.md` 又把它写成 Runtime 目录内文件。
- 部分模块说明和注释描述的是历史阶段而非当前实现。

### 3.7 操作员提示混入实现术语和诊断信息

当前提示文字存在以下几类问题：

- `TemplateApplicationService::storeErrorMessage()` 把用户消息、模板路径和内部诊断拼接为同一字符串，模板页面随后将其直接显示在弹窗中；
- 运行故障警告只显示固定原因、接收数和完成数，不显示 `InspectionFaultSnapshot::diagnostic`；
- 启动失败和模板预检失败路径会把错误码或诊断字符串追加到主要提示；
- 部分固定文字使用 Runtime、Snapshot、ROI、DLL、资源初始化和队列等实现术语；
- 设置读写提示还直接使用“对象”“字段”“设备合同”“规范化绝对路径”和“原子提交”等存储实现术语；
- 模板保存提示还直接使用“临时目录”“符号链接”“提交”“回滚”“加载目标”和“写入目标”等文件事务术语；
- 检测启停和故障提示还直接使用“采集线程”“正式采集”等内部流程术语；
- “刚印/钢印”、“触发模式/硬触发模式”等同一含义存在不一致表达；
- “清空当前软件数据”“所有设置已经完成”等文字没有准确说明实际影响范围。

独立的二维码结果接收工具也存在同类问题：`CSV 原子替换失败`、`首次使用必须选择绝对输出目录`、`坏 JSON` 和 `不返回 ACK` 会直接出现在其窗口或弹窗中。该工具仍保留 JSONL、CSV、监听端口等完成维护任务所必需的格式和网络名称，但失败提示改为结果、影响和处理动作。

这些文字不改变检测结果，但会让操作员看到实现过程而不是当前结果和下一步动作。

## 4. 整改范围

### 4.1 纳入范围

1. 旧 Schema 静默默认覆盖行为的现行说明同步。
2. 已证明零调用的公开/内部接口及其纯转发实现。
3. Runtime 与 Application 的重复相机结果数据类型和转换函数。
4. 第一方 C++ 文件中的模板化、复述性和失真注释。
5. 第 9 节确认的主程序与二维码结果接收工具提示文字、固定标签和诊断信息显示边界。
6. 根 README、相关模块 README、Schema、开发者指南和计划索引的当前状态同步。
7. 因删除、移动或改名直接影响的 qmake 条目和 include。

### 4.2 明确不纳入范围

- 五种检测算法、阈值、定位、OCR、字符匹配和钢印判定；二维码解码算法、`tools/barcode_decoder/**`、`app/engines/barcode/**`、`app/contracts/barcode_parameter_defaults.h`、模板中的解码参数校验、引擎构造与接入、DLL、格式、后端、选项、预算和版本全部保持现状，后续另案处理；第 9 节只允许修改现有二维码相关路径中的操作员文字和诊断显示边界，不得改变任何解码调用或判定；
- 模板制作、模板文件夹结构、模板 Schema 版本和模板事务策略；
- 软硬触发、180 ms 软件节拍、Line0、容量一帧队列和队列满处理；保持当前代码的软件触发阻塞提交、硬触发非阻塞提交，以及硬触发队列满后进入 `HardTriggerQueueOverflow` Fault 的行为；
- PLC 地址、输出值、脉冲、延迟 NG、复位和 Fault 时序；
- PresentationMailbox、产品结果生命周期、Overlay、统计和存图；第 9 节以外的 UI 文字与呈现保持不变；
- 海康相机 SDK 初始化、采集节点、GigE 参数和触发时序；
- PaddleOCR、Snap7 等供应商源码；
- `experiments/industrial_char_segmenter/` 的独立实验架构；
- `tools/result_receiver/` 的接收协议、JSONL/CSV 文件格式、端口、存储和部署行为；第 9 节只修改其中直接显示给使用者的固定文字；
- 新功能、新设置项、新页面、新弹窗、新视觉、新兼容读取、新数据迁移、全量翻译重做或未来扩展接口。
- 参数页集中绑定、字符串键、通用控件指针和运行时控件类型分发的结构性改写。

## 5. 第一阶段：统一旧 Schema 静默覆盖说明

### 5.1 最终行为

当设置文件不存在时：

- 继续使用 `AppSettings::defaults()` 的现有首次启动行为；
- 不显示设置版本错误；
- 不提前创建额外文件。

当设置文件 `schemaVersion != 8` 时：

1. 保持 `AppSettingsStore::load()` 返回 `AppSettingsLoadStatus::ResetRequired` 和 `SETTINGS_RESET_REQUIRED`，只把它们作为启动层执行默认覆盖的内部控制结果。
2. `ApplicationStartup` 立即取得 `AppSettings::defaults()`，通过现有 `AppSettingsStore::save()` 和 `QSaveFile` 原子覆盖同一路径的设置文件。
3. 覆盖成功后直接继续启动，不显示提示或确认，不等待用户选择，也不记录旧版本专项提醒。
4. 不读取或转换旧 Schema 的其他字段，不迁移、不备份、不另存、不保留旧文件副本，也不新增任何 UI、服务或兼容解析。
5. 覆盖失败时保持现有设置写入失败并退出的处理，不以未落盘的默认设置继续启动。

当文件已经是 Schema 8 但内容损坏时：

- 保持当前拒绝启动行为；
- 复用同一个标题为“设置错误”的现有设置加载失败弹窗，不新增其他弹窗；
- 不覆盖文件，不把损坏归类为版本不兼容。

### 5.2 实现边界

- 当前 Store 和 `ApplicationStartup` 的静默默认覆盖代码保持不变，本阶段只同步与其冲突的现行说明；
- 不恢复软件设置页的“恢复默认设置”按钮；
- 不新增提示或确认弹窗、Reset Service、迁移器、备份管理器或旧 Schema 解析；
- 不修改 `AppSettings::CurrentSchemaVersion`、默认值、JSON 字段或 `QSaveFile` 保存边界；
- 不增加旧字段读取、转换、默认回填、别名或并行覆盖入口。

### 5.3 预计文件

```text
app/startup/README.md
docs/development/OCRGangYin新架构数据Schema.md
```

## 6. 第二阶段：删除零调用接口

### 6.1 删除规则

- 实施前再次执行全仓声明、定义和调用检索；
- 只有“除声明与定义外零调用”的符号才进入删除；
- 删除声明、定义、只因该接口存在的注释和失去用途的 include；
- 保留支撑内部状态机、日志、Fault 快照、统计事务或当前计划固定合同的真实方法；
- 不把已删除 getter 改为新名字，不新增诊断对象或统一查询 Facade。

### 6.2 当前已确认删除候选

```text
InspectionRuntime::acceptedProductCount
InspectionRuntime::completedProductCount
InspectionRuntime::abnormalStatistics
InspectionRuntime::totalCount
InspectionRuntime::ngCount
InspectionRuntime::pendingDelayedNgCount
InspectionRuntime::resetAbnormalStatistics
ImageSaveService::workerCount
DetectionWorker::cancelledFrameCount
TissueDetectionPipeline::roughnessThreshold
TissueRollDetector::roughnessThreshold
TemplateEditorPage::activePreparedTemplate
TemplateApplicationService::activePreparedTemplate
InspectionCameraRecoveryResult::isRecovered
```

`InspectionRuntime` 必须继续保留现行调用链真实使用的：

```text
statistics
requiresPlcForRun
resetStatistics
resetNgCount
clearPendingDelayedNgRequests
状态、Fault、运行和帧提交相关接口
```

`ResultService::abnormalStatistics`、计数 getter 或重置函数如仅被上述待删 Runtime 包装调用，则同步删除；如实施基线出现新的真实内部调用，则保留真实最内层能力，但不得恢复无消费者的 Runtime 转发。

### 6.3 预计文件

```text
app/runtime/inspection_runtime.h
app/runtime/inspection_runtime.cpp
app/runtime/result_service.h
app/runtime/result_service.cpp
app/runtime/image_save_service.h
app/runtime/image_save_service.cpp
app/runtime/detection_worker.h
app/runtime/detection_worker.cpp
app/detection/detectionmode/tissue/tissue_detection_pipeline.h
app/detection/detectionmode/tissue/tissue_detection_pipeline.cpp
app/detection/detectionmode/tissue/tissue_roll_detector.h
app/detection/detectionmode/tissue/tissue_roll_detector.cpp
app/application/template_application_service.h
app/application/template_application_service.cpp
app/ui/main_window/template/template_editor_page.h
app/ui/main_window/template/template_editor_page.cpp
app/runtime/camera_session.h
```

## 7. 第三阶段：合并相机结果合同

### 7.1 最终结构

相机打开、参数设置和恢复结果只保留一套轻量纯数据类型，由 CameraSession、Application 和 UI 共同使用。固定处理为：

1. 新建唯一共享文件 `app/contracts/camera_operation_result.h`，原样承接现有 Application/UI 已使用的 `CameraOpenIssueDto`、`CameraParameterResultDto`、`CameraOpenResultDto`、`CameraRecoveryIssueDto` 和 `CameraRecoveryResultDto`；不让该文件 include 相机 SDK、Runtime 实现、Detection、PLC 或 QWidget。
2. `CameraSession` 直接返回这套唯一合同，删除 `InspectionCameraOpenIssue`、`InspectionCameraParameterResult`、`InspectionCameraOpenResult`、`InspectionCameraRecoveryIssue` 和 `InspectionCameraRecoveryResult`，不保留别名或转发类型。
3. `InspectionApplicationService` 保留 `OpenCameraResult` 用例级组合结果；停止接口直接返回 `CameraRecoveryResultDto`，其相机字段使用唯一合同。
4. 删除 `cameraParameterDto()`、`cameraOpenIssueDto()`、`cameraOpenDto()`、`cameraRecoveryIssueDto()`、`cameraRecoveryDto()` 等机械复制函数。
5. `InspectionAcquisitionDto` 只表示一次检测启动采用软件触发还是硬件触发，不属于相机操作结果；将它原样移入现有 `inspection_application_service.h`，保持枚举值、结果字段和 MainWindow 显示逻辑不变。
6. `CameraSession` 同步停止采集，仅保留恢复预览所需的局部状态，不为停止结果增加跨层类型。
7. 删除 `app/application/camera_application_contract.h` 及工程条目、include，不保留转发头。
8. 保留 Application 的用户错误、PLC 组合结果和 RuntimeSnapshot；不把整个 CameraSession 暴露给 UI。
9. `CameraParameterResultDto` 继续保留当前范围、实际值、原生错误码和 `diagnostic`；`CameraRecoveryResultDto` 继续只保留当前已有的 `errorMessage`，不得为日志补加原生错误码或 `diagnostic`。曝光和增益失败复用 MainWindow 现有的原生错误码与 `diagnostic` 日志，不重复记录；停止后恢复曝光失败在 MainWindow 补记现有 `errorMessage`。随后显示第 9 节对应固定提示；不新增 `userMessage` 字段、相机错误文案框架或第二套映射类型。

### 7.2 保持项

- 相机打开、曝光范围、实际曝光、增益和恢复行为不变；相机提示文字只按第 9 节修改；
- 相机准备、正式采集、模板预览、停止和恢复时序不变；
- MainWindow 继续只通过 Application 发起相机命令；
- 相机曝光、增益失败时不再直接显示 `CameraParameterResultDto::diagnostic`，停止后恢复曝光失败时不再直接显示 `CameraRecoveryResultDto::errorMessage`；
- Runtime 的产品结果/Fault 数据 signal 直连 UI 的既有架构不变；
- 不新增 CameraMapper、Adapter、Facade 或第二套错误转换。

### 7.3 预计文件

```text
app/application/camera_application_contract.h（删除）
app/contracts/camera_operation_result.h（新增唯一合同）
app/runtime/camera_session.h
app/runtime/camera_session.cpp
app/application/inspection_application_service.h
app/application/inspection_application_service.cpp
app/ui/main_window/main_window_inspection.cpp
app/ui/main_window/main_window_settings.cpp
app/AutoOCRproject.pro
```

不新增第二个并行合同文件；旧路径必须零引用且不保留转发头。`InspectionAcquisitionDto` 只移动定义位置，不改名、不复制。

## 8. 第四阶段：保持参数页现有实现

### 8.1 最终决策

1. 保留 `SettingsEditState` 对整机设置与模板设置的未应用汇总和用户提示。
2. 保留 `MachineSettingsPage` 的字符串键、`GlobalSettingBinding`、通用控件指针和现有控件类型分发。
3. MainWindow 继续按现有字符串键或明确的字符串键组调用设置页。
4. 不新增 `MachineSettingField`、字段枚举、第二套并行入口或逐字段显式分发。
5. 只允许第 9 节操作员提示和第 10 节注释清理直接影响该区域代码。

### 8.2 必须保持的行为

- 需要“应用/设置/连接”后生效的字段继续显示 `*`；
- 即时保存字段继续按当前时点保存；
- 非法输入不保存，失败后恢复已应用值；
- 相机、PLC、图像设置的运行中可用性和硬件依赖不变；
- 同一标签对应多个字段时，只要其中一个未应用，标签仍显示 `*`；
- 启动前未应用参数提示内容和放弃逻辑不变；
- 当前设置日志仍记录实际字段和值。

### 8.3 文件边界

```text
app/ui/main_window/settings/machine_settings_page.cpp
```

该文件只实施操作员提示与日志边界修改；参数绑定结构保持。其他设置相关文件只允许进行本方案统一的注释清理或已有提示文字修改。不修改 `.ui` 控件结构、对象名、QSS、设置 Schema、默认值或应用服务接口。

## 9. 第五阶段：收口操作员提示文字

### 9.1 显示边界

操作员界面固定只显示：

1. 当前发生的结果；
2. 对检测、设备或数据的直接影响；
3. 操作员能够执行的处理动作。

本阶段统一采用“先记录技术错误，后生成操作员文字”的顺序。日志必须写在错误产生处，或写在该路径实际存在的 `code`、`path`、原生错误、`diagnostic` 等技术信息仍可取得的最晚边界；只记录现有结构和调用现场能够取得的信息，不为补齐统一字段而扩展错误合同。不得先把错误压成操作员文字后再尝试补日志，也不得用笼统说明代替逐路径落实。

```text
错误产生处或结构化错误仍完整的边界
    → 先把当前路径实际可取得的技术信息写入现有分类日志
    → 再生成只包含结果、影响和操作动作的操作员文字
    → 操作员文字进入 QMessageBox、状态栏或识别结果正文
```

各路径的落点固定如下：

1. 启动设置：旧 Schema 按第 5 节直接静默覆盖，不进入设置加载失败提示或旧版本专项日志路径；其他设置加载失败时，`ApplicationStartup` 在 `AppSettingsStoreError` 仍完整时先把 `settingsFilePath()`、错误码和诊断写入 `logStartup`，再通过现有设置加载失败弹窗显示固定启动提示。
2. 设置保存：`SettingsApplicationService::saveConfiguration()` 和 `commitAppliedHardwareSettings()` 在保存失败且 `AppSettingsStoreError`、`m_store` 仍可用时，先把错误码、`m_store->settingsFilePath()` 和诊断写入 `logUi`，再调用 `storeFailure()` 生成 `OperationResult`；MainWindow 删除设置文件失败时，也先把目标路径和 `QFile::errorString()` 写入 `logUi`，再显示固定文字。硬件已经生效但软件设置保存失败的分支仍保留该状态事实。
3. 模板操作：`TemplateApplicationService::storeErrorMessage()` 在生成返回页面的文字前，先把 `TemplateStoreError` 的错误码、路径和诊断写入 `logTemplate`，随后只返回操作员文字，不再拼接路径和诊断；没有用户消息时返回固定的“模板操作失败。”。
4. 检测启动：`presentStartFailure()` 先把 `StartInspectionResult` 的错误码、诊断和明细写入 `logRuntime`，再显示操作员文字；不得把诊断或内部明细重新拼入弹窗。
6. 存图失败：`ImageSaveService` 在 `image_save.failed` 记录具体错误后，状态区只显示失败数量以及检查文件夹、权限和磁盘空间的建议，不再追加 `latestError`。
7. 相机参数与恢复：曝光和增益失败保留 MainWindow 现有 `logDevice` 记录，其中包含 `CameraParameterResultDto` 的原生错误码与 `diagnostic`，不得重复写第二条同义日志；停止后恢复曝光失败在 `CameraRecoveryResultDto::errorMessage` 仍可取得时补写一条 `logDevice`。随后按当前操作显示固定文字，不得把 `diagnostic`、`errorMessage` 直接当作弹窗正文，也不为此增加字段。
8. 独立二维码结果接收工具：`ResultReceiverServer`、`ResultReceiverStore` 或其调用窗口在原生网络/文件错误仍可取得时先用 `qWarning()` 记录，再返回固定操作员文字；这是补齐现有 Qt 诊断输出，不新增日志文件、日志页面或错误框架。

二维码+三期的普通 NG 结果当前把 `DetectionResult::diagnostic` 直接拼成“原因”。为保持现有“原因”展示而不继续混用诊断字段，只在现有 `BarcodeWordDetectionWorkOutput` 增加一个操作员原因字符串，由检测管线填写第 9.2—9.3 节的结果文字，`DetectionRegistry` 显示该字符串；`DetectionResult::diagnostic` 在错误仍完整时先写入 `detection.result` 日志。不新增跨模式错误合同或通用结果映射层。

二维码结果接收工具不再把 `QTcpServer::errorString()` 或 `QFile::errorString()` 直接写入窗口和弹窗；界面按监听、结果记录保存或 CSV 生成场景显示固定结果与处理动作，原生错误先写入 Qt 警告输出。

不新增“详细信息”按钮、可展开区域、维护模式、错误文案注册表或通用转换框架。固定文字在现有产生位置原位修改；只在缺失的真实错误路径补写现有分类日志或 `qWarning()`。

二维码相关应用文件只允许为本节增加或传递操作员原因文字、停止把诊断直接显示给操作员，以及补写现有日志；不得修改 `IBarcodeDecoder` 调用、解码输入、选项、格式、后端、预算、策略、模板参数校验、结果状态或 OK/NG 判定。

### 9.2 用户明确指定文字

以下文字是本方案实施时必须采用的最终表达：

| 当前文字 | 最终文字 |
|---|---|
| `启用本机 CSV 记录` | `保存二维码结果到本机（CSV）` |
| `运行模板快照未准备` | `当前模板未准备好` |
| `运行定位资源初始化失败` | `模板定位准备失败，请重新选择或制作模板` |
| `启动资源预检失败` | `无法开始检测` |
| `日期ROI无效或超出原图范围` | `日期检测区域无效或超出图像范围` |
| `BarcodeDecoder.dll 不可用`、`BarcodeDecoder.dll不可用` | `二维码识别组件无法使用` |
| 模板制作提示中的`二维码解码器内部错误`、`二维码解码器发生内部错误` | `二维码识别异常，请联系维护人员` |
| `参数已下发，但保存配置失败，重启后可能不会保留。` | `参数已应用到设备，但未保存到软件设置。` |
| `剔除队列已清空！` | `待执行的剔除动作已清除。` |
| `触发模式运行中` | `硬触发模式运行中` |
| `所有设置已经完成！` | `PLC 运行参数已应用。` |
| `相机异常！` | `相机打开失败，请检查相机连接和参数。` |
| `当前使用产品模板文件夹名称` | `当前模板` |
| `清空当前软件数据` | `删除已保存的软件设置` |
| 用户可见文字中的`定位锚点` | `定位参考区域` |
| 用户可见文字中的`刚印检测`、`刚印检测区域` | `钢印检测`、`钢印检测区域` |

模板选择表格中的“待移除”保持不变。代码标识、稳定模式 ID、文件名、类名、日志字段和开发文档中的 `stamp`、ROI、DLL、Runtime、Snapshot 等技术词不因本节机械改名。

“参数已应用到设备，但未保存到软件设置。”只替换当前首段固定提示；其后不得追加路径、错误码或内部诊断。需要保留的具体原因进入现有日志。

### 9.3 全仓复核补充文字

以下文字经调用链复核后确认会进入主程序或独立二维码结果接收工具的窗口、弹窗、状态区或结果正文，同样纳入本阶段。表中的省略号只合并同类现有分支；实施时必须先按第 9.1 节记录各分支实际可取得的技术信息，再生成表中操作员文字，不新增统一文案框架。

#### 9.3.1 启动与软件设置

| 当前文字 | 最终文字 |
|---|---|
| `程序运行中避免重复打开` | `软件已在运行，请勿重复打开。` |
| `无法确定当前用户的应用数据目录。` | `无法访问软件设置文件夹，软件不能启动。` |
| 设置加载失败弹窗标题`设置文件损坏` | `设置错误` |
| `设置文件包含不支持的字段。`、`设置文件缺少……或……类型不正确。`、`模板路径列表中存在非文本值。`、`设置文件包含不支持的选项。` | `软件设置文件内容不完整或格式不正确。` |
| `设置数值超出合法范围。` | `软件设置中的数值无效。` |
| `设置字段组合不符合设备合同。` | `软件设置与当前设备不匹配。` |
| `启用存图时必须选择绝对输出目录。` | `保存图像前，请先选择保存文件夹` |
| `启用存图前，请先选择图像保存路径。` | `保存图像前，请先选择保存文件夹` |
| `启用本机 CSV 时必须选择绝对输出目录。` | `保存二维码结果前，请先选择保存文件夹。` |
| `模板保存目录必须是规范化绝对路径。` | `请选择有效的模板保存文件夹` |
| `PLC 地址与当前设备合同不一致。` | `PLC 参数与当前设备不匹配。` |
| `检测方案中的模板路径或纸巾阈值无效。` | `检测设置无效，请重新选择模板或检查纸巾检测阈值。` |
| `设置读取目标无效。` | `软件设置读取失败，请联系维护人员。` |
| `无法读取设置文件。` | `无法读取软件设置，请检查文件权限。` |
| `设置文件已损坏，程序不会自动覆盖原文件。` | `软件设置文件已损坏，原设置不会被覆盖。` |
| `无法创建设置目录。` | `无法创建软件设置文件夹，请检查文件夹权限。` |
| `无法写入设置文件。`、`设置文件写入不完整。`、`设置文件原子提交失败，原文件保持不变。` | `软件设置保存失败，原设置保持不变。` |
| `应用设置保存失败。` | `软件设置保存失败。` |
| `设置清空失败` | `软件设置更新失败` |
| `当前界面设置保存失败`、`配置保存失败` | `软件设置保存失败` |
| `无法删除软件设置：`后直接追加原生文件错误 | 先把目标路径和原生文件错误写入 `logUi`，再显示`无法删除软件设置，请检查文件权限。` |

普通设置保存失败继续恢复界面中的已保存值。相机或 PLC 参数已经应用到设备、随后持久化失败的分支不得使用“原设置保持不变”掩盖硬件已生效事实，固定使用第 9.2 节的“参数已应用到设备，但未保存到软件设置。”。

#### 9.3.2 软件设置页与二维码结果保存

| 当前文字 | 最终文字 |
|---|---|
| `软件数据` | `软件设置` |
| `当前软件数据文件夹：` | `软件设置文件夹：` |
| `软件公共设置保存在此文件夹。双击可打开目录；产品模板、识别图片、授权和日志不在清空范围内。` | `软件设置保存在此文件夹。双击可打开；产品模板、识别图片、授权文件和日志不会被删除。` |
| `无法打开软件数据文件夹` | `无法打开软件设置文件夹` |
| `二维码结果本机记录` | `二维码结果保存` |
| 二维码结果设置中的`输出目录` | `保存文件夹` |
| `选择二维码 CSV 输出目录` | `选择二维码结果保存文件夹` |
| `二维码 CSV 设置保存失败` | `二维码结果保存设置失败` |
| `二维码 CSV 输出目录不可用，请重新选择可写入的本机目录。` | `二维码结果保存文件夹不可用，请重新选择。` |
| `二维码 CSV 写入不可用` | `二维码结果无法保存到本机` |

#### 9.3.3 模板读取、校验与保存

| 当前文字 | 最终文字 |
|---|---|
| `模板配置包含不支持的字段。`、`模板配置缺少有效版本号。`、`模板模式或目标文字字段无效。`、`模板图像阈值字段无效。`、`模板定位区域字段无效。`、`模板区域点集格式无效。`、`模板字符配置格式无效。`、`字符源图尺寸无效。`、`字符框格式无效。`、`二维码参数格式无效。` | `模板文件内容不完整或格式不正确。` |
| `模板配置版本不受支持。` | `模板版本与当前软件不兼容。` |
| `模板配置文件已损坏。`、`模板配置无效。` | `模板文件已损坏。`、`模板文件内容无效。` |
| `无法读取模板配置。` | `无法读取模板文件。` |
| `模板参数超出合法范围。`、`模板区域中存在无效坐标。` | `模板参数或检测区域无效，请重新制作模板。` |
| `模板原图或定位区域无效。` | `模板图像或定位参考区域无效。` |
| `模板缺少 template_settings.json。` | `模板文件内容不完整。` |
| `模板缺少必需图片资源。` | `模板缺少检测所需图片。` |
| `模板缺少字符模板目录。` | `模板缺少字符图片。` |
| `模板资源已损坏。` | `模板文件已损坏。` |
| `模板资源尚未制作完整。` | `模板尚未制作完整。` |
| `模板图片编码失败。` | `模板图片保存失败。` |
| `模板保存路径无效。` | `请选择有效的模板保存文件夹` |
| `模板加载目标无效。`、`运行模板加载目标无效。` | `模板加载失败，请联系维护人员。` |
| `字符模板写入目标无效。` | `字符模板保存失败，请联系维护人员。` |
| `无法创建模板临时目录。` | `模板保存失败，请检查保存文件夹权限和磁盘空间。` |
| `模板目录不能包含符号链接。` | `所选模板文件夹包含不支持的链接，请选择普通文件夹。` |
| `模板保存目标不是文件夹。` | `所选模板保存位置不是文件夹。` |
| `无法创建模板上级目录。` | `无法创建模板保存文件夹。` |
| `无法备份原模板，未修改原目录。` | `模板保存失败，原模板仍可使用。` |
| `模板提交失败，原模板已恢复。` | `模板保存失败，原模板仍可使用。` |
| 模板提交或回滚失败 | `模板保存失败，原模板可能不可用，请联系维护人员。` |
| `模板已保存，但旧备份目录清理失败。` | `模板已保存，但旧备份文件未能清理，请联系维护人员。` |

#### 9.3.4 检测启停与相机

| 当前文字 | 最终文字 |
|---|---|
| `没有可用的多模板定位配置。` | `当前模板未准备好。` |
| `应用设置中的检测模式无效。` | `当前选择的检测模式无效。` |
| `未知模板错误` | `模板无法使用。` |
| `当前模式没有可用运行模板。` | `当前模式没有可用模板。` |
| `运行模板模式不匹配。` | `所选模板与当前检测模式不匹配。` |
| `钢印模板无法初始化防重叠检测资源。` | `钢印模板中的防重叠区域无效，请重新制作模板。` |
| `深度OCR引擎或目标文本未初始化。` | `深度 OCR 未准备好，请检查模板中的目标文字。` |
| `读码组件不可用：%1` | `二维码识别组件无法使用。`；具体原因只写日志 |
| `二维码+三期模板资源预检失败。`、`二维码+三期模板预检失败` | `无法开始检测` |
| `无法启动……检测工作线程。` | `无法开始……检测，请重试。`；保留对应检测模式名称 |
| `启动识别前PLC参数下发失败`、`PLC运行参数下发失败` | `PLC 参数应用失败，无法开始检测。`、`PLC 运行参数应用失败` |
| `相机采集会话尚未准备完成。` | `相机尚未准备好，无法开始检测。` |
| `采集线程启动失败。`、`相机采集线程启动失败。` | `图像采集启动失败。` |
| `相机采集线程仍在运行，请先停止当前任务。` | `相机正在采集图像，请先停止检测。` |
| `实时取景线程启动失败。` | `无法开始实时取景，请重试。` |
| `实时取景线程尚未停止，请稍后重试。` | `实时取景尚未停止，请稍后重试。` |
| `运行状态提交失败。` | `无法开始检测，请重试。` |
| `检测运行时未处于启动状态。` | `检测尚未准备好，无法开始。` |
| `未找到定位锚点` | `未找到定位参考区域` |
| `二维码区域配置无效或未映射` | `二维码检测区域未设置或无效` |
| `日期检测区域配置无效或未映射` | `日期检测区域未设置或无效` |
| `相机断连或正式采集异常` | `相机连接或图像采集异常` |
| `PLC连接或结果输出异常` | `PLC 连接或检测结果发送异常` |
| `产品身份无法唯一确定` | `无法确定当前图像对应的产品` |
| `请先停止当前任务再清空剔除队列。` | `请先停止检测，再清除待执行的剔除动作。` |
| 相机曝光设置失败时直接显示 `CameraParameterResultDto::diagnostic` | `相机曝光设置失败，请检查输入值和相机状态。`；先记录原生错误码和诊断 |
| 相机增益设置失败时直接显示 `CameraParameterResultDto::diagnostic` | `相机增益设置失败，请检查输入值和相机状态。`；先记录原生错误码和诊断 |
| `停止识别后恢复相机曝光失败：`后直接追加恢复错误 | `停止检测后，相机曝光恢复失败，请检查相机状态。`；先记录恢复错误 |

检测模式名中的“深度 OCR”、PLC、IP、NG、CSV 等当前产品或维护操作所需名称继续保留。

#### 9.3.5 独立二维码结果接收工具

| 当前文字 | 最终文字 |
|---|---|
| `首次使用必须选择绝对输出目录。` | `首次使用前，请先选择结果保存文件夹。` |
| `无法创建输出目录。` | `无法创建结果保存文件夹，请检查文件夹权限。` |
| `输出目录为空。` | `请先选择结果保存文件夹。` |
| `输出目录`、`选择结果输出目录` | `结果保存文件夹`、`选择结果保存文件夹` |
| `CSV 原子替换失败。` | `CSV 文件保存失败，原文件保持不变。` |
| `time 字段无效。`、`time 必须使用 UTC Z。`、`time 不是有效的 ISO-8601 时间。` | `结果记录中的时间格式无效。` |
| `JSONL 尾行备份失败。` | `无法备份不完整的结果记录。` |
| `JSONL 尾行截断失败。` | `无法修复不完整的结果记录。` |
| `JSONL 中间记录损坏。` | `结果记录已损坏，无法生成 CSV 文件。` |
| `收到坏 JSON，已关闭当前客户端。` | `收到的结果数据格式不正确，当前连接已断开。` |
| `产品 JSON 字段无效，不返回 ACK。` | `收到的产品结果格式不正确，未确认接收。` |
| `产品 JSON 的 OK/NG 与 qrContent 组合无效。` | `收到的产品判定与二维码内容不一致，未确认接收。` |
| `JSONL 写入失败。`、`JSONL 写入失败，不返回 ACK。` | `结果记录保存失败，未确认接收。` |
| 结果列表中的`accepted`、`duplicate` | `已接收`、`重复记录` |
| `CSV 同步未全部成功` | `部分 CSV 文件未能生成` |
| 监听失败弹窗直接显示原生网络错误 | 先用 `qWarning()` 记录原生网络错误，再显示`无法开始接收，请检查端口是否被占用。` |
| 结果记录或 CSV 保存失败时直接显示原生文件错误 | 先用 `qWarning()` 记录原生文件错误，再按失败动作显示`结果记录保存失败，请检查保存文件夹权限和磁盘空间。`或`CSV 文件保存失败，请检查保存文件夹权限和磁盘空间。` |

结果接收工具中的“监听端口”“同步 JSONL 到 CSV”以及授权工具中的 `license.ini` 是其使用者完成明确维护任务所需的真实名称，保持不变。`.ui` 中的 `idle`、`danger`、`primary` 是控件动态属性值，不是显示文字，保持不变；PLC 设置中的 `Rack/Slot` 改为说明性标签`机架/槽位（Rack/Slot）：`，字段、默认值和连接行为不变。

### 9.4 文件范围

预计只修改以下直接产生或显示上述文字的文件：

```text
app/startup/application_startup.cpp
app/contracts/detection_mode.cpp
app/system_support/settings/app_settings_store.cpp
app/application/settings_application_service.cpp
app/application/inspection_application_service.cpp
app/application/inspection_start_preflight.cpp
app/application/template_application_service.cpp
app/application/template_geometry_service.cpp
app/templates/template_store.cpp
app/detection/detection_registry.cpp
app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h
app/detection/detectionmode/word/word_detection_pipeline.cpp
app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.cpp
app/runtime/camera_session.cpp
app/runtime/inspection_runtime.cpp
app/ui/main_window/main_window.cpp
app/ui/main_window/main_window_inspection.cpp
app/ui/main_window/main_window_settings.cpp
app/ui/main_window/inspection/inspection_page.h
app/ui/main_window/inspection/inspection_page.cpp
app/ui/main_window/inspection/inspection_info_page.ui
app/ui/main_window/settings/machine_settings_page.cpp
app/ui/main_window/settings/detection_settings_page.ui
app/ui/main_window/settings/plc_settings_page.ui
app/ui/main_window/settings/software_settings_page.ui
app/ui/main_window/template/template_editor_page.cpp
app/resource/Translate_CN.ts
app/resource/Translate_EN.ts
tools/result_receiver/result_receiver_window.ui
tools/result_receiver/result_receiver_window.cpp
tools/result_receiver/result_receiver_store.cpp
tools/result_receiver/result_receiver_server.cpp
```

翻译文件只同步被 `.ui` 源文字直接影响的条目，并保持现有英文含义；不借此补做全量国际化。实施时若某个预计文件没有对应旧文字或真实显示链，不为保持清单整齐而制造差异。

### 9.5 保持项

- 所有按钮动作、弹窗触发条件、默认按钮和危险操作确认流程不变；
- 运行故障触发自动停止，尚未完成的产品记为未确认，收口后界面回到空闲；
- 设置失败回退、硬件已生效但磁盘未保存的状态语义不变；
- 模板加载、保存、批量处理、选择和磁盘目录语义不变；
- 用户主动选择或查看的模板目录、存图目录和 CSV 输出目录继续在现有路径控件中显示；禁止的是把错误结构中的内部路径自动追加到提示正文；
- CSV 文件格式、字段、保存位置和写入时机不变；
- 钢印检测模式 ID、模板目录、Schema、算法和代码类型名不变；
- 二维码相关应用文件只修改操作员文字和诊断显示边界，解码调用、输入、选项、格式、后端、预算、策略、模板参数校验、结果状态和 OK/NG 判定保持不变；
- “待移除”及第 9.2—9.3 节未列出的固定提示文字不变；第 9.1 节所列路径仍须停止直接显示动态诊断；
- 每条被改写为操作员文字的错误路径，都必须先在错误产生处或现有技术信息仍可取得的边界记录该路径实际存在的错误码、路径、原生错误和诊断；缺少其中某类信息时不新增字段、错误类型或转换层。

## 10. 第六阶段：清理注释

### 10.1 删除范围

在第一方 `app/` 与 `tools/` 的 `.h/.cpp` 中删除，但完整排除 `tools/barcode_decoder/**`、`app/engines/barcode/**` 和 `app/contracts/barcode_parameter_defaults.h`。供应商源码只排除 `app/engines/ocr/vendor/paddle/**` 和 `app/devices/plc/vendor/snap7.h`、`app/devices/plc/vendor/snap7.cpp`；项目适配器 `app/engines/ocr/vendor/paddle_ocr_engine.h/.cpp` 与 `app/devices/plc/vendor/snap7_plc_device.h/.cpp` 必须纳入：

```text
文件作用：本文件用于……
主要职责：……
模块位置：……
协作说明：本文件只通过明确的接口……
函数说明：某函数实现名称所表示的处理步骤
函数说明：某函数检查相关状态并返回判断结果
组件说明：某组件封装本文件中与其名称对应的单一职责
组件说明：某数据结构集中保存该流程需要的一组相关数据
```

同时删除或修正与实际代码不符的注释，例如 `MainWindow` 析构函数中不存在的“删除临时文件”。

### 10.2 必须保留的注释

- C ABI、DLL 导出、调用约定和内存所有权；
- Qt 线程归属、跨线程投递和对象销毁顺序；
- OpenCV/QImage 缓冲区生命周期与必要深拷贝；
- ProductKey、Fault、PLC、队列和 mailbox 的真实不变量；
- 坐标系、ROI、旋转、缩放和几何转换中不直观的规则；
- 算法公式、阈值单位和供应商 API 的特殊限制；
- 当前仍存在且有明确依据的外部兼容合同。

### 10.3 清理规则

- 不把删除的模板注释替换为另一种模板；
- 不修改上述明确排除的 PaddleOCR、Snap7 上游源码；不得把整个 `vendor/` 目录排除在外；
- 不扫描、修改或格式化 `tools/barcode_decoder/**`、`app/engines/barcode/**` 或 `app/contracts/barcode_parameter_defaults.h`；
- 不修改历史计划和执行记录中的过程说明；
- 注释清理不顺手重命名类型、格式化全文件或调整业务代码；
- 只删除注释后失去必要性的空行，不制造大范围无关格式差异。

## 11. 第七阶段：同步现行文档

### 11.1 根 README

将当前架构改为实际目录和类型：

```text
app/templates/
TemplateSettings
PreparedTemplate
TemplateStore
TemplateApplicationService 编辑状态
```

删除把 `app/recipes/`、`ProductRecipe`、`PreparedRecipe`、`RecipeStore` 和 `RecipeEditorSession` 描述为当前实现的段落。历史方案中的旧术语不批量改写。

### 11.2 计划索引

当前代码和真实调用链决定“当前已实现行为”，本方案中用户明确指定的内容决定“整改目标”。计划索引只能按这两个层次记录，不得用计划中的目标文字反向覆盖代码事实，也不得在生产代码尚未修改时把目标状态标为已实施。

本方案条目已在保留计划索引既有差异的基础上更新为“代码已实施，待用户统一验证”，并完成以下修正：

1. 已按真实实施证据更新本方案状态。
2. 软硬触发条目只把已经完成的启动统计与界面清理写为已实施，并按当前代码保留软件触发阻塞提交、硬触发非阻塞提交、硬触发队列满进入 `HardTriggerQueueOverflow` Fault；已删除与当前代码冲突的“过载按 NG 继续运行”等表述。
3. 所有现行条目和任务路由只引用真实存在的计划文件；尚未形成方案文件的后续任务不预建条目或路由。
4. 相机说明只在现有模块 README 和开发者指南中按当前代码与实际验证证据同步必要内容，不另造现状文档。
5. 不虚构构建、运行或现场验证证据。

### 11.3 当前维护文档

同步以下现行说明：

```text
README.md
app/startup/README.md
app/application/README.md
app/runtime/README.md
app/contracts/README.md
app/ui/README.md
docs/development/OCRGangYin新架构数据Schema.md
docs/development/OCRGangYin开发者代码结构与维护指南.md
docs/development/OCRGangYin计划索引.md
```

其中 `app/application/README.md` 删除旧相机合同路径和不存在的 `inspection_ui_contract.h`；`app/contracts/README.md` 登记现有 `inspection_presentation.h` 与新增的唯一相机结果合同；`app/runtime/README.md` 使用真实合同路径；`app/ui/README.md` 保持参数页集中绑定的真实说明。二维码引擎与模板结构未被本方案改变，不修改 `app/engines/README.md` 或 `app/templates/README.md`。其他文件只有内容被本方案实际改变时才修改，不为“已检查”制造无内容差异。

## 12. 分阶段实施顺序

### 阶段 0：重新确认基线

1. 记录分支、HEAD、工作区、暂存区和全部用户已有差异。
2. 重新执行本方案涉及符号、文件和调用链检索。
3. 以当前代码确认软硬触发、Fault、日志和相机错误结构的真实行为，不使用计划索引中的目标描述替代代码事实。
4. 若当前代码已发生变化，只按新的真实调用修订本方案范围，不机械套用旧行号。

### 阶段 1：旧 Schema 说明同步

- 保持当前代码的默认设置原子覆盖和继续启动路径；
- 删除现行说明中的用户确认、提示、迁移、备份和旧字段读取表述；
- 完成本阶段静态门禁后再进入下一阶段。

### 阶段 2：零调用接口与相机合同

- 先删除零调用接口；
- 再合并相机结果数据类型并删除转换函数；
- 保持 Runtime、Application、UI 的现有命令和结果流。

### 阶段 3：参数页保持确认

- 确认集中绑定、字符串键和运行时控件类型分发保持原实现；
- 只应用第 9 节操作员提示和第 10 节注释清理，不进行参数页结构重写。

### 阶段 4：操作员提示文字

- 对第 9.1 节每条路径先在错误产生处或现有技术信息仍可取得的边界记录该路径实际可取得的信息，再停止向弹窗、状态栏和结果正文拼接路径、错误码与内部诊断；
- 再按第 9.2—9.3 节逐项替换固定文字，并同步直接受影响的 `.ui` 翻译条目；
- 不修改提示触发条件、业务结果、设备操作或界面结构。

### 阶段 5：注释净化

- 在生产结构稳定后删除模板化注释；

### 阶段 6：文档同步

- 最后同步 README、Schema、开发者指南和计划索引，使文档只描述最终状态。

每个阶段只处理自己的直接范围；不得因看到相邻问题顺手扩展。

## 13. 静态门禁

### 13.1 旧 Schema 静默覆盖

- `schemaVersion != 8` 通过 `ResetRequired` 和 `SETTINGS_RESET_REQUIRED` 进入唯一默认覆盖分支；
- 该分支只执行 `AppSettings::defaults()`、`AppSettingsStore::save()` 和正常继续启动，不显示提示或确认，也不写旧版本专项日志；
- 覆盖结果是当前 Schema 8 的完整默认设置，不读取、转换或保留旧 Schema 的其他字段；
- 原路径通过现有 `QSaveFile` 原子替换，不备份、不另存、不迁移，也不保留第二套覆盖入口；
- 覆盖失败时按现有设置写入失败路径退出；当前 Schema 内容损坏仍拒绝启动且不被覆盖；
- 软件设置页不存在“恢复默认设置”入口。

### 13.2 零调用接口

- 第 6.2 节最终确认删除的符号在生产代码中零引用；
- 没有以新名字恢复同等 getter、转发或包装；
- `InspectionRuntime::statistics()` 等受保护真实接口仍有调用者；
- 声明、定义、include 和 qmake 清单闭环。

### 13.3 相机合同

- 相机打开、参数和恢复结果各只有一套类型；
- `cameraParameterDto`、`cameraOpenIssueDto`、`cameraOpenDto`、`cameraRecoveryIssueDto`、`cameraRecoveryDto` 零引用；
- 旧合同文件路径零引用且不保留转发头；
- `InspectionAcquisitionDto` 只在 `inspection_application_service.h` 定义一次，现有软件/硬件触发枚举值和消费路径不变；
- 共享合同不 include QWidget、相机 SDK、Runtime 实现或 Detection；
- Application 的组合结果和 MainWindow 相机命令调用仍闭环；
- 曝光和增益失败继续只有现有一条包含原生错误码与 `diagnostic` 的 `logDevice` 记录；停止后恢复曝光失败只补写一条包含现有 `errorMessage` 的 `logDevice` 记录；随后显示对应固定操作员提示；
- 操作员界面不直接显示相机结果中的 `diagnostic` 或 `errorMessage`，且没有为此增加 `userMessage` 字段或通用映射类型。

### 13.4 参数页

- `QMap<QString, GlobalSettingBinding>`、通用控件指针和现有 `qobject_cast` 分发保持；
- `MachineSettingsPage` 和 MainWindow 继续使用现有 `QString`/`QStringList` 设置键；
- 不存在 `MachineSettingField` 或第二套参数分发入口；
- JSON 字段名、UI 对象名、显示文字和日志字段不发生计划外变化；
- 所有现有字段继续通过原集中绑定完成 signal、读取、dirty、恢复和日志路径。

### 13.5 注释与文档

除 `app/engines/ocr/vendor/paddle/**`、`app/devices/plc/vendor/snap7.h`、`app/devices/plc/vendor/snap7.cpp`、`tools/barcode_decoder/**`、`app/engines/barcode/**` 和 `app/contracts/barcode_parameter_defaults.h` 外，第一方 `app/` 与 `tools/` 代码中以下模板短语必须为零：

```text
文件作用：本文件用于
主要职责：
模块位置：
协作说明：本文件只通过明确的接口
函数说明：
组件说明：
```

同时确认：

- `MainWindow` 析构注释不再描述不存在的行为；
- 根 README 不再把 `app/recipes/` 描述为当前目录；
- 计划索引按当前代码记录软件触发阻塞提交和硬触发非阻塞提交；
- 计划索引中的当前任务入口均指向真实存在的 Markdown 文件，也没有为尚未形成方案的任务预建条目、路由或现状文档；
- 历史计划和执行记录没有被改写成当前状态。

### 13.6 操作员提示文字

- 第 9.2—9.3 节列出的最终文字在对应用户可见路径中存在且拼写一致；
- 用户可见的“刚印检测”和“刚印检测区域”为零，“钢印”代码标识和稳定模式 ID 不发生改名；
- 用户可见的“定位锚点”为零，代码变量、几何合同和开发文档不做机械替换；
- “待移除”在模板选择表格中保持不变；
- 硬触发状态明确显示“硬触发模式运行中”，软触发状态保持“软触发模式运行中”；
- 用户可见的“启用存图前，请先选择图像保存路径。”“模板保存路径无效。”为零，对应文字分别为“保存图像前，请先选择保存文件夹”“请选择有效的模板保存文件夹”；
- 主程序用户消息中“原子提交”“设备合同”“规范化绝对路径”“运行模板快照”“运行定位资源初始化”“采集线程”和“检测队列”等实现术语为零；同词若只存在于代码、注释、日志或诊断字段，不做机械替换；
- 二维码结果接收工具不再显示“CSV 原子替换”“坏 JSON”“不返回 ACK”等实现细节，`同步 JSONL 到 CSV`等任务所需格式名保持不变；
- `.ui` 的 `idle`、`danger`、`primary` 仍仅作为动态属性值存在，不被误改成界面文字；
- 二维码+三期结果中的“原因”来自现有工作输出的操作员原因字符串，不再读取 `DetectionResult::diagnostic`；内部诊断仍进入 `detection.result` 日志；
- 模板和启动提示不直接拼接错误结构中的 `code`、`path` 或 `diagnostic`；存图失败状态不再追加 `latestError`；
- 相机曝光和增益失败弹窗不直接显示 `diagnostic`，并复用现有包含原生错误码与 `diagnostic` 的唯一 `logDevice` 记录；停止后恢复曝光失败不直接显示 `errorMessage`，只补写一条包含现有 `errorMessage` 的 `logDevice` 记录；
- ResultReceiver 的监听、结果记录和 CSV 生成失败先通过 `qWarning()` 记录原生网络或文件错误，再显示固定操作员文字；
- `tools/barcode_decoder/**`、`app/engines/barcode/**` 和 `app/contracts/barcode_parameter_defaults.h` 没有工作区差异；`app/templates/template_store.cpp` 及二维码相关应用文件的差异只涉及第 9 节文字和诊断显示边界；
- 第 9.1 节每条错误路径都能证明该路径实际可取得的技术信息在生成操作员文字之前已写入对应日志；缺少某类信息时不新增字段、错误类型或转换层；
- 不新增文案表、错误映射框架、弹窗、控件、QSS、图标或配置字段；
- `.ui` 源文字变化与 `Translate_CN.ts`、`Translate_EN.ts` 的直接关联条目一致。

### 13.7 工程与文本

- `AutoOCRproject.pro` 中源码、头文件、UI 和 QRC 路径存在、大小写匹配、无重复、无已删除路径、无未登记生产文件；
- `tools/result_receiver/ResultReceiver.pro` 中 SOURCES、HEADERS、FORMS 和 DISTFILES 路径存在、大小写匹配、无重复，且没有计划外工程条目变化；
- 所有本地 quoted include 可解析；
- `.ui`、`.qrc` 可按 XML 严格解析；
- Python 与 PowerShell 独立脚本保持可解析；
- 修改文本为严格 UTF-8、有文件末尾换行、无尾随空白；
- 无 TODO、FIXME、合并冲突标记或注释旧实现；
- `git diff --check` 与 `git diff --cached --check` 通过；
- 静态检查结果不得表述为构建、运行或现场验证通过。

## 14. 用户构建与人工验证

### 14.1 设置文件

1. 无设置文件启动：不显示设置版本提示，使用当前默认值进入程序。
2. 准备一个非 Schema 8 文件：程序不显示提示或确认，直接进入主界面。
3. 退出后检查该文件已被当前 Schema 8 的完整默认设置原子替换，旧字段值未迁移，旁边没有备份或副本。
4. 准备一个字段损坏但版本为 8 的文件：程序通过现有设置加载失败弹窗拒绝启动，文件不被覆盖，正文表达文件损坏。
5. 确认软件设置页仍没有“恢复默认设置”入口，“删除已保存的软件设置”仍只删除当前 Windows 用户的软件设置。

### 14.2 运行时与相机

1. 对主程序执行 Run qmake、Rebuild，确认移动合同和删除接口后没有 moc、include、声明或链接错误。
2. 验证相机打开、曝光/增益设置、模板预览、正式采集和正常停止。
3. 验证五种模式软触发；涉及 PLC 的硬触发和结果输出由现场继续验证。
4. 验证统计清零、NG 清零、延迟 NG、存图和 Presentation 显示没有因接口清理改变。
5. 分别触发曝光和增益失败，确认日志包含现有原生错误码与 `diagnostic`；再触发停止后恢复曝光失败，确认日志只记录当前已有的 `errorMessage`；三类弹窗均只显示对应固定操作提示。

### 14.3 参数设置

1. 逐项修改需要应用的相机、图像、纸巾和 PLC 参数，确认 `*`、成功提交和失败回退。
2. 验证检测模式、存图范围、存图内容、目录和硬触发开关的即时保存。
3. 验证同标签多字段、启动前未应用提示、放弃未应用值和当前日志输出。
4. 在 Idle、模板操作、Running 和 Stopping 状态检查原有控件权限。

### 14.4 操作员提示文字

1. 逐项触发第 9.2—9.3 节涉及的设置、模板、检测启动、二维码、相机、硬触发和剔除复位场景，确认显示最终文字。
2. 确认模板选择表格仍显示“待移除”。
3. 确认操作员弹窗不再显示文件路径、错误码、原生错误或内部诊断，但日志仍包含对应路径实际可取得的排障信息。
4. 确认“删除已保存的软件设置”对话框继续明确说明不会删除产品模板、识别图片、授权文件和日志。
5. 对 `tools/result_receiver/ResultReceiver.pro` 执行 Run qmake、Rebuild，再验证保存目录、接收数据、重复记录、损坏记录和 CSV 同步失败提示，同时确认接收协议及 JSONL/CSV 内容不变。
6. 确认文字修改没有改变弹窗触发时机、默认按钮、取消路径、检测结果和设备动作。
7. 分别触发未选择图像保存文件夹和模板保存路径无效，确认显示第 9.3 节指定文字。
9. 在 ResultReceiver 中触发监听、结果记录和 CSV 生成失败，确认 `qWarning()` 先记录原生错误，界面不显示原生错误。

## 15. 完成标准

本方案只有同时满足以下条件才可标记为完成：

1. 旧 Schema 版本不匹配时直接由当前 Schema 8 默认设置原子覆盖并继续启动，没有提示、确认、迁移、备份或旧字段读取；当前 Schema 内容损坏仍保留原文件并拒绝启动。
2. 已确认零调用接口及逐层转发被完整删除，没有别名、空实现或替代包装。
3. 相机打开、参数和恢复结果只保留一套纯数据合同，机械 DTO 转换函数消失；参数失败记录现有原生错误码与 `diagnostic`，恢复失败只记录现有 `errorMessage`，界面只显示固定操作提示。
4. 参数页保留原有集中绑定、字符串键和通用控件类型分发，现有 dirty、应用和失败回退行为保持。
5. 第一方代码中的模板化、复述性和失真注释清理完成，必要约束注释保留。
6. 根 README、相关模块 README、Schema、开发者指南和计划索引只描述当前最终状态；计划索引以当前代码记录软硬触发行为，所有条目和路由指向真实文件，也没有为尚未形成方案的任务预建文档。
7. 第 9.2—9.3 节规定的使用者可见文字全部生效；所有相关路径均先记录现有错误结构和调用现场实际可取得的技术信息，再生成不含内部路径、错误码或诊断字符串的操作员文字。
8. 工程清单、include、XML、UTF-8 和 Git 差异静态门禁通过。
9. 用户完成主程序与 ResultReceiver 的 Qt Creator 构建、程序交互及必要的相机、PLC 现场验证。
10. 未增加本方案范围外功能、抽象、兼容路径、迁移逻辑、视觉或部署内容。

## 16. 最终结果

整改后的仓库只保留当前产品真实需要的结构：

```text
旧 Schema 由当前默认设置静默覆盖并继续启动
唯一相机结果数据合同
真实调用者需要的 Runtime/Application API
集中式参数页绑定与设置操作
面向操作员的结果与动作提示
解释真实约束的必要注释
与源码一致的现行文档和计划路由
```

不保留历史实现的运行入口、无消费者接口、逐字段 DTO 搬运、模板化注释或不存在的计划链接；操作员界面不直接展示内部路径、错误码、诊断字符串或已确认替换的实现术语。
