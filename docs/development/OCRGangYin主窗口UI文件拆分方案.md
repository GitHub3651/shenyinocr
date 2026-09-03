# OCRGangYin 主窗口与模板弹窗 UI 文件拆分方案

## 1. 文档状态

- 编写日期：2026-09-02。
- 当前状态：代码实施和 Agent 静态验收已完成，等待用户在 Qt Creator 执行 Run qmake、构建和人工交互验证。
- 权威范围：将右侧抽屉的五个内容页从当前 `app/ui/main_window.ui` 拆成五个独立 `.ui` 文件；将选择模板、保存模板和分割字符模板三处固定弹窗界面拆成三个独立 `.ui` 文件；把 MainWindow 核心文件以及检测、设置、模板相关 UI 文件统一归入 `app/ui/main_window/` 功能域，保留 `app/ui/README.md` 作为整个 UI 模块说明；将职责不清的 `ImageLabel` 完整改名为 `InspectionImageCanvas`；删除右侧抽屉重复的标题标签和独立收起按钮，以及按技术类型划分的旧目录、单体 UI 结构产生的控件打包、回调包装、手写固定界面和无效防御代码。
- 当前进度：SUI-0 至 SUI-5 已按本文最终结构完成；九个 `.ui`、最终目录、生成 Ui 所有权、直接连接、qmake 清单、正式 QSS、翻译源路径、维护文档和计划删除项已完成静态检查。SUI-6 中由用户执行的 Qt Creator 构建和第 15 节人工交互验证尚未完成。
- 实施顺序：本方案先于 `OCRGangYin主控操作台按钮分组界面优化方案.md` 实施。主控按钮分组不并入本批次。
- 目标行为基线：保护右侧导航、默认展开检测信息、运行成功后收起、Fault 首次展开检测信息、显著判定栏、模板选择、模板保存、字符框选/排序/命名、设置应用、相机、PLC、统计和结果传输行为。

## 2. 替代关系

用户已经明确批准拆分多个 `.ui`，因此本文在本范围内替代以下旧约束：

- `OCRGangYinUI架构精简优化方案.md` 中“保留一个 `main_window.ui`”以及本轮不删除 ViewBindings 的结论。
- `OCRGangYin右侧折叠导航栏界面优化方案.md` 中“五个内容页必须全部写在 `main_window.ui`”的文件归属要求，以及必须保留 `frame_rightDrawerHeader`、`label_rightDrawerTitle`、`toolButton_collapseRightDrawer` 的界面结构要求。右侧抽屉改为由五个可选中导航按钮同时承担打开、切换和再次点击收起操作。
- `OCRGangYin主控操作台按钮分组界面优化方案.md` 中后续继续修改 ViewBindings 的条目；本方案实施后，按钮分组直接修改新的生成 Ui 引用，不恢复 ViewBindings。

上述旧方案中除右侧抽屉标题栏和主图像画布名称外仍然有效的界面结构、对象名、状态规则、业务行为和验收要求继续保留。本文只改变固定界面内容分别存放在哪个 `.ui` 文件、强绑定 UI 文件的目录归属、这些控件在 C++ 中的直接访问方式、模板弹窗固定外壳的所有权、主图像画布的类型/文件/对象名，以及右侧抽屉顶部重复操作区的删除范围。

## 3. 当前代码事实

### 3.1 当前 UI 文件

当前 `app/ui` 只有一个 `.ui` 文件：

```text
app/ui/main_window.ui
```

该文件当前约 2585 行，同时包含：

- 主窗口水平骨架。
- 主控操作台。
- 主图像显示区和模板向导。
- 右侧抽屉标题栏。
- 右侧 `QStackedWidget`。
- 检测信息、参数设定、图像设置、PLC 通讯、软件设置五个完整内容页。
- 右侧五个导航按钮。

问题不是运行性能，而是五个内容页只能藏在 `main_window.ui` 的 `QStackedWidget` 中编辑。Qt Designer 打开主窗口时不方便单独查看、调整和维护每个页面。

### 3.2 当前五页边界

| 当前容器 | 当前内容 | 计划目标文件 |
|---|---|---|
| `page_inspectionInfo` | 运行状态、识别内容、生产统计、当前模板、剔除复位 | `inspection_info_page.ui` |
| `page_detectionSettings` | 当前模板参数、二维码结果传输、模板保存、拍照距离 | `detection_settings_page.ui` |
| `page_imageSettings` | 检测模式、硬触发、图像保存、相机曝光/增益、颜色通道、旋转、纸巾阈值 | `image_settings_page.ui` |
| `page_plcSettings` | PLC 触发模式、连接参数、运行参数 | `plc_settings_page.ui` |
| `page_softwareSettings` | 软件数据目录、清空数据、恢复默认设置 | `software_settings_page.ui` |

### 3.3 当前 C++ 访问方式

当前三个逻辑 Page 并不拥有独立 Ui，而是由 MainWindow 从 `Ui::MainWindow` 中逐个取出控件指针，再打包为：

- `InspectionPageViewBindings`。
- `MachineSettingsPageViewBindings`。
- `TemplateEditorViewBindings`。

MainWindow 还构造：

- `MachineSettingsPage::Callbacks`。
- `TemplateEditorPageCallbacks`。

这形成了大量逐字段赋值、可空控件检查和回调判空。它们是单体 `main_window.ui` 时形成的人工绑定支架；拆分完成后不再保留。

### 3.4 当前自动槽影响

`ui->setupUi(this)` 只会为 `main_window.ui` 中的控件对 MainWindow 执行自动槽连接。五个独立页面在各自根容器上执行 `setupUi()` 后，页面内按钮不会再自动连接到 MainWindow 的 `on_<objectName>_<signal>()`。

因此实施时必须把受影响的页面按钮直接连接到最终处理函数。迁出页面内部的按钮使用 Qt 原生信号槽直接连接，不增加转发 lambda、适配器或中间 Controller；仍留在 `main_window.ui` 的五个导航按钮按第 8.1 节保留绑定固定参数所需的短 lambda。

### 3.5 当前默认页不一致

当前 `main_window.ui` 同时存在以下属性：

- `toolButton_showInspectionInfo.checked = true`。
- 抽屉标题为“检测信息”。
- `stackedWidget_rightDrawer.currentIndex = 1`。

当前五页顺序中索引 0 才是 `page_inspectionInfo`，索引 1 是 `page_detectionSettings`，而构造函数没有在启动末尾再次调用 `setCurrentWidget(page_inspectionInfo)`。因此当前源文件的默认选中按钮和实际页面不一致；标题标签将在本方案中删除，不再参与页面状态表达。

实施拆分时直接把 `main_window.ui` 的 `currentIndex` 改为 0，使其符合用户已经确认的“打开软件默认显示检测信息”要求。不增加启动修正函数、延迟调用或备用状态。

### 3.6 当前模板弹窗

当前模板流程共有三处固定弹窗界面，但都没有 `.ui` 文件：

| 当前入口 | 当前实现 | 拆分后边界 |
|---|---|---|
| 选择模板 | `TemplateSelectionDialog` 在构造函数中手写标题、说明、模板树、操作按钮和按钮盒布局 | 现有弹窗类直接拥有 `template_selection_dialog.ui` |
| 保存模板 | `TemplateEditorPage` 在保存流程中临时创建局部 `QDialog`、输入框、目录行和按钮 | 建立职责明确的 `TemplateSaveDialog`，直接拥有 `template_save_dialog.ui` |
| 分割字符模板 | `CharacterTemplateEditorDialog::buildUi()` 手写双页结构、框选区、预览区、命名区和按钮布局 | 现有弹窗类直接拥有 `character_template_editor_dialog.ui` |

模板树节点、字符命名行和字符预览卡片的数量由运行数据决定，仍由各自弹窗类动态创建。文件夹选择、警告、确认和错误提示继续使用 `QFileDialog`、`QMessageBox`，不为系统标准弹窗创建 `.ui`。

当前字符框选画布 `CharacterTemplateEditorDialog::CropImageLabel` 是定义在 `.cpp` 内的真实交互控件，包含图像绘制、鼠标拖拽和坐标换算。它不能以占位 `QLabel` 加运行时替换的方式接入新 `.ui`；实施时提取为一个可由 Designer 提升使用的正式 `CharacterCropLabel`，不增加空壳包装类。

### 3.7 当前目录和主图像控件

当前五个 MainWindow 核心文件和 UI README 直接位于 `app/ui/`；三个逻辑 Page 位于 `app/ui/pages/`；两个已有模板弹窗类位于 `app/ui/dialogs/`；两个 UI 状态类位于 `app/ui/controllers/`；Fault 呈现类位于 `app/ui/presenters/`；`ImageLabel` 位于 `app/ui/widgets/`。这些目录按控件、页面、控制器、呈现器等技术类型划分，使同一个检测、设置或模板功能的强关联文件分散在多个目录中。

当前 `ImageLabel` 不是普通标签：它同时负责检测图像等比例显示、模板图像显示、鼠标矩形框选、多边形绘制、模板绘制步骤和几何状态。实施时完整改名为：

```text
ui/widgets/image_label.h/.cpp       -> ui/main_window/inspection_image_canvas.h/.cpp
ImageLabel                          -> InspectionImageCanvas
imageLabel_inspection               -> inspectionImageCanvas
```

该重命名只改变 UI 类型、文件、对象名和直接引用，不改变绘图状态、几何数据、信号语义或显示行为。检测层现有 `clearImageLabelRects` 数据字段不属于 UI 类型和目录整理范围，本方案不修改检测合同。

## 4. 最终拆分范围

### 4.1 保留在 `main_window.ui` 的内容

`main_window.ui` 只保留主窗口固定骨架：

```text
MainWindow
└─ horizontalLayout_mainWindow
   ├─ widget_leftPanel
   │  ├─ groupBox_mainControls
   │  └─ groupBox_imageDisplay
   │     ├─ frame_templateGuide
   │     ├─ label_verdictResult
   │     └─ inspectionImageCanvas
   ├─ widget_rightPanel
   │  └─ stackedWidget_rightDrawer
   │     ├─ page_inspectionInfo
   │     ├─ page_detectionSettings
   │     ├─ page_imageSettings
   │     ├─ page_plcSettings
   │     └─ page_softwareSettings
   └─ widget_rightNavigationRail
      ├─ toolButton_showInspectionInfo
      ├─ toolButton_showDetectionSettings
      ├─ toolButton_showImageSettings
      ├─ toolButton_showPlcSettings
      └─ toolButton_showSoftwareSettings
```

