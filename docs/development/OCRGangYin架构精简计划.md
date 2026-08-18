# OCRGangYin 架构精简计划

版本：1.8（S8 Runtime/Recipes认知精简已完成）
编制日期：2026-08-18
状态：S1、S2～S7及S8均已通过用户统一门禁；S8进入最终本地提交收口
适用范围：`app/` 当前新架构中的代码和代码结构

## 一、结论

当前架构不是需要推倒重来的失败架构。相机、采集线程、检测线程、有界队列、运行生命周期、结果唯一结算、配方事务和设备适配器等边界具有明确的工业运行价值，应继续保留。

当前确实存在中等程度的过度设计和迁移遗留。2026-08-18进一步按“新增第六种检测模式”的真实修改路径复核后，复杂度主要集中在以下五处：

1. 检测模式知识散落在Contracts、Settings、Recipes、Application、Runtime和UI，缺少唯一模式定义和装配边界。
2. `PipelineRegistry`实际是五模式中央工厂，`ResultService`又重复维护五套结果处理和收尾函数。
3. 同一模式数据经过`ProductRecipe → PreparedRecipe → InspectionProfileSnapshot → PipelineRegistry状态 → ResultService`多次重包装。
4. UI虽然按文件命名为页面，实际仍共享一份`Ui::MainWindow`，并存在模式下拉框静态项和整数索引判断。
5. `InspectionApplicationService`和`ResultService`承担的职责偏多，模式预检、运行编排和呈现接口面过宽。
6. 大量中文提示仍以`\uXXXX`转义写在源码中；运行显示通常正确，但代码审查和维护时无法直接阅读。

因此当前问题不是单纯“文件太多”，而是“层次很多，但检测模式这个最关键的变化单元没有被完整封装”。本计划把后续优先级调整为：先统一五模式结果链，再集中模式元数据和Pipeline装配，随后收口Recipes、Runtime、Application和UI中的模式分支。

本计划的目标不是追求最少类、最少文件或最少代码行，而是在不改变五种算法、正常PLC时序、统计口径、存图规则和用户操作流程的前提下，减少无效代码文件、转发层、跨层穿透和单类认知负担。

本次精简仅处理代码，不精简资源文件。图片、图标、QSS/CSS、翻译文件、Qt资源清单、模型和运行时资产全部保持不动，不把资源文件数量计入本计划的精简成果。

## 二、当前基线

截至本计划编制时，对`app/`的静态盘点结果如下：

| 项目 | 当前值 | 说明 |
|---|---:|---|
| `.h/.cpp`文件 | 164 | S1已删除2个影子比较器文件；vendor源码不能按普通业务代码看待 |
| UI代码 | 27个文件、9,898行 | 当前最大的自研认知负担 |
| Runtime代码 | 28个文件、5,900行 | 其中线程、队列、PLC和存图边界大多有保留价值 |
| Application代码 | 15个文件、2,994行 | `InspectionApplicationService.cpp`为1,323行 |
| Recipes代码 | 12个文件、2,696行 | `product_recipe.cpp`和`prepared_recipe.cpp`合计1,235行 |
| Detection代码 | 26个文件、4,115行 | 五种Pipeline边界清楚，但模式装配仍泄漏到Runtime |
| ResultService | 973行 | 同时涉及结算、统计、存图、PLC和呈现 |
| 直接写死模式枚举或ID的文件 | 25 | 更广义的模式分类、索引和策略分支还会扩大此范围 |
| Unicode转义中文 | 21个文件、206行、1,452个`\uXXXX` | 主工程已启用MSVC `/utf-8`，这些人类可读文案应直接写中文 |

当前还有8个功能处于既有修改后的用户门禁等待状态：

```text
CAM-003、CAM-004、RUN-005、SYS-006、TPL-004、DET-004、DET-006、TOOL-002
```

在这8项完成Qt Creator和真实相机相关门禁前，不开始新的生产代码精简，避免把已有行为修订、目录迁移和新精简差异叠在一起。

仓库`tests/`已按用户此前要求删除。本计划不得自行恢复测试目录；后续每个代码精简批次仍需静态门禁和用户Qt Creator门禁。

## 三、“减少代码文件”与“降低复杂度”必须分开

### 3.1 能直接减少代码文件的精简

这类精简只包括已经证明没有生产调用的`.h/.cpp`文件、无调用函数和迁移期代码桥。它们能够直接减少代码量，适合优先执行。

### 3.2 主要降低理解成本的精简

这类精简包括UI真实页面化、结果呈现链缩短和大服务职责收口。它们可能不减少文件，个别情况下还会增加少量组件文件，但会让开发者能够沿着明确边界理解和修改系统。

不能为了让文件数看起来更少，把`CaptureWorker`、`DetectionWorker`、`FrameQueue`、`InspectionRuntime`和五种Pipeline重新合并为一个大类。那会降低文件数，却明显增加运行风险和维护难度。

### 3.3 明确排除的资源范围

下列文件和目录不属于本次精简范围，不删除、不合并、不改名，也不从`.qrc`或工程部署清单中移除：

- `app/image/`下的图片、图标和其他视觉资源。
- `*.css`、`*.qss`及其配套主题图片。
- `*.ts`、`*.qm`翻译文件。
- `image.qrc`及其中的资源登记项。
- OCR模型、二维码DLL、相机/PLC运行库和部署资产。
- 配方运行所需模板、字符图片和校准文件。

`*.ui`表示Qt界面结构，S6可以为消除控件所有权穿透而调整其页面组织，但不得借此删除视觉资源、翻译或用户可见功能。

### 3.4 新增检测模式的当前变更扩散

当前增加一个非平凡检测模式，不是只在`detection/`增加一组Pipeline，而是大致沿以下路径扩散：

