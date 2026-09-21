# OCRGangYin 现有功能对照表

> 原始功能基线版本：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`；2026-08-16 新终局治理基准：`8595cb2`。本表按当前生产源码、UI、工程、资源和脚本人工反向核对；不以目标架构推测现有行为。运行 Fault 保留已经完成正式结果收口的产品，尚未收口的产品只计入停止摘要中的未确认数量。

## 状态与证据约定

- `待盘点`：尚未追踪完整调用链。
- `已基线`：已记录当前输入、输出、副作用和可重复验证方法；若证据写“U”，仍需用户提供实际运行值，Stage 0门禁尚不因此自动通过。
- `迁移中`：本阶段实现已接入或正在接入，但尚未通过本轮用户集中门禁；该状态不表示允许保留计划内旧路径。
- `已验证`：新路径通过约定验证，旧路径可进入引用清理。
- `已延期`：升级计划明确延期，现有实现保持原位。
- `已确认删除`：用户明确同意，且已记录影响。
- 证据缩写：`S`=已在基线HEAD完成源码/UI/工程静态核对；`T`=已有离线测试源码、等待Qt Creator执行；`U`=等待用户从原入口、真实设备或固定样本确认；`P`=升级计划明确延期。

## 软件设置重建入口（2026-09-09，恢复默认设置已确认删除）

软件设置页只保留“清空当前软件数据”。确认后删除当前 Windows 用户的 `settings/app_settings.json` 并退出，下一次启动使用完整默认配置；产品模板、识别图片、授权文件和日志不在删除范围内。在线“恢复默认设置”的按钮、连接、权限、声明、实现和专用日志已全部删除，不保留等价入口或转发层。

## 左侧导航与页面抽屉（2026-09-07，代码实施完成待统一验证）

本节是当前主窗口 UI 事实，覆盖后文历史条目中关于右侧导航、固定页面宽度、旧 Splitter 状态和旧判定栏视觉的描述。

| 范围 | 当前唯一正式路径 | 已删除的旧路径 | 待用户统一验证 |
|---|---|---|---|
| 导航与抽屉 | 80px 左侧导航栏控制同一个五页抽屉；抽屉与主画面由 `splitter_leftDrawerMain` 承载 | 右侧方向对象名、方法名和 QSS 选择器均已删除，不保留别名或转发层 | 五入口打开、切换、再次点击收起及故障自动停止后保持当前抽屉 |
| 宽度保存 | 正常关闭时用 `QSplitter::saveState()` 保存到 `AppSettings::leftDrawerSplitterState`，JSON 只写 `ui.leftDrawerSplitterStateBase64` | 不连接 `splitterMoved`，不保存独立宽度，不迁移或兼容读取 Schema 7 | 首次 400px、拖动及重启恢复 |
| 文字显示 | `label_recognitionText` 和四个统计值使用 20px；识别结果允许任意字符换行，目标文字使用 `WrapAtWordBoundaryOrAnywhere` | 无第二份识别结果，不改变原始检测文字 | 长二维码、序列号和目标文字不再撑出页面横向滚动条 |
| 判定栏 | 闲置为空且与主画布同为 `#202830`；`VerdictResultLabel` 将 OK/NG SVG 按判定栏当前宽度的 70% 矢量绘制并水平、垂直居中 | 不显示等待、正确或错误文字；不使用 `QPixmap` 缩放或 `verdict` 动态属性 | 首次启动、视图清空、OK/NG、布局缩放和故障自动停止 |

主程序已通过 qmake、MSVC x64 Release 编译、链接和运行库部署；真实交互、重启状态和长文本效果等待用户统一验证。

## 二维码结果本机 CSV 直写（2026-09-03，代码实施完成待统一验证）

二维码+三期的结果输出使用主程序本机每日 CSV 直接追加；`tools/result_receiver/` 独立维护。

| 范围 | 当前正式路径 | 当前行为 | 待用户统一验证 |
|---|---|---|---|
| 设置与 UI | 严格 AppSettings Schema 8 使用 `barcodeCsv.enabled/outputDirectory`；二维码+三期页只保留启用、只读目录和选择目录 | 运行期间按统一权限规则禁用修改 | Schema 7 整体重置、目录与启用状态保存/恢复、未选目录时拒绝启用 |
| 启动与运行配置 | 启用时开始识别只调用 `QDir::mkpath()` 准备目录；本轮配置冻结开关和目录 | 目录准备不创建测试文件或写探针 | 目录可创建/不可创建、其他四模式无 CSV、运行中控件禁用 |
| 正式结果事务 | `ResultService::process()` 在 `claimResult()` 后仅对整体 OK 按本机日期追加 `qr_results_YYYYMMDD.csv`；整体 NG 不创建、不打开且不追加 CSV 文件；随后完成存图任务提交、当前 PLC 合同和结果呈现，运行未进入 Fault 时才提交统计并调用 `finalizeResultClaim()` | 每件整体 OK 的已完成正式结果只追加一次；后续步骤失败时本件计为未确认 | OK 二维码原文、NG 零写入、BOM、无表头、转义、逐行 flush、同日续写、跨日新文件及后续步骤故障收口 |
| 失败与关闭 | 打开、写入或 flush 失败记录 ERROR 并进入 `BarcodeCsvUnavailable` 通用 Fault；自动停止后回到 Idle | 关闭流程不保留待发送数据 | 首件和运行中写入失败自动停止，统计/PLC/呈现停止，未完成产品记为未确认 |

Agent 已完成旧业务符号、旧网络依赖、工程清单、UI/翻译 XML、UTF-8、末尾换行、`git diff --check` 和接收端零差异静态检查；未运行 qmake、构建、测试程序或主程序。

## 正式产品失效安全 A2（2026-08-30，代码实施完成待统一验证）

A2 的正式产品结果固定为 `Ok/Ng`。无定位、ROI 无效、OCR 空文本、二维码不可读/正常超时均形成普通 NG；执行期引擎异常、有效定位后的非法模板下标、预处理失败和无效 completion 进入现有 Runtime Fault，不伪造产品 NG。`label_runtimeStatus` 保留运行中、停止、模板制作和存图失败等状态。

| A2 范围 | 当前唯一正式路径 | 本轮代码证据 | 待用户统一验证 |
|---|---|---|---|
| 结果契约与五种 Pipeline | `DetectionResult` 仅 `AlgorithmVerdict::Ok/Ng`；五种 Pipeline 明确区分普通 NG 与异常抛出 | A2-1/A2-2 已完成；旧状态符号静态零引用 | Qt Creator qmake、Clean/Rebuild、五模式软触发 OK/NG、无定位/ROI 边界 |
| Registry、ResultService 与 Fault | 合法 completion 只有一个 `process()` 出口；Fault 复用 Worker→Runtime 主链；取消只在队列/生命周期层 | A2-3/A2-4 已完成；无取消结果事务和无 ROI 发布调用 | OCR/二维码引擎异常、模板下标越界、预处理失败、延迟 NG 顺序 |
| ROI 与 UI 警告 | 所有模式 ROI 无效只形成 NG，不更新 `label_runtimeStatus`；存图失败和模板制作提示保持 | A2-5 已完成；ROI 警告符号、信号、方法和主窗口连接均为零 | 软/硬触发连续运行时状态显示，存图失败和故障自动停止不回归 |

本表新增 A2 条目在用户统一验证通过前保持“迁移中/待验证”语义；Agent 未执行 Qt Creator 构建、真实相机、PLC 或机械现场验证。

## 阶段8：产品配方完全替换（2026-08-20，用户统一验证通过）

本轮从 `b7721f0` 开始，彻底删除 ProductRecipe、RecipeStore、配方 UUID、发布/重发和 AppData `recipes` 生产路径，改为唯一 `app_settings.json` 加外部模板文件夹。实际受影响功能为 `SYS-007、SYS-010、UI-001..002、SET-001..013、TPL-001..016、DET-001..008、CAM-003..004、RUN-001..006、RES-005`，共50项。2026-08-20用户确认《OCRGangYin模板方案完全替换计划》第十二节统一验证全部通过，这50项恢复为`已验证`；其他37项继续保留原验证结论，`MC-001..003`继续为`已确认删除`。

下表是阶段8差异的当前事实，覆盖后文各ID行中关于 Recipe、Profile、UUID、发布/重发、旧 INI/YAML 和 MachineSettings 的历史描述；发生冲突时以下表及《OCRGangYin新架构数据Schema.md》为准。

| 影响功能ID | 当前唯一正式路径 | 已删除的旧路径 | 本轮统一验证重点 |
|---|---|---|---|
| `SYS-007、SYS-010、UI-001..002` | `ApplicationStartup`只加载一个 `AppSettingsStore`，注入共享 `TemplateStore`；模式页面根据唯一 `DetectionModeDescriptor` 显示无模板/单模板/多模板 UI | `MachineSettingsStore`、RecipeStore、配方UUID恢复和产品配方选择均已删除 | Run qmake、Rebuild、首次启动/重启恢复、五模式切换、控件显隐和操作状态 |
| `SET-001..012` | 整机、UI和五模式检测方案统一保存在 `app_settings.json`；正式设置与UI草稿仍保持“应用”边界；纸巾阈值直接属于 `detectionSchemes.tissue` | 不再创建 `detection_schemes.json`，也不从旧设置、Recipe或模板目录补整机参数；`SET-013` 在线恢复入口已删除 | 首次启动、保存/重启、损坏整文件重置、清空、未应用参数、相机/PLC/存图设置 |
| `TPL-001..016` | 模板是外部文件夹；`TemplateStore`负责摘要、编辑加载、运行准备和唯一事务保存；统一选择框列出当前路径并以复选框选择批量移除范围；当前编辑模板可切换或单项移除引用，多模板批量操作先全量预验证 | 配方发布、UUID目录、Recipe编辑会话、Profile持久索引、`calibrate_config.yaml`均已删除；引用移除不会删除模板文件夹 | 四种有模板模式的新建/覆盖/编辑/选择/取消/批量引用移除、单多模板约束、损坏跳过、事务失败保护、字符与ROI资源 |
| `DET-001..008、RUN-001..006、CAM-003..004` | 启动时按当前检测方案准备模板；单模板损坏拒绝，多模板逐项警告并跳过；有效模板形成紧凑不可变快照；多模板定位并行计算并取最高分；纸巾直接读取阈值且完全不访问模板 | Runtime和Detector不再持有Recipe/Profile，也不在检测线程读取模板磁盘；不再保存可错位的原始模板索引 | 五模式软/硬触发、单模板拒绝、多模板部分损坏/全部损坏、并行最高分、停止/重启、定位失败和结果收口 |
| `RES-005` | 结果只显示本次紧凑运行快照中实际命中的模板名称；无定位或模式无模板时按既有规则显示空状态 | 不再显示配方名、UUID或持久Profile索引 | 单模板、多模板最高分命中、坏模板跳过和纸巾结果中的模板名显示 |

阶段8只修改代码、工程清单和开发文档，没有修改或删除图片、模型、DLL、翻译、样式、图标、`.qrc`及用户模板资源。Agent不会运行 qmake、编译、测试程序或主程序；运行证据等待用户一次性提供。

## 字符目标解析统一与大小写敏感支持（2026-08-29，代码完成待人工回归）

本专项影响 `TPL-011、TPL-015、DET-002、DET-003、DET-006`。字符模式的目标文字统一由 `TemplateStore::templateTargetUnits()` 单次解析，`A/a` 分别使用 `upper_A/lower_a` 字符资产；字库和二维码＋三期通过 `MultiTemplateRuntimeSnapshot::runtimeConfigs` 传递 `targetUnits`，钢印通过 `StampRuntimeConfig` 传递，三个 Pipeline 不再解析 `targetText`。OCR 仍使用原始 `TemplateSettings::targetText`，字符匹配、阈值、数量判定、定位、二维码、钢印重叠、统计、PLC 和存图行为不变。

2026-08-29 Agent 已完成 qmake 和 MSVC x64 Release 全量编译、链接、部署，静态旧名称与旧解析路径已清零；真实模板文件名、三种字符模式和 OCR 原始目标仍待用户按专项方案第 14 节人工回归。发生状态冲突时，以本节和末尾状态统计为准。

阶段1实际调用链影响范围：`SYS-001、SYS-007..008、UI-001、UI-008..009、SET-001..013、TPL-003..016、DET-002..006、CAM-001、CAM-003..004、CAM-006、RUN-001、RUN-005、PLC-001、PLC-003..004、SAVE-001..003`，共50项。2026-08-17用户确认阶段1最终差异的Run qmake、Rebuild、设置/配方测试和主程序人工回归均无问题，50项已统一恢复为`已验证`。

### 阶段1已验证调用链覆盖（2026-08-17门禁通过）

下表记录阶段1最终差异中已通过统一门禁的正式路径，覆盖后续各ID行保留的阶段0历史基线描述；发生冲突时以下表为当前事实。验证范围包括新设置首启/保存/重启/恢复默认/清空、五模式新配方创建/加载/编辑/资源预检、事务失败保护、旧格式拒绝、相机曝光/增益、模板入口及“基础模板保存不依赖目标字符”的恢复流程。

