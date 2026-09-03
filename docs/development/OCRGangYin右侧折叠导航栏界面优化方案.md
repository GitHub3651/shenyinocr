# OCRGangYin 右侧折叠导航栏界面优化方案

## 1. 文档状态

- 编写日期：2026-09-01。
- 最后更新：2026-09-02。
- 最新决定：用户确认采用“右侧窄导航栏＋点击展开内容抽屉＋图像区顶部唯一显著判定栏”的推荐方案；固定可见控件和布局全部由 `main_window.ui` 静态创建，C++ 只连接信号并更新运行状态；软件启动时由 `.ui` 静态默认展开检测信息页并选中“检测信息”，检测启动成功后仍按既有边界自动收起；四个旧 `tab_*` 页面容器直接重命名为 `page_*` 并删除 Tab 标题元数据；检测信息页与参数设定页采用相同的直接内容结构，删除抽屉标题下重复的“检测信息”外层分组及其两层包装布局；抽屉页面和开合状态直接读取现有控件，不新增重复状态字段；抽屉标题直接读取导航按钮文字，不传递或保存第二份标题；两个现有 Fault 呈现入口都复用 `m_faultAlarmPresented` 的首次呈现边界；对 `.ui` 固定控件和新增抽屉方法不增加空指针防护、回退查找或备用路径；实施前 AppSettings Schema 4 中的旧 Splitter 持久化字段和 JSON 键已直接删除并将 Schema 更新为 5，不设置兼容层、迁移层、双写、废弃别名或其他胶水代码，同时完整保留 `resultExportEnabled` 和 `resultExport.enabled` 合同；导航图标直接使用用户指定的五个 SVG，并已统一重命名为 `nav_*.svg`。
- 当前状态：代码已实施，待用户验证。
- 当前进度：右侧导航、抽屉、五页静态结构、显著判定栏、启动/Fault 开合、SVG 资源、正式 QSS 和 Schema 5 已落地；启动初始状态已在 `.ui` 中设为展开检测信息页并选中“检测信息”；检测信息页的重复外层分组和两层包装布局已删除，五块现有内容由滚动内容页的唯一网格布局直接承载；旧 Splitter、旧 Tab、旧判定包装、动态界面装配及布局持久化链已端到端删除，当前 Schema 5 的 `templateSaveDirectory` 与 `resultExport` 字段均严格必填；生产统计布局使用 Qt Designer 支持的 `columnstretch="1,1"` 属性静态声明两列等宽伸缩；Agent 静态门禁通过，需由用户重新执行 Run qmake、构建、主程序和人工交互验证。
- 权威范围：主窗口右侧检测信息与四类设置页的折叠导航、展开抽屉、运行时自动收起、Fault 自动打开检测信息、图像区显著判定栏、配套图标资源，以及随旧布局一起删除的 Splitter 持久化字段。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `1e381854ecb0ae81c1bd44467edd35b808baa56b`；Schema 4 结果传输启用策略已作为受保护基线提交，本批次未覆盖或回退其字段和调用链。
- 替代关系：本文不替代 `OCRGangYinUI架构精简优化方案.md`、统一 QSS 基线、`OCRGangYin二维码结果传输启用策略持久化与连接解耦方案.md` 或其他功能计划；仅在右侧布局、导航方式、判定栏位置和旧 Splitter 状态删除上采用用户本次更新决定。其他 UI 所有权、模板绘图、状态策略、Schema 4 结果传输启用政策和业务合同继续服从现有有效计划。

## 2. 结论

当前 `widget_rightPanel` 同时显示“检测信息”和包含四个标签页的设置区，最小宽度为 500，长期占用主图像空间。目标布局固定为：

```text
收起状态
┌──────────────────────────────────────────────┬──────────┐
│ 主控操作台＋显著判定栏＋图像显示区           │ 右侧导航栏 │
└──────────────────────────────────────────────┴──────────┘

展开状态
┌───────────────────────────┬──────────────────┬──────────┐
│ 主控操作台＋判定栏＋图像区 │ 当前展开内容抽屉 │ 右侧导航栏 │
└───────────────────────────┴──────────────────┴──────────┘
```

右侧导航栏始终可见；软件启动时默认展开检测信息页，检测启动成功后自动收起。导航栏提供五个入口：

1. 检测信息。
2. 参数设定。
3. 图像设置。
4. PLC通讯。
5. 软件设置。

抽屉一次只显示一个现有页面。展开时压缩主图像区域，不覆盖图像；收起时释放原右侧面板宽度。现有 `label_verdictResult` 移出抽屉，作为图像区顶部唯一、持续可见的产品判定栏。

## 3. 实施基线事实

### 3.1 实施前右侧结构

`app/ui/main_window.ui` 实施前结构为：

```text
widget_rightPanel（minimumWidth = 500）
└─ splitter_mainContent（垂直）
   ├─ 检测信息外层分组
   │  ├─ label_runtimeStatus
   │  ├─ label_verdictResult
   │  ├─ label_recognitionText
   │  ├─ lineEdit_totalCount
   │  ├─ lineEdit_ngCount
   │  ├─ lineEdit_detectionDuration
   │  ├─ lineEdit_passRate
   │  ├─ lineEdit_currentTemplateName
   │  └─ pushButton_resetRejectQueue
   └─ tabWidget_settings
      ├─ tab_detectionSettings
      ├─ tab_imageSettings
      ├─ tab_plcSettings
      └─ tab_softwareSettings
```

`MainWindow` 在 C++ 中为检测信息创建滚动区和可拖动的纵向 Splitter Handle；`MachineSettingsPage` 恢复已有 `rightPanelSplitterState`，窗口关闭时又保存该状态。

### 3.2 当前结果显示链

`InspectionPage::present()` 已经通过同一个 `label_verdictResult` 显示正式产品的“正确/错误”，并设置现有 `verdict=ok/ng` 属性。Fault 继续由 `InspectionFaultPresenter` 和现有故障弹窗/判定样式呈现。

因此本方案只移动现有判定控件并加强样式，不新增第二套判定数据、结果回调或业务状态。

### 3.3 实施前图标资源

用户在 `app/resource/svg` 提供并指定五个同系列 SVG：`scan-cube.svg`、`adjustments-horizontal.svg`、`photo-cog.svg`、`plug-connected.svg` 和 `settings.svg`。实施前这些文件尚未登记到 `image.qrc`，`app/AutoOCRproject.pro` 也尚未声明 Qt SVG 模块。

本方案已直接使用这五个 SVG，不生成 PNG 副本；资源已按固定映射重命名、登记到 Qt 资源，并增加项目实际需要的 Qt SVG 模块。

### 3.4 实施前 AppSettings 与结果传输基线

当前工作区已经按 `OCRGangYin二维码结果传输启用策略持久化与连接解耦方案.md` 完成以下生产代码修改：

- `AppSettings::CurrentSchemaVersion` 已为 4。
- `AppSettings::resultExportEnabled` 是唯一持久化启用政策，默认值为 `false`。
- JSON 的 `resultExport.enabled` 是 Schema 4 必填布尔字段，继续与 `receiverIp/receiverPort` 同属现有 `resultExport` 分区。
- `AppSettingsStore` 的 `readBool()`、严格允许键和读写链已实际使用。
- `MainWindow` 直接从 AppSettings 恢复和保存 `resultExportEnable`，不再维护 `m_resultExportUserEnabled` 或 `m_resultExportAutoEnableApplied`。
- `InspectionApplicationService::start()` 直接读取 AppSettings 执行二维码+三期结果传输门禁，`StartInspectionCommand` 不再携带临时启用字段。

上述内容是本方案必须保护的当前基线，不属于待删除的旧右侧布局代码。本方案只删除 `rightPanelSplitterState/rightPanelSplitterStateBase64`，不得删除、改名、重置、转发或重新包装任何结果传输字段和调用链。