```text
DetectionMode枚举、JSON ID、UI ID
→ MachineSettings模式列表与main_window.ui静态选项
→ ProductRecipe JSON读写、校验和资源角色
→ PreparedRecipe资源加载和算法前置准备
→ InspectionApplicationService启动预检、定位类型和运行计划
→ PipelineRegistry模式专用Consumer、State、createXWorker和switch
→ ResultService模式专用handle/finalize
→ InspectionPresentationRenderer模式专用状态
→ TemplateEditorPage模式分支和下拉框索引
→ qmake工程清单与新Pipeline源码
```

按当前结构估算：简单整帧模式约需修改10～12个文件；带模板、Profile、特殊参数和展示信息的典型模式约需修改18～25个文件。这个修改面说明模式边界没有真正收口。

### 3.5 必要复杂度与可删除复杂度

必须保留的复杂度包括：相机会话、采集线程、检测线程、有界队列、唯一运行上下文、协作停止、PLC脉冲、存图反压和结果去重。这些职责不能为了减少文件数重新塞入`MainWindow`或一个巨型Runtime。

需要消除的是同一模式在不同层重复出现的分支和数据搬运：

- Runtime不应知道`TissueRollResult`、`StampDetectionWorkOutput`、`BarcodeWordDetectionWorkOutput`等具体模式输出。
- ResultService不应为五种模式分别维护`handleXCompletion()`和`finalizeX()`。
- Application不应重复判断单模板、多Profile、纸巾整帧和二维码特殊预检。
- UI不应通过`currentIndex() == 3`识别纸巾模式，也不应手写第二份模式名称顺序。
- Recipes应负责配方数据、Schema、事务和通用资源完整性，不执行具体算法，也不成为所有模式运行策略的中央分支库。

### 3.6 目标模式边界

不引入动态插件、反射、运行时脚本或复杂依赖注入。Qt 5.14/C++11下只建立两层静态边界：

```text
contracts/DetectionModeDescriptor
├─ 稳定枚举、JSON ID、UI ID、显示名
├─ 配方形态：整帧/单Profile/多Profile
├─ 资源能力：定位、字符、二维码、印章环
└─ 运行能力：定位策略、允许的触发策略

detection/DetectionModeRegistry
├─ Stamp模块工厂
├─ Word模块工厂
├─ Ocr模块工厂
├─ Tissue模块工厂
└─ BarcodeWord模块工厂
```

`DetectionModeDescriptor`是Settings、Recipes、Application和UI可读取的唯一静态模式清单；它不包含算法对象。`DetectionModeRegistry`位于Detection层，只负责把模式映射到对应Pipeline/Detector工厂。Runtime只提交通用请求并接收通用`DetectionCompletion`，不再包含五模式分支。

目标新增模式路径为：

```text
新增自己的detection/<mode>/模块
→ 在静态模式清单登记一次
→ 在Detection Registry登记一次工厂
→ 有独有配方字段时增加对应Recipe codec/validator
→ 有独有编辑交互时增加对应参数面板
→ 更新qmake清单
```

普通新模式目标控制在4～7个源码/工程文件变化；不承诺动态“放入目录即可加载”，因为当前qmake工具链仍需要显式登记源码。

### 3.7 旧职责到新职责的明确映射

| 当前位置或机制 | 目标位置或机制 | 处理方式 |
|---|---|---|
| `contracts/detection_mode.*`中的多组switch | `DetectionModeDescriptor`唯一静态表 | 保留稳定枚举和ID，集中能力元数据 |
| `runtime/pipeline_registry.*` | `detection/detection_mode_registry.*` | Registry返回具体Pipeline；Runtime只创建通用Worker |
| `PipelineResultConsumers`五种Consumer | 一个`DetectionWorker::CompletionConsumer` | 替换后删除五种回调类型 |
| Registry中的五个`createXWorker()` | 各模式目录自己的工厂 | Registry只做静态登记，不知道模式内部类型 |
| `ResultService`五套handle/finalize | 一个通用完成与唯一结算函数 | 替换后删除重复收尾函数 |
| Renderer中的模式专用旁路状态 | 通用Overlay图元和完整Presentation | 保持现有视觉语义，不保留模式旁路 |
| `runtime/inspection_profile_snapshot.*` | Detection拥有的不可变运行输入 | Runtime只持有通用Detector/请求 |
| `barcodeWordHardTriggerMode` | 通用采集策略或模式能力 | 删除具体模式名称 |
| Application的`modeKind()/trackingKind()` | Descriptor能力查询 | 删除第二套模式分类 |
| UI静态下拉项和整数索引 | Descriptor列表与稳定`itemData` | 删除模式顺序依赖 |
| ProductRecipe重复模式族判断 | Descriptor能力 + 明确Recipe codec | 保持Schema和事务边界 |

### 3.8 UTF-8源码中文可读性规则

截图中的：

```cpp
L"\u2195  \u62d6\u52a8\u8c03\u6574"
```

运行时等价于：

```cpp
L"↕  拖动调整"
```

这类写法不是乱码，也不是界面不能显示中文，而是源码把Unicode码点写成了转义序列。Git追踪表明截图对应代码在阶段6 UI替换提交中引入；这种写法可以规避旧工具链或补丁链的源文件编码风险，但当前主工程已经统一使用`/utf-8`，继续保留只会降低源码可读性。

本计划增加以下强制规则：

1. `app/`自研`.h/.cpp`中面向用户、日志和诊断人员的人类可读文本，必须直接写为UTF-8中文和可见符号，不再使用`\uXXXX`、`\UXXXXXXXX`或以十六进制逐字拼接中文。
2. `QStringLiteral("\u6b63\u786e")`改为`QStringLiteral("正确")`；`QString::fromWCharArray(L"\u4e2d\u6587")`若只是构造常量QString，改为`QStringLiteral("中文")`；只有确实调用宽字符API时才保留`L"中文"`。
3. `\n`、`\t`、`\\`、转义引号、正则表达式、二进制协议、JSON转义测试样本等具有语法或协议含义的转义继续保留，不能机械替换。
4. 稳定JSON键、错误码、模式ID、文件名、DLL名、`OCR/PLC/ROI/Profile`等技术标识保持原值；本项只改变源码表达，不擅自改写提示含义。
5. 替换前后逐条核对Unicode码点、占位符`%1/%2`、换行、空格、标点和字符串拼接顺序，确保运行时文本完全等价。
6. 图片、QSS、翻译、`.qrc`、模型、DLL和配方资源不属于本项修改范围。
7. 各阶段先清理本阶段触及文件中的人类可读Unicode转义；S6对整个`app/`自研源码执行最终扫尾并关闭零残留门禁。

