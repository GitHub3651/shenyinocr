# OCRGangYin UI 架构精简优化方案

## 1. 文档状态

- 文档用途：指导当前 UI 统一样式完成后的代码架构精简。
- 编写日期：2026-08-21。
- 最新修订：2026-08-21，按钢印真实检测语义修订批次 B，将旧“三步、两个矩形、一个多边形”方案替换为五模式 `InspectionImageCanvas` 绘图状态机与模板向导直接绑定方案，并明确只保留一个应用层 `TemplateDrawingInput`，不再增加重复的 UI 几何快照结构。
- 当前基线：以实施开始时已经通过用户验证并提交的最新 UI 版本为准，不直接以本文编写时的未提交工作区作为实施基线。
- 当前执行状态：批次 A 已由提交 `85d3b58` 独立收口；批次 B 已按修订后的五模式方案完成代码修复和 Agent 静态门禁，等待用户在 Qt Creator 构建、完成人工交互及真实钢印样本验证，未通过前不得提交。
- 目标范围：`app/ui`、`app/startup/application_startup.cpp`、现有模板几何应用服务以及对应 qmake 清单。
- 实施前提：当前“UI 统一样式重构”必须先完成 Qt Creator 构建、人工验证和本地提交。
- 本文是 UI 后续优化计划，不改变阶段 8 已确认的 `AppSettings + 外部模板文件夹 + TemplateStore` 数据方案。
- 本文中修订后的 UI-4 方案是当前批次 B 权威；旧版“三步刚印框选”描述不再有效。

## 2. 结论

当前 UI 的业务边界、统一状态和统一 QSS 方向正确，主要问题不是文件数量，也不是运行性能，而是迁移期间形成的 UI 组合支架没有拆除：

1. `application_startup` 创建 `MainWindow` 后，又向 `MainWindow` 索取控件、状态指针和回调来创建三个 Page。
2. 三个 Page 创建后再通过 `attachPages()` 挂回 `MainWindow`。
3. `MainWindow` 保留大量只转发给 Page 的一行函数。
4. 稳定存在的界面控件仍有一部分在 C++ 中动态创建，Qt Designer 无法展示完整布局。
5. 模板制作需要统一使用 Qt `InspectionImageCanvas`，并按检测模式切换绘图状态；刚印模式必须分别采集吸管口锚点、刚印多边形、日期锚点和日期多边形，不能复用同一个多边形。

最终优化方向不是继续拆文件，而是删除过渡胶水：

```text
application_startup
    ├── 创建应用级服务
    └── 创建 MainWindow(应用级服务)

MainWindow
    ├── setupUi()
    ├── 创建并持有 InspectionPage
    ├── 创建并持有 MachineSettingsPage
    ├── 创建并持有 TemplateEditorPage
    ├── 负责少量跨页面协调
    └── 不再向 Startup 暴露内部控件、状态地址和 UI 回调

三个 Page
    ├── 负责各自区域的显示、输入、校验和控件连接
    ├── 只调用应用服务或使用只读 DTO
    └── 不拥有设备、生产线程、算法、PLC 时序或模板磁盘
```

## 3. 当前架构及形成原因

### 3.1 当前实际流程

```text
application_startup
    ├── 创建 MainWindow
    ├── 调用 MainWindow::inspectionPageViewBindings()
    ├── 调用 MainWindow::machineSettingsPageViewBindings()
    ├── 调用 MainWindow::templateEditorViewBindings()
    ├── 索取 QTimer*、bool*、QString*、SettingsEditState*
    ├── 索取三组 Callback
    ├── 创建 InspectionPage/MachineSettingsPage/TemplateEditorPage
    └── 调用 MainWindow::attachPages()
```

页面运行后又出现反向调用：

```text
MainWindow 自动槽 → Page
Page → Callback → MainWindow
Page → ApplicationService
MainWindow → ApplicationService
```

### 3.2 为什么最初采用该结构

该结构原本用于从历史巨大 `MainWindow` 中安全迁移代码，目标是：

- 保留一个 `main_window.ui`，避免同时改变界面布局、对象名和 Qt 自动槽。
- 不一次性重写全部相机、PLC、模板和设置行为。
- 通过 ViewBindings 逐步把控件行为迁移到 Page。
- 保持 UI 不直接依赖 Runtime、设备和具体检测算法。
- 每次只迁移一个职责，降低阶段 0～8 重构期间的功能回归风险。

ViewBindings 和 Page 在迁移期是合理支架；错误在于迁移完成后又把三个 UI 内部 Page 当成应用级组件交给 Startup 创建，导致支架成为长期架构。

### 3.3 本文冻结的纠正原则

“Startup 是组合根”只表示 Startup 负责应用级、外部资源级对象，例如：

- 设置存储与应用服务。
- 模板存储与应用服务。
- 相机、PLC、Runtime 和检测注册表。
- 主窗口。

它不表示 MainWindow 的每个内部 UI 辅助对象都必须由 Startup 创建。三个 Page 只服务于 `main_window.ui`，由 MainWindow 内部创建不会破坏组合根原则。

## 4. 当前量化基线

本文编写时静态统计如下，实施时应重新统计：

| 项目 | 当前数量 |
|---|---:|
| `app/ui` 中 `.h/.cpp` | 24 个 |
| UI C++ 总行数 | 约 7862 行 |
| `main_window.ui` | 1 个，约 2071 行 |
| `TemplateEditorPage` 实现 | 约 1401 行 |
| `MachineSettingsPage` 实现 | 约 1010 行 |
| MainWindow 三个实现文件合计 | 约 2116 行 |
| MachineSettings ViewBindings 控件指针 | 约 46 个 |
| TemplateEditor ViewBindings 控件指针 | 约 19 个 |
| Inspection ViewBindings 控件指针 | 约 15 个 |
| MainWindow 对外 UI 组合接口 | 约 13 个 |
| 无有效信息的模板化函数注释 | 约 167 条 |

这些数据说明：当前问题是少数文件中的胶水和职责回流，不是目录里代码文件过多。

## 5. 必须保持的设计

以下结构是有效设计，本轮不得因“精简”而删除。

### 5.1 保留三个 Page

- `InspectionPage`：检测结果、统计、故障和主操作按钮呈现。
- `MachineSettingsPage`：设置控件绑定、校验、已应用值和未应用状态。
- `TemplateEditorPage`：模板选择、当前编辑模板、取景、框选和模板参数。

Page 的存在不是问题；问题是它们的外部创建方式和 MainWindow 重复转发。

### 5.2 保留 OperationUiPolicy

继续由唯一状态规则计算：

- 控件是否可用。
- 控件禁用原因。
- 主按钮文字。
- 检测、停止、故障和模板制作状态。

不得恢复各页面自行判断同一状态的方式。

### 5.3 保留 SettingsEditState

继续统一跟踪：

- 已应用的整机设置。
- 尚未应用的整机设置。
- 当前模板目标文字和阈值的未保存修改。
- 启动检测前的未应用修改提示。

该状态被设置页、模板页和启动流程共同使用，不应为减少两个文件而复制回多个页面。

