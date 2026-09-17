# OCRGangYin 界面文字统一方案

## 1. 文档状态

- 文档日期：2026-09-06。
- 最新决定：用户已授权完整实施本文全部阶段，整体基础文字由原计划的 16px 提升为 18px，全应用只保留三个字号和常规/加重两种字重；普通主文字和次文字统一为纯黑色，模板向导保留现有 RichText、四种区域强调色和必要转义，字符框选画布空提示保留现有 `Qt::gray`、只统一继承字体；代码完成后由用户统一验证。
- 当前状态：代码已实施，Agent 静态门禁通过，待用户在 Qt Creator 统一验证。
- 权威范围：主程序九个 `.ui`、运行时动态文字、动态创建控件、模型/列表项、Tooltip、Placeholder、Qt 消息框、非原生目录选择框、模板制作向导和字符框选画布中的 UI 提示文字，以及正式 QSS 的字体族、字号、字重、文字颜色、选中文字颜色和加载时点。
- 保持范围：不改变任何文字内容、翻译、控件结构、业务状态、操作流程、模板绘制步骤、检测逻辑或数据合同。
- 计划关系：本方案只在文字系统范围内覆盖 `OCRGangYin工业视觉风格UI改造方案.md` 的旧字号、字重、文字颜色和 QSS 加载时点；其他布局、控件尺寸、背景、边框、图标和状态规则继续保持。
- 后续覆盖关系：`OCRGangYin界面字号导航与画布细节调整方案.md` 和 `OCRGangYin左侧导航与页面抽屉迁移方案.md` 覆盖本文字号细目；判定栏已改用 OK/NG SVG，不再参与文字字号、字重和文字颜色映射。

## 2. 实施前事实与问题

实施前实现已经满足以下基础条件：

- 只有 `app/resource/qss/app_theme.qss` 一份正式 QSS。
- 字体族只在 `QWidget` 根规则中定义一次，为 `Microsoft YaHei UI`、`Segoe UI`、sans-serif。
- 九个 `.ui` 没有 `font`、`palette` 或局部 `styleSheet`。
- 生产 UI C++ 没有 `QFont`、`setFont()`、`setPalette()` 或控件级 `setStyleSheet()`。

2026-09-06 对主程序真实 UI 入口的补充审计结果为：

- 九个 `.ui` 覆盖主窗口、五个右侧页面和三个模板对话框；静态文字包括窗口内容、分组标题、标签、按钮、复选框、输入值、单位、下拉项、树表头、Tooltip 和 Placeholder。
- `app/ui` 与 `app/startup` 中共有 12 个生产源码文件会设置、生成或绘制可见文字；运行状态、判定、统计值、当前模板、按钮状态、未应用标记、动态说明、模板树内容和字符编辑内容都必须按其所属控件统一，不因文字由 C++ 创建而排除。
- 实施前有 61 处 `QMessageBox` 静态便利调用和 6 处显式构造，共 67 个消息框创建位置；删除零调用富文本函数后保留 61 处便利调用和 5 处显式构造，共 66 个有效创建位置。4 个 `QFileDialog::getExistingDirectory()` 全部保持 `DontUseNativeDialog`。
- 字符模板对话框会动态创建序号、字符名称输入、校验错误、保存文件名、预览文件名、空状态和 Tooltip，这些文字当前由普通 Qt 控件承载，可以直接继承正式 QSS。
- `CharacterCropLabel` 有两处 `QPainter::drawText()`：无图时的 UI 空提示和字符框序号。两处文字只统一继承字体；空提示保留现有 `Qt::gray`，序号颜色继续跟随字符框图像标注。
- 除模板向导外，`MainWindow::showParameterInfoWithRedWarning()` 还保留一条 `Qt::RichText`、`<div>`、`<p>`、`<strong>` 组合路径；该函数的声明和定义当前均为零调用，应直接删除而不是改造成第二个文字样式入口。
- 当前 QSS 只显式覆盖 `QTreeWidget/QListWidget/QTableWidget`，尚未完整覆盖非原生 `QFileDialog` 使用的 `QTreeView/QListView`；应收口到 `QAbstractItemView`。

但当前文字体系仍然过细：