五个 `page_*` 仍是 `stackedWidget_rightDrawer` 的固定页面根节点，用于导航切换。它们只保留根容器，不再包含页面内部控件和布局。

### 4.2 新增的五个 `.ui`

```text
app/ui/main_window/inspection/inspection_info_page.ui
app/ui/main_window/settings/detection_settings_page.ui
app/ui/main_window/settings/image_settings_page.ui
app/ui/main_window/settings/plc_settings_page.ui
app/ui/main_window/settings/software_settings_page.ui
```

每个文件的顶层类和生成类型固定为：

| `.ui` 文件 | `<class>` / 生成 Ui 类型 | 装配到现有根节点 |
|---|---|---|
| `inspection_info_page.ui` | `Ui::InspectionInfoPage` | `ui->page_inspectionInfo` |
| `detection_settings_page.ui` | `Ui::DetectionSettingsPage` | `ui->page_detectionSettings` |
| `image_settings_page.ui` | `Ui::ImageSettingsPage` | `ui->page_imageSettings` |
| `plc_settings_page.ui` | `Ui::PlcSettingsPage` | `ui->page_plcSettings` |
| `software_settings_page.ui` | `Ui::SoftwareSettingsPage` | `ui->page_softwareSettings` |

五个文件的顶层控件均使用普通 `QWidget`。不新增五个同名 QWidget C++ 包装类，不使用 Promote Widget，不增加只有构造函数和 `setupUi()` 的空壳 `.h/.cpp`。

### 4.3 新增的三个模板弹窗 `.ui`

```text
app/ui/main_window/template/selection/template_selection_dialog.ui
app/ui/main_window/template/save/template_save_dialog.ui
app/ui/main_window/template/character_editor/character_template_editor_dialog.ui
```

三个文件的顶层控件均为 `QDialog`：

| `.ui` 文件 | `<class>` / 生成 Ui 类型 | 所有者 |
|---|---|---|
| `template_selection_dialog.ui` | `Ui::TemplateSelectionDialog` | 现有 `TemplateSelectionDialog` |
| `template_save_dialog.ui` | `Ui::TemplateSaveDialog` | 新建 `TemplateSaveDialog` |
| `character_template_editor_dialog.ui` | `Ui::CharacterTemplateEditorDialog` | 现有 `CharacterTemplateEditorDialog` |

`TemplateSaveDialog` 是替换当前局部临时对话框的实际交互类，负责模板名称、保存目录、浏览和输入有效性；它不是转发器，也不接触模板服务。`TemplateEditorPage` 仍负责模板几何、覆盖确认、模板创建和保存结果处理。

`character_template_editor_dialog.ui` 静态声明双页 `QStackedWidget`、两个页面、固定提示、滚动区、固定按钮和动态内容布局。字符框选位置使用 Designer 的 Promote to 机制直接创建同目录下的 `CharacterCropLabel`，提升头文件固定为 `ui/main_window/template/character_editor/character_crop_label.h`；不放置临时占位控件，不在运行时删除或替换控件。

### 4.4 删除右侧抽屉标题栏

从 `main_window.ui` 完全删除：

```text
frame_rightDrawerHeader
label_rightDrawerTitle
toolButton_collapseRightDrawer
```

`label_rightDrawerTitle` 与当前选中的导航按钮文字重复，`toolButton_collapseRightDrawer` 与再次点击当前已选中导航按钮的操作重复。父容器 `frame_rightDrawerHeader` 删除两个子控件后没有独立职责，因此同时删除，不保留 48px 空白、空布局或隐藏占位。

删除后 `stackedWidget_rightDrawer` 直接填充 `widget_rightPanel`。五个导航按钮继续使用现有 `checkable` 状态：点击未选中的按钮时打开或切换对应页面，再次点击当前已选中的按钮时调用 `hideRightPanel()` 隐藏整个右侧面板。启动识别成功后的自动隐藏流程同样调用该函数。

不把标题标签或收起按钮复制进五个页面，不新建独立标题栏 `.ui`，也不增加替代收起入口。

### 4.5 不继续拆分的内容

以下区域继续留在 `main_window.ui`：

- `groupBox_mainControls`。
- `groupBox_imageDisplay`。
- 模板制作向导。
- 唯一显著判定栏。
- 五个导航按钮。

这些区域始终和主窗口同时存在，没有独立打开、复用或单独维护的现实需求。继续拆成 `main_controls.ui`、`image_display.ui` 或 `navigation_rail.ui` 只会增加装配代码，不纳入本方案。

以下动态内容和系统弹窗也不拆为 `.ui`：

- 模板树的 `QTreeWidgetItem`。
- 字符命名页按框选数量生成的输入行。
- 字符模板预览区按已有模板数量生成的预览卡片。
- `QMessageBox` 和 `QFileDialog`。

### 4.6 最终目录结构

本方案实施后的 `app/ui` 目录固定为：

```text
app/ui/
├─ README.md
└─ main_window/
   ├─ main_window.ui
   ├─ main_window.h
   ├─ main_window.cpp
   ├─ main_window_inspection.cpp
   ├─ main_window_settings.cpp
   ├─ inspection_image_canvas.h
   ├─ inspection_image_canvas.cpp
   ├─ operation_ui_policy.h
   ├─ operation_ui_policy.cpp
   ├─ inspection/
   │  ├─ inspection_info_page.ui
   │  ├─ inspection_page.h
   │  ├─ inspection_page.cpp
   │  ├─ inspection_fault_presenter.h
   │  └─ inspection_fault_presenter.cpp
   ├─ settings/
   │  ├─ detection_settings_page.ui
   │  ├─ image_settings_page.ui
   │  ├─ plc_settings_page.ui
   │  ├─ software_settings_page.ui
   │  ├─ machine_settings_page.h
   │  ├─ machine_settings_page.cpp
   │  ├─ settings_edit_state.h
   │  └─ settings_edit_state.cpp
   └─ template/
      ├─ template_editor_page.h
      ├─ template_editor_page.cpp
      ├─ selection/
      │  ├─ template_selection_dialog.ui
      │  ├─ template_selection_dialog.h
      │  └─ template_selection_dialog.cpp
      ├─ save/
      │  ├─ template_save_dialog.ui
      │  ├─ template_save_dialog.h
      │  └─ template_save_dialog.cpp
      └─ character_editor/
         ├─ character_template_editor_dialog.ui
         ├─ character_template_editor_dialog.h
         ├─ character_template_editor_dialog.cpp
         ├─ character_crop_label.h
         └─ character_crop_label.cpp
```

`main_window/` 是主窗口完整功能域：根目录只放 MainWindow 骨架、跨检测/模板共用的主图像画布，以及统一控制主控按钮状态的 `OperationUiPolicy`。`inspection/` 收拢检测信息页、检测呈现逻辑和 Fault 呈现；`settings/` 收拢四个设置页、设置逻辑和设置编辑状态；`template/` 收拢模板编辑逻辑及三个模板弹窗。`CharacterCropLabel` 只服务字符模板编辑，因此与字符模板弹窗同目录。`app/ui/README.md` 保持在 UI 根目录，说明整个 UI 模块而不是单独说明 MainWindow。

实施后不再保留 `pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/` 这些按技术类型划分的目录，也不建立 `main_window/widgets/`、`main_window/controllers/` 等替代性技术目录。三个功能子目录只对应当前真实功能边界，不增加页面工厂、通用基类或目录级包装类。

目录迁移不引入新的 C++ 命名空间、不修改现有类名，也不增加目录对应的包装类；唯一的类型改名仍是本方案明确规定的 `ImageLabel` 到 `InspectionImageCanvas`。

## 5. 页面内容迁移规则

### 5.1 原样移动

五个页面内部的以下内容必须从 `main_window.ui` 原样移动到对应新文件；三个模板弹窗当前由 C++ 固定创建的对应属性也按同一规则写入各自 `.ui`：

- 控件类型和 `objectName`。
- 布局类型、层级、边距、间距和拉伸比例。
- 最小/最大尺寸和尺寸策略。
- 初始文本、占位文本、只读状态和默认选项。
- `uiRole`、`uiState`、`hasError` 等动态属性。
- ToolTip、单位、对齐、滚动条策略和可见性。
- `QSpacerItem` 和滚动内容页尺寸。

本批次不顺带修改文字、间距、控件类型、按钮位置、图标或颜色。

### 5.2 唯一实例

迁移后的页面控件只允许在对应独立 `.ui` 中存在一份：

- 不在 `main_window.ui` 保留隐藏副本。
- 不保留空的旧布局、旧 GroupBox 或占位控件。
- 不保留删除标题栏后形成的空 `frame_rightDrawerHeader` 或 48px 顶部间距。
- 不复制同名控件用于兼容旧 `ui->` 访问。
- 不在 C++ 中重新 `new` 同一批固定控件或布局。

`main_window.ui` 中只保留五个必须存在的 `page_*` 页面根节点。这五个根节点是最终结构的一部分，不是兼容占位。

### 5.3 对象名

除主图像画布 `imageLabel_inspection` 明确改名为 `inspectionImageCanvas` 外，页面内部现有业务控件对象名全部保持不变。其他对象名仍被正式 QSS、代码搜索、Qt Designer 和现有功能语义使用，本批次没有继续改名的收益。

新 `.ui` 的顶层 QWidget 使用独立的 Designer 根名称，例如 `InspectionInfoPage`。运行时调用 `setupUi(ui->page_inspectionInfo)` 时，现有 `page_inspectionInfo` 已有对象名，生成代码不会把页面根对象改名。

三个模板弹窗保留当前可见控件的现有 `objectName`；当前没有名称的固定控件只补充与职责对应的唯一名称，不建立旧名称别名。弹窗标题、尺寸、窗口属性、文字、布局和动态属性保持现有行为，不借拆分重新设计视觉。

`main_window.ui` 中主图像画布的提升类、提升头文件和对象名一次性同步为：