### 5.4 保留 InspectionFaultPresenter

故障文案包含现场安全、已受理件数、已完成件数和恢复提示，属于高风险、需要一致呈现的内容。保留集中转换，不优先为了减少两个小文件而合并。

### 5.5 保留单一正式 QSS

- 正式样式继续只从 `:/qss/app_theme.qss` 加载。
- C++ 只设置 `uiRole/uiState/verdict/hasError` 等状态属性。
- 不恢复控件级完整 `setStyleSheet()` 字符串。
- 不新增 ThemeManager、样式工厂或多个运行时主题服务。

### 5.6 保留应用边界

- UI 只调用 ApplicationService、应用命令、查询和只读 DTO。
- UI 不直接持有 TemplateStore、相机 SDK、PLC Controller、Runtime 或 Pipeline。
- Runtime 不依赖 QWidget、QMessageBox 或 `Ui::MainWindow`。
- Detection 不访问 UI、磁盘、PLC 或相机 SDK。

## 6. 需要解决的问题

### 6.1 Startup 知道过多 UI 内部细节

当前 Startup 知道：

- MainWindow 内部有哪些控件分组。
- 哪些状态保存在 MainWindow。
- Page 构造需要哪些 UI 指针。
- Page 与 MainWindow 之间有哪些回调。
- Page 何时挂回 MainWindow。

这使启动层和界面内部结构耦合。以后增加一个设置控件，可能同时修改：

1. `main_window.ui`。
2. ViewBindings 结构。
3. MainWindow 的绑定构造。
4. Startup 的 Page 构造参数。
5. Page 本身。

目标是让 Startup 只知道 MainWindow 构造所需的应用服务。

### 6.2 MainWindow 暴露内部状态地址

当前存在对外返回：

- `QTimer*`。
- `bool*`。
- `QString*`。
- `SettingsEditState*`。

Page 通过这些地址修改 MainWindow 内部状态，所有权不直观，也使生命周期必须依赖调用者保持正确顺序。

目标是：

- UI 局部状态由使用它的 Page 持有。
- 跨页面状态由 MainWindow 明确协调。
- 正式设置只由 `SettingsApplicationService` 持有。
- 不再通过 Startup 转发 MainWindow 成员地址。

### 6.3 MainWindow 存在大量一行转发

典型形式：

```cpp
void MainWindow::on_pushButton_applyTargetText_clicked()
{
    m_templateEditorPage->applyCurrentTargetText();
}
```

这类函数没有协调、校验或错误处理，只增加一次跳转。目标是让 Page 在初始化时直接连接自己负责的按钮；MainWindow 只保留真正跨 Page 或跨应用服务的协调槽。

### 6.4 稳定布局在 C++ 中创建

当前固定存在但由 C++ 创建的主要界面包括：

- “当前编辑模板”标签、下拉框和“移除模板”按钮。
- 模板制作向导标题和说明栏。
- 检测信息滚动包装。

这些结构不是根据数据数量动态生成，只是按模式显示或隐藏，应尽量放在 `main_window.ui`。否则 Qt Designer 无法展示真实布局，容易出现尺寸、边框和排列问题。

QSplitter Handle 的自定义抓手属于运行时对象，可以继续留在 C++，不要求为了形式统一迁入 `.ui`。

### 6.5 `InspectionImageCanvas` 缺少按模式区分的绘图状态

模板制作必须继续只使用 Qt 主界面的同一个 `InspectionImageCanvas`。原 OpenCV HighGUI 交互应保持删除，生产代码不得恢复：

- `cv::namedWindow()`。
- `cv::imshow()`。
- `cv::setMouseCallback()`。
- `cv::waitKey()` 循环。

当前批次 B 初版虽然删除了第二套窗口，但把全部模式压缩成统一状态：

```text
STEP_TRACKING
    ↓
可选 STEP_SECONDARY_RECT
    ↓
STEP_DETECTION_POLY
    ↓
STEP_DONE
```

该状态只能保存一个 `m_detectionPoly`。它可以表达字库、OCR 和二维码流程，却不能表达钢印检测的两个独立定位系统：

```text
吸管口定位锚点 → 钢印多边形

生产日期定位锚点 → 生产日期多边形
```

初版保存代码把唯一绿色多边形先转换为 `datePolygon`，随后又使用同一组点推导 `stampPolygon`，造成严重语义回归。无论用户把唯一多边形画在钢印上还是生产日期上，另一套检测区域都会错误。

目标是让 `InspectionImageCanvas` 根据当前检测模式选择明确的绘图步骤、几何存储和显示内容；纸巾模式完全禁用模板绘图。`InspectionImageCanvas` 只负责鼠标/键盘交互、UI 坐标和框线显示，不负责提示文案、二维码业务校验、模板保存或检测算法。

### 6.6 模板向导连接仍经过 MainWindow 且使用字符串协议

当前 `InspectionImageCanvas` 已经会在鼠标处理后发出模板向导事件，但接口为：

```cpp
signal_templateGuideEvent(QString eventName, int pointCount)
```

并由 `MainWindow` 转接给 `TemplateEditorPage`。这存在三个问题：

1. `"tracking_done"`、`"poly_point_added"` 等字符串没有编译期检查。
2. Page 自有的模板绘图行为仍然经过 MainWindow 中转。
3. `InspectionImageCanvas` 还保留无人消费的原始 `QMouseEvent*` 信号，容易让后续代码错误依赖底层鼠标事件。

模板向导不需要读取原始鼠标事件。`InspectionImageCanvas` 应内部消费 `mousePressEvent/mouseMoveEvent/mouseReleaseEvent/keyPressEvent`，只向 `TemplateEditorPage` 发出强类型的绘图进度事件；模板页再更新现有 `.ui` 中的向导标题和正文。

### 6.7 无效代码、重复状态和无效注释

当前可确认或需要实施前复核的项目包括：

- `m_barcodeWordRunActive` 只有赋值，没有有效读取。
- `PLCmode` 可以改为函数局部变量。
- `TemplateEditorSupport::setLabelTextIfChanged()` 没有调用者。
- `TemplateEditorSupport::isSingleTemplateMode()` 没有调用者。
- `MainWindow::adjustTemplateGuideHeight()` 等部分转发接口没有外部调用者。
- `TemplateEditorPage::guideFrame()`、`manualCharacterCropButton()` 在 Tooltip 事件过滤删除后没有调用者。
- `selectedDir` 与 AppSettings 图片保存路径存在重复正式值风险。
- 大量“函数说明：某函数实现名称所表示的处理步骤”注释没有提供维护信息。

删除前必须用静态引用检查重新确认，不能按本文列表机械删除。

## 7. 目标架构

### 7.1 对象所有权

```text
ApplicationStartup
    ├── shared SettingsApplicationService
    ├── shared InspectionApplicationService
    ├── shared TemplateApplicationService
    └── stack MainWindow

MainWindow
    ├── Ui::MainWindow
    ├── unique_ptr<InspectionPage>
    ├── unique_ptr<MachineSettingsPage>
    ├── unique_ptr<TemplateEditorPage>
    ├── 顶层窗口定时器
    └── 少量跨页面协调状态
```