| 项目 | 当前状态 | 问题 |
|---|---|---|
| 字号 | 13、14、15、18、20、24、28、32、46，共九档 | 相邻档差异小，普通标题、提示和重要信息之间缺少稳定映射 |
| 字重 | 400、600、700、800，共四档 | 本机微软雅黑只有 Light、Regular、Bold，多个高字重可能映射到相同粗体字形 |
| QSS 文字色 | 十六种 | 中性色过多，普通正文与辅助说明没有必要继续使用不同灰度 |
| 模板向导富文本 | 四种区域强调色和 `<b><span>` | 属于现有绘制步骤辨识功能，按用户最新决定原样保留，不扩散为通用文字角色 |
| 其他 C++ 富文本字重 | 一处零调用 `<strong>` 路径 | 与模板向导无关且没有调用，应直接删除 |
| 自绘 UI 文字 | 一条空提示和一组字符框序号 | 现有颜色保持不变；两处文字尚未显式继承控件统一字体 |
| 模型视图与弹出视图 | 只覆盖三个具体 Widget 类 | 非原生目录选择框和 ComboBox 弹出视图的选中文字尚未统一收口 |
| 加载时点 | 主窗口构造时加载 QSS | RuntimeGuard、单实例和设置错误等启动消息框可能先使用系统字体与系统文字色 |

需要取消文字用途的中性色为：

```text
#21303D  #263746  #314455  #344A5B  #4C5C69
#536573  #536574  #687887  #F7F9FB
```

以上颜色只取消 `color` 或 `selection-color` 用途；仍被背景、边框等非文字属性使用的颜色保持不变。

模板向导继续保留的四个 C++ 区域强调色为：

```text
#157A3D  #0067C0  #B85C00  #C43D24
```

## 3. 统一原则

1. 不把所有文字设置成同一字号和字重；只保留能够表达正文、标题、重要数据和最终判定的必要层级。
2. 全应用只保留三个字号值：18、26、48px。
3. 全应用只保留常规和加重两个字重：常规为 400，加重为 600。
4. 普通信息统一使用 18px 和纯黑色，不再区分主文字与次文字；同为黑色的正文、说明、单位和标题只通过常规/加重、位置与留白建立层级，更大字号只留给重要数据和最终判定。
5. 业务状态只使用固定的蓝、绿、橙、红；同一语义在任何页面使用同一颜色。
6. 静态创建和动态创建的文字使用同一角色表；动态 `setText()`、模型项和标准对话框不建立单独字号、字重或颜色规则。
7. 通用字体、字重、字号和文字颜色只在正式 QSS 中定义；`.ui` 和普通生产 C++ 不写视觉值。现有模板向导 RichText 是唯一保留的局部富文本颜色入口；`CharacterCropLabel` 只增加 `QPainter::setFont(font())` 以继承控件字体，现有 `Qt::gray` 和图像标注色保持不变，不读取调色板或新增颜色。
8. QSS 在 `QApplication` 创建后、任何消息框出现前加载一次；不保留主窗口内的第二个加载入口。
9. 不新增文字角色属性、字体管理器、主题管理器、运行时缩放器、颜色转换工具或对话框专用主题。
10. 本方案只覆盖主程序 `app/`；不把两个独立工具、Windows 原生标题栏和图像 Overlay 扩入本轮。

## 4. 字体族

正式字体族保持：

```qss
font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
```

- `Microsoft YaHei UI` 是 Windows Qt 界面的主字体。
- `Segoe UI` 和 sans-serif 只作为字体栈后备，不为不同页面主动指定不同字体。
- 所有 QWidget 子类从根规则继承字体族。
- `.ui` 不写 `<font>`，C++ 不创建或覆盖 `QFont`。
- 不改成参考 HTML 的 `Microsoft YaHei`；本机二者来自同一 TTC 字体集合，切换名称不能解决当前层级混乱。

## 5. 三档字号与两档字重

### 5.1 字号和字重表

| 层级 | 字号 | 字重 | 使用范围 |
|---|---:|---:|---|
| 常规界面文字 | 18px | 常规 400 | 普通标签、正文、输入值、Placeholder、下拉项、列表/树/表格内容、复选框、单位、说明、Tooltip、消息框正文、动态提示、空状态、错误提示、只读值 |
| 加重界面文字 | 18px | 加重 600 | 分组标题、页面内小标题、表头、普通按钮、对话框按钮、主控按钮、右侧导航、统计小卡标题、重要开关 |
| 重要信息与数据 | 26px | 加重 600 | 识别内容、当前模板名称、模板制作向导标题、统计值、运行状态、目标字符编辑框 |
| 最终判定 | 48px | 加重 600 | 等待检测、OK、NG 的唯一大判定文字 |