```text
InspectionImageCanvas
ui/main_window/inspection_image_canvas.h
inspectionImageCanvas
```

旧类型、旧头文件和旧对象名不保留别名或双路径。

### 5.4 静态外壳与动态内容边界

三个模板弹窗的固定控件、固定布局和固定页面全部写入 `.ui`。C++ 只保留：

- 根据检测模式设置选择模板说明文字。
- 填充、增加、勾选和移除模板树节点。
- 向字符框选画布写入源图和已有框。
- 根据实际字符框数量生成命名行。
- 根据实际字符模板数量生成预览卡片。
- 用户输入校验、文件夹选择、确认提示和业务调用。

不得为了减少动态代码而在 `.ui` 预建固定数量的空模板项、空命名行或空预览卡片。

## 6. UI 装配方式

### 6.1 MainWindow 直接持有生成 Ui

MainWindow 使用 `std::unique_ptr` 私有持有主窗口生成 Ui 和五个页面生成 Ui，不新增中间聚合类：

```cpp
namespace Ui {
class MainWindow;
class InspectionInfoPage;
class DetectionSettingsPage;
class ImageSettingsPage;
class PlcSettingsPage;
class SoftwareSettingsPage;
}
```

现有 `Ui::MainWindow *ui` 直接改为 `std::unique_ptr<Ui::MainWindow> ui`，五个页面生成 Ui 也分别由一个 `std::unique_ptr<Ui::...>` 持有。仍属于 `main_window.ui` 的控件继续使用现有 `ui->控件` 访问方式；迁入独立页面的控件改用对应页面生成 Ui。生成 Ui 只保存控件地址，实际 QWidget 仍由 Qt 父子对象树统一销毁；不存在第二套控件所有权。

项目当前使用 C++11，因此构造函数初始化继续使用 `ui(new Ui::MainWindow)` 以及相同形式的五个页面 Ui 初始化，不为使用 `std::make_unique` 升级语言标准，也不增加工厂函数。

MainWindow 成员按依赖关系声明：主窗口生成 Ui 在前，三个 Page 使用的服务、状态和 `m_faultAlarmPresented` 位于三个 Page 成员之前，五个页面生成 Ui 位于三个 Page 成员之前，三个逻辑 Page 的 `std::unique_ptr` 位于这些依赖之后。关键相对顺序固定为：

```text
std::unique_ptr<Ui::MainWindow> ui
服务、SettingsEditState、m_faultAlarmPresented
五个页面生成 Ui 的 std::unique_ptr
InspectionPage / MachineSettingsPage / TemplateEditorPage 的 std::unique_ptr
```

C++ 按成员声明的相反顺序析构，因此三个逻辑 Page 自动先销毁，随后销毁五个页面生成 Ui，再销毁它们使用的状态和服务，最后销毁 `Ui::MainWindow`。不手工 `reset()` Page，不手工 `delete ui`，不增加生命周期管理器或析构回调。

`MachineSettingsPage` 和 `TemplateEditorPage` 虽然继承 `QObject`，但都只由 MainWindow 对应的 `std::unique_ptr` 唯一拥有。两者构造函数删除当前未使用的 `QObject *parent = nullptr` 参数，实现中删除 `QObject(parent)`，让 `QObject` 基类默认构造；不得再调用 `setParent()`，也不得在创建时传入 MainWindow 作为 QObject 父对象。这样只保留 `std::unique_ptr` 一条对象所有权路径。模板弹窗的 `QWidget *parent` 用于窗口归属，不属于此项删除范围。

`MainWindow` 继续在头文件声明析构函数，并在已经包含完整 `ui_main_window.h` 的 `main_window.cpp` 中定义，因此 `std::unique_ptr<Ui::MainWindow>` 会在类型完整的位置析构；不需要在头文件包含生成 Ui，也不增加额外析构辅助函数。

### 6.2 固定初始化顺序

构造顺序固定为：

```text
1. ui->setupUi(this)
2. InspectionInfoPage::setupUi(ui->page_inspectionInfo)
3. DetectionSettingsPage::setupUi(ui->page_detectionSettings)
4. ImageSettingsPage::setupUi(ui->page_imageSettings)
5. PlcSettingsPage::setupUi(ui->page_plcSettings)
6. SoftwareSettingsPage::setupUi(ui->page_softwareSettings)
7. 初始化页面数据、样式、Page 逻辑和信号槽
```

所有页面在 MainWindow 第一次访问 `resultExportIp`、`comboBox_detectionMode`、`label_runtimeStatus` 等控件之前完成 `setupUi()`。不按导航点击延迟加载，不保存“是否已加载”状态，也不增加页面加载失败回退。

MainWindow 析构函数体只保留停止模板制作预览和关闭应用服务等真实业务清理，直接调用最终 Page 和服务方法；不负责释放任何生成 Ui 或逻辑 Page。析构函数体结束后，三个逻辑 Page、五个页面生成 Ui 和主窗口生成 Ui 按第 6.1 节的成员顺序自动析构，随后由 QWidget 父子对象树销毁实际控件。删除当前三个 Page 的手工 `reset()`、`delete ui` 和 `ui = nullptr`，不得以其他手工释放代码替代。

### 6.3 这不是运行时动态拼界面

固定控件、布局和属性全部由五个 `.ui` 文件生成；手写 C++ 只对五个现有页面根节点各调用一次生成的 `setupUi()`。禁止出现：

- 手写 `new QLabel/QPushButton/QLayout/QScrollArea` 来还原页面。
- `QUiLoader` 和外部 `.ui` 运行时文件加载。
- `findChild()` 按对象名重新寻找控件。
- 页面工厂、注册表、通用加载器或反射式装配。

### 6.4 模板弹窗直接拥有生成 Ui

三个模板弹窗各自使用 `std::unique_ptr` 只拥有自己的生成 Ui，并在构造函数开始处调用一次 `setupUi(this)`。生成 Ui 不交给 MainWindow、不打包进 ViewBindings，也不通过 getter 暴露整套控件。主窗口、五个页面和三个弹窗共九个生成 Ui 的所有权全部使用 `std::unique_ptr`，不混用裸指针和智能指针。

三个弹窗头文件只前置声明各自的 `Ui::...` 类型，因此必须分别显式声明析构函数，并在包含对应 `ui_*.h` 的 `.cpp` 中使用 `= default` 定义：

```cpp
~TemplateSelectionDialog() override;
~TemplateSaveDialog() override;
~CharacterTemplateEditorDialog() override;
```

这些析构函数只保证 `std::unique_ptr<Ui::...>` 在生成 Ui 类型完整的位置析构，不增加清理逻辑、兼容分支或生命周期管理层。

```text
TemplateSelectionDialog        -> Ui::TemplateSelectionDialog
TemplateSaveDialog             -> Ui::TemplateSaveDialog
CharacterTemplateEditorDialog  -> Ui::CharacterTemplateEditorDialog
```

现有两个弹窗类保留；当前内联保存模板对话框由 `TemplateSaveDialog` 直接替换。禁止再增加 DialogFactory、DialogBindings、通用弹窗基类或只负责转发调用的中间类。

## 7. C++ 最终访问方式

### 7.1 删除 ViewBindings

三个现有逻辑 Page 直接接收其实际使用的生成 Ui 引用：

```text
InspectionPage
    ├─ QWidget&（MainWindow 窗口对象）
    ├─ Ui::MainWindow&
    └─ Ui::InspectionInfoPage&

MachineSettingsPage
    ├─ Ui::DetectionSettingsPage&
    ├─ Ui::ImageSettingsPage&
    ├─ Ui::PlcSettingsPage&
    └─ Ui::SoftwareSettingsPage&

TemplateEditorPage
    ├─ QWidget&（弹窗父窗口）
    ├─ Ui::MainWindow&
    ├─ Ui::DetectionSettingsPage&
    └─ Ui::ImageSettingsPage&
```

使用引用而不是可空控件指针。实现文件通过生成 Ui 类型直接访问控件，例如：

```cpp
m_inspectionInfoUi.label_runtimeStatus
m_detectionSettingsUi.textEdit_targetText
m_imageSettingsUi.comboBox_detectionMode
m_plcSettingsUi.lineEdit_plcIpAddress
m_softwareSettingsUi.pushButton_clearSoftwareData
```

不增加控件 getter、View 接口、Accessor、Facade 或新的 Bindings 结构。`InspectionPage` 和 `TemplateEditorPage` 所需的窗口对象都由 MainWindow 构造时以 `QWidget &` 明确传入；不得通过 `parent()`、`window()`、`qobject_cast`、`findChild()` 或生成 Ui 反向查找窗口对象。

当前保存模板按钮虽然位于参数设定页，但启用状态仍由 `InspectionPage::applyOperationState()` 跨页设置。拆分后必须把 `operationUi.saveTemplate` 的应用移动到 `TemplateEditorPage::applyOperationState()`，由其通过已经持有的 `Ui::DetectionSettingsPage &` 直接设置保存模板按钮；`InspectionPage` 中对应访问和代码完全删除。不得为了保留旧位置给 `InspectionPage` 增加 `Ui::DetectionSettingsPage &`，也不得新增状态转发函数。

### 7.2 服务和状态依赖

`MachineSettingsPage`、`TemplateEditorPage` 和 `TemplateSelectionDialog` 使用的正式服务，以及两个 Page 使用的 `SettingsEditState`，都由当前项目装配保证存在。实施时统一改为引用并直接保存，删除空指针分支和默认值回退。尤其必须删除 `TemplateSelectionDialog` 中 `m_templateService ? ... : TemplateSummary()` 和 `if (!m_settingsService)` 路径，不保留无服务备用结果。

`MachineSettingsPage` 和 `TemplateEditorPage` 的 QObject 父对象不是业务依赖，也不是生命周期依赖。两者构造函数不再接收 `QObject *parent`，实现中不保留 `QObject(parent)` 初始化；MainWindow 只通过各自的 `std::unique_ptr` 管理其生命周期，不同时使用 QObject 父子对象树管理这两个逻辑 Page。

`InspectionPage` 的 MainWindow 窗口对象同样是必需依赖，构造参数和成员由 `QWidget *` 改为 `QWidget &`。模板提示计时器和开关改为 `InspectionPage` 自有的值成员：