Page 的销毁必须早于 `ui` 控件失效。实施时采用以下任一简单方式，不新增生命周期管理类：

1. `MainWindow` 析构中先 `reset()` 三个 Page，再删除 `ui`；或
2. Page 继承 QObject 并以 MainWindow 为父对象，同时移除外部 `unique_ptr`。

优先采用第一种，因为不要求为了生命周期给没有信号槽需求的类型强制增加 QObject 层级。

### 7.2 MainWindow 的最终职责

MainWindow 只保留：

- `setupUi()` 和 Page 内部组合。
- 顶层窗口关闭和软件退出。
- 应用级服务与 UI 页面之间的顶层绑定。
- 检测模式变化时设置页与模板页的跨页面协调。
- 相机、PLC、模板制作等确实跨两个以上 Page 的流程协调。
- 加载正式 QSS。

MainWindow 不再承担：

- 单个模板参数按钮的一行转发。
- 单个设置控件的脏状态细节。
- 仅供 Startup 创建 Page 的控件打包接口。
- 仅供 Startup 修改 MainWindow 内部状态的指针接口。

### 7.3 ViewBindings 的处理

本轮不强制删除 ViewBindings。

保留原因：

- 当前只有一个 `main_window.ui`。
- ViewBindings 能限制每个 Page 可访问的控件范围。
- 直接把完整 `Ui::MainWindow*` 交给每个 Page 虽然少写代码，但会让 Page 可以访问全部控件，边界更差。
- 使用 `findChild()` 按字符串寻找控件会失去编译期检查。

优化方式：

- ViewBindings 只在 MainWindow 内部构造和传递。
- 不再通过 Startup 暴露。
- 删除 Page 不再需要的控件指针。
- 不为 ViewBindings 再增加 Builder、Factory 或注册表。

### 7.4 页面间协调

只允许两种方式：

1. MainWindow 直接调用两个已有 Page 的明确方法，适合检测模式变化等少量顶层协调。
2. Page 调用 ApplicationService，由正式状态变化信号驱动其他页面更新。

不新增事件总线、消息中心或通用命令分发器。

## 8. 分阶段实施计划

### 阶段 UI-0：冻结当前 UI 基线

#### 目标

先结束当前统一样式和 Tooltip 重构，避免把尚未验证的视觉修改与架构精简混在同一差异中。

#### 操作

1. Qt Creator 执行 Run qmake 和 Rebuild。
2. 验证五种模式布局、Tooltip、模板显示/隐藏、输入光标、状态禁用和 QSS。
3. 修复构建或人工回归问题。
4. 用户明确验证通过后提交当前 UI 样式版本。
5. 记录新阶段开始 HEAD 和干净工作区。

#### 完成条件

- 当前 UI 样式提交已存在。
- 工作区无未归属修改。
- 后续 UI 架构差异可独立审查和回滚。

### 阶段 UI-1：纯删除和局部精简

#### 目标

不改变界面、调用结果和数据，只删除已经没有意义的代码。

#### 主要工作

1. 删除无调用的 MainWindow 转发函数和 Page 访问器。
2. 删除未使用的辅助函数、include、前置声明和成员。
3. 将 `PLCmode` 等只在一个函数使用的状态改为局部变量。
4. 确认 `m_barcodeWordRunActive` 无读取后删除。
5. 将图片保存路径收敛到 AppSettings 草稿和对应控件，不保留平行 `selectedDir` 正式值。
6. 删除模板化、无信息量的函数注释，保留真正说明原因、单位、线程、安全和兼容边界的注释。
7. 更新 `app/ui/README.md` 的实际维护说明。

#### 明确不做

- 不移动 Page 所有权。
- 不改变 Startup。
- 不改变 `.ui` 布局。
- 不改变刚印框选。
- 不改变设置、模板和检测行为。

#### 静态门禁

- 所有删除符号零引用。
- qmake 中无删除文件残留。
- UTF-8 和文件末尾换行正确。
- `git diff --check` 通过。
- 无资源文件差异。

### 阶段 UI-2：收回 Page 组合与状态所有权

#### 目标

让三个 Page 成为 MainWindow 的内部实现对象，删除 Startup 与 MainWindow 之间的 UI 往返组合。

#### Startup 修改

`app/startup/application_startup.cpp` 删除：

- 三个 Page 的 `unique_ptr` 局部变量。
- `window.*PageViewBindings()` 调用。
- `window.*ForComposition()` 调用。
- `window.*PageCallbacks()` 调用。
- `window.attachPages()`。

Startup 最终只保留：

```cpp
MainWindow window(
    inspectionService,
    settingsService,
    templateService);
window.showMaximized();
```

#### MainWindow 修改

1. `setupUi()` 后按依赖顺序创建三个 Page。
2. 在 MainWindow 内部构造现有 ViewBindings。
3. 在 MainWindow 内部建立确有必要的跨页面回调。
4. 直接完成 `InspectionApplicationService::bindView()`。
5. 将 `attachPages()` 后的初始化流程合并为一个私有 `initializePages()` 或直接放在构造函数的明确段落中。
6. MainWindow 持有三个 Page 的唯一所有权。
7. 析构时先销毁 Page，再释放 `ui`。

不得新增 `PageManager`、`UiCompositionRoot`、`MainWindowBuilder` 或新的组合文件。

#### 状态所有权修改

| 当前状态 | 目标所有者 |
|---|---|
| 模板按钮闪烁定时器和布尔值 | `InspectionPage` 或 MainWindow 内部直接协作，不再传给 Startup |
| 设置页 applying/updating 标志 | `MachineSettingsPage` 私有状态 |
| 图片保存路径 `selectedDir` | `SettingsApplicationService::editableDraft().imageSavePath` |
| `SettingsEditState` | MainWindow 内部共享对象，直接传给内部 Page，不暴露给 Startup |
| Page Callbacks | MainWindow 构造 Page 时内部创建，不再成为公共 API |

检测模式初始化时优先使用 `QSignalBlocker` 避免触发用户操作槽，不通过共享 `bool*` 抑制信号。

#### 完成后必须清零

- `ForComposition`。
- `attachPages`。
- Startup 中的 `InspectionPage/MachineSettingsPage/TemplateEditorPage`。
- Startup 对 `ViewBindings`、Page Callback 和 UI 状态地址的认识。

### 阶段 UI-3：删除一行转发并把固定布局放回 `.ui`

#### 目标

缩短按钮到 Page 的调用链，并让 Qt Designer 显示实际固定布局。

#### 直接连接规则

Page 自己负责的按钮在 Page 初始化时直接 `connect()`：

- 选择模板。
- 保存模板。
- 应用当前目标文字。
- 批量应用目标文字。
- 应用当前阈值。
- 批量应用阈值。
- 应用纸巾阈值。
- 字符模板编辑。
- 移除当前模板引用。

MainWindow 保留：

- 相机打开/关闭。
- 检测启动/停止。
- PLC连接、断开和运行写入。
- 需要同时协调设置页、模板页和检测页的模式切换。
- 顶层退出、故障恢复和应用生命周期。

