# OCRGangYin 图像上方常驻工具栏与设备状态重构方案

## 1. 文档状态

- 编写日期：2026-09-13。
- 修订日期：2026-09-17。
- 当前状态：代码实施完成，待用户统一验证。
- 局部替代：五个模板按钮的位置已由 `OCRGangYin模板管理按钮迁入参数设定页方案.md` 替代；本文关于七入口工具栏和 `widget_templateActions` 的正文仅保留为历史实施记录，当前工具栏只保留相机、识别、设备状态和硬触发。
- 硬触发最终状态：唯一 `QCheckBox` 同时显示滑块和“开/关”文字，`checked` 是唯一状态源；无已保存配置时默认关闭，已有配置仍按保存值恢复。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `3115fa6`。
- 视觉合同：本文第 3、4 节定义的 74px 工具栏、76x62px 按钮、16px 文字和独立退出按钮是唯一最终视觉基线。
- 权威范围：把主画面顶部改为 74px 常驻工具栏，设置七个操作入口、相机/PLC 状态和唯一硬触发开关。
- 实施关系：本文与 `OCRGangYin运行故障自动停止方案.md` 组成同一最终实施批次；逻辑上先收口 Fault 接口和停止链，再一次性落地最终工具栏，不保留中间兼容结构。
- 实施门禁：生产代码和现行说明已完成实施；Agent 只执行静态门禁，构建、运行和真实设备验证由用户统一完成。

## 2. 源码范围

当前源码中：

- `main_window.ui` 只保留 74px 高的 `frame_mainToolbar`，包含七个最终操作按钮、相机与 PLC 状态和硬触发开关。
- `checkBox_hardwareTriggerEnabled` 只存在于主工具栏，由 `MachineSettingsPage` 直接绑定 `trigger.enabled`，负责初始化、立即保存、保存失败恢复和运行权限。
- `OperationUiPolicy` 输出相机、识别和模板退出三个最终按钮合同；相机和识别各由一个状态化按钮承接。
- `updateOperationUiState()` 每次只读取一份 `RuntimeSnapshot`，同时刷新按钮、设备状态和各 Page。
- 模板操作区统一使用 `widget_templateActions` 控制显隐。

以下边界保持原有实现：

- 80px 左侧导航栏、五页抽屉、手动开合、拖动调宽和 `QSplitter` 状态持久化。
- 五个页面 `.ui`、三个 Page、`OperationUiPolicy`、`SettingsEditState` 和应用服务边界。
- 图像画布、判定栏、模板制作流程和五种检测模式。
- 设置 Schema、模板 Schema、相机与 PLC 协议、软硬触发处理、统计、CSV、存图和算法。

## 3. 最终界面

```text
左侧导航栏 | 五页内容抽屉 | 图像主画面
                             ├─ 74px 常驻主控工具栏
                             └─ 图像显示区
```

工具栏从左到右为：

```text
[相机开关] [识别启停] | [选择模板] [制作模板] [保存模板] [分割字符] [退出]
                                      [相机状态] [PLC状态] [硬触发]
```

`main_window.ui` 中的最终控件树为：

```text
widget_mainPanel
└─ verticalLayout_mainPanel
   ├─ frame_mainToolbar
   │  └─ horizontalLayout_mainToolbar
   │     ├─ toolButton_cameraAction
   │     ├─ toolButton_inspectionAction
   │     ├─ line_toolbarActionSeparator
   │     ├─ widget_templateActions
   │     │  └─ horizontalLayout_templateActions
   │     │     ├─ toolButton_selectTemplate
   │     │     ├─ toolButton_createTemplate
   │     │     ├─ toolButton_saveTemplate
   │     │     ├─ toolButton_editCharacterTemplates
   │     │     └─ toolButton_exitTemplate
   │     ├─ horizontalSpacer_mainToolbar
   │     └─ widget_deviceStatus
   │        └─ horizontalLayout_deviceStatus
   │           ├─ frame_cameraStatus
   │           │  └─ horizontalLayout_cameraStatus
   │           │     ├─ frame_cameraStatusDot
   │           │     └─ label_cameraStatusText
   │           ├─ frame_plcStatus
   │           │  └─ horizontalLayout_plcStatus
   │           │     ├─ frame_plcStatusDot
   │           │     └─ label_plcStatusText
   │           └─ frame_hardwareTrigger
   │              └─ horizontalLayout_hardwareTrigger
   │                 ├─ label_hardwareTriggerTitle
   │                 └─ checkBox_hardwareTriggerEnabled
   └─ groupBox_imageDisplay
```