表中按使用场景列出四种文字角色，但只使用三个不同字号。所有普通界面文字统一为 18px 和纯黑色，通过常规或加重区分正文与标题；原 20px、24px、28px、32px 统一为 26px，不再为相邻的重要信息单独设档。

### 5.2 删除的字号和字重

- 取消原计划的 16px，并删除现有 13、14、15px；原 18px 模板向导正文保持 18px，其他普通界面文字统一提升或合并为 18px，不再区分辅助小字、正文和普通标题字号。
- 原 18px 模板向导标题与 20、24、28、32px 的重要信息、统计数据和运行状态全部统一为 26px。
- 删除 46px：最终判定放大为 48px。
- 删除 700、800：只保留常规 400 和加重 600；重要程度由三档字号、颜色、背景和位置承担。

## 6. 七个通用文字颜色与四个模板向导强调色

### 6.1 七个通用文字颜色

| 文字角色 | 固定颜色 | 使用规则 |
|---|---|---|
| 普通文字 | `#000000` | 正文、普通标签、输入值、标题、按钮、表头、单位、说明、提示、只读值、导航默认文字和统计小标题 |
| 禁用文字 | `#909DA8` | Disabled、空内容和不可用提示 |
| 主操作 | `#1769AA` | 选中、Focus、运行中、识别内容、模板向导标题 |
| 成功 | `#17834C` | Ready、OK 统计值和成功语义 |
| 警告 | `#C98218` | Stopping、Warning |
| 危险 | `#C13F3F` | NG、错误提示和危险操作 |
| 反白 | `#FFFFFF` | 实色主按钮、危险按钮、OK/NG 判定及深色 Tooltip |

规则：

- 原主文字和次文字全部并入纯黑色 `#000000`；普通正文、说明、单位、只读值、导航默认文字和普通标题之间不再使用深浅差异。
- `#21303D`、`#687887`、`#263746`、`#314455`、`#344A5B`、`#4C5C69`、`#536573`、`#536574` 和 `#F7F9FB` 不再作为文字色；其中仍承担背景或边框职责的颜色保持不变。
- `#FFFFFF` 统一替代按钮禁用态中的 `#F7F9FB` 文字；`#F7F9FB` 仍可作为背景色存在。
- 所有 `QAbstractItemView` 的选中背景统一为 `#1769AA`，选中文字统一为 `#FFFFFF`，同时覆盖下拉弹出项、主程序树表和非原生目录选择框内部视图。
- `#0E568E` 只保留在既有按钮按下背景和边框等非文字视觉属性中，不得再出现在 `color` 或 `selection-color`。
- Tooltip 使用现有深色背景和 `#FFFFFF` 文字，不增加专用字体层级。
- Placeholder 使用 18px/400，并沿用当前 Qt 5.15.2 `QPalette::PlaceholderText` 默认呈现；不为 Placeholder 新增 `setPalette()`、显式颜色或专用样式入口。
- `CharacterCropLabel` 的自绘空提示保留现有 `Qt::gray`，不为它新增 QSS 颜色选择器或调色板映射；该颜色是既有自绘例外，不计入七个通用文字角色色。
- 不在 QSS 或普通生产 C++ 主动生成新的透明文字色，不在 C++ 调整颜色深浅。

### 6.2 模板向导的四个保留强调色

| 强调对象 | 保留颜色 |
|---|---|
| 普通检测区域、生产日期检测区域 | `#157A3D` |
| 文字/OCR/二维码定位锚点、生产日期定位锚点 | `#0067C0` |
| 二维码区域、钢印区域定位锚点 | `#B85C00` |
| 钢印检测区域 | `#C43D24` |

- 四个颜色只允许存在于 `emphasizedDrawingRegion()` 的现有 RichText 生成路径，不纳入通用文字角色，也不扩展到其他页面。
- 保留现有 `<b><span style="color:...">`、`Qt::RichText` 和区域名称 `toHtmlEscaped()`，继续同时提供颜色和加重强调。
- 模板向导的区域名称、步骤编号、提示内容和现有括号形式全部保持，不增加新的格式或颜色。

## 7. 各区域的最终映射

### 7.1 全局和容器