当前 Schema 4 设置文件仍固定包含 `ui.rightPanelSplitterStateBase64`。删除该严格字段会改变 JSON 合同，因此本方案最终将 AppSettings 从 Schema 4 直接更新为 Schema 5；Schema 4 继续复用现有版本不匹配重置流程，不增加 Schema 4 到 5 的迁移、兼容读取或缺字段回退。

## 4. 目标与非目标

### 4.1 目标

- 软件启动时直接呈现检测信息；检测启动成功后自动收起抽屉以释放图像显示空间。
- 用固定窄边栏统一承载五类信息和设置入口。
- 点击后显示完整现有内容，不删除字段和操作按钮。
- 运行时只持续突出一个产品判定：“正确”或“错误”。
- 保留完整检测信息的按需查看能力。
- 保持设置禁用原因、未应用 `*`、成功提交和失败回退行为。
- 保持本方案全部固定可见控件、页面层级和布局由 `main_window.ui` 静态创建，C++ 不动态拼装界面；视觉只由正式 `app_theme.qss` 表达。
- 将已无用途的 Splitter UI、状态字段、JSON 键和读写代码一次删除干净。

### 4.2 非目标

- 不修改检测算法、判定规则、阈值含义或五种检测模式。
- 不修改 Runtime、Detection、Camera、PLC Controller、设备 SDK 或线程时序。
- 不修改模板格式、模板路径、模板保存和模板绘图流程。
- 除删除旧 Splitter 状态并将 AppSettings Schema 版本从 4 更新为 5 外，不调整其他设置字段或设置语义。
- 不修改 Schema 4 已落地的 `resultExportEnabled`、`resultExport.enabled`、接收端 IP/端口、严格布尔读取、复选框恢复/保存、二维码+三期启动门禁或每轮运行配置。
- 不为旧开发配置增加迁移、兼容读取、双写、默认填充或废弃字段保留；旧配置直接重新生成。
- 不修改统计、存图、二维码结果传输、日志或 Fault 业务合同。
- 不新增窗口、页面文件、事件总线、导航管理器、主题管理器或通用 UI 框架。
- 不顺带重画主控操作台、重排页面字段或统一历史资源。

## 5. 最终布局

### 5.1 主窗口横向结构

主窗口固定为三个相邻区域：

```text
widget_leftPanel
widget_rightPanel（抽屉内容，按需显示）
widget_rightNavigationRail（始终显示）
```

- `widget_leftPanel`：继续包含主控操作台、模板向导、显著判定栏和唯一 `ImageLabel`。
- `widget_rightPanel`：复用现有对象名，改为可整体显示/隐藏的内容抽屉。
- `widget_rightNavigationRail`：新增固定右侧导航栏，只负责页面选择和抽屉开合。

### 5.2 尺寸

- 右侧导航栏固定宽度：80 个 Qt 逻辑像素。
- 单个导航入口最小高度：68 个 Qt 逻辑像素。
- 图标显示尺寸：28×28。
- 图标下方中文名称字号：13px；项目正式全局字体保持不变。
- 抽屉最小宽度：500。
- 抽屉建议宽度：540。
- 抽屉最大宽度：580。
- 检测信息页四个生产统计分组删除当前 285px 最小宽度，`minimumWidth` 设为 0，横向尺寸策略使用 `Expanding`，两列拉伸比固定为 1:1；不改变现有两行两列顺序。
- 抽屉不允许覆盖 `ImageLabel`，不新增浮层和遮罩。
- 本轮不提供用户拖动改变抽屉宽度，也不保存抽屉开合或宽度。

### 5.3 抽屉内部结构

`widget_rightPanel` 内部固定为：

```text
frame_rightDrawerHeader
├─ label_rightDrawerTitle
└─ toolButton_collapseRightDrawer

stackedWidget_rightDrawer
├─ page_inspectionInfo
│  └─ scrollArea_inspectionInfo
│     └─ scrollAreaWidgetContents_inspectionInfo
│        └─ gridLayout_inspectionInfoContent
│           ├─ groupBox_runtimeStatus
│           ├─ groupBox_recognitionText
│           ├─ groupBox_productionStatistics
│           ├─ groupBox_currentTemplate
│           └─ pushButton_resetRejectQueue
├─ page_detectionSettings
├─ page_imageSettings
├─ page_plcSettings
└─ page_softwareSettings
```

实施时必须在 `main_window.ui` 中建立固定 `QStackedWidget`，把现有检测信息和四个设置页原位迁入。不得复制页面内容，也不得创建第二套同名控件。

`scrollArea_inspectionInfo` 改为 `page_inspectionInfo` 内由 `.ui` 固定声明的滚动区，继续保证低分辨率下完整检测信息可访问。滚动内容直接以唯一 `gridLayout_inspectionInfoContent` 承载五块现有内容，不再设置与抽屉标题重复的外层分组，也不保留仅用于该分组的包装布局；同时删除当前 C++ 中对该滚动区的动态创建、装配和插入代码。

原 `tabWidget_settings` 本体及其可见标签栏、原纵向 Splitter Handle 都由新结构替代并直接删除，不得隐藏后作为中间容器继续保留。四个页面容器直接归属 `stackedWidget_rightDrawer`，并按以下固定映射去除旧 Tab 命名：

| 当前对象名 | 最终对象名 |
|---|---|
| `tab_detectionSettings` | `page_detectionSettings` |
| `tab_imageSettings` | `page_imageSettings` |
| `tab_plcSettings` | `page_plcSettings` |
| `tab_softwareSettings` | `page_softwareSettings` |

四个页面在旧 `QTabWidget` 下使用的 `<attribute name="title">` 同时删除；页面内部业务控件对象名保持不变。

### 5.4 静态 UI 所有权

本方案主窗口中的固定可见界面必须全部在 `main_window.ui` 中创建并形成最终父子层级，包括：

- `widget_rightNavigationRail` 及其布局。
- 五个固定对象名的导航 `QToolButton`。
- 复用的 `widget_rightPanel` 及其布局。
- `frame_rightDrawerHeader`、`label_rightDrawerTitle` 和 `toolButton_collapseRightDrawer`。
- `stackedWidget_rightDrawer`、`page_inspectionInfo` 和 `scrollArea_inspectionInfo`。
- 直接迁入 `stackedWidget_rightDrawer` 并重命名后的四个 `page_*` 页面。
- 移动到图像区顶部的唯一 `label_verdictResult`。

以下固定属性同样由 `.ui` 表达：

- 父子层级、布局类型、布局顺序、边距、间距、拉伸比例和尺寸约束。
- 五个导航入口的文字、Tooltip、QRC 图标、28×28 图标尺寸、`ToolButtonTextUnderIcon` 和 `checkable=true`。
- “检测信息”入口初始选中，其余四个导航入口初始未选中。
- `stackedWidget_rightDrawer` 默认页为 `page_inspectionInfo`。
- `widget_rightPanel` 初始可见，程序启动即显示检测信息页。

`main_window.cpp` 和 `main_window_inspection.cpp` 只允许：

- 通过 `ui->固定对象名` 直接连接信号。
- 使用 `setVisible()`、`setCurrentWidget()`、`setChecked()` 和标题文本更新现有静态对象。
- 在启动成功和首次 Fault 的既有调用边界更新抽屉，不维护额外运行状态边沿。
- 更新运行状态、判定、识别内容和其他运行数据。

本方案不得在 C++ 中：

- `new` 任何导航栏、抽屉、标题栏、导航按钮、页面、滚动区、判定栏或它们的布局。
- 对本方案固定界面调用 `addWidget()`、`insertWidget()`、`setWidget()`、`setLayout()`、`setParent()` 或运行时重建父子关系。
- 使用 `findChild()`、`findChildren()` 按对象名查找本方案控件；必须直接使用 `ui->` 成员。
- 为本方案控件增加缓存指针、重复成员别名、动态 `QButtonGroup`、导航管理器或抽屉控制器。
- 为 `main_window.ui` 固定创建且由 `ui->` 直接访问的本方案控件增加空指针判断、回退查找、备用控件或静默返回；`showRightDrawerPage()`、`collapseRightDrawer()` 和 `InspectionPage::showWaitingResult()` 均直接操作各自固定控件。