| 影响功能ID | 当前唯一正式路径 | 已删除/拒绝的旧路径 | 本轮门禁重点 |
|---|---|---|---|
| `SYS-001、SYS-007..008、UI-001、UI-008..009` | `ApplicationStartup`一次加载`MachineSettingsStore`，把同一设置快照、Store和`RecipeStore`注入主窗；模式记忆只保存新配方UUID；软件目录来自新AppData根，分隔条状态属于MachineSettings | `AppSettingsManager`、`GlobalSettings`、旧`settings.ini`和旧设置目录兼容读取已从生产代码删除 | 首启、重启恢复、损坏拒绝、五模式切换/配方恢复、PLC延迟连接与布局恢复 |
| `SET-001..013` | 所有机器参数只由`MachineSettingsStore`事务读写；唯一默认在`MachineSettings`构造函数；曝光/增益属于机器设置，纸巾阈值属于`ProductRecipe`；清空只删除新设置JSON | 旧INI读写、双读取、重复默认值及模板私有设置混存已删除 | 保存/重载、恢复默认、清空、相机开关与PLC连接状态下的dirty语义、存图策略 |
| `TPL-003..005` | 原模板画布和ROI入口保留；保存时由`RecipeEditorSession`组装类型化字段与内部资源清单并交给`RecipeStore` | 不再把画布结果写入旧模板私有INI或外部模板目录 | 四种有模板模式的ROI、二维码即时校验与保存失败提示 |
| `TPL-006..016` | 新建、加载、编辑、单/多Profile、字符资产和发布统一走`ProductRecipe`→`RecipeEditorSession`→`RecipeStore`→`PreparedRecipe`；纸巾保存为零资源配方 | 旧模板选择/装配/草稿/发布并行类型、旧命名目录和旧私有INI读取全部删除；旧目录显式拒绝 | 五模式创建、UUID目录、编辑重发、单/多Profile、损坏/旧格式拒绝与事务回滚 |
| `DET-002..006` | 五模式启动资产只来自不可变`PreparedRecipeSnapshot`；图片和YAML在准备阶段解码校验，运行时不再读外部资源路径 | 检测器按目录自行加载资源、算法失败后fallback到旧模板路径已删除 | 五模式资源预检、目标文本/阈值/ROI/字符模板与二维码资源完整性 |
| `CAM-001、CAM-003..004、CAM-006、RUN-001、RUN-005` | 启动使用已应用`MachineSettings`和本次`PreparedRecipe`快照；模式、相机曝光/增益、旋转、通道、硬触发延时及纸巾阈值均不从启动时UI临时拼装 | 运行启动对旧全局设置、旧模板目录和旧UI参数补缺路径已删除 | 软/硬触发、五模式启停、曝光/增益、旋转/通道、硬触发延时0/非0和缺资源禁止启动 |
| `PLC-001、PLC-003..004` | PLC连接、地址及工艺参数来自本次MachineSettings快照；`InspectionPlcController`使用注入地址图并保持既有写入顺序 | PLC参数不再从旧设置管理器或启动时UI临时解析 | Fake验证连接参数、0/1工作模式、固定地址/大端顺序；不冒充真实PLC现场验收 |
| `SAVE-001..003` | 判定选择、原图/标注图组合、保存目录和JPEG质量92来自MachineSettings运行快照 | 旧设置、旧目录fallback和第二套JPEG默认已删除 | 四类策略、三种内容组合、目录、命名、JPEG 92及事务设置重载 |

阶段2实际调用链影响范围：`RUN-001..003、UI-002、SET-004..005、CAM-001..002、PLC-001..004`，共12项。2026-08-17用户确认本轮Qt Creator统一门禁全部通过，这12项已恢复为`已验证`；真实PLC现场验证仍按各功能行既有记录延期，不以Fake或无PLC验证替代。

### 阶段2已验证调用链覆盖（2026-08-17门禁通过）

| 影响功能ID | 当前唯一正式路径 | 本阶段已删除的旧路径 | 本轮门禁重点 |
|---|---|---|---|
| `RUN-001..003` | `InspectionApplicationService::start/stop`统一完成访问预检、当前UUID配方重新准备、五模式资源预检、`InspectionRunContext`快照建立、运行事务和协作停止；检测状态只来自`InspectionRuntimeController`并通过只读`RuntimeSnapshot`投影给UI | `InspectionStartController`、`InspectionStopController`、两个业务`friend`、`isCollecting`、`m_operationState`、`m_bOpenDevice`、`hasRunningInspectionThread`及停止中的`QCoreApplication::processEvents`兜底已删除 | 五模式启动/停止/重启；快速重复启动只形成一条运行链；停止后可再次启动；线程退出无残留 |
| `UI-002、SET-004..005` | Widget只收集脏参数名称、展示确认/错误并调用应用服务；继续运行时由`SettingsApplicationService`丢弃草稿，按钮状态由`RuntimeSnapshot`和模板编辑状态计算；设置页的保存、默认、清空继续只经设置应用服务写Store | UI不再自行决定检测运行状态，也不直接构造启停控制器；设置页不再持有或写`MachineSettingsStore` | 预检提示顺序、未应用参数取消/继续及原值恢复；运行中关键参数禁用；正常停止与故障自动停止均回到空闲界面 |
| `CAM-001..002` | 打开/关闭命令统一进入`InspectionApplicationService`并返回结构化结果；保持先尝试PLC连接、PLC失败不阻止打开首台相机、曝光越界调整后事务保存以及忙碌时拒绝关闭的既有语义 | 相机开关状态不再由Widget布尔字段保存；开关入口不再自行判定重复运行状态 | 无PLC开相机、相机枚举/打开/曝光失败提示、空闲关闭、检测中和模板制作中拒绝关闭 |
| `PLC-001..004` | 延迟连接、打开相机附带连接、手动连接/断开、触发模式、工艺参数和拍照距离命令统一经`InspectionApplicationService`进入既有Runtime/PLC端口；启动执行使用本次MachineSettings快照下发 | UI入口不再直接调用PLC连接、断开、触发和工艺参数命令；失败不更新已应用值 | Fake验证连接/断开、0/1触发值、固定地址与大端顺序、首错停止；真实PLC和现场时序继续标为待验 |

阶段3实际调用链影响范围：`SYS-009、UI-002..003、SET-006..010、TPL-001..002、DET-001..007、CAM-001..006、RUN-001..006、PLC-001、MC-001..003`，共33项。2026-08-17用户确认阶段3集中门禁验证完成；除`MC-001..003`继续保持`已确认删除`外，其余30项已由`迁移中`恢复为`已验证`。

### 阶段3已完成调用链覆盖

阶段3门禁完成时，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。阶段3差异已经完成Agent静态门禁和用户Qt Creator集中验证；进入阶段4后，以后文阶段4状态为当前状态。

| 当前状态 / 功能ID | 当前唯一正式路径 | 已删除旧路径或保持边界 | 本轮集中门禁 |
|---|---|---|---|
| `已验证`：`SYS-009、UI-002..003、TPL-001..002、CAM-001..003、CAM-005..006` | `ApplicationStartup`构造唯一vendor设备、`CameraSession`和应用服务；相机开关、曝光/增益、模板实时预览/冻结/重取/退出都经应用服务进入同一Session；预览帧不创建`ProductKey` | Widget、TemplateEditorController和TemplateMatch不再持有相机设备或采集线程；旧相机操作/恢复Controller和模板预览线程已删除 | 真实相机开关、曝光/增益；预览、冻结、退出和再次进入；空闲/预览/检测中关闭程序后进程退出 |
| `已验证`：`SET-006..010、DET-001、CAM-003..005、RUN-001、RUN-005..006` | MachineSettings运行快照映射为`FramePreprocessSettings`与`CameraSessionCaptureConfiguration`；软件触发不消费硬触发延时，硬触发把界面ms值换算为µs写入海康`TriggerDelay`；旋转/通道统一进入`FramePreprocessor` | 两个旧采集线程内重复的旋转、通道、软件延时和图像缓冲逻辑已删除；没有UI临时参数或第二套默认 | 硬触发延时0/非0；四种旋转、彩色/红/绿/蓝；软硬触发停止与重启 |
| `已验证`：`DET-002..007、CAM-003..004、RUN-001..004` | `InspectionPositioner`复用迁移后的`detection/common/TrackingPoseMatcher`，对单Profile和多Profile共享同一预处理帧并选择Pose；`CameraSession`只向现有阶段4前Runtime提交正式帧/Pose | `Zhuizong`、根目录TrackingPoseMatcher副本以及MyThread/CameraThread中的两套定位/Profile分发已删除；五种Pipeline、阈值、判定、正常统计和PLC结果链未改 | 五模式软触发；硬触发Fake/现有相机路径；单/多Profile定位；固定样本判定与Overlay不变 |
| `已验证`：`SYS-009、CAM-003..004、RUN-002..006` | 单一`CaptureWorker`仅拥有一个`std::thread`；停止固定执行停止标志→`ICameraDevice::interruptWait()`→`join()`；软触发、硬触发和预览共用该生命周期 | `MyThread`、`CameraThread`、`InspectionAcquisitionController`、`InspectionWorkerConfigurator`、启动/停止/恢复Transition及超时泄漏兜底全部删除；无detach/terminate/放弃所有权 | 连续停止/重启；等待帧时停止；检测中/预览中退出；确认无残留线程 |
| `已验证`：`CAM-001..005、PLC-001` | `devices/camera/vendor/HikvisionCameraDevice`直接调用MVS；保持首台相机、软件TriggerSource=7和硬件stop→200ms→Line0→曝光/增益/TriggerDelay→回调/start→LineDebouncerTime=5000→100ms；停止后按旧合同关闭、100ms、重开软件触发 | `CMvCamera`、旧Hikvision函数表/Native桥、ReadBuffer/latestImage等实现泄漏API已删除；MVS类型只存在vendor目录；PLC仍只保持原连接尝试和正常合同，本阶段不改Fault或PLC结果逻辑 | 真实相机首开/关闭/参数；软触发五模式；硬触发现有路径；停止后相机恢复并可再次启动 |

阶段4实际调用链影响范围：`SYS-008、SYS-010、UI-001..005、SET-005、DET-001..008、CAM-001..002、RUN-001..006、PLC-001..007、RES-001..005、SAVE-001..005`，共41项。范围来自当前唯一生产链的反向追踪：统一Runtime替换会覆盖进程退出、延迟PLC连接、模式类型、运行状态和相机忙碌门禁；五种Pipeline、结果呈现、统计、PLC结果、存图及Fault收口属于同一不可拆分结果事务。2026-08-17用户确认阶段4Qt Creator集中门禁“没问题”，这41项已统一恢复为`已验证`。

### 阶段4已验证调用链覆盖

当前运行链由 `InspectionRuntime`、`ResultService`、`RuntimeSnapshot` 和 `InspectionPresentation` 组成；模式装配和 Profile 快照归 Detection 管理，五种模式共用统一结果入口。下表以当前事实为准：正常算法、PLC 0/49→约100ms→0、存图和 UI 语义保持；Fault 停止新的正式受理，保留已经完成正式结果收口的产品，尚未收口的产品只进入停止摘要中的未确认数量。

| 影响功能ID | 当前唯一正式路径 | 本阶段已删除的旧路径 | 本轮门禁重点 |
|---|---|---|---|
| `SYS-008、CAM-001..002、PLC-001..004` | 延迟连接、相机附带连接、手动连接/断开、触发模式和工艺参数继续由`InspectionApplicationService`命令进入唯一`InspectionRuntime`与`InspectionPlcController`；启动参数只取本次MachineSettings快照 | `InspectionRuntimeController`及UI侧启动PLC参数编排已删除；Widget不再持有PLC结果脉冲状态 | 无PLC不阻止主窗/开相机；Fake连接、断开、0/1、固定地址/大端顺序；真实PLC仍不冒充验收 |
| `SYS-010、UI-001..003、SET-005` | 五模式使用稳定`DetectionMode`；运行状态与只读`RuntimeSnapshot`来自唯一Runtime；结果图由Runtime内纯渲染器生成，再由UI绑定应用；运行中硬件参数门禁继续查询同一状态 | 旧模式整数Worker工厂、UI目录反向依赖、第二套结果状态和运行Controller查询已删除 | Release Run qmake/Rebuild；五模式控件/图像/Overlay；运行中参数禁用和退出释放 |
| `UI-004..005、RES-001..005` | 每个 `ProductKey` 只允许一次 `ResultService` 事务：完成 CSV、存图步骤和 PLC 合同，形成完整 `InspectionPresentation` 并经容量 1 邮箱投递；运行未进入 Fault 时提交统计并最终收口 | `InspectionResultCoordinator`、`DetectionCompletionController`、旧 ResultHandler、分散五模式收尾和兜底计数展示已删除 | 五模式图文、模板名、判定、耗时和统计属于同一产品；重复结果只收口一次；故障产品只计入停止摘要中的未确认数量 |
| `DET-001..008` | `DetectionRegistry`按PreparedRecipe和唯一`DetectionModeDescriptor`选择纸巾、钢印、字库、OCR或二维码+三期Pipeline；全部Worker容量1并把同一种`DetectionCompletion`交给唯一ResultService | Runtime旧`PipelineRegistry`、五种结果Consumer/收尾函数、`DetectionModeWorkerFactory`、`DetectionSession`及Widget中的模式装配/结果出口已删除；算法失败不切换其他算法 | 五模式固定样本与资源；定位失败仍形成既有NG；系统故障与产品NG严格分离 |
| `RUN-001..006` | 每次启动只创建一个不可变`InspectionRunContext`，冻结runId、开始时间、MachineSettings、PreparedRecipe和Profile；Runtime唯一拥有Worker、产品账本、Fault和容量1呈现邮箱；停止按采集→Worker→相机恢复协作完成，故障后自动回到Idle | 单一运行会话和状态边界 | 五模式启停/重启、软硬触发背压、重复结果、Fault首因、停止后线程退出及下一次预检 |
| `PLC-005..007` | 正常OK写0；正常NG写49并由ResultService约100ms后写0；延迟NG队列携带原始ProductKey且只输出一次；任一写失败进入Fault并停止新的正式受理 | PLC 输出由 ResultService 与运行控制器统一编排 | Fake锁定0、49→0、延迟位置、重复去重和写失败自动停止；不声称机械剔除成功 |
| `SAVE-001..005` | ResultService按运行设置快照每产品最多提交一个`ImageSaveTask`；`ImageSaveService`保持容量32、两个写线程、队满阻塞且不丢任务，原图/标注图仍来自同一帧 | UI侧保存模式判断、任务拼装和结果协调器私有存图服务已删除 | 四种策略、三种图像组合、OCR同帧原图、JPEG 92、不可写报警和慢盘反压 |

正式功能当前状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。2026-08-18用户确认Runtime/Recipes认知精简S8统一门禁通过；本轮实际调用链影响`SYS-007、SYS-010、UI-001..002、SET-003、SET-008..010、TPL-001..016、CAM-003..004、RUN-001..006`共32项，已全部从`迁移中`恢复为`已验证`。其余55项持续保持`已验证`；真实PLC、机械剔除和现场恢复仍单列待验。
| `已确认删除`：`MC-001..003` | 无入口、无类型、无底层API | `MultiCameraWidget/Controller/Unit/SyncManager/Provider/Types`、隐藏按钮、UI文件及qmake项已全部删除，不留兼容桥或未来扩建API | 主界面无多相机入口；源码/UI/qmake旧符号零引用 |

阶段5实际调用链影响范围：`SYS-007、SYS-009、UI-001..003、UI-006、SET-003..006、SET-008..010、TPL-001..016、DET-002..006、CAM-001..002、CAM-006、RUN-001..002`，共39项。范围来自模板制作按钮、相机预览/冻结/退出、ImageLabel绘图、二维码即时验证、坐标换算、字符裁切、单/多Profile编辑、同UUID事务重发、模式记忆、启动资源预检、相机关闭门禁和程序退出的真实调用链。阶段5门禁通过前，这39项统一保持`迁移中`。