```cpp
QTimer m_templateAttentionTimer;
bool m_templateAttentionOn = false;
```

`presentFault()` 继续复用 MainWindow 唯一的 `m_faultAlarmPresented`，但参数改为必需引用：

```cpp
void presentFault(
    const InspectionFaultSnapshot &snapshot,
    bool &alarmPresented);
```

因此删除 `if (!m_rootWidget)`、外部计时器判空和外部布尔指针是否存在的判断，但保留 Fault 首次提示这一业务条件。当前 `if (alarmPresented && !*alarmPresented)` 在改为引用后必须准确变为 `if (!alarmPresented)`，并在首次提示前把该引用设为 `true`；不得把整个条件删除，也不在 `InspectionPage` 内复制第二份 Fault 是否已提示状态。

`TemplateEditorPage` 删除 `TemplateEditorViewBindings::parentWidget` 后，直接保存构造时传入的 `QWidget &` 作为所有模板弹窗和消息框的父窗口。删除仅返回旧指针的 `dialogParent()`；不得用父对象遍历、类型转换或空父窗口作为替代路径。

真正的业务输入校验和失败处理继续保留，例如：

- 用户输入的数值格式与范围。
- 模板几何完整性。
- 模板、设置、相机、PLC 和文件操作返回的失败。
- 运行时快照、Fault 和图片有效性。

只删除对项目自身固定创建的 Ui 控件和必需服务的“可能为空”防护，不删除业务规则。

### 7.3 页面内重复别名

生成 Ui 已经提供明确成员后，不再额外保存同一控件的第二份成员指针。实施时删除仅用于别名的成员，例如：

- MainWindow 的 `imageLabel`。
- MainWindow 的 `m_softwareDataDirLineEdit`。
- TemplateEditorPage 中与生成 Ui 成员重复的当前模板控件、向导控件和字符模板按钮指针。

需要访问控件时直接使用对应生成 Ui 成员。

### 7.4 弹窗控件直接访问

`TemplateSelectionDialog` 删除 `m_tree`、`m_removeCheckedButton` 等固定控件别名，直接使用自己的生成 Ui。`CharacterTemplateEditorDialog` 删除 `m_cropLabel`、`m_drawPage`、`m_namePage`、`m_stack`、`m_nameListLayout`、`m_previewScrollArea`、`m_previewGrid` 等固定控件或布局别名，直接使用自己的生成 Ui。

字符命名输入框、错误标签和保存名标签是按实际框选数量创建的动态对象，其列表成员继续保留。它们不是固定 Ui 控件别名。

`CharacterCropLabel` 是包含既有绘制、鼠标框选和坐标换算行为的正式控件，必须声明 `Q_OBJECT` 和唯一的 `itemsChanged()` 信号。它直接复用现有 `TemplateCharacterBox` 表达框选矩形和名称，不再保留仅供嵌套类使用的重复 `CharacterBox` 结构，也不保留 `std::function` 回调备用路径。

## 8. 信号槽处理

### 8.1 主窗口内控件

继续位于 `main_window.ui` 且由 MainWindow 自身包含实际业务逻辑的控件保持现有连接方式：

- 打开/关闭相机。
- 启动/停止识别。
- 右侧五个导航按钮。

本批次不调整这些按钮的业务处理和布局。

制作模板按钮虽然继续位于 `main_window.ui`，但当前 `MainWindow::on_toolButton_createTemplate_clicked()` 只判空并转发到 `TemplateEditorPage::handleTemplateCaptureButton()`，没有独立业务逻辑。实施时删除该 MainWindow 中转槽，在 `TemplateEditorPage` 创建完成后把 `toolButton_createTemplate::clicked` 直接连接到 `TemplateEditorPage::handleTemplateCaptureButton()`。连接表达式同时包含控件名和最终处理方法，仍可直接搜索调用链；不保留自动槽和显式连接双路径。

右侧五个导航按钮继续通过现有五个短 lambda，把各自固定的页面根节点和按钮传给 `showRightPanelPage()`。这些 lambda 只存在于对应 `connect()` 表达式中，不保存回调、不转发到另一层对象，是绑定 `clicked` 信号缺少的固定参数所需的最终直接连接，不属于本方案删除的 Callback 胶水。不得为了消除这五个 lambda 新增五个一行槽函数、按钮映射结构、路由器或导航类。

目标按钮在点击后变为未选中时，`showRightPanelPage()` 直接调用 `hideRightPanel()`；变为选中时切换页面并显示右侧面板。删除标题标签后，`showRightPanelPage()` 不再更新标题文字。

### 8.2 独立页面内控件

拆出后的控件直接连接到最终负责逻辑，不使用只调用下一层的一行 lambda。

五个页面生成 Ui 完成 `setupUi()` 后，MainWindow 必须对仍由自身处理的迁出按钮逐一使用函数指针建立直接连接。不能继续依赖 `on_<objectName>_<signal>()` 自动发现，因为页面生成代码的 `connectSlotsByName()` 作用于各自的 `page_*` 根 QWidget，而不是 MainWindow；也不得再次对 MainWindow 整体调用 `connectSlotsByName(this)`，以免仍留在 `main_window.ui` 的控件产生重复连接。现有 MainWindow 命名槽只要包含实际业务逻辑即可保留并作为显式连接的最终接收者。

`TemplateEditorPage` 和 `MachineSettingsPage` 自己负责的页面控件，由对应 Page 在构造时直接连接自己的最终处理方法。所有迁出按钮只建立一条最终连接；不新增统一连接器、控件路由表、转发槽或批量映射结构。

| 页面 | 控件范围 | 最终处理位置 |
|---|---|---|
| 检测信息 | 总数清零、NG 清零、剔除复位 | 现有 MainWindow 对应命名槽 |
| 参数设定 | 模板参数应用、模板保存、字符模板编辑 | `TemplateEditorPage` 现有方法 |
| 参数设定 | 结果传输启用、连接、断开、拍照距离应用 | 现有 MainWindow 对应命名槽 |
| 图像设置 | 保存路径、曝光、增益、颜色通道、旋转 | 现有 MainWindow 对应命名槽 |
| 图像设置 | 纸巾阈值 | `MachineSettingsPage` 现有处理 |
| PLC 通讯 | 触发模式、连接、断开、运行参数应用 | 现有 MainWindow 对应命名槽 |
| 软件设置 | 清空软件数据、恢复默认设置 | 现有 MainWindow 方法 |

保留有实际逻辑的命名槽，便于按控件名搜索实现。只删除无业务内容的中转槽或中转 lambda。

### 8.3 删除 Callback 包装

`MachineSettingsPage::Callbacks` 中同一页面内的显示更新直接移入 `MachineSettingsPage`：

- 图像保存选项可见性。
- 保存目录文字和 ToolTip。
- 软件数据目录打开失败提示。

检测模式引起的跨区域可见性仍由 MainWindow 现有 `updateTissueRoughnessUiVisibility()` 直接协调，并在设置应用完成后明确调用；不再由回调绕回 MainWindow。

`TemplateEditorPageCallbacks` 删除后，只保留两个必要的 Qt 语义信号：

```text
TemplateEditorPage::operationUiRefreshRequested()
TemplateEditorPage::previewFramePresentationRequested(const cv::Mat &)
```

MainWindow 分别直接连接到现有状态刷新逻辑和预览显示逻辑。连接使用函数指针，不增加转发 lambda。`TemplateEditorPage` 增加 `Q_OBJECT` 以声明这两个信号，不新增事件总线或通用通知框架。

原 `slot_displayAndDetect(cv::Mat *)` 只被模板预览回调使用，实施时改为接收 `const cv::Mat &` 的明确预览呈现方法；旧指针槽直接删除，不保留重载或兼容入口。

### 8.4 模板弹窗信号槽

- `TemplateSelectionDialog` 的增加、移除、确认应用和取消按钮直接连接到该弹窗的实际处理方法。
- `CharacterCropLabel` 用一个 `itemsChanged()` Qt 信号通知 `CharacterTemplateEditorDialog` 刷新预览，删除当前 `std::function<void()>` 回调保存和判空调用。
- `CharacterTemplateEditorDialog` 的撤销、清空、下一步、返回、保存和取消按钮直接连接到实际处理方法或最终对象，不经过 MainWindow 和 `TemplateEditorPage` 转发。
- `TemplateSaveDialog` 自己处理浏览目录、名称启用状态、名称/目录校验、接受和取消；`TemplateEditorPage` 只执行弹窗并读取已确认的模板名称和父目录。

弹窗内包含实际逻辑的方法可以保留或使用明确名称，不为连接按钮新增只调用另一个方法的一行中转槽。

## 9. 模板按钮闪烁状态收口

当前模板制作按钮闪烁由 MainWindow 创建 `QTimer`，再把计时器和 `bool*` 传给 `InspectionPage`。这是单体 UI 绑定遗留。

实施时由 `InspectionPage` 直接持有模板按钮闪烁计时器和布尔状态：

- `applyOperationState()` 进入模板预览状态时启动计时器。
- 离开模板预览状态时停止计时器并清除按钮临时属性。
- 计时器触发时只切换按钮的 `uiState` 和刷新样式。

MainWindow 中对应计时器、布尔成员、构造连接、关闭时停止代码以及构造参数全部删除。不新增第二份操作状态；是否启动和停止仍由传入的唯一 `OperationUiState` 决定。

## 10. 必须完全删除的旧代码

### 10.1 旧 UI 内容

从 `main_window.ui` 完全删除五个 `page_*` 根节点内部的旧控件和布局定义。迁入新文件后不得在主窗口 UI 中保留副本、注释块、隐藏容器或空旧布局。

### 10.2 ViewBindings 与构造函数

生产代码中以下符号实施后必须零引用：

```text
InspectionPageViewBindings
MachineSettingsPageViewBindings
TemplateEditorViewBindings
MainWindow::inspectionPageViewBindings()
MainWindow::machineSettingsPageViewBindings()
MainWindow::templateEditorViewBindings()
```

三个 Page 的 `m_view` 成员和逐控件赋值代码一并删除。

### 10.3 Callback 胶水

以下符号实施后必须零引用：

```text
MachineSettingsPage::Callbacks
TemplateEditorPageCallbacks
MainWindow::machineSettingsPageCallbacks()
MainWindow::templateEditorPageCallbacks()
```