- `QWidget`：18px、常规 400、`#000000`。
- 所有普通 `QGroupBox::title`：18px、加重 600、`#000000`。
- 右侧五页分组标题不再单独使用 14px 和 `#344A5B`。
- 生产统计外层标题：18px、加重 600、`#000000`。
- 四个统计小卡标题：18px、加重 600、`#000000`。

### 7.2 标签、说明和提示

- 普通字段标签全部继承 18px、常规 400、`#000000`。
- `label_currentEditTemplate`、`label_plcConnectionSection`、`label_plcProcessParametersSection`、字符示意图标题等真正的小标题：18px、加重 600、`#000000`。
- `label_characterDrawHint`、`label_characterNameHint`、`label_hint`、`label_templateSelectionDescription` 和单位：18px、常规 400、`#000000`。
- `label_characterPreviewEmpty` 的无图空提示：18px、常规 400、`#909DA8`。
- 错误提示：18px、常规 400、`#C13F3F`。
- 不再为普通表单标签、说明或单位设置另一种中性文字色。

### 7.3 按钮、导航和输入

- 普通按钮与八个主控按钮：18px、加重 600、`#000000`。
- 右侧导航：18px、加重 600、默认 `#000000`，Hover/Checked 为 `#1769AA`。
- 输入框、下拉框、文本框、复选框以及列表、树、表格内容：18px、常规 400、`#000000`。
- 只读输入值：18px、常规 400、`#000000`。
- 表头：18px、加重 600、`#000000`。
- 用 `QAbstractItemView` 统一代替只针对 `QTreeWidget/QListWidget/QTableWidget` 的重复文字与选择规则；ComboBox 弹出项、模板树和非原生目录选择框使用同一映射。
- `QDialogButtonBox`、`QMessageBox` 和 `QFileDialog` 内部按钮继续继承普通 `QPushButton` 的 18px、加重 600 规则，不建立对话框专用按钮样式。
- 静态和动态 Tooltip：18px、常规 400、`#FFFFFF`。
- `checkBox_hardwareTriggerEnabled` 作为重要开关保留 18px、加重 600，但不新增专用颜色。

### 7.4 识别、统计和状态

- `label_recognitionText`：26px、加重 600、`#1769AA`。
- `lineEdit_currentTemplateName`：26px、加重 600、`#000000`。
- 四个统计值：26px、加重 600，并按总数绿、NG 红、耗时蓝、通过率纯黑显示。
- `label_runtimeStatus`：26px、加重 600，颜色继续由 `uiState` 映射到普通黑色、成功、主操作、警告或危险。
- `textEdit_targetText`：26px、加重 600、`#000000`。
- `label_verdictResult`：Idle/OK/NG 使用 48px、加重 600。

### 7.5 模板制作向导

- 标题：26px、加重 600、`#1769AA`。
- 正文：18px、常规 400、`#000000`。
- 保留 `emphasizedDrawingRegion()` 中四个颜色分支和 `<b><span style="color:...">`，区域名称继续使用当前局部强调色和加重显示。
- `label_templateGuideBody` 继续使用 `Qt::RichText`。
- 保留区域名称和模板名称现有的 `toHtmlEscaped()`，避免动态内容被解释为 HTML。
- 步骤编号、Esc 提示、区域名称内容、现有括号形式和颜色映射全部保持不变。

### 7.6 动态控件、模型项和运行时文字

- `MainWindow`、`InspectionPage`、`MachineSettingsPage` 和 `TemplateEditorPage` 在运行时写入的状态、判定、统计、模板名、按钮文字、字段值、未应用标记、说明和错误信息，全部按目标控件的既有对象名或控件类型继承本方案，不修改文字生产逻辑。
- 字符模板对话框动态创建的序号、字符名称输入、保存名称和预览文件名使用普通黑色，校验错误使用危险色，空状态使用禁用文字色；动态 QWidget 不新增局部样式。
- 模板选择树的动态模板名称、完整路径和状态使用 18px/400；四列表头使用 18px/600；选中和勾选不产生新的文字色。
- `.ui` 中的 ComboBox 项、运行时模型项和 Qt 翻译器生成的标准按钮文字按承载控件统一，不逐字符串建立选择器。
- 静态与动态 Tooltip、Placeholder、只读路径和禁用原因全部纳入人工验证；只改变显示样式，不改变文字内容或产生条件。