2026-08-18静态基线：`app/`共有21个源码文件、206个代码行、1,452个`\uXXXX`码点转义；未发现`\UXXXXXXXX`或`\xNN`形式。当前命中均为可直接阅读的界面、日志、错误和检测诊断文本，预期最终为0。

## 四、不可破坏的行为边界

所有精简批次必须保持：

- 五种检测模式的算法、模型、阈值、识别文字和Overlay语义不变。
- 正常统计口径不变；同一`ProductKey`最多结算一次。
- 正常PLC合同不变：OK写0；NG写49，约100ms后写0。
- Fault不猜测补发NG；未完成产品继续记为`Unconfirmed`。
- 软触发不读取硬触发延时；硬触发延时继续按ms转µs写入相机。
- 预览不创建正式产品，不进入统计、存图和PLC链。
- 停止继续使用协作取消、`interruptWait()`和`join()`，不使用强杀或泄漏所有权。
- 配方和机器设置继续保持唯一Schema及事务保存。
- UI不获得设备、线程、算法、PLC时序或存图服务所有权。
- Runtime不依赖具体UI控件；Detection不访问UI、磁盘、PLC或相机SDK。
- 工具链、第三方库版本和部署方式不在本精简计划中升级或降级。
- UTF-8源码中文替换前后的运行时Unicode文本、占位符、换行、标点和提示语义完全一致。

## 五、精简清单

### 5.1 阶段数量与计算口径

结论：S2→S3→S4→S5→S6→S7已按技术依赖顺序完成，并根据2026-08-18用户的明确要求作为1个连续实施批次统一验证。用户已确认S2～S7验证成功，本批次进入封板提交。

完整口径如下：

| 类别 | 数量 | 当前状态 | 是否修改生产代码 |
|---|---:|---|---|
| 前置门禁G0 | 1 | 已通过统一门禁 | 否；已与S2～S7一起完成验证 |
| 已完成代码阶段S1 | 1 | 已完成并提交 | 是；已删除零调用代码 |
| 已完成代码阶段S2～S7 | 6 | 已完成并通过用户门禁 | 是；已按依赖顺序执行 |
| 全计划代码阶段S1～S7 | 7 | 全部完成 | 是 |
| S2～S7交付轮次 | 1 | G0+S2～S7合并交付已通过 | 已完成一次静态收口和一次用户统一验证，待创建本地提交 |

本次用户指令覆盖了原“逐阶段用户门禁”的执行方式。Agent仍必须按S2→S7的依赖顺序修改，但只在全部新路径接入、旧路径删除、qmake清单和静态门禁一并完成后交给用户统一验证。

### 5.2 阶段总览与依赖

| 阶段 | 状态 | 必须依赖 | 核心目标 | 主要代码范围 | 计划删除或替换 | 轮末用户验收重点 |
|---|---|---|---|---|---|---|
| G0 | 已完成 | S1已完成 | 收口当前8项既有变更 | 相机节拍、`engines/ocr`、`engines/barcode`及对应调用链 | 不新增架构；失败时只修复对应既有变更 | 用户统一门禁已通过；真实PLC、机械和现场项继续单列待验 |
| S1 | 已完成 | 用户已批准零调用清理 | 删除完全无生产调用的代码 | Runtime、Application、TemplateEditor、qmake | 影子比较器、零调用查询、只写状态 | 已完成Run qmake、Rebuild和主程序回归，提交`769ece0` |
| S2 | 已完成 | G0统一门禁已通过 | 统一五模式结果和唯一收尾链 | Detection结果合同、Runtime Registry/ResultService、Presentation、Application/UI结果发布 | 五种Consumer、五套handle/finalize、模式专用呈现旁路、多层字段回调 | 用户统一门禁已通过 |
| S3 | 已完成 | S2代码完成 | 建立唯一模式清单和Detection装配边界 | `contracts`、`detection`、Runtime创建Pipeline处、Settings模式列表 | Runtime中央五模式工厂、重复ID转换、模式列表副本 | 用户统一门禁已通过 |
| S4 | 已完成 | S3代码完成 | 收口Recipes与Runtime数据边界 | ProductRecipe、PreparedRecipe、RecipeStore、Detection运行输入、RunConfiguration | 重复模式族判断、Runtime模式Profile/二维码策略、`barcodeWordHardTriggerMode` | 用户统一门禁已通过 |
| S5 | 已完成 | S4代码完成 | 收缩Application启动预检和公开接口 | InspectionApplicationService、InspectionStartPreflight、Pipeline readiness | `modeKind()/trackingKind()`、第二套模式分类、二维码专用准备入口、零价值转发 | 用户统一门禁已通过 |
| S6 | 已完成 | S5代码完成 | 消除UI模式索引、完成页面所有权拆分并关闭UTF-8源码文字门禁 | MainWindow、三个Page、TemplateEditor拆分实现、`main_window.ui`；最终扫尾涉及的自研源码 | 固定模式索引、静态模式顺序依赖、整份`Ui::MainWindow *`穿透、人类可读`\uXXXX`文案 | 用户统一门禁已通过 |
| S7 | 已完成 | S6代码完成 | 把预处理和定位归入DetectionWorker | CameraSession、CaptureWorker、FrameQueue、DetectionWorker、FramePreprocessor、Positioner | 采集线程中的旋转/通道/定位/Profile选择 | 用户统一门禁已通过；真实PLC、机械和现场项继续单列待验 |

### 5.3 每阶段固定交付物