现有 `QTimer`、输入校验器以及 `InspectionPage/MachineSettingsPage/TemplateEditorPage` 页面逻辑对象不是固定可见布局，继续由 C++ 管理；不得为了形式上的“全部放入 `.ui`”改变这些既有职责。

## 6. 右侧导航栏

### 6.1 固定入口与顺序

| 顺序 | 固定控件对象名 | 显示文字 | 当前 SVG | 实施后资源名 | 对应页面 |
|---:|---|---|---|---|---|
| 1 | `toolButton_showInspectionInfo` | 检测信息 | `app/resource/svg/scan-cube.svg` | `app/resource/svg/nav_inspection.svg` | `page_inspectionInfo` |
| 2 | `toolButton_showDetectionSettings` | 参数设定 | `app/resource/svg/adjustments-horizontal.svg` | `app/resource/svg/nav_parameters.svg` | `page_detectionSettings` |
| 3 | `toolButton_showImageSettings` | 图像设置 | `app/resource/svg/photo-cog.svg` | `app/resource/svg/nav_image.svg` | `page_imageSettings` |
| 4 | `toolButton_showPlcSettings` | PLC通讯 | `app/resource/svg/plug-connected.svg` | `app/resource/svg/nav_plc.svg` | `page_plcSettings` |
| 5 | `toolButton_showSoftwareSettings` | 软件设置 | `app/resource/svg/settings.svg` | `app/resource/svg/nav_software.svg` | `page_softwareSettings` |

每个入口使用 `QToolButton`，采用“图标在上、文字在下”。不能只显示图标；Tooltip 继续使用完整中文名称。

### 6.2 导航状态

- 抽屉收起：五个入口全部为未选中状态。
- 抽屉展开：只有当前页面入口为选中状态。
- 选中视觉：浅蓝背景、蓝色文字、靠抽屉一侧的蓝色指示条。
- Hover：比选中状态更弱的浅蓝背景。
- 导航入口在检测运行、停止和 Fault 状态下仍可点击。
- 页面内部按钮和编辑控件继续由现有 `OperationUiPolicy` 决定是否可用；导航栏不复制权限判断。

## 7. 交互合同

### 7.1 手动操作

1. 点击未选中的入口：显示抽屉并切换到该页面。
2. 抽屉已展开时点击其他入口：直接切换页面，抽屉保持展开。
3. 再次点击当前入口：收起抽屉并清除导航选中状态。
4. 点击抽屉标题栏收起按钮：收起抽屉并清除选中状态。
5. 一次只能显示一个页面。
6. 不使用 Hover 自动展开，不点击图像空白处自动收起，不增加手势。
7. 不做滑入滑出动画，避免布局抖动和额外状态。

切换或收起页面只改变 `visible/currentIndex/checked` 等 UI 状态，不得：

- 应用、保存或放弃参数。
- 清除未应用 `*`。
- 重新读取正式设置覆盖草稿。
- 启停相机、检测、PLC 或模板预览。
- 清空统计、结果、图像或模板制作几何。

### 7.2 启动与运行

- 程序启动完成后默认展开 `page_inspectionInfo` 并选中“检测信息”，不恢复上次打开页面。
- 点击“启动识别”但启动被取消或失败：保持原抽屉状态。
- `StartInspectionResult::isAccepted()` 成功后立即收起抽屉并显示“等待结果”；不等待后续普通状态刷新，也不保存上一 `OperationUiState`。
- 运行中用户仍可手动打开任意页面查看；后续普通结果或状态刷新不得再次强制收起。
- 设置页面运行中可打开，但其内部控件继续按现有规则禁用并显示既有禁用原因。
- 进入 `Stopping`：保持用户当时的抽屉状态。
- 正常停止完成：不自动打开抽屉。

### 7.3 Fault

- 当前生产代码存在两个 Fault 呈现入口：`main_window.cpp` 中 `InspectionRuntime::faultSnapshotChanged` 的现有回调，以及 `main_window_inspection.cpp` 中现有 `MainWindow::presentInspectionFault()`。两个入口都必须在调用 `InspectionPage::presentFault()` 之前检查 `m_faultAlarmPresented`；值为 `false` 时先自动打开“检测信息”页面，再沿用各自现有 Fault 呈现流程。
- 两个入口共同复用现有 `m_faultAlarmPresented`，不新增第二个 Fault 标记、Fault 转发方法、重载或中转层；不得只修改其中一个入口。
- 现有 Fault 弹窗、现场安全提示、恢复确认和锁定逻辑保持不变。
- 用户看过故障后仍可手动收起抽屉；同一次 Fault 的普通状态刷新不得反复弹开。
- Fault 解除后不自动切换到其他页面。

自动收起直接使用现有启动成功返回边界，Fault 自动打开复用现有 `m_faultAlarmPresented` 首次呈现边界。不得新增上一 `OperationUiState`、Fault 抽屉标记，也不得把抽屉状态写入 Runtime、ApplicationService 或 `OperationUiPolicy`。

## 8. 显著判定栏

### 8.1 位置与唯一性

- 保留现有唯一 `label_verdictResult` 对象名和现有 ViewBindings。
- 将它从旧检测信息容器移到 `groupBox_imageDisplay` 内，位于模板向导之后、`imageLabel_inspection` 之前。
- 删除只用于包裹它的 `groupBox_verdictResult`，不新增第二个产品判定标签。
- 判定栏横向占满图像显示区，最小高度 72，文字居中。

### 8.2 文案和状态

| 状态 | 文案 | 视觉 |
|---|---|---|
| 未开始检测 | `等待检测` | 中性灰 |
| 新一轮检测已开始、首件结果未到 | `等待结果` | 中性灰 |
| 正常 OK | `正确` | 绿色实底＋白字 |
| 正常 NG | `错误` | 红色实底＋白字 |
| Fault | `系统故障：检测已暂停` 或现有 FaultPresenter 文案 | 深红警示样式 |

- 产品判定必须同时用中文文字和颜色表达，不能只依赖颜色。
- 新结果替换旧结果，结果之间不闪烁、不自动清空。
- 新一轮检测正式开始时显示“等待结果”；启动取消或失败不得覆盖旧判定。
- 停止检测后保留最后一件产品判定，直到新一轮检测开始、相机关闭或现有清理流程明确清除。
- `InspectionPage::showWaitingResult()`、`present()`、`presentFault()` 和现有 `verdict=idle/ok/ng/fault` 属性作为唯一判定栏更新边界；`MainWindow` 不直接拼写判定文案或设置判定样式。

### 8.3 样式

建议在正式 QSS 中调整：

- `idle`：背景 `#eef1f6`，文字 `#606266`。
- `ok`：背景 `#2e9b51`，文字白色，边框同背景。
- `ng`：背景 `#d93025`，文字白色，边框同背景。
- `fault`：沿用深红故障语义，字号可小于产品判定以容纳完整故障文案。
- 产品判定字号建议 48px，字重使用现有正式 QSS 能稳定支持的高字重。

不在 C++ 或 `.ui` 中写完整内联 QSS。

## 9. 页面内容保持

### 9.1 检测信息

除 `label_verdictResult` 移至图像区外，以下内容继续位于“检测信息”抽屉页：

- 运行状态。
- 识别内容。
- 识别总数和清零。
- 不合格数和清零。
- 单帧完整处理耗时。
- 合格率。
- 当前使用产品模板文件夹名称。
- 剔除复位。