### 7.7 字符框选画布中的 UI 文字

- `CharacterCropLabel` 无图时显示的“没有可显示的喷码区域图像”属于 UI 空提示；保留现有 `painter.setPen(Qt::gray)`，不增加 QSS 颜色选择器或 `QPalette` 读取。
- 在 `QPainter` 创建后设置一次 `painter.setFont(font())`，使空提示和字符框序号都使用控件继承的 18px/400 字体，不另设字号或字重。
- 空提示的 `Qt::gray`、字符框线和序号的蓝色、绘制中框线的橙色及其他几何颜色全部保持不变。它们属于既有自绘或图像操作标注色，不计入七个通用文字角色色或模板向导四个强调色。
- 不修改字符框坐标、编号顺序、绘制宽度、撤销、清空、预览或保存行为。

### 7.8 消息框和目录选择框

- 把现有 QSS 加载代码从 `MainWindow::initStyle()` 移到 `ApplicationStartup::run()`。
- 加载位置为 `QApplication` 与翻译器安装完成之后、`RuntimeGuard::check()` 和任何 `QMessageBox` 之前。
- 删除 `MainWindow` 中的 `initStyle()` 调用、声明和定义，不保留第二个入口。
- 删除零调用的 `showParameterInfoWithRedWarning()` 声明和定义，同时删除其中 `Qt::RichText`、`<div>`、`<p>`、`<strong>` 和只为该 HTML 存在的转义拼接；不建立替代包装器。
- 沿用现有资源打开失败日志，不新增回退主题、系统字体覆盖或重复加载。
- 删除零调用富文本函数后剩余的 66 个消息框创建位置，其正文和按钮均由应用级 QSS 统一；不逐调用点修改标题、正文或按钮文案。
- 四个现有目录选择框均保持 `DontUseNativeDialog`，其标签、路径输入、树/列表和按钮使用统一 Qt 控件规则。
- Windows 原生非客户区标题栏仍由操作系统绘制；`windowTitle` 内容保持，但不通过无边框窗口重做标题栏。

## 8. 文件修改范围

| 文件 | 计划修改 |
|---|---|
| `app/resource/qss/app_theme.qss` | 收口为三个字号、两个字重和七个通用文字颜色；普通主文字和次文字统一为 `#000000`；`#0E568E` 只保留为非文字按下背景/边框色；用 `QAbstractItemView` 覆盖主程序视图、ComboBox 弹出项和非原生目录选择框；明确 Tooltip、选择文字及各控件和状态映射 |
| `app/ui/main_window/template/character_editor/character_crop_label.cpp` | 只增加一次 `QPainter::setFont(font())`，使两处自绘文字继承控件字体；现有 `Qt::gray`、绘图标注色和行为不变 |
| `app/startup/application_startup.cpp` | 在首个消息框之前加载唯一正式 QSS |
| `app/ui/main_window/main_window.cpp` | 删除构造函数中的 `initStyle()` 调用 |
| `app/ui/main_window/main_window.h` | 删除 `initStyle()` 声明和零调用的 `showParameterInfoWithRedWarning()` 声明 |
| `app/ui/main_window/main_window_inspection.cpp` | 删除迁移后的 `initStyle()` 定义；删除零调用富文本消息框辅助函数及其 HTML 拼接 |
| `app/resource/README.md` | 更新正式主题的文字层级和维护边界 |
| `app/ui/README.md` | 更新三档字号、两档字重、文字颜色和启动加载入口说明 |
| `docs/development/OCRGangYin工业视觉风格UI改造方案.md` | 同步文字方案覆盖关系和最终门禁 |
| `docs/development/OCRGangYin计划索引.md` | 更新本方案状态与进度 |

九个 `.ui` 默认零修改。若实施中发现 `.ui` 仍存在字体、调色板或局部样式，只删除这些视觉覆盖，不改变控件、文字、布局或对象名。

以下动态文字生产文件属于审计和人工验证范围，但预计保持零差异：

```text
app/ui/main_window/inspection/inspection_page.cpp
app/ui/main_window/main_window_settings.cpp
app/ui/main_window/operation_ui_policy.cpp
app/ui/main_window/settings/machine_settings_page.cpp
app/ui/main_window/template/template_editor_page.cpp
app/ui/main_window/template/character_editor/character_template_editor_dialog.cpp
app/ui/main_window/template/save/template_save_dialog.cpp
app/ui/main_window/template/selection/template_selection_dialog.cpp
```