`widget_templateActions` 只排列五个模板按钮，并按现有检测模式整体显隐。状态块只负责显示，不持有业务状态。

## 4. 尺寸、文字和图标

- `frame_mainToolbar` 固定高度 74px，左右边距 8px，上下边距 5px。
- 七个操作按钮固定为 76x62px，图标为 27x27px，统一使用 `Qt::ToolButtonTextUnderIcon`，按钮间距 2px。
- 分隔线宽 1px、高 42px。
- 设备状态区靠右，最小宽度 300px，内部间距 8px；三个状态块高 30px。
- 相机和 PLC 状态圆点固定为 10x10px。
- 工具栏按钮、设备状态和硬触发文字均为 16px、`#000000`，字重只使用现有 400 和 600 两档。
- 所有按钮可见文字最多四个汉字，完整说明放在 Tooltip。

图标使用：

| 状态或操作 | 资源 |
|---|---|
| 打开相机 | `:/svg/camera_on.svg` |
| 关闭相机 | `:/svg/camera_off.svg` |
| 启动识别 | `:/svg/start.svg` |
| 停止识别 | `:/svg/stop.svg` |
| 选择模板 | `:/svg/template_select.svg` |
| 制作模板 | `:/svg/template_make.svg` |
| 保存模板 | `:/svg/template_save.svg` |
| 分割字符 | `:/svg/character_seg.svg` |
| 退出模板制作 | `:/svg/stop.svg` |

工具栏背景为 `#E7EDF2`。相机和 PLC 状态圆点使用绿、红两种状态；硬触发开启状态使用蓝色。

硬触发开关继续使用唯一 `QCheckBox`，指示器固定为 30x16px；未勾选和勾选状态由正式 QSS 分别切换 `:/svg/toggle_off.svg` 和 `:/svg/toggle_on.svg`，控件自身文字同步显示“关”或“开”，禁用时保持当前勾选状态对应的图像和文字。不增加自定义控件、绘制类、运行时图标生成或主题服务。

## 5. 七个操作入口

### 5.1 相机开关

`toolButton_cameraAction` 的状态合同：

| 界面状态 | 文字 | 可用 | 点击结果 |
|---|---|---:|---|
| `CameraClosed` | 打开相机 | 是 | 执行现有打开相机流程 |
| `CameraReady` | 关闭相机 | 是 | 执行现有关闭相机流程 |
| `Detecting` | 关闭相机 | 否 | 无动作 |
| `Stopping` | 关闭相机 | 否 | 无动作 |
| `TemplatePreviewing` | 关闭相机 | 否 | 无动作 |
| `TemplateFrozen` | 关闭相机 | 否 | 无动作 |

按钮不设为 checkable，也不增加相机状态成员。打开或关闭成功后，由同一份 `RuntimeSnapshot::cameraOpen` 同步刷新按钮、图标和状态块；操作失败时显示仍然以快照为准。

### 5.2 识别启停

`toolButton_inspectionAction` 的状态合同：

| 界面状态 | 文字 | 可用 | 点击结果 |
|---|---|---:|---|
| `CameraClosed` | 启动识别 | 否 | 无动作 |
| `CameraReady` | 启动识别 | 是 | 执行现有启动流程 |
| `Detecting` | 停止识别 | 是 | 执行现有停止流程 |
| `Stopping` | 停止中 | 否 | 无动作 |
| `TemplatePreviewing` | 启动识别 | 否 | 使用独立退出按钮 |
| `TemplateFrozen` | 启动识别 | 否 | 使用独立退出按钮 |

### 5.3 模板操作