### 阶段5迁移中调用链覆盖

| 影响功能ID | 当前唯一正式路径 | 本阶段已删除的旧路径 | 本轮门禁重点 |
|---|---|---|---|
| `SYS-007、UI-001、SET-003..004、TPL-008..010、TPL-016` | `TemplateApplicationService`统一开始新建/编辑、加载已发布配方、同UUID重发、模式记忆和只读编辑DTO；`TemplateEditorPage`只展示当前草稿并调用命令 | `TemplateEditorController`中的目录选择、Store直写、会话与模式记忆所有权，Widget业务`friend`和模板状态穿透 | 五模式切换/恢复、已发布配方选择、单/多Profile、同UUID重发、重启恢复 |
| `SYS-009、UI-002、TPL-001..002、CAM-001..002、RUN-002` | 页面通过`InspectionApplicationService`执行模板预览命令并持有纯UI预览状态；停止固定回到Idle，退出/关闭相机/关闭程序均先结束预览 | Widget中的模板采集状态、预览会话和模板制作业务编排 | 预览、冻结、重拍、Esc退出、关闭相机门禁、预览中退出程序零残留线程 |
| `UI-003、UI-006、TPL-003..007` | `TemplateGeometryService`统一显示/原图坐标、定位/日期/二维码ROI、裁剪与标定资源；页面负责鼠标绘图、中文引导和二维码即时验证结果展示 | Controller中的重复缩放/坐标/ROI/YAML拼装和对Widget几何缓存的写入 | 定位矩形、二维码框即时验证、日期多边形闭合、钢印环、边界裁剪与重新取景 |
| `TPL-006、TPL-011..015` | `RecipeAssetService`组织原图、定位模板、校准YAML和字符图；`CharacterTemplateEditorDialog`只返回字符框、名称与图像；应用服务在完整校验后调用`RecipeEditorSession`事务发布 | 旧字符对话框直接持久化语义、Controller内字符文件组织、单项/批量编辑的Store直写 | 字符切割/命名、单项与批量目标字符/阈值、多Profile资源完整性、失败时正式配方不变 |
| `SET-005..006、SET-008..010、DET-002..006、CAM-006、RUN-001` | 页面只读取设置与PreparedRecipe只读快照；曝光、旋转、通道和纸巾阈值分别保持既有归属，发布后仍由五模式启动资源预检验证 | 模板页面对运行算法缓存、TemplateMatch和可变机器设置的反向写入 | 运行中禁用、曝光/旋转/通道预览、纸巾无资源配方、五模式发布后启动资源预检 |

## 1. 启动、系统保护与部署

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SYS-001 | 应用启动与本地化 | 双击主程序 | 部署目录完整 | `startup/main.cpp::main`→`ApplicationStartup::run`→安装翻译→组装相机与PLC控制器→注入`Widget`→最大化显示 | Qt资源、具体设备实现和运行控制器；启动生效 | 主窗最大化、中文UI和浅蓝样式；资源缺失时对应翻译/样式不加载 | 创建QApplication、设备组合和主窗 | 启动外观、窗口状态和文本均需保持 | `startup/`+`ui/` | 最终架构启动组装 | Release启动，核对窗口、中文按钮、禁用态样式 | 已验证 | S；U；2026-08-15用户确认主工程Run qmake、Rebuild、Run及无PLC启动均正常 |
| SYS-002 | 授权校验 | 启动及每24小时定时器 | 可执行文件旁有`license.ini` | `ApplicationStartup::run`→`RuntimeGuard::check`→`LicenseCodec`统一XOR/SHA256解密→解析`expires` | 当前日期；固定密钥；启动和24h周期生效 | 有效则继续；缺失、格式错或过期弹模态错误并退出 | 读取授权文件；创建24h定时器 | 失败必须阻止继续运行 | `startup/runtime_guard.*` | 保留 | 分别使用有效、缺失、损坏、过期授权启动；记录提示与退出 | 已验证 | S；U |
| SYS-003 | 单实例 | 第二次启动 | 首实例共享内存`ecust`仍存在 | `ApplicationStartup::run`→`SingleInstanceGuard::acquire`→`QSharedMemory::create(1)` | 固定键`ecust`；启动生效 | 首实例继续；第二实例提示“程序已经运行”并退出 | 创建进程间共享内存 | 同一用户会话只允许一个实例 | `startup/single_instance_guard.*` | 保留 | 连续启动两次，第二次提示且不出现第二主窗 | 已验证 | S；U |
| SYS-004 | 日志写入与保留 | 启动后任意INFO/WARN/ERROR/FATAL日志 | 可执行目录可写 | `ApplicationStartup::run`→`ApplicationLogger::start`→Qt全局消息处理器→同一UTF-8行直接写文件和`stderr` | `<exe>/logs/ShengYin_yyyyMMdd_HHmmss_zzz_pid_part.log`；32 MiB分卷；正常日志60天且总量1024 MiB | 本地毫秒时间（无时区）、级别、六个固定类别、线程ID和紧凑业务字段；正常帧只保留`mode/result/ms`及按需最终字段，不显示逐帧累计统计、RunId、sequence和帧号；Run停止集中记录计数和累计统计；不输出DEBUG，不访问旧`<exe>/log` | 创建会话文件；只删除严格匹配的新格式日志；启动和换卷前清理 | INFO不逐条刷新；WARN/ERROR/FATAL立即刷新；文件失败后仅保留`stderr` | `system_support/logging/`及当前业务日志边界 | 完全替换旧日志实现 | Release启动和代表操作后核对文件/Qt Creator双输出、UTF-8、紧凑单帧摘要、完整停止统计、分卷与保留规则，确认旧目录不变 | 迁移中 | S；首轮Release已确认写入和UTF-8文件中文；第二轮跨层去重、高频收口、模板最终结果和设置实际值已实施且静态门禁通过；U待统一Release复验 |
| SYS-005 | 崩溃记录 | Windows未处理异常 | `<exe>/logs`可写 | `ApplicationStartup::run`→`WindowsCrashHandler::install`→`WindowsCrashStack`→独立`crash_yyyyMMdd_HHmmss_zzz_pid.txt` | Windows异常上下文；崩溃文件独立保留60天且不计入正常日志1024 MiB | 尽力写独立崩溃文件后保持异常终止；不访问正常日志文件或互斥锁 | 创建崩溃文件；只删除严格匹配且超过60天的崩溃文件 | 不增加恢复、minidump、符号化或额外容量算法 | `system_support/crash/` | 完全替换同文件追加 | 不制造生产崩溃；仅按批准的隔离方式检查文件命名和异常文本 | 迁移中 | S；静态门禁通过；U待隔离验证 |
| SYS-006 | OCR模型初始化 | OCR检测运行启动 | `config_ocr.txt`、PP-OCRv6 tiny DET/REC、官方字典和 Paddle Inference 3.0.0 运行库存在 | `ApplicationStartup::run`向`DetectionRegistry`提供OCR配置路径→`DetectionRegistry::create`同步创建`DeepOcrEngine`→公共构造过程创建并预热DET/REC predictor→成功后创建检测Worker并启动采集 | CPU、oneDNN、4线程；DET参数960/0.2/0.4/1.4；配置相对路径以配置文件目录解析；每次检测运行生效 | predictor创建或预热异常沿启动失败链返回，不创建Worker且不启动采集；逐帧推理异常进入Runtime Fault | 本次运行的OCR执行器持有引擎，检测线程独占使用，Worker释放后释放引擎 | 配置、模型、字典和运行库必须成套部署 | `detection/`+`engines/ocr/` | PP-OCRv6 tiny与启动生命周期 | 有效部署启动检测并执行代表帧；验证初始化失败不采集；启动、停止后再次启动 | 代码已实施，待用户统一验证 | S；每次运行启动前同步创建并预热引擎，待用户Qt Creator构建和主程序统一验证 |
| SYS-007 | 公共设置与模板恢复 | `Widget`构造 | 用户AppData可读 | `Widget::loadSettings`→`AppSettingsManager::loadGlobalSettings`→`applyGlobalSettingsToUi`→`restoreTemplatesForMode` | `AppDataLocation/settings.ini`，配置v2；无效时默认 | 恢复模式、保存、相机/PLC、模板历史和分隔条；读失败使用默认并记日志 | 读取设置和模板资源 | 详见SET/TPL功能ID | `recipes/`+`system_support/settings/` | 保留后拆分 | 修改并应用设置、退出重启，逐项核对；损坏INI核对默认回退 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过；2026-08-18用户确认S8统一门禁通过 |
| SYS-008 | 启动PLC延迟连接 | 主窗构造后1秒 | 运行控制器持有PLC控制器 | `Widget`保留原1秒定时入口→`InspectionRuntimeController::connectPlc`→类型化PLC控制器→Snap7 | 保存的IP/Rack/Slot；1秒后生效 | 成功连接并刷新硬件控件；失败仅日志/状态，不阻止主窗 | 建立PLC网络连接 | 延迟和提示保持 | `runtime/inspection_plc_controller.*`+UI命令桥 | 最终架构PLC边界收口 | 有PLC/无PLC各启动一次，记录1秒后状态和可编辑控件 | 已验证 | S；T；U；2026-08-15用户确认无PLC连接失败不影响主窗；真实PLC成功连接继续延期 |
| SYS-009 | 正常退出与资源释放 | 关闭主窗/进程退出 | 可有运行线程、相机、PLC | `Widget::closeEvent`/析构→协作停止线程/关相机→运行控制器断PLC；`ApplicationStartup`从事件循环正常返回并析构启动组合 | 线程等待上限和当前设备状态 | 正常关闭；线程未及时退出仅记录警告，不调用`terminate()` | 停线程、关设备、断PLC、释放OpenCV窗和启动组合 | 必须保持协作停止、无残留线程 | `runtime/`+`startup/`+`devices/` | 最终架构设备所有权收口 | 检测中、模板预览中、空闲时分别关闭；确认进程退出和设备释放 | 已验证 | S；T；U；2026-08-15用户确认软触发停止及关闭主窗口后进程正常退出 |
| SYS-010 | Release运行时部署校验 | Qt Creator Release链接后 | `dist/ShengYin`完整 | `AutoOCRproject.pro::QMAKE_POST_LINK`→`system_support/deployment/deploy_runtime.ps1` | 源`dist/ShengYin`、目标构建`release`；Release链接后 | 清理目标中的旧OCR资源，复制并检查`config_ocr.txt`、V6 tiny模型、字典和Paddle运行库；必需文件缺失或配置路径不匹配时失败 | 写Release运行目录 | 保持部署可复现；构建只由用户执行 | `system_support/deployment/` | 同步V6 tiny运行资产 | Qt Creator Run qmake+Release Rebuild，核对部署文件集合和脚本报错 | 代码已实施，待用户统一验证 | S；部署脚本已完成静态核对；待Release链接后实际执行验证 |