对应 `std::function` 成员、构造参数、判空调用和只转发 lambda 一并删除。若某文件不再因其他业务使用 `<functional>`，同时删除无用 include。

### 10.4 重复成员和设置函数

以下旧成员或函数在新 Ui 直接访问成立后删除：

```text
MainWindow::imageLabel
MainWindow::m_softwareDataDirLineEdit
MachineSettingsPage::setSoftwareDataDirectoryEditor()
MainWindow::m_templateCaptureAttentionTimer
MainWindow::m_templateCaptureAttentionOn
MainWindow::slot_displayAndDetect(cv::Mat *)
MainWindow::on_toolButton_createTemplate_clicked()
MainWindow::resetTemplateCaptureState()
MainWindow::confirmInspectionFaultRecovery()
```

TemplateEditorPage 内与生成 Ui 控件重复的别名成员也全部删除，不保留一套 `m_xxxButton` 和一套 `m_pageUi.xxxButton` 并存。

上述三个 MainWindow 方法在当前代码中只调用 `TemplateEditorPage` 或 `InspectionPage` 的同一最终方法，没有额外状态计算或跨区域协调。它们的调用点改为直接调用最终 Page 方法；制作模板按钮按第 8.1 节直接连接最终处理方法。不保留同名转发、旧槽或兼容入口。

MainWindow 的生成 Ui 所有权同时一次性改为最终形式：

```text
删除：Ui::MainWindow *ui
保留：std::unique_ptr<Ui::MainWindow> ui
删除：m_templateEditorPage.reset()
删除：m_machineSettingsPage.reset()
删除：m_inspectionPage.reset()
删除：delete ui
删除：ui = nullptr
```

不保留裸指针 Ui 成员、手工释放分支、辅助释放函数或新旧所有权双路径。

### 10.5 无效防御代码

对五个已经完成 `setupUi()` 的页面及其固定控件，删除以下形式的检查：

```cpp
if (!ui) return;
if (!m_view.someControl) return;
if (m_view.someControl) { ... }
throw std::invalid_argument("... requires complete bindings");
```

对于确定创建并由引用传入的 Page 和服务，同样删除只防止项目内部装配错误的空指针分支。保留真正表达可选业务含义的空值，例如某个设置项明确没有标签，以及用户可能取消文件夹选择。

当前代码中的以下固定对象检查必须明确处理：

- MainWindow 在完成固定初始化后，对 `m_inspectionPage`、`m_machineSettingsPage`、`m_templateEditorPage` 的判空全部删除；初始化顺序保证任何相关信号连接和首次方法调用都发生在三个 Page 创建之后。
- `setLabelTextIfChanged(QLabel *, ...)` 和 `setStyleProperty(QWidget *, ...)` 改为接收引用并删除控件判空；`QWidget::style()` 的无意义判空同时删除。
- `MachineSettingsPage::installWheelProtection(QWidget *)` 的必需根窗口改为 `QWidget &`，保留现有按控件类型安装事件过滤器的行为，删除根窗口判空。
- 五个生成 Ui、三个弹窗生成 Ui、必需窗口、必需服务和必需状态不增加“尚未初始化”“setupUi 失败”或空对象回退路径。

用户可能取消目录选择、文件或目录可能不可用、输入可能非法、服务和设备操作可能失败、运行快照或图片可能无效，以及 `alarmPresented` 是否已经为 `true`，都属于真实业务或环境状态，相关判断必须保留。

以下旧依赖形式及其防御分支必须同时消失：

```text
TemplateSelectionDialog 的 TemplateApplicationService* / SettingsApplicationService*
TemplateSelectionDialog 的无服务默认摘要和提前返回
InspectionPage::m_rootWidget 指针及其判空
InspectionPage 的外部 QTimer* / bool* 模板提示状态
InspectionPage::presentFault(..., bool*)
TemplateEditorViewBindings::parentWidget
TemplateEditorPage::dialogParent()
```

最终代码直接使用必需引用、自有计时器和值状态，不保留指针重载、旧构造函数或空值兼容入口。

### 10.6 手写模板弹窗界面

实施后以下旧结构必须完全消失：

- `TemplateSelectionDialog` 构造函数中创建固定标签、模板树、按钮、按钮盒和布局的代码。
- `CharacterTemplateEditorDialog::buildUi()` 的声明、定义和调用。
- `CharacterTemplateEditorDialog::CropImageLabel` 嵌套类定义及其 `std::function` 回调。
- `TemplateEditorPage` 保存流程中局部 `QDialog saveDialog` 以及随其创建的固定控件、布局和连接代码。
- 仅因上述手写固定界面使用的前置声明、成员指针和 Qt 布局 include。

对应固定界面只能存在于三个新 `.ui` 中；字符框选行为只能存在于最终 `CharacterCropLabel` 中；保存模板弹窗行为只能存在于最终 `TemplateSaveDialog` 中，不保留旧实现、兼容入口或双路径。

### 10.7 右侧抽屉标题栏

以下 Ui 对象实施后必须在生产代码、`.ui` 和正式 QSS 中零引用：

```text
frame_rightDrawerHeader
label_rightDrawerTitle
toolButton_collapseRightDrawer
```

同时删除收起按钮的 `connect()`、`showRightDrawerPage()` 中更新标题文字的语句，以及三个对象的专用 QSS 选择器。现有 `MainWindow::showRightDrawerPage()` 原位改名为 `showRightPanelPage()`，现有 `MainWindow::collapseRightDrawer()` 原位改名为 `hideRightPanel()`；全部调用点一次性同步修改。旧函数名实施后必须零引用，不保留别名、重载、转发函数或兼容入口。

### 10.8 旧目录和主图像控件名称

目录归拢后，以下旧源文件路径必须从工程清单和生产源码中消失，旧位置不保留文件或转发头：

```text
app/ui/main_window.ui
app/ui/main_window.h
app/ui/main_window.cpp
app/ui/main_window_inspection.cpp
app/ui/main_window_settings.cpp
app/ui/pages/inspection_page.h/.cpp
app/ui/pages/machine_settings_page.h/.cpp
app/ui/pages/template_editor_page.h/.cpp
app/ui/dialogs/template_selection_dialog.h/.cpp
app/ui/dialogs/character_template_editor_dialog.h/.cpp
app/ui/controllers/operation_ui_policy.h/.cpp
app/ui/controllers/settings_edit_state.h/.cpp
app/ui/presenters/inspection_fault_presenter.h/.cpp
app/ui/widgets/image_label.h/.cpp
```

上述文件迁入最终功能目录后，空置的 `app/ui/pages/`、`app/ui/dialogs/`、`app/ui/controllers/`、`app/ui/presenters/` 和 `app/ui/widgets/` 同时删除。`app/ui/README.md` 保持原位，不属于旧路径。

主图像画布重命名后，以下旧 UI 类型标识符、对象名和 include 路径在生产代码、`.ui`、正式 QSS、qmake 清单及当前维护说明中必须消失：

```text
ImageLabel
imageLabel_inspection
ui/widgets/image_label.h
```

不得保留 `using ImageLabel = InspectionImageCanvas`、旧类派生包装、旧对象名备用查找或新旧头文件双路径。检测合同中的 `clearImageLabelRects` 不属于本次 UI 类型改名，保持现状。

## 11. 文件级修改范围