本次合并批次内部的每个S2～S7阶段仍必须满足以下交付物；全部满足后才进入唯一的末尾用户门禁：

1. 开始前列出真实受影响功能ID，并从UI/设备入口追踪到算法、统计、存图、PLC和停止生命周期。
2. 新职责接入正式唯一路径；被替代的旧类型、函数、字段、include和qmake项在同阶段删除，不保留运行时开关或长期双路径。
3. 保持五种算法、Schema、资源文件、正常统计和PLC合同不变；任何计划外行为变化必须停下单独确认。
4. 本阶段触及的自研源码同步把人类可读`\uXXXX`改为直接UTF-8中文，逐条保持运行时文本等价；S6负责最终全仓扫尾。
5. 完成include、声明/定义、Qt信号槽、对象所有权、工程清单、零引用和`git diff --check`静态门禁。
6. 仓库`tests/`继续保持不存在，不自行重建；用户只在S2～S7全部完成后执行一次Run qmake、Rebuild和完整主程序回归。
7. 统一门禁通过后更新功能表和执行记录，只暂存本次合并批次文件并创建一个本地提交；不推送、合并或变基。
8. 真实PLC、机械剔除和现场恢复始终单列待验，不使用Fake或无PLC结果冒充完成。

### G0：先完成当前8项门禁

这不是新的精简批次，而是开始精简前的前置条件。

用户需在Qt Creator完成当前记录要求的Run qmake、Rebuild和主程序回归，并覆盖：

- 软触发连续节拍不受硬触发延时影响。
- 硬触发延时正确写入真实海康相机。
- OCR引擎初始化和代表帧OCR。
- 模板制作中的即时二维码读取。
- 二维码+三期正式检测模式。

原计划要求通过后先把上述8项恢复为`已验证`并收口当前差异，再开始S1。2026-08-18用户明确要求先执行“删除完全不影响功能的代码或文件”，因此S1作为独立的零调用清理批次先行实施；上述8项仍保持`迁移中`，本次清理不改变其状态，也不把它们视为已经通过门禁。

### S1：删除零调用代码

优先级：最高
目标类型：直接减少代码文件和无效符号
风险：低

#### S1.1 删除影子比较器

已删除：

```text
app/runtime/detection_shadow_comparator.h
app/runtime/detection_shadow_comparator.cpp
```

同步删除`app/AutoOCRproject.pro`中的两个工程项。

当前静态证据表明，该类只在自身定义和qmake清单中出现，没有被`startup`、`CameraSession`、`DetectionWorker`、五种Pipeline或`ResultService`调用。它只负责比较两个外部提供的`DetectionResult`，本身不运行影子算法，也没有接入正式生产链。

实际影响：减少2个C++文件，不改变检测、统计、存图、UI或PLC行为。

#### S1.2 审计其他零调用代码

除影子比较器外，实施S1时对自研`.h/.cpp`执行一次完整调用审计，检查：

- 只有声明和定义、没有生产调用的函数。
- 只做原样转发、没有策略或边界价值的方法。
- 已经没有入口的迁移桥、回调结构和状态字段。
- qmake登记但没有任何正式入口的整对源码文件。

删除条件必须同时满足：

1. 全仓源码、Qt自动槽、动态信号连接和启动组装均无入口。
2. 不承担SDK回调、Qt元对象、序列化或插件式动态调用。
3. 不改变用户可见功能、算法、统计、存图、PLC和生命周期。
4. 同步删除对应声明、定义、include和qmake源码项。

S1不检查或删除任何资源文件，不修改`image.qrc`、翻译清单和部署资产。除已确认的2个影子比较器文件外，其他代码删除项必须在实施前形成精确清单，不能以目录或命名推断其无用。

#### S1.3 实际零调用审计结果

2026-08-18完成全仓自研代码符号审计，并排除Qt自动连接槽、虚函数回调、SDK回调、许可证工具入口和vendor源码。除影子比较器外，确认并删除以下零调用代码：

| 删除项 | 静态证据 | 保留的正式能力 |
|---|---|---|
| `InspectionApplicationService::queryCameraExposureRange()` | 仅有声明和定义，无UI、启动组装或生产调用 | `CameraSession::queryExposureRange()`仍由实际曝光设置流程使用 |
| `InspectionRuntime::unresolvedFaultProductCount()` | 仅有声明和定义 | 故障产品清理及`ResultService::recordUnconfirmedProducts()`保持不变 |
| `InspectionRuntime::faultUnconfirmedProductCount()`及只写累计字段 | 累计值除该零调用getter外无人读取 | 每次故障未确认数量仍按原逻辑写入结果服务 |
| `InspectionRuntime::runContext()` | 仅有声明和定义 | Runtime内部不可变运行上下文及全部实际使用点保持不变 |
| `TemplateEditorPage::validatedBarcodeText()`及只写文本字段 | 文本只被保存、清空和通过该零调用getter读取 | 二维码仍实际解码；可读状态、失败原因和已验证ROI保持不变 |
| `TemplateBarcodeValidationResult`返回通道 | 结果DTO只把二维码文本返回给上述只写字段，无其他消费者 | 校验成功/失败、失败提示、解码参数和模板保存阻断保持不变 |

同时删除已无消费者的二维码角点坐标回填计算、对应空壳参数和局部变量。S1最终代码差异为删除2个代码文件、清理6个零调用查询/返回接口及2组只写状态；`app/`中的`.h/.cpp`由166个降为164个。没有删除检测模式、Pipeline、Recipe、设备/引擎适配器、资源或第三方源码。

2026-08-18用户按本轮交付清单完成Run qmake、Rebuild和主程序回归后反馈“没问题”，S1门禁通过。S1不对应独立用户功能ID，因此不修改功能表状态；此前采集节拍和engines目录迁移涉及的8项功能继续保持`迁移中`，等待各自专项门禁。

### S2：统一五模式检测结果与唯一收尾链