`page_inspectionInfo` 使用 `main_window.ui` 中固定存在的 `scrollArea_inspectionInfo`，其内容页通过唯一 `gridLayout_inspectionInfoContent` 直接承载运行状态、识别内容、生产统计、当前模板和剔除复位按钮。抽屉标题下不再嵌套第二个“检测信息”分组，不保留旧外层分组或包装布局，也不在 C++ 中再次创建、替换父对象或插入滚动区。滚动区横向滚动条固定关闭；四个生产统计分组取消 285px 最小宽度并在两列中等宽伸缩，保证 500px 抽屉下不裁剪。

### 9.2 四个设置页

四个现有页面内部结构、顺序、业务控件名、显示条件和操作语义全部保持，只清理页面容器的旧 Tab 命名和标题元数据：

- `page_detectionSettings`：参数设定。
- `page_imageSettings`：图像设置。
- `page_plcSettings`：PLC通讯。
- `page_softwareSettings`：软件设置。

二维码模式的结果传输区域、纸巾阈值可见性、单/多模板差异和未应用标记继续由现有页面逻辑控制，导航层不认识这些业务差异。

`page_detectionSettings` 内现有 `resultExportEnable`、`resultExportIp`、`resultExportPort`、连接/断开按钮和连接状态标签只随页面容器迁移，内部对象名、信号槽、AppSettings 提交和运行中禁用政策全部保持当前实现。

## 10. UI 局部状态设计

不新增抽屉页面枚举、当前页面字段、开合布尔值或上一 `OperationUiState`。现有控件是唯一状态来源：

- `widget_rightPanel->isVisible()` 表示抽屉是否展开。
- `stackedWidget_rightDrawer->currentWidget()` 表示当前页面。
- 五个导航按钮的 `checked` 状态由显示和收起方法同步，不单独缓存。
- 抽屉标题根据目标页面在显示时直接设置，不保存第二份标题状态。

只增加两个私有方法：

```text
showRightDrawerPage(QWidget *page,
                    QToolButton *button)
collapseRightDrawer()
```

导航按钮由 Qt 的 `checkable` 状态直接表达本次点击意图：目标按钮在点击后为未选中时，`showRightDrawerPage()` 调用 `collapseRightDrawer()`；为选中时，切换页面、显示抽屉并同步五个按钮。`showRightDrawerPage()` 直接以 `button->text()` 更新 `label_rightDrawerTitle`，不接收标题参数，不在连接代码中重复五份标题字符串。启动成功后直接调用 `collapseRightDrawer()`；两个现有 Fault 呈现入口均在调用 `InspectionPage::presentFault()` 前使用现有 `m_faultAlarmPresented` 判断，将检测信息按钮置为选中并显示 `page_inspectionInfo`。

`page`、`button`、抽屉控件和 `showWaitingResult()` 使用的判定标签均为 `.ui` 固定控件，不增加空指针判断、回退查找、备用路径或静默返回。不得新增 `NavigationManager`、`DrawerController`、Fault 转发方法或重载、状态机框架、独立代码文件、抽屉状态成员、运行状态副本或页面对象缓存。

## 11. 旧 Splitter 布局状态彻底删除

新布局不再允许用户上下拖动“检测信息/设置”比例，也不保存抽屉开合、当前页面或宽度。旧状态失去唯一用途后必须端到端删除，不留下无效字段或兼容代码。

实施时同时完成：

- 从 `main_window.ui` 删除 `splitter_mainContent` 和 `tabWidget_settings` 本体；四个旧 `tab_*` 页面容器重命名为 `page_*` 后直接迁入 `stackedWidget_rightDrawer`，并删除旧 `<attribute name="title">`，不得通过旧对象名、隐藏标签栏或无效标题元数据保留旧结构。
- 将检测信息滚动结构固定写入 `main_window.ui`：`page_inspectionInfo` 只包含一套 `scrollArea_inspectionInfo`，滚动内容页由唯一 `gridLayout_inspectionInfoContent` 直接承载现有五块内容；删除重复外层分组、两层包装布局以及 `main_window.cpp` 中 `new QScrollArea`、`setWidget()`、`insertWidget()` 等动态包装代码。
- 删除可视拖动抓手、`visualHandleHeight` 动态属性、Handle 尺寸恢复、Splitter 状态恢复，以及 `ui.splitter_state_invalid` 日志。
- 删除 `closeEvent()` 中只服务旧布局状态的整个保存代码块，包括 `AppSettings candidate`、`saveState()`、`saveConfiguration(candidate)` 和 `ui.splitter_state_save_failed` 日志；正常关闭不得留下无字段变化的重复设置保存。
- 删除 `MachineSettingsPage::ViewBindings::splitter_mainContent`、`class QSplitter` 前置声明及只服务旧 Splitter 的 `QSplitter/QSplitterHandle` 头文件。
- 删除 `main_window.cpp` 中只服务动态滚动区和拖动抓手的 `QLabel`、`QHBoxLayout`、`QSizePolicy`、`QFrame`、`QScrollArea`、`QSplitterHandle` 头文件；如果实施后的新代码没有实际使用，不得保留。
- 删除 `AppSettings::rightPanelSplitterState`。
- 删除 `app_settings.h` 中随该字段失去用途的 `QByteArray` 头文件；`app_settings_store.cpp` 仍因 JSON 文件字节读写使用 `QByteArray`，不做无关删除。
- 删除 `AppSettings` 相等比较中对该字段的比较。
- 删除 JSON 键 `ui.rightPanelSplitterStateBase64` 的写入、允许键声明、读取、Base64 转换和校验。
- 删除 `app_settings_store.cpp` 中只服务旧键的局部变量 `splitter`。
- 将 `AppSettings::CurrentSchemaVersion` 从 4 更新为 5；开发环境 Schema 4 配置直接删除或按现有重置流程重新生成。
- 全仓生产代码对 `rightPanelSplitterState` 和 `rightPanelSplitterStateBase64` 零引用。
- 生产 UI 对 `tab_detectionSettings`、`tab_imageSettings`、`tab_plcSettings`、`tab_softwareSettings` 和旧 Tab `title` 元数据零引用。
- 将 `app/system_support/settings/app_settings_store.cpp` 的当前文件说明和 `app/system_support/README.md` 的 AppSettings Store 说明同步为 Schema 5。
- 依据当前 `settingsToJson()` 更新数据参考 `docs/development/OCRGangYin新架构数据Schema.md` 的现行 AppSettings 结构和完整示例为 Schema 5；删除 `rightPanelSplitterStateBase64`，保留 `ui.templateSaveDirectory` 和包含 `enabled/receiverIp/receiverPort` 的 `resultExport` 分区；既有历史完成方案保持原始记录，不回写历史。
- 从正式 `app_theme.qss` 删除 `groupBox_verdictResult`、`splitter_mainContent`、`label_mainContentSplitterGrip` 的失效选择器；删除唯一正式 `QTabWidget` 后，同时删除正式主题中不再有目标控件的 `QTabWidget/QTabBar` 样式。未启用的历史主题 CSS 不在本轮精简范围内。
- 不新增 `rightDrawerOpen/rightDrawerPage/rightDrawerWidth` 等替代字段。

明确禁止：

- 不兼容读取旧键。
- 不保留废弃成员、占位字段或无效 JSON 键。
- 不做新旧键双写。
- 不增加 Schema 4 到 Schema 5 的迁移器、适配器或条件分支。
- 不为旧配置缺失或多出该字段增加特殊回退。
- 不把 Schema 4 的 `resultExport.enabled` 改成可选字段、默认填充、旧键别名或其他兼容形式。

## 12. 图标资源方案

### 12.1 用户选定资源

五个导航入口固定使用以下现有 SVG，不再搜索或替换其他图标：