| 文件 | 计划修改 |
|---|---|
| `app/ui/main_window/main_window.ui` | 由当前 `app/ui/main_window.ui` 原位移动；只保留主窗口骨架和五个空内容页根节点；删除五页内部 UI 定义、标题标签、独立收起按钮及其空父容器；将提升类、提升头和对象名改为 `InspectionImageCanvas`、`ui/main_window/inspection_image_canvas.h`、`inspectionImageCanvas` |
| `app/ui/main_window/main_window.h` | 由当前 `app/ui/main_window.h` 移动；将 `Ui::MainWindow` 和五个页面生成 Ui 统一改为 `std::unique_ptr` 所有权并按依赖顺序声明；删除 ViewBindings/Callbacks 工厂、重复控件成员和模板闪烁成员；使用 `InspectionImageCanvas` 最终类型 |
| `app/ui/main_window/main_window.cpp` | 由当前 `app/ui/main_window.cpp` 移动；按固定顺序装配五个生成 Ui；直接创建三个逻辑 Page；建立必要的直接连接；析构函数只保留真实业务清理并删除 Page 手工 `reset()`、`delete ui`、`ui = nullptr`；删除标题更新、独立收起按钮连接、绑定和回调构造代码；将 `showRightDrawerPage()` / `collapseRightDrawer()` 原位改名为 `showRightPanelPage()` / `hideRightPanel()` |
| `app/ui/main_window/main_window_inspection.cpp` | 由当前 `app/ui/main_window_inspection.cpp` 移动；将页面控件访问改到对应生成 Ui；删除旧预览指针槽、固定 Ui/Page 判空和旧闪烁清理 |
| `app/ui/main_window/main_window_settings.cpp` | 由当前 `app/ui/main_window_settings.cpp` 移动；将五页控件访问改到对应生成 Ui；保留真正跨区域协调；删除旧同页显示回调和重复控件别名 |
| `app/ui/README.md` | 保持原位；更新最终功能目录、Designer 打开方式、直接 Ui 引用和 `InspectionImageCanvas` 职责 |
| `app/ui/main_window/inspection_image_canvas.h/.cpp` | 由 `app/ui/widgets/image_label.h/.cpp` 移动并完整改名；类型改为 `InspectionImageCanvas`，与唯一宿主 MainWindow 放在同一目录；保持现有图像显示、模板绘制、几何状态和强类型事件行为 |
| `app/ui/main_window/operation_ui_policy.h/.cpp` | 由 `app/ui/controllers/operation_ui_policy.h/.cpp` 移动；继续作为主控按钮状态的唯一规则，不修改状态计算行为 |
| `app/ui/main_window/inspection/inspection_info_page.ui` | 新建；接收检测信息页完整静态内容 |
| `app/ui/main_window/inspection/inspection_page.h/.cpp` | 由 `app/ui/pages/inspection_page.h/.cpp` 移动；直接使用 MainWindow 窗口引用及 MainWindow/检测信息生成 Ui；以值成员收回模板按钮闪烁计时器和开关；`presentFault()` 使用 `bool &`；删除 ViewBindings、外部状态指针和固定控件判空 |
| `app/ui/main_window/inspection/inspection_fault_presenter.h/.cpp` | 由 `app/ui/presenters/inspection_fault_presenter.h/.cpp` 移动；继续负责 Fault 文本和呈现数据，不修改 Fault 规则 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 新建；接收参数设定页完整静态内容 |
| `app/ui/main_window/settings/image_settings_page.ui` | 新建；接收图像设置页完整静态内容 |
| `app/ui/main_window/settings/plc_settings_page.ui` | 新建；接收 PLC 通讯页完整静态内容 |
| `app/ui/main_window/settings/software_settings_page.ui` | 新建；接收软件设置页完整静态内容 |
| `app/ui/main_window/settings/machine_settings_page.h/.cpp` | 由 `app/ui/pages/machine_settings_page.h/.cpp` 移动；直接使用四个设置页生成 Ui；删除 ViewBindings、Callbacks、软件目录控件 setter、固定控件判空以及未使用的 `QObject *parent` 构造参数和 `QObject(parent)` 初始化 |
| `app/ui/main_window/settings/settings_edit_state.h/.cpp` | 由 `app/ui/controllers/settings_edit_state.h/.cpp` 移动；继续作为设置编辑状态的唯一实现，不修改状态规则 |
| `app/ui/main_window/template/template_editor_page.h/.cpp` | 由 `app/ui/pages/template_editor_page.h/.cpp` 移动；直接使用弹窗父窗口引用及 MainWindow/参数设定/图像设置生成 Ui；删除 ViewBindings、Callbacks、`dialogParent()`、控件别名以及未使用的 `QObject *parent` 构造参数和 `QObject(parent)` 初始化；使用 `InspectionImageCanvas` 及三个弹窗最终 include；增加两个明确语义信号；以 `TemplateSaveDialog` 替换内联保存模板固定界面 |
| `app/ui/main_window/template/selection/template_selection_dialog.ui` | 新建；接收选择模板弹窗的完整固定界面 |
| `app/ui/main_window/template/selection/template_selection_dialog.h/.cpp` | 由 `app/ui/dialogs/template_selection_dialog.h/.cpp` 移动到同一弹窗目录；以 `std::unique_ptr` 直接拥有生成 Ui；服务依赖改为引用；删除手写固定布局、固定控件别名、服务判空和默认摘要回退；保留模板树动态数据及选择业务 |
| `app/ui/main_window/template/save/template_save_dialog.ui` | 新建；接收保存模板弹窗的完整固定界面 |
| `app/ui/main_window/template/save/template_save_dialog.h/.cpp` | 新建实际弹窗类；负责模板名称、保存目录、浏览和输入有效性，不调用模板服务 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.ui` | 新建；接收分割字符模板弹窗的完整固定界面 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.h/.cpp` | 由 `app/ui/dialogs/character_template_editor_dialog.h/.cpp` 移动到同一弹窗目录；以 `std::unique_ptr` 直接拥有生成 Ui；删除 `buildUi()`、嵌套框选类和固定控件别名；保留动态命名、预览和结果生成 |
| `app/ui/main_window/template/character_editor/character_crop_label.h/.cpp` | 接收现有字符图像绘制、鼠标框选和坐标换算逻辑；声明 `Q_OBJECT` 和 `itemsChanged()`；作为同目录弹窗 Ui 的 Designer 提升控件 |
| `app/resource/qss/app_theme.qss` | 删除右侧抽屉标题栏三个已删除对象的专用样式；将主图像画布选择器改为 `InspectionImageCanvas#inspectionImageCanvas`；不调整其他样式 |
| `app/resource/Translate_CN.ts`、`app/resource/Translate_EN.ts` | 只刷新移动和拆分 `.ui` 后的源文件位置记录，不修改现有翻译文本 |
| `app/startup/application_startup.cpp` | 将 MainWindow include 更新为 `ui/main_window/main_window.h` |
| `app/application/template_geometry_service.h` | 将指向旧 UI 类型的职责注释更新为 `InspectionImageCanvas`，不修改接口和行为 |
| `app/AutoOCRproject.pro` | 一次性登记 `app/ui/main_window/` 下的最终源文件、头文件和八个新增 `.ui` 路径；删除 `pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/` 的全部旧路径和重复项 |
| `docs/development/OCRGangYinUI架构精简优化方案.md`、`OCRGangYin开发者代码结构与维护指南.md`、`OCRGangYin现有功能对照表.md` | 只同步仍然有效的 MainWindow 路径、弹窗路径和 `InspectionImageCanvas` 名称；不改写已完成阶段和历史执行事实 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案及其与现有 UI 方案的权威关系 |

本批次对 `app_theme.qss` 只删除三个失效标题栏选择器并同步主图像画布类型/对象名；对翻译文件只刷新 `.ui` 源位置。不修改 SVG、qrc、AppSettings、JSON Schema、ApplicationService、Runtime、Detection、设备层或模板磁盘格式。

## 12. 明确不做

- 除删除右侧抽屉 48px 标题栏，以及将主图像画布类型/文件/对象名改为 `InspectionImageCanvas`、`inspection_image_canvas.*`、`inspectionImageCanvas` 外，不修改页面视觉设计、文字、布局尺寸、控件类型或其他对象名。
- 不实施主控操作台按钮重新分组。
- 不把保存模板和分割字符模板提前迁入主控操作台。
- 不新增 QWidget 页面包装类。
- 不保留按技术类型划分的 `app/ui/pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/`，也不在 `main_window/` 下重新建立同类目录。
- 不继续细分 `inspection/`、`settings/`，不为 `InspectionImageCanvas`、`OperationUiPolicy`、`InspectionPage`、`MachineSettingsPage` 或 `TemplateEditorPage` 单独建立目录。
- 不为两个现有模板弹窗增加第二层包装类；只新增替换内联局部对话框的 `TemplateSaveDialog` 和承载既有鼠标交互的 `CharacterCropLabel`。
- 不新增 `ViewBindings` 的替代结构。
- 不新增 View 接口、Adapter、Facade、Presenter、页面基类或通用 Controller。
- 不新增页面工厂、注册表、路由器、事件总线或依赖注入框架。
- 不使用 `QUiLoader`、`findChild()`、字符串控件查找或运行时 `.ui` 文件路径。
- 不用占位 `QLabel/QWidget` 加运行时删除、替换或重新插入的方式装配字符框选控件。
- 不为 `QMessageBox`、`QFileDialog` 或动态模板项创建 `.ui`。
- 不把删除的标题标签或收起按钮复制到五个页面，不增加新标题栏、悬浮收起按钮或键盘收起入口。
- 不保留旧 Ui 成员别名、旧对象名别名、旧槽重载或新旧调用双路径。
- 不保留旧文件路径转发头、旧 `ImageLabel` 类型别名、旧提升控件声明或新旧 qmake 路径双登记。
- 不保留必需窗口、必需服务、模板提示计时器和 Fault 提示状态的可空指针构造形式或兼容重载；不保留 `MachineSettingsPage`、`TemplateEditorPage` 未使用的 QObject 父对象参数，也不为二者设置 QObject 父对象。
- 不通过 `parent()`、`window()`、`qobject_cast` 或对象查找补回已经删除的 `parentWidget` 绑定。
- 不为页面初始化增加失败回退、空页面备用显示或重复 `setupUi()` 防护。
- 不新增 AppSettings 字段、页面索引字段、页面加载状态或持久化数据。
- 不为使用 `std::make_unique` 升级当前 C++11 标准；生成 Ui 的 `std::unique_ptr` 直接在构造函数初始化列表中接收 `new` 创建的对象。
- 不改变相机、PLC、模板、检测、统计、Fault 和结果传输业务规则。
- 除同步 UI 文件的新 include 路径、MainWindow include、主图像画布类型引用和 `.ui` 翻译源位置外，不借拆分机会清理 `app/ui` 之外与本方案无关的代码。

## 13. 实施步骤

### 阶段 SUI-0：目录归拢和主图像画布改名

1. 建立 `app/ui/main_window/` 及其 `inspection/`、`settings/`、`template/selection/`、`template/save/`、`template/character_editor/` 最终功能目录；`app/ui/README.md` 保持原位。
2. 将 MainWindow 的 `.ui`、`.h` 和三个 `.cpp` 移入 `app/ui/main_window/`，将 `OperationUiPolicy` 从 `app/ui/controllers/` 移入同一目录。
3. 将 `InspectionPage` 和 `InspectionFaultPresenter` 分别从 `app/ui/pages/`、`app/ui/presenters/` 移入 `app/ui/main_window/inspection/`。
4. 将 `MachineSettingsPage` 和 `SettingsEditState` 分别从 `app/ui/pages/`、`app/ui/controllers/` 移入 `app/ui/main_window/settings/`。
5. 将 `TemplateEditorPage` 移入 `app/ui/main_window/template/`；将两个现有模板弹窗类移入 `selection/` 和 `character_editor/`；保存模板新类从一开始只建立在 `save/`。
6. 将 `app/ui/widgets/image_label.h/.cpp` 直接移动并改名为 `app/ui/main_window/inspection_image_canvas.h/.cpp`，将类型 `ImageLabel` 和对象名 `imageLabel_inspection` 同步改为 `InspectionImageCanvas` 和 `inspectionImageCanvas`。
7. 一次性更新生产源码 include、两个 Designer 提升控件头、正式 QSS、qmake `SOURCES/HEADERS/FORMS`、UI README 和翻译源位置；`ui_main_window.h` 等 uic 生成文件名不手工修改。
8. 删除空置的 `app/ui/pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/`，并删除全部旧路径、旧类型名和旧对象名；不建立转发头、类型别名、临时复制文件或新旧工程清单双登记。

该阶段只改变源文件归属和主图像画布名称，不改变任何运行行为。目录移动、引用更新和旧路径删除必须在同一个实施差异中完成。

### 阶段 SUI-1：提取五个静态页面