优先级：最高（S1之后的第一个代码批次）
目标类型：删除Runtime中的模式专用回调和重复收尾代码
风险：中高

当前`DetectionWorker`已经使用通用`DetectionResult`和`DetectionCompletion`，但`PipelineRegistry`又额外定义五种模式专用Consumer，`ResultService`随后再维护五套`handleXCompletion()`和`finalizeX()`。这使已经统一的结果合同被再次拆散。

目标链路：

```text
DetectionPipeline
→ DetectionResult（包含完整判定和类型化通用展示数据）
→ DetectionCompletion
→ ResultService::handleCompletion()
→ 唯一去重、统计、存图和PLC决策
→ InspectionPresentation
→ UI一次应用
```

实施内容：

1. 扩充通用`DetectionResult`，补齐命中Profile名、Pose/坐标变换和统一Overlay图元；多边形、椭圆、文字等使用带明确枚举的C++11结构，不使用`QVariantMap`或字符串键值袋。
2. 五种Pipeline在Detection层内把各自内部输出归一为最终`DetectionResult`；模式内部临时类型不再越过Detection边界。
3. `PipelineResultConsumers`由五种Consumer缩成一个`DetectionWorker::CompletionConsumer`。
4. 删除`ResultService`中的五套`handleXCompletion()`、`finalizeX()`和重复呈现构造，保留一个产品唯一结算入口。
5. `InspectionPresentationRenderer`只消费通用Overlay和完整`InspectionPresentation`，不保存`stampIsOverlap`、`TissueRollPresentation`等模式专用旁路状态。
6. 删除多层`ViewBindings`拆字段回调，Application只发布一份完整呈现快照，InspectionPage只提供一个`applyPresentation(...)`入口。

保持不变：五种算法、识别文字、Overlay视觉语义、同一`ProductKey`最多结算一次、统计口径、存图策略和正常PLC合同。

静态完成标准：Runtime中不存在五种Consumer、五套结果handle/finalize和模式专用Detection输出类型；正式结果链仍只有一个统计、存图和PLC出口。

### S3：建立唯一模式清单和Detection装配边界

优先级：高
目标类型：把“新增检测模式”的修改面收敛到模式自身
风险：中

建立两个简单的静态组件，不引入动态插件框架：

- `contracts/DetectionModeDescriptor`：集中稳定枚举、JSON/UI ID、中文显示名、配方形态、资源能力、定位策略和触发能力。
- `detection/DetectionModeRegistry`：集中模式到Detector/Pipeline工厂的静态映射。

实施内容：

1. 把现有`detectionModeJsonId()`、`detectionModeUiId()`、反向转换、`isWordFamilyMode()`及各处重复模式分类收敛到唯一Descriptor表。
2. 将`PipelineRegistry`从Runtime职责移至Detection装配边界；Registry只返回`IDetectionPipeline`，Runtime负责创建唯一通用`DetectionWorker`并安装通用完成回调，Detection不得反向依赖Runtime。
3. 删除中央类中的五个`createXWorker()`。每个模式目录提供自己的Pipeline静态工厂，Registry只保存模式与工厂函数的对应关系。
4. Settings和UI从Descriptor列表生成模式选项，使用`itemData`保存稳定ID，不依赖显示顺序或整数索引。
5. 模式能力只描述稳定事实，不包含算法阈值、设备对象、UI控件或资源文件内容。

新增模式的目标修改面：自身模式模块2～4个文件、Descriptor登记1处、Registry登记1处、qmake清单1处；只有出现独有配方字段或独有编辑交互时，才额外修改对应Recipe codec或UI参数面板。

### S4：收口Recipes与Runtime的数据边界

优先级：高
目标类型：减少配方数据重复包装和Runtime模式泄漏
风险：中高

`recipes`继续存在，并继续拥有Schema、配方事务、资源路径和通用资源完整性；它不是算法能力库。公共算法能力保留在`detection/common`或`engines`，具体模式如何使用这些能力由Detection模块决定。

实施内容：

1. `ProductRecipe`保留产品UUID、名称、模式、Profile、类型化参数和资源相对路径；`RecipeStore`继续负责JSON读写、事务保存和损坏拒绝。
2. 用Descriptor中的配方形态和资源能力替换`product_recipe.cpp`中重复的模式族判断；只有真正独有的字段编解码保留在明确命名的Recipe codec/validator中。
3. `PreparedRecipe`只准备通用只读资源和Recipe快照；字符匹配器索引、二维码策略状态等算法准备移入对应Detection模块。
4. `InspectionProfileSnapshot`移出Runtime，改为Detection拥有的不可变运行输入，或在不复制图像资产的前提下并入模式Detector对象。
5. `InspectionRunConfiguration::barcodeWordHardTriggerMode`改为通用`InspectionAcquisitionPolicy`或能力查询，Runtime不再出现具体模式名称。
6. 不改变现有Schema字段、资源相对路径、事务保存方式和已发布配方内容；本批次只移动内部职责。

静态完成标准：Recipes不依赖Detection实现；Detection可读取Recipes合同；Runtime不包含字符模板、二维码解码策略和五模式专用Profile类型。

### S5：收缩Application启动预检和公开接口

优先级：中高
目标类型：降低`InspectionApplicationService`认知负担
风险：中

`InspectionApplicationService.cpp`当前1,323行。它是UI到运行核心的合法门面，但不应自行维护另一套模式分类和资源规则。

实施内容：

1. 删除本地`modeKind()`、`trackingKind()`及Stamp/Ocr/Word/BarcodeWord/Tissue分支，统一读取`DetectionModeDescriptor`。
2. Detection Registry返回通用`PipelineReadiness`，Application不再只为二维码模式调用特殊准备接口。
3. 启动预检读取`PreparedRecipeReadiness + Descriptor能力`，不再维护`InspectionStartModeKind`的第二套模式分类。
4. 保留一个应用层门面，但公开方法只表达打开相机、开始、停止、预览、PLC命令和状态查询等用户用例。
5. 删除没有UI入口或只有一层原样转发的方法；纯数据转换放回对应合同或已有服务。
6. Settings继续由`SettingsApplicationService`负责，模板编辑继续由`TemplateApplicationService`负责，不新增一组空壳Controller。