`main_window.cpp`、`main_window_inspection.cpp` 和 `application_startup.cpp` 同时承担文字生产，但只执行表中已列的必要样式入口清理，不改其余文字内容和产生条件。`template_editor_page.cpp` 保留现有模板向导 RichText 和四种区域强调色，本轮保持零差异。

## 9. 实施阶段

### 阶段 TXT-0：冻结基线

1. 记录分支、HEAD、工作区和暂存区。
2. 保存计划内文件实施前差异，保护现有工业视觉与资源整理改动。
3. 再次确认九个 `.ui` 没有局部字体和颜色。
4. 复核 12 个动态文字生产/绘制文件、67 个消息框创建位置、4 个非原生目录选择框和 2 个 `drawText()` 入口，确认没有新增 UI 文字来源。

### 阶段 TXT-1：QSS 文字体系收口

1. 先统一根字体族、18px/常规 400 和纯黑色普通文字基线。
2. 按第 5 节只保留 18、26、48px。
3. 只保留常规 400、加重 600。
4. 按第 6 节把原主文字、次文字和其他中性文字色并入 `#000000`，同时统一状态颜色。
5. 用 `QAbstractItemView` 统一输入弹出视图、列表、树、表格和非原生目录选择框的选中文字颜色；删除被它替代的重复具体类文字规则。
6. 明确 Tooltip 和标准对话框内部控件的继承结果。
7. 不调整背景、边框、圆角、间距、尺寸或图标。

### 阶段 TXT-2：C++ 冗余旁路与自绘字体清理

1. `template_editor_page.cpp` 保持零差异，保留现有 `Qt::RichText`、`emphasizedDrawingRegion()`、四个区域强调色、`<b><span>` 和必要的 `toHtmlEscaped()`。
2. 删除零调用的 `showParameterInfoWithRedWarning()` 声明、定义和 `<strong>` 富文本拼接，不增加替代包装器。
3. `CharacterCropLabel` 只增加一次 `painter.setFont(font())`，让空提示和字符框序号继承控件字体；保留空提示的 `Qt::gray` 以及既有蓝色、橙色标注色，不增加调色板读取或其他样式逻辑。

### 阶段 TXT-3：正式 QSS 提前加载

1. 把现有加载逻辑整体迁移到启动层。
2. 确认全应用仍只调用一次 `setStyleSheet()`。
3. 删除 MainWindow 的旧入口，不建立包装器或兼容调用。

### 阶段 TXT-4：静态门禁与用户验证

1. 执行第 11 节静态门禁。
2. Agent 不将静态检查表述为视觉运行验证。
3. 用户执行 Run qmake、Rebuild 和第 12 节人工验证。

## 10. 明确禁止

- 不把所有文字改为相同字号或字重。
- 不增加 12px、13px、14px、15px、16px、17px、20px、22px、24px、28px、30px、32px、42px、46px 等新旧中间档。
- 不增加 500、700、800、900 等字重。
- 不在 `.ui` 写 `font`、`palette`、`foreground`、`background` 或局部 `styleSheet`。
- 除模板向导现有四个区域强调色及其 `<b><span>`、字符框和检测 Overlay 的既有图像标注色外，不在 C++ 新增颜色、字体、字号、字重、HTML 字体标签或控件样式。
- `CharacterCropLabel` 唯一允许新增的样式代码是 `QPainter::setFont(font())`；现有 `Qt::gray` 和图像标注色保持原样，不新增字体值、颜色或调色板逻辑。
- 不新增 `textRole`、`fontRole` 等动态属性，不建立 TypographyManager、ThemeManager、字体表、颜色转换器或运行时缩放逻辑。
- 不保留旧颜色别名、旧 QSS 块、第二套加载入口、兼容分支或回退主题。
- 不借文字统一修改控件高度、布局、文案、翻译、状态名、信号槽或业务逻辑。
- 不逐一改写现有动态 `setText()`、模型项、Tooltip 或消息框调用点；它们通过承载控件和应用级 QSS 统一。
- 不修改 `tools/license_tool` 或 `tools/result_receiver`；二者是独立程序，拥有独立 `.ui`、启动入口和验证范围。
- 不改造 Windows 原生标题栏，不建立无边框窗口或自绘标题栏。
- 不修改 `InspectionImageCanvas` 的矩形、多边形、二维码框等 Overlay；字符框序号和框线颜色继续视为同一图像标注。
- 不把日志、CSV、配置文件、错误码或非 UI 数据文本纳入字体方案。