1. 建立五个独立 `.ui` 文件。
2. 从 `main_window.ui` 剪切五个页面的内部布局和控件到对应文件。
3. 保留 `stackedWidget_rightDrawer` 中五个现有 `page_*` 根节点。
4. 删除 `frame_rightDrawerHeader`、`label_rightDrawerTitle` 和 `toolButton_collapseRightDrawer`，让 `stackedWidget_rightDrawer` 直接填充右侧内容区。
5. 将 `stackedWidget_rightDrawer.currentIndex` 设为 0，与已选中的检测信息按钮保持一致。
6. 逐页核对控件名、属性、布局、Spacer 和滚动区结构。
7. 将五个文件加入 qmake `FORMS`。

该阶段不允许复制后再长期保留两份；同一个实施差异中完成剪切和旧内容删除。

### 阶段 SUI-2：直接装配生成 Ui

1. 将 MainWindow 现有 `Ui::MainWindow *ui` 改为 `std::unique_ptr<Ui::MainWindow>`，以 `std::unique_ptr` 创建五个页面生成 Ui，并按第 6.2 节顺序执行六个 `setupUi()`；现有 `ui->` 访问方式不变。
2. 确保所有页面控件在首次读写前已经创建。
3. 将 MainWindow 三个实现文件中的 `ui->页面控件` 改为对应页面 Ui 成员。
4. 为迁出页面且仍由 MainWindow 处理的按钮逐一使用函数指针连接到现有业务槽；不再次调用 `connectSlotsByName(this)`，不新增批量连接器或中转方法。
5. 保持导航继续切换原五个 `page_*` 根节点。
6. 删除收起按钮连接和标题更新语句，将导航按钮二次点击及启动识别成功后的隐藏调用统一改为 `hideRightPanel()`。
7. 将 `showRightDrawerPage()` 原位改名为 `showRightPanelPage()`，同步修改五个导航连接和 Fault 首次展开检测信息的调用点。
8. 在 `TemplateEditorPage` 创建后，将仍位于主窗口的制作模板按钮直接连接到 `TemplateEditorPage::handleTemplateCaptureButton()`，并删除原 MainWindow 中转槽。

### 阶段 SUI-3：删除绑定和回调支架

1. 三个逻辑 Page 改为接收生成 Ui 引用；`InspectionPage` 和 `TemplateEditorPage` 同时接收必需的 `QWidget &`。
2. 删除三组 ViewBindings 及 MainWindow 的逐字段映射函数。
3. 删除两组 Callback 结构和只转发 lambda。
4. 将同一页面内的显示更新留在负责该页面的现有 Page 类。
5. TemplateEditorPage 使用两个明确 Qt 信号连接 MainWindow 的跨区域协调。
6. `MachineSettingsPage`、`TemplateEditorPage` 和 `TemplateSelectionDialog` 的必需服务及状态全部改为引用。
7. 删除 `MachineSettingsPage` 和 `TemplateEditorPage` 构造函数中的 `QObject *parent` 参数及实现中的 `QObject(parent)` 初始化；二者只由 MainWindow 的 `std::unique_ptr` 唯一拥有。
8. `InspectionPage::presentFault()` 改用 `bool &` 复用 `m_faultAlarmPresented`，删除可空状态路径，同时保留并准确改写为 `if (!alarmPresented)` 的首次提示业务判断。
9. 将保存模板按钮的 `operationUi.saveTemplate` 状态应用从 `InspectionPage` 移到已持有参数设定 Ui 的 `TemplateEditorPage`，删除原跨页访问。
10. 删除固定 Ui 控件、必需窗口、必需服务和必需 Page 的无效判空及默认回退；辅助函数和 `installWheelProtection()` 的必需控件参数改用引用。

### 阶段 SUI-4：删除重复状态和旧引用

1. 模板制作按钮闪烁计时器和开关以值成员收回 `InspectionPage`。
2. 删除 MainWindow 的控件别名和软件目录控件二次绑定。
3. 删除 TemplateEditorPage 的控件别名。
4. 删除 `MainWindow::on_toolButton_createTemplate_clicked()`、`MainWindow::resetTemplateCaptureState()`、`MainWindow::confirmInspectionFaultRecovery()` 三个纯转发方法，调用点直接使用最终 Page 方法。
5. 删除失效的前置声明、include、函数声明和实现。
6. 删除标题栏三个对象的失效 QSS 选择器。
7. 按第 6.1 节调整 MainWindow 成员声明的相对顺序，使三个逻辑 Page、五个页面生成 Ui、状态和服务、主窗口生成 Ui 自动按依赖顺序析构；删除三个 Page 的显式 `reset()`、`delete ui` 和 `ui = nullptr`，不增加生命周期包装。
8. 全仓搜索计划删除符号，确认零引用。

### 阶段 SUI-5：拆分三个模板弹窗

1. 在三个最终弹窗子目录中新建各自的 `.ui`，将现有固定结构和属性写入对应文件。
2. 三个弹窗都以 `std::unique_ptr` 直接拥有自己的生成 Ui；在头文件声明析构函数，并在包含完整生成 Ui 类型的 `.cpp` 中使用 `= default` 定义；`TemplateSelectionDialog` 的两个服务改为必需引用。
3. 新建 `TemplateSaveDialog`，原位替换 `TemplateEditorPage` 中的内联局部保存对话框。
4. 将现有嵌套框选类提取为带 `Q_OBJECT` 和 `itemsChanged()` 信号的 `CharacterCropLabel`，与字符模板弹窗放在 `app/ui/main_window/template/character_editor/`，由 `character_template_editor_dialog.ui` 通过 Promote to 直接创建。
5. 保留模板树节点、字符命名行和字符预览卡片的动态创建逻辑。
6. 删除三个旧手写固定界面、固定控件别名、旧回调和失效 include。
7. 将三个新 `.ui`、新弹窗类和字符框选控件加入 qmake 对应清单。

### 阶段 SUI-6：静态检查和用户验证

Agent 只执行第 14 节静态检查。Qt Creator 的 Run qmake、构建、运行和人工交互由用户执行。

## 14. 静态验收

### 14.1 `.ui` 与工程清单

- 九个 `.ui` 均为合法 XML。
- qmake `FORMS` 中九个文件路径均存在、无重复。
- `app/ui/` 根目录只保留 `README.md` 和 `main_window/`；README 未被复制或移动到功能子目录。
- `app/ui/main_window/` 根目录只包含 MainWindow 的 `.ui`、`.h`、三个 `.cpp`、`InspectionImageCanvas`、`OperationUiPolicy` 以及 `inspection/`、`settings/`、`template/` 三个功能目录。
- `inspection/` 只包含检测信息页、`InspectionPage` 和 `InspectionFaultPresenter`；`settings/` 只包含四个设置页、`MachineSettingsPage` 和 `SettingsEditState`。
- 三个模板弹窗分别位于 `template/selection/`、`template/save/` 和 `template/character_editor/`，`TemplateEditorPage` 直接位于 `template/`。
- `CharacterCropLabel` 只位于 `template/character_editor/`；`InspectionImageCanvas` 只位于 `app/ui/main_window/`。
- `app/ui/pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/` 均不存在，工程清单和生产源码中对应旧路径零引用。
- `app/ui/main_window/main_window.ui` 中五个 `page_*` 根节点各存在一次。
- `stackedWidget_rightDrawer` 直接位于 `widget_rightPanel` 的布局中，不存在空标题栏或 48px 顶部占位。
- `stackedWidget_rightDrawer.currentIndex` 为 0，初始选中按钮为检测信息。
- 五个页面的业务控件只存在于对应独立 `.ui`，不在主窗口文件保留副本。
- 五个新 `.ui` 可分别在 Qt Designer 中独立打开并显示完整页面。
- 三个模板弹窗 `.ui` 可分别在 Qt Designer 中独立打开并显示完整固定界面。
- `main_window.ui` 的主图像提升控件为 `InspectionImageCanvas`，头文件为 `ui/main_window/inspection_image_canvas.h`，对象名为 `inspectionImageCanvas`。
- `character_template_editor_dialog.ui` 的提升控件类为 `CharacterCropLabel`，头文件为 `ui/main_window/template/character_editor/character_crop_label.h`。
- 没有新增手写固定布局代码、`QUiLoader` 或字符串控件查找。
- `frame_rightDrawerHeader`、`label_rightDrawerTitle` 和 `toolButton_collapseRightDrawer` 在生产代码、`.ui` 和正式 QSS 中零引用。

### 14.2 C++ 结构

