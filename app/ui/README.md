# ui：主窗口、页面区域、对话框、控件和展示策略

## 一句话理解

`ui` 只负责“用户看到什么、输入什么、点击后调用哪个 Application 用例，以及如何显示返回结果”。它不拥有相机/PLC、生产线程、检测算法、结果结算或正式存图服务。

## 先把 28 个文件折叠成 8 个概念

```text
ui/
├─ MainWindow 外壳（5 个文件）
│  ├─ main_window.ui
│  ├─ main_window.h
│  ├─ main_window.cpp
│  ├─ main_window_inspection.cpp
│  └─ main_window_settings.cpp
│
├─ 3 个页面区域管理器（9 个文件）
│  ├─ InspectionPage（2）
│  ├─ MachineSettingsPage（2）
│  └─ TemplateEditorPage（4 个实现文件 + 1 个头）
│
├─ 模板页共享 UI 工具（2 个文件）
│  └─ template_editor_support.h/.cpp
│
├─ 2 个界面状态/策略组件（4 个文件）
│  ├─ OperationUiPolicy
│  └─ SettingsEditState
│
├─ 2 个对话框（4 个文件）
│  ├─ RecipeSelectionDialog
│  └─ CharacterTemplateEditorDialog
│
├─ 1 个 Fault Presenter（2 个文件）
└─ 1 个自定义 ImageLabel（2 个文件）
```

这里的文件多，主要是 C++ 头/源成对，以及一个较大的 `TemplateEditorPage` 按主题拆成多个 `.cpp`。它们不是 28 层架构，也不是 28 个 Controller。

## 当前完整文件树

```text
ui/
├─ main_window.ui
├─ main_window.h
├─ main_window.cpp
├─ main_window_inspection.cpp
├─ main_window_settings.cpp
├─ controllers/
│  ├─ operation_ui_policy.h
│  ├─ operation_ui_policy.cpp
│  ├─ settings_edit_state.h
│  └─ settings_edit_state.cpp
├─ dialogs/
│  ├─ recipe_selection_dialog.h
│  ├─ recipe_selection_dialog.cpp
│  ├─ character_template_editor_dialog.h
│  └─ character_template_editor_dialog.cpp
├─ pages/
│  ├─ inspection_page.h
│  ├─ inspection_page.cpp
│  ├─ machine_settings_page.h
│  ├─ machine_settings_page.cpp
│  ├─ template_editor_page.h
│  ├─ template_editor_page.cpp
│  ├─ template_editor_view.cpp
│  ├─ template_editor_recipe.cpp
│  ├─ template_editor_profile_commands.cpp
│  ├─ template_editor_support.h
│  └─ template_editor_support.cpp
├─ presenters/
│  ├─ inspection_fault_presenter.h
│  └─ inspection_fault_presenter.cpp
├─ widgets/
│  ├─ image_label.h
│  └─ image_label.cpp
└─ README.md
```

## UI 在整体架构中的位置

```text
用户
↕
MainWindow / Page / Dialog / Widget
↓ 结构化命令
ApplicationService
↓
Runtime / Recipes / Settings

Runtime 完整结果
→ Application UI 合同
→ InspectionPresentationRenderer
→ InspectionPage 的显式 ViewBindings
→ 用户
```

UI 不能越过 Application 直接操作 Runtime、Camera、PLC 或 RecipeStore。

## 只有一份 .ui，不是三个独立页面

`main_window.ui` 是唯一 Qt Designer 表单，拥有主窗口全部真实控件。三个 Page 不是三个独立 QWidget 窗口，也没有各自 `.ui`：

```text
MainWindow（唯一拥有 Ui::MainWindow）
├─ InspectionPageViewBindings
│  └─ InspectionPage 借用检测区控件
├─ MachineSettingsPageViewBindings
│  └─ MachineSettingsPage 借用设置区控件
└─ TemplateEditorViewBindings
   └─ TemplateEditorPage 借用模板区控件
```

`ApplicationStartup` 构造 MainWindow 和三个 Page，再调用 `MainWindow::attachPages`。Page 不删除这些控件；控件生命周期由 MainWindow/Qt 父子关系管理。