`toolButton_selectTemplate`、`toolButton_saveTemplate` 和 `toolButton_editCharacterTemplates` 保持原有业务行为。

`toolButton_createTemplate` 使用以下短文字：

| 状态 | 文字 | Tooltip |
|---|---|---|
| 普通状态 | 制作模板 | 进入当前检测模式的模板制作流程。 |
| `TemplatePreviewing` | 拍照框选 | 拍照并开始框选模板区域。 |
| `TemplateFrozen` | 重新取景 | 清除当前临时框线并重新获取模板画面。 |

`toolButton_exitTemplate` 固定显示“退出”，Tooltip 为“退出当前模板制作流程”。它只在 `TemplatePreviewing` 和 `TemplateFrozen` 可用，点击后直接停止模板预览、取消绘图并刷新界面。

运行故障自动停止期间，工具栏按 `Stopping` 状态呈现并使用停止中的按钮合同。

## 6. 最终代码连接

### 6.1 MainWindow 动作

`MainWindow` 使用以下最终函数：

```cpp
void handleCameraAction();
void handleInspectionAction();
void openCamera();
void closeCamera();
void startInspection();
void stopInspection();
void exitTemplate();
```

- 两个统一按钮只连接各自的 `handle*Action()`。
- `handleCameraAction()` 根据当前快照调用 `openCamera()` 或 `closeCamera()`。
- `handleInspectionAction()` 根据当前运行状态调用 `startInspection()` 或 `stopInspection()`。
- `exitTemplate()` 集中承接模板退出逻辑，直接停止模板预览、取消绘图并刷新界面。
- `stopInspection()` 是普通停止的唯一 MainWindow 入口，直接调用 `m_inspectionApplicationService->stop(InspectionFaultReason::None)`；故障排队 lambda 直接调用同一个应用服务停止合同并传入 `snapshot.reason`，不经过 `stopInspection()` 或其他转发函数。
- 四个旧自动槽及旧停止槽中的多动作分派一次删除；新按钮不通过旧槽、转发函数或 lambda 别名调用旧入口。
- 相机、识别和模板动作位于 `main_window_inspection.cpp`；`main_window_settings.cpp` 只更新模板操作区显隐引用。

### 6.2 OperationUiPolicy

`OperationUiSnapshot` 删除以下字段：

```text
openCamera
closeCamera
startDetection
stop
startDetectionText
stopText
```

对应的最终字段为：

```cpp
Access cameraAction;
Access inspectionAction;
Access templateExit;
QString cameraActionText;
QString inspectionActionText;
QString templateCaptureText;
```

现有模板、设置、统计权限和 `cameraOpen`、`plcConnected` 继续由 `OperationUiPolicy::create()` 统一生成。实现中不增加 Action 枚举、命令类、第二个策略对象或新状态机。

`updateOperationUiState()` 每次只读取一份 `RuntimeSnapshot`，并把它传给 `operationUiState(const RuntimeSnapshot &snapshot) const` 生成界面状态；按钮、设备状态和各 Page 使用这同一份快照，不增加快照缓存或成员镜像。

### 6.3 硬触发绑定

`checkBox_hardwareTriggerEnabled` 从 `detection_settings_page.ui` 移到 `main_window.ui`，对象名和 `trigger.enabled` 设置键不变，全仓只有一个控件实例。

- `MachineSettingsPage` 构造函数直接接收该 `QCheckBox &`，现有设置绑定改为使用这一个引用。
- `MachineSettingsPage` 不接收 `Ui::MainWindow`，不使用 `findChild()`，不建立 ViewBindings、别名或镜像控件。
- 空闲时按现有 `generalSettings` 权限编辑；识别、停止和模板操作期间禁用。
- PLC 未连接时不改写已经保存的硬触发值，启动识别仍执行现有 PLC 门禁。
- MainWindow 在设置恢复前把唯一 QCheckBox 的 `toggled` 直接连接到该控件自身，按 `checked` 显示“开”或“关”；不增加同步函数、成员状态或第二个显示控件。
- `restoreAppliedValues("trigger.enabled")` 设置 `m_updatingSettingsUi` 后直接恢复勾选值，不对该 QCheckBox 使用 `QSignalBlocker`；现有保存回调会跳过重复保存，控件文字仍会随勾选状态同步。
- `AppSettings` 在无已保存配置时将 `triggerEnabled` 默认为 `false`；已有配置继续由 `camera.triggerSource` 恢复，不迁移或覆盖用户保存值。

