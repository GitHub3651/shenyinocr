# OCRGangYin 开发者代码结构与维护指南

状态：阶段 8 实现完成；A2 生产代码实施完成，等待用户统一验证
范围：只列代码和工程文件，不列图片、模型、DLL、样式或翻译资源

## 1. 先用一句话理解架构

```text
UI 收集操作
 → Application 组织用例
 → Settings/Templates 提供持久化数据
 → Runtime 冻结一次生产运行
 → Detection 调用算法和 Engines
 → Devices 与相机/PLC通信
 → Runtime 收口结果、统计、PLC、存图
 → UI 展示
```

最重要的边界：

- `contracts`：五模式稳定规则。
- `system_support/settings`：整机设置和当前检测方案。
- `templates`：一个外部模板文件夹的唯一磁盘入口。
- `application`：把一次用户操作跨模块组织起来。
- `runtime`：一次正式生产运行的线程和状态。
- `detection`：纯图像定位和判定。
- `devices/engines`：外部硬件和识别能力适配。
- `ui`：控件、对话框和展示。
- `startup`：只创建并连接上述对象。

## 2. 完整代码文件树

```text
app/
├─ AutoOCRproject.pro
├─ application/
│  ├─ application_result.h
│  ├─ camera_application_contract.h
│  ├─ inspection_application_service.h/.cpp
│  ├─ inspection_start_preflight.h/.cpp
│  ├─ inspection_ui_contract.h
│  ├─ runtime_snapshot.h
│  ├─ settings_application_service.h/.cpp
│  ├─ template_application_service.h/.cpp
│  ├─ template_editor_contract.h
│  └─ template_geometry_service.h/.cpp
├─ contracts/
│  ├─ barcode_parameter_defaults.h
│  └─ detection_mode.h/.cpp
├─ templates/
│  └─ template_store.h/.cpp
├─ detection/
│  ├─ detection_registry.h/.cpp
│  ├─ multi_template_runtime_snapshot.h/.cpp
│  ├─ common/
│  │  ├─ character_template_matcher.h/.cpp
│  │  ├─ detection_roi_geometry.h
│  │  ├─ detection_pose.h
│  │  ├─ frame_preprocessor.h/.cpp
│  │  ├─ inspection_positioner.h/.cpp
│  │  ├─ template_pose_selector.h/.cpp
│  │  └─ tracking_pose_matcher.h/.cpp
│  └─ detectionmode/
│     ├─ stamp/
│     │  ├─ overlap_detector.h/.cpp
│     │  └─ stamp_detection_pipeline.h/.cpp
│     ├─ word/word_detection_pipeline.h/.cpp
│     ├─ ocr/ocr_detection_pipeline.h/.cpp
│     ├─ tissue/
│     │  ├─ tissue_detection_pipeline.h/.cpp
│     │  └─ tissue_roll_detector.h/.cpp
│     └─ barcode_word/barcode_word_detection_pipeline.h/.cpp
├─ devices/
│  ├─ camera/
│  │  ├─ camera_device.h
│  │  └─ vendor/hikvision_camera_device.h/.cpp
│  └─ plc/
│     ├─ plc_device.h
│     └─ vendor/
│        ├─ snap7_plc_device.h/.cpp
│        └─ snap7.h/.cpp
├─ engines/
│  ├─ barcode/
│  │  ├─ barcode_decoder.h
│  │  ├─ barcode_types.h
│  │  └─ vendor/
│  │     ├─ barcode_decoder_adapter.h/.cpp
│  │     └─ barcode_decoder_api.h
│  └─ ocr/
│     ├─ ocr_engine.h
│     └─ vendor/
│        ├─ paddle_ocr_engine.h/.cpp
│        └─ paddle/
│           ├─ include/
│           │  ├─ clipper.h
│           │  ├─ config.h
│           │  ├─ ocr_cls.h
│           │  ├─ ocr_det.h
│           │  ├─ ocr_rec.h
│           │  ├─ postprocess_op.h
│           │  ├─ preprocess_op.h
│           │  └─ utility.h
│           └─ src/
│              ├─ clipper.cpp
│              ├─ config.cpp
│              ├─ ocr_cls.cpp
│              ├─ ocr_det.cpp
│              ├─ ocr_rec.cpp
│              ├─ postprocess_op.cpp
│              ├─ preprocess_op.cpp
│              └─ utility.cpp
├─ runtime/
│  ├─ camera_session.h/.cpp
│  ├─ capture_worker.h/.cpp
│  ├─ detection_worker.h/.cpp
│  ├─ frame_queue.h/.cpp
│  ├─ image_save_service.h/.cpp
│  ├─ inspection_plc_controller.h/.cpp
│  ├─ inspection_presentation.h
│  ├─ inspection_presentation_renderer.h/.cpp
│  ├─ inspection_runtime.h/.cpp
│  ├─ result_presentation_mailbox.h/.cpp
│  └─ result_service.h/.cpp
├─ startup/
│  ├─ main.cpp
│  ├─ application_startup.h/.cpp
│  ├─ runtime_guard.h/.cpp
│  └─ single_instance_guard.h/.cpp
├─ system_support/
│  ├─ machine_settings_policy.h/.cpp
│  ├─ settings/
│  │  ├─ app_settings.h/.cpp
│  │  └─ app_settings_store.h/.cpp
│  ├─ license/license_codec.h/.cpp
│  ├─ logging/application_logger.h/.cpp
│  └─ crash/
│     ├─ windows_crash_handler.h/.cpp
│     └─ windows_crash_stack.h/.cpp
└─ ui/
   ├─ main_window.ui
   ├─ main_window.h/.cpp
   ├─ main_window_inspection.cpp
   ├─ main_window_settings.cpp
   ├─ controllers/
   │  ├─ operation_ui_policy.h/.cpp
   │  └─ settings_edit_state.h/.cpp
   ├─ dialogs/
   │  ├─ character_template_editor_dialog.h/.cpp
   │  └─ template_selection_dialog.h/.cpp
   ├─ pages/
   │  ├─ inspection_page.h/.cpp
   │  ├─ machine_settings_page.h/.cpp
   │  ├─ template_editor_page.h/.cpp
   │  └─ template_editor_support.h/.cpp
   ├─ presenters/inspection_fault_presenter.h/.cpp
   └─ widgets/image_label.h/.cpp
```

