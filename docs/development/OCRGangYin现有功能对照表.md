# OCRGangYin 现有功能对照表

> 原始功能基线版本：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`；2026-08-16 新终局治理基准：`8595cb2`。本表按当前生产源码、UI、工程、资源和脚本人工反向核对；不以目标架构推测现有行为。2026-08-16 用户批准删除 `MC-001..003`，并批准 Fault 不再对唯一未结论产品猜测性补发 NG；本阶段只更新治理状态，不表示删除代码已经完成。

## 状态与证据约定

- `待盘点`：尚未追踪完整调用链。
- `已基线`：已记录当前输入、输出、副作用和可重复验证方法；若证据写“U”，仍需用户提供实际运行值，Stage 0门禁尚不因此自动通过。
- `迁移中`：本阶段实现已接入或正在接入，但尚未通过本轮用户集中门禁；该状态不表示允许保留计划内旧路径。
- `已验证`：新路径通过约定验证，旧路径可进入引用清理。
- `已延期`：升级计划明确延期，现有实现保持原位。
- `已确认删除`：用户明确同意，且已记录影响。
- 证据缩写：`S`=已在基线HEAD完成源码/UI/工程静态核对；`T`=已有离线测试源码、等待Qt Creator执行；`U`=等待用户从原入口、真实设备或固定样本确认；`P`=升级计划明确延期。

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
| `UI-002、SET-004..005` | Widget只收集脏参数名称、展示确认/错误并调用应用服务；继续运行时由`SettingsApplicationService`丢弃草稿，按钮状态由`RuntimeSnapshot`和模板编辑状态计算；设置页的保存、默认、清空继续只经设置应用服务写Store | UI不再自行决定检测运行状态，也不直接构造启停控制器；设置页不再持有或写`MachineSettingsStore` | 预检提示顺序、未应用参数取消/继续及原值恢复；运行中关键参数禁用；普通停止不误触发Fault样式复位 |
| `CAM-001..002` | 打开/关闭命令统一进入`InspectionApplicationService`并返回结构化结果；保持先尝试PLC连接、PLC失败不阻止打开首台相机、曝光越界调整后事务保存以及忙碌时拒绝关闭的既有语义 | 相机开关状态不再由Widget布尔字段保存；开关入口不再自行判定重复运行状态 | 无PLC开相机、相机枚举/打开/曝光失败提示、空闲关闭、检测中和模板制作中拒绝关闭 |
| `PLC-001..004` | 延迟连接、打开相机附带连接、手动连接/断开、触发模式、工艺参数和拍照距离命令统一经`InspectionApplicationService`进入既有Runtime/PLC端口；启动执行使用本次MachineSettings快照下发 | UI入口不再直接调用PLC连接、断开、触发和工艺参数命令；失败不更新已应用值 | Fake验证连接/断开、0/1触发值、固定地址与大端顺序、首错停止；真实PLC和现场时序继续标为待验 |

阶段3实际调用链影响范围：`SYS-009、UI-002..003、SET-006..010、TPL-001..002、DET-001..007、CAM-001..006、RUN-001..006、PLC-001、MC-001..003`，共33项。2026-08-17用户确认阶段3集中门禁验证完成；除`MC-001..003`继续保持`已确认删除`外，其余30项已由`迁移中`恢复为`已验证`。

### 阶段3已完成调用链覆盖

阶段3门禁完成时，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。阶段3差异已经完成Agent静态门禁和用户Qt Creator集中验证；进入阶段4后，以后文阶段4状态为当前状态。

| 当前状态 / 功能ID | 当前唯一正式路径 | 已删除旧路径或保持边界 | 本轮集中门禁 |
|---|---|---|---|
| `已验证`：`SYS-009、UI-002..003、TPL-001..002、CAM-001..003、CAM-005..006` | `ApplicationStartup`构造唯一vendor设备、`CameraSession`和应用服务；相机开关、曝光/增益、模板实时预览/冻结/重取/退出都经应用服务进入同一Session；预览帧不创建`ProductKey` | Widget、TemplateEditorController和TemplateMatch不再持有相机设备或采集线程；旧相机操作/恢复Controller和模板预览线程已删除 | 真实相机开关、曝光/增益；预览、冻结、退出和再次进入；空闲/预览/检测中关闭程序后进程退出 |
| `已验证`：`SET-006..010、DET-001、CAM-003..005、RUN-001、RUN-005..006` | MachineSettings运行快照映射为`FramePreprocessSettings`与`CameraSessionCaptureConfiguration`；软件触发不消费硬触发延时，硬触发把界面ms值换算为µs写入海康`TriggerDelay`；旋转/通道统一进入`FramePreprocessor` | 两个旧采集线程内重复的旋转、通道、软件延时和图像缓冲逻辑已删除；没有UI临时参数或第二套默认 | 硬触发延时0/非0；四种旋转、彩色/红/绿/蓝；软硬触发停止与重启 |
| `已验证`：`DET-002..007、CAM-003..004、RUN-001..004` | `InspectionPositioner`复用迁移后的`detection/positioning/TrackingPoseMatcher`，对单Profile和多Profile共享同一预处理帧并选择Pose；`CameraSession`只向现有阶段4前Runtime提交正式帧/Pose | `Zhuizong`、根目录TrackingPoseMatcher副本以及MyThread/CameraThread中的两套定位/Profile分发已删除；五种Pipeline、阈值、判定、正常统计和PLC结果链未改 | 五模式软触发；硬触发Fake/现有相机路径；单/多Profile定位；固定样本判定与Overlay不变 |
| `已验证`：`SYS-009、CAM-003..004、RUN-002..006` | 单一`CaptureWorker`仅拥有一个`std::thread`；停止固定执行停止标志→`ICameraDevice::interruptWait()`→`join()`；软触发、硬触发和预览共用该生命周期 | `MyThread`、`CameraThread`、`InspectionAcquisitionController`、`InspectionWorkerConfigurator`、启动/停止/恢复Transition及超时泄漏兜底全部删除；无detach/terminate/放弃所有权 | 连续停止/重启；等待帧时停止；检测中/预览中退出；确认无残留线程 |
| `已验证`：`CAM-001..005、PLC-001` | `devices/camera/vendor/HikvisionCameraDevice`直接调用MVS；保持首台相机、软件TriggerSource=7和硬件stop→200ms→Line0→曝光/增益/TriggerDelay→回调/start→LineDebouncerTime=5000→100ms；停止后按旧合同关闭、100ms、重开软件触发 | `CMvCamera`、旧Hikvision函数表/Native桥、ReadBuffer/latestImage等实现泄漏API已删除；MVS类型只存在vendor目录；PLC仍只保持原连接尝试和正常合同，本阶段不改Fault或PLC结果逻辑 | 真实相机首开/关闭/参数；软触发五模式；硬触发现有路径；停止后相机恢复并可再次启动 |

阶段4实际调用链影响范围：`SYS-008、SYS-010、UI-001..005、SET-005、DET-001..008、CAM-001..002、RUN-001..006、PLC-001..007、RES-001..005、SAVE-001..005`，共41项。范围来自当前唯一生产链的反向追踪：统一Runtime替换会覆盖进程退出、延迟PLC连接、模式类型、运行状态和相机忙碌门禁；五种Pipeline、结果呈现、统计、PLC结果、存图及Fault收口属于同一不可拆分结果事务。2026-08-17用户确认阶段4Qt Creator集中门禁“没问题”，这41项已统一恢复为`已验证`。

### 阶段4已验证调用链覆盖

阶段4最终差异已经建立`InspectionRuntime`、`PipelineRegistry`、`ResultService`、`RuntimeSnapshot`和`InspectionPresentation`，并删除旧Runtime Controller/Transaction/Fault/Reconciler、自动补建会话、UI结果协调以及UI侧PLC/存图编排。下表覆盖后续各ID行保留的阶段0历史基线描述；发生冲突时以下表为当前事实。正常算法、统计、PLC 0/49→约100ms→0、存图和UI语义保持；Fault只停止新正式受理、保留已有算法结论并把未完成产品记为`Unconfirmed`，不补发兜底49。

| 影响功能ID | 当前唯一正式路径 | 本阶段已删除的旧路径 | 本轮门禁重点 |
|---|---|---|---|
| `SYS-008、CAM-001..002、PLC-001..004` | 延迟连接、相机附带连接、手动连接/断开、触发模式和工艺参数继续由`InspectionApplicationService`命令进入唯一`InspectionRuntime`与`InspectionPlcController`；启动参数只取本次MachineSettings快照 | `InspectionRuntimeController`及UI侧启动PLC参数编排已删除；Widget不再持有PLC结果脉冲状态 | 无PLC不阻止主窗/开相机；Fake连接、断开、0/1、固定地址/大端顺序；真实PLC仍不冒充验收 |
| `SYS-010、UI-001..003、SET-005` | 五模式使用稳定`DetectionMode`；运行状态与只读`RuntimeSnapshot`来自唯一Runtime；结果图由Runtime内纯渲染器生成，再由UI绑定应用；运行中硬件参数门禁继续查询同一状态 | 旧模式整数Worker工厂、UI目录反向依赖、第二套结果状态和运行Controller查询已删除 | Release Run qmake/Rebuild；五模式控件/图像/Overlay；运行中参数禁用和退出释放 |
| `UI-004..005、RES-001..005` | 每个`ProductKey`只允许一次`ResultService`事务：形成完整`InspectionPresentation`，正常统计更新一次，再经容量1邮箱整体呈现；Fault使用独立异常统计和“视觉检测已暂停、输送线状态未知”说明 | `InspectionResultCoordinator`、`DetectionCompletionController`、旧ResultHandler、分散五模式收尾和Fault兜底计数展示已删除 | 五模式图文/模板名/判定/耗时/统计同产品；重复结果只呈现/统计一次；Fault不改变已有判定 |
| `DET-001..008` | `PipelineRegistry`按PreparedRecipe中的稳定模式唯一选择纸巾、钢印、字库、OCR或二维码+三期Pipeline；全部Worker容量1并把类型化完成结果交给唯一ResultService | `DetectionModeWorkerFactory`、`DetectionSession`及Widget中的模式装配/结果出口已删除；算法失败不切换其他算法 | 五模式固定样本与资源；定位失败仍形成既有NG；系统故障与产品NG严格分离 |
| `RUN-001..006` | 每次启动只创建一个不可变`InspectionRunContext`，冻结runId、开始时间、MachineSettings、PreparedRecipe和Profile；Runtime唯一拥有Worker、产品账本、Fault和容量1呈现邮箱；停止按采集→Worker→相机恢复协作完成 | 启动/停止Transaction、旧FaultState、ProductReconciler、自动补建运行会话和并行运行状态已删除 | 五模式启停/重启、软硬触发背压、重复结果、Fault首因、停止后线程退出及下一次预检 |
| `PLC-005..007` | 正常OK写0；正常NG写49并由ResultService约100ms后写0；延迟NG队列携带原始ProductKey且只输出一次；任一写失败进入Fault并停止新的正式受理 | UI回调写PLC、旧完成控制器/ResultHandler队列及“唯一未结论产品兜底49”分支已删除 | Fake锁定0、49→0、延迟位置、重复去重、写失败零兜底；不声称机械剔除成功 |
| `SAVE-001..005` | ResultService按运行设置快照每产品最多提交一个`ImageSaveTask`；`ImageSaveService`保持容量32、两个写线程、队满阻塞且不丢任务，原图/标注图仍来自同一帧 | UI侧保存模式判断、任务拼装和结果协调器私有存图服务已删除 | 四种策略、三种图像组合、OCR同帧原图、JPEG 92、不可写报警和慢盘反压 |

正式功能当前状态为待盘点0、已基线0、迁移中3、已验证84、已延期0、已确认删除3。2026-08-18用户批准`CAM-003、CAM-004、RUN-005`的新节拍语义，正式代码与Schema已接入，等待本轮Qt Creator主程序及真实海康相机门禁后恢复为`已验证`。
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
| SYS-004 | 日志写入与保留 | 启动后任意Qt日志 | 可执行目录可写 | `ApplicationStartup::run`→`ApplicationLogger::install`→`qInstallMessageHandler` | `<exe>/log/app_log_yyyy-MM-dd.txt`；保留3个月 | 写入时间、级别、文件/行；目录或文件不可写时不能落盘 | 创建日志目录/文件；删除3个月前日志 | 日志格式和保留策略纳入回归 | `system_support/logging/` | 保留 | 启动后触发一次提示，核对当日日志；放置过期测试日志后重启核对清理 | 已验证 | S；U |
| SYS-005 | 崩溃记录 | Windows未处理异常 | 日志目录可写 | `ApplicationStartup::run`→`WindowsCrashHandler::install`→`WindowsCrashStack` | Windows异常上下文；进程崩溃时生效 | 尝试写崩溃信息后进程终止；写盘失败无业务恢复 | 写崩溃日志 | 保留现有诊断，不用测试性崩溃污染生产 | `system_support/crash/` | 保留 | 仅在隔离调试构建按现场批准方案验证；Stage 0记录源码链 | 已验证 | S；U |
| SYS-006 | OCR模型初始化 | 应用启动时组装设备工厂 | `config1.txt`、模型和字典存在 | `ApplicationStartup`提供`PaddleOcrEngine`工厂→`Widget`在原初始化位置取得`IOcrEngine`→运行Worker使用接口 | 配置相对路径以应用目录解析；启动生效 | 模型加载成功后OCR可用；配置键/模型异常保持构造期失败 | 启动组合定义具体实现；适配器独占模型原生对象 | 模型、阈值、路径解析和初始化日志顺序保持 | `startup/`+`devices/ocr/` | 最终架构启动组装 | 有效部署启动、日志顺序2..5；实际OCR代表帧 | 已验证 | S；T；U；2026-08-15用户确认深度模型可启动检测，工厂注入后主工程正常 |
| SYS-007 | 公共设置与模板恢复 | `Widget`构造 | 用户AppData可读 | `Widget::loadSettings`→`AppSettingsManager::loadGlobalSettings`→`applyGlobalSettingsToUi`→`restoreTemplatesForMode` | `AppDataLocation/settings.ini`，配置v2；无效时默认 | 恢复模式、保存、相机/PLC、模板历史和分隔条；读失败使用默认并记日志 | 读取设置和模板资源 | 详见SET/TPL功能ID | `recipes/`+`system_support/settings/` | 保留后拆分 | 修改并应用设置、退出重启，逐项核对；损坏INI核对默认回退 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SYS-008 | 启动PLC延迟连接 | 主窗构造后1秒 | 运行控制器持有PLC控制器 | `Widget`保留原1秒定时入口→`InspectionRuntimeController::connectPlc`→类型化PLC控制器→Snap7 | 保存的IP/Rack/Slot；1秒后生效 | 成功连接并刷新硬件控件；失败仅日志/状态，不阻止主窗 | 建立PLC网络连接 | 延迟和提示保持 | `runtime/inspection_plc_controller.*`+UI命令桥 | 最终架构PLC边界收口 | 有PLC/无PLC各启动一次，记录1秒后状态和可编辑控件 | 已验证 | S；T；U；2026-08-15用户确认无PLC连接失败不影响主窗；真实PLC成功连接继续延期 |
| SYS-009 | 正常退出与资源释放 | 关闭主窗/进程退出 | 可有运行线程、相机、PLC | `Widget::closeEvent`/析构→协作停止线程/关相机→运行控制器断PLC；`ApplicationStartup`从事件循环正常返回并析构启动组合 | 线程等待上限和当前设备状态 | 正常关闭；线程未及时退出仅记录警告，不调用`terminate()` | 停线程、关设备、断PLC、释放OpenCV窗和启动组合 | 必须保持协作停止、无残留线程 | `runtime/`+`startup/`+`devices/` | 最终架构设备所有权收口 | 检测中、模板预览中、空闲时分别关闭；确认进程退出和设备释放 | 已验证 | S；T；U；2026-08-15用户确认软触发停止及关闭主窗口后进程正常退出 |
| SYS-010 | Release运行时部署校验 | Qt Creator Release链接后 | `dist/ShengYin`完整 | `AutoOCRproject.pro::QMAKE_POST_LINK`→`system_support/deployment/deploy_runtime.ps1` | 源`dist/ShengYin`、目标构建`release`；Release链接后 | 校验清单/哈希并复制DLL、模型、配置；缺失或不一致使部署脚本失败 | 写Release运行目录 | 保持部署可复现；构建只由用户执行 | `system_support/deployment/` | 保留 | Qt Creator Run qmake+Release Rebuild，核对部署结果和脚本报错 | 已验证 | S；U |

## 2. 主界面与交互

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UI-001 | 模式与参数页面 | “识别模式”下拉框 | 非检测/非模板忙碌状态 | `comboBox_4::currentIndexChanged`→`setupDetectModeChangeTracking`→存旧模式路径→`restoreTemplatesForMode`→更新可见参数；本切片统一钢印与字库家族的当前模板编辑区并恢复钢印字符切割入口 | 界面固定顺序：模板匹配、字库匹配、深度模型、纸巾检测、二维码+三期；内部ID依次为stamp/word/ocr/tissue/barcode_word | 切换对应参数和模板历史；钢印显示单Profile编辑选择及字符切割；无效历史保留空状态并可提示 | 保存当前模式和模板历史 | 五个现有入口不可丢失，钢印不能退化为仅重叠检测 | `ui/pages/` | 保留 | 依次切换五模式，核对控件显隐、模板名、历史恢复；钢印编辑区与字库家族布局一致 | 已验证 | S；U；2026-08-13用户确认钢印编辑区、字符裁切入口及两种单模板模式切换均正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| UI-002 | 操作状态与按钮使能 | 开相机、预览、冻结、检测、停止、关闭 | 任意主流程状态变化 | `updateOperationUiState`+`updateHardwareParameterUiEnabled`；检测ROI外扩碰边时直接裁到原图边界；Fault人工恢复只有在产品收口完成后才能解除锁定 | `CameraClosed/CameraReady/TemplatePreviewing/TemplateFrozen/Detecting/Stopping/Fault` | 只允许当前状态合法动作；Fault只保留人工确认恢复入口；未收口产品或PLC复位失败继续保持Fault；边缘ROI继续检测 | 控件enable/style变化 | 状态机可观察行为保持；Fault必须人工确认且产品收口完成 | `ui/controllers/main_page_controller.*`+运行控制器 | Stage 4异常恢复迁移 | 喷码区域靠四边连续检测并点击停止；制造PLC断连或硬触发过载，核对Fault锁定、产品收口和人工恢复 | 已验证 | S；T；U；第三轮130项自动测试及无PLC主程序门禁通过；真实PLC Fault锁定/恢复继续延期补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| UI-003 | 图像自适应显示 | 相机帧、检测结果、模板原图 | `ImageLabel`有图像 | `DetectionResultPresenter`按原图绘制结果Overlay并输出QImage，Widget薄桥继续交给`ImageLabel::setAutoFitPixmap/resizeEvent`按宽高比缩放居中 | 当前控件尺寸 | 缩放但不改变原图；空图清空 | Presenter短期生成QImage，ImageLabel仅缓存QPixmap | 保持缩放、居中和重绘 | `ui/presenters/detection_result_presenter.*`+后续`ui/widgets/image_label.*` | Stage 3结果绘制职责已迁移 | 用横图/竖图并调整窗口，核对比例、居中和Overlay位置 | 已验证 | S；T；U；2026-08-15用户确认五模式结果图、Overlay及窗口缩放均正常，ImageLabel物理移动留后续独立切片 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| UI-004 | 结果帧绑定显示 | 任一模式产生正式检测结果 | 检测运行 | 容量1 `UiCompletionMailbox`整体交付结果→五模式收尾生成同一`ProductKey`只读呈现快照→`DetectionResultPresenter::present`一次应用 | 生产检测启用结果绑定；邮箱满时检测线程等待 | 结果图不被实时帧覆盖；图片、框、文字、模板名、判定、统计和耗时在同一UI调用中替换 | 容量1 UI完成邮箱反压检测线程；快照短期持有最终QImage | 保持画面、框、OK/NG、统计和耗时来自同一`ProductKey` | `ui/controllers/detection_completion_controller.*`+`runtime/result_presentation_mailbox.*`+`ui/presenters/detection_result_presenter.*` | Stage 3检测完成协调迁移 | 软硬触发连续检测并移动产品，确认图像、框、模板名、OK/NG、统计和耗时同步替换 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮将五模式结果快照、统计、存图及PLC副作用顺序从Widget迁入统一完成控制器，门禁已通过 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| UI-005 | 结果与状态展示 | 任一检测完成或状态变化 | 已启动检测 | 正常结果仍由`DetectionResultPresenter`统一显示“正确/错误”；系统Fault由`InspectionFaultPresenter`生成红色持续告警，恢复时显示兜底NG请求数和未确认产品数 | 识别文本、模板Profile、耗时、判定或Fault快照 | 正常五模式继续统一“正确/错误”；Fault明确输送线状态未知，未确认产品要求现场隔离且不伪装产品NG | 正常结果同一UI调用整体更新；Fault冻结正式结果画面并锁定普通操作 | 正常判定样式不变；系统故障不得伪装产品NG或声称输送线已停 | 两类Presenter+运行控制器+Widget恢复薄桥 | Stage 4异常提示迁移 | 五模式正常结果回归；制造PLC断连或硬触发过载核对红色持续告警、收口摘要和人工恢复 | 已验证 | S；T；U；第三轮130项自动测试及无PLC主程序门禁通过；真实PLC红色告警、摘要和恢复继续延期补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| UI-006 | 模板引导与提示 | 制作模板、绘图事件、悬停 | 模板预览或冻结 | `setupTemplateGuide`→`handleTemplateGuideEvent`→`updateTemplateGuideText`；`eventFilter`延迟500ms工具提示 | 当前模式与已画点数 | 显示分步引导和模式专用说明；离开隐藏 | 创建/调整引导Frame | 保持中文提示和步骤含义 | `ui/template_editor/` | 保留 | 五模式进入制作模板，悬停按钮并执行绘图，核对引导变化 | 已验证 | S；U ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| UI-007 | 防滚轮误改参数 | 鼠标滚轮经过下拉框/SpinBox | 主窗活动 | `Widget::eventFilter`拦截`QComboBox/QAbstractSpinBox`的Wheel | 所有安装事件过滤器的控件 | 滚轮被丢弃，点击/键盘仍可修改 | 无 | 防误操作行为保持 | `ui/` | 保留 | 记录值，滚轮后不变；点击选择后可变 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| UI-008 | 软件数据目录快捷打开 | 双击只读目录框 | AppData目录可创建/打开 | `eventFilter`→`QDir::mkpath`→`QDesktopServices::openUrl` | `AppSettingsManager::globalSettingsDirPath()` | 打开目录；创建/打开失败弹提示 | 可能创建目录并启动资源管理器 | 保留入口 | `ui/settings_page.*` | 保留 | 双击目录，核对资源管理器路径；只读失败场景记录提示 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| UI-009 | 右侧分隔条记忆 | 拖动参数/结果区域分隔条 | 主窗已加载 | `QSplitter::saveState`→全局设置；启动`restoreState` | 字节状态；退出/保存后生效 | 重启恢复；无效状态回默认并记日志 | 写全局设置 | 保持布局记忆 | `ui/layout/` | 保留 | 拖动、退出、重启；再注入无效状态核对回退 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |

## 3. 设置与参数应用

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SET-001 | 全局设置读取与安全保存 | 启动及各“设置/确认”按钮 | AppData可访问 | `AppSettingsManager::loadGlobalSettings/saveGlobalSettings`→临时文件验证→替换/回滚；本切片把按模式已发布配方UUID恢复从字库家族扩展到钢印与深度OCR | 配置版本2；当前用户AppData；每个模式至多一个已发布配方UUID | 合法值加载；未知模式的UUID记忆被丢弃；旧配置没有新键时保持兼容；保存失败保留旧文件并报告/日志 | 读写`settings.ini`及临时/备份 | 整机设置语义保持，新增键为可选兼容字段 | `system_support/settings/` | 保留后封装 | 四种模板模式分别选择配方、切换及重启恢复；损坏配置和不可写目录回退 | 已验证 | S；T；U；2026-08-13用户确认钢印/OCR切换恢复、模式隔离及旧入口覆盖正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-002 | 整机默认值 | 首次启动或恢复默认 | 无有效全局设置 | `defaultGlobalSettings`→`applyGlobalSettingsToUi`；恢复默认同时清空已发布配方模式记忆 | 字库模式、全不保存、仅标注、彩色、无旋转、触发启用、间歇、曝光800、增益1、PLC `192.168.10.10/0/1`、纸巾6.0；配方UUID记忆默认为空 | UI采用默认且不自动选择配方；硬件未必立即写入 | 默认可被保存 | Stage 1保持既有默认并增加空配方记忆 | `recipes/machine_settings.*` | 保留/计划内优化 | 备份并清除设置后启动，确认不自动选配方；不覆盖用户现有文件 | 已验证 | S；T；U；空UUID默认与恢复默认清理已由静态检查覆盖，设置往返测试和主程序回归通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-003 | 模板私有设置 | 选择、保存、编辑模板 | 模板目录可读写且没有外部程序正在浏览其内部目录 | 旧INI继续映射为规范`RecipeProfile`；本切片将Widget对草稿/编辑会话的候选复制、参数/资产校验及发布成功提交收口到统一`TemplateRecipeWorkflow`，四模板模式共享相同事务边界 | Profile目标、阈值、定位框、字符框、二维码参数、顺序和模式特有资产保持；纸巾不进入模板会话 | 工作流只在完整校验和正式发布成功后替换会话状态；失败保留上一会话与正式配方 | 旧Profile继续写INI和原目录；新配方由RecipeStore整目录替换 | 后续迁入Recipe，不要求兼容旧格式 | recipes工作流事务+Widget薄桥 | Stage 1分步迁移 | 四模板模式发布/重发；失败后继续编辑；旧入口回归 | 已验证 | S；T；U；2026-08-13用户确认统一工作流测试及可执行主程序路径无问题；深度OCR因当前无旧模板未单独执行配方重发；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| SET-004 | 未应用标记与启动确认 | 编辑带绑定的参数 | 参数可编辑 | `setupGlobalSettingBindings`/模板dirty跟踪继续生成脏项；启动顺序由`InspectionStartPreflight::evaluateAccess`统一判定，Widget只展示原确认框并在继续时恢复已应用值 | UI值与`m_appliedGlobalSettings`/Profile比较 | 启动前列出未应用项；取消不启动；继续会恢复已应用值后运行；恢复后按最新已应用PLC触发状态继续预检 | 标签变化；可能丢弃未应用UI值 | 保持“应用”和“编辑”边界 | `runtime/inspection_start_preflight.*`+Widget提示桥，后续迁入`ui/settings_controller.*` | Stage 2启动门禁统一 | 修改相机、PLC、阈值但不确认，启动后分别选取消/继续；确认脏项先于PLC未连接提示 | 已验证 | S；T；U；2026-08-14用户确认运行测试37项及主程序未应用参数取消/继续、原值恢复和启动行为均无问题；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| SET-005 | 运行中参数禁用 | 检测/模板状态变化 | 硬件或模板操作进行中 | `updateHardwareParameterUiEnabled`→相机运行状态+`InspectionRuntimeController::isPlcConnected`+`registerHardwareAction` | 相机打开、PLC连接、操作状态 | 运行中禁止会改变设备/关键参数的控件；不同连接状态允许不同字段 | 控件状态改变 | 防止中途改运行快照 | `ui/settings_controller.*`+运行控制器窄查询 | 最终架构设备查询收口 | 在空闲、相机开/关、PLC连/断、检测中逐项核对 | 已验证 | S；T；U；既有运行中参数门禁证据保留，2026-08-15主程序设备查询边界回归正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-006 | 相机曝光应用 | 曝光“设置”或检测启动 | 相机已打开 | `on_sureButton_clicked`→`queryCameraExposureRange`→`ICameraDevice::get/setFloatValue` | 整数曝光；相机SDK给最小/最大；默认800 | 范围内写入；越界/SDK失败提示；打开相机时保存值会按范围调整并提示 | 写相机`ExposureTime`；成功保存设置 | 数值与生效时机保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 最小、最大、越界、正常值各一次；重启开相机核对 | 已验证 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-007 | 相机增益应用 | 增益“设置”或检测启动 | 相机已打开 | `on_pushButton_12_clicked`→`applyCameraGainFromUi`→`ICameraDevice::get/setFloatValue` | 整数；SDK范围；默认1 | 合法写入并保存；空/越界/SDK失败提示 | 写相机`Gain` | 保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 边界/越界/正常值，重启核对 | 已验证 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-008 | 颜色通道应用 | “颜色通道→确认”或检测启动 | 线程存在 | UI索引先经`InspectionRunConfiguration::parseSettings`映射为0..3运行码，再由Widget原`choosechannel`信号送入`MyThread/CameraThread::receivecolorchannel*` | 彩色/红/绿/蓝；默认彩色 | 后续帧按选定通道处理；无效索引仍回彩色0 | 更新线程参数并保存 | 通道映射保持 | `runtime/inspection_run_configuration.*`+后续detection input transform | Stage 2运行配置统一 | 纯逻辑覆盖0..3和越界回退；主程序切换代表通道启停 | 已验证 | S；T；U；2026-08-14用户确认运行测试44项、代表通道设置及主程序启停正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-009 | 图像旋转应用 | “图像旋转→设置”或检测启动 | 线程存在 | UI索引先经`InspectionRunConfiguration::parseSettings`映射为0..3运行码，再由Widget原`rotate`信号送入采集线程 | 无/顺90/逆90/180；默认无 | 后续采集图旋转；无效索引仍回无旋转0 | 更新线程参数并保存 | 旋转方向保持 | `runtime/inspection_run_configuration.*`+后续detection input transform | Stage 2运行配置统一 | 纯逻辑覆盖0..3和越界回退；主程序切换代表旋转启停 | 已验证 | S；T；U；2026-08-14用户确认运行测试44项、代表旋转设置及主程序启停正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-010 | 纸巾粗糙度阈值 | 纸巾模式“设置”及启动 | 值>0 | `InspectionRunConfiguration::parseSettings`保持原`toDouble`与大于0门禁→Widget显式构造`TissueRecipeParameters`→启动前复制到两采集线程 | 唯一代码默认由`TissueRecipeParameters`提供6.0；有已保存整机设置时使用保存值 | 合法值固定为本次运行线程的参数副本；非法仍用原提示且不启动 | 保存整机设置；不再更新进程级检测器默认 | Stage 1唯一6.0来源保持，Stage 2抽出运行参数校验 | `runtime/inspection_run_configuration.*`+`recipes/product_recipe.*` | Stage 2运行配置统一 | 纯逻辑覆盖合法、小于等于0和非数字；纸巾启动回归 | 已验证 | S；T；U；2026-08-14用户确认纸巾阈值显示、整图启动和停止均正常 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-011 | 存图策略设置 | 保存模式、类型、路径浏览 | 主窗空闲 | UI改变→`syncImmediateGlobalSettingsFromUi`→`saveSettings`；浏览按钮选目录 | 不保存/NG/OK/全部；两类都存/仅标注/仅原图；默认不保存+仅标注 | 选项控制后续存图；未选目录时依现有路径逻辑；浏览取消不变 | 写全局设置 | 详见SAVE功能 | `recipes/save_policy.*` | 保留后迁移 | 逐组合选择、重启，核对显隐和实际文件 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-012 | 清空当前软件数据 | 设置页按钮 | 用户二次确认 | `clearCurrentSoftwareData`→删除全局设置→重置UI/状态 | 只针对当前用户软件数据 | 确认后恢复默认公共设置；取消不变；失败提示 | 删除`settings.ini`；不删除模板、图片、授权、日志 | 删除范围必须保持 | `system_support/settings/` | 保留 | 在测试用户数据中确认/取消各一次，核对保留项 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| SET-013 | 恢复默认设置 | 设置页按钮 | 用户确认；设备状态决定可立即应用项 | `restoreDefaultGlobalSettings`→按相机/PLC连接状态选择性恢复→dirty刷新 | `defaultGlobalSettings` | 可立即项恢复；不能立即写硬件项保持`*`待应用并提示 | 改UI/已应用设置，可能写设置 | 状态相关语义保持 | `ui/settings_controller.*` | 保留 | 相机开/关、PLC连/断四组合执行并核对星号/提示 | 已验证 | S；U ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |

## 4. 模板制作、加载与编辑

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| TPL-001 | 实时模板预览 | “制作模板”第一次点击 | 相机已打开、未检测 | `on_VideoShoot_clicked`→`startTemplatePreview`→软触发`MyThread`→`ICameraDevice`→`signal_templatePreviewFrame` | 当前曝光/增益/旋转/通道 | 显示实时画面和引导；相机/线程错误提示并退出预览 | 启动采集线程；不统计/PLC | 预览入口保持 | `ui/template_editor/`+`devices/camera/` | Stage 2相机适配 | 五模式分别进入预览，核对状态、按钮、实时画面 | 已验证 | S；T；U；2026-08-13用户确认模板预览与主程序集中门禁无问题；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-002 | 冻结、重拍与退出 | 预览中再次点制作模板；已有选择时重拍；退出按钮/Esc | 有最近预览帧 | `freezeTemplatePreview`/`resetTemplateCaptureState`/`stopTemplatePreview`→相机接口停止唤醒 | 最近克隆帧 | 冻结当前帧并允许绘图；重拍有确认；退出清理选择并回相机就绪 | 协作停止预览线程、清缓存 | 保持状态转换 | `ui/template_editor/`+`devices/camera/` | Stage 2相机适配 | 冻结、重拍取消/确认、Esc/退出各一次 | 已验证 | S；T；U；既有深度OCR冻结/绘图/保存及2026-08-13相机适配主程序集中门禁均无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-003 | 定位矩形绘制 | 冻结模板图后左键拖动 | 钢印/字库/深度OCR/二维码模式 | `ImageLabel::mousePress/Move/Release`→`m_trackingRect`→引导事件 | 显示坐标，保存时换算到原图 | 形成归一化定位框；过小/无框不能完成保存 | 仅UI选择状态 | 独立登记ImageLabel行为 | `ui/widgets/image_label.*` | 保留后移动 | 不同比例窗口画框，核对显示与保存后物理框 | 已验证 | S；U；深度OCR实际模板发布证明定位框已形成并完成坐标保存 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-004 | 二维码矩形与即时读码 | 二维码模式第二个矩形 | 已有定位框 | `ImageLabel`→`validateBarcodeTemplateRect`→`IBarcodeDecoder::decode`；DLL与通用策略已迁入设备适配器 | BarcodeOptions默认DataMatrix、padding8%、预算60ms、fallback开 | 可读则保留框并继续日期多边形；不可读弹原因、清二维码/日期但保留定位框 | 适配器动态加载/调用`BarcodeDecoder.dll` | 二维码必须先验证 | `ui/template_editor/`+`devices/barcode/` | Stage 2已适配 | 可读、不可读、越界二维码框各一次，核对清理范围 | 已验证 | S；T；U；2026-08-13用户确认假ABI测试、实际框选即时读码和主工程均无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-005 | 日期多边形绘制与闭合 | 左键逐点、右键闭合 | 已有前置框 | `ImageLabel`多边形状态→`signal_templateGuideEvent` | 至少3点 | 闭合后可保存/提示；点数不足保持绘制；Esc清理当前选择 | UI状态 | 保持鼠标/键盘语义 | `ui/widgets/image_label.*` | 保留后移动 | 2点右键、3+点右键、Esc，核对状态与提示 | 已验证 | S；U；深度OCR实际模板发布及随后检测启动证明日期多边形已闭合并落盘 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-006 | 坐标换算与通用模板保存 | “保存模板”及动态“发布当前模板/模板组” | 单模板保存需框完整；模板组发布需字库家族已加载至少两个旧Profile | 坐标/资源生成不改；草稿发布及成功后编辑会话建立由统一工作流一次完成，Widget不再分别编排两类会话 | 显示名、模式、Profile顺序、参数和资产命名空间保持；钢印配方包含独立`stampRing`角色 | 发布或编辑会话建立失败不替换Widget当前工作会话；正式目录仍由RecipeStore保证 | 旧目录只读作为发布源；成功时事务写入`AppDataLocation/recipes/<UUID>` | 计划内改为Recipe整目录安全保存 | recipes workflow/publisher/store+Widget薄命令 | Stage 1分步迁移 | 四模板模式首次发布、随后编辑、UUID和失败保持 | 已验证 | S；T；U；统一工作流自动测试及字库/二维码/钢印实际发布回归通过；深度OCR当前无旧模板未单独发布 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-007 | 钢印环与钢印区域标定 | 钢印模板保存及已发布配方选择 | 通用框已保存 | 旧入口仍写`template_ring.bmp`；本切片选择钢印配方时从`stampRing`和标定资产原子初始化候选重叠引擎 | OpenCV交互ROI；相对吸管口中心坐标；配方内目标为`assets/profiles/<n>/template_ring.bmp` | 环图、YAML或钢印区域无效时拒绝选择且保留当前引擎 | 旧入口仍写模板/YAML；新发布只事务复制到配方目录 | 保持标定次序、坐标及检测行为 | recipes资产合同+Widget运行资源桥 | Stage 1分步迁移 | 钢印配方选择后重叠引擎可启动；缺环/坏环/缺钢印区拒绝 | 已验证 | S；T；U；2026-08-13用户确认钢印配方选择、启动/停止及旧模板回归正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-008 | 单模板选择与校验 | 旧“选择模板”及动态“已发布配方” | 空闲状态 | 本切片把钢印和深度OCR已发布配方选择接入统一列表，并在完整解码定位图/YAML/钢印环后一次安装；旧目录选择保留 | UUID、当前`DetectionMode`和模式必需角色；钢印需要tracking/calibration/stampRing，OCR需要tracking/calibration | UUID/Schema/资源/模式、图像解码或运行标定任一无效即拒绝且不改写当前模板 | 新路径只读配方目录；旧入口仍更新历史和引擎 | 保持失败不误启动和旧入口兼容 | recipes selection/assembler/store+Widget薄桥 | Stage 1分步迁移 | Stamp/OCR选择、模式错误、坏资源、输出保持及旧入口回归 | 已验证 | S；T；U；2026-08-13用户确认两模式已发布配方选择、参数显示、启停及旧入口覆盖正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-009 | 字库家族多Profile选择 | 旧“选择模板”多目录入口；新增“发布当前模板组”和“已发布配方” | 空闲状态 | 编辑态保存完整有序Profile；启动时深拷贝为本次运行专用Profile快照，定位选择和检测只读快照 | 配方内部Profile顺序、名称、参数、资源路径、资产键及UUID保持；运行期间编辑缓存与运行缓存隔离 | 快照准备或原有预检失败不启动；停止/线程结束释放运行快照；旧入口保持 | 发布/选择继续更新编辑会话，启动仅复制内存资源且不写盘 | 新格式以一个产品配方承载一个或多个Profile并自动选择 | recipes选择/编辑会话+Widget启动薄桥，后续迁入runtime | Stage 1分步迁移 | 两模式启动后Profile选择、停止/再次启动、重选/重启和旧入口 | 已验证 | S；T；U；2026-08-13用户确认两个字库家族及旧多目录入口启停/再次启动均正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-010 | 当前Profile编辑器 | 多Profile加载或模板组发布后下拉选择 | 至少一个Profile | 编辑器继续读编辑态`m_wordTemplateProfiles`；启动前形成独立运行Profile快照，运行检测不再回读编辑态Profile | Profile顺序、名称、目标、阈值、资源键及原图绝对路径保留 | 运行中控件继续禁用；停止后编辑缓存保持并可再次启动 | 编辑显示与运行资源在内存中分离；不新增磁盘写入 | 保持编辑对象与运行快照边界 | recipes编辑会话+Widget资源桥+后续`ui/template_editor/` | Stage 1分步迁移 | 启动/停止前后编辑显示保持，检测只读快照 | 已验证 | S；T；U；2026-08-13用户确认停止后Profile、目标字符和阈值保持且可再次启动；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-011 | 单Profile目标字符 | “确认字符”及当前Profile“分割字符模板” | 当前模板有效且检测停止 | 目标解析和资源预检保持；当前或完整Profile候选由统一工作流校验并同UUID发布，Widget只安装成功结果 | `RecipeProfile::targetText`、字符框及当前Profile内部字符资产 | 字符缺失、Schema或事务失败时工作会话、正式配方和运行字符缓存均保持上一版本 | 旧Profile行为不变；新Profile不写旧INI，成功时同UUID事务替换 | 保持字符解析和提示 | recipes workflow+Widget资源桥 | Stage 1分步迁移 | 四模板模式单Profile目标、同UUID和失败保持 | 已验证 | S；T；U；参数事务测试和字库/二维码/钢印实际同UUID重发无问题；深度OCR人工项未执行；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-012 | 批量目标字符 | 多Profile配方“批量确认字符”及单Profile裁切 | 已加载多个Profile | 完整有序Profile候选的原子校验和单次发布收口到统一工作流，Widget不再先原地更新编辑会话 | 每Profile目标、顺序和资源命名空间独立；一次操作最多一次配方发布 | 任一验证或事务失败时工作会话和正式配方同时保留上一版本 | 旧目录逐项写INI；新配方只经一次整目录事务提交 | 保持旧批量结果与多Profile资产隔离 | recipes workflow+Widget薄接入 | Stage 1分步迁移 | 多Profile批量目标、单次发布、失败后重试和UUID保持 | 已验证 | S；T；U；统一工作流自动测试及字库家族批量重发回归无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-013 | 单Profile图像阈值 | 阈值“设置” | 当前Profile有效且检测已停止 | 阈值范围和生效顺序保持；候选Profile由统一工作流同UUID发布成功后Widget才更新运行阈值 | `RecipeProfile::imageThreshold`，保持0..100百分比语义和当前生效顺序 | Schema或事务失败时工作会话、正式配方和运行阈值保持上一版本 | 旧Profile行为不变；新Profile不写旧INI | 保持百分比语义 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 四模板模式阈值、同UUID和失败回退 | 已验证 | S；T；U；参数事务测试和可用模板模式实际同UUID重发无问题；深度OCR人工项未执行；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-014 | 批量图像阈值 | 多Profile配方“批量设置阈值” | 多Profile已加载 | 原按钮校验、缓存/`ssim`/dirty顺序保持；完整Profile更新和单次事务提交由统一工作流完成 | 每Profile阈值不被重排；一次按钮操作最多一次配方发布 | 任一Profile验证或事务失败时工作会话与正式配方均保留上一版本 | 旧目录逐项写INI；新Profile只经一次整目录事务提交 | 保持旧逐项结果和整配方原子提交 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 多Profile批量阈值、`ssim`、单次发布和失败重试 | 已验证 | S；T；U；统一工作流自动测试及字库家族批量阈值回归无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-015 | 手动字符模板切割 | “分割字符模板” | 字库家族或钢印模式、Profile有原图/定位框/date_poly且检测已停止 | 裁切对话框和临时工作区保持；Profile资产替换、整配方校验及同UUID发布由统一工作流的候选副本完成 | 框、变体名、Profile顺序、目标路径、UUID和对话框交互保持；钢印按原精确字符文件名规则重载 | 取消不发布；资产映射、Schema或事务失败时工作会话、正式配方和当前缓存均保持原值 | 旧字符文件仍写旧目录；已发布配方只写临时工作区和RecipeStore事务目录 | 保持绘图、排序、变体和清理范围 | 原对话框+recipes workspace/workflow+Widget交互 | Stage 1分步迁移 | 字库/钢印已发布配方裁切、失败保持、同UUID及重选 | 已验证 | S；T；U；字符资产事务测试及钢印/字库实际保存、同UUID重发回归无问题 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |
| TPL-016 | 按模式记忆模板 | 配方发布/选择、切换模式、退出和下次启动 | 全局设置可写 | 本切片将钢印和深度OCR单配方UUID纳入同一按模式恢复规则，旧路径保留回退 | 每个模式的UUID和配方Profile彼此保持 | 发布后安装失败不覆盖旧路径记忆；恢复失败清理失效UUID并回退旧路径 | 读写全局`settings.ini` | 保持模式隔离、失败原子性和旧路径兼容 | 设置桥+recipes选择装配+Widget薄接入 | Stage 1分步迁移 | 四模板模式切换/重启恢复及旧路径覆盖规则 | 已验证 | S；T；U；2026-08-13用户确认钢印/OCR来回切换恢复、配方不串模式及旧路径覆盖正常；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第3轮集中门禁通过（详见执行记录） |

## 5. 五种检测与共同算法行为

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| DET-001 | 共同定位与位姿 | 软/硬采集线程获得帧 | 非纸巾模式且模板有效 | `MyThread/CameraThread`→`TrackingPoseMatcher::setTemplate/match`→`DetectionPose`→`dispatchDetectionByMode` | tracking模板；-45..45度、步长2、金字塔0.2、阈值0.3 | 匹配成功映射日期/二维码多边形；失败产生对应NG路径或跳过 | 线程持有模板旋转缓存；无直接PLC | 定位范围、阈值和坐标变换保持 | `detection/common/pose_matcher.*` | 保留后迁移 | 固定角度/位移/无目标样本，记录pose、Profile和结果 | 已验证 | S；U ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| DET-002 | 模板匹配模式（内部钢印+字符检测） | 界面“模板匹配”（索引0）检测帧 | 有tracking、date、ring/stamp和字符模板 | Widget只准备当前运行参数和重叠检测回调，运行时分发器装配`StampDetectionPipeline` Worker并把完整输出交给统一完成控制器 | 启动时快照目标字符、阈值、字符模板和钢印引擎 | 字符数等于目标且零重叠才OK；资源/定位/匹配/重叠异常语义保持 | Overlay、统计、存图、PLC/剔除队列只在统一结果入口执行一次 | 两条件AND、失败文本与硬件时序不变 | `runtime/detection_mode_worker_factory.*`+`detection/stamp/`+`ui/controllers/detection_completion_controller.*` | Stage 3检测完成协调迁移 | Pipeline、工厂及分发合同测试；硬触发实际OK/字符NG/重叠NG、停止重启和软触发回归 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，不改钢印与字符AND判定，门禁已通过 |
| DET-003 | 字库多Profile模式 | 界面“字库匹配”（索引1）检测帧 | 至少一个完整Profile | `InspectionProfileSnapshotBuilder`一次生成同序定位/检测快照，运行时分发器负责Profile索引门禁、`WordDetectionPipeline`调用并把完整输出交给统一完成控制器 | 每Profile目标、字符图、阈值；最佳定位Profile；同分保留先出现Profile | 快照准备或原预检失败不启动；运行判定、无定位和NG路径不变 | Profile名/框显示、统计、存图、PLC只在统一结果入口执行一次 | 自动选择、字符计数、失败NG和Profile显示语义保持 | 公共Selector+`runtime/inspection_profile_snapshot.*`+字库Pipeline+统一完成控制器 | Stage 3检测完成协调迁移 | 快照/分发/Pipeline合同测试；硬触发多Profile命中/定位失败/OK/NG、停止重启及软触发回归 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，Profile选择和判定不变，门禁已通过 |
| DET-004 | 深度OCR模式 | 模式2检测帧 | OCR设备已初始化、模板日期区域有效 | Widget只冻结目标文本和OCR接口，运行时分发器装配`OcrDetectionPipeline` Worker并把同帧Pose及完整输出交给统一完成控制器 | 启动时快照目标文本；按字节清洗规则、换行拼接和Paddle返回顺序保持 | 清洗文本非空且精确相等OK；无效ROI仍不形成产品判定；资源装配失败不替换当前模板 | 识别文本、统计、异步存图、PLC只在统一结果入口执行一次 | 精确比较、清洗、Overlay和收尾保持 | `runtime/detection_mode_worker_factory.*`+`detection/ocr/`+`devices/ocr/`+统一完成控制器 | Stage 3检测完成协调迁移 | OCR/工厂/分发合同测试；真实模型硬触发OK/NG、停止/重启及软触发回归 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，OCR清洗和精确比较不变，门禁已通过 |
| DET-005 | 纸巾卷粗糙度模式 | 模式3采集线程 | 相机帧；不需传统模板 | Widget只提供运行阈值，运行时分发器装配`TissueDetectionPipeline` Worker并把纸巾完整输出交给统一完成控制器 | 运行参数副本中的粗糙度阈值；算法找内孔、外圆和环粗糙度 | 找到卷且score<threshold为OK；空图、无圆、外轮廓失败或score>=阈值为NG并带诊断 | Overlay、统计、存图、PLC | 当前边界是`>=`判NG；不改纸巾算法和阈值 | `runtime/detection_mode_worker_factory.*`+`detection/tissue/`+统一完成控制器 | Stage 3检测完成协调迁移 | 纸巾Pipeline/工厂/分发合同测试；主程序连续OK/NG、停止、重启，核对score/阈值/圆框/统计/存图/PLC | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，粗糙度算法和阈值边界不变，门禁已通过 |
| DET-006 | 二维码优先+三期模式 | 界面“二维码+三期”（索引4）检测帧 | Profile含tracking、二维码4点、日期多边形、字符模板，DLL可用 | 一次生成同序定位/检测快照，运行时分发器负责Profile门禁、策略状态、二维码优先Pipeline并把完整输出交给统一完成控制器 | DataMatrix/QR格式掩码、padding8%、预算60ms、fallback、运行内首选策略；同分保留先出现Profile | 配方装配、资源、DLL、ROI或快照预检失败不启动；读码失败短路日期并形成一次NG | 显示码内容/日期状态、统计、存图、PLC只在统一结果入口执行一次 | “最高分Profile、读码优先、失败短路”和硬触发取帧条件保持 | 公共Selector+`runtime/inspection_profile_snapshot.*`+`detection/barcode_word/`+统一完成控制器 | Stage 3检测完成协调迁移 | 快照/分发/Pipeline合同测试；硬触发可读日期OK/NG、不可读/定位失败逐触发收尾、停止重启和软触发回归 | 已验证 | S；T；U；用户确认Qt Creator集中门禁无问题；本轮仅迁移检测后副作用编排，读码优先和日期判定不变，门禁已通过 |
| DET-007 | 定位失败收尾 | 字库家族采集时无有效pose | 已启动检测 | 普通字库和二维码软硬触发均把无效Pose作为同一产品工作项提交Worker并形成一次NG | 软触发由正式链反压形成节拍；硬触发每个新回调帧 | 显示定位失败NG；二维码硬触发保证本次触发有且只有一次收尾 | 增总数/NG、可存图、PLC或排队 | 软硬触发差异和Stage 4异常归类边界保持 | `runtime/result_handler.*`+各模式Pipeline | Stage 3软硬触发失败收尾已验证 | 移出视野：软触发保持；硬触发逐次打光记录结果数和PLC | 已验证 | S；T；U；2026-08-15用户确认硬触发集中门禁无问题，定位失败正式出口已统一 |
| DET-008 | 算法/系统失败当前统计语义 | 模板缺失、读码失败、无圆、无定位、PLC断连或硬触发FIFO满 | 检测已启动或启动预检 | 算法失败仍按现有NG收尾；基础设施故障进入Fault；`InspectionProductReconciler`按ProductKey区分未检测、算法已完成和已正式记录产品 | 系统故障、取消、未确认与Fault后丢弃独立统计 | 普通算法失败继续计入产品NG；算法结论已完成但尚未正式记录时只记未确认，不用系统NG覆盖 | Fault取消Worker/UI邮箱并拒收新正式帧；收口账本只持有身份不持图；普通存图失败只报警 | 产品NG、已提交算法结论与系统Fault严格分离 | `runtime/inspection_fault_state.*`+`inspection_product_reconciler.*`+`result_handler.*` | Stage 4异常分类迁移 | 回归五模式算法NG；覆盖Fault前未检测、算法已完成、正式结果PLC失败和Fault后新帧 | 已验证 | S；T；U；新增7项收口合同并由用户确认总计130项通过；真实PLC异常仍延期补验 |

## 6. 相机、采集线程与运行控制

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CAM-001 | 扫描并打开首台相机 | “打开相机” | 无运行任务、相机未开 | `on_HandwareDetect_clicked`→相机枚举/打开流程；伴随PLC连接改经`InspectionRuntimeController`类型化PLC命令，具体设备在`main`启动组装 | 保存的曝光/增益及PLC地址；固定选第0设备 | 成功进入CameraReady；无设备/打开/曝光失败提示并回滚相机；PLC失败不阻止继续开相机 | 连接PLC、启动抓图 | 固定第0台及伴随PLC连接是当前行为 | `startup/main`+`runtime/inspection_plc_controller.*`+`devices/camera/` | 最终架构设备所有权收口 | 0/1/多相机场景及PLC在线/离线组合，记录选择和状态 | 已验证 | S；T；U；2026-08-15用户确认无PLC条件下相机可正常打开并完成软触发检测 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| CAM-002 | 关闭相机 | “关闭相机” | 非检测、非停止、非模板制作 | `on_CloseCamera_clicked`→`ICameraDevice::close`→清图/文本/计数→CameraClosed | 当前状态 | 成功释放相机并清零显示；忙碌时拒绝并提示先停止 | 关闭SDK、清UI统计和缓存 | 清理范围保持 | `devices/camera/`+UI控制器 | Stage 2薄适配 | 空闲关闭；检测中/模板中尝试关闭，核对拒绝；再正常关闭 | 已验证 | S；T；U；2026-08-13用户确认相机开关与主程序集中门禁无问题；2026-08-15用户确认最终Qt Creator门禁通过：detection_completion_test 112项、recipe_store_test 31项及主程序操作状态、未应用参数和跨模式模板记忆均正常 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 ；2026-08-15固定四轮第2轮：146项运行测试及主程序集中门禁通过 |
| CAM-003 | 软触发采集 | 未勾PLC触发后“启动识别” | 相机开、模板预检通过 | `CaptureWorker`每轮执行SoftwareTrigger并等待本次触发的新帧；纸巾整帧及OCR/钢印/字库/二维码定位任务提交容量1检测队列，完成结果再经容量1 UI邮箱反压 | 不读取硬触发延时，不附加人为最小间隔或固定休眠 | 每次正式检测使用新相机帧；检测/UI/存图越慢会通过有界链路反压采集，快时最高受相机速度限制；不丢正式输入 | 单采集线程+单检测线程+容量1 UI完成邮箱 | 旋转/通道、算法、PLC和存图规则不变；软触发输入节拍由相机速度与实际处理能力驱动 | `runtime/capture_worker.*`+`frame_queue.*`+`detection_worker.*`+`result_presentation_mailbox.*` | 当前正式采集链 | 五模式连续检测/停止/重启；硬触发延时0和非0时软触发节拍均不受影响 | 迁移中 | S；U；2026-08-18用户批准断开该参数与软触发的联系，待Qt Creator主程序现场复验 |
| CAM-004 | 硬触发采集 | 勾“启用触发”并启动，外部Line触发 | PLC已连、相机支持Line0 | `InspectionRunPlan`生成HardwareTrigger；停止采集后切Line0，应用曝光/增益及海康`TriggerDelay`，再注册回调并启动采集，正式帧提交容量1检测链 | `LineDebouncerTime=5000`；界面“硬触发延时(ms)”乘1000写入MVS `TriggerDelay`（µs） | 外部沿到达后由相机按TriggerDelay延时曝光；正常节拍每个有效硬触发按序处理新帧；初始化失败提示；队列溢出进入Fault | MVS回调进入采集线程并向有界FIFO提交 | 保持Line参数、注册/启动顺序和正式帧受理规则 | `runtime/camera_session.*`+`capture_worker.*`+`frame_queue.*`+`devices/camera/` | 当前正式采集链 | 真实相机分别设置0/1/300ms核对MVS节点和成像时刻；停止/再次启动 | 迁移中 | S；U；2026-08-18用户批准由该参数控制海康`TriggerDelay`，待Qt Creator真实相机复验 |
| CAM-005 | 帧读取与停止唤醒 | 采集线程调用或停止 | 相机抓图中 | `CMvCamera`原回调与条件变量保留→`HikvisionCameraDevice`委托帧读取/序号/停止唤醒→软硬触发线程 | 超时/非阻塞模式 | 返回克隆最新帧；停止请求唤醒等待；空帧/超时返回失败 | 持有最新cv::Mat和序号 | 不允许`QThread::terminate()` | `devices/camera/hikvision_camera_device.*` | Stage 3停止协调器接管调用顺序 | 连续采集、无帧超时、等待中停止，核对退出延迟 | 已验证 | S；T；U；2026-08-15用户确认软硬线程停止无超时、相机正常恢复且可再次启动；适配器和条件变量行为未改 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| CAM-006 | 采集前图像变换 | 每帧进入定位/算法前 | 已设置旋转/通道 | 启动时由`InspectionRunConfiguration`统一生成原0..3运行码，仍经原信号写入`MyThread/CameraThread`，线程内rotate/channel分支与先后顺序不改 | SET-008/009应用值 | 输出彩色或单通道派生图、指定方向；异常帧不进入正常检测 | 新cv::Mat临时内存 | 变换实现、顺序与方向保持 | `runtime/inspection_run_configuration.*`+后续`detection/input_transform.*` | Stage 2配置统一、Stage 3迁移变换 | 纯逻辑映射测试；代表旋转/通道主程序回归 | 已验证 | S；T；U；2026-08-14用户确认代表旋转/通道及两种采集模式画面正常 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-001 | 启动预检与快照 | “启动识别” | 相机开、非忙碌且非Fault | 原预检和硬件下发顺序保持；`InspectionRuntimeController`在Starting建立Worker/UI邮箱并为新运行清空产品收口账本 | 已应用设置、模式资源、非活动Fault | Fault未人工确认或仍有未收口产品时禁止重启；恢复后仍按原预检和参数快照启动 | 控制器持有Worker、模式、UI邮箱、Fault快照和小型ProductKey账本 | 正常启动时序保持；Fault恢复前零重新启动 | 既有运行配置+运行控制器+产品收口器 | Stage 4恢复门禁迁移 | 五模式正常启动回归；Fault未收口时确认失败、收口后再次启动且新账本为空 | 已验证 | S；T；U；第三轮130项自动测试及无PLC主程序门禁通过；真实PLC恢复后重启待补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-002 | 停止识别 | 顶栏“停止识别”或Fault“确认故障并恢复” | 检测中、线程运行或Fault锁定 | 正常停止保持原事务；Fault恢复先人工确认，再停止采集/Worker、恢复相机、完成产品收口，最后`acknowledgeFault` | 原采集等待上限；Fault首因、未收口ProductKey、PLC可写状态 | 取消、线程超时、PLC复位失败或仍有未收口产品均继续Fault；完成后才回Idle | 停采集/Worker并释放队列；收口只记录兜底请求或未确认 | 不使用`terminate()`；解除软件锁定不表示输送线已停 | 运行控制器+产品收口器+相机恢复转换器+Fault Presenter | Stage 4人工恢复迁移 | 正常停止；Fault确认取消/确认；单未结论、多未结论、复位失败和再次恢复 | 已验证 | S；T；U；正常停止和130项合同门禁通过；真实PLC复位失败及人工恢复继续延期补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-003 | 线程重建与信号接回 | 启动前、旧线程结束后、Fault进入和恢复 | 主窗存活 | Fault立即取消Worker/UI邮箱；等待实际退出后对Fault前身份收口；账本只存ProductKey，队列/邮箱/外部引用释放后不保留图像 | 相机接口、模板、运行参数、Fault状态 | Fault后Direct信号只登记丢弃帧；收口完成才能确认；后续启动重建全新队列、Worker和账本 | 取消队列和邮箱，释放帧引用，重建Worker | 不重复连接、不残留线程、不重复分发或持图 | 运行控制器+产品收口器+采集停止协调器 | Stage 4异常线程恢复迁移 | Fault前帧释放weak_ptr合同；Fault后持续进帧、确认恢复、再次启动 | 已验证 | S；T；U；图像引用释放和新运行空账本合同包含在用户确认的130项中；真实PLC恢复待补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-004 | 流帧与结果内存持有 | 相机持续采集/检测完成 | 主窗活动 | 软触发阻塞提交；硬触发非阻塞`trySubmit`满即Fault；Fault前已受理身份进入收口器，Fault后新帧只登记丢弃 | 软触发背压；硬触发容量1；PLC是否可写；未收口数量 | 单个无结论且PLC可写才允许一次兜底NG；已有算法结论或多产品只记未确认；Fault后零正式结果 | Fault取消队列/UI邮箱；收口器不持图；新帧不创建`ProductKey` | 软触发正常不丢；硬触发过载安全暂停且每个Fault前身份只收口一次 | 帧队列+Worker+运行控制器+产品收口器 | Stage 4硬触发过载迁移 | 非阻塞FIFO、单/多未收口、算法已完成、重复收口及图像释放合同 | 已验证 | S；T；U；单/多件、算法结论、重复收口和释放合同均在130项中通过；真实硬触发过载继续延期补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
| RUN-005 | 采集节拍与硬触发延时 | 软/硬触发正式采集 | 运行参数已应用 | 软触发按“软触发命令→新帧→正式检测链完成受理”串行循环；硬触发启动配置将机器设置ms值换算为µs写入相机 | 软触发无人工间隔；硬触发延时默认300ms、0表示相机不增加触发延时 | 软触发吞吐由相机、检测队列、UI邮箱及存图反压决定；硬触发由MVS节点延后曝光 | 软触发影响吞吐；硬触发参数影响外部沿到曝光的时刻 | 保持设置值单位为ms，设备边界显式使用µs | MachineSettings+CameraSession | 当前正式采集链 | 软触发在参数0/非0时吞吐不变；硬触发0/1/300ms核对节点与实际时序 | 迁移中 | S；U；2026-08-18语义经用户批准，待Qt Creator真实相机复验 |
| RUN-006 | 最近Overlay随位姿逻辑 | 非结果绑定模式收到新pose | 有上一检测框和pose | `slot_saveBoxesFromThread`→按角差/中心差旋转平移`g_lastDrawResults/g_lastStampPoly` | 新旧`DetectionPose` | pose无效清框；有效时框跟随；字库结果绑定时直接抑制 | 更新全局Overlay缓存 | 保持模式差异 | `ui/presenters/` | 保留后拆分 | 移动/旋转产品并移出视野，核对框跟随和清除 | 已验证 | S；U ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |

## 7. PLC与剔除

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLC-001 | 连接PLC | 启动延迟、打开相机附带连接、或“连接PLC” | IP/Rack/Slot可用 | 三入口→`InspectionRuntimeController::connectPlc`→`InspectionPlcController`→`IPlcDevice`→Snap7 | 默认`192.168.10.10/0/1` | 成功显示/使能状态；失败弹错误或日志，连接按钮路径不伪装成功 | 建立TCP/S7会话、写设置 | 三个入口和原顺序均保留 | `startup/main`+`runtime/inspection_plc_controller.*`+`devices/plc/` | 最终架构PLC所有权收口 | 在线/错误IP/错误rack-slot分别走三个入口 | 已验证 | S；T；U；2026-08-15用户确认无PLC失败路径及主窗可用；Fake连接合同通过，真实PLC在线路径延期 |
| PLC-002 | 断开PLC | “断开PLC”或退出 | PLC控制器存在 | UI命令/应用退出→`InspectionRuntimeController::disconnectPlc`→`InspectionPlcController`→设备接口 | 当前连接状态 | 成功断开并开放连接配置；失败提示 | 关闭PLC会话 | 保持 | `startup/main`+`runtime/inspection_plc_controller.*` | 最终架构PLC所有权收口 | 在线/离线点击断开及退出，核对状态 | 已验证 | S；T；U；2026-08-15用户确认关闭主窗进程正常退出；Fake断开合同通过，真实PLC在线路径延期 |
| PLC-003 | 触发工作模式下发 | 工作模式“确认”或启动 | PLC已连接 | UI读取模式→`InspectionRuntimeController::writePlcTriggerMode`→控制器内部编码并写`DB1.DBB1032` | 连续=0、间歇=1 | 成功保存设置；无连接/写失败提示且不更新已应用值 | PLC写`DB1.DBB1032` | 地址和值不变 | `runtime/inspection_plc_controller.*` | 最终架构PLC业务命令收口 | Fake锁定0/1字节；现场读回延期 | 已验证 | S；T；U；136项运行测试确认0/1字节合同；现场读回延期 |
| PLC-004 | 工艺参数下发 | “PLC参数→设置”或启动 | PLC已连接、整数可解析 | UI构造`PlcRunSettingsCommand`→应用服务→PLC控制器内部大端编码和固定顺序写入 | 剔除时间DB980 Word、剔除距离DB920 DWord、拍照时间DB982 Word、拍照距离DB924 DWord；“硬触发延时”只保存为机器设置并在硬触发启动时写相机，不下发PLC | 全部成功后保存；任一写失败提示并返回失败字段 | 多次PLC写入 | PLC地址、长度、顺序和值保持 | `runtime/inspection_plc_controller.*` | 当前正式PLC链 | 边界值、正常值和中途写失败；真实PLC读回延期 | 已验证 | S；T；U；2026-08-18明确硬触发延时归属相机MVS节点而非PLC |
| PLC-005 | OK输出 | 正式结果OK且PLC连接 | 检测收尾 | 完成控制器携带`ProductKey`→运行控制器类型化结果输出→PLC控制器写`DB1.DBB1033=0` | DB1偏移1033一字节，值0；PLC连接/返回码、产品身份 | 成功写0；失败不重复写、不覆盖算法OK、不静默继续生产 | PLC写0或Fault取消正式检测链；失败增加异常未确认数 | 正常OK值和唯一请求保持；失败归系统Fault | 完成控制器+运行控制器+PLC控制器 | 最终架构PLC输出收口 | Fake写0失败与去重合同；真实PLC延期 | 已验证 | S；T；U；136项运行测试确认固定地址和值0；真实PLC输出延期 |
| PLC-006 | NG脉冲输出 | 立即剔除NG或Fault单件兜底 | PLC连接 | 正常NG保持按`ProductKey`请求49→约100ms→0；具体字节写经运行控制器与PLC控制器，Widget不接触设备接口 | DB1.DBB1033；49持续约100ms；PLC可写和唯一身份 | 正常脉冲保持；49失败记未确认；49成功但0失败保持Fault直至复位，不伪造剔除成功 | PLC两次写；成功兜底记取消产品，失败记未确认 | 正常值/顺序/脉冲保持；Fault不会重复或盲发PLC | 完成控制器+产品收口器+运行控制器+PLC控制器 | 最终架构PLC输出收口 | 正常NG、失败和兜底合同；真实PLC延期 | 已验证 | S；T；U；136项运行测试确认固定地址和值49/0，既有约100ms合同保留；真实PLC延期 |
| PLC-007 | 延迟剔除队列与复位 | NG结果、每次产品收尾、“剔除复位” | `wrongindex`可能>0 | 原到期公式和顺序保持；到期输出携带原始`ProductKey`，具体写入经运行控制器PLC命令 | 剔除位置输入；PLC连接/返回码、原始产品身份 | 正常到目标计数触发一次49→0；到期输出失败进入Fault，不把当前产品误记为失败产品 | 运行层身份队列、PLC脉冲或Fault | 延迟公式和值不变 | 完成控制器+ResultHandler+运行控制器+PLC控制器 | 最终架构PLC输出收口 | wrongindex回归；Fake断线核对原始身份 | 已验证 | S；T；U；既有延迟身份合同与136项运行测试通过；真实PLC复位延期 |

## 8. 结果、统计与存图

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RES-001 | 总数、NG与合格率 | 每个正式检测收尾或Fault产品收口 | 结果到达UI槽或操作员确认恢复 | 正常完成仍唯一更新产品统计；Fault兜底NG记入异常取消数，无法唯一收口或PLC失败记未确认数，均不写正常产品统计 | 正常产品结论、Fault收口类型、ProductKey | 正常总数/NG/合格率公式不变；同一Fault产品重复收口被拒绝；未确认不伪装产品NG | 正常统计与异常统计独立；UI恢复摘要显示本次收口数量 | 产品质量合格率只使用OK与产品NG | 完成控制器+运行控制器+产品收口器+Presenter | Stage 4异常统计收口迁移 | 正常OK/NG；单件兜底、多件未确认、重复解析和统计清零相互独立 | 已验证 | S；T；U；正常统计既有门禁和第三轮130项异常收口合同均通过；真实PLC摘要待补验 ；2026-08-15固定四轮第1轮：139项运行测试及无PLC主程序集中门禁通过 |
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
| TOOL-002 | BarcodeDecoder.dll重建与ABI | 独立构建脚本/应用启动组装 | VS2022+CMake+网络仅重建时；本轮Agent不执行 | `tools/barcode_decoder`→固定C ABI；`ApplicationStartup`构造`BarcodeDecoderAdapter`；独立`IBarcodeDecoder`头供运行/检测依赖 | v2.1.0；DataMatrix+QR；libdmtx fallback15ms；静态CRT | DLL返回码、内容、角点和耗时保持；缺DLL预检失败 | 适配器管理DLL生命周期，Widget不包含具体适配器头 | ABI、版本日志、错误文本和调用约定保持 | `startup/`+`devices/barcode/` | 最终架构接口/实现分离 | 适配器测试、二维码Pipeline及实际读码 | 已验证 | S；T；U；2026-08-15用户确认适配器8项、Pipeline 10项及实际二维码读码正常 |

## 无独立可观察功能ID的源码候选

以下内容已检查普通调用、Qt自动槽、显式连接、定时器、回调、工程清单和动态入口。它们当前不对应独立可观察功能，因此不制造虚假功能ID；没有已验证替代链的候选继续保留，Stage 4只清理已确认零引用且已有正式替代入口的兼容代码。

| 候选 | 当前引用事实 | 处理决定 |
|---|---|---|
| `Zhuizong::createTrackerByName/getRandomColors` | `Widget`、`MyThread`和`CameraThread`会构造`Zhuizong`对象，但全仓没有调用这两个方法；构造本身无副作用 | 保留原文件和构造，待后续清理阶段再次做零引用确认；不得在当前基线切片删除 |
| `Widget::timer1` | 只在构造初始化，未连接、未启动、未读取 | 保留，当前无可观察行为 |
| `Widget::on_eliminatebutton_clicked` | `.ui`不存在名为`eliminatebutton`的控件，全仓无显式连接；实际剔除位置通过PLC参数应用和`wrongindex`路径生效 | 保留孤立槽，当前不登记成可达按钮功能 |
| `lineBoxIndex` | `.ui`中明确`visible=false`，源码无读写；实际合格率使用`lineBoxIndex_6` | 保留隐藏占位，不登记成当前可见功能 |
| PaddleOCR和Snap7内部实现 | 作为第三方/现有集成源码分别由DET-004和PLC-001..007的调用链覆盖，无额外用户入口 | 保留原位；本轮不把库内部辅助函数逐一伪装成业务功能 |
| `InspectionRuntimeController::markFault` | Stage 4第三轮复核为零引用兼容别名；正式故障入口已全部使用带原因和诊断的`enterFault` | 已删除别名；不改变任何可观察功能或故障原因 |
| 控制器直接`recordCancelledProduct/recordUnconfirmedProduct` | 生产路径零引用，且直接调用会绕过`ProductKey`唯一收口账本 | 已删除控制器公开入口；改由产品收口器解析后驱动内部异常统计，不登记为功能删除 |

## 入口覆盖检查

- [x] 所有主页面、顶栏按钮、设置页按钮、输入控件、状态显示和对话框入口已从`widget.ui`反向核对。
- [x] `ImageLabel`的自适应显示、定位框、二维码框、多边形、闭合、重置、坐标换算、状态清理和事件转发已拆分登记。
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
| 迁移中 | 0 | 阶段7最终统一门禁已通过 |
| 已验证 | 87 | 2026-08-17用户确认阶段7最终统一门禁全部通过 |
| 已延期 | 0 | 无；原`MC-002..003`延期结论已被2026-08-16删除决策取代 |
| 已确认删除 | 3 | `MC-001..003`；2026-08-16用户确认删除全部多相机功能 |

## 未决差异与已知基线风险

| ID | 计划规定/期望 | 源码当前行为 | 是否影响结果/硬件 | 处理决定 | 确认人/证据 |
|---|---|---|---|---|---|
| DIFF-001 | Stage 1形成纸巾阈值唯一默认6.0 | 当前代码已删除`.ui`静态5.2和检测器进程级5.2默认；`GlobalSettings`默认引用`TissueRecipeParameters`的6.0，线程启动前复制显式参数 | 计划内行为修正；可消除绕过主窗时的阈值分歧 | 2026-08-12 Agent静态核对和用户Qt Creator主程序/Pipeline测试门禁通过，差异已关闭 | 计划3.1/阶段1；SET-010/DET-005 |
| DIFF-002 | 模板阈值由Recipe唯一来源 | 私有设置和初始化代码默认70，`.ui`静态文本80 | 可能影响首次显示，但构造后通常为70 | 记录，迁移时以当前构造后实际值和模板私有值为基线 | S；TPL-013 |
| DIFF-003 | 未来系统故障不进入产品质量分母 | 当前到达正式收尾的读码失败、无定位、无纸卷等多按产品NG计数并可能触发PLC | 是 | Stage 1-3保持当前；硬件/故障策略不得提前进入Stage 4 | 计划3.5/阶段4；DET-008 |
| DIFF-004 | 检测帧通过`DetectionCompletion`短期交接 | 当前切片已删除OCR结果后另取相机帧，五模式统一携带判定对应原帧 | 修复存图对应性，不改判定 | Stage 2实现完成，等待Qt Creator门禁后关闭差异 | SAVE-004 |
| DIFF-005 | 存图队列容量32、双写线程且满时反压不丢图 | 原实现每图一次`QtConcurrent::run`，无统一容量；首版容量8拒新在实际运行中累计漏存24个任务 | 影响内存/吞吐；用户确认正常生产宁可降低检测速度也不能漏存 | Stage 2实现满时等待；连续检测核对结果数与文件组数 | SAVE-005 |
| DIFF-006 | 多相机全部删除 | 窗口可打开，但除“返回”外可见按钮均未接Controller；底层类无当前运行入口 | 删除后不影响单相机；可消除占位功能误导和无入口维护成本 | 2026-08-16用户批准删除；阶段3与新相机采集核心替换同阶段删除并复验 | 新终局方案阶段3；MC-001..003 |
| DIFF-007 | 相机和PLC边界后续分离 | “打开相机”会先尝试连接PLC，PLC失败仍继续开相机 | 影响设备操作时序 | Stage 1-3保持，Stage 2只用适配器复现现有顺序 | CAM-001/PLC-001 |
| DIFF-008 | 检测ROI外扩碰边时裁到原图边界 | 原模板匹配日期ROI外扩20像素后若整体落到图外会逐帧弹窗并跳过检测 | 按用户明确要求，20像素改为期望边距；边缘不足时使用0..width-1/height-1边界，只在裁剪后无有效面积时失败 | `DetectionRoiGeometry`统一模板匹配、字库和二维码日期ROI边界；2026-08-14离线边界测试及主程序靠边模板由用户确认通过 | DET-002、DET-003、DET-006、UI-002、RUN-002 |
| DIFF-009 | 五模式生产存图统一使用JPEG质量92 | 原钢印、字库、二维码和纸巾请求PNG，深度OCR请求未显式质量的JPG | 用户明确选择JPEG 90～95并采用中间值92，以降低PNG编码积压；不改变保存范围、原图/标注图组合、目录和失败报警 | `DetectionCompletionSaveOptions`显式传递quality，`ImageSaveService`调用三参数`QImage::save`；2026-08-15运行测试136项及主程序集中门禁通过 | SAVE-001..005 |
| DIFF-010 | Fault不发送猜测性兜底NG | 阶段4前`InspectionProductReconciler`曾在恰有一个未结论产品且PLC可写时允许请求一次49→约100ms→0 | 已实现的计划内行为变化：`InspectionRuntime`保留已有算法结论，未完成产品统一记`Unconfirmed`，不计入产品NG或合格率，不冒充机械剔除 | 2026-08-16用户批准；阶段4已删除收口器及兜底分支，测试源码覆盖PLC写失败只进入Fault且不产生猜测性49，等待本轮Qt Creator门禁执行 | `UI-002、UI-005、RUN-001..004、PLC-005..007、RES-001`；新终局方案阶段4 |
| DIFF-011 | “相机延时(ms)”与SDK `TriggerDelay`语义 | 历史UI `cameraDelay`作为软件线程节流，SDK `TriggerDelay`固定为0 | 2026-08-18用户明确批准：软触发不受该参数控制；参数改名“硬触发延时(ms)”并控制海康`TriggerDelay` | 已修改正式消费者：软触发删除最小间隔；硬触发启动时ms×1000写入MVS µs节点；Schema 1历史JSON键名暂不改，避免制造第二份设置格式 | 已解决；关联`CAM-003..004、RUN-005、PLC-004` |

## 基线资源

| 资源 | 路径/来源 | 覆盖功能 | 可重复条件 | 当前状态 |
|---|---|---|---|---|
| 五模式样本清单 | `tests/baseline/sample_manifest.tsv` | DET-002..008、RES、SAVE、PLC | 用户填写每模式OK/NG/FAILURE的固定图、模板、文本、Overlay、计数、存图、PLC和耗时 | 已建清单，15份实际证据待用户 |
| 纸巾离线基线测试 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | SET-010、DET-005 | Qt Creator打开`tests/tests.pro`，Run qmake、Build并运行测试 | 2026-08-12用户确认纸巾Pipeline切片的4项业务测试全部通过，汇总`6 passed, 0 failed` |
| 生产主程序构建 | `app/AutoOCRproject.pro` | 全部主程序功能 | Qt 5.14.2/MSVC2017 x64 Release，Run qmake、Rebuild、Run | 2026-08-12用户先后确认含纸巾Pipeline和深度OCR Pipeline的主程序正常构建运行；纸巾阈值与OCR模式切换正常 |
| 运行与性能记录 | `docs/development/OCRGangYin重构执行记录.md` | 内存、P50/P95、慢盘、停止/重启 | 用户按执行记录步骤填写真实数值 | 待用户验证 |