## 7. 设备状态

### 7.1 相机

- 初始值、打开结果和关闭结果统一读取 `RuntimeSnapshot::cameraOpen`。
- `true` 显示绿色圆点和“相机已打开”，`false` 显示红色圆点和“相机未打开”。
- 状态与相机按钮在同一次 `updateOperationUiState()` 中刷新。
- `MainWindow::updateOperationUiState()` 在同一次刷新中，依据快照给 `frame_cameraStatusDot` 设置 `connectionState=connected/disconnected`；该属性只由本次快照派生，不保存成员状态。
- 不增加相机轮询、物理在线探测或成员布尔量。

### 7.2 PLC

- 程序自动连接、手动连接或打开相机流程中的 PLC 连接成功后，显示绿色圆点和“PLC已连接”。
- 手动断开、连接失败或 PLC Fault 停止流程断开后，显示红色圆点和“PLC未连接”。
- 状态统一读取 `RuntimeSnapshot::plcConnected`，不保存第二份 PLC 状态。
- `MainWindow::updateOperationUiState()` 依据 `plcConnected` 给 `frame_plcStatusDot` 设置 `connectionState=connected/disconnected`。
- 现有 500ms 健康检查只负责运行期间的 PLC Fault 判定；状态块不增加定时器、精确心跳或自动重连。

两个圆点的动态属性发生变化后，`updateOperationUiState()` 直接对对应控件执行一次重新 polish 和刷新，使 QSS 立即生效；不为此增加样式服务、辅助对象或持久状态。

## 8. QSS 与资源

正式样式只写入 `:/qss/app_theme.qss`。新增选择器限定为最终控件对象名：

```text
QFrame#frame_mainToolbar
QToolButton#toolButton_cameraAction
QToolButton#toolButton_inspectionAction
QWidget#widget_templateActions QToolButton
QToolButton#toolButton_exitTemplate
QFrame#frame_cameraStatus
QFrame#frame_plcStatus
QFrame#frame_hardwareTrigger
QLabel#label_cameraStatusText
QLabel#label_plcStatusText
QLabel#label_hardwareTriggerTitle
QCheckBox#checkBox_hardwareTriggerEnabled
QCheckBox#checkBox_hardwareTriggerEnabled::indicator
QCheckBox#checkBox_hardwareTriggerEnabled::indicator:unchecked
QCheckBox#checkBox_hardwareTriggerEnabled::indicator:checked
QFrame#frame_cameraStatusDot[connectionState="connected"]
QFrame#frame_cameraStatusDot[connectionState="disconnected"]
QFrame#frame_plcStatusDot[connectionState="connected"]
QFrame#frame_plcStatusDot[connectionState="disconnected"]
```

状态只通过现有 `uiState` 和必要的控件属性呈现。生产 C++ 不设置完整控件样式字符串，不增加内联主题、样式工厂或第二份 QSS。

`svg/toggle_off.svg` 和 `svg/toggle_on.svg` 是硬触发开关唯一的关闭、开启图像，登记到现有 `image.qrc`；不增加禁用副本、PNG 副本、运行时换色或回退资源。

主工具栏使用 `camera_on.svg`、`camera_off.svg`、`start.svg`、`stop.svg`、`template_select.svg`、`template_make.svg`、`template_save.svg` 和 `character_seg.svg`；静态按钮和运行时相机、识别状态统一引用这组资源。

旧 `groupBox_mainControls` 专用 QSS 随对应控件删除。现有以 `QCheckBox#checkBox_hardwareTriggerEnabled` 为对象的规则直接改写为最终工具栏开关规则，不叠加参数页版和工具栏版两套样式；全局 QCheckBox 规则只在仍有其他使用者时保留。