当前统计：148 个 `.h/.cpp`；模板磁盘模块只有 `template_store.h/.cpp` 两个生产文件。

## 3. 程序启动和对象所有权

```text
startup/main.cpp
 → ApplicationStartup::run()
    ├─ RuntimeGuard / SingleInstanceGuard / Logger / CrashHandler
    ├─ AppSettingsStore → SettingsApplicationService
    ├─ TemplateStore（共享唯一实例）
    ├─ CameraDevice / PlcDevice / OCR / Barcode
    ├─ DetectionRegistry → InspectionRuntime → CameraSession
    ├─ InspectionApplicationService
    ├─ TemplateApplicationService
    └─ MainWindow + InspectionPage + MachineSettingsPage + TemplateEditorPage
```

不要在页面或槽函数里再次创建 Store、Runtime 或设备实例。所有长期对象都在启动层建立一次并显式注入。

## 4. 三条核心业务流程

### 4.1 启动检测

```text
MainWindow::on_toolButton_startInspection_clicked
 → InspectionPage
 → InspectionApplicationService::start
 → InspectionStartPreflight::evaluateAccess
 → SettingsApplicationService::current
 → 当前模式 Descriptor
 → TemplateStore::loadPrepared（纸巾不调用）
 → 多模板坏项汇总警告、有效项构成紧凑数组
 → InspectionRuntime::beginStart
 → DetectionRegistry::create
 → CameraSession::startInspection
 → InspectionRuntime::commitStart
```

单模板加载失败直接拒绝；多模板跳过坏项但必须至少剩一个；纸巾只冻结粗糙度阈值。

### 4.2 一帧检测

```text
CameraSession/CaptureWorker
 → InspectionRuntime::acceptFrame
 → DetectionWorker
 → DetectionRegistry executor
 → FramePreprocessor
 → InspectionPositioner
 → 当前模式 Pipeline
 → ResultService
    ├─ 统计
    ├─ PLC结果/延迟剔除
    ├─ ImageSaveService
    └─ ResultPresentationMailbox
 → UI Renderer
```

字库和二维码+三期在 `InspectionPositioner` 中并行评价全部有效定位模板，`TemplatePoseSelector` 只在分数严格更高时替换最佳项。

### 4.2.1 A2 结果与故障边界