#### 固定布局迁移

优先迁入 `main_window.ui`：

1. 当前编辑模板标签、下拉框和“移除模板”按钮。
2. 模板制作向导 Frame、标题 Label 和正文 Label。
3. 检测信息滚动容器；只有确认 Qt Designer 结构不会破坏当前 Splitter 行为时才迁移。

保留在 C++：

- QSplitter Handle 内的抓手装饰。
- QTimer。
- 根据模式变化的文字、步骤、可见性和属性。
- 数据数量真正动态的列表项、模板项和字符预览项。

#### QSS 规则

- 迁入 `.ui` 后保持现有对象名，尽量不改 `app_theme.qss` 选择器。
- 若必须改名，同一差异中同步修改 `.ui`、C++、QSS 和文档。
- 不在迁移过程中新增内联样式。

### 阶段 UI-4：`InspectionImageCanvas` 五模式绘图与模板向导绑定

#### 性质与当前状态

UI-4 是批次 B，也是唯一明确改变模板绘图交互的阶段，必须与已提交的 UI-1～UI-3 分开验证和提交。

批次 B 初版已经完成两项正确改造：

- OpenCV HighGUI 模板交互已经从生产代码删除。
- `template_editor_support.h/.cpp` 在只剩页面私有小函数后已经收口删除。

但初版错误地把钢印模式实现为“三步、两个矩形、一个多边形”。经检测 Pipeline、定位器、重叠检测器和模板保存字段逐项核对，真实钢印流程必须是“四步、两个定位锚点、两个独立多边形”。因此当前批次 B 必须停留在验证修复状态，旧三步实现不能提交。

本次修复仍属于原批次 B，不新增第三个 UI 批次，不恢复 OpenCV 窗口，也不改变模板 Schema 或检测算法。

#### 五种模式的绘图流程

| 检测模式 | `InspectionImageCanvas` 绘图步骤 | 需要保存的 UI 几何 | 显示建议 |
|---|---|---|---|
| 字库匹配 | 定位区域 → 文字检测多边形 | `trackingAnchorRect`、`datePolygon` | 蓝色定位框＋绿色文字区域 |
| 深度 OCR | 定位区域 → OCR 检测多边形 | `trackingAnchorRect`、`datePolygon` | 蓝色定位框＋绿色 OCR 区域 |
| 二维码＋三期 | 日期定位锚点 → 二维码矩形 → 日期多边形 | `trackingAnchorRect`、`barcodeRect`、`datePolygon` | 蓝色锚点＋黄色二维码框＋绿色日期区域 |
| 刚印检测 | 吸管口定位锚点 → 钢印多边形 → 日期定位锚点 → 日期多边形 | `stampAnchorRect`、`stampPolygon`、`trackingAnchorRect`、`datePolygon` | 橙色吸管口框＋橙红钢印区域＋蓝色日期锚点＋绿色日期区域 |
| 纸巾检测 | 不进入模板绘图 | 无 | 不显示模板框线和模板制作向导 |

其中磁盘字段继续沿用阶段 8 已确认语义：

- `trackingRoi`：生产日期、文字或 OCR 的定位锚点。
- `datePolygon`：相对 `trackingRoi` 中心的检测区域；字库、OCR、二维码和刚印日期检测继续复用现有字段名。
- `barcodePolygon`：相对 `trackingRoi` 中心的二维码四点区域。
- `stampRingTemplate`：由吸管口定位锚点裁剪得到的现有刚印定位资源；不修改模板文件名和算法输入名。
- `stampPolygon`：相对吸管口定位锚点中心的钢印区域。

#### 钢印模式的唯一正确流程

```text
步骤1/4：框选吸管口定位锚点
    ↓
步骤2/4：左键依次点击钢印区域边缘，右键闭合
    ↓
步骤3/4：框选生产日期定位锚点
    ↓
步骤4/4：左键依次点击生产日期区域边缘，右键闭合
    ↓
询问是否立即保存模板
```

保存关系固定为：

```text
stampRingTemplate = 原图中的吸管口定位锚点裁剪图
stampPolygon      = 原图钢印多边形 - 吸管口定位锚点中心

trackingTemplate  = 原图中的生产日期定位锚点裁剪图
trackingRoi       = 生产日期定位锚点原图矩形
datePolygon       = 原图生产日期多边形 - 生产日期定位锚点中心
```

运行时继续保持：

```text
吸管口模板匹配 → 变换 stampPolygon → 当前钢印绝对区域

日期锚点模板匹配 → 变换 datePolygon → 当前生产日期绝对区域
                                      ├── 日期区域执行字库匹配
                                      └── 日期区域与钢印区域计算重叠

最终结果 = 字符匹配合格 && 不发生钢印/日期重叠
```

#### `InspectionImageCanvas` 模式接口

`InspectionImageCanvas` 是项目内专用模板绘图控件，不需要再抽象成通用画布框架。建议用当前稳定的 `DetectionMode` 直接启动模式化绘图：

```cpp
void beginTemplateDrawing(DetectionMode mode);
void cancelTemplateDrawing();
DetectionMode templateDrawingMode() const;
bool isTemplateDrawingComplete() const;
```

`beginTemplateDrawing()` 负责：

1. 记录当前检测模式。
2. 清空上一模式的全部临时几何。
3. 选择该模式的第一步。
4. 启用键盘焦点和鼠标交互。
5. 发出初始绘图状态事件，由模板页显示第一步提示。

`cancelTemplateDrawing()` 负责：

1. 终止当前拖动或多边形采集。
2. 清空全部临时几何和预览点。
3. 回到 `Idle`。
4. 更新画面。
5. 不修改任何已保存模板、设置或运行快照。

纸巾模式调用 `beginTemplateDrawing(DetectionMode::Tissue)` 时应直接等价于禁用绘图，不进入任何选择步骤。

#### 强类型绘图步骤

删除当前过于通用的：

```text
STEP_TRACKING
STEP_SECONDARY_RECT
STEP_DETECTION_POLY
```

改为能表达业务语义的强类型步骤。名称可以在实施时按现有代码风格微调，但职责必须保持：

```cpp
enum class DrawingStep {
    Idle,
    TrackingAnchor,
    DetectionPolygon,
    BarcodeRegion,
    StampAnchor,
    StampPolygon,
    DateAnchor,
    DatePolygon,
    Complete
};
```

模式到步骤的转换固定为：

```text
Word / Ocr
    TrackingAnchor → DetectionPolygon → Complete

BarcodeWord
    TrackingAnchor → BarcodeRegion → DatePolygon → Complete

Stamp
    StampAnchor → StampPolygon → DateAnchor → DatePolygon → Complete

Tissue
    Idle
```

不新增 `DrawingStrategy` 子类、状态机框架、注册表或每模式独立 Widget。一个明确的 `switch (DetectionMode)` 和一个 `switch (DrawingStep)` 足以表达当前五种模式。

#### `InspectionImageCanvas` 独立几何成员

