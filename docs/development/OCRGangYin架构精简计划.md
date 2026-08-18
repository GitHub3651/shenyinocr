# OCRGangYin 架构精简计划

版本：1.2（S1执行稿）
编制日期：2026-08-18
状态：S1零调用代码已完成并通过用户Qt Creator门禁
适用范围：`app/` 当前新架构中的代码和代码结构

## 一、结论

当前架构不是需要推倒重来的失败架构。相机、采集线程、检测线程、有界队列、运行生命周期、结果唯一结算、配方事务和设备适配器等边界具有明确的工业运行价值，应继续保留。

当前确实存在中等程度的过度设计和迁移遗留，但复杂度主要集中在以下四处：

1. 仍存在无生产调用的代码和迁移期遗留转发层。
2. UI虽然按文件命名为页面，实际仍共享一份`Ui::MainWindow`和大量控件裸指针。
3. 检测结果到UI之间存在多层`ViewBindings`回调转接。
4. `InspectionApplicationService`和`ResultService`承担的职责偏多，接口面过宽。

本计划的目标不是追求最少类、最少文件或最少代码行，而是在不改变五种算法、正常PLC时序、统计口径、存图规则和用户操作流程的前提下，减少无效代码文件、转发层、跨层穿透和单类认知负担。

本次精简仅处理代码，不精简资源文件。图片、图标、QSS/CSS、翻译文件、Qt资源清单、模型和运行时资产全部保持不动，不把资源文件数量计入本计划的精简成果。

## 二、当前基线

截至本计划编制时，对`app/`的静态盘点结果如下：

| 项目 | 当前值 | 说明 |
|---|---:|---|
| `.h/.cpp`文件 | 166 | 其中Paddle、Snap7等vendor源码不能按普通业务代码看待 |
| `.h/.cpp`总行数 | 38,652 | 物理行统计，仅用于判断规模 |
| UI代码 | 27个文件、9,919行 | 当前最大的自研认知负担 |
| Runtime代码 | 30个文件、6,119行 | 包含必要的生命周期和并发边界 |
| Application代码 | 15个文件、3,026行 | `InspectionApplicationService`单个实现文件1,330行 |
| ResultService | 973行 | 同时涉及结算、统计、存图、PLC和呈现 |

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

`*.ui`表示Qt界面结构，S5可以为消除控件所有权穿透而调整其页面组织，但不得借此删除视觉资源、翻译或用户可见功能。

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

## 五、精简清单

### S0：先完成当前8项门禁

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

### S2：缩短结果到UI的转接链

优先级：高
目标类型：主要降低理解成本，同时可减少DTO和转发代码
风险：中

当前结果呈现大致经过：

```text
InspectionPage::resultViewBindings()
→ InspectionViewBindingsDto
→ InspectionApplicationService::bindView()
→ InspectionPresentationViewBindings
→ ResultService::bindView()
→ InspectionPresentationRenderer::bindView()
→ 多个控件回调
```

这个链路把一份完整检测结果重新拆成图像、判定样式、文字、模板名、计数、合格率和耗时等多个函数对象，增加了阅读和生命周期管理成本。

目标链路调整为：

```text
ResultService形成一份InspectionPresentation
→ Application发布一个类型化结果事件
→ InspectionPage在UI线程一次应用整份快照
```

精简要点：

- 保留`InspectionPresentation`作为同一产品的完整快照。
- 删除两套重复的ViewBindings结构和多层`bindView()`转发。
- UI页面只提供一个`applyPresentation(...)`入口。
- 预览帧、清空瞬态结果和统计重置也使用少量明确事件，不重新扩展成十几个字段回调。
- Runtime继续只生成数据，不持有`Ui::MainWindow`或具体控件。

验收重点：同一产品的图片、Overlay、识别文字、模板名、OK/NG、统计和耗时必须一次同步替换，不能出现跨产品混合显示。

### S3：收缩应用服务和纯转发接口

优先级：中高
目标类型：降低单类认知负担，文件数只会小幅变化
风险：中

`InspectionApplicationService.cpp`当前约1,330行。它是UI到运行核心的合法门面，但同时包含模式转换、启动预检、相机操作、PLC连接、模板预览、运行状态、结果视图绑定、统计重置和Fault发布。

本批次不再新增一组空壳Controller，而采用以下方式收口：

1. 先绘制所有公开方法的真实调用图。
2. 删除没有UI入口或只有一层原样转发的方法。
3. 将纯数据转换放回对应数据类型或已有服务，不留在大服务内部。
4. 保留一个应用层门面，但公开接口只表达用户用例。
5. 设置继续由`SettingsApplicationService`负责，模板编辑继续由`TemplateApplicationService`负责。
6. 相机、PLC和Runtime的具体实现仍由现有下层对象承担，应用服务只编排用例和错误结果。

不以“把一个大类拆成十个只有一个函数的小类”为完成标准。完成标准是调用路径变短、公开方法减少、责任边界可用一句话解释。

### S4：收缩ResultService，但保留唯一结算入口

优先级：中
目标类型：降低高风险核心的复杂度
风险：中高

`ResultService`必须继续是唯一产品结果结算入口，不能拆成多个可以分别更新统计、PLC和存图的正式入口。精简方向是把“结算决策”和“具体执行机制”分开：

- `ResultService`保留去重、唯一认领、统计更新、一次性副作用决策和Fault规则。
- UI呈现绑定按S2移出Runtime，只发布完整`InspectionPresentation`。
- PLC字节写入和脉冲时序归入现有PLC运行边界或一个明确的PLC结果输出组件。
- 图片保存任务构造与实际写入规则归入图片保存边界，`ResultService`只提交一次产品保存请求。
- 五种模式重复的`preparePresentation/finalizePresentation`样板收敛为数据驱动的呈现构造。