| 导航入口 | 原始文件 | 当前文件 |
|---|---|---|
| 检测信息 | `app/resource/svg/scan-cube.svg` | `app/resource/svg/nav_inspection.svg` |
| 参数设定 | `app/resource/svg/adjustments-horizontal.svg` | `app/resource/svg/nav_parameters.svg` |
| 图像设置 | `app/resource/svg/photo-cog.svg` | `app/resource/svg/nav_image.svg` |
| PLC通讯 | `app/resource/svg/plug-connected.svg` | `app/resource/svg/nav_plc.svg` |
| 软件设置 | `app/resource/svg/settings.svg` | `app/resource/svg/nav_software.svg` |

### 12.2 重命名与资源规则

- 五个文件在 `app/resource/svg` 内原位重命名，不复制文件，不保留旧文件名。
- 最终只保留 `nav_inspection.svg`、`nav_parameters.svg`、`nav_image.svg`、`nav_plc.svg` 和 `nav_software.svg` 五个本方案导航资源。
- 不生成 PNG、ICO 或选中态副本，不保留 SVG＋PNG 两套重复资源。
- 保持现有 SVG 的图形路径、24×24 `viewBox`、透明背景和统一线性风格，界面显示尺寸为 28×28。
- 默认图标使用同一中性色；选中状态只通过 QSS 背景、中文文字和靠抽屉一侧的指示条表达。
- `main_window.ui` 和 `image.qrc` 只引用重命名后的 `nav_*.svg`，不得引用旧文件名或外部绝对路径。
- `app/AutoOCRproject.pro` 的 `QT` 模块增加 `svg`，直接使用 Qt 自带 SVG 支持；不引入第三方图标库或运行依赖。

### 12.3 来源与登记

- 在 `app/resource/README.md` 记录五个 SVG 的来源、许可、原始文件名、最终文件名和用途。
- `image.qrc` 登记重命名后的五个 SVG。
- 未核清来源或许可时暂停资源入库，不另找相似图标顶替。

## 13. 文件范围

### 13.1 计划内生产文件

| 文件 | 计划修改 |
|---|---|
| `app/ui/main_window.ui` | 建立右侧导航栏、抽屉标题、五页 `QStackedWidget` 和静态检测信息滚动区；删除旧 Splitter 与 `tabWidget_settings`；四个页面容器由 `tab_*` 重命名为 `page_*` 并删除 Tab 标题；取消统计分组 285px 最小宽度；移动唯一判定标签 |
| `app/ui/main_window.h` | 只增加显示页面和收起抽屉两个私有方法声明，不增加抽屉状态成员、页面枚举或运行状态副本 |
| `app/ui/main_window.cpp` | 直接使用 `.ui` 已创建的固定对象连接五个导航入口和收起按钮；从控件读取页面和开合状态，标题直接读取按钮文字；在现有 `faultSnapshotChanged` 回调调用 `presentFault()` 前处理首次 Fault；删除动态检测信息滚动包装、Splitter 装饰及其无用头文件；不动态创建或重新组装本方案界面 |
| `app/ui/main_window_inspection.cpp` | 在启动成功返回边界直接收起抽屉；在现有 `presentInspectionFault()` 调用 `presentFault()` 前处理首次 Fault；不保存上一运行状态；删除关闭时只服务旧 Splitter 的整个设置保存代码块 |
| `app/ui/pages/inspection_page.h/.cpp` | 保持单一判定入口；增加单一“等待结果”显示方法，不改变 Presentation 合同，不为固定判定标签增加空指针防护或回退路径 |
| `app/ui/pages/machine_settings_page.h/.cpp` | 删除只服务旧 Splitter 状态恢复的 UI binding、声明、头文件和代码；其余设置逻辑不动 |
| `app/system_support/settings/app_settings.h` | 删除 `rightPanelSplitterState` 及失效 `QByteArray` 头文件，将 `CurrentSchemaVersion` 从 4 更新为 5；保留 `resultExportEnabled` |
| `app/system_support/settings/app_settings.cpp` | 删除相等比较中的旧 Splitter 状态比较；保留 `resultExportEnabled` 默认值和比较 |
| `app/system_support/settings/app_settings_store.h/.cpp` | 删除 `rightPanelSplitterStateBase64` 的写入、允许键、读取、转换、校验及专用局部变量，将文件说明更新为 Schema 5；`templateSaveDirectory` 改为 Schema 5 必填字段；保留 `readBool()` 和 `resultExport.enabled` 严格读写，不增加迁移分支 |
| `app/system_support/README.md` | 将 AppSettings Store 的当前说明从 Schema 4 同步为 Schema 5；其他子系统说明不动 |
| `app/AutoOCRproject.pro` | 在现有 Qt 模块列表中增加 `svg` |
| `app/resource/qss/app_theme.qss` | 增加导航栏、抽屉、选中状态和显著判定栏样式；移除失效 Splitter、判定包装和正式 Tab 样式 |
| `app/resource/image.qrc` | 登记重命名后的五个 `app/resource/svg/nav_*.svg` 导航图标 |
| `app/resource/svg/scan-cube.svg` 等五个用户指定 SVG | 按第 12.1 节映射原位重命名为五个 `nav_*.svg`，不保留旧名或生成 PNG |
| `app/resource/README.md` | 记录五个 SVG 的来源、许可、原始名称、最终名称和用途 |
| `app/ui/README.md` | 更新主窗口右侧布局和维护规则 |

### 13.2 计划内配套文档

| 文件 | 计划修改 |
|---|---|
| `docs/development/OCRGangYin新架构数据Schema.md` | 按当前写出结构更新现行 AppSettings 章节和完整示例为 Schema 5，删除 `rightPanelSplitterStateBase64`，保留 `templateSaveDirectory` 和 `resultExport.enabled/receiverIp/receiverPort`；不修改历史完成方案中的旧结构记录 |

### 13.3 禁止修改的范围

除上述文件外，默认禁止修改：

- `app/application`。
- `app/contracts`。
- `app/runtime`。
- `app/detection`。
- `app/devices`。
- `app/templates`。
- `app/system_support/settings` 中除上表三个明确列出的设置文件之外的其他文件。
- `app/startup`。
- `app/tools` 和所有接收端程序。
- 模型、DLL、产品模板、翻译和历史主题资源。

如果实施中发现必须修改禁止范围才能成立，应暂停并向用户说明，不得以“配套修改”名义扩大范围。

## 14. 分阶段实施计划

### 阶段 RSB-0：实施基线和图标门禁（已完成）

1. 获得用户明确代码实施授权。
2. 记录分支、HEAD、工作区和暂存区。
3. 确认当前 UI-4、Overlay、图像提示栏和其他待验证差异的归属，不混入本方案。
4. 确认结果传输启用策略计划的 Schema 4 代码已成为受保护基线；若 `app_settings.h/.cpp`、`app_settings_store.cpp`、`main_window.h/.cpp` 或 `main_window_inspection.cpp` 仍含该计划的暂存/未提交修改，先由用户完成其验证与收口或明确授权在当前结果上继续。
5. 固定记录并保护 `resultExportEnabled`、`resultExport.enabled`、`readBool()`、复选框恢复/保存和 ApplicationService 启动门禁，禁止在本方案差异中回退。
6. 核对用户提供的五个指定 SVG 的来源、许可、24×24 `viewBox` 和透明背景，不再寻找替代图标。
7. 冻结 `main_window.ui` 当前对象名清单和四个设置页控件数量。

退出条件：实施基线清晰，图标资源可合法进入仓库，计划外工作区差异已隔离。

### 阶段 RSB-1：静态布局迁移（已完成）