## 2. 主界面与交互

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UI-001 | 模式与参数页面 | “识别模式”下拉框 | 非检测/非模板忙碌状态 | `comboBox_4::currentIndexChanged`→`setupDetectModeChangeTracking`→存旧模式路径→`restoreTemplatesForMode`→更新可见参数；本切片统一钢印与字库家族的当前模板编辑区并恢复钢印字符切割入口 | 界面固定顺序：模板匹配、字库匹配、深度模型、纸巾检测、二维码+三期；内部ID依次为stamp/word/ocr/tissue/barcode_word | 切换对应参数和模板历史；钢印显示单Profile编辑选择及字符切割；无效历史保留空状态并可提示 | 保存当前模式和模板历史 | 五个现有入口不可丢失，钢印不能退化为仅重叠检测 | `ui/main_window/settings/` | 保留 | 依次切换五模式，核对控件显隐、模板名、历史恢复；钢印编辑区与字库家族布局一致 | 已验证 | S；U；2026-08-13用户确认钢印编辑区、字符裁切入口及两种单模板模式切换均正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| UI-002 | 常驻工具栏、模板管理、操作状态与设备状态 | 相机开关、识别启停、参数页模板操作、相机/PLC状态或硬触发变化 | 主窗口活动 | `updateOperationUiState()`读取一份`RuntimeSnapshot`→`OperationUiPolicy`计算统一相机、识别、模板操作权限→同一快照更新按钮和相机/PLC圆点；`TemplateEditorPage`直接应用参数页五个模板按钮状态；运行故障对外映射为`Stopping`，自动停止完成后回到`Idle` | `CameraClosed/CameraReady/TemplatePreviewing/TemplateFrozen/Detecting/Stopping`；工具栏74px、相机/识别按钮76×62px；参数页模板按钮固定3+2，第一行制作、保存、分割字符，第二行选择、退出模板制作，退出按钮108×62px | 只允许当前状态合法动作；相机和识别各一个状态化按钮，模板管理提供选择、制作、保存、分割字符和独立退出；相机/PLC状态与快照一致 | 控件文字、图标、enable和`connectionState`样式变化；硬触发立即保存 | 左侧抽屉、图像画布和业务状态机保持 | `ui/main_window/main_window*`+`settings/detection_settings_page.ui`+`template/template_editor_page.*`+`operation_ui_policy.*`+`machine_settings_page.*` | 常驻工具栏、设备状态与模板管理迁移 | 1600×950和现场分辨率核对尺寸；逐状态验证工具栏两个操作入口、参数页五个模板入口、设备圆点和硬触发保存/恢复 | 代码实施完成，待用户统一验证 | S；U；模板按钮唯一迁入参数页，旧容器和旧访问已删除，等待用户统一验证 |
| UI-003 | 图像自适应显示 | 相机帧、检测结果、模板原图 | `InspectionImageCanvas`有图像 | `DetectionResultPresenter`按原图绘制结果Overlay并输出QImage，Widget薄桥继续交给`InspectionImageCanvas::setAutoFitPixmap/resizeEvent`按宽高比缩放居中 | 当前控件尺寸 | 缩放但不改变原图；空图清空 | Presenter短期生成QImage，`InspectionImageCanvas`仅缓存QPixmap | 保持缩放、居中和重绘 | `ui/presenters/detection_result_presenter.*`+`ui/main_window/inspection_image_canvas.*` | Stage 3结果绘制职责已迁移 | 用横图/竖图并调整窗口，核对比例、居中和Overlay位置 | 已验证 | S；T；U；2026-08-15用户确认五模式结果图、Overlay及窗口缩放均正常，ImageLabel物理移动留后续独立切片 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| UI-004 | 结果帧绑定显示 | 任一模式产生正式检测结果 | 检测运行 | 容量1 `UiCompletionMailbox`整体交付结果→五模式收尾生成同一`ProductKey`只读呈现快照→`DetectionResultPresenter::present`一次应用 | 生产检测启用结果绑定；邮箱满时检测线程等待 | 结果图不被实时帧覆盖；图片、框、文字、模板名、判定、统计和耗时在同一UI调用中替换 | 容量1 UI完成邮箱反压检测线程；快照短期持有最终QImage | 保持画面、框、OK/NG、统计和耗时来自同一`ProductKey` | `ui/controllers/detection_completion_controller.*`+`runtime/result_presentation_mailbox.*`+`ui/presenters/detection_result_presenter.*` | Stage 3检测完成协调迁移 | 软硬触发连续检测并移动产品，确认图像、框、模板名、OK/NG、统计和耗时同步替换 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮将五模式结果快照、统计、存图及PLC副作用顺序从Widget迁入统一完成控制器，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| UI-005 | 结果与状态展示 | 任一检测完成或状态变化 | 已启动检测 | 正常结果按 `verdictStyle` 显示 OK/NG SVG；运行故障停止采集、Worker和对应硬件处置后由 MainWindow 显示一次固定警告 | 识别文本、模板 Profile、耗时、判定类型及仅含原因/接收数/完成数的故障快照 | 五模式统一使用对勾/叉号图像；故障不伪装产品 NG，未完成数量只写停止日志 | 正常结果同一 UI 调用整体更新；停止完成后保留既有正式结果 | 正常判定使用固定 SVG；故障不长期占用判定栏 | Runtime + MainWindow + InspectionPage | 运行故障自动停止收口 | 五模式正常结果回归；制造相机、PLC、队列或 CSV 故障，核对硬件状态、一次警告和回到 Idle | 迁移中 | S；U；MainWindow 唯一故障入口和自动停止已实施，等待用户统一验证 |
| UI-006 | 模板引导与提示 | 制作模板、绘图事件、悬停 | 模板预览或冻结 | `setupTemplateGuide`→`handleTemplateGuideEvent`→`updateTemplateGuideText`；`eventFilter`延迟500ms工具提示 | 当前模式与已画点数 | 显示分步引导和模式专用说明；离开隐藏 | 创建/调整引导Frame | 保持中文提示和步骤含义 | `ui/main_window/template/` | 保留 | 五模式进入制作模板，悬停按钮并执行绘图，核对引导变化 | 已验证 | S；U ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| UI-007 | 防滚轮误改参数 | 鼠标滚轮经过下拉框/SpinBox | 主窗活动 | `Widget::eventFilter`拦截`QComboBox/QAbstractSpinBox`的Wheel | 所有安装事件过滤器的控件 | 滚轮被丢弃，点击/键盘仍可修改 | 无 | 防误操作行为保持 | `ui/` | 保留 | 记录值，滚轮后不变；点击选择后可变 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| UI-008 | 软件数据目录快捷打开 | 双击只读目录框 | AppData目录可创建/打开 | `eventFilter`→`QDir::mkpath`→`QDesktopServices::openUrl` | `AppSettingsManager::globalSettingsDirPath()` | 打开目录；创建/打开失败弹提示 | 可能创建目录并启动资源管理器 | 保留入口 | `ui/settings_page.*` | 保留 | 双击目录，核对资源管理器路径；只读失败场景记录提示 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| UI-009 | 右侧分隔条记忆 | 拖动参数/结果区域分隔条 | 主窗已加载 | `QSplitter::saveState`→全局设置；启动`restoreState` | 字节状态；退出/保存后生效 | 重启恢复；无效状态回默认并记日志 | 写全局设置 | 保持布局记忆 | `ui/layout/` | 保留 | 拖动、退出、重启；再注入无效状态核对回退 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |

## 3. 设置与参数应用

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SET-001 | 全局设置读取与安全保存 | 启动及各“设置/确认”按钮 | AppData可访问 | `AppSettingsManager::loadGlobalSettings/saveGlobalSettings`→临时文件验证→替换/回滚；本切片把按模式已发布配方UUID恢复从字库家族扩展到钢印与深度OCR | 配置版本2；当前用户AppData；每个模式至多一个已发布配方UUID | 合法值加载；未知模式的UUID记忆被丢弃；旧配置没有新键时保持兼容；保存失败保留旧文件并报告/日志 | 读写`settings.ini`及临时/备份 | 整机设置语义保持，新增键为可选兼容字段 | `system_support/settings/` | 保留后封装 | 四种模板模式分别选择配方、切换及重启恢复；损坏配置和不可写目录回退 | 已验证 | S；T；U；2026-08-13用户确认钢印/OCR切换恢复、模式隔离及旧入口覆盖正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-002 | 整机默认值 | 首次启动、严格 Schema 重建或清空设置后的下一次启动 | 设置文件不存在或按既有严格规则重建 | `AppSettings::defaults()`→`AppSettingsStore`→主窗口与设置页初始化 | Schema 8 唯一默认对象；相机曝光300、增益1、无已保存抽屉状态时左侧宽度400px | UI加载完整默认配置；设备参数仍按各自正常连接和应用链生效 | 首次保存时创建`app_settings.json` | 默认值只服务配置初始化，不提供在线恢复入口 | `system_support/settings/` | 保留 | 备份并清空设置后启动，核对默认配置且不覆盖范围外用户文件 | 已验证 | S；U；在线恢复入口删除后，默认对象继续作为首次启动和清空后的唯一来源 |
| SET-003 | 模板私有设置 | 选择、保存、编辑模板 | 模板目录可读写且没有外部程序正在浏览其内部目录 | 旧INI继续映射为规范`RecipeProfile`；本切片将Widget对草稿/编辑会话的候选复制、参数/资产校验及发布成功提交收口到统一`TemplateRecipeWorkflow`，四模板模式共享相同事务边界 | Profile目标、阈值、定位框、字符框、二维码参数、顺序和模式特有资产保持；纸巾不进入模板会话 | 工作流只在完整校验和正式发布成功后替换会话状态；失败保留上一会话与正式配方 | 旧Profile继续写INI和原目录；新配方由RecipeStore整目录替换 | 后续迁入Recipe，不要求兼容旧格式 | recipes工作流事务+Widget薄桥 | Stage 1分步迁移 | 四模板模式发布/重发；失败后继续编辑；旧入口回归 | 已验证 | S；T；U；2026-08-13用户确认统一工作流测试及可执行主程序路径无问题；深度OCR因当前无旧模板未单独执行配方重发；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| SET-004 | 未应用标记与启动确认 | 编辑带绑定的参数 | 参数可编辑 | `setupGlobalSettingBindings`/模板dirty跟踪继续生成脏项；启动顺序由`InspectionStartPreflight::evaluateAccess`统一判定，Widget只展示原确认框并在继续时恢复已应用值 | UI值与`m_appliedGlobalSettings`/Profile比较 | 启动前列出未应用项；取消不启动；继续会恢复已应用值后运行；恢复后按最新已应用PLC触发状态继续预检 | 标签变化；可能丢弃未应用UI值 | 保持“应用”和“编辑”边界 | `runtime/inspection_start_preflight.*`+Widget提示桥，后续迁入`ui/settings_controller.*` | Stage 2启动门禁统一 | 修改相机、PLC、阈值但不确认，启动后分别选取消/继续；确认脏项先于PLC未连接提示 | 已验证 | S；T；U；2026-08-14用户确认运行测试37项及主程序未应用参数取消/继续、原值恢复和启动行为均无问题；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| SET-005 | 运行中参数禁用 | 检测/模板状态变化 | 硬件或模板操作进行中 | `updateHardwareParameterUiEnabled`→相机运行状态+`InspectionRuntimeController::isPlcConnected`+`registerHardwareAction` | 相机打开、PLC连接、操作状态 | 运行中禁止会改变设备/关键参数的控件；不同连接状态允许不同字段 | 控件状态改变 | 防止中途改运行快照 | `ui/settings_controller.*`+运行控制器窄查询 | 最终架构设备查询收口 | 在空闲、相机开/关、PLC连/断、检测中逐项核对 | 已验证 | S；T；U；既有运行中参数门禁证据保留，2026-08-15主程序设备查询边界回归正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-006 | 相机曝光应用 | 曝光“设置”或检测启动 | 相机已打开 | `on_sureButton_clicked`→`queryCameraExposureRange`→`ICameraDevice::get/setFloatValue` | 整数曝光；相机SDK给最小/最大；默认800 | 范围内写入；越界/SDK失败提示；打开相机时保存值会按范围调整并提示 | 写相机`ExposureTime`；成功保存设置 | 数值与生效时机保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 最小、最大、越界、正常值各一次；重启开相机核对 | 已验证 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-007 | 相机增益应用 | 增益“设置”或检测启动 | 相机已打开 | `on_pushButton_12_clicked`→`applyCameraGainFromUi`→`ICameraDevice::get/setFloatValue` | 整数；SDK范围；默认1 | 合法写入并保存；空/越界/SDK失败提示 | 写相机`Gain` | 保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 边界/越界/正常值，重启核对 | 已验证 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-008 | 颜色通道应用 | “颜色通道→确认”或检测启动 | 线程存在 | UI索引先经`InspectionRunConfiguration::parseSettings`映射为0..3运行码，再由Widget原`choosechannel`信号送入`MyThread/CameraThread::receivecolorchannel*` | 彩色/红/绿/蓝；默认彩色 | 后续帧按选定通道处理；无效索引仍回彩色0 | 更新线程参数并保存 | 通道映射保持 | `runtime/inspection_run_configuration.*`+后续detection input transform | Stage 2运行配置统一 | 纯逻辑覆盖0..3和越界回退；主程序切换代表通道启停 | 已验证 | S；T；U；2026-08-14用户确认运行测试44项、代表通道设置及主程序启停正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过；2026-08-18用户确认S8统一门禁通过 |
| SET-009 | 图像旋转应用 | “图像旋转→设置”或检测启动 | 线程存在 | UI索引先经`InspectionRunConfiguration::parseSettings`映射为0..3运行码，再由Widget原`rotate`信号送入采集线程 | 无/顺90/逆90/180；默认无 | 后续采集图旋转；无效索引仍回无旋转0 | 更新线程参数并保存 | 旋转方向保持 | `runtime/inspection_run_configuration.*`+后续detection input transform | Stage 2运行配置统一 | 纯逻辑覆盖0..3和越界回退；主程序切换代表旋转启停 | 已验证 | S；T；U；2026-08-14用户确认运行测试44项、代表旋转设置及主程序启停正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过；2026-08-18用户确认S8统一门禁通过 |
| SET-010 | 纸巾粗糙度阈值 | 纸巾模式“设置”及启动 | 值>0 | `InspectionRunConfiguration::parseSettings`保持原`toDouble`与大于0门禁→Widget显式构造`TissueRecipeParameters`→启动前复制到两采集线程 | 唯一代码默认由`TissueRecipeParameters`提供6.0；有已保存整机设置时使用保存值 | 合法值固定为本次运行线程的参数副本；非法仍用原提示且不启动 | 保存整机设置；不再更新进程级检测器默认 | Stage 1唯一6.0来源保持，Stage 2抽出运行参数校验 | `runtime/inspection_run_configuration.*`+`recipes/product_recipe.*` | Stage 2运行配置统一 | 纯逻辑覆盖合法、小于等于0和非数字；纸巾启动回归 | 已验证 | S；T；U；2026-08-14用户确认纸巾阈值显示、整图启动和停止均正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过；2026-08-18用户确认S8统一门禁通过 |
| SET-011 | 存图策略设置 | 保存模式、类型、路径浏览 | 主窗空闲 | UI改变→`syncImmediateGlobalSettingsFromUi`→`saveSettings`；浏览按钮选目录 | 不保存/NG/OK/全部；两类都存/仅标注/仅原图；默认不保存+仅标注 | 选项控制后续存图；未选目录时依现有路径逻辑；浏览取消不变 | 写全局设置 | 详见SAVE功能 | `recipes/save_policy.*` | 保留后迁移 | 逐组合选择、重启，核对显隐和实际文件 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-012 | 清空当前软件数据 | 软件设置页按钮 | 用户二次确认 | `on_pushButton_clearSoftwareData_clicked()`→删除`settings/app_settings.json`→成功提示并退出 | 只针对当前 Windows 用户的软件设置文件 | 取消时不变；删除失败时提示且不退出；成功后退出并在下次启动加载完整默认配置 | 删除`app_settings.json`；不删除模板、图片、授权或日志 | 唯一完整回到默认配置的用户入口 | `ui/main_window/main_window_settings.cpp`、`system_support/settings/` | 保留 | 在测试用户数据中确认/取消各一次，重启核对默认值和保留项 | 已验证 | S；U；既有清空流程保持，待用户核对删除恢复按钮后的软件设置页 |
| SET-013 | 恢复默认设置 | 无，入口已删除 | 用户于2026-09-09确认删除 | UI按钮、显式连接、状态权限、MainWindow声明/实现、恢复日志和提示全部删除 | 无 | 软件不再提供在线部分恢复行为 | 无 | 与清空当前软件数据职责重叠 | 无 | 删除 | 生产符号零引用；软件设置页确认无等价入口 | 已确认删除 | S；用户明确要求完全删除且不保留兼容层或胶水层 |