## MainWindow 外壳

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `main_window.ui` | 唯一界面布局真源，定义左右面板、操作按钮、结果区、模板、图像、PLC 和软件设置页，以及自定义 `ImageLabel`。 | 不手改生成的 `ui_main_window.h`；控件改名要同步自动槽和 ViewBindings。 |
| `main_window.h` | 声明 MainWindow、Application 服务引用、三个 Page 组合点、Qt 自动槽和少量跨页面 UI 状态。 | MainWindow 不是业务所有者；不要继续把算法、线程或设备成员塞入。 |
| `main_window.cpp` | `setupUi`、构造顶层定时器/回调、建立三组 ViewBindings 和 Callbacks、attach 页面、初始恢复和析构清理。 | 只有这里拥有生成的 Ui 对象；Page 不能重新接收整份 Ui 指针。 |
| `main_window_inspection.cpp` | 相机、启停、Fault、运行快照、关闭事件和检测操作槽；统一生成一次 `OperationUiSnapshot` 再分发。 | 槽只收集参数、调用 Application、呈现结果；不复制 Runtime 业务状态判断。 |
| `main_window_settings.cpp` | 设置保存/恢复、模式切换、模板/字库协调、相机/PLC 参数和 Profile 命令的槽与页面转发。 | 参数格式校验留在 UI；设备是否允许执行由 Application 再裁决。 |

### 为什么 MainWindow 有三个 `.cpp`

它们共同实现同一个 `MainWindow` 类：

- `main_window.cpp`：构造、绑定和生命周期。
- `main_window_inspection.cpp`：运行检测主题。
- `main_window_settings.cpp`：设置与模板协调主题。

这是按主题拆分一个大类，不是三个主窗口。

## controllers：界面状态与权限策略

这里的“Controller”不是业务控制器，只是两个 UI 内部辅助对象。

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `controllers/operation_ui_policy.h` | 定义七种 UI 操作状态、Context 和统一 `OperationUiSnapshot`，每个操作有 enabled 与禁用原因。 | 不是 Runtime 状态机，不代替 Application 命令门禁。 |
| `controllers/operation_ui_policy.cpp` | 唯一计算相机、启停、模板、设置、配方、PLC、统计和队列操作的 UI 可用性与按钮文字。 | 新可执行控件先归入一个 Access，再由所属 Page 显式应用。 |
| `controllers/settings_edit_state.h` | 定义全局设置和模板目标/阈值 dirty 状态。 | 只记录 UI 编辑状态，不持久化。 |
| `controllers/settings_edit_state.cpp` | 登记设置显示名、汇总 dirty 项并生成“未应用参数”提示。 | 新设置要同步登记和清理。 |

## pages：三个主界面区域

### InspectionPage

| 文件 | 当前职责 |
|---|---|
| `pages/inspection_page.h` | 显式声明检测区控件绑定、结果回调、操作状态、Fault、ROI 和存图错误接口。 |
| `pages/inspection_page.cpp` | 更新图像、判定、文字、统计、耗时、状态和检测区按钮；显示 Fault/ROI/存图告警。 |

它主要消费结果和权限快照，不直接启动 Worker；运行命令仍经 MainWindow → InspectionApplicationService。

### MachineSettingsPage

| 文件 | 当前职责 |
|---|---|
| `pages/machine_settings_page.h` | 显式声明机器设置控件绑定、回调、设置绑定和硬件依赖类型。 |
| `pages/machine_settings_page.cpp` | 验证器、滚轮保护、UI↔MachineSettings 双向映射、dirty、保存/恢复和硬件权限应用。 |

控件按四类硬件依赖管理：`None`、`Camera`、`PlcConnection`、`PlcRuntime`。页面不会用“整个设置页全部开/关”覆盖细分权限。

### TemplateEditorPage

这是一个类，不是五个 Controller。一个页面同时包含预览、绘图、字符编辑、Profile 编辑和发布，所以实现按主题分文件：

| 文件 | 当前职责 |
|---|---|
| `pages/template_editor_page.h` | 同一个 `TemplateEditorPage` 的完整公开接口、ViewBindings、Callbacks、CaptureState 和私有状态。 |
| `pages/template_editor_page.cpp` | 构造、权限应用、预览开始/冻结/停止、帧/失败回调、选择/保存入口和条码即时验证状态。 |
| `pages/template_editor_view.cpp` | 引导框、状态文字、闪烁、绘图事件、dirty 显示和手工字符编辑入口。 |
| `pages/template_editor_recipe.cpp` | 五模式配方恢复、选择、激活、发布、同 UUID 重发和字库 Profile 选择。 |
| `pages/template_editor_profile_commands.cpp` | 字符资产加载、单/多 Profile 保存、目标文字与阈值的单项/批量应用、纸巾阈值。 |
| `pages/template_editor_support.h/.cpp` | OpenCV 临时 ROI 窗口、鼠标回调、图像/模式/数值等模板页共享 UI 辅助。 |

维护规则：新成员在 `template_editor_page.h` 声明，再按“基础流程/视图/配方/Profile 命令”放进对应 `.cpp`；不要再新建只转发的 Controller。

## dialogs：短生命周期交互

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `dialogs/recipe_selection_dialog.h/.cpp` | 显示已发布配方和损坏项，让用户按稳定 recipeId 选择。 | 只消费目录 DTO，不访问 RecipeStore。 |
| `dialogs/character_template_editor_dialog.h/.cpp` | 框选字符、排序、命名、预览、文件名去重并返回 Profile/裁切图。 | 只返回用户编辑结果；正式资源持久化由 TemplateApplicationService 完成。 |