当前单个 `m_secondaryRect` 和单个 `m_detectionPoly` 必须直接拆成 `InspectionImageCanvas` 的五个语义明确的私有成员，避免跨模式和跨区域复用：

```cpp
QRect m_trackingAnchorRect;
QRect m_barcodeRect;
QRect m_stampAnchorRect;
QPolygon m_datePolygon;
QPolygon m_stampPolygon;
```

`InspectionImageCanvas` 对模板页只提供对应的只读 getter：

```cpp
QRect trackingAnchorRect() const;
QRect barcodeRect() const;
QRect stampAnchorRect() const;
QPolygon datePolygon() const;
QPolygon stampPolygon() const;
```

不得再增加 `TemplateDrawingGeometry` 或其他内容相同的 UI 快照结构。`TemplateEditorPage` 只在完成校验或保存时读取这些 getter；`InspectionImageCanvas` 不依赖应用层的 `TemplateDrawingInput`，也不负责把显示坐标转换成原图坐标。

几何清理规则：

- 进入新模式时清空全部字段，不保留旧模式框线。
- 字库和 OCR 只允许 `trackingAnchorRect + datePolygon` 非空。
- 二维码模式只允许 `trackingAnchorRect + barcodeRect + datePolygon` 非空。
- 刚印模式允许 `stampAnchorRect + stampPolygon + trackingAnchorRect + datePolygon` 非空。
- 纸巾模式所有字段必须为空。
- 保存前按当前模式检查必需字段，不能只检查“任意一个矩形和任意一个多边形”。

#### 模式化绘制

`paintEvent()` 只绘制当前模式允许的几何：

- 蓝色矩形：文字、OCR、二维码或刚印日期的定位锚点。
- 黄色矩形：二维码区域。
- 橙色矩形：吸管口定位锚点。
- 橙红色多边形：钢印区域。
- 绿色多边形：文字、OCR或生产日期检测区域。
- 当前正在点击的多边形继续显示鼠标跟随虚线和闭合预览。
- 已闭合多边形显示实线闭合边界和顶点。

颜色只用于区分同一图像中的区域角色，不在 C++ 拼接控件样式，也不修改正式 QSS 体系。

#### 模板向导与 `InspectionImageCanvas` 的绑定

模板向导需要和 `InspectionImageCanvas` 的绘图进度绑定，但不能让 `InspectionImageCanvas` 直接持有 `QFrame/QLabel` 或生成业务文案。

正确方向是：

```text
TemplateEditorPage -- beginTemplateDrawing(mode) --> InspectionImageCanvas

InspectionImageCanvas -- templateDrawingChanged(...) --> TemplateEditorPage

TemplateEditorPage -- setText/show/hide --> main_window.ui 中的模板向导
```

原始 `QMouseEvent*` 不应跨出 `InspectionImageCanvas`。模板提示真正需要的不是鼠标坐标，而是以下语义事件：

```cpp
enum class DrawingEvent {
    StepStarted,
    StepCompleted,
    RegionTooSmall,
    PointAdded,
    TooFewPoints,
    Reset,
    WorkflowCompleted
};

signals:
    void templateDrawingChanged(
        DrawingStep step,
        DrawingEvent event,
        int pointCount);
```

`mouseMoveEvent()` 只更新拖框、多边形跟随线和 `paintEvent()`，不需要为模板向导持续发送事件。模板提示只在步骤开始、步骤完成、校验失败、点数变化、Esc 和全流程完成时更新。

`TemplateEditorPage` 在构造或 `setupTemplateGuide()` 中直接连接：

```cpp
connect(imageLabel,
        &InspectionImageCanvas::templateDrawingChanged,
        this,
        &TemplateEditorPage::handleTemplateDrawingChanged);
```

必须删除 `MainWindow` 中对模板绘图信号的一行中转连接。`MainWindow` 仍可负责“检测模式发生变化”这一跨页面协调，但只调用模板页的明确模式切换方法；由模板页取消绘图、清空框线和隐藏提示。

当前无人消费的：

```cpp
mousePressed(QMouseEvent *)
mouseMoved(QMouseEvent *)
mouseReleased(QMouseEvent *)
```

在全仓引用确认仍为零后删除。不得为了模板向导重新使用这些原始信号。

#### 向导文字职责

`TemplateEditorPage` 继续拥有全部中文标题、步骤文字、二维码失败提示和保存确认。`InspectionImageCanvas` 不负责：

- 检测模式中文名称。
- “步骤 1/4”等提示文案。
- `QMessageBox`。
- 二维码即时解码。
- 模板目录选择和保存。
- `TemplateStore` 或 ApplicationService 调用。

向导必须覆盖：

- 当前模式总步骤数。
- 当前正在操作的区域名称。
- 拖动矩形时的完成提示。
- 多边形当前点数。
- 少于三个点不能闭合。
- 矩形太小时要求重新框选。
- Esc 清空并回到第一步。
- 工作流完成后询问是否立即保存。

二维码模式保持既有特殊行为：完成二维码矩形后由 `TemplateEditorPage` 调用现有即时解码校验；失败时保留定位锚点，只命令 `InspectionImageCanvas` 回到二维码步骤并清除二维码及后续日期区域。

#### 模式切换与重置

- 冻结模板预览后，`TemplateEditorPage` 根据当前 `DetectionMode` 调用 `beginTemplateDrawing(mode)`。
- 重新取景前若存在任意临时几何，继续显示确认提示；确认后统一取消绘图并清空。
- 切换检测模式时，MainWindow 通知模板页；模板页调用 `cancelTemplateDrawing()`、隐藏向导并清理二维码校验状态。
- Esc 清空当前模式的全部临时框线并回到该模式第一步，不影响已保存模板。
- 多边形少于三个点时不推进步骤。
- 工作流完成后不能因为遗留鼠标移动改变已闭合几何。
- 取消保存只保留或清理当前制作草稿，按现有交互决定，不得覆盖磁盘模板。

#### 坐标转换与保存输入

`InspectionImageCanvas` 只保存显示坐标，不得读取模板 Schema，也不得自行计算原图相对坐标。显示坐标到原图坐标、KeepAspectRatio 留白偏移、边界裁剪和中心相对坐标继续由现有应用层模板几何能力负责。

现有 `TemplateGeometryService::buildGeometry()` 只能接收一个定位矩形、一个二维码矩形和一个日期多边形，无法正确消费钢印的两个锚点和两个多边形。建议在现有 `template_geometry_service.h/.cpp` 中增加输入结构，不新增新 Service 或代码文件：

```cpp
struct TemplateDrawingInput {
    DetectionMode mode;
    QRect trackingAnchorRect;
    QRect barcodeRect;
    QRect stampAnchorRect;
    QPolygon datePolygon;
    QPolygon stampPolygon;
};
```

这是本流程唯一新增的绘图几何输入结构。`TemplateEditorPage` 在完成校验或保存时，从 `InspectionImageCanvas` 的五个只读 getter 读取当前显示坐标，连同当前 `DetectionMode` 组装一个局部 `TemplateDrawingInput`，再传给 `TemplateApplicationService/TemplateGeometryService`。该对象不作为 Page 或 `InspectionImageCanvas` 的长期状态保存。