1. 必须在 `main_window.ui` 中静态创建固定右侧导航栏、五个导航按钮及其最终布局和属性。
2. 在 `main_window.ui` 中将 `widget_rightPanel` 改为初始隐藏的抽屉，静态增加标题栏和收起按钮。
3. 在 `main_window.ui` 中静态创建一个 `QStackedWidget`，承载检测信息和四个现有设置页。
4. 在 `page_inspectionInfo` 中静态建立唯一 `scrollArea_inspectionInfo`，以唯一 `gridLayout_inspectionInfoContent` 直接承载现有检测信息内容，删除重复外层分组、两层包装布局和 C++ 动态包装代码。
5. 删除 `tabWidget_settings` 本体，将四个旧 `tab_*` 页面容器重命名为固定 `page_*` 名称并删除四个旧 Tab `title` 属性，页面内部业务控件对象名不变。
6. 取消四个生产统计分组的 285px 最小宽度，设置两列 1:1 静态拉伸并关闭检测信息横向滚动条。
7. 将 `label_verdictResult` 移到图像区顶部，删除空包装 GroupBox。
8. 删除旧纵向拖动抓手的可见布局和失效 QSS。
9. 确认本方案没有任何固定可见控件、布局或父子关系留到 C++ 动态创建或装配。

退出条件：Qt Designer 能完整看到收起导航、展开抽屉、五个页面和显著判定栏的固定布局。

### 阶段 RSB-2：UI 交互与旧状态清理（已完成）

1. 连接五个导航按钮与 `QStackedWidget` 页面；显示方法只接收页面和按钮，标题直接读取按钮文字。
2. 实现点击当前入口再次收起和标题栏按钮收起。
3. `.ui` 中的抽屉默认隐藏、默认页为检测信息、五个导航入口默认未选中；不增加对应 C++ 状态副本。
4. 在现有 `StartInspectionResult::isAccepted()` 成功分支直接显示“等待结果”并收起抽屉；启动取消或失败不改变抽屉和判定。
5. 分别在 `main_window.cpp` 的现有 `faultSnapshotChanged` 回调和 `main_window_inspection.cpp` 的现有 `presentInspectionFault()` 中，于调用 `InspectionPage::presentFault()` 前检查 `m_faultAlarmPresented`；首次 Fault 打开检测信息，同一 Fault 后续刷新不再打开，Fault 解除沿用现有复位，不新增转发方法、重载或状态。
6. 普通 `updateOperationUiState()` 不操作抽屉，保证运行中手动打开后不会被状态刷新再次关闭。
7. 移除旧 Splitter 的 UI 绑定、恢复、动态属性、日志和相关声明、头文件。
8. 删除 `closeEvent()` 中只服务 Splitter 布局状态的整个设置保存代码块，不保留无字段变化的重复保存。
9. 删除 `rightPanelSplitterState`、`rightPanelSplitterStateBase64` 及全部读写、比较、校验和专用局部变量，将 AppSettings Schema 版本从 4 更新为 5。
10. 保留 `resultExportEnabled`、`resultExport.enabled`、`readBool()` 及全部现有结果传输 UI、启动门禁和运行配置代码，不借设置文件修改回退该已实施计划。
11. 将 AppSettings Store 文件说明、子系统 README 和当前数据 Schema 参考同步为 Schema 5；`templateSaveDirectory` 与 `resultExport` 字段按 Schema 5 严格必填，不保留缺字段读取；不修改历史完成方案。
12. 不实现 Schema 4 迁移、兼容读取、双写或替代持久化字段。

退出条件：抽屉操作只改变 UI 可见性、当前页和按钮选中状态。

### 阶段 RSB-3：判定栏与统一样式（已完成）

1. 在正式 QSS 中增加导航、抽屉和判定栏状态样式，删除旧 Splitter、旧判定包装和已无正式目标控件的 Tab 样式。
2. 增加单一 `InspectionPage::showWaitingResult()`，只在启动成功分支调用并直接更新固定判定标签，不增加空指针防护或回退路径。
3. 保持 `InspectionPage::present()` 正确/错误更新链和 FaultPresenter 不变。
4. 按第 12.1 节原位重命名五个 SVG，增加 Qt SVG 模块，登记 `image.qrc` 并更新 UI/资源 README。

退出条件：正确、错误和 Fault 文字清楚，颜色与现有统一主题协调，生产结果仍只有一个判定控件。

### 阶段 RSB-4：静态门禁已完成，待用户验证

Agent 完成静态门禁后，由用户在 Qt Creator 执行 Run qmake、Clean、Rebuild 和人工交互验证。未通过用户验证前不得标记完成或提交。

## 15. 行为保持清单

必须保持：

- 主控操作台六个按钮及其行为。
- 唯一 `ImageLabel`、模板向导位置关系和五模式模板绘图。
- 相机打开、关闭、预览、软硬触发和检测启停。
- 模板选择、当前编辑模板、目标文字、阈值和模板保存。
- 二维码结果传输区域的模式可见性、连接状态和操作行为。
- 图像设置、PLC 设置和软件设置的全部字段、默认值和应用行为。
- `resultExportEnabled` 的持久化、`resultExportEnable` 恢复/保存、TCP 状态解耦、二维码+三期开始门禁和当前每轮运行配置。
- 所有未应用 `*`、取消、成功提交和失败回退。
- 正确/错误判定、识别内容、耗时、统计、模板名和主图像的同生命周期更新。
- 统计清零、NG 清零和剔除复位的空闲态权限。
- Fault 锁定、弹窗、现场确认和恢复流程。
- 正式视觉样式只来自 `app_theme.qss`。

允许改变：

- 右侧信息和设置由常驻大面板改为按需抽屉。
- 原四个横向 Tab 由五个纵向导航入口替代。
- 原纵向 Splitter Handle 不再显示或使用。
- 现有判定标签从检测信息页移动到图像区顶部并增强视觉。
- 启动检测和 Fault 进入时发生本文规定的一次性 UI 自动开合。
- 旧 Splitter 持久化字段和 JSON 键被删除，Schema 4 开发配置不再兼容，按 Schema 5 重新生成。

## 16. 风险与控制

| 风险 | 控制方式 |
|---|---|
| 页面迁移后控件绑定失效 | 四个页面容器统一改为 `page_*`；页面内部全部业务控件对象名保持不变并逐项核对 ViewBindings |
| 抽屉切换触发参数应用或丢失草稿 | 导航只设置可见性和当前页，不调用页面初始化、恢复或应用接口 |
| 运行状态刷新反复关闭用户手动打开的抽屉 | 只在启动成功返回点收起；普通 `updateOperationUiState()` 不操作抽屉，不保存上一 UI 状态 |
| 启动失败却收起抽屉 | 只在 `StartInspectionResult::isAccepted()` 成功分支收起 |
| Fault 被当成产品错误 | 保留 FaultPresenter 和故障弹窗；Fault 自动打开检测信息并使用独立文案 |
| 判定控件被复制导致状态不一致 | 只移动原 `label_verdictResult`，全仓仍只保留一个同名正式判定控件 |
| 旧 Splitter 状态形成无效字段和胶水代码 | 从 `AppSettings`、JSON 和 UI 端到端删除，Schema 从 4 直接更新为 5，不增加迁移或兼容分支 |
| Schema 更新回退已实施的结果传输持久化 | 以当前 Schema 4 为受保护基线，只删除 Splitter 字段；保留 `resultExportEnabled`、`resultExport.enabled`、`readBool()`、复选框提交和启动门禁 |
| 重叠文件仍含其他计划的暂存或未提交修改 | RSB-0 先完成交叉计划门禁；没有用户明确协调时不覆盖、不取消暂存、不把右侧栏实施混入结果传输批次 |
| 删除动态滚动包装后检测信息在低分辨率下不可完整访问 | 在 `main_window.ui` 的检测信息页固定保留唯一 `scrollArea_inspectionInfo`，只删除 C++ 动态创建胶水 |
| 删除字段后关闭窗口仍执行无意义设置保存 | 删除 `closeEvent()` 中只服务 Splitter 状态的整个 candidate/saveConfiguration/失败日志代码块 |
| 旧容器被隐藏而非删除 | `tabWidget_settings` 和 `splitter_mainContent` 本体零引用，四个设置页直接归属 `stackedWidget_rightDrawer` |
| 旧 Tab 命名和标题元数据残留 | 四个页面容器直接重命名为 `page_*`，删除旧 `<attribute name="title">`，不保留旧对象名别名 |
| 抽屉页面、开合和运行边沿形成重复状态 | 页面和开合直接读取 `QStackedWidget/QWidget`；启动使用成功返回点，Fault 复用 `m_faultAlarmPresented`，不新增状态成员 |
| 两个 Fault 呈现入口只修改其中一个 | `faultSnapshotChanged` 回调和 `presentInspectionFault()` 都在各自现有 `presentFault()` 调用前执行同一首次 Fault 判断，不新增转发层 |
| 500–580px 抽屉裁剪生产统计 | 在 `.ui` 中取消四个统计分组 285px 最小宽度并设置两列等宽伸缩，不增加运行时响应式代码 |
| 固定界面继续由 C++ 动态拼装 | 所有本方案控件、布局、父子层级和初始属性写入 `main_window.ui`；C++ 只连接信号和更新已有对象状态 |
| 为访问静态控件新增查找或缓存胶水 | 新增控件使用固定对象名并直接通过 `ui->` 访问，不增加 `findChild()`、缓存指针、成员别名或动态按钮组 |
| 标题参数和按钮文字形成重复文案 | 显示方法只接收页面和按钮，抽屉标题直接读取 `button->text()` |
| 为固定 `.ui` 控件增加防御性分支 | 新增抽屉方法和 `showWaitingResult()` 直接操作固定控件，不增加空指针判断、回退查找、备用路径或静默返回 |
| SVG 重命名后仍残留旧名或重复资源 | 按固定映射原位重命名；QRC 和 UI 只引用 `nav_*.svg`；旧五个名称和 `nav_*.png` 零残留 |
| SVG 资源已登记但运行时无法加载 | 工程显式增加 Qt `svg` 模块，Debug/Release 均从 QRC 验证五个图标 |
| 低分辨率下图像空间不足 | 抽屉最大 580，导航固定 80；在 1600×950 和 1366×768 分别验证 |
| QSS 选择器误伤主控 QToolButton | 所有新增选择器限定到右侧导航和抽屉对象名 |