正式 `DetectionResult` 只允许 `AlgorithmVerdict::Ok` 或 `AlgorithmVerdict::Ng`，默认值为 `Ng`；`DetectionStatus`、`NotEvaluated` 和取消/故障产品结果均已删除。无定位、ROI 无效、OCR 空文本、二维码不可读或正常超时属于可执行后的普通 NG，仍进入一次统计、存图、PLC 和结果呈现事务，运行状态保持 Running。

无效工作项、定位成功后模板下标越界、执行期 OCR/二维码引擎异常、预处理失败或无效 completion 不生成产品结果，由 `DetectionWorker::failureConsumer` 进入现有 Runtime Fault。队列取消只保留在 Worker/FrameQueue 生命周期层，不进入 `ResultService`。

所有模式均不再发布 ROI 专用警告；`label_runtimeStatus` 只展示既有运行中、停止、Fault、模板制作和存图失败状态。二维码+三期的日期子检测直接读取字库 Pipeline 的 `DetectionResult::verdict`，最终 NG 原因统一来自 `DetectionResult::diagnostic`。

### 4.3 模板选择、编辑和保存

```text
toolButton_selectTemplate
 → TemplateSelectionDialog
    ├─ 当前路径全部预勾选
    ├─ 增加外部文件夹 → TemplateApplicationService::readSummary
    ├─ 单选互斥 / 多选排序
    └─ 确认 → SettingsApplicationService::saveTemplatePaths

当前编辑模板下拉框
 → TemplateApplicationService::beginEdit(path)
 → loadEditable
 → 编辑草稿
 → TemplateApplicationService::save
 → TemplateStore::save(path, draft, preserveExistingContents)
```

“移除模板”只删当前模式路径列表中的一项，再调用 `saveTemplatePaths`；不会调用磁盘删除。

## 5. 每个代码文件的作用

### 5.1 工程和 contracts

| 文件 | 作用与修改注意点 |
|---|---|
| `app/AutoOCRproject.pro` | 主 qmake 清单、Qt/OpenCV/厂商库和部署规则。增删/改名源码必须同步且不得重复。 |
| `app/contracts/barcode_parameter_defaults.h` | 二维码默认格式、外扩、预算和回退开关；默认值只能在这里定义一次。 |
| `app/contracts/detection_mode.h` | 五模式枚举、定位分类和 Descriptor 声明。 |
| `app/contracts/detection_mode.cpp` | 五模式唯一登记表；中文名可改，稳定 ID 不可随意改。 |

### 5.2 application

| 文件 | 作用与修改注意点 |
|---|---|
| `application_result.h` | `ApplicationError/OperationResult`，供 UI 展示结构化错误。 |
| `camera_application_contract.h` | 相机操作 DTO，不含 SDK 类型。 |
| `inspection_ui_contract.h` | 检测 UI 回调和视图绑定 DTO。 |
| `runtime_snapshot.h` | UI 可读取的相机、PLC、运行和当前模板路径快照。 |
| `inspection_start_preflight.h` | 启动 Issue、访问输入、资源输入和结果声明。 |
| `inspection_start_preflight.cpp` | 固定启动拒绝顺序；不重复模板字段校验。 |
| `inspection_application_service.h` | 检测、相机和 PLC 用户用例接口。 |
| `inspection_application_service.cpp` | 读取正式设置、准备模板、控制 Runtime/Camera、映射错误和发布快照。 |
| `settings_application_service.h` | 唯一 `AppSettings current/draft` 和分区保存接口。 |
| `settings_application_service.cpp` | 保证模板路径保存不提交或丢弃整机草稿；写盘成功后才替换正式值。 |
| `template_editor_contract.h` | 初始模板资产与二维码即时校验选项。 |
| `template_geometry_service.h` | 显示几何与转换结果类型。 |
| `template_geometry_service.cpp` | 将 UI 框/点换算为原图 ROI 和相对多边形。 |
| `template_application_service.h` | 模板草稿、读取、保存、批量参数更新和条码校验接口。 |
| `template_application_service.cpp` | 直接持有一个编辑草稿，委托 `TemplateStore`；批量操作先全量预读再逐项事务保存。 |

### 5.3 templates 和设置