## 4. 模板制作、加载与编辑

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| TPL-001 | 实时模板预览 | “制作模板”第一次点击 | 相机已打开、未检测 | `on_VideoShoot_clicked`→`startTemplatePreview`→软触发`MyThread`→`ICameraDevice`→`signal_templatePreviewFrame` | 当前曝光/增益/旋转/通道 | 显示实时画面和引导；相机/线程错误提示并退出预览 | 启动采集线程；不统计/PLC | 预览入口保持 | `ui/main_window/template/`+`devices/camera/` | Stage 2相机适配 | 五模式分别进入预览，核对状态、按钮、实时画面 | 已验证 | S；T；U；2026-08-13用户确认模板预览与主程序集中门禁无问题；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-002 | 冻结、重拍与退出 | 预览中再次点制作模板；已有选择时重拍；退出按钮/Esc | 有最近预览帧 | `freezeTemplatePreview`/`resetTemplateCaptureState`/`stopTemplatePreview`→相机接口停止唤醒 | 最近克隆帧 | 冻结当前帧并允许绘图；重拍有确认；退出清理选择并回相机就绪 | 协作停止预览线程、清缓存 | 保持状态转换 | `ui/main_window/template/`+`devices/camera/` | Stage 2相机适配 | 冻结、重拍取消/确认、Esc/退出各一次 | 已验证 | S；T；U；既有深度OCR冻结/绘图/保存及2026-08-13相机适配主程序集中门禁均无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-003 | 定位矩形绘制 | 冻结模板图后左键拖动 | 钢印/字库/深度OCR/二维码模式 | `InspectionImageCanvas::mousePress/Move/Release`→`m_trackingRect`→引导事件 | 显示坐标，保存时换算到原图 | 形成归一化定位框；过小/无框不能完成保存 | 仅UI选择状态 | 独立登记`InspectionImageCanvas`行为 | `ui/main_window/inspection_image_canvas.*` | 保留后移动 | 不同比例窗口画框，核对显示与保存后物理框 | 已验证 | S；U；深度OCR实际模板发布证明定位框已形成并完成坐标保存 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-004 | 二维码矩形与即时读码 | 二维码模式第二个矩形 | 已有定位框 | `InspectionImageCanvas`→`validateBarcodeTemplateRect`→`IBarcodeDecoder::decode`；DLL与通用策略已迁入引擎适配器 | BarcodeOptions默认DataMatrix、padding8%、预算60ms、fallback开 | 可读则保留框并继续日期多边形；不可读弹原因、清二维码/日期但保留定位框 | 适配器动态加载/调用`BarcodeDecoder.dll` | 二维码必须先验证 | `ui/main_window/template/`+`engines/barcode/` | Stage 2已适配 | 可读、不可读、越界二维码框各一次，核对清理范围 | 已验证 | S；T；U；2026-08-13用户确认假ABI测试、实际框选即时读码和主工程均无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18按用户要求将OCR/二维码端口与vendor实现从devices迁至engines，等待Qt Creator Run qmake、Rebuild及对应功能复验；2026-08-18用户确认S8统一门禁通过 |
| TPL-005 | 日期多边形绘制与闭合 | 左键逐点、右键闭合 | 已有前置框 | `InspectionImageCanvas`多边形状态→`signal_templateGuideEvent` | 至少3点 | 闭合后可保存/提示；点数不足保持绘制；Esc清理当前选择 | UI状态 | 保持鼠标/键盘语义 | `ui/main_window/inspection_image_canvas.*` | 保留后移动 | 2点右键、3+点右键、Esc，核对状态与提示 | 已验证 | S；U；深度OCR实际模板发布及随后检测启动证明日期多边形已闭合并落盘 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-006 | 坐标换算与通用模板保存 | “保存模板”及动态“发布当前模板/模板组” | 单模板保存需框完整；模板组发布需字库家族已加载至少两个旧Profile | 坐标/资源生成不改；草稿发布及成功后编辑会话建立由统一工作流一次完成，Widget不再分别编排两类会话 | 显示名、模式、Profile顺序、参数和资产命名空间保持；钢印配方包含独立`stampRing`角色 | 发布或编辑会话建立失败不替换Widget当前工作会话；正式目录仍由RecipeStore保证 | 旧目录只读作为发布源；成功时事务写入`AppDataLocation/recipes/<UUID>` | 计划内改为Recipe整目录安全保存 | recipes workflow/publisher/store+Widget薄命令 | Stage 1分步迁移 | 四模板模式首次发布、随后编辑、UUID和失败保持 | 已验证 | S；T；U；统一工作流自动测试及字库/二维码/钢印实际发布回归通过；深度OCR当前无旧模板未单独发布 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-007 | 钢印环与钢印区域标定 | 钢印模板保存及已发布配方选择 | 通用框已保存 | 旧入口仍写`template_ring.bmp`；本切片选择钢印配方时从`stampRing`和标定资产原子初始化候选重叠引擎 | OpenCV交互ROI；相对吸管口中心坐标；配方内目标为`assets/profiles/<n>/template_ring.bmp` | 环图、YAML或钢印区域无效时拒绝选择且保留当前引擎 | 旧入口仍写模板/YAML；新发布只事务复制到配方目录 | 保持标定次序、坐标及检测行为 | recipes资产合同+Widget运行资源桥 | Stage 1分步迁移 | 钢印配方选择后重叠引擎可启动；缺环/坏环/缺钢印区拒绝 | 已验证 | S；T；U；2026-08-13用户确认钢印配方选择、启动/停止及旧模板回归正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-008 | 单模板选择与校验 | 旧“选择模板”及动态“已发布配方” | 空闲状态 | 本切片把钢印和深度OCR已发布配方选择接入统一列表，并在完整解码定位图/YAML/钢印环后一次安装；旧目录选择保留 | UUID、当前`DetectionMode`和模式必需角色；钢印需要tracking/calibration/stampRing，OCR需要tracking/calibration | UUID/Schema/资源/模式、图像解码或运行标定任一无效即拒绝且不改写当前模板 | 新路径只读配方目录；旧入口仍更新历史和引擎 | 保持失败不误启动和旧入口兼容 | recipes selection/assembler/store+Widget薄桥 | Stage 1分步迁移 | Stamp/OCR选择、模式错误、坏资源、输出保持及旧入口回归 | 已验证 | S；T；U；2026-08-13用户确认两模式已发布配方选择、参数显示、启停及旧入口覆盖正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-009 | 字库家族多Profile选择 | 旧“选择模板”多目录入口；新增“发布当前模板组”和“已发布配方” | 空闲状态 | 编辑态保存完整有序Profile；启动时深拷贝为本次运行专用Profile快照，定位选择和检测只读快照 | 配方内部Profile顺序、名称、参数、资源路径、资产键及UUID保持；运行期间编辑缓存与运行缓存隔离 | 快照准备或原有预检失败不启动；停止/线程结束释放运行快照；旧入口保持 | 发布/选择继续更新编辑会话，启动仅复制内存资源且不写盘 | 新格式以一个产品配方承载一个或多个Profile并自动选择 | recipes选择/编辑会话+Widget启动薄桥，后续迁入runtime | Stage 1分步迁移 | 两模式启动后Profile选择、停止/再次启动、重选/重启和旧入口 | 已验证 | S；T；U；2026-08-13用户确认两个字库家族及旧多目录入口启停/再次启动均正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-010 | 当前Profile编辑器 | 多Profile加载或模板组发布后下拉选择 | 至少一个Profile | 编辑器继续读编辑态`m_wordTemplateProfiles`；启动前形成独立运行Profile快照，运行检测不再回读编辑态Profile | Profile顺序、名称、目标、阈值、资源键及原图绝对路径保留 | 运行中控件继续禁用；停止后编辑缓存保持并可再次启动 | 编辑显示与运行资源在内存中分离；不新增磁盘写入 | 保持编辑对象与运行快照边界 | recipes编辑会话+Widget资源桥+后续`ui/main_window/template/` | Stage 1分步迁移 | 启动/停止前后编辑显示保持，检测只读快照 | 已验证 | S；T；U；2026-08-13用户确认停止后Profile、目标字符和阈值保持且可再次启动；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-011 | 单Profile目标字符 | “确认字符”及当前Profile“分割字符模板” | 当前模板有效且检测停止 | 目标解析和资源预检保持；当前或完整Profile候选由统一工作流校验并同UUID发布，Widget只安装成功结果 | `RecipeProfile::targetText`、字符框及当前Profile内部字符资产 | 字符缺失、Schema或事务失败时工作会话、正式配方和运行字符缓存均保持上一版本 | 旧Profile行为不变；新Profile不写旧INI，成功时同UUID事务替换 | 保持字符解析和提示 | recipes workflow+Widget资源桥 | Stage 1分步迁移 | 四模板模式单Profile目标、同UUID和失败保持 | 已验证 | S；T；U；参数事务测试和字库/二维码/钢印实际同UUID重发无问题；深度OCR人工项未执行；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-012 | 批量目标字符 | 多Profile配方“批量确认字符”及单Profile裁切 | 已加载多个Profile | 完整有序Profile候选的原子校验和单次发布收口到统一工作流，Widget不再先原地更新编辑会话 | 每Profile目标、顺序和资源命名空间独立；一次操作最多一次配方发布 | 任一验证或事务失败时工作会话和正式配方同时保留上一版本 | 旧目录逐项写INI；新配方只经一次整目录事务提交 | 保持旧批量结果与多Profile资产隔离 | recipes workflow+Widget薄接入 | Stage 1分步迁移 | 多Profile批量目标、单次发布、失败后重试和UUID保持 | 已验证 | S；T；U；统一工作流自动测试及字库家族批量重发回归无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-013 | 单Profile图像阈值 | 阈值“设置” | 当前Profile有效且检测已停止 | 阈值范围和生效顺序保持；候选Profile由统一工作流同UUID发布成功后Widget才更新运行阈值 | `RecipeProfile::imageThreshold`，保持0..100百分比语义和当前生效顺序 | Schema或事务失败时工作会话、正式配方和运行阈值保持上一版本 | 旧Profile行为不变；新Profile不写旧INI | 保持百分比语义 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 四模板模式阈值、同UUID和失败回退 | 已验证 | S；T；U；参数事务测试和可用模板模式实际同UUID重发无问题；深度OCR人工项未执行；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-014 | 批量图像阈值 | 多Profile配方“批量设置阈值” | 多Profile已加载 | 原按钮校验、缓存/`ssim`/dirty顺序保持；完整Profile更新和单次事务提交由统一工作流完成 | 每Profile阈值不被重排；一次按钮操作最多一次配方发布 | 任一Profile验证或事务失败时工作会话与正式配方均保留上一版本 | 旧目录逐项写INI；新Profile只经一次整目录事务提交 | 保持旧逐项结果和整配方原子提交 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 多Profile批量阈值、`ssim`、单次发布和失败重试 | 已验证 | S；T；U；统一工作流自动测试及字库家族批量阈值回归无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |
| TPL-015 | 字符模板切割与命名 | “分割字符模板” | 字库家族或钢印模式、Profile有原图/定位框/date_poly且检测已停止 | `CharacterTemplateEditorDialog` 允许手动画框或窗口独立 `CharacterOcrEngine` 以 CTC 位置为锚点、通过文字行前景投影修正边界后自动分割；两种框统一进入 `CharacterCropLabel::m_items`，按现有规则排序；进入命名页时只对空名称框执行单字符 REC，最终仍由普通输入框确认；Profile资产替换、整配方校验及同UUID发布由统一工作流完成 | 框、名称、Profile顺序、目标路径和UUID保持；自动分割与自动命名都使用 `TemplateStore::templateTargetUnits()`，字符框统一采用 `4 × 4` 最小尺寸 | OCR 不可用、推理异常、空结果或取消替换时保留现有框并可继续手工流程；取消 Dialog 不发布；资产映射、Schema或事务失败时保持原值 | 字符模板窗口持有并释放独立 DET/REC predictor；保存仍只写临时工作区和 RecipeStore 事务目录 | OCR 只替代画框和空名称首次填写，绘制、删除上一个、清空、预览、排序、校验、变体命名和保存规则一致 | `ui/main_window/template/character_editor/`+`engines/ocr/`+recipes workflow | 增加 OCR 辅助输入 | 手动/自动框混合编辑、已有框替换确认、日期标点边界、重复字符、自动命名、返回框选、保存及正式 OCR 回归 | 代码已实施，待用户统一验证 | S；既有手工裁切、字符资产事务和同UUID重发验证继续有效；OCR 辅助代码与静态门禁完成后等待 Qt Creator 构建和实际字符区域验证 |
| TPL-016 | 按模式记忆模板 | 配方发布/选择、切换模式、退出和下次启动 | 全局设置可写 | 本切片将钢印和深度OCR单配方UUID纳入同一按模式恢复规则，旧路径保留回退 | 每个模式的UUID和配方Profile彼此保持 | 发布后安装失败不覆盖旧路径记忆；恢复失败清理失效UUID并回退旧路径 | 读写全局`settings.ini` | 保持模式隔离、失败原子性和旧路径兼容 | 设置桥+recipes选择装配+Widget薄接入 | Stage 1分步迁移 | 四模板模式切换/重启恢复及旧路径覆盖规则 | 已验证 | S；T；U；2026-08-13用户确认钢印/OCR来回切换恢复、配方不串模式及旧路径覆盖正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录）；2026-08-18用户确认S8统一门禁通过 |