## 17. 静态门禁

实施后 Agent 必须完成：

- `main_window.ui` XML 可解析。
- `widget_rightNavigationRail`、`frame_rightDrawerHeader`、`label_rightDrawerTitle`、`toolButton_collapseRightDrawer` 和五个固定对象名的导航按钮均由 `main_window.ui` 创建且各唯一存在。
- `stackedWidget_rightDrawer` 恰好包含 `page_inspectionInfo`、`page_detectionSettings`、`page_imageSettings`、`page_plcSettings` 和 `page_softwareSettings` 五个页面。
- 五个导航按钮在 `.ui` 中具有固定文字、Tooltip、重命名后的 QRC SVG、28×28 图标尺寸、`ToolButtonTextUnderIcon` 和 `checkable=true`；“检测信息”初始选中，其余四个入口初始未选中。
- `widget_rightPanel` 在 `.ui` 中初始可见；`stackedWidget_rightDrawer` 默认页为 `page_inspectionInfo`。
- `page_detectionSettings/page_imageSettings/page_plcSettings/page_softwareSettings` 各唯一存在；生产 UI 中不存在 `tab_detectionSettings/tab_imageSettings/tab_plcSettings/tab_softwareSettings` 或四个旧 Tab `title` 属性。
- `page_inspectionInfo` 中恰好存在一个由 `.ui` 声明的 `scrollArea_inspectionInfo`；其 `scrollAreaWidgetContents_inspectionInfo` 只以 `gridLayout_inspectionInfoContent` 直接承载运行状态、识别内容、生产统计、当前模板和剔除复位按钮；生产 UI 中不存在重复的检测信息外层分组或旧包装布局对象名，C++ 中不存在该滚动区的 `new QScrollArea`、`setWidget()`、`insertWidget()` 或重新挂接父对象代码。
- `groupBox_totalCount/groupBox_ngCount/groupBox_detectionDuration/groupBox_passRate` 的 `minimumWidth` 均为 0，生产统计两列拉伸比为 1:1，`scrollArea_inspectionInfo` 横向滚动条关闭。
- `label_verdictResult` 全仓正式 UI 只存在一个，ViewBindings 仍指向它。
- 判定栏只由 `InspectionPage` 的现有清理/恢复入口及 `showWaitingResult()/present()/presentFault()` 更新；`MainWindow` 不直接设置其文案、`verdict` 属性或样式。
- `tabWidget_settings`、`splitter_mainContent`、`groupBox_verdictResult` 和 `label_mainContentSplitterGrip` 在生产 UI、C++ 和正式 QSS 中零引用；旧容器不得以隐藏、禁用或占位方式保留。
- `app/ui` 中不存在 Splitter 保存、恢复、`visualHandleHeight`、动态 Handle 装饰、`ui.splitter_state_invalid` 或 `ui.splitter_state_save_failed` 引用。
- `closeEvent()` 中不存在只为布局状态创建 `AppSettings candidate` 或调用 `saveConfiguration()` 的代码；关闭流程不产生无字段变化的设置写盘。
- `main_window.cpp` 不保留只服务旧动态包装和抓手的 `QLabel/QHBoxLayout/QSizePolicy/QFrame/QScrollArea/QSplitterHandle` 头文件；`machine_settings_page.h/.cpp` 不保留 `QSplitter` 前置声明、ViewBinding 或 `QSplitter/QSplitterHandle` 头文件。
- `main_window.cpp` 和 `main_window_inspection.cpp` 不 `new` 本方案任何可见控件或布局，不对本方案固定界面调用 `addWidget()/insertWidget()/setWidget()/setLayout()/setParent()`，不使用 `findChild()/findChildren()` 查找本方案控件。
- 新导航和抽屉代码直接使用 `ui->固定对象名`，不增加缓存指针、重复成员别名、动态 `QButtonGroup`、导航管理器或抽屉控制器；现有定时器、校验器和页面逻辑对象不纳入此项禁止范围。
- `main_window.h/.cpp` 不新增抽屉页面枚举、当前页面字段、开合布尔值、上一 `OperationUiState`、Fault 抽屉标记或页面缓存；显示页面和收起抽屉只有两个私有方法，显示方法签名固定为 `showRightDrawerPage(QWidget *page, QToolButton *button)`，不存在标题参数或五份重复标题字符串，标题只读取 `button->text()`。
- `main_window.cpp` 的 `faultSnapshotChanged` 回调和 `main_window_inspection.cpp` 的 `presentInspectionFault()` 均在各自调用 `InspectionPage::presentFault()` 前检查 `m_faultAlarmPresented`；两处之外不新增 Fault 转发方法、重载、中转层或状态。
- `showRightDrawerPage()`、`collapseRightDrawer()` 和 `InspectionPage::showWaitingResult()` 不对 `.ui` 固定控件增加空指针判断、回退查找、备用路径或静默返回。
- 正式 `app_theme.qss` 中不存在 `QTabWidget/QTabBar`、`groupBox_verdictResult`、`splitter_mainContent` 或 `label_mainContentSplitterGrip` 失效选择器；未启用历史主题 CSS 不作为本轮零引用门禁。
- `rightPanelSplitterState` 与 `rightPanelSplitterStateBase64` 在生产代码和当前方案中除删除说明外零引用。
- `AppSettings::CurrentSchemaVersion` 为 5；新写出的 JSON 不包含旧 Splitter 键。
- `app_settings.h` 不再包含只服务旧字段的 `QByteArray` 头文件，`app_settings_store.cpp` 不存在旧键专用局部变量 `splitter`。
- `resultExportEnabled`、`resultExport.enabled`、`readBool()`、`hasOnlyKeys(resultExport, enabled/receiverIp/receiverPort)`、MainWindow 复选框恢复/保存和 ApplicationService 启动门禁保持当前实现；本方案不新增或删除其他 AppSettings 字段。
- 设置代码中不存在 Schema 4 到 5 的迁移、旧键兼容读取、双写、占位成员或替代抽屉持久化字段；现有通用 Schema 不匹配拒绝机制保持原样，不增加 Schema 4 专用分支。
- Schema 5 的 `ui.templateSaveDirectory`、`resultExport.enabled/receiverIp/receiverPort` 均通过现有严格读取函数读取，不保留缺字段默认、可选读取或旧 Schema 回退。
- `app_settings_store.cpp` 文件说明和 `app/system_support/README.md` 的 AppSettings Store 说明均为 Schema 5。
- `OCRGangYin新架构数据Schema.md` 的现行 AppSettings 章节和完整示例为 Schema 5，不包含 `rightPanelSplitterStateBase64`，并包含当前 `templateSaveDirectory` 与 `resultExport.enabled/receiverIp/receiverPort`；历史完成方案不纳入零引用要求。
- `app/system_support/settings` 只有计划内四个设置文件发生最小删除式差异；Runtime、Detection、Devices、Templates 无差异。
- `app/resource/svg/nav_inspection.svg`、`nav_parameters.svg`、`nav_image.svg`、`nav_plc.svg` 和 `nav_software.svg` 全部存在，SVG XML 可解析且保持24×24 `viewBox`。
- `scan-cube.svg`、`adjustments-horizontal.svg`、`photo-cog.svg`、`plug-connected.svg` 和 `settings.svg` 五个旧文件名不存在；不生成任何 `nav_*.png`。
- `app/AutoOCRproject.pro` 已声明 Qt `svg` 模块，`image.qrc` 和 `main_window.ui` 只引用重命名后的五个 SVG。
- qmake 资源清单路径存在且无重复。
- C++ 不新增完整控件 `setStyleSheet()`；正式样式仍只从 `app_theme.qss` 加载。
- `git diff --check`、UTF-8 中文和文件末尾换行检查通过。
- 静态检查不得表述为构建或交互验证通过。