| 文件 | 作用与修改注意点 |
|---|---|
| `templates/template_store.h` | `TemplateSettings/EditableTemplate/PreparedTemplate/Summary/Error` 与四个 Store 方法。不要拆出 Repository、Manager 或 Session。 |
| `templates/template_store.cpp` | 严格 JSON、图片/字符加载、两级校验和唯一目录事务保存。任何模板磁盘格式修改都集中在这里。 |
| `system_support/settings/app_settings.h` | 整机扁平字段、固定五模式 `DetectionSchemes` 和默认/相等接口。 |
| `system_support/settings/app_settings.cpp` | 默认值、枚举 ID、路径规范化与单/多/无模板路径规则。 |
| `system_support/settings/app_settings_store.h` | Schema 2 加载状态、错误和统一 Store 接口。 |
| `system_support/settings/app_settings_store.cpp` | 严格分区 JSON 读写、完整约束和 `QSaveFile` 原子提交。 |
| `system_support/machine_settings_policy.h/.cpp` | 相机/PLC当前状态下可恢复的整机默认字段；必须保留检测方案。 |

### 5.4 detection 公共能力与装配

| 文件 | 作用与修改注意点 |
|---|---|
| `detection_registry.h` | 运行准备、创建请求和 Detection Executor 接口。 |
| `detection_registry.cpp` | 五模式唯一 Pipeline 装配；运行资源只来自快照。 |
| `multi_template_runtime_snapshot.h` | 字库和二维码＋三期的多模板定位条目及同下标运行配置。 |
| `multi_template_runtime_snapshot.cpp` | 从有效 `PreparedTemplate` 构建紧凑同序运行数组、复制 `targetUnits` 并预编译字符模板。 |
| `common/frame_preprocessor.h/.cpp` | 旋转、通道转换等统一帧预处理。 |
| `common/detection_roi_geometry.h` | ROI/多边形几何内联工具。 |
| `common/character_template_matcher.h/.cpp` | 字符模板预编译和字形匹配；各模式不要复制。 |
| `common/template_pose_selector.h/.cpp` | 多模板最高分选择。 |
| `common/detection_pose.h` | `FrameData/ProductKey/DetectionResult/Overlay/Pose/WorkItem` 高影响合同。 |
| `common/tracking_pose_matcher.h/.cpp` | 单个定位模板初始化、帧准备和匹配。 |
| `common/inspection_positioner.h/.cpp` | WholeFrame、SingleTemplate、MultipleTemplates 统一定位，多模板使用 `cv::parallel_for_`。 |

### 5.5 detectionmode 五种模式算法

| 文件 | 作用与修改注意点 |
|---|---|
| `detectionmode/stamp/overlap_detector.h/.cpp` | 刚印环定位、区域变换和刚印/日期重叠判断；纯内存算法。 |
| `detectionmode/stamp/stamp_detection_pipeline.h/.cpp` | 刚印字符检测、重叠结果和统一输出。 |
| `detectionmode/word/word_detection_pipeline.h/.cpp` | 字库字符匹配、文字组合、Overlay 和 OK/NG。 |
| `detectionmode/ocr/ocr_detection_pipeline.h/.cpp` | 调用 OCR Engine、目标比较和结果转换。 |
| `detectionmode/tissue/tissue_roll_detector.h/.cpp` | 纸巾纹理/粗糙度算法；阈值由构造参数提供。 |
| `detectionmode/tissue/tissue_detection_pipeline.h/.cpp` | 纸巾算法适配为统一 `DetectionResult`。 |
| `detectionmode/barcode_word/barcode_word_detection_pipeline.h/.cpp` | 二维码优先解码、日期字符检测、策略状态和综合判定。策略状态跟随运行模板条目。 |

### 5.6 runtime

| 文件 | 作用与修改注意点 |
|---|---|
| `inspection_runtime.h/.cpp` | 正式 Run 唯一所有者、状态机、工作线程、Fault 和不可变运行上下文。 |
| `camera_session.h/.cpp` | 相机预览与正式采集会话、曝光调整和帧提交。 |
| `capture_worker.h/.cpp` | 采集线程循环与协作停止。 |
| `frame_queue.h/.cpp` | 有界线程安全帧队列。 |
| `detection_worker.h/.cpp` | Detection Executor 工作线程和提交结果。 |
| `result_service.h/.cpp` | 一次产品最终结果、统计、PLC、存图和 UI 投递的唯一收口点。 |
| `inspection_plc_controller.h/.cpp` | PLC 连接、工艺参数、结果脉冲和延迟剔除队列。 |
| `image_save_service.h/.cpp` | 按运行快照保存原图/标注图。 |
| `result_presentation_mailbox.h/.cpp` | 后台到 UI 的有界完成邮箱。 |
| `inspection_presentation.h` | UI 呈现的纯数据结构。 |
| `inspection_presentation_renderer.h/.cpp` | 将 Detection 结果变成文字、颜色和图像显示。 |