固定调用关系为：

```text
InspectionImageCanvas 五个私有几何成员
    ↓ 只读 getter
TemplateEditorPage 组装唯一 TemplateDrawingInput
    ↓
TemplateGeometryService 转换并返回 TemplateGeometryResult
```

不得同时维护 `TemplateDrawingGeometry` 与 `TemplateDrawingInput`，也不得让几何服务反向读取 `InspectionImageCanvas`。

现有几何服务根据模式一次返回：

```text
trackingRoi
trackingImageRect
datePolygon
barcodePolygon
stampAnchorImageRect
stampPolygon
```

保存端固定关系：

- 字库/OCR：`trackingAnchorRect + datePolygon`。
- 二维码：`trackingAnchorRect + barcodeRect + datePolygon`。
- 刚印：`stampAnchorRect + stampPolygon + trackingAnchorRect + datePolygon`。
- `stampAnchorImageRect` 用于从原图裁剪现有 `stampRingTemplate`。
- `stampPolygon` 直接由独立的钢印 UI 多边形转换，不再从 `datePolygon` 推导。

最终仍填充现有 `TemplateSettings`、`InitialTemplateAssets` 和 `TemplateStore::save()`，不修改 `template_settings.json` Schema、资源文件名、检测 Pipeline 输入或运行结果语义。

#### 已保存几何的显示边界

本批次必须保证当前模板制作草稿按模式正确显示。若现有功能需要在重新打开模板时恢复框线，则由 `TemplateEditorPage` 将已保存原图坐标反向转换为显示坐标，再一次性传给 `InspectionImageCanvas`；反向转换仍放在现有模板几何服务中。

不得让 `InspectionImageCanvas` 直接读取模板目录或 `template_settings.json`。如果当前已验证功能并不包含“重新打开模板后恢复可编辑框线”，本批次不顺带增加该新功能，只保持已有模板图片和参数显示行为。

#### `TemplateEditorSupport` 收口

OpenCV 原生 UI 删除后，`template_editor_support.h/.cpp` 已经只剩页面私有小函数，初版将其合并到 `template_editor_page.cpp` 匿名命名空间并删除两个文件的方向正确，应继续保持：

- 不恢复 HighGUI 交互。
- 不恢复 Support 文件只为容纳一两个私有函数。
- 不为这些函数新增 Utility、Manager 或 Service。
- qmake 清单继续移除已删文件。

#### 明确不做

- 不新增第二个图像控件、StampEditorDialog 或每模式独立窗口。
- 不增加状态机框架、事件总线、GuideManager 或 DrawingManager。
- 不让 `InspectionImageCanvas` 持有向导控件或模板应用服务。
- 不通过原始鼠标事件在 Page 中重新实现绘图状态。
- 不修改模板 Schema、AppSettings Schema、模板路径、算法、阈值、统计、PLC 或存图合同。
- 不恢复 Recipe、旧模板兼容或 YAML 区域文件。
- 不把相机异常修复、其他 UI 新需求或资源修改混入批次 B 模板绘图提交。

## 9. 文件级修改清单

| 文件 | 计划修改 |
|---|---|
| `app/startup/application_startup.cpp` | 删除三个 Page 的外部创建与回挂，只创建应用服务和 MainWindow |
| `app/ui/main_window/main_window.h` | 删除组合公开接口、无调用转发和无效成员；增加 Page 私有所有权 |
| `app/ui/main_window/main_window.cpp` | MainWindow 内部创建 Page，建立必要跨页面连接；UI-4 删除 `InspectionImageCanvas` 到模板页的绘图信号中转 |
| `app/ui/main_window/main_window_settings.cpp` | 删除模板页一行转发和重复设置状态，保留真正跨页面协调 |
| `app/ui/main_window/main_window_inspection.cpp` | 删除模板页一行转发和死状态，保留检测/PLC/退出协调 |
| `app/ui/main_window/main_window.ui` | 保留主窗口骨架、主控区、右侧导航和五个页面根节点；固定内容分别位于五个页面 `.ui` |
| `app/ui/main_window/inspection/inspection_page.h/.cpp` | 收回检测页局部 UI 状态和按钮连接，不再接收 Startup 转发状态 |
| `app/ui/main_window/settings/machine_settings_page.h/.cpp` | 收回 applying/updating 和保存路径编辑状态，删除外部 bool*/QString* |
| `app/ui/main_window/template/template_editor_page.h/.cpp` | 直接连接模板按钮和 `InspectionImageCanvas` 强类型绘图事件；按五模式生成向导、执行二维码校验，并从只读 getter 组装唯一的 `TemplateDrawingInput` 完成保存 |
| `app/ui/pages/template_editor_support.h/.cpp` | 保持初版已完成的删除，不恢复 HighGUI 或只容纳页面私有小函数的 Support 文件 |
| `app/ui/main_window/operation_ui_policy.*` | 保持统一状态规则，只删除确认无用的 API，不拆分新文件 |
| `app/ui/main_window/settings/settings_edit_state.*` | 保持统一未应用状态，不复制回 MainWindow/Page |
| `app/ui/main_window/inspection/inspection_fault_presenter.*` | 保持故障呈现边界 |
| `app/ui/main_window/inspection_image_canvas.*` | 按 `DetectionMode` 切换五模式绘图步骤；使用五个私有几何成员、对应只读 getter、模式化绘制和强类型进度事件；不新增几何快照结构，不承担提示文案、模板保存和检测业务 |
| `app/application/template_geometry_service.*` | 在现有文件中定义并接收唯一的 `TemplateDrawingInput`，统一转换两个锚点、二维码区域和两个独立多边形，不新增 Service |
| `app/application/template_application_service.*` | 仅按现有门面转发新的几何输入/结果并继续填充现有模板草稿，不新增保存入口 |
| `app/resource/qss/app_theme.qss` | 只在对象迁移确需时同步选择器，不进行第二轮主题重写 |
| `app/AutoOCRproject.pro` | 同步删除文件和保持清单无重复 |
| `app/ui/README.md` | 更新最终所有权、调用流和维护规则 |

## 10. 行为保持清单

UI-1～UI-3 必须保持以下可观察行为完全不变：

- 五种检测模式名称和切换结果。
- 刚印、深度 OCR 单模板；字库、二维码+三期多模板；纸巾无模板。
- 模板选择预勾选、取消不保存、确认保存最终列表。
- 当前编辑模板、移除引用、模板异常显示。
- 模板新建、编辑、同名覆盖和完整目录替换语义。
- 目标文字、图像阈值、纸巾阈值和批量操作范围。
- 相机打开、关闭、预览、软硬触发和检测启停。
- PLC连接、模式、拍照/剔除参数和正常结果合同。
- 统计、耗时、合格率、当前模板名称和识别文字。
- 图像保存范围、内容、路径和质量。
- 未应用设置提示和放弃语义。
- Fault 锁定、现场安全提示和恢复确认。
- Tooltip 禁用原因覆盖与恢复。
- 所有正式视觉样式继续来自 `app_theme.qss`。