## 9. 文件级实施范围

| 文件 | 处理 |
|---|---|
| `app/ui/main_window/main_window.ui` | 建立最终工具栏、七个按钮、完整状态子控件和硬触发唯一控件；删除旧主控容器和四个相机/识别按钮 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 删除硬触发控件及其布局项，不留空行或占位 |
| `app/ui/main_window/main_window.h/.cpp` | 建立最终按钮连接和硬触发控件自身文字直连，把硬触发控件直接传给 `MachineSettingsPage`；每次界面刷新只读取一份运行快照 |
| `app/ui/main_window/main_window_inspection.cpp` | 承接统一相机、识别和模板退出动作，删除旧槽 |
| `app/ui/main_window/main_window_settings.cpp` | 删除旧停止槽，把模板区显隐改为 `widget_templateActions` |
| `app/ui/main_window/operation_ui_policy.h/.cpp` | 输出两个统一按钮和独立退出按钮的权限与文字 |
| `app/ui/main_window/inspection/inspection_page.h/.cpp` | 应用最终文字、图标、状态和权限 |
| `app/ui/main_window/settings/machine_settings_page.h/.cpp` | 直接绑定迁移后的 QCheckBox，沿用初始化、立即保存、失败恢复和权限控制 |
| `app/system_support/settings/app_settings.cpp` | 无已保存配置时硬触发默认关闭；不改变已有配置读取规则 |
| `app/resource/qss/app_theme.qss` | 写入最终工具栏、设备状态和硬触发开关样式 |
| `app/resource/image.qrc`、`app/resource/svg/{camera_on,camera_off,template_select,template_make,template_save,character_seg,start,stop,toggle_off,toggle_on}.svg` | 登记并提供主工具栏和硬触发开关使用的十个 SVG |
| `app/resource/README.md` | 同步工具栏和硬触发 SVG 的用途与调用边界 |
| `app/ui/README.md` | 同步 UI 职责 |
| `docs/development/OCRGangYin计划索引.md` | 同步计划状态和路由 |
| `docs/development/OCRGangYin开发者代码结构与维护指南.md` | 同步最终工具栏结构和入口 |
| `docs/development/OCRGangYin现有功能对照表.md` | 同步最终操作入口、设备状态和硬触发位置 |

本文不修改 `.pro`、AppSettings Schema、模板 Schema、Detection、Devices、PLC 协议或算法。

每个受影响文件在完成最终职责后，同步清理失去用途的 include、前置声明、字段、函数、局部辅助代码和对象引用。

## 10. 最终结构门禁

实施完成后，下列符号在生产代码中必须为零引用：

```text
groupBox_mainControls
horizontalLayout_mainControls
widget_cameraControls
horizontalLayout_cameraControls
widget_templateControls
horizontalLayout_templateControls
widget_inspectionControls
horizontalLayout_inspectionControls
toolButton_openCamera
toolButton_closeCamera
toolButton_startInspection
toolButton_stopInspection
on_toolButton_openCamera_clicked
on_toolButton_closeCamera_clicked
on_toolButton_startInspection_clicked
on_toolButton_stopInspection_clicked
OperationUiSnapshot::openCamera
OperationUiSnapshot::closeCamera
OperationUiSnapshot::startDetection
OperationUiSnapshot::stop
OperationUiSnapshot::startDetectionText
OperationUiSnapshot::stopText
QGroupBox#groupBox_mainControls
```

`checkBox_hardwareTriggerEnabled` 只在 `main_window.ui` 定义一次，在 `detection_settings_page.ui` 零出现。

硬触发的滑块图像和“开/关”文字只由唯一 QCheckBox 的 `checked` 状态派生。

生产代码中不存在并行按钮、隐藏副本、对象名别名、槽转发、新按钮调用被列出的槽、`findChild()`、硬触发镜像控件、设备状态镜像成员或新 Manager/Controller/Adapter/Factory/Builder/事件总线。

联合 Fault 方案列出的删除项也必须保持零引用；完整清单以该方案第 10 节为准。

## 11. 实施顺序