## 5. 五种检测与共同算法行为

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| DET-001 | 共同定位与位姿 | 软/硬采集线程获得帧 | 非纸巾模式且模板有效 | `MyThread/CameraThread`→`TrackingPoseMatcher::setTemplate/match`→`DetectionPose`→`dispatchDetectionByMode` | tracking模板；-45..45度、步长2、金字塔0.2、阈值0.3 | 匹配成功映射日期/二维码多边形；失败产生对应NG路径或跳过 | 线程持有模板旋转缓存；无直接PLC | 定位范围、阈值和坐标变换保持 | `detection/common/tracking_pose_matcher.*` | 保留后迁移 | 固定角度/位移/无目标样本，记录pose、Profile和结果 | 已验证 | S；U ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| DET-002 | 模板匹配模式（内部钢印+字符检测） | 界面“模板匹配”（索引0）检测帧 | 有tracking、date、ring/stamp和字符模板 | Widget只准备当前运行参数和重叠检测回调，运行时分发器装配`StampDetectionPipeline` Worker并把完整输出交给统一完成控制器 | 启动时快照目标字符、阈值、字符模板和钢印引擎 | 字符数等于目标且零重叠才OK；资源/定位/匹配/重叠异常语义保持 | Overlay、统计、存图、PLC/剔除队列只在统一结果入口执行一次 | 两条件AND、失败文本与硬件时序不变 | `runtime/detection_mode_worker_factory.*`+`detection/detectionmode/stamp/`+`ui/controllers/detection_completion_controller.*` | Stage 3检测完成协调迁移 | Pipeline、工厂及分发合同测试；硬触发实际OK/字符NG/重叠NG、停止重启和软触发回归 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，不改钢印与字符AND判定，门禁已通过 |
| DET-003 | 字库多Profile模式 | 界面“字库匹配”检测帧；模式由稳定itemData解析，不依赖索引 | 至少一个完整Profile | `DetectionProfileSnapshotBuilder`一次生成同序定位/检测快照，`DetectionRegistry`执行Profile索引门禁、`WordDetectionPipeline`调用并把完整通用结果交给唯一ResultService | 每Profile目标、字符图、阈值；最佳定位Profile | 快照准备或预检失败不启动；运行判定、无定位和NG路径不变；越界索引仍取消该结果 | Profile名/框显示、统计、存图、PLC只在统一结果入口执行一次 | 自动选择、字符计数、失败NG和Profile显示语义保持 | 公共Selector+`detection/detection_profile_snapshot.*`+`detection/detection_registry.*`+字库Pipeline+唯一ResultService | S2～S7检测边界收口 | 硬触发多Profile命中/定位失败/OK/NG、停止重启及软触发回归 | 已验证 | S；U；旧Snapshot/Registry生产引用为0；2026-08-18用户确认S2～S7统一门禁通过 |
| DET-004 | 深度OCR模式 | 模式2检测帧 | 模板日期区域有效；启动时同步创建本次运行的PP-OCRv6 tiny引擎，成功后才创建Worker和启动采集 | `DetectionRegistry::create`创建`DeepOcrEngine`和执行器→同帧Pose→`prepareOrientedDateRoi`→`DeepOcrEngine::recognize`内部DET、阅读顺序排序、透视裁剪和REC→`OcrDetectionPipeline`清洗、组合和判定→统一完成控制器 | 启动时快照目标文本；同一次运行逐帧复用同一引擎；每个DET框对应一个REC片段；组合结果保留换行用于显示和日志，比较时分别移除识别结果和`targetText`中的换行 | 初始化失败不创建Worker且不启动采集；去除换行后的识别文本非空且与目标大小写敏感地完全相等时OK；无框、空识别和不匹配为普通NG；DET/REC执行异常进入Runtime Fault | 识别文本、统计、异步存图、PLC只在统一结果入口执行一次 | 定位、ROI、清洗、Overlay和收尾保持；每次运行独立创建引擎 | `detection/detection_registry.*`+`detection/detectionmode/ocr/`+`engines/ocr/`+`runtime/detection_worker.*` | PP-OCRv6 tiny DET+REC与启动生命周期 | 主程序覆盖初始化失败、启动/停止/再启动、中文、英文大小写、中英混排、多框、旋转、空结果、NG和Fault；软硬触发统一回归 | 代码已实施，待用户统一验证 | S；V6 tiny链、逐次Run失败处理、忽略换行比较及启动前同步初始化已完成静态核对，Release构建通过；待主程序和真实产品验证 |
| DET-005 | 纸巾卷粗糙度模式 | 模式3采集线程 | 相机帧；不需传统模板 | Widget只提供运行阈值，运行时分发器装配`TissueDetectionPipeline` Worker并把纸巾完整输出交给统一完成控制器 | 运行参数副本中的粗糙度阈值；算法找内孔、外圆和环粗糙度 | 找到卷且score<threshold为OK；空图、无圆、外轮廓失败或score>=阈值为NG并带诊断 | Overlay、统计、存图、PLC | 当前边界是`>=`判NG；不改纸巾算法和阈值 | `runtime/detection_mode_worker_factory.*`+`detection/detectionmode/tissue/`+统一完成控制器 | Stage 3检测完成协调迁移 | 纸巾Pipeline/工厂/分发合同测试；主程序连续OK/NG、停止、重启，核对score/阈值/圆框/统计/存图/PLC | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，粗糙度算法和阈值边界不变，门禁已通过 |
| DET-006 | 二维码优先+三期模式 | 界面“二维码+三期”（索引4）检测帧 | Profile含tracking、二维码4点、日期多边形、字符模板，DLL可用 | 一次生成同序定位/检测快照，运行时分发器负责Profile门禁、策略状态、二维码优先Pipeline并把完整输出交给统一完成控制器 | 仅Data Matrix、padding8%、预算60ms、七路预处理、运行内首选策略；每次DLL调用libdmtx先行、ZXing Data Matrix后备 | 配方装配、资源、DLL、ROI或快照预检失败不启动；读码失败短路日期并形成一次NG | 显示码内容/日期状态、统计、存图、PLC只在统一结果入口执行一次 | “最高分Profile、读码优先、失败短路”和硬触发取帧条件保持 | 公共Selector+`runtime/inspection_profile_snapshot.*`+`detection/detectionmode/barcode_word/`+统一完成控制器 | Data Matrix解码优先级调整 | 快照/分发/Pipeline合同测试；固定Data Matrix与QR样本；硬触发可读日期OK/NG、不可读/定位失败逐触发收尾、停止重启和软触发回归 | 代码已实施，待用户验证 | S；静态确认libdmtx先行、ZXing仅Data Matrix后备，QR生产符号已删除；待用户重建DLL并完成真实样本验证 |
| DET-007 | 定位失败收尾 | 字库家族采集时无有效pose | 已启动检测 | 普通字库和二维码软硬触发均把无效Pose作为同一产品工作项提交Worker并形成一次NG | 软触发由正式链反压形成节拍；硬触发每个新回调帧 | 显示定位失败NG；二维码硬触发保证本次触发有且只有一次收尾 | 增总数/NG、可存图、PLC或排队 | 软硬触发差异和Stage 4异常归类边界保持 | `runtime/result_handler.*`+各模式Pipeline | Stage 3软硬触发失败收尾已验证 | 移出视野：软触发保持；硬触发逐次打光记录结果数和PLC | 已验证 | S；T；U；2026-08-15用户确认硬触发集中门禁无问题，定位失败正式出口已统一 |
| DET-008 | 算法/系统失败当前统计语义 | 模板缺失、读码失败、无圆、无定位、PLC断连、硬触发FIFO满、CSV或正式结果提交失败 | 检测已启动或启动预检 | 普通算法失败按NG走唯一正式结果事务；基础设施或收口失败进入Fault并自动停止 | 候选统计、`ProductKey`账本和故障首因 | 只有CSV、存图任务提交、PLC合同和呈现完成后才提交统计并增加正式结果完成数；中途失败的产品留作未确认 | Fault取消Worker/UI邮箱、清空延迟NG请求并拒收新正式帧；异步存图写盘失败保持警告 | 产品NG、正式结果完成数与系统Fault严格分离 | `inspection_runtime.*`+`result_service.*` | 运行故障与正式结果收口 | 回归五模式算法NG；覆盖CSV、存图提交、PLC、呈现故障及Fault后新帧 | 迁移中 | S；U；候选提交顺序、正式完成计数和最小故障快照已实施，等待用户统一验证 |

## 6. 相机、采集线程与运行控制

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CAM-001 | 扫描并打开首台相机 | 工具栏“打开相机” | 无运行任务、相机未开 | `toolButton_cameraAction`→`handleCameraAction()`→`openCamera()`→应用服务枚举并打开首台相机；沿用打开相机时尝试连接PLC的顺序 | 保存的曝光/增益及PLC地址；固定选第0设备 | 成功进入CameraReady；无设备/打开/曝光失败提示并回滚相机；PLC失败不阻止继续开相机 | 连接PLC、启动抓图；同一快照刷新按钮和状态圆点 | 固定第0台及伴随PLC连接是当前行为 | MainWindow + ApplicationService + CameraSession | 常驻工具栏设备入口 | 0/1/多相机场景及PLC在线/离线组合，核对按钮、图标和状态 | 迁移中 | S；U；统一相机按钮已实施，真实相机与PLC组合等待用户验证 |
| CAM-002 | 关闭相机 | 工具栏“关闭相机” | 非检测、非停止、非模板制作 | `toolButton_cameraAction`→`handleCameraAction()`→`closeCamera()`→`CameraSession::close()`→清图/文本/计数→CameraClosed | 当前运行快照 | 成功释放相机并清零显示；忙碌状态下按钮禁用 | 关闭SDK、清UI统计和缓存；同一快照刷新按钮和状态圆点 | 清理范围保持 | MainWindow + ApplicationService + CameraSession | 常驻工具栏设备入口 | 空闲关闭；检测中/模板中核对禁用；再正常关闭 | 迁移中 | S；U；统一相机按钮已实施，等待用户验证 |
| CAM-003 | 软触发采集 | 未勾PLC触发后“启动识别” | 相机开、模板预检通过 | `CaptureWorker`每轮执行SoftwareTrigger并等待本次触发的新帧；纸巾整帧及OCR/钢印/字库/二维码定位任务提交容量1检测队列，完成结果再经容量1 UI邮箱反压 | 不读取硬触发延时，不附加人为最小间隔或固定休眠 | 每次正式检测使用新相机帧；检测/UI/存图越慢会通过有界链路反压采集，快时最高受相机速度限制；不丢正式输入 | 单采集线程+单检测线程+容量1 UI完成邮箱 | 旋转/通道、算法、PLC和存图规则不变；软触发输入节拍由相机速度与实际处理能力驱动 | `runtime/capture_worker.*`+`frame_queue.*`+`detection_worker.*`+`result_presentation_mailbox.*` | 当前正式采集链 | 五模式连续检测/停止/重启；硬触发延时0和非0时软触发节拍均不受影响 | 已验证 | S；U；2026-08-18用户批准断开该参数与软触发的联系，待Qt Creator主程序现场复验；2026-08-18用户确认S8统一门禁通过 |
| CAM-004 | 硬触发采集 | 工具栏开启“硬触发”后启动，外部Line触发 | PLC已连、相机支持Line0 | 工具栏唯一`checkBox_hardwareTriggerEnabled`的勾选状态同时决定滑块和“开/关”文字并立即保存`trigger.enabled`；启动时生成HardwareTrigger，停止采集后切Line0，应用曝光/增益及海康`TriggerDelay`，再注册回调并启动采集，正式帧提交容量1检测链 | 无已保存配置时默认关闭；`LineDebouncerTime=5000`；“硬触发延时(ms)”乘1000写入MVS `TriggerDelay`（µs） | 外部沿到达后由相机按TriggerDelay延时曝光；队列溢出进入Fault并自动停止 | MVS回调进入采集线程并向有界FIFO提交 | 保持Line参数、注册/启动顺序和正式帧受理规则 | `machine_settings_page.*`+`camera_session.*`+`capture_worker.*` | 工具栏设置入口与正式采集链 | 首次无配置默认关闭；开关文字同步；保存失败恢复、重启恢复；真实相机分别设置0/1/300ms核对成像时刻 | 迁移中 | S；U；硬触发控件已迁入工具栏且保持唯一绑定，真实设备等待用户验证 |
| CAM-005 | 帧读取与停止唤醒 | 采集线程调用或停止 | 相机抓图中 | `CMvCamera`原回调与条件变量保留→`HikvisionCameraDevice`委托帧读取/序号/停止唤醒→软硬触发线程 | 超时/非阻塞模式 | 返回克隆最新帧；停止请求唤醒等待；空帧/超时返回失败 | 持有最新cv::Mat和序号 | 不允许`QThread::terminate()` | `devices/camera/hikvision_camera_device.*` | Stage 3停止协调器接管调用顺序 | 连续采集、无帧超时、等待中停止，核对退出延迟 | 已验证 | S；T；U；2026-08-15用户确认软硬线程停止无超时、相机正常恢复且可再次启动；适配器和条件变量行为未改 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| CAM-006 | 采集前图像变换 | 每帧进入定位/算法前 | 已设置旋转/通道 | 启动时由`InspectionRunConfiguration`统一生成原0..3运行码，仍经原信号写入`MyThread/CameraThread`，线程内rotate/channel分支与先后顺序不改 | SET-008/009应用值 | 输出彩色或单通道派生图、指定方向；异常帧不进入正常检测 | 新cv::Mat临时内存 | 变换实现、顺序与方向保持 | `runtime/inspection_run_configuration.*`+后续`detection/input_transform.*` | Stage 2配置统一、Stage 3迁移变换 | 纯逻辑映射测试；代表旋转/通道主程序回归 | 已验证 | S；T；U；2026-08-14用户确认代表旋转/通道及两种采集模式画面正常 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-001 | 启动预检与快照 | 工具栏“开始识别” | 相机已打开且 Runtime 空闲 | `handleInspectionAction()` 根据一次 `RuntimeSnapshot` 调用 `startInspection()`；原预检和硬件下发顺序保持；OCR模式先在`DetectionRegistry::create()`同步创建引擎，再创建Worker，随后启动图像采集 | 已应用设置和模式资源 | OCR初始化失败沿启动失败链回到Idle且不采集；自动停止完成后按当前已应用配置再次启动 | Runtime 持有 Worker、模式、UI 邮箱、最小 Fault 快照和 ProductKey 账本；OCR执行器持有本次运行的引擎 | 非OCR启动路径不创建OCR引擎 | 既有运行配置 + Runtime | 当前正式启动边界 | 五模式正常启动；OCR初始化失败；OCR启动/停止/再启动；Fault 自动停止后再次启动且新账本为空 | 迁移中 | S；启动前同步初始化静态门禁和Release构建已完成，等待用户统一运行和设备验证 |
| RUN-002 | 停止识别 | 工具栏“停止识别”或 Fault 自动停止 | 检测中或线程运行 | `stopInspection()` 或故障信号直接调用 `InspectionApplicationService::stop(reason)`；同步停止采集、等待 Worker、按原因处置硬件，并由 `finishStop()` 唯一收口回到 Idle | Fault 首因、已接收数、已完成数和未收口 ProductKey | 未完成产品只进入停止摘要的未确认数量；相机恢复问题沿用现有日志和状态提示 | 停止采集与 Worker，释放队列并清理运行数据 | 不使用 `terminate()`，不保留确认、恢复或解锁链 | Runtime + ApplicationService + CameraSession | 当前正式停止边界 | 正常停止；相机、PLC、硬触发、CSV或运行不变量故障自动停止 | 迁移中 | S；本轮静态门禁完成后等待用户统一构建、运行和设备验证 |
| RUN-003 | 线程重建与信号接回 | 启动前、旧线程结束后和 Fault 自动停止 | 主窗存活 | Fault 立即取消 Worker 与 UI 邮箱并请求停止；实际退出后统一清空运行数据；后续启动重建全新队列、Worker、邮箱和账本 | 相机接口、模板和运行参数 | Fault 后到达的帧由活动状态检查直接拒绝 | 取消队列和邮箱，释放帧引用并重建 Worker | 不重复连接、不残留线程、不重复分发或持图 | Runtime + 采集停止边界 | 当前异常线程收口 | Fault 后持续进帧、引用释放和再次启动 | 迁移中 | S；本轮静态门禁完成后等待用户统一构建、运行和设备验证 |
| RUN-004 | 流帧与结果内存持有 | 相机持续采集/检测完成 | 主窗活动 | 软触发阻塞提交；硬触发非阻塞 `trySubmit`，队列满进入 Fault；只有完成 CSV、统计、存图步骤、PLC 和呈现收口的产品才累计已完成数 | 软触发背压；硬触发容量 1；已接收数和已完成数 | 已完成正式结果保持；尚未收口的产品计为未确认；Fault 后不产生正式结果 | Fault 取消队列和 UI 邮箱并清空延迟 NG；新帧不创建 ProductKey | 软触发正常不丢；硬触发过载自动停止；每件产品只最终收口一次 | FrameQueue + Worker + Runtime + ResultService | 当前正式结果收口边界 | 非阻塞 FIFO、单件/多件未确认、各事务步骤失败和图像释放 | 迁移中 | S；本轮静态门禁完成后等待用户统一构建、运行和设备验证 |
| RUN-005 | 采集节拍与硬触发延时 | 软/硬触发正式采集 | 运行参数已应用 | 软触发按“软触发命令→新帧→正式检测链完成受理”串行循环；硬触发启动配置将机器设置ms值换算为µs写入相机 | 软触发无人工间隔；硬触发延时默认300ms、0表示相机不增加触发延时 | 软触发吞吐由相机、检测队列、UI邮箱及存图反压决定；硬触发由MVS节点延后曝光 | 软触发影响吞吐；硬触发参数影响外部沿到曝光的时刻 | 保持设置值单位为ms，设备边界显式使用µs | MachineSettings+CameraSession | 当前正式采集链 | 软触发在参数0/非0时吞吐不变；硬触发0/1/300ms核对节点与实际时序 | 已验证 | S；U；2026-08-18语义经用户批准，待Qt Creator真实相机复验；2026-08-18用户确认S8统一门禁通过 |
| RUN-006 | 最近Overlay随位姿逻辑 | 非结果绑定模式收到新pose | 有上一检测框和pose | `slot_saveBoxesFromThread`→按角差/中心差旋转平移`g_lastDrawResults/g_lastStampPoly` | 新旧`DetectionPose` | pose无效清框；有效时框跟随；字库结果绑定时直接抑制 | 更新全局Overlay缓存 | 保持模式差异 | `ui/presenters/` | 保留后拆分 | 移动/旋转产品并移出视野，核对框跟随和清除 | 已验证 | S；U ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过；2026-08-18用户确认S8统一门禁通过 |