## 18. 用户验证清单

### 18.1 布局和导航

1. Run qmake、Clean、Rebuild，启动后窗口最大化且默认展开检测信息页。
2. “检测信息”入口初始选中，右侧五个图标和中文名称清晰、无裁剪。
3. 点击当前“检测信息”入口收起抽屉，确认右侧只常驻 80px 导航栏；再逐一点击五个入口，确认只显示对应现有页面。
4. 点击当前入口和标题栏收起按钮都能收起，且五个入口清除选中状态。
5. 展开抽屉压缩图像但不覆盖图像、模板向导或框选内容。
6. 1600×950、1366×768 和 Windows 125% 缩放下无重叠、横向滚动或文字裁剪。
7. 抽屉宽度分别为 500、540、580 时，检测信息四个生产统计分组保持两行两列且内容完整，无横向裁剪。

### 18.2 设置和草稿保持

1. 在各页面修改但不应用参数，切换页面和收起再展开后值与 `*` 保持。
2. 页面切换不自动应用、不恢复正式值、不弹出无关提示。
3. 五种检测模式切换后，参数页和纸巾/二维码专用区域可见性保持当前行为。
4. 参数应用、失败回退、模板选择、图像设置、PLC 设置和软件设置逐项可用。

### 18.3 运行状态

1. 启动前打开任意抽屉；取消未应用设置确认后抽屉保持原状态。
2. 启动失败时抽屉保持原状态。
3. 启动成功时抽屉自动收起一次，判定栏显示“等待结果”。
4. 运行中手动打开检测信息，后续产品结果不会再次强制收起。
5. 运行中打开四个设置页，内部控件继续禁用且 Tooltip 原因正确。
6. 正常停止后不自动打开抽屉，最后一件判定保持可见。
7. Fault 首次进入时自动打开检测信息，故障弹窗、文案和恢复流程正确；同一 Fault 不反复弹开。

### 18.4 判定和检测信息

1. 正常 OK 显示绿色实底白字“正确”。
2. 正常 NG 显示红色实底白字“错误”。
3. 判定与本次图像、识别文字、耗时、统计和模板名保持同一生命周期。
4. 点击“检测信息”仍能查看运行状态、识别内容、统计、耗时、模板名和复位按钮。
5. 在 1366×768 和 Windows 125% 缩放下，检测信息页可以通过自身纵向滚动区访问全部内容，无横向滚动。
6. Fault 不显示为普通“错误”，恢复后新 Run 不显示旧 Run 结果。

### 18.5 资源和回归

1. Debug/Release 中五个重命名后的 SVG 均能从 Qt 资源加载，无 PNG 副本和外部绝对路径。
2. 主控按钮、模板制作向导、相机预览、五模式检测、统计清零、剔除复位和正常退出无回归。
3. 使用当前 Schema 4 开发配置启动，确认沿用现有版本不匹配流程重置并生成 Schema 5 配置，不含 `rightPanelSplitterStateBase64`，且不经过迁移或兼容读取。
4. 确认 Schema 5 配置仍包含严格布尔字段 `resultExport.enabled` 以及现有 `receiverIp/receiverPort`，结果传输复选框恢复、保存和二维码+三期启动门禁保持当前行为。
5. 使用新配置关闭并重启，确认设置正常读取且抽屉仍默认展开检测信息页。

## 19. 实施与提交策略

- 用户已明确授权并完成本方案生产代码实施。
- 本批次保持为独立 UI 改造，不修改模板绘图、Overlay、提示栏、日志、TCP 或算法实现。
- 旧 Splitter 状态采用开发期直接删除策略；不得为旧配置增加任何兼容层、迁移层、胶水层或防御性回退。
- Schema 4 结果传输持久化是本方案的受保护前置基线；本方案只将其完整带入 Schema 5，不修改该计划的字段、政策或调用链。
- Agent 完成静态门禁后，由用户统一构建和人工验证。
- 用户明确验证通过后才能更新计划状态并创建本地提交；提交、推送和合并仍需服从用户后续授权。

## 20. 完成标准

同时满足以下条件才可标记本方案完成：

- 右侧导航栏始终可用，软件启动时默认展开检测信息页，并能稳定切换和收起五个现有页面。
- 主图像区域在抽屉收起时获得原右侧大面板空间。
- 唯一判定栏持续、清晰地区分“正确”“错误”和系统 Fault。
- 导航和抽屉不改变任何参数、设备、检测、模板、统计、存图或故障合同。
- 旧 Splitter、旧 `QTabWidget`、旧判定包装、状态字段、JSON 键、关闭保存块、相关声明头文件日志和失效正式 QSS 全部删除，不存在隐藏容器、兼容或占位残留。
- 检测信息页直接显示现有五块内容，抽屉标题下不存在第二个“检测信息”分组，旧外层分组及其包装布局零残留。
- 四个设置页面容器使用正式 `page_*` 名称，旧 `tab_*` 名称和 Tab 标题元数据完全删除。
- 抽屉状态只来自 `widget_rightPanel` 和 `stackedWidget_rightDrawer`，启动与 Fault 复用现有调用边界，不存在重复页面、开合或运行状态字段。
- 抽屉标题只读取当前导航按钮文字，两个现有 Fault 呈现入口均覆盖且不经过新增转发层；新增抽屉和等待判定代码不包含针对固定 `.ui` 控件的防御性分支。
- 本方案所有固定可见控件、布局、页面层级和初始属性均由 `main_window.ui` 静态表达，C++ 只连接信号并更新已有对象状态，不存在运行时界面拼装胶水。
- 五个用户指定 SVG 已按固定映射重命名并直接使用，旧名称和 PNG 副本不存在。
- AppSettings 最终为 Schema 5，只删除旧 Splitter 字段和 JSON 键；Schema 4 已存在的结果传输启用字段、严格读写和运行行为完整保留，未增加新的 AppSettings 字段。
- 计划内静态门禁全部通过。
- 用户完成 Qt Creator 构建和全部人工交互验证并明确确认通过。