完成标准：Application中没有五模式`switch`；新增模式若不增加新的用户交互，不需要修改`InspectionApplicationService`。

### S6：消除UI模式索引并完成页面所有权拆分

优先级：中
目标类型：降低模式UI分支和控件所有权穿透
风险：高

当前`main_window.ui`静态写入五种模式，设置页用`currentIndex() == 3`识别纸巾；模板编辑器也在多个实现文件中重复判断模式族。

实施内容：

1. 检测模式下拉框由Descriptor列表运行时填充，显示文本与稳定ID分离；业务代码只读取稳定ID或`DetectionMode`。
2. 模板编辑器基于“整帧、单Profile、多Profile、需要二维码、需要字符”等能力决定流程，不重复列举五个模式。
3. 确有独有交互的模式使用小型参数面板，不把所有模式控件和状态继续堆入同一个页面类。
4. `InspectionPage`、`MachineSettingsPage`和`TemplateEditorPage`分别拥有自己的根Widget和控件，不再保存整份`Ui::MainWindow *`。
5. `MainWindow`只组合页面、连接顶层命令和展示顶层错误；页面之间通过类型化数据和信号协作，不交换控件指针。
6. 模板编辑器现有跨多个`.cpp`的实现按画布/ROI、Profile编辑、配方发布三个真实职责收口，不重新合成巨型文件。
7. 对前序阶段未触及的自研源码执行UTF-8文字最终扫尾，把全部人类可读`\uXXXX`替换为直接中文或可见符号；常量QString优先使用`QStringLiteral("中文")`，仅在宽字符API边界保留`L"中文"`。
8. 逐条核对窗口标题、按钮、工具提示、确认框、故障提示、检测诊断、启动预检、存图错误和引擎加载日志，禁止在清理转义时顺手修改文字内容。

本批次允许新增少量页面或参数面板文件；评价指标是新增模式是否只触及自己的交互，而不是总文件数是否下降。

UTF-8源码文字完成标准：`app/`自研`.h/.cpp`中的人类可读`\uXXXX`、`\UXXXXXXXX`和`\xNN`为0；若未来存在协议或测试数据必须保留，必须逐条列入白名单并说明机器语义，不能以“可能有用”为由整类放行。

### S7：把定位和预处理归回DetectionWorker

优先级：最后
目标类型：修正线程职责，使架构与批准方案一致
风险：高

批准的正式链路要求：

```text
CaptureWorker
→ FrameQueue
→ FramePreprocessor
→ Pose/Profile定位
→ DetectionPipeline
```

当前实际代码仍由`CameraSession`在采集提交路径中执行预处理和定位，然后再提交检测。这会让采集线程承担算法工作，使相机采集能力、定位耗时和队列反压语义耦合。

目标是让`CameraSession/CaptureWorker`只负责获得独立帧和提交队列，把旋转、颜色通道、定位及Profile选择放到同一个串行`DetectionWorker`中。

该调整可能改变吞吐、队列满时机、停止时序和硬触发Fault条件，因此内部仍作为S2～S7的最后一步实施；按用户2026-08-18的合并指令，不单独停下构建，而是在末尾统一执行真实相机门禁。

### 5.4 S2～S7合并实施结果（2026-08-18）

1. `DetectionResult`已成为五模式唯一结果合同，包含判定、文字、模板名、Overlay、耗时和呈现/存图策略；`ResultService`只保留一个Completion入口。
2. `PipelineRegistry`已从Runtime删除，替换为`detection/detection_registry.*`；五模式ID、显示名、定位类型、资源要求和结果策略由`DetectionModeDescriptor`唯一提供。
3. `InspectionProfileSnapshot`已从Runtime移入Detection并改为`DetectionProfileSnapshot`；Application不再拆解字符模板和二维码策略。
4. `InspectionApplicationService`中的`modeKind()/trackingKind()`和第二套启动模式分类已删除；启动预检直接查询Descriptor。
5. 检测模式下拉框改为Descriptor动态生成，模式整数索引判断已清理；`InspectionPage`和`MachineSettingsPage`只接收显式控件绑定，不再持有整份`Ui::MainWindow *`。
6. `app/`自研`.h/.cpp`中的人类可读Unicode转义已改为直接UTF-8中文；零残留搜索无白名单。
7. 正式采集现在只把相机原帧提交给`FrameQueue`；旋转、颜色通道、定位和Profile选择全部在`DetectionWorker`的Detection执行器中完成。模板实时预览仍在预览分支执行显示预处理，不进入正式产品结果链。
8. 旧Registry/Profile四个Runtime文件已删除并在Detection中替换；代码文件总数仍为164，没有删除或精简任何资源文件。
9. 合并批次最终静态门禁通过：主工程168个源码、头、UI、qrc和翻译登记项无缺失、无重复；5个模式Descriptor唯一；旧模式装配/结果旁路、Runtime具体模式和算法依赖、CameraSession定位及UI整窗指针均为0；UTF-8转义、资源差异和用户生成文件差异均为0；`git diff --check`通过。
10. 2026-08-18用户确认S2～S7统一验证成功，87项保留功能恢复为`已验证`；真实PLC、机械剔除和现场恢复仍保持单列待验，不能由本轮验证结果替代。

### 5.5 S8 Runtime/Recipes认知精简（2026-08-18）

S8只删除“包装一层但没有独立策略或生命周期”的代码，不合并采集、检测、队列、Runtime状态机、结果、存图、算法和设备适配器。目标是让维护者按一条主路径阅读，而不是继续增加抽象层。