### 5.7 devices

| 文件 | 作用与修改注意点 |
|---|---|
| `camera/camera_device.h` | 相机稳定抽象接口。 |
| `camera/vendor/hikvision_camera_device.h/.cpp` | 海康 MVS 适配；SDK 类型只能留在 vendor。 |
| `plc/plc_device.h` | PLC 稳定字节读写接口。 |
| `plc/vendor/snap7_plc_device.h/.cpp` | 将稳定接口适配到 Snap7。 |
| `plc/vendor/snap7.h/.cpp` | 第三方 Snap7 实现，修改应视为供应商代码变更。 |

### 5.8 engines

| 文件 | 作用与修改注意点 |
|---|---|
| `barcode/barcode_types.h` | 条码输入选项、状态和读取结果。 |
| `barcode/barcode_decoder.h` | 条码引擎稳定接口。 |
| `barcode/vendor/barcode_decoder_api.h` | DLL ABI 声明。 |
| `barcode/vendor/barcode_decoder_adapter.h/.cpp` | DLL 动态加载、调用和错误映射。 |
| `ocr/ocr_engine.h` | OCR 稳定接口。 |
| `ocr/vendor/paddle_ocr_engine.h/.cpp` | PaddleOCR 适配和模型配置。 |
| `ocr/vendor/paddle/include/clipper.h`、`src/clipper.cpp` | 文本框多边形裁切几何。 |
| `ocr/vendor/paddle/include/config.h`、`src/config.cpp` | PaddleOCR 配置读取。 |
| `ocr/vendor/paddle/include/ocr_cls.h`、`src/ocr_cls.cpp` | 文字方向分类器。 |
| `ocr/vendor/paddle/include/ocr_det.h`、`src/ocr_det.cpp` | 文本检测器。 |
| `ocr/vendor/paddle/include/ocr_rec.h`、`src/ocr_rec.cpp` | 文本识别器。 |
| `ocr/vendor/paddle/include/postprocess_op.h`、`src/postprocess_op.cpp` | 检测后处理。 |
| `ocr/vendor/paddle/include/preprocess_op.h`、`src/preprocess_op.cpp` | 推理预处理。 |
| `ocr/vendor/paddle/include/utility.h`、`src/utility.cpp` | PaddleOCR 公共图像和文字工具。 |

### 5.9 system_support 和 startup

| 文件 | 作用与修改注意点 |
|---|---|
| `license/license_codec.h/.cpp` | 授权信息编码/校验。 |
| `logging/application_logger.h/.cpp` | 应用日志目录和 Qt 日志接管。 |
| `crash/windows_crash_handler.h/.cpp` | Windows 未处理异常接入。 |
| `crash/windows_crash_stack.h/.cpp` | Windows 调用栈解析。 |
| `startup/main.cpp` | 唯一 `main()`。 |
| `startup/application_startup.h/.cpp` | 设置重置提示、所有对象装配、页面组合和事件循环。 |
| `startup/runtime_guard.h/.cpp` | 运行库/DLL环境前置检查。 |
| `startup/single_instance_guard.h/.cpp` | 单实例互斥。 |

### 5.10 ui