当前PLC复位使用单个`QTimer`。多件NG在100ms窗口内是否会合并脉冲属于行为正确性问题，不能借“精简”名义顺手改变。若实施中确认需要修复，应单独列出PLC影响功能和真实现场门禁。

同样，`Cancelled`结果是否可能被接受后未统计、未存图、未输出PLC而在正常停止时清除，属于独立正确性核对项，不并入无行为变化的结构精简批次。

### S5：把UI页面从“文件拆分”变为“所有权拆分”

优先级：中
目标类型：显著降低理解成本，但不承诺减少文件数
风险：高

当前`InspectionPage`和`MachineSettingsPage`仍保存`Ui::MainWindow *`，`TemplateEditorPage`通过`TemplateEditorViewBindings`接收大量控件指针，`MainWindow`还公开`viewForComposition()`。这说明当前只是把实现代码移动到了不同文件，并没有真正形成页面组件。

目标结构：

```text
MainWindow
├─ InspectionPage        自己拥有检测页控件和显示逻辑
├─ MachineSettingsPage   自己拥有设置页控件和dirty交互
└─ TemplateEditorPage    自己拥有模板页控件
   ├─ Canvas/ROI交互
   ├─ Profile编辑
   └─ 配方发布与选择
```

实施原则：

- 每个页面拥有自己的根Widget和控件，不再接收整份`Ui::MainWindow *`。
- `MainWindow`只组合页面、转发顶层命令和展示顶层错误。
- 页面之间通过类型化数据和信号协作，不交换控件指针。
- 模板编辑器当前由同一个类跨5个`.cpp`实现，后续按真实职责拆成少量内部组件，而不是重新合回一个巨型文件。
- 不为了追求文件更少，把页面逻辑全部塞回`MainWindow`。

本批次可能新增少量`.ui/.h/.cpp`文件，但会显著减少一个修改需要同时理解的代码范围。它属于认知复杂度精简，不属于文件数量精简。

### S6：把定位和预处理归回DetectionWorker

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

该调整可能改变吞吐、队列满时机、停止时序和硬触发Fault条件，因此必须作为最后一个独立批次，使用真实相机验证，不能与UI或其他代码精简混做。

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

因此本计划暂时保留这个小型共享合同目录。后续只允许在以下条件下调整：

- 能为类型找到不制造反向依赖的唯一所有者。
- 不复制默认值或字符串转换。
- 不让Recipes依赖Detection实现，也不让Runtime依赖Application。

`barcode_parameter_defaults.h`可以在二维码参数模型重新明确所有权时一并收口，但不作为优先精简项。

## 七、推荐实施顺序

```text
当前8项门禁收口
→ S1 零调用代码清理
→ S2 结果到UI单快照链
→ S3 应用服务与纯转发接口收缩
→ S4 ResultService职责收缩
→ S5 UI真实页面化
→ S6 定位/预处理迁入DetectionWorker
```

每个S批次都是一个独立批准、独立差异、独立门禁和独立提交的轮次。上一批次未通过，不进入下一批次。

不建议把S1到S6一次完成。死代码删除、UI重构、结果链和线程职责调整的验证方法不同，混在同一差异中会使问题难以定位。

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
- `git diff --check`通过。
- 差异中不包含`*.pro.user`、`*.ui.autosave`、`app.zip`或其他用户文件。
- Agent不运行qmake、构建、测试程序或主程序。

### 8.3 用户Qt Creator门禁

每个代码批次完成最终差异后，由用户统一执行：

1. Run qmake。
2. Rebuild主工程。
3. 回归该批次所有受影响功能。
4. 涉及相机线程、触发、定位或PLC时，执行对应真实设备门禁。

### 8.4 收口

- 用户门禁通过后，受影响保留功能恢复为`已验证`。
- 更新功能对照表和重构执行记录。
- 精确暂存本批次文件，复核暂存清单后创建唯一的本地提交。
- 不自动推送、合并或变基。

## 九、预期结果

S1已减少2个影子比较器代码文件，并按完整调用审计清理了上述零调用符号和只写状态；`.h/.cpp`由166个降为164个。资源文件数量保持不变，不作为精简指标。

完成S2到S6后，更重要的变化不是总文件数，而是主要调用链可以清晰表达为：

```text
UI用户操作
→ Application用户用例
→ Runtime生命周期
→ Capture/Detection
→ ResultService唯一结算
→ 一份InspectionPresentation
→ UI一次应用
```

最终期望达到：

- 新开发者只需沿正式代码入口阅读，不需要把资源文件和vendor源码当成业务架构逐个分析。
- 通过目录即可找到设备、算法、配方、运行时和UI的唯一入口。
- 修改一个页面不需要持有整份主窗口UI。
- 修改结果呈现不触碰统计、PLC和存图唯一性。
- 修改检测算法不触碰相机SDK、UI和磁盘。
- 重要的工业安全边界保留，迁移期遗留和无效代码被清除。

## 十、本计划不包含的工作

- 不调整算法准确率、模型、阈值或ROI业务定义。
- 不改变正常PLC值、地址或约100ms脉冲合同。
- 不升级Qt、OpenCV、PaddleOCR、Barcode DLL或Snap7。
- 不恢复已删除的多相机功能。
- 不恢复旧设置、旧模板或旧目录兼容。
- 不自行重建已删除的自动测试目录。
- 不删除、合并、改名或精简图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL和其他资源文件。
- 不把已发现但尚未批准的正确性问题伪装成结构精简一起修改。