| 精简点 | 当前问题 | 本轮处理 | 保持不变 |
|---|---|---|---|
| Runtime触发配置 | `InspectionRunConfiguration → InspectionRunPlan → InspectionAcquisitionKind`只把一个`bool`转换成枚举 | `CameraSessionCaptureConfiguration`直接保存`hardwareTriggerEnabled`，删除配置`.h/.cpp` | 软/硬触发选择、硬触发延时、曝光、增益、统计清零和相机时序 |
| Runtime运行上下文 | `InspectionRunContext`只有`InspectionRuntime`实现使用，却单独暴露头文件 | 在`inspection_runtime.cpp`中定义私有实现，头文件只前置声明，删除独立头文件 | 每次运行冻结的runId、开始时间、设置、配方、Profile和预处理快照 |
| Recipes模式记忆 | `TemplateModeMemory`只包装一个`QMap`和Descriptor索引查询 | `TemplateApplicationService`直接持有并只读公开该Map；UI直接查询唯一模式Descriptor，删除包装`.h/.cpp` | 各模式已发布配方记忆、保存和恢复语义 |
| Recipes资产暂存 | `RecipeAssetService`只被`RecipeEditorSession`所在用例使用，形成并列对象 | 把资源暂存函数并入`RecipeEditorSession`，删除服务`.h/.cpp` | 工作区路径、文件格式、资产键、相对目录、事务发布和失败行为 |

实际代码文件数已从164降到157：`runtime`从24降到21，`recipes`从12降到8。S8没有修改任何图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL、配方Schema、资源目录或资源内容。2026-08-18用户确认统一门禁通过，受影响32项功能恢复为`已验证`。

### 5.6 S9 UI状态与命令门禁统一（2026-08-18）

S9解决的是同一操作在MainWindow、Page、Application和Runtime之间重复判断、互相覆盖的问题，不以增加抽象层或让所有动作机械地复制三次`if`为目标。

| 操作类型 | 保护层级 | 统一规则 |
|---|---|---|
| 状态文字、提示、纯显示交互 | UI一层 | 只影响显示，不改变业务、设备或持久化状态。 |
| 设置草稿、配方编辑和发布 | UI + Application/Store | UI根据统一快照禁用；Application/Store继续执行字段校验、Schema约束和事务保存。 |
| 检测启停、Fault恢复、相机、模板取景、PLC、统计和剔除队列 | UI + Application + Runtime/Session/Device | UI负责可见可用性，Application负责命令当前是否允许，底层负责线程、连接、事务和SDK不变量；三层职责不同。 |

具体修改边界：

1. `OperationUiPolicy`接收一次`OperationUiContext`，唯一输出开关相机、启停、模板、普通设置、相机设置、PLC连接/运行参数、配方、统计和剔除队列的`Access{enabled, disabledReason}`。
2. `MainWindow::updateOperationUiState()`只读取一次`RuntimeSnapshot`并分发同一权限快照；`InspectionPage`只更新明确绑定的主操作按钮，不再`findChildren<QAbstractButton *>()`扫描并覆盖整窗控件。
3. `MachineSettingsPage`按None/Camera/PlcConnection/PlcRuntime四种依赖应用权限；`TemplateEditorPage`按配方选择和配方编辑权限应用，不再保留“全部编辑器启用”入口。
4. UI槽删除忙碌、相机已开/未开、PLC已连/未连和采集线程状态的重复业务判断，只保留输入格式、确认对话框和页面内部CaptureState转换。
5. `InspectionApplicationService`直接以`InspectionRuntime`和`CameraSession`真实状态裁决公开命令；删除`m_cameraOpen`状态副本。公开PLC命令要求Idle，正式启动内部下发则使用私有设备助手，避免公开门禁阻断启动事务。
6. 模板取景、统计清零和剔除队列清理改为结构化`OperationResult`；拒绝时UI显示Application给出的稳定中文原因，不再把失败误报为成功。

S9不增删代码文件，不修改资源、算法、配方Schema、统计口径、PLC地址/值或约100ms脉冲时序。2026-08-18用户确认Run qmake、Rebuild和统一人工回归通过，受影响59项已由`迁移中`恢复为`已验证`。

## 六、暂时保留，不应为了少文件而合并的模块

以下组件具备明确职责或安全价值，应继续保留：

| 组件 | 保留原因 |
|---|---|
| `CameraSession` | 相机启动、恢复、预览和正式采集事务边界 |
| `CaptureWorker` | 唯一采集线程及软硬触发语义 |
| `DetectionWorker` | 串行检测和协作停止边界 |
| `FrameQueue` | 正式帧容量与反压语义 |
| `InspectionRuntime` | Idle/Starting/Running/Stopping/Fault唯一状态 |
| `ImageSaveService` | 容量32、两个写入线程和写盘反压 |
| `RecipeStore/RecipeEditorSession` | 配方事务保存和编辑工作区隔离 |
| 相机、PLC、OCR、二维码适配器 | 隔离供应商SDK和DLL |
| 五种Detection Pipeline | 保持模式算法边界，避免主流程条件分支膨胀 |

### 关于`contracts/`目录的修正结论

初步口头分析曾把只有3个文件的`contracts/`列为可合并对象。进一步检查后，`DetectionMode`实际被UI、Application、Settings、Recipes和Runtime共同使用。若仅为了减少一个目录把它移动到某一业务层，会迫使其他层反向依赖该层。

因此本计划保留这个小型共享合同目录，并在S3把稳定的`DetectionModeDescriptor`放入该边界。后续只允许在以下条件下调整：

- 能为类型找到不制造反向依赖的唯一所有者。
- 不复制默认值或字符串转换。
- 不让Recipes依赖Detection实现，也不让Runtime依赖Application。

`barcode_parameter_defaults.h`可以在二维码参数模型重新明确所有权时一并收口，但不作为优先精简项。

## 七、推荐实施顺序