- 三组 ViewBindings 符号全仓零引用。
- 两组 Callback 符号全仓零引用。
- UI 生产代码中不再使用旧类型 `ImageLabel`、旧对象名 `imageLabel_inspection` 和旧 include `ui/widgets/image_label.h`；不存在旧类型别名、派生包装或转发头。检测合同字段 `clearImageLabelRects` 不属于 UI 类型改名。
- `InspectionImageCanvas` 保留原主图像控件的显示、缩放、模板绘制、几何状态和强类型事件接口，没有第二套实现。
- 主窗口、五个页面和三个弹窗共九个生成 Ui 均由各自所有者使用 `std::unique_ptr` 持有；`Ui::MainWindow *ui` 裸指针所有权零引用，不存在第二套所有权。
- MainWindow 的三个逻辑 Page 声明在其使用的生成 Ui、状态和服务之后；析构函数体不手工释放 Page 或生成 Ui，成员按依赖顺序自动析构，没有生命周期管理类。
- `TemplateSelectionDialog`、`TemplateSaveDialog`、`CharacterTemplateEditorDialog` 都在头文件声明析构函数，并在生成 Ui 类型完整的 `.cpp` 中定义。
- 五个生成 Ui 在任何页面控件首次访问前完成 `setupUi()`。
- 三个逻辑 Page 只保存生成 Ui 和必需服务/状态的直接引用。
- `MachineSettingsPage` 和 `TemplateEditorPage` 构造函数中不存在 `QObject *parent` 参数、兼容重载或 `QObject(parent)` 初始化；两个逻辑 Page 只由 MainWindow 的 `std::unique_ptr` 拥有，未设置 QObject 父对象。
- 页面控件没有重复成员别名。
- 固定 Ui 控件、必需服务和固定创建的三个 Page 不再做空指针防护；相关辅助函数和 `installWheelProtection()` 的必需对象参数使用引用。
- `TemplateSelectionDialog` 的两个服务均为引用；服务指针、无服务默认摘要和提前返回零引用。
- `InspectionPage` 的窗口对象为引用，模板提示计时器和开关为值成员；外部 `QTimer *`、模板提示 `bool *` 及对应判空零引用。
- `InspectionPage::presentFault()` 以 `bool &` 使用 MainWindow 的 `m_faultAlarmPresented`，不存在指针重载或第二份 Fault 提示状态，并保留 `if (!alarmPresented)` 的首次提示业务判断。
- `TemplateEditorPage` 的弹窗父窗口为直接引用；`TemplateEditorViewBindings::parentWidget`、`dialogParent()`、父对象遍历和类型转换零引用。
- 保存模板按钮的 `operationUi.saveTemplate` 状态只由 `TemplateEditorPage::applyOperationState()` 通过参数设定 Ui 直接应用，`InspectionPage` 不持有或访问参数设定 Ui。
- 页面内按钮全部存在一条可追踪的最终连接，无重复连接；迁出页面且由 MainWindow 处理的按钮使用函数指针显式连接，不依赖页面根 QWidget 的自动槽，也不对 MainWindow 重复调用 `connectSlotsByName(this)`。
- 制作模板按钮直接连接 `TemplateEditorPage::handleTemplateCaptureButton()`；`MainWindow::on_toolButton_createTemplate_clicked()`、`MainWindow::resetTemplateCaptureState()` 和 `MainWindow::confirmInspectionFaultRecovery()` 全仓零引用。
- 五个导航按钮只保留各自绑定固定页面和按钮参数的短 lambda，不存在导航槽批量包装、按钮映射结构、路由器或导航类。
- 最终只存在 `showRightPanelPage()` 和 `hideRightPanel()`；`showRightDrawerPage()`、`collapseRightDrawer()` 全仓零引用。
- `showRightPanelPage()` 不再更新标题；`hideRightPanel()` 由导航二次点击和启动成功后的自动隐藏流程复用，Fault 首次呈现通过 `showRightPanelPage()` 打开检测信息。
- 三个模板弹窗各自只拥有自己的生成 Ui，不存在 DialogBindings、弹窗工厂或控件转发层。
- `CharacterTemplateEditorDialog::buildUi()`、嵌套 `CropImageLabel`、`setChangedCallback()` 和内联 `QDialog saveDialog` 全仓零引用。
- `CharacterCropLabel` 包含 `Q_OBJECT` 和唯一的 `itemsChanged()` 信号，不存在 `std::function` 变更回调备用路径。
- `TemplateSelectionDialog` 和 `CharacterTemplateEditorDialog` 不再保存固定控件或固定布局别名。
- `slot_displayAndDetect(cv::Mat *)` 旧签名零引用。
- 模板按钮闪烁计时器和状态只在 `InspectionPage` 存在一份。

### 14.3 行为与范围

- 右侧导航仍使用原五个 `page_*` 切换。
- 默认打开检测信息、运行成功收起、Fault 首次打开检测信息的代码边界不变。
- `OperationUiPolicy` 和 `SettingsEditState` 仍是唯一状态规则，不增加副本。
- 模板、相机、PLC、统计、软件设置和结果传输仍调用现有应用服务。
- 没有 AppSettings、Schema、qrc 或 SVG 差异；正式 QSS 只删除三个失效标题栏选择器并同步主图像画布类型/对象名，翻译文件只刷新 `.ui` 源位置。
- `git diff --check` 通过。

静态检查通过只表示代码和资源关系已核对，不表示构建或交互验证通过。

## 15. 用户验证

1. 在 Qt Creator 中执行 Run qmake，使八个新 `FORMS` 生成对应 `ui_*.h`。
2. 用 Qt Designer 打开 `app/ui/main_window/main_window.ui`，确认中央画布显示为 `InspectionImageCanvas`；再分别打开五个页面和三个模板弹窗 `.ui`，确认每个文件都能独立显示并编辑完整固定界面，分割字符模板界面的框选画布显示为 `CharacterCropLabel`。
3. 构建并启动程序，确认默认展开检测信息页。
4. 逐个点击五个导航按钮，确认选中状态、抽屉展开和页面内容正确，右侧内容区顶部不再显示标题栏。
5. 再次点击当前已选中的任一导航按钮，确认抽屉收起且五个导航按钮均恢复未选中；重新点击后确认原页面内容状态未丢失。
6. 验证总数清零、NG 清零和剔除复位。
7. 验证模板选择弹窗的当前路径显示、增加、勾选移除、确认应用和取消。
8. 验证保存模板弹窗的名称启用状态、目录浏览、非法名称/无效目录提示、取消、覆盖确认和成功保存。
9. 验证分割字符模板弹窗的已有框恢复、鼠标框选、撤销、清空、排序、下一步、返回、动态命名、预览、保存和取消。
10. 验证模板制作和参数应用仍使用原有流程。
11. 验证检测模式、图像保存、曝光、增益、颜色通道、旋转和纸巾阈值。
12. 验证 PLC 连接、断开、触发模式和运行参数应用。
13. 验证软件数据目录、清空软件数据和恢复默认设置。
14. 验证二维码结果传输启用、连接、断开和状态显示。
15. 启动识别后确认抽屉收起；制造或模拟同一次 Fault，确认只在首次呈现时打开检测信息页。

## 16. 完成标准

同时满足以下条件才算本方案实施完成：

- `app/ui/README.md` 保持在 UI 根目录；其余本方案涉及的 UI 文件全部归入 `app/ui/main_window/` 功能域。
- MainWindow 核心文件、`InspectionImageCanvas` 和 `OperationUiPolicy` 位于 `main_window/` 根目录；检测、设置、模板文件分别位于 `inspection/`、`settings/`、`template/`。
- 三个模板弹窗分别位于 `template/selection/`、`template/save/`、`template/character_editor/`，`CharacterCropLabel` 与字符模板弹窗位于同一目录。
- 旧 `pages/`、`dialogs/`、`controllers/`、`presenters/`、`widgets/` 目录及其工程引用已全部删除。
- `ImageLabel`、`image_label.*` 和 `imageLabel_inspection` 已分别完整替换为 `InspectionImageCanvas`、`app/ui/main_window/inspection_image_canvas.*` 和 `inspectionImageCanvas`；旧类型、旧文件、旧对象名及其兼容入口不存在。
- 右侧五个内容页已分别存在于五个独立 `.ui` 文件中。
- 选择模板、保存模板和分割字符模板已分别存在于三个独立 `.ui` 文件中。
- `main_window.ui` 只保留主窗口骨架和五个页面根节点，不保留任何页面内容副本。
- `frame_rightDrawerHeader`、`label_rightDrawerTitle`、`toolButton_collapseRightDrawer` 及其连接、标题更新和专用样式已完全删除，不保留空白或页面内副本。
- 再次点击当前导航按钮和启动识别成功都通过唯一的 `hideRightPanel()` 隐藏右侧面板；Fault 首次呈现通过 `showRightPanelPage()` 打开检测信息。
- 旧 `showRightDrawerPage()` 和 `collapseRightDrawer()` 已完全删除，不保留别名、重载、转发函数或兼容入口。
- 五个页面全部通过生成 Ui 直接装配，没有 QWidget 空壳类、动态加载器或页面注册框架。
- 三组 ViewBindings、两组 Callback、旧映射函数、重复控件别名和旧模板闪烁外部状态已完全删除。
- 主窗口、五个页面和三个弹窗共九个生成 Ui 的所有权统一且唯一，全部使用 `std::unique_ptr`，不存在裸指针/智能指针并行方案或手工生命周期胶水。
- MainWindow 的成员声明顺序保证三个逻辑 Page 先于其引用的页面生成 Ui、状态和服务销毁，`Ui::MainWindow` 最后销毁；析构函数中不存在 Page 手工 `reset()`、`delete ui` 或 `ui = nullptr`。三个弹窗的生成 Ui 均通过在 `.cpp` 定义的显式析构函数正确释放，不增加生命周期包装层。
- `MachineSettingsPage` 和 `TemplateEditorPage` 不接收或保存 QObject 父对象，不调用 `QObject(parent)` 或 `setParent()`；两者只保留 MainWindow `std::unique_ptr` 的单一所有权路径。
- `TemplateSelectionDialog`、`MachineSettingsPage` 和 `TemplateEditorPage` 的必需服务均使用引用，不保留无服务回退、旧指针构造函数或兼容重载。
- `InspectionPage` 的必需窗口使用引用，模板提示计时器和开关由自身以值成员持有，Fault 提示状态通过 `bool &` 复用 MainWindow 的唯一状态。
- `TemplateEditorPage` 直接保存弹窗父窗口引用，不保留 `dialogParent()` 或父对象查找路径。
- 固定 Ui 控件、必需窗口、必需服务和固定创建的三个 Page 的无效防御性判空已删除，辅助函数和 `installWheelProtection()` 的必需对象参数使用引用，真实业务校验仍完整保留。
- Fault 仍以 MainWindow 唯一的 `m_faultAlarmPresented` 保证同一次故障只首次提示；删除的只是指针为空路径，不删除 `if (!alarmPresented)` 业务条件。
- 保存模板启用状态由 `TemplateEditorPage` 通过参数设定 Ui 直接应用，`InspectionPage` 不再跨页访问保存模板按钮。
- 页面按钮直接连接到最终处理逻辑；迁出页面按钮不依赖 MainWindow 自动槽发现，不重复调用 `connectSlotsByName(this)`，没有兼容层、转发层或新旧双路径。
- `MainWindow::on_toolButton_createTemplate_clicked()`、`MainWindow::resetTemplateCaptureState()` 和 `MainWindow::confirmInspectionFaultRecovery()` 三个纯转发方法已删除。
- 三个模板弹窗不再手写固定界面；动态条目仍由所属弹窗直接维护；字符框选控件通过 Designer 提升直接创建。
- `CharacterCropLabel` 使用 `Q_OBJECT` 和唯一的 `itemsChanged()` 信号；旧 `buildUi()`、嵌套框选类、`std::function` 变更回调和内联保存模板对话框已完全删除，没有 DialogBindings、占位替换或弹窗工厂。
- Agent 静态门禁通过。
- 用户完成 Qt Creator 构建和第 15 节人工交互验证。