1. 在同一代码批次中先按 Fault 方案收口停止接口、自动停止和故障警告所有权，但不保留旧停止合同、确认链或 Presenter。
2. 一次性修改两个 `.ui`，形成最终控件树并删除旧控件实例。
3. 把 `MachineSettingsPage` 的硬触发设置绑定切到工具栏唯一控件。
4. 收敛 `OperationUiSnapshot`，同步三个 Page 和模板操作区显隐。
5. 直接建立两个统一按钮及独立退出按钮和最终 `stopInspection()`，删除旧槽和多动作分派。
6. 更新 QSS、清理被替换控件的专用样式并同步 Fault 方案涉及的工程条目和模块说明。
7. 执行联合静态门禁，不建立新旧界面或新旧停止链并存的过渡阶段。

## 12. 验证

### 12.1 Agent 静态门禁

- 两个 `.ui` 均为合法 XML，最终控件及对象名唯一。
- 第 10 节符号全部零引用；旧主控 QSS 不存在。
- 两个统一按钮和独立退出按钮只连接最终函数。
- Fault 方案列出的旧停止合同、旧回调、故障确认/恢复函数和 `InspectionFaultPresenter` 源文件全部零引用并已从工程清单删除。
- 按钮文字在 76x62px 内完整显示，长说明只位于 Tooltip。
- 每次 `updateOperationUiState()` 只读取一份 `RuntimeSnapshot`；相机与 PLC 状态由这份快照派生，无新轮询、镜像状态和自动重连。
- 四个圆点状态选择器均使用明确的 `connected` 或 `disconnected` 属性值，属性变化后会重新 polish 对应控件。
- `trigger.enabled` 只有一个控件、一个设置绑定和一条保存失败恢复路径。
- 无已保存配置时 `triggerEnabled` 为 `false`；读取已有 `hardwareLine0` 或 `software` 配置时仍分别恢复为开或关。
- 硬触发开关的未勾选和勾选状态分别使用唯一的关闭、开启 SVG；禁用状态沿用当前勾选状态图像，两个资源路径在 QRC 中各登记一次。
- 受影响文件中的 include、前置声明、字段、函数、局部辅助代码和对象引用均与最终职责一致。
- 左侧导航、抽屉、Schema、Detection、Devices、PLC 协议和算法无计划外差异。
- qmake 文件清单、UTF-8、文件末尾换行和 `git diff --check` 通过。

静态检查不表述为构建、运行或设备验证通过。

### 12.2 用户统一验证

1. Qt Creator 执行 Run qmake、Clean 和 Release Rebuild。
2. 在 1600x950 和现场分辨率下确认工具栏高度、按钮、状态文字均不截断或重叠。
3. 展开、切换、收起五个页面并拖动抽屉，确认工具栏常驻且画布正常扩展。
4. 验证相机打开/关闭成功和失败时，按钮、图标与状态显示一致。
5. 验证识别启动、停止和自动停止期间的按钮文字及权限。
6. 验证模板预览、拍照框选、重新取景和独立退出流程。
7. 验证 PLC 连接、断开、连接失败和 PLC 故障自动停止后的状态显示及重连入口。
8. 验证首次无配置时硬触发显示“关闭滑块 + 关”，并验证开关文字同步、立即保存、失败恢复、重启恢复和运行权限。
9. 回归软硬触发、统计、剔除、CSV、存图、模板和五种检测模式。

## 13. 完成条件

- 主画面只有一条 74px 常驻工具栏和七个最终操作入口；按钮为 76x62px、文字为 16px。
- 相机/识别各使用一个状态化按钮，模板退出使用独立按钮。
- 相机、PLC 和硬触发状态只读取现有快照及唯一设置控件。
- 旧主控容器、四个旧相机/识别按钮、旧槽、旧权限字段和旧专用 QSS 全部通过零引用门禁。
- 参数页没有硬触发控件、空行或替代占位。
- 实现中没有兼容层、转发层、隐藏副本、镜像状态和计划外抽象。
- Agent 静态门禁通过，用户完成 Qt Creator、交互和真实设备验证。