UI-4 允许把四种模板模式的绘图步骤统一纳入同一个模式化 `InspectionImageCanvas`，其中字库、OCR 和二维码保持已验证流程，钢印修复为两个锚点、两个独立多边形的四步流程。允许改变的是绘图载体、步骤状态和向导绑定；必须保持的是模板字段、坐标参考中心、资源文件、检测算法、阈值、统计、PLC、存图和结果语义。

钢印行为保持的准确含义不是保持当前错误三步实现，而是恢复检测算法一直需要的真实合同：

- 吸管口锚点只用于定位钢印区域。
- `stampPolygon` 只来自独立钢印多边形，并相对吸管口锚点中心保存。
- 日期锚点只用于定位生产日期区域。
- `datePolygon` 只来自独立日期多边形，并相对日期锚点中心保存。
- 日期区域继续执行字库匹配，并与钢印区域计算重叠。

## 11. 禁止的过度设计

实施中明确禁止：

- 新增 PageManager、UiManager、MainWindowBuilder、UiCompositionRoot。
- 新增事件总线、消息中心或通用 Command 分发框架。
- 为每个按钮建立 Command 类。
- 将三个 Page 再拆成大量一函数类或一函数文件。
- 将一个 `main_window.ui` 强制拆成多个 `.ui`，除非用户以后单独批准真正的多窗口/可复用页面需求。
- 将完整 `Ui::MainWindow*` 直接交给所有 Page。
- 使用 `findChild()` 字符串代替现有类型安全绑定。
- 为 Tooltip、QSS、状态属性新增 Manager 或配置文件。
- 为绘图增加 DrawingManager、GuideManager、每模式 Strategy 子类、事件总线或第二个状态机框架。
- 同时增加字段重复的 `TemplateDrawingGeometry` 和 `TemplateDrawingInput`，或增加其他只为搬运同一组几何数据的 UI 快照类型。
- 让 `InspectionImageCanvas` 直接持有模板向导控件、显示中文业务文案、弹出 `QMessageBox` 或调用模板应用服务。
- 把原始 `QMouseEvent*` 传给 `TemplateEditorPage` 后在 Page 中复制一套绘图状态判断。
- 使用字符串事件名继续维持 `InspectionImageCanvas` 与模板向导之间的隐式协议。
- 为减少行数删除 OperationUiPolicy、SettingsEditState 或 FaultPresenter 后复制逻辑。
- 顺带修改检测算法、阈值、模板 Schema、AppSettings Schema、PLC时序或存图合同。
- 删除图片、图标、旧主题 CSS/QSS、翻译、模型、DLL 或其他资源文件。

## 12. 风险与控制

| 风险 | 产生阶段 | 控制方式 |
|---|---|---|
| Page 销毁晚于 UI 控件 | UI-2 | MainWindow 析构先销毁 Page，再释放 `ui` |
| 自动槽删除后按钮无响应 | UI-3 | 每个删除槽都检查对应显式 `connect()`，建立静态槽映射表 |
| 初始化设置触发模式切换 | UI-2 | 使用 `QSignalBlocker`，不依赖共享 applying 指针 |
| 图片保存路径出现两个正式值 | UI-1/UI-2 | 只保留 AppSettings 草稿，控件显示从该值刷新 |
| 固定控件迁入 `.ui` 后布局变化 | UI-3 | 保持对象名、Layout stretch、minimumSize 和 sizePolicy，逐模式截图核对 |
| 钢印两个多边形再次被混用 | UI-4 | `InspectionImageCanvas`、几何输入和保存结果都使用独立 `stampPolygon/datePolygon` 字段，禁止从其中一个推导另一个 |
| 钢印两个锚点中心混淆 | UI-4 | `stampPolygon` 只减吸管口锚点中心，`datePolygon` 只减日期锚点中心，逐字段对照模板 JSON 和检测消费端 |
| 显示坐标转换变化 | UI-4 | 冻结原图尺寸、控件尺寸、KeepAspectRatio 实际图像尺寸、留白偏移和边界裁剪样本，统一由现有几何服务转换 |
| 模式切换遗留旧框线 | UI-4 | 每次进入新模式先清空全部临时几何；保存前按当前模式验证必需字段和非适用字段为空 |
| 模板向导与绘图步骤不同步 | UI-4 | 只消费强类型步骤事件；初始、失败、Esc、二维码重试和工作流完成都由同一状态变化驱动 |
| 向导连接重新绕过 MainWindow | UI-4 | `TemplateEditorPage` 直接连接 `InspectionImageCanvas`，MainWindow 只通知模式切换，不中转绘图事件 |
| Qt 主线程仍被阻塞 | UI-4 | 生产 UI 中 `cv::waitKey/namedWindow/imshow` 零引用 |
| 精简时误删状态防线 | 全阶段 | OperationUiPolicy 与应用服务底层命令校验均保留 |
| 当前未提交 UI 样式差异混入 | UI-0 | 先验证和提交当前 UI，再建立新阶段基线 |

## 13. 静态门禁

### 13.1 UI-1～UI-3 终局零引用

- `ForComposition` 为零。
- `attachPages` 为零。
- Startup 中三个 Page 类型为零。
- Startup 中 ViewBindings 和 PageCallbacks 为零。
- `selectedDir` 平行设置状态为零。
- 确认无用的 MainWindow 一行转发为零。
- 已删除辅助函数和成员为零。
- C++ 生产代码中的完整控件 `setStyleSheet()` 为零；正式 `qApp->setStyleSheet()` 加载入口除外。

### 13.2 UI-4 终局零引用

- UI 交互中的 `cv::namedWindow` 为零。
- UI 交互中的 `cv::imshow` 为零。
- UI 交互中的 `cv::waitKey` 为零。
- UI 交互中的 `cv::setMouseCallback` 为零。
- `template_editor_support.h/.cpp` 和 qmake 条目保持为零。
- 旧 `SecondaryRegionMode`、`STEP_SECONDARY_RECT` 和只能表示一个多边形的旧状态为零。
- `signal_templateGuideEvent(QString, int)` 及 `"tracking_done"/"poly_done"` 等字符串绘图协议为零。
- MainWindow 对 `InspectionImageCanvas` 模板绘图事件的中转连接为零。
- 无消费者的 `mousePressed/mouseMoved/mouseReleased(QMouseEvent *)` 信号为零。
- `TemplateDrawingGeometry` 及其他重复 UI 几何快照类型为零；应用层只保留唯一的 `TemplateDrawingInput`。
- 保存代码中使用 `datePolygon` 推导 `stampPolygon` 的路径为零。
- 钢印完成条件必须同时检查两个有效锚点和两个已闭合多边形。
- 字库/OCR、二维码和钢印分别只访问本模式允许的几何字段。
- 纸巾模式绘图步骤、临时几何和模板向导全部为空/禁用。

### 13.3 共同检查