Dialog 最好遵守“构造参数输入、`exec()` 交互、结果方法输出”，不要成为长期业务对象。

## presenters：把数据翻译成显示模型

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `presenters/inspection_fault_presenter.h/.cpp` | 把 `ApplicationFaultSnapshot` 纯转换为中文状态、结果、操作员提示和样式。 | 不操作 Runtime/设备，不弹窗，不声称软件无法确认的机械状态。 |

Presenter 应保持确定性：相同快照得到相同显示结果，没有 I/O 和副作用。

## widgets：可复用界面控件

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `widgets/image_label.h/.cpp` | 等比自适应显示图片，处理跟踪矩形、可选二维码矩形、日期多边形的鼠标/键盘绘制与 Overlay。 | 只保存显示坐标和交互状态；保存前必须由 TemplateGeometryService 转为原图坐标。 |

`ImageLabel` 不加载配方、不解二维码、不执行正式检测，也不保存正式结果历史。

## 一次“开始检测”如何流动

```text
用户点击 toolButton_startInspection
→ Qt 自动连接 MainWindow 槽
→ 槽收集当前 UI 参数
→ InspectionApplicationService::start
→ Application 返回结构化成功/拒绝结果
→ MainWindow 更新 OperationUiState
→ OperationUiPolicy 生成唯一 Snapshot
→ InspectionPage / MachineSettingsPage / TemplateEditorPage 各自应用属于自己的控件
```

正式结果返回时：

```text
ResultService 生成完整 InspectionPresentation
→ 容量 1 UI 邮箱
→ UI 线程 InspectionPresentationRenderer
→ InspectionPage 绑定的图像、文字、判定、统计和耗时一次更新
```

这样避免图像来自产品 A、文字却来自产品 B。

## UI 禁用与底层门禁如何分工

当前不是所有交互都机械复制三层判断，而是按风险分工：

1. 纯显示、提示、展开/折叠：只需 UI 自身约束。
2. 设置草稿、配方编辑/发布：UI 权限 + Application/Store 数据校验和事务。
3. 正式检测、相机、模板取景、PLC、统计和剔除队列：UI 策略 + Application 命令门禁 + Runtime/Session/Device 不变量。

三层判断的职责不同。按钮禁用负责引导用户；Application 防止非按钮调用或竞态；底层保证线程、连接和设备 SDK 安全。不能在三层复制完全相同的 `if`。

## 允许放什么

- Qt 控件、布局、输入验证、对话框、鼠标键盘交互。
- UI 状态、dirty 状态、禁用原因和纯展示转换。
- 调用 Application 服务并呈现结构化结果。
- 短期当前画面与绘图状态。

## 禁止放什么

- 相机/PLC 供应商对象、生产线程和队列。
- Detection Pipeline、识别算法、统计结算和正式存图服务。
- 绕过 Application 直接使用 Runtime/RecipeStore/MachineSettingsStore。
- 扫描整窗所有按钮再统一启用/禁用。
- 用 UI 状态代替底层安全不变量。

## 按需求定位文件

| 需求 | 首先修改 | 通常同步 |
|---|---|---|
| 改布局/控件 | `main_window.ui` | 对应 ViewBindings、自动槽 |
| 改运行按钮权限 | `operation_ui_policy.*` | 所属 Page 的显式应用 |
| 改相机/启停交互 | `main_window_inspection.cpp` | InspectionApplicationService |
| 改整机设置控件 | `machine_settings_page.*` | SettingsApplicationService、MachineSettings/Store |
| 改模板预览/冻结 | `template_editor_page.cpp` | InspectionApplicationService 的预览命令 |
| 改模板引导/绘图 | `template_editor_view.cpp`、`image_label.*` | TemplateGeometryService |
| 改配方选择/发布 | `template_editor_recipe.cpp` | TemplateApplicationService/RecipeStore |
| 改字库目标/阈值 | `template_editor_profile_commands.cpp` | Recipe/Profile 消费者 |
| 改 Fault 文案 | `inspection_fault_presenter.*` | ApplicationFaultSnapshot、Fault 人工回归 |

## 推荐阅读顺序

```text
main_window.ui
→ main_window.cpp（看 ViewBindings 和 attachPages）
→ pages/inspection_page.*
→ pages/machine_settings_page.*
→ pages/template_editor_page.h
→ template_editor_page.cpp / view.cpp / recipe.cpp / profile_commands.cpp
→ main_window_inspection.cpp / main_window_settings.cpp
→ controllers/operation_ui_policy.*
```

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单、自动槽/ViewBindings 和开发者维护指南。