## 7. PLC与剔除

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLC-001 | 连接 PLC | 启动延迟、打开相机附带连接、或“连接PLC” | IP/Rack/Slot 可用 | 三入口调用 `InspectionApplicationService::connectPlc()`，再经 `InspectionRuntime`、`InspectionPlcController` 和 `IPlcDevice` 到 Snap7 | 默认 `192.168.10.10/0/1` | 成功后同一 `RuntimeSnapshot::plcConnected` 刷新工具栏状态点和设置权限；失败提示且不伪装成功 | 建立 TCP/S7 会话、写设置 | 三个入口和原顺序均保留 | ApplicationService + Runtime + PLC 控制器 + 设备层 | 当前正式 PLC 连接链 | 在线、错误 IP、错误 rack-slot 分别走三个入口 | 迁移中 | S；等待用户统一构建和真实 PLC 验证 |
| PLC-002 | 断开 PLC | “断开PLC”、退出或 PLC Fault | PLC 控制器存在 | 人工入口和退出经 ApplicationService；`PlcDisconnected` 在故障停止链中直接调用 `InspectionRuntime::disconnectPlc()` | 当前连接状态和 Fault 原因 | 成功后工具栏状态显示未连接；人工失败提示，Fault 停止完成后只显示一次故障警告 | 关闭 PLC 会话 | 不增加第二份连接状态 | ApplicationService + Runtime + PLC 控制器 | 当前正式 PLC 断开链 | 在线/离线人工断开、退出和 PLC Fault 自动断开 | 迁移中 | S；等待用户统一构建和真实 PLC 验证 |
| PLC-003 | 触发工作模式下发 | 工作模式“确认”或启动 | PLC已连接 | UI读取模式→`InspectionRuntimeController::writePlcTriggerMode`→控制器内部编码并写`DB1.DBB1032` | 连续=0、间歇=1 | 成功保存设置；无连接/写失败提示且不更新已应用值 | PLC写`DB1.DBB1032` | 地址和值不变 | `runtime/inspection_plc_controller.*` | 最终架构PLC业务命令收口 | Fake锁定0/1字节；现场读回延期 | 已验证 | S；T；U；136项运行测试确认0/1字节合同；现场读回延期 |
| PLC-004 | 工艺参数下发 | “PLC参数→设置”或启动 | PLC已连接、整数可解析 | UI构造`PlcRunSettingsCommand`→应用服务→PLC控制器内部大端编码和固定顺序写入 | 剔除时间DB980 Word、剔除距离DB920 DWord、拍照时间DB982 Word、拍照距离DB924 DWord；“硬触发延时”只保存为机器设置并在硬触发启动时写相机，不下发PLC | 全部成功后保存；任一写失败提示并返回失败字段 | 多次PLC写入 | PLC地址、长度、顺序和值保持 | `runtime/inspection_plc_controller.*` | 当前正式PLC链 | 边界值、正常值和中途写失败；真实PLC读回延期 | 已验证 | S；T；U；2026-08-18明确硬触发延时归属相机MVS节点而非PLC |
| PLC-005 | OK 输出 | 正式结果 OK 且 PLC 连接 | 结果事务收尾 | `ResultService` 携带 ProductKey，经 Runtime 和 PLC 控制器写 `DB1.DBB1033=0`；写入成功后继续呈现、统计提交和最终收口 | DB1 偏移 1033 一字节，值 0；PLC 连接和返回码 | 写入失败进入 `PlcDisconnected` Fault，本件保留为未确认且不提交统计或已完成数 | PLC 写 0 或 Fault 取消正式检测链 | 正常 OK 值和唯一请求保持 | ResultService + Runtime + PLC 控制器 | 当前正式结果事务 | 写 0 成功、失败和重复结果合同；真实 PLC 验证 | 迁移中 | S；等待用户统一构建和真实 PLC 验证 |
| PLC-006 | NG 脉冲输出 | 正常产品 NG | PLC 连接 | 当前 NG 通过 `ResultService` 按 ProductKey 请求 49→约 100ms→0；写入经 Runtime 与 PLC 控制器，全部成功后继续呈现、统计和最终收口 | DB1.DBB1033；49 持续约 100ms | 49 或 0 写入失败进入 `PlcDisconnected` Fault，本件计为未确认 | PLC 两次写或 Fault 取消正式检测链 | 正常值、顺序和脉冲保持；不重复或盲发 PLC | ResultService + Runtime + PLC 控制器 | 当前正式结果事务 | 正常 NG、49/0 写入失败和产品收口 | 迁移中 | S；等待用户统一构建和真实 PLC 验证 |
| PLC-007 | 延迟剔除队列与复位 | NG 结果、每次产品收尾、“剔除复位” | `wrongindex` 可能大于 0 | 到期公式和顺序保持；本件 NG 在结果事务末尾登记延迟请求即完成当前 PLC 合同，到期后携带原始 ProductKey 输出 49→0 | 剔除位置、PLC 连接/返回码和原始产品身份 | 到期输出失败进入 Fault；Fault 首因接受后立即清空剩余延迟请求 | ResultService 内的延迟请求队列、PLC 脉冲或 Fault | 延迟公式和值不变；不等待未来产品才完成本件收口 | ResultService + Runtime + PLC 控制器 | 当前延迟 NG 合同 | wrongindex、原始身份、故障清空和再次启动 | 迁移中 | S；等待用户统一构建和真实 PLC 验证 |

## 8. 结果、统计与存图

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RES-001 | 总数、NG 与合格率 | 每个正式结果收口 | 结果已经由 Runtime 唯一认领 | `ResultService` 先形成候选统计；CSV、存图步骤、PLC 和呈现均完成且 Runtime 未进入 Fault 后才提交统计并最终收口 | 产品结论和 ProductKey | 正常总数、NG 与合格率公式不变；同一产品只收口一次；未确认只记录在停止摘要 | 正常产品统计 | 产品质量合格率只使用已经完成收口的 OK 与产品 NG | ResultService + Runtime | 当前正式结果收口边界 | 正常 OK/NG；各事务步骤失败、单件/多件未确认、重复结果和统计清零 | 迁移中 | S；等待用户统一构建、五模式结果和真实 PLC 验证 |
| RES-002 | 总数清零 | 总数旁“清零” | 任意空闲/运行状态当前可点击性依UI状态 | 按钮薄桥→协调器`resetStatistics`→Presenter只刷新总数和NG | 无 | 总数和NG同时置0；合格率框仍不在该槽重算 | 清运行层统计并刷新指定控件 | 精确清理范围保持 | Widget薄桥+runtime协调器+UI Presenter | Stage 3检测完成协调回归 | 先产生多结果再清零，核对三个统计字段和下一帧 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮结果统计写入路径迁移，清零入口作为回归门禁，现有不对称规则不改；Stage 4第一轮扩展统计结构，现有UI清零行为通过119项运行测试及主工程门禁确认 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RES-003 | NG数清零 | NG旁“清零” | 同上 | 按钮薄桥→协调器`resetNgCount`→Presenter只刷新NG | 无 | 仅NG置0；总数保持；合格率仍不在该槽重算 | 改运行层NG统计并刷新指定控件 | 当前不对称行为纳入基线 | Widget薄桥+runtime协调器+UI Presenter | Stage 3检测完成协调回归 | 产生2NG/1OK后清NG，核对总数/NG/合格率及下一帧 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮结果统计写入路径迁移，NG清零入口作为回归门禁，规则不改；Stage 4第一轮确认异常计数与NG清零相互独立，119项运行测试及主工程门禁通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RES-004 | 检测耗时 | 五模式检测完成 | 正式检测执行 | 各模式原计时范围→统一完成控制器的同产品呈现快照→Presenter更新`speedLabel`；中文格式文字使用代码页无关Unicode转义 | 各模式当前计时范围不同 | 保持各模式当前整数/两位小数和“毫秒/ms”格式 | UI/日志 | 不在结构切片统一计时口径 | `ui/controllers/detection_completion_controller.*`+`ui/presenters/detection_result_presenter.*` | Stage 3检测完成协调迁移 | 每模式连续检测核对格式、刷新和日志耗时 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移耗时文本提交位置，五模式计时范围和格式不改，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RES-005 | 当前模板名 | 选择/保存/Profile命中/停止 | 模板状态存在 | 配方/模板选择仍走`updateCurrentTemplateName`；字库和二维码命中Profile由统一完成控制器的同产品快照选择性更新 | 单模板目录名或命中Profile名 | 显示当前/命中模板；无有效Pose时`--`，否则保持原选择名 | UI | 保持名称来源及条件更新 | `ui/controllers/detection_completion_controller.*`+`ui/presenters/detection_result_presenter.*` | Stage 3检测完成协调迁移 | 单/多模板选择与自动命中，核对名称和停止后状态 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮迁移检测期模板名提交，非检测期入口和名称来源不改，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| SAVE-001 | 按判定选择存图 | 正式结果完成 | 已设置保存模式和目录 | 五模式完成对象→统一完成控制器唯一产品门禁→ResultHandler选择不保存/NG/OK→`ImageSaveTask`→`ImageSaveService::submit` | 不保存/NG/OK/全部 | 只保存策略允许的判定；重复/倒序产品不再次提交；队列满时等待空位，不因容量丢弃 | 向容量32存图队列提交一个产品任务 | 选择语义保持；用户确认正常检测宁可减速也不能漏存 | `ui/controllers/detection_completion_controller.*`+`runtime/inspection_runtime_controller.*`+`runtime/image_save_service.*` | Stage 3检测完成协调迁移 | OK/NG命中保存策略并模拟重复完成对象，核对每产品只产生一组文件 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮将record后的存图决策与提交从Widget迁入统一完成控制器，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SAVE-002 | 标注图/原图组合 | 存图被允许 | 保存类型已设置 | Presenter按本产品原帧和Overlay生成标注图→统一完成控制器按保存类型生成同一产品任务→按标注图、原图顺序写盘 | 两者都存/仅标注/仅原图 | 使用组合存图的钢印、字库、二维码、纸巾四模式生成对应组合且同产品文件顺序不变 | 单任务短期持有原尺寸标注QImage和/或只读原帧 | 组合、目录和命名保持 | `ui/controllers/detection_completion_controller.*`+`runtime/image_save_service.*`+Presenter | Stage 3检测完成协调迁移 | 三种组合及四模式代表各跑1次，核对标注图与当前结果Overlay；OCR保持原帧单图入口 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮删除Widget组合判断和任务组装，保留四模式组合与OCR单原图差异，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SAVE-003 | 目录、命名与JPEG质量 | 任一保存任务 | 根目录可写 | 统一完成控制器生成原时间戳和`selectedDir/{ok,ng,ok_raw,ng_raw}`目标→传递显式质量→存图工作线程按`QImage::save(..., "JPG", 92)`编码落盘 | 当前时间和结果分类；五模式统一JPEG质量92 | 成功写对应JPG目录；创建/编码/写入失败累计并合并提示 | 后台创建目录、JPEG编码和写文件 | 路径、目录和同产品命名保持；格式按用户确认由PNG/JPG混用统一为JPEG 92 | `ui/controllers/detection_completion_controller.*`+`runtime/image_save_service.*` | 用户确认的存图性能优化 | 核对四类目录、`.jpg`扩展名、同产品基名、实际可打开图片和不可写目录红色警告 | 已验证 | S；T；U；2026-08-15用户选择JPEG 90～95并确认采用固定92；质量参数传递断言随136项运行测试通过，主程序集中门禁无问题 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SAVE-004 | OCR原图来源 | OCR结果触发原图保存 | 相机仍可取图 | OCR槽→`DetectionCompletion`→`saveImage2Async`直接持有本次检测只读原帧，不再向相机另取一帧 | 本次检测帧 | 保存帧与产生OCR判定的输入帧一致；空帧拒绝并记录日志 | 后台短期持有只读帧并写盘 | 计划内修复检测与存图错帧风险 | `TrackingTypes.h`+Widget临时结果桥 | Stage 2当前切片 | 移动物体连续OCR，像素对照检测图和raw文件 | 已验证 | S；T；U；DetectionCompletion测试及主程序同帧存图门禁由用户确认无问题 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| SAVE-005 | 异步写盘容量与失败 | 每个需保存结果 | 磁盘正常/慢/满 | `ImageSaveService`两个工作线程；在写+待写产品任务合计容量32 | 正常异步；容量满时提交者持有当前图并等待空位，不丢任务；实际写失败累计 | 短期持有最多32个服务内产品任务，另由当前提交者持有等待任务；失败向UI报警 | 队列满可降低检测吞吐；实际写失败不改变已产生的算法结论、统计或PLC | `runtime/image_save_service.*` | Stage 2当前切片 | Fake慢盘锁定满队列提交等待、腾位后全部写入、失败累计；主程序连续检测核对检测数量与文件组数 | 已验证 | S；T；U；用户确认接受存图反压，更新后的测试和主程序门禁均无问题；不增加断电恢复等持久队列 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |

## 9. 多相机与独立工具

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| MC-001 | 多相机窗口入口 | 顶栏“多相机模式” | 非检测/非停止/非模板制作 | `on_MultiCameraMode_clicked`→new/show`MultiCameraWidget`；返回按钮→close | 无持久参数 | 当前基线可打开独立窗口；忙碌时提示；重复点击复用现存窗口；关闭后指针清空 | 创建/销毁窗口 | 2026-08-16 用户确认不再保留该入口 | 删除 | 删除窗口入口、创建/销毁逻辑及工程/UI引用；删除后确认主界面无入口 | 不再执行功能回归；阶段3执行零引用、Run qmake、Rebuild和主程序回归 | 已确认删除 | S；2026-08-16用户明确批准删除全部多相机功能 |
| MC-002 | 多相机可见控件现状 | 多相机窗口内扫描/打开/采集/停止/触发/保存按钮 | 窗口已打开 | `multicamerawidget.ui`；`MultiCameraWidget`只初始化两行和连接“返回”，其余按钮无信号接线 | UI静态默认值 | 当前点击其余按钮无业务动作，预览/状态保持占位 | 无设备/统计/PLC副作用 | 可达但未接线的占位 UI | 删除 | 删除占位窗口、控件、UI文件和所有工程项 | 阶段3执行文件不存在、工程清单零引用、Run qmake和Rebuild门禁 | 已确认删除 | S；2026-08-16用户明确批准连同占位窗口删除，不再延期补齐 |
| MC-003 | 双相机底层API | 当前无UI/脚本运行入口，仅编译进主工程 | 需另行代码调用 | `MultiCameraController`→2个`MultiCameraUnit`→Hikvision；`MultiCameraSyncManager`校验shotId/frameId/时间差 | 默认2台、软件触发、最大时间差5000us、要求相同frameId | API可扫描/open/start/trigger/grab；当前窗口未实例化Controller，生产统计/PLC未接入 | 若被调用会开2相机并持有帧 | 无生产入口底层 API | 删除 | 删除Controller、Unit、SyncManager、Provider及工程项，不保留无入口 API | 阶段3执行全仓`MultiCamera|IMultiCameraProvider`零引用及Qt Creator删除门禁 | 已确认删除 | S；2026-08-16用户明确批准删除全部无入口多相机底层API |
| TOOL-001 | 授权生成/读取工具 | 单独打开`tools/license_tool/LicenseTool.pro`构建的程序 | 与主程序相同Qt；输出目录可写 | 工具UI→`makeLicenseFile`/`licenseInfoText`→共享`LicenseCodec`，与RuntimeGuard使用同一实现 | 到期日默认当前+1年；默认输出工具目录`license.ini` | 生成加密授权并立即读回；无效/不可写提示 | 写授权文件 | 独立工程必须保留 | `tools/license_tool`原位 | 保留 | Qt Creator构建工具；生成未来/过期授权并由主程序分别验证 | 已验证 | S；U |
| TOOL-002 | BarcodeDecoder.dll重建与ABI | 独立构建脚本/应用启动组装 | VS2022+CMake+网络仅重建时；本轮Agent不执行 | `tools/barcode_decoder`→固定C ABI；`ApplicationStartup`构造`BarcodeDecoderAdapter`；独立`IBarcodeDecoder`头供运行/检测依赖 | v2.3.0；仅Data Matrix；libdmtx先行15ms；ZXing Data Matrix后备；静态CRT | DLL返回码、内容、角点和耗时保持；非Data Matrix格式拒绝；缺DLL预检失败 | 适配器管理DLL生命周期，Widget不包含具体适配器头 | 唯一C ABI、版本日志、错误文本和调用约定保持 | `tools/barcode_decoder`+`startup/`+`engines/barcode/` | 删除QR旧链并调整解码顺序 | 重建DLL；核对2.3.0日志；固定Data Matrix成功/失败样本和QR拒绝；回归模板即时读码与正式检测 | 代码已实施，待用户验证 | S；静态确认CMake仅启用Data Matrix、API头路径有效、QR生产符号和旧libdmtx fallback路径零引用；待用户VS2022重建及现场验证 |

## 无独立可观察功能ID的源码候选

以下内容已检查普通调用、Qt自动槽、显式连接、定时器、回调、工程清单和动态入口。它们当前不对应独立可观察功能，因此不制造虚假功能ID；没有已验证替代链的候选继续保留，Stage 4只清理已确认零引用且已有正式替代入口的兼容代码。

| 候选 | 当前引用事实 | 处理决定 |
|---|---|---|
| `Zhuizong::createTrackerByName/getRandomColors` | `Widget`、`MyThread`和`CameraThread`会构造`Zhuizong`对象，但全仓没有调用这两个方法；构造本身无副作用 | 保留原文件和构造，待后续清理阶段再次做零引用确认；不得在当前基线切片删除 |
| `Widget::timer1` | 只在构造初始化，未连接、未启动、未读取 | 保留，当前无可观察行为 |
| `Widget::on_eliminatebutton_clicked` | `.ui`不存在名为`eliminatebutton`的控件，全仓无显式连接；实际剔除位置通过PLC参数应用和`wrongindex`路径生效 | 保留孤立槽，当前不登记成可达按钮功能 |
| `lineBoxIndex` | `.ui`中明确`visible=false`，源码无读写；实际合格率使用`lineBoxIndex_6` | 保留隐藏占位，不登记成当前可见功能 |
| PaddleOCR和Snap7内部实现 | 作为第三方/现有集成源码分别由DET-004和PLC-001..007的调用链覆盖，无额外用户入口 | 保留原位；本轮不把库内部辅助函数逐一伪装成业务功能 |

## 入口覆盖检查

- [x] 所有主页面、顶栏按钮、设置页按钮、输入控件、状态显示和对话框入口已从`widget.ui`反向核对。
- [x] `InspectionImageCanvas`的自适应显示、定位框、二维码框、多边形、闭合、重置、坐标换算、状态清理和事件转发已拆分登记。
- [x] 显式连接、Qt自动槽、定时器、软硬采集线程、相机回调和PLC写入已核对。
- [x] 模板制作、加载、字符切割、多Profile编辑、模式记忆、文件覆盖和失败回退已核对。
- [x] 设置默认、保存、加载、清除、恢复、dirty状态和硬件可编辑性已核对。
- [x] 五模式的正常、产品NG和失败收尾调用链已登记；固定实际样本仍待用户提供。
- [x] 统计、清零、存图组合、目录、日志、授权、单实例、部署和退出已核对。
- [x] 多相机入口、未接线控件和无入口底层 API 已登记，并于2026-08-16取得删除授权；授权工具和BarcodeDecoder独立工程继续保留。

## 状态统计

| 状态 | 数量 | 功能ID/说明 |
|---|---:|---|
| 待盘点 | 0 | 无 |
| 已基线 | 0 | 无 |
| 迁移中 | 18 | `SYS-004..005、UI-002、UI-005、DET-008、CAM-001..002、CAM-004、RUN-001..004、PLC-001..002、PLC-005..007、RES-001` 等待各自专项统一验证 |
| 代码已实施，待用户验证 | 2 | `DET-004` 等待深度OCR整体验收；`DET-006` 等待字符目标专项人工回归 |
| 已验证 | 65 | 当前未被待验证专项重新打开的功能 |
| 已延期 | 0 | 无；原`MC-002..003`延期结论已被2026-08-16删除决策取代 |
| 已确认删除 | 4 | `MC-001..003`、`SET-013`；用户已确认删除全部多相机功能和在线恢复默认设置 |

## 未决差异与已知基线风险

| ID | 计划规定/期望 | 源码当前行为 | 是否影响结果/硬件 | 处理决定 | 确认人/证据 |
|---|---|---|---|---|---|
| DIFF-001 | Stage 1形成纸巾阈值唯一默认6.0 | 当前代码已删除`.ui`静态5.2和检测器进程级5.2默认；`GlobalSettings`默认引用`TissueRecipeParameters`的6.0，线程启动前复制显式参数 | 计划内行为修正；可消除绕过主窗时的阈值分歧 | 2026-08-12 Agent静态核对和用户Qt Creator主程序/Pipeline测试门禁通过，差异已关闭 | 计划3.1/阶段1；SET-010/DET-005 |
| DIFF-002 | 模板阈值由Recipe唯一来源 | 私有设置和初始化代码默认70，`.ui`静态文本80 | 可能影响首次显示，但构造后通常为70 | 记录，迁移时以当前构造后实际值和模板私有值为基线 | S；TPL-013 |
| DIFF-003 | 未来系统故障不进入产品质量分母 | 当前到达正式收尾的读码失败、无定位、无纸卷等多按产品NG计数并可能触发PLC | 是 | Stage 1-3保持当前；硬件/故障策略不得提前进入Stage 4 | 计划3.5/阶段4；DET-008 |
| DIFF-004 | 检测帧通过`DetectionCompletion`短期交接 | 当前切片已删除OCR结果后另取相机帧，五模式统一携带判定对应原帧 | 修复存图对应性，不改判定 | Stage 2实现完成，2026-08-18用户统一门禁通过，差异关闭 | SAVE-004 |
| DIFF-005 | 存图队列容量32、双写线程且满时反压不丢图 | 原实现每图一次`QtConcurrent::run`，无统一容量；首版容量8拒新在实际运行中累计漏存24个任务 | 影响内存/吞吐；用户确认正常生产宁可降低检测速度也不能漏存 | Stage 2实现满时等待；连续检测核对结果数与文件组数 | SAVE-005 |
| DIFF-006 | 多相机全部删除 | 窗口可打开，但除“返回”外可见按钮均未接Controller；底层类无当前运行入口 | 删除后不影响单相机；可消除占位功能误导和无入口维护成本 | 2026-08-16用户批准删除；阶段3与新相机采集核心替换同阶段删除并复验 | 新终局方案阶段3；MC-001..003 |
| DIFF-007 | 相机和PLC边界后续分离 | “打开相机”会先尝试连接PLC，PLC失败仍继续开相机 | 影响设备操作时序 | Stage 1-3保持，Stage 2只用适配器复现现有顺序 | CAM-001/PLC-001 |
| DIFF-008 | 检测ROI外扩碰边时裁到原图边界 | 原模板匹配日期ROI外扩20像素后若整体落到图外会逐帧弹窗并跳过检测 | 按用户明确要求，20像素改为期望边距；边缘不足时使用0..width-1/height-1边界，只在裁剪后无有效面积时失败 | `DetectionRoiGeometry`统一模板匹配、字库和二维码日期ROI边界；2026-08-14离线边界测试及主程序靠边模板由用户确认通过 | DET-002、DET-003、DET-006、UI-002、RUN-002 |
| DIFF-009 | 五模式生产存图统一使用JPEG质量92 | 原钢印、字库、二维码和纸巾请求PNG，深度OCR请求未显式质量的JPG | 用户明确选择JPEG 90～95并采用中间值92，以降低PNG编码积压；不改变保存范围、原图/标注图组合、目录和失败报警 | `DetectionCompletionSaveOptions`显式传递quality，`ImageSaveService`调用三参数`QImage::save`；2026-08-15运行测试136项及主程序集中门禁通过 | SAVE-001..005 |
| DIFF-010 | Fault 不发送猜测性 NG | 运行故障进入自动停止 | `InspectionRuntime` 保留已经完成正式结果收口的产品；尚未收口的产品只计入停止摘要中的未确认数量，不计入产品 NG 或合格率 | PLC 写失败只进入 Fault 并自动停止，不产生猜测性 49 | `UI-002、UI-005、RUN-001..004、PLC-005..007、RES-001` |
| DIFF-011 | “相机延时(ms)”与SDK `TriggerDelay`语义 | 历史UI `cameraDelay`作为软件线程节流，SDK `TriggerDelay`固定为0 | 2026-08-18用户明确批准：软触发不受该参数控制；参数改名“硬触发延时(ms)”并控制海康`TriggerDelay` | 已修改正式消费者：软触发删除最小间隔；硬触发启动时ms×1000写入MVS µs节点；Schema 1历史JSON键名暂不改，避免制造第二份设置格式 | 已解决；关联`CAM-003..004、RUN-005、PLC-004` |

## 基线资源

| 资源 | 路径/来源 | 覆盖功能 | 可重复条件 | 当前状态 |
|---|---|---|---|---|
| 五模式样本清单 | `tests/baseline/sample_manifest.tsv` | DET-002..008、RES、SAVE、PLC | 用户填写每模式OK/NG/FAILURE的固定图、模板、文本、Overlay、计数、存图、PLC和耗时 | 已建清单，15份实际证据待用户 |
| 纸巾离线基线测试 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | SET-010、DET-005 | Qt Creator打开`tests/tests.pro`，Run qmake、Build并运行测试 | 2026-08-12用户确认纸巾Pipeline切片的4项业务测试全部通过，汇总`6 passed, 0 failed` |
| 生产主程序构建 | `app/AutoOCRproject.pro` | 全部主程序功能 | 当前本地 Qt Creator Kit：Qt 5.15.2/MSVC2019 64-bit，Release，Run qmake、Rebuild、Run | 2026-08-12 用户在当时的 Qt 5.14.2/MSVC2017 Kit 上确认含纸巾 Pipeline 和深度 OCR Pipeline 的主程序正常构建运行；2026-08-29 Agent 使用 Qt 5.15.2 qmake，并在 VS2022 Developer Command Prompt 下完成 MSVC x64 Release 全量编译、链接及部署；当前 Qt 5.15.2/MSVC2019 Kit 尚无单独的用户门禁记录 |
| 运行与性能记录 | `docs/development/OCRGangYin重构执行记录.md` | 内存、P50/P95、慢盘、停止/重启 | 用户按执行记录步骤填写真实数值 | 待用户验证 |