## 11. 静态门禁

1. 正式 QSS 中 `font-family` 只出现一次，值保持第 4 节字体栈。
2. 所有 `font-size` 值的唯一集合严格为 `18px/26px/48px`。
3. 所有 `font-weight` 值的唯一集合严格为常规 `400`、加重 `600`。
4. 正式 QSS 中所有显式定义的文字 `color` 和 `selection-color` 只使用第 6.1 节七个通用颜色；Qt 5.15.2 `QPalette::PlaceholderText` 使用默认框架呈现，不新增 `setPalette()` 或显式 Placeholder 颜色。
5. `#21303D`、`#687887` 及第 2 节列出的其他中性颜色不再作为文字色出现；普通正文、说明、单位、只读值、导航默认文字和普通标题统一为 `#000000`。
6. `#0E568E` 不再出现在 `color` 或 `selection-color`，只允许继续用于既有非文字背景和边框属性。
7. `QAbstractItemView` 统一定义普通文字和选择文字；不再用三组具体 Widget 规则遗漏 `QTreeView/QListView`。
8. `QToolTip` 显式映射到 18px/400 和反白文字；`QDialogButtonBox`、`QMessageBox`、`QFileDialog` 不建立另一套字号或字重。
9. `Qt::RichText`、`<b><span style="color:...">` 和内联文字颜色只允许保留在 `template_editor_page.cpp` 的现有模板向导区域强调路径；`app/ui` 与 `app/startup` 的其他路径中 `<strong>`、`<font>` 及内联字体/文字颜色 HTML 零引用。
10. `emphasizedDrawingRegion()`、`#157A3D`、`#0067C0`、`#B85C00`、`#C43D24` 和区域名称所需 `toHtmlEscaped()` 保持原值、原位置和原行为；`showParameterInfoWithRedWarning()` 声明、定义和调用均为零引用。
11. 九个 `.ui` 中 `font/styleSheet/palette/foreground/background/brush` 视觉属性零新增。
12. 普通生产 C++ 中 `QFont/setFont/setPalette/setTextColor/setForeground` 和 UI 文字颜色字面量零新增；模板向导四个既有强调色保持不变，唯一允许新增的 `QPainter::setFont(font())` 位于 `CharacterCropLabel`，且不包含字号或字重值；该文件不新增 `QPalette` 读取。
13. QSS 不为 `QLabel#label_characterCropCanvas` 新增文字颜色规则；`CharacterCropLabel` 的空提示继续使用现有 `Qt::gray`，两处 `drawText()` 均使用继承字体；既有字符框线、序号和绘制中框线颜色保持原值，`InspectionImageCanvas` Overlay 零差异。
14. `qApp->setStyleSheet()` 或等价的 `application.setStyleSheet()` 全应用只出现一次，且位于第一个启动消息框之前。
15. `MainWindow::initStyle` 的声明、定义和调用零引用。
16. 12 个已登记动态文字文件无遗漏；除第 8 节明确列为修改文件者外，其余动态文字生产文件零差异，尤其 `template_editor_page.cpp` 必须保持零差异。
17. 九个 `.ui` 和 `image.qrc` XML 可解析，QSS 花括号平衡。
18. `tools/license_tool`、`tools/result_receiver` 和其他计划外生产文件零差异，`git diff --check` 通过。

## 12. 用户验证

### 12.1 正常文字层级

1. 检查主窗口、五个右侧页面和三个模板对话框。
2. 普通字段标签、输入值、正文、说明、单位和只读值应统一为纯黑色，不再出现同级文字深浅不同。
3. 分组标题、按钮和表头应比正文更醒目，但字号不跳变。
4. 单位、提示和说明与正文使用相同纯黑色，通过常规字重、位置和留白保持信息层级。
5. 检查 ComboBox 弹出项、模板选择树的四列表头与动态内容，确认普通、选中和禁用文字映射一致。

### 12.2 重要信息

1. 识别内容、当前模板和模板向导标题保持明显。
2. 四个统计值、运行状态和目标字符输入保持生产现场可读。
3. OK、NG 和等待检测保持最大层级。

### 12.3 状态颜色