| 文件 | 作用与修改注意点 |
|---|---|
| `main_window.ui` | 固定窗口布局和静态控件；对象名改动必须同步自动槽和绑定。 |
| `main_window.h` | MainWindow组合接口、自动槽和页面指针。 |
| `main_window.cpp` | UI初始化、三页面绑定/回调和组合根薄转发。 |
| `main_window_inspection.cpp` | 检测/相机/PLC操作槽、操作状态应用、错误提示和悬停说明。 |
| `main_window_settings.cpp` | 设置保存/恢复/清空、模式显隐、参数应用和模板页薄转发。 |
| `controllers/operation_ui_policy.h/.cpp` | 唯一按钮权限矩阵和状态文字。 |
| `controllers/settings_edit_state.h/.cpp` | 整机和模板未应用项记录。 |
| `dialogs/template_selection_dialog.h/.cpp` | 当前路径预勾选、添加、异常状态、单选互斥、多选排序和确认保存。 |
| `dialogs/character_template_editor_dialog.h/.cpp` | 字符框绘制、排序、命名、预览和字符图片结果。 |
| `pages/inspection_page.h/.cpp` | 主检测页面显示、启动/停止交互和状态。 |
| `pages/machine_settings_page.h/.cpp` | 整机设置控件绑定、验证、dirty 和硬件依赖权限。 |
| `pages/template_editor_page.h/.cpp` | 模板取景、冻结、选择、当前编辑项、移除引用、新建/覆盖、字符编辑和批量更新，集中在一个实现文件以减少分散。 |
| `pages/template_editor_support.h/.cpp` | 模板页少量无状态转换辅助。 |
| `presenters/inspection_fault_presenter.h/.cpp` | Fault 文案和恢复提示。 |
| `widgets/image_label.h/.cpp` | 图像显示、定位矩形、二维码矩形、日期多边形和刚印区域交互。 |

## 6. 常见修改指南

### 6.1 修改模式中文名

只改 `contracts/detection_mode.cpp` 的 `displayName`。不要改 `modeId/uiId`，除非同时执行持久化 Schema 变更。

### 6.2 新增模式

1. 扩展 `DetectionMode` 和唯一 Descriptor。
2. 明确 WholeFrame、SingleTemplate 或 MultipleTemplates。
3. 在 `DetectionSchemes` 增加固定字段和严格 JSON。
4. 确定 `TemplateSettings` 字段/资源，或明确无模板。
5. 在 `DetectionRegistry` 装配 Pipeline。
6. 更新 UI 显隐、启动失败路径、数据 Schema 和功能表。

不要创建第二张模式表或通用动态模式框架。

### 6.3 新增模板私有参数

1. 加入 `TemplateSettings`。
2. 在 `template_store.cpp` 同步写出、严格读入、默认、编辑校验和运行校验。
3. 找到唯一算法消费者，运行时从 `PreparedTemplate` 读取。
4. 更新数据 Schema 和对应 UI。

### 6.4 新增整机参数

1. 加入 `AppSettings` 和唯一默认值。
2. 同步 `AppSettingsStore` 的 JSON、严格键、范围和相等比较。
3. 在 MachineSettingsPage 登记控件、dirty 和硬件依赖。
4. 只通过 `SettingsApplicationService::applyDraft()` 保存。

### 6.5 修改模板保存

只能修改 `TemplateStore::save()` 的同一流程。必须保持：编辑继承完整旧目录；同名覆盖从空目录构建；正式目录替换前先备份；失败恢复；未知附加文件在编辑时保留。

### 6.6 修改多模板匹配

重点检查：

- `MultiTemplateRuntimeSnapshotBuilder` 是否仍生成紧凑同序的 `trackingTemplates` 与 `runtimeConfigs`；
- `InspectionPositioner` 是否仍评价全部模板；
- `TemplatePoseSelector` 是否只在严格更高分时替换；
- Registry 是否用命中下标访问同一检测条目。

## 7. 禁止重新引入的复杂度

- 不建立 TemplateRepository、TemplateManager、TemplateCatalog 或编辑 Session。
- 不建立独立 DetectionScheme 文件、Store 或 Service。
- 不把一个模板的参数拆成多个 JSON/YAML。
- 不为新建、更新、同名覆盖各写一套保存逻辑。
- 不持久化模板索引，不建立模板索引 Map。
- 不让 Runtime/Pipeline 在运行中读取磁盘。
- 不把 MainWindow 再拆成大量只包含少数转发函数的 `.cpp`。

## 8. 验证顺序

Agent 只执行静态检查：旧符号清零、qmake 文件存在/无重复、include 解析、UI XML、UTF-8、资源差异和 `git diff --check`。用户在 Qt Creator 统一执行：Run qmake、Rebuild、设置重置、五模式显隐、模板选择/批量引用移除、新建/覆盖、坏模板跳过、并行最高分、纸巾阈值和完整生产回归。

在用户确认前，阶段 8 状态保持“迁移中”，不得提交。