```text
S1 零调用代码清理（已完成，属于用户批准的低风险先行批次）
→ 当前8项既有门禁收口
→ S2 五模式通用结果与唯一收尾链
→ S3 唯一模式清单与Detection装配边界
→ S4 Recipes/Runtime数据边界收口
→ S5 Application启动预检和接口收缩
→ S6 UI模式索引清理与页面所有权拆分
→ S7 定位/预处理迁入DetectionWorker
→ S8 Runtime/Recipes认知精简
→ S9 UI状态与命令门禁统一（当前等待统一门禁）
```

本轮采用“内部分阶段、对外单批次”：结果合同、模式注册、配方边界、Application、UI和线程职责仍按顺序收口；用户只在末尾执行一次Run qmake、Rebuild和完整回归。

## 八、每批次统一门禁

### 8.1 开始前

- 工作区干净或明确列出用户已有修改。
- 列出本批次全部受影响功能ID，不能只写最低范围。
- 从真实UI、相机或回调入口追踪到算法、统计、存图、PLC和停止生命周期。
- 明确本批次是“减少代码文件”还是“降低理解成本”。

### 8.2 Agent静态门禁

- 删除文件的全仓引用为0。
- qmake清单不存在已删文件、重复项和大小写错误。
- 图片、QSS、翻译、`.qrc`和部署资源不出现在本批次差异中。
- UI、Runtime、Detection、Recipes依赖边界不回退。
- 不出现第二套正式结果、统计、PLC或存图入口。
- S2完成后，`PipelineResultConsumers`五种Consumer和`ResultService`五套模式收尾函数为0引用。
- S3完成后，Runtime中具体五模式枚举分支为0；所有模式列表只来自Descriptor。
- S4完成后，Runtime对字符模板、二维码策略和模式专用Profile类型为0依赖。
- S5完成后，Application中模式`switch`和本地模式族分类为0。
- S6完成后，不存在通过固定整数索引识别检测模式的业务代码；以下搜索在`app/`自研源码中为0：

```text
rg -n "\\u[0-9A-Fa-f]{4}|\\U[0-9A-Fa-f]{8}|\\x[0-9A-Fa-f]{2,8}" app -g "*.h" -g "*.cpp"
```

- 若上述搜索未来命中协议、正则或机器测试数据，必须以精确文件和行为理由建立最小白名单；当前基线没有这类例外。
- S9完成后，整窗`findChildren<QAbstractButton *>`操作禁用、`m_cameraOpen`状态副本、页面级全部编辑器开关和UI槽内重复Runtime/相机/PLC业务判断为0；所有设备类公开命令保留Application结构化拒绝路径。
- `git diff --check`通过。
- 差异中不包含`*.pro.user`、`*.ui.autosave`、`app.zip`或其他用户文件。
- Agent不运行qmake、构建、测试程序或主程序。

### 8.3 本轮唯一的用户Qt Creator门禁

每个已批准的合并批次全部代码完成后，由用户只执行一次：

1. Run qmake。
2. Rebuild主工程。
3. 回归该批次所有受影响功能。
4. 涉及相机线程、触发、定位或PLC时，执行对应真实设备门禁。
5. S6额外核对主窗口、模式切换、模板对话框、启动预检、故障提示、检测诊断、存图错误及OCR/二维码加载日志中的中文、符号、换行和占位值均与替换前一致。

### 8.4 收口

- 用户门禁通过后，受影响保留功能恢复为`已验证`。
- 更新功能对照表和重构执行记录。
- 精确暂存本批次文件，复核暂存清单后创建唯一的本地提交。
- 不自动推送、合并或变基。

## 九、预期结果

S1已减少2个影子比较器代码文件，并按完整调用审计清理了上述零调用符号和只写状态；`.h/.cpp`由166个降为164个。资源文件数量保持不变，不作为精简指标。

完成S2到S7后，更重要的变化不是总文件数，而是主要调用链和模式扩展路径都可以清晰表达为：

```text
UI用户操作
→ Application用户用例
→ Runtime生命周期
→ Capture/DetectionModeRegistry
→ 模式自身Pipeline
→ ResultService唯一结算
→ 一份InspectionPresentation
→ UI一次应用

新增模式
→ 增加模式自身模块
→ Descriptor登记一次
→ Detection Registry登记一次
→ 可选Recipe codec/UI参数面板
→ qmake显式登记
```

最终期望达到：

- 新开发者只需沿正式代码入口阅读，不需要把资源文件和vendor源码当成业务架构逐个分析。
- 通过目录即可找到设备、算法、配方、运行时和UI的唯一入口。
- 修改一个页面不需要持有整份主窗口UI。
- 修改结果呈现不触碰统计、PLC和存图唯一性。
- 修改检测算法不触碰相机SDK、UI和磁盘。
- 新增普通检测模式控制在约4～7个源码/工程文件变化，不再修改Runtime和ResultService。
- `recipes`只承担配方数据、Schema、事务和资源完整性，`runtime`只承担生命周期与通用编排。
- 自研源码中的中文提示、日志和诊断可以直接阅读，不再出现需要人工解码的`\uXXXX`；运行时显示内容与替换前逐码点一致。
- 重要的工业安全边界保留，迁移期遗留和无效代码被清除。

## 十、本计划不包含的工作

- 不调整算法准确率、模型、阈值或ROI业务定义。
- 不改变正常PLC值、地址或约100ms脉冲合同。
- 不升级Qt、OpenCV、PaddleOCR、Barcode DLL或Snap7。
- 不恢复已删除的多相机功能。
- 不恢复旧设置、旧模板或旧目录兼容。
- 不自行重建已删除的自动测试目录。
- 不删除、合并、改名或精简图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL和其他资源文件。
- 不借UTF-8源码文字清理修改中文措辞、翻译策略、错误码、JSON字段、算法诊断含义或用户操作流程。
- 不把已发现但尚未批准的正确性问题伪装成结构精简一起修改。
- 不在结构精简中顺手改变多件NG在约100ms窗口内的PLC脉冲合并行为；如需调整，另列正确性批次和真实现场门禁。
- 不在结构精简中顺手改变`Cancelled`结果的统计、存图或PLC语义；如确认存在问题，另列功能影响和行为决策。