1. Ready/OK 使用同一成功绿。
2. Running、识别内容、选中和 Focus 使用同一主蓝。
3. Stopping/Warning 使用同一警告橙。
4. NG 和错误提示使用同一危险红。
5. Disabled 使用禁用灰色；ReadOnly、辅助说明和普通正文统一使用纯黑色。

### 12.4 启动与对话框

1. 正常启动后抽查信息、警告、错误、询问四类 QMessageBox，确认正文为 18px/400，标准和自定义按钮为 18px/600。
2. 再启动第二个实例，确认“程序运行中避免重复打开”消息框正文和按钮已使用应用统一字体和文字颜色。
3. 检查设置加载失败、RuntimeGuard 失败等首窗前消息框，确认没有回到系统默认应用字体；不把 Windows 原生标题栏纳入判定。
4. 依次检查模板增加目录、模板保存目录、图像保存目录和二维码 CSV 目录四个非原生目录选择框的标签、路径输入、目录树、列表和按钮文字。
5. 检查目录树和列表的选中背景为主蓝、选中文字为白色，不能出现深蓝字叠在蓝色选中背景上。

### 12.5 动态文字和字符框选画布

1. 触发运行状态、等待检测、OK、NG、统计刷新、当前模板切换、按钮状态切换和未应用标记，确认动态文字不会回到其他字号或字重。
2. 逐步完成文字、OCR、二维码、钢印和生产日期区域绘制，确认模板向导的绿色、蓝色、橙褐色和红橙色区域名称仍按原映射加重显示，步骤、标题和普通正文内容不变。
3. 在字符模板对话框分别检查无字符框、已有字符框、进入命名、空名称错误、合法名称、重复名称、保存文件名预览和完整路径 Tooltip。
4. 无喷码区域图像时，画布空提示应使用 18px/400，并保持现有 `Qt::gray` 外观；有图时字符框序号字体应一致，蓝色框线/序号和橙色绘制中框线保持原有标注意义。
5. 检查静态与动态 Tooltip、图像保存路径 Placeholder、只读路径和禁用原因，确认字体统一且没有增加新的样式入口。

### 12.6 缩放与范围保护

1. 在 100%、125%、150% 缩放下检查上述主窗口、页面、对话框和动态状态，确认无截断、重叠或异常换行。
2. 确认文字统一没有改变按钮高度、页面布局、对话框流程、字符框坐标、模板内容、检测逻辑或设置提交行为。
3. 独立授权工具、结果接收工具、Windows 原生标题栏和检测 Overlay 应保持本轮前状态。

## 13. 完成标准

同时满足以下条件才可标记完成：

- 字体族只有一个正式入口。
- 字号只有 18、26、48px 三种。
- 字重只有常规 400、加重 600 两种。
- 通用文字颜色只使用七个固定角色色，普通主文字和原次文字全部统一为 `#000000`；Placeholder 使用 Qt 5.15.2 默认框架呈现。
- 普通正文、辅助文字、单位、只读值和普通标题不再通过不同中性色区分；标题操作、重要信息、数据状态和最终判定层级清楚。
- 九个 `.ui`、运行时动态文字、动态字符编辑内容、模型项、Tooltip、Placeholder、66 个有效消息框入口、四个非原生目录选择框和字符画布 UI 提示均已纳入统一规则与验证。
- 模板向导继续保留现有 RichText、`emphasizedDrawingRegion()`、四种区域强调色、`<b><span>` 和必要的 `toHtmlEscaped()`，其文件保持零差异；除此之外生产 UI 不存在第二条 RichText 字体旁路。
- 列表、树、表格、ComboBox 弹出项和非原生目录选择框统一通过 `QAbstractItemView` 获得普通与选中文字规则。
- 字符画布空提示和字符框序号使用继承字体；空提示保留现有 `Qt::gray`，不增加 QSS 颜色映射或调色板读取，图像标注色和绘制行为不变。
- QSS 在任何启动消息框之前加载，且全应用只加载一次。
- `.ui` 不保存视觉字体值；除模板向导现有局部区域强调和图像标注外，生产 C++ 不保存新增视觉字体或文字颜色值，不存在第二套通用样式入口、兼容层或胶水层。
- 两个独立工具、Windows 原生标题栏、检测 Overlay、日志和数据文件保持计划外零差异。
- 静态门禁通过，用户完成 Qt Creator 构建与人工验证并明确确认通过。