- UI 不直接包含设备、Runtime、Detection 或 PLC 实现头文件。
- Runtime/Detection/Devices 不依赖 UI。
- `.ui` 对象名、C++绑定、自动槽/显式连接和 QSS 选择器同步。
- qmake 文件存在性和重复项检查通过。
- UTF-8 中文、文件末尾换行和 `git diff --check` 通过。
- 不出现计划外资源文件差异。
- 不用静态检查宣称构建和人工操作已经通过。

## 14. 用户统一验证清单

### 14.1 UI-1～UI-3 验证

1. Run qmake、Clean、Rebuild。
2. 启动、单实例、主窗口最大化和正常退出。
3. 五种模式逐一切换，检查模板区和参数区显示/隐藏。
4. 模板选择对话框预勾选、增加、取消、确认和排序。
5. 当前编辑模板切换、异常模板显示和移除引用。
6. 目标文字、图像阈值、纸巾阈值及多模板批量操作。
7. 模板制作向导位置、文字、步骤和 Esc 重置。
8. 相机打开/关闭、模板取景、检测启动/停止。
9. 运行中全部设置禁用原因和重新启用后的 Tooltip 恢复。
10. 整机设置修改、未应用提示、应用、恢复默认和重启恢复。
11. 图片保存路径、范围、内容和输出。
12. 统计清零、NG清零、剔除队列复位。
13. PLC连接、断开、运行参数和现场合同。
14. Fault 显示、现场安全提示和恢复确认。

### 14.2 UI-4 额外验证

1. 逐一切换五种模式，确认旧模式框线不会残留到新模式。
2. 字库模式严格按“定位区域 → 文字检测多边形”两步完成，提示、点数和保存均正确。
3. 深度 OCR 严格按“定位区域 → OCR 检测多边形”两步完成，不显示二维码或钢印区域。
4. 二维码＋三期严格按“日期定位锚点 → 二维码矩形 → 日期多边形”三步完成。
5. 二维码矩形验证成功后进入日期步骤；验证失败时保留定位锚点并只重试二维码区域。
6. 钢印严格按“吸管口锚点 → 钢印多边形 → 日期锚点 → 日期多边形”四步完成。
7. 钢印第一步完成后必须进入钢印多边形，而不是日期锚点或通用第二矩形。
8. 钢印多边形闭合后才进入日期锚点；日期锚点完成后才进入日期多边形。
9. 两个矩形反向拖动都得到合法归一化矩形；过小矩形留在当前步骤并显示对应区域提示。
10. 任一多边形少于三个点时不能闭合，提示当前点数；两个多边形的点数和预览线互不串用。
11. 钢印保存后的 `stampRingTemplate` 确实来自吸管口锚点裁剪图。
12. 保存后的 `stampPolygon` 与用户点击的钢印区域一致，并相对吸管口锚点中心。
13. 保存后的 `trackingTemplate/trackingRoi` 确实来自日期定位锚点。
14. 保存后的 `datePolygon` 与用户点击的日期区域一致，并相对日期锚点中心。
15. 同一钢印模板中的 `stampPolygon` 和 `datePolygon` 不相同，也不再由同一 UI 多边形生成。
16. 实际运行钢印检测，确认吸管口定位钢印区域、日期锚点定位日期区域、日期字库匹配和重叠判断均正确。
17. 纸巾模式不进入模板绘图，不显示框线和模板制作向导。
18. 每个模式的模板向导标题、总步骤数、当前区域、点数、太小/点数不足、Esc 和完成提示与实际状态完全一致。
19. Esc 在四种模板模式中都清空本模式全部临时框线并回到第一步，不影响磁盘模板。
20. 切换模式、重新取景、停止模板预览和关闭窗口均能安全取消绘图状态。
21. 取消保存不覆盖已保存模板；保存失败显示明确错误且不留下半成品目录。
22. 模板制作期间 Qt 主窗口持续响应，不出现 OpenCV 子窗口。
23. 字库、OCR、二维码和钢印的模板 Schema、阈值、Overlay、统计和运行结果除修复钢印错误外保持不变。

## 15. 实施与提交策略

推荐采用两个用户验证批次：

### 批次 A：纯架构精简

一次性完成 UI-1、UI-2、UI-3，Agent 完成全部静态门禁后，由用户统一构建和人工验证。通过后创建一个本地提交。

### 批次 B：`InspectionImageCanvas` 模式化绘图与向导绑定

单独完成修订后的 UI-4。字库、OCR 和二维码流程只做状态显式化与强类型向导绑定，不改变既有步骤；钢印修复为两个锚点、两个独立多边形的四步流程。批次必须使用真实刚印模板和检测样本验证，通过后才能创建第二个本地提交。

当前批次 B 初版验证失败，因此：

- 继续在同一批次中修复，不提前提交错误三步实现。
- 代码实现前先以本文修订内容替换旧三步方案。
- 当前工作区中的相机异常修复属于另一条故障修复链，最终暂存和提交时必须与模板绘图差异精确隔离。
- 本文计划文档本身不混入只含生产代码的批次 B 提交，除非用户明确要求文档随批次提交。

这样既避免让用户为每个小步骤重复构建，又不会把高风险的刚印交互变化与纯删除重构混成一个无法定位问题的大提交。

## 16. 预期结果

完成 UI-1～UI-3 后：

- Startup 不再了解 UI 内部 Page、控件和状态。
- MainWindow 不再公开内部状态地址。
- Page 创建与控件绑定集中在 MainWindow 内部。
- 大量一行转发、无调用接口和无意义注释被删除。
- Qt Designer 能看到主要固定布局。
- 新增普通控件不再修改 Startup。
- 保留三个 Page、统一状态、统一 QSS 和应用服务边界。

完成 UI-4 后：

- 模板制作只使用一套 Qt UI。
- 同一个 `InspectionImageCanvas` 按检测模式执行两步、三步、四步或禁用绘图，不再用一个通用多边形表达不同业务区域。
- 刚印框选不阻塞 Qt 事件循环，并正确保存吸管口锚点/钢印多边形和日期锚点/日期多边形。
- 模板制作向导直接绑定 `InspectionImageCanvas` 的强类型绘图状态，不获取原始鼠标事件，也不经过 MainWindow 中转。
- `TemplateEditorPage` 继续统一拥有向导文案、二维码校验和保存确认。
- `InspectionImageCanvas` 只通过五个私有几何成员和只读 getter 管理模板制作期间的 UI 坐标、交互状态和框线显示，不拥有 `TemplateDrawingInput`、模板磁盘、应用服务或检测业务。
- `TemplateEditorPage` 只在校验或保存时组装唯一的应用层 `TemplateDrawingInput`，不维护重复的 UI 几何快照结构。
- `template_editor_support` 保持删除，HighGUI 交互零引用。
- 模板 Schema、坐标参考中心、检测算法、阈值、统计、PLC 和存图合同保持阶段 8 终局。

最终目标不是追求最少类或最少行，而是让一次普通 UI 修改只需要检查：

```text
main_window.ui（布局）
    ↓
对应 Page（行为）
    ↓
app_theme.qss（视觉）
    ↓
ApplicationService（需要业务操作时）
```

不再经过 Startup、MainWindow 公开绑定、状态地址、Page Callback 和 MainWindow 一行转发的完整绕行。
