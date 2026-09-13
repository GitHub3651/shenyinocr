# OCRGangYin 配置未应用标记与失败回退优化方案

## 1. 文档状态

- 方案日期：2026-08-24。
- 当前状态：四个阶段的代码实施、静态门禁和用户统一验证均已完成。
- 权威范围：界面中机器设置、模板参数的未应用 `*`、成功提交、失败回退、配置持久化失败、启动前检查和缺字提示。
- 当前进度：机器设置单一 `current`、未应用标记与页面归属、提交顺序与失败回退、模板参数与批量处理均已实施；四个阶段分别提交，最终静态门禁通过，用户已完成统一验证并确认功能正常。
- 核心决定：`SettingsApplicationService` 只保留本次会话已生效的 `current`，删除机器设置的长期 `draft`；UI 控件保存尚未应用的输入，`SettingsEditState` 是未应用状态的唯一来源。
- 边界说明：`TemplateApplicationService::draft()` 是模板制作过程中的编辑对象，继续保留；它不属于本方案删除的机器设置 draft。
- 页面归属：纸巾检测不使用模板，纸巾粗糙度阈值完整归属 `MachineSettingsPage`；`TemplateEditorPage` 不再绑定、加载、应用或恢复该参数。
- 保存入口：删除 `MainWindow::saveSettings()`、`MachineSettingsPage::save()`、`MachineSettingsPage::syncImmediateSettings()` 和 `MachineSettingsPage::Callbacks::saveSettings`；立即生效字段、恢复默认和窗口关闭分别使用明确的局部 candidate，不保留通用整页保存链。
- 模式切换：用户手动切换 `detect.mode` 时只由 `MainWindow::setupDetectModeChangeTracking()` 保存并同步模式相关页面；恢复默认是独立的批量恢复操作，允许把默认 `detectModeId` 与其他四个立即生效字段一次性保存；`MachineSettingsPage` 保留检测模式控件的初始化、绑定和操作禁用规则，但不再自动保存用户手动切换。
- 替代关系：本版本替代本文此前的 machine draft、`m_current != m_draft`、`acceptDraftForCurrentSession()` 设计；不保留两套新旧逻辑。
- 实施门禁：本轮代码实施和用户统一验证已完成；未覆盖既有用户差异，`main_window.ui` 保持零计划差异，本方案不再继续执行。

## 2. 目标与设计原则

最终只保留四种含义明确的状态来源：

```text
UI 控件
    → 保存用户正在编辑、尚未应用的输入

SettingsEditState
    → 记录每个需要确认/应用/设置/连接的 UI 参数是否未应用
    → 生成 * 和启动前的参数名称列表

SettingsApplicationService::current()
    → 保存本次会话真正生效的完整 AppSettings
    → Runtime、相机、PLC、预览和检测启动统一读取

app_settings.json
    → 保存下次启动使用的持久化配置
    → 硬件已生效但保存失败时，允许暂时落后于 current
```

设计目标：

1. `*` 只表示“控件值尚未通过对应操作生效”，不表示配置是否已写入磁盘。
2. 未应用状态只由 `SettingsEditState` 汇总，不再同时使用 `m_current != m_draft`。
3. `current()` 只表示本次会话已生效设置，不表示磁盘一定已经持久化成功。
4. UI 不直接修改 `current()`；每次操作只从 `current()` 复制一个函数内局部 candidate，并只修改本次按钮负责的字段。
5. 校验、纯配置保存、模板保存或硬件操作失败时，恢复本次操作负责的 UI 值并清除对应 `*`。
6. 相机或 PLC 已成功采用新值但配置保存失败时，UI、`current()` 和真实硬件保留新值，清除 `*`，只提示未持久化。
7. 不新增通用状态框架、事务系统、补偿系统、配置镜像或兼容分支。
8. 不再提供“读取整页 UI 后统一保存”的入口；任何保存只覆盖当前操作明确负责的字段。

## 3. 状态模型

### 3.1 UI 控件是未应用输入的暂存位置

需要点击按钮才能生效的参数，在用户编辑时只改变控件值：

```text
current.cameraExposure = 1000
UI 输入 1500
    → 不写 current
    → 不写 app_settings.json
    → SettingsEditState 标记 camera.exposure 为 dirty
    → 曝光标签显示 *
```

用户手动改回当前生效值：

```text
UI 输入重新变为 1000
    → 与 current 相同
    → dirty 清除
    → * 立即消失
```

不再把整份 UI 输入持续复制到机器设置 draft。

### 3.2 `SettingsEditState` 是唯一未应用状态来源

`SettingsEditState` 继续使用现有 global key 和两个 template dirty 字段：

- global key：相机、图像、PLC 和纸巾阈值；
- template dirty：目标字符、图像合格阈值。

每次相关控件变化后固定执行：

```text
比较 UI 值和已生效值
    → 写入 SettingsEditState
    → 根据 dirty 更新标签原文本或“原文本 + *”
```

代码不得通过读取 QLabel 文本中是否含有 `*` 来反推状态；`*` 只是 `SettingsEditState` 的显示结果。

启动检查只使用：

```cpp
SettingsEditState::dirtyNames()
```

删除 Application 中基于 `SettingsApplicationService::hasUnappliedChanges()` 的第二套判断。

### 3.3 `current()` 是本次会话生效值

`SettingsApplicationService` 最终只保存：

```cpp
AppSettings m_current;
```

`m_current` 在启动时由 `app_settings.json` 加载，此后只在明确操作成功后更新。它是以下流程的唯一机器设置来源：

- Runtime 启动；
- 相机打开、预览和检测；
- PLC 连接与生产参数；
- UI 未应用比较和失败恢复；
- 模板页需要读取的检测方案、模板路径和模板保存目录。

程序运行过程中不得为了取得“当前生效参数”重新读取 `app_settings.json`。磁盘文件在硬件已生效但保存失败时可能仍是旧值。

### 3.4 只删除机器设置 draft

从 `SettingsApplicationService` 删除：

```cpp
AppSettings m_draft;

draft()
editableDraft()
updateDraft()
applyDraft()
discardDraft()
hasUnappliedChanges()
```

`TemplateApplicationService::draft()` 继续保留。模板制作存在框选、绘制、字符调整后再保存的真实编辑过程，需要一份完整的 `EditableTemplate`；它与机器配置双状态无关。

## 4. 统一行为合同

### 4.1 需要点击操作才生效的参数

| 参数 | 生效操作 | 修改后 | 操作成功 | 操作失败 |
|---|---|---|---|---|
| 目标字符内容 | 确认字符 / 批量确认 | 显示 `*` | 保存新值并清除 `*` | 恢复本次操作前模板值，清除 `*` |
| 图像合格阈值 | 应用 / 批量应用 | 显示 `*` | 保存新值并清除 `*` | 恢复本次操作前模板值，清除 `*` |
| 纸巾粗糙度阈值 | 应用 | 显示 `*` | 保存新值、更新 `current` 并清除 `*` | 恢复 `current`，清除 `*` |
| 相机曝光 | 设置曝光 | 显示 `*` | 硬件应用、更新 `current`、尝试持久化并清除 `*` | 恢复已生效曝光，清除 `*` |
| 相机增益 | 设置增益 | 显示 `*` | 硬件应用、更新 `current`、尝试持久化并清除 `*` | 恢复已生效增益，清除 `*` |
| 颜色通道 | 应用颜色通道 | 显示 `*` | 保存成功后更新 `current` 并清除 `*` | 恢复 `current`，清除 `*` |
| 图像旋转 | 应用图像旋转 | 显示 `*` | 保存成功后更新 `current` 并清除 `*` | 恢复 `current`，清除 `*` |
| PLC IP、Rack、Slot | 连接 PLC | 显示 `*` | 连接成功、更新 `current`、尝试持久化并清除 `*` | 整组恢复已生效连接参数，清除 `*` |
| PLC 触发模式 | 应用触发模式 | 显示 `*` | PLC 应用、更新 `current`、尝试持久化并清除 `*` | 恢复已生效模式，清除 `*` |
| PLC 拍照距离 | 设置拍照距离 | 显示 `*` | PLC 写入、更新 `current`、尝试持久化并清除 `*` | 恢复已生效距离，清除 `*` |
| PLC 过程参数 | 应用 PLC 过程参数 | 逐项显示 `*` | PLC 整组提交、更新 `current`、尝试持久化并清除本组 `*` | 整组恢复操作前已生效值，清除本组 `*` |

PLC 过程参数组固定包括：拍照距离、拍照时间、硬触发延迟、剔除距离、剔除时间和剔除位置。现有“拍照距离”独立按钮继续只处理拍照距离。

表格中的“操作失败”固定指：

- 输入校验失败；
- 模板校验或模板保存失败；
- 纯配置保存失败；
- 相机设置失败；
- PLC 连接或写入失败。

相机或 PLC 已经采用新值、但随后 `app_settings.json` 保存失败，不属于硬件操作失败，执行第 7.4 节的未持久化规则。

### 4.2 立即生效的软件设置

检测模式、图片保存范围、保存内容、保存路径和硬件触发开关等没有独立应用按钮的现有设置保持“修改即提交”，不显示 `*`：

本次固定的立即生效 key 只有：

```text
detect.mode
image.save_mode
image.save_type
image.save_path
trigger.enabled
```

`plc.ip`、`plc.rack`、`plc.slot` 不在该列表中；它们必须等待连接 PLC 成功后才提交。

五个立即生效 key 的保存责任固定拆分为：

```text
detect.mode
    → 用户手动切换时由 MainWindow::setupDetectModeChangeTracking() 唯一保存
    → 恢复默认时作为五字段批量 candidate 的一部分保存

image.save_mode
image.save_type
image.save_path
trigger.enabled
    → MachineSettingsPage 各自单字段保存
```

```text
UI 变化
    → 从 current 复制局部 candidate
    → 只写入对应字段
    → 保存 app_settings.json
        → 成功：candidate 写入 current
        → 失败：current 不变，UI 恢复 current，显示保存失败
```

立即生效项不再借用 machine draft 暂存，也不会产生不可见的 `current != draft` 状态。

### 4.3 模板批量操作

目标字符和图像阈值批量操作固定为：

```text
逐个尝试所有已选择模板
    → 加载
    → 修改
    → 保存
    → 失败则记录并继续下一个

全部结束
    → 无失败：显示一次成功提示
    → 有失败：显示一次失败汇总，不再显示额外成功弹窗
    → 重新加载当前编辑模板
```

当前编辑模板的 UI 规则：

- 当前模板成功：重新加载后显示新参数并清除本项 `*`；
- 当前模板失败：重新加载后显示磁盘中的原参数并清除本项 `*`；
- 其他模板成功或失败不得直接决定当前 UI 值；
- 批量结束统一重新加载当前模板，本次未涉及的另一个模板参数也恢复磁盘值，允许丢弃其尚未应用输入并清除对应 `*`。

## 5. “原来的参数”的唯一定义

失败时不为回退保存第二份长期状态，原值直接来自现有权威对象：

| 参数类型 | 原值来源 |
|---|---|
| 当前模板目标字符和图像阈值 | 点击操作前 `TemplateApplicationService::draft()` 中的模板设置 |
| 批量设置失败模板 | 该模板本次加载得到、修改前的 `EditableTemplate` |
| 纸巾阈值 | `SettingsApplicationService::current().detectionSchemes.tissueRoughnessThreshold` |
| 颜色通道、旋转等纯配置 | `SettingsApplicationService::current()` 中的本次会话生效值 |
| 相机和 PLC 操作失败 | 操作前 `SettingsApplicationService::current()` 中的本次会话生效值 |
| 相机或 PLC 已成功但配置保存失败 | 已先写入 `current()` 的硬件实际新值；磁盘配置仍保持旧内容 |
| 立即生效设置保存失败 | `SettingsApplicationService::current()` 中尚未被候选值替换的旧值 |

局部 candidate 只存在于一次函数调用中：

```cpp
AppSettings candidate = m_settingsService->current();
// 只复制本次操作负责的 UI 字段到 candidate
```

函数结束后 candidate 自动销毁。它不是新的长期 draft，也不新增 `PreviousSettings`、Rollback DTO、历史栈、撤销管理器或参数镜像。

## 6. 当前实现问题

### 6.1 存在两套未应用判断

当前启动检查同时使用：

```cpp
m_settings->hasUnappliedChanges()
|| !command.unappliedChanges.isEmpty()
```

其中前者来自 `m_current != m_draft`，后者来自 `SettingsEditState`。两者可能不一致：

```text
current == draft，但 UI 已输入新值并显示 *
    → 只有 SettingsEditState 能识别

current != draft，但 UI 没有 *
    → Application 阻止启动，用户却看不到具体未应用参数
```

本方案删除前者，只保留 UI 可见、能列出具体参数名称的 `SettingsEditState`。

### 6.2 MachineSettingsPage 直接持有可变 draft 引用

当前 `MainWindow::m_appliedMachineSettings` 和 `MachineSettingsPage::m_appliedSettings` 实际引用 `SettingsApplicationService::editableDraft()`。页面输入、已生效状态和待持久化状态因此混在同一个对象中，导致：

- UI 操作前可能提前修改 draft；
- 保存失败后 draft 与 current 分离；
- `refreshDirty()` 可能拿 UI 与已经被写入新值的 draft 比较，错误清除 `*`；
- 后续保存可能把其他尚未应用字段一起带入配置。

本方案删除可变 draft 引用。页面只读取 `current()` 作比较和恢复，并把指定 UI 字段写入一次性的局部 candidate。

### 6.3 模板参数只记录状态，没有完整显示到标签

`TemplateEditorPage::refreshTemplateDirty()` 已经计算目标字符和图像阈值差异，但没有完整更新 `label_targetText` 和 `label_imageThreshold`，导致状态存在而页面看不到 `*`。当前 `lineEdit_imageThreshold` 还同时进入 `MachineSettingsPageViewBindings` 和 `TemplateEditorViewBindings`：前者只设置校验器和 Tooltip，后者才负责模板参数加载、应用和 dirty。一个模板参数控件被两个页面持有，页面边界不清楚。本次把图像阈值的控件、校验器、Tooltip、应用和 dirty 全部归 `TemplateEditorPage`。

### 6.4 保存一个模板参数可能错误清除另一个参数

`TemplateEditorPage::saveCurrentDraft()` 成功后直接调用 `clearTemplateDirty()`，可能把另一个尚未保存参数的 `*` 一起清除。保存单项后必须重新比较两项，不能无条件清空。

### 6.5 PLC 连接参数被当成立即保存项

`plc.ip`、`plc.rack`、`plc.slot` 当前使用 `requireApply = false`，连接成功前就可能写入设置。它们必须改为连接成功后才生效的待应用参数组。

### 6.6 纸巾阈值职责放错页面，多个失败分支缺少统一反馈

纸巾检测不读取模板，但当前 `TemplateEditorPage` 仍保存纸巾阈值输入框和应用按钮绑定，并负责加载及保存该参数；`MachineSettingsPage` 只设置了校验器。该职责与检测模型不一致。本次把纸巾阈值的控件绑定、dirty、应用和失败恢复完整迁移到 `MachineSettingsPage`，并从 `TemplateEditorPage` 删除对应字段、方法和连接。

相机曝光、增益、PLC 连接、触发模式、拍照距离和过程参数的多个失败分支只报错，没有按本次范围恢复 `current()` 对应值。

### 6.7 旧通用保存链依赖 machine draft

当前仍存在：

```text
MainWindow::saveSettings()
    → MachineSettingsPage::save()
        → syncImmediateSettings()
        → SettingsApplicationService::applyDraft()
```

`MachineSettingsPage` 还通过 `Callbacks::saveSettings` 回调 `MainWindow`，再由 `MainWindow` 转回页面执行保存。该链把多项 UI 输入集中写入 machine draft，与“每次只提交本次字段”的最终模型冲突；其中 `syncImmediateSettings()` 还会同步 PLC IP、Rack、Slot，可能在 PLC 未连接成功时写入配置。

删除 machine draft 后不得把这条链机械改为“读取整页 UI 后调用 `saveConfiguration()`”。否则恢复默认、窗口关闭或保存某个立即生效字段时，仍可能把带 `*` 的曝光、增益、PLC 等未应用输入一起持久化。本次必须删除这四个通用入口，并按第 7.6 节迁移全部调用点。

### 6.8 检测模式存在重复保存和失败状态不一致

当前检测模式下拉框变化时会同时触发：

```text
MachineSettingsPage 的 requireApply = false 处理
    → updateAppliedFromUi("detect.mode")
    → Callbacks::saveSettings
    → 第一次保存

MainWindow::setupDetectModeChangeTracking()
    → 更新当前模式、模板和纸巾区域
    → MainWindow::saveSettings()
    → 第二次保存
```

同一次用户操作因此可能重复写入配置。更重要的是，当前 `MainWindow` 会在确认保存成功前先切换 `m_currentDetectModeId`、模板列表和纸巾区域；保存失败时页面可能已经显示新模式，而 `current()` 和磁盘仍是旧模式。

本方案不增加模式协调器或新回调。检测模式改由现有 `setupDetectModeChangeTracking()` 完成一次保存和相关页面同步；`MachineSettingsPage` 不再为 `detect.mode` 执行立即保存。

## 7. 精简实现设计

### 7.1 SettingsApplicationService 只保留 current

最终公开接口保留现有专用设置操作，并把机器设置通用提交收口为两个含义明确的入口：

```cpp
const AppSettings &current() const;

OperationResult saveConfiguration(
    const AppSettings &candidate);

OperationResult commitAppliedHardwareSettings(
    const AppSettings &appliedSettings);
```

`saveConfiguration()` 用于没有外部硬件既成状态的设置：

```text
保存 candidate 到 app_settings.json
    → 成功：m_current = candidate，返回成功
    → 失败：m_current 不变，返回保存错误
```

`commitAppliedHardwareSettings()` 只能在相机或 PLC 已经成功采用参数后调用：

```text
m_current = appliedSettings
    → 尝试保存 m_current 到 app_settings.json
        → 成功：返回成功
        → 失败：m_current 保留新值，返回保存错误
```

两个入口复用一个现有文件保存实现和同一套 `OperationResult` 错误转换，不新增策略对象、提交模式枚举或事务类。

`saveTemplatePaths()`、`saveTemplatePathsAndDirectory()`、`saveTissueThreshold()` 和 `clearSettings()` 继续使用单一 `m_current`，按各自是否存在外部硬件既成状态选择明确顺序。当前没有调用点的 `SettingsApplicationService::restoreDefaults()` 删除；“恢复默认设置”由 UI 根据硬件状态生成默认显示值，并按第 7.6 节只提交固定的立即生效字段。不得为了兼容旧调用保留 draft 或旧恢复默认接口。

### 7.2 MachineSettingsPage 不再拥有可变 AppSettings 引用

删除 `MachineSettingsPage::m_appliedSettings` 对 `editableDraft()` 的引用。页面保留现有 `SettingsApplicationService *`，按需读取：

```cpp
m_settingsService->current()
```

现有 UI 字段映射收口为一个定向复制方法：

```cpp
void copyUiValuesTo(
    AppSettings &settings,
    const QStringList &keys) const;
```

它只根据 `keys` 把对应控件值写入调用方提供的局部 candidate，不保存配置、不修改 `current()`、不清除 dirty。

定向恢复继续使用两个简单入口：

```cpp
void restoreAppliedValue(const QString &key);
void restoreAppliedValues(const QStringList &keys);
```

它们只执行：

1. 从 `current()` 读取对应值；
2. 使用 `QSignalBlocker` 写回控件；
3. 重新比较并清除相应 dirty；
4. 恢复标签原文本。

不再执行“同步 machine draft”步骤，因为 machine draft 已删除。曝光失败只恢复曝光，增益失败只恢复增益；只有 PLC 连接组和 PLC 过程参数组按按钮实际职责整组恢复。

纸巾阈值的页面边界固定为：

```text
MachineSettingsPageViewBindings
    → label_tissueRoughnessThreshold
    → lineEdit_tissueRoughnessThreshold
    → pushButton_applyTissueRoughnessThreshold

MachineSettingsPage
    → 设置现有数值校验器
    → 注册 tissue.roughness_threshold 为 requireApply = true
    → 监听输入并更新 SettingsEditState 和标签 *
    → 连接应用按钮
    → 保存成功后刷新 dirty
    → 校验或保存失败后恢复 current 并清除 *
```

初始化和纸巾阈值应用失败时，从 `current().detectionSchemes.tissueRoughnessThreshold` 恢复该控件。恢复默认成功后，`MachineSettingsPage::applyToUi(editableDefaults)` 显示默认纸巾阈值，但不把该值写入 `current()` 或 `app_settings.json`；随后重新比较 UI 与 `current()`，二者不同时显示 `*`，等待用户点击纸巾阈值应用按钮。恢复默认的五字段 candidate 保存失败时，不显示默认纸巾阈值，页面保持/恢复操作前 `current()`。`MachineSettingsPage::applyOperationState()` 使用现有 `generalSettings` 权限控制纸巾阈值输入框、标签和应用按钮，不增加新的权限类型。

`TemplateEditorViewBindings` 删除纸巾阈值输入框和应用按钮；`TemplateEditorPage` 删除 `applyCurrentTissueThreshold()`、纸巾模式加载赋值、纸巾控件访问控制和按钮连接。模式切换时纸巾参数区域是否可见仍由现有 MainWindow 模式可见性装配负责，不把模板业务重新引入纸巾参数链。

### 7.3 未应用标记只比较 UI 与生效值

`MachineSettingsPage::isDirtyByValue()` 固定比较：

```text
当前控件值
vs
SettingsApplicationService::current() 对应字段
```

模板页固定比较：

```text
当前控件值
vs
TemplateApplicationService::draft() 中当前模板已加载值
```

纸巾阈值由 `MachineSettingsPage` 使用固定 key `tissue.roughness_threshold` 注册到 `SettingsEditState`，并与其他整机参数一样比较 UI 和 `current()`。标签只使用“原文本”和“原文本 + ` *`”，不新增 QSS、图标、动画或 Tooltip。

### 7.4 相机和 PLC 硬件参数

每个硬件按钮使用相同的简单顺序：

```text
original = current
candidate = current
copyUiValuesTo(candidate, 本次 keys)
    → 校验
    → 向硬件下发 candidate 对应字段
```

硬件操作失败：

```text
current 不变
    → restoreAppliedValues(本次 keys)
    → 清除本次 *
    → 显示硬件失败
```

硬件操作成功：

```text
commitAppliedHardwareSettings(candidate)
    → current 先更新为硬件实际值
    → 清除本次 *
    → 尝试保存配置
```

配置保存成功：显示现有普通成功信息。

配置保存失败：

```text
真实硬件 = candidate
UI = candidate
current = candidate
磁盘 = 原配置
    → 不恢复旧值
    → 不向硬件发送反向设置
    → 不显示普通成功提示
    → 提示“参数已下发，但保存配置失败，重启后可能不会保留”
```

本次在 `MainWindow` 中新增并统一使用一个复用硬件成功分支的私有方法：

```cpp
bool saveAppliedHardwareSettings(
    const QStringList &keys);
```

它只负责从 `current()` 建立 candidate、调用 `copyUiValuesTo()`、调用 `commitAppliedHardwareSettings()`、刷新对应 dirty 并显示未持久化错误。它不得执行硬件操作，也不得接受颜色通道、图像旋转等纯配置项。

### 7.5 纯配置参数

颜色通道、图像旋转、纸巾阈值等没有外部硬件既成状态的参数固定为：

```text
candidate = current
copyUiValuesTo(candidate, 本次 keys)
    → 校验
    → saveConfiguration(candidate)
```

成功：

```text
磁盘 = candidate
current = candidate
UI = candidate
    → 清除本次 *
    → 显示成功信息
```

失败：

```text
磁盘和 current 均保持旧值
    → restoreAppliedValues(本次 keys)
    → 清除本次 *
    → 显示保存失败
    → 不显示普通成功信息
```

纯配置保存失败不得调用 `commitAppliedHardwareSettings()`，也不得把失败 candidate 确认为本次会话生效值。

纸巾阈值不需要通过通用 `copyUiValuesTo()` 绕到 MainWindow。`MachineSettingsPage` 在自身应用方法中读取并校验输入，然后直接调用现有：

```cpp
SettingsApplicationService::saveTissueThreshold(value)
```

该服务方法内部从 `current()` 创建 candidate，只修改 `detectionSchemes.tissueRoughnessThreshold`，再执行 `saveConfiguration()`。保存成功后 `current()` 更新；保存失败时 `current()` 保持原值，页面恢复原阈值并清除 `*`。

### 7.6 立即生效设置

五个立即生效项不进入 `SettingsEditState`，但保存责任不混在一起。图片保存范围、保存内容、保存路径和硬件触发开关由 `MachineSettingsPage` 在输入变化后直接构造只修改本字段的 candidate 并调用 `saveConfiguration()`：

```text
控件变化
    → candidate = current()
    → copyUiValuesTo(candidate, 当前 key)
    → saveConfiguration(candidate)
```

保存失败时使用 `QSignalBlocker` 把该控件恢复为 `current()`；不保留不可见的待保存状态，不新增 `*` 或重试队列。

PLC IP、Rack、Slot 改为 `requireApply = true`，并从立即同步列表中删除。其他立即生效项保持现有交互语义。

检测模式是唯一的协调型立即生效项。用户手动切换检测模式时，固定由现有：

```cpp
MainWindow::setupDetectModeChangeTracking()
```

负责保存。`MachineSettingsPage` 继续持有 `detect.mode` 的强类型控件绑定，用于初始化、从 `current()` 恢复和现有操作权限禁用；其通用立即变化处理必须跳过 `detect.mode`，不得因用户手动切换调用 `saveConfiguration()`。

用户手动切换检测模式的唯一链路固定为：

```text
用户选择 nextModeId
    → candidate = current()
    → candidate.detectModeId = nextModeId
    → saveConfiguration(candidate)

保存成功
    → resetTemplateCaptureState()
    → m_currentDetectModeId = nextModeId
    → 更新纸巾阈值区域可见性
    → 取消旧模板绘制
    → 模式确实变化时清理旧模板编辑状态
    → 刷新模板编辑区并加载新模式模板列表

保存失败
    → current() 和磁盘保持 previousModeId
    → 使用 QSignalBlocker 把检测模式下拉框恢复到 current().detectModeId
    → m_currentDetectModeId 恢复为 current().detectModeId
    → 恢复该模式的纸巾区域可见性和模板列表
    → 显示一次配置保存失败
```

保存成功前不得清理模板制作状态或发布新模式页面状态；恢复下拉框时不得再次触发保存。该链只使用现有 `MainWindow`、`MachineSettingsPage` 和 `SettingsApplicationService`，不新增模式事务、回滚对象或专用协调类。

恢复默认不模拟用户手动切换，也不触发 `setupDetectModeChangeTracking()` 逐项保存。它是独立的批量恢复操作，按下文只调用一次 `saveConfiguration(candidate)`，其中允许同时包含默认 `detectModeId` 和其他四个立即生效字段；保存成功后再统一更新检测模式及其关联页面。该例外不构成第二条“用户手动切换模式”保存链。

旧通用保存链一次性删除：

```cpp
MainWindow::saveSettings()
MachineSettingsPage::save()
MachineSettingsPage::syncImmediateSettings()
MachineSettingsPage::Callbacks::saveSettings
```

`MachineSettingsPage::Callbacks` 中其他现有 UI 通知继续保留，只删除 `saveSettings` 成员。所有旧 `saveSettings(...)` 调用点必须按调用目的迁移到单字段纯配置提交、硬件成功提交、恢复默认或关闭窗口保存，不保留同名转发函数。

恢复默认设置固定为：

```text
defaults = defaultsForHardwareState(current, 设备状态)
candidate = current()
candidate 只从 defaults 取得以下五个立即生效字段：
    detectModeId
    imageSaveModeId
    imageSaveTypeId
    imageSavePath
    triggerEnabled
    → saveConfiguration(candidate)

保存成功
    → 将 defaults 显示到机器设置控件
    → 五个立即生效字段无 *
    → 需要点击操作的默认值只留在 UI，并根据它们与 current() 的差异显示 *
    → 检测模式、模板列表和纸巾阈值区域统一切换到默认模式对应状态

保存失败
    → current 和 UI 保持/恢复操作前值
    → 显示一次恢复默认失败
```

恢复默认不得提交曝光、增益、颜色通道、图像旋转、纸巾阈值、PLC 连接参数、PLC 触发模式或 PLC 过程参数等需要点击操作的字段，也不得提交模板设置。`rightPanelSplitterState` 在恢复默认时保持 `current()` 中的现值。

窗口关闭固定为：

```text
candidate = current()
candidate.rightPanelSplitterState = splitter_mainContent->saveState()
    → saveConfiguration(candidate)
    → 继续现有关闭流程
```

关闭窗口只从 UI 读取 `rightPanelSplitterState`；不得读取或复制任何其他设置控件。普通立即生效字段已经在变化时保存，带 `*` 的曝光、增益、颜色通道、图像旋转、纸巾阈值和 PLC 参数不得因关闭窗口而写入配置。不新增关闭专用 Service、DTO 或批量字段收集器。

### 7.7 启动检测

启动按钮固定执行：

```text
MachineSettingsPage::refreshAllDirty()
TemplateEditorPage::refreshTemplateDirty()
    → command.unappliedChanges = SettingsEditState::dirtyNames()
```

Application 只判断：

```cpp
access.dirtySettings =
    !command.unappliedChanges.isEmpty();
```

存在未应用参数时先阻止直接启动并显示具体参数名称。用户选择放弃未应用修改并继续时：

```text
机器参数控件恢复 current
模板参数恢复当前已加载/已保存值
清除对应 dirty 和 *
command.unappliedChanges.clear()
再次发起启动
```

不再调用 `discardDraft()`。真正启动时 Runtime 只读取 `SettingsApplicationService::current()`，不会读取 UI 或重新读取配置文件。

### 7.8 相机自动调整曝光路径

`InspectionApplicationService` 中相机自动调整曝光的持久化回调当前通过 `updateDraft()` 和 `applyDraft()` 保存。删除 draft 后改为：

```text
adjusted = current
adjusted.cameraExposure = 自动调整后的曝光
    → saveConfiguration(adjusted)
```

该内部路径继续服从现有 `CameraSession` 的整体成功/失败语义：保存失败仍由现有相机打开或预览恢复流程处理，不改动 `CameraSession`、相机时序或故障自动停止规则，也不经过 UI 的 `saveAppliedHardwareSettings()`。

### 7.9 模板参数与缺字检查

模板页增加两个模板参数标签的 UI 内部绑定：

```text
label_targetText
label_imageThreshold
```

`lineEdit_imageThreshold` 只保留在 `TemplateEditorViewBindings`。从 `MachineSettingsPageViewBindings` 删除该字段，并从 `MachineSettingsPage::setupNumericInputValidators()` 删除对它的访问。现有 `0` 到 `100` 的 `QIntValidator`、最大长度 `3` 和 Tooltip 原样迁到 `TemplateEditorPage` 的初始化代码；不改变阈值范围、文本或保存行为。

纸巾阈值标签属于 `MachineSettingsPageViewBindings`，不进入 `TemplateEditorViewBindings`。

单模板操作：

```text
original = 当前 EditableTemplate draft
candidate = original
candidate 只修改本次字段
    → 校验/保存成功：保留 candidate，重新比较两项 dirty
    → 失败：只恢复本次字段，清除本项 *
```

保存目标字符前立即复用现有字符完整性检查。目标字符为 `20`、模板缺少 `0` 时固定提示：

```text
标题：目标文字保存失败

模板缺少目标文字所需字符：“0”。
目标文字未保存，已恢复为原内容。
```

缺失字符必须使用引号；不得让 `0` 看起来像错误数量或成功数量。运行准备阶段的同类缺字信息保持一致。

批量模板继续使用一个循环完成“加载 → 修改 → 保存 → 失败记录 → 继续”，不建立中间 `QVector<EditableTemplate>`、整体事务或回滚。

## 8. 文件级实施清单

| 文件 | 计划修改 |
|---|---|
| `app/application/settings_application_service.h/.cpp` | 删除机器设置 `m_draft`、draft 访问、`hasUnappliedChanges()` 和未使用的 `restoreDefaults()`；保留单一 `m_current`；增加纯配置保存和硬件已生效提交的明确入口；现有专用保存操作迁移到单一 current |
| `app/application/inspection_application_service.cpp` | 启动检查删除 `hasUnappliedChanges()`；相机自动曝光持久化回调从 `updateDraft()/applyDraft()` 迁移到单一 current 的纯配置保存入口 |
| `app/ui/pages/machine_settings_page.h/.cpp` | 删除可变 draft 引用、`save()`、`syncImmediateSettings()` 和 `Callbacks::saveSettings`；删除 `lineEdit_imageThreshold` ViewBinding 及其校验器/Tooltip 设置；dirty 改为 UI 对比 `current()`；定向复制 UI 字段到局部 candidate；完整接管纸巾阈值；补齐定向恢复和 PLC 连接参数待应用行为；四个普通立即生效项分别提交单字段 candidate；保留 `detect.mode` 控件绑定和禁用规则但不自动保存 |
| `app/ui/main_window.h` | 删除 `m_appliedMachineSettings` 引用和 `saveSettings()` 声明；新增私有 `saveAppliedHardwareSettings(const QStringList &keys)` |
| `app/ui/main_window.cpp` | 构造函数不再绑定 `editableDraft()`；删除 `Callbacks::saveSettings` 装配；将纸巾阈值控件只装配到 `MachineSettingsPageViewBindings`；从 `TemplateEditorViewBindings` 删除纸巾阈值控件；`lineEdit_imageThreshold` 只装配到 `TemplateEditorViewBindings`；补充两个模板参数 QLabel；初始化继续读取 `current()`；自动 PLC 连接成功改走硬件成功提交入口 |
| `app/ui/main_window_settings.cpp` | 删除 `MainWindow::saveSettings()` 并迁移全部调用点；`setupDetectModeChangeTracking()` 成为用户手动切换检测模式的唯一保存入口，成功后更新当前模式、模板列表和纸巾区域，失败恢复 `current()` 模式及页面；恢复默认作为独立五字段批量保存；其他四个立即生效控件各自只保存本字段；相机、颜色通道、PLC 连接、触发模式和过程参数按纯配置/硬件两种顺序提交 |
| `app/ui/main_window_inspection.cpp` | 使用 `current()` 代替旧 draft 引用；图像旋转失败恢复；打开相机附带 PLC 连接成功后提交硬件实际值；放弃未应用修改时不再调用 `discardDraft()`；`closeEvent()` 只从 UI 取得并保存 `rightPanelSplitterState` |
| `app/ui/pages/template_editor_page.h/.cpp` | 删除纸巾阈值 ViewBindings、访问控制、模式加载、应用方法和按钮连接；唯一持有 `lineEdit_imageThreshold`，接管其现有校验器、最大长度和 Tooltip；绑定目标字符与图像阈值两个模板参数标签；逐项显示 `*`；单项失败只恢复本项；批量后重新加载当前模板；修正缺字提示 |
| `app/application/template_application_service.cpp` | 批量设置改为单循环逐个加载、修改和保存；失败继续；最后一次返回汇总 |
| `app/templates/template_store.cpp` | 运行准备阶段缺字提示对缺失字符加引号 |

固定不修改：

- `app/ui/main_window.ui`：需要的标签和按钮已存在，保留当前用户差异；
- `app/ui/controllers/settings_edit_state.h/.cpp`：现有 global map 和两个 template dirty 字段已经足够；纸巾 key 只在 `MachineSettingsPage` 中通过现有 `registerGlobalSetting("tissue.roughness_threshold", ...)` 动态注册，不修改这两个文件；
- `app/AutoOCRproject.pro`：本方案不新增或删除生产代码文件；
- 模板 Schema、AppSettings Schema、检测算法、Runtime、统计、存图和 PLC 地址合同；
- `CameraSession` 内部采集、恢复和故障时序。

## 9. 分阶段实施

### 阶段 1：删除机器配置双状态

1. `SettingsApplicationService` 删除 `m_draft` 和 draft 相关 API。
2. 增加 `saveConfiguration()` 和 `commitAppliedHardwareSettings()` 两种明确提交顺序。
3. `MachineSettingsPage` 删除可变 AppSettings 引用，改为读取 `current()` 并向局部 candidate 定向复制 UI 字段。
4. `MainWindow` 删除 `m_appliedMachineSettings` 对 draft 的引用。
5. `InspectionApplicationService` 迁移相机自动曝光的 draft 调用。
6. 一次性删除 `MainWindow::saveSettings()`、`MachineSettingsPage::save()`、`MachineSettingsPage::syncImmediateSettings()`、`MachineSettingsPage::Callbacks::saveSettings` 及其全部调用/装配点。

阶段结束时不得保留旧接口兼容实现、通用整页保存入口或 `m_current != m_draft` 判断。

### 阶段 2：统一未应用标记和启动检查

1. `MachineSettingsPage` 完整接管纸巾阈值标签、输入框、应用按钮和 dirty；`TemplateEditorPage` 删除纸巾阈值职责。
2. 补齐目标字符和图像阈值两个模板参数标签绑定；从 `MachineSettingsPageViewBindings` 删除 `lineEdit_imageThreshold`，由 `TemplateEditorPage` 唯一持有并接管其现有校验器、最大长度和 Tooltip。
3. PLC IP/Rack/Slot 改为连接前待应用参数，并从立即提交列表移除。
4. `MachineSettingsPage` 保留 `detect.mode` 的控件绑定和操作禁用规则，但从其立即保存处理排除该 key。
5. `setupDetectModeChangeTracking()` 成为用户手动切换检测模式的唯一保存入口，成功后更新模式相关页面，失败恢复 `current()` 模式和页面状态；恢复默认继续作为一次独立五字段批量保存。
6. 所有需点击操作的机器参数固定比较 UI 与 `current()`。
7. Application 启动检查只使用 `command.unappliedChanges`。
8. 放弃未应用修改时从 `current()` 和当前模板值恢复 UI，不调用 `discardDraft()`。

### 阶段 3：提交顺序与失败回退

1. 纯配置项保存成功后才更新 `current()`；失败恢复本次 UI 并清除 `*`。
2. 相机和 PLC 成功后统一调用 `MainWindow::saveAppliedHardwareSettings(keys)`，先更新 `current()`，再持久化；持久化失败保留硬件实际值并提示。
3. 非批量操作只恢复本次按钮负责的字段；PLC 连接和过程参数按既定参数组恢复。
4. 四个普通立即生效项直接保存局部 candidate；用户手动切换检测模式只通过 `setupDetectModeChangeTracking()` 保存；失败均恢复当前生效值，不产生隐藏 dirty。
5. 恢复默认只提交五个立即生效字段；需要点击操作的默认值留在 UI 并按与 `current()` 的差异显示 `*`。
6. `closeEvent()` 只在基于 `current()` 的 candidate 中覆盖 `rightPanelSplitterState`，不得复制其他 UI 输入。
7. 任一失败或未持久化分支不得继续显示普通成功提示。

### 阶段 4：模板与静态收口

1. 模板参数单项成功后重新比较两项 dirty，失败只恢复本项。
2. 批量模板使用单循环，成功保留、失败保持原值，最后一次提示并重新加载当前模板。
3. 目标字符确认和运行准备的缺字提示统一使用带引号字符。
4. 执行静态门禁和差异检查，最后由用户统一构建和人工验证。

四个阶段可在一轮实施中连续完成，但每个阶段结束后必须静态核对；只有用户另行要求时才提交代码。

## 10. 禁止的过度设计、防御性编程和兼容逻辑

实施中明确禁止：

- 新增 SettingsManager、DirtyStateManager、RollbackManager、ParameterRegistry 或通用事务类；
- 新增 EventBus、Command 框架、撤销栈、历史快照、持久化重试器或通用补偿机制；
- 继续保留机器设置 `m_draft`、draft API 或 `m_current != m_draft` 兼容判断；
- 增加“未持久化 dirty”状态、第二种星号或额外状态图标；
- 为每个参数建立独立类、策略对象、DTO 或新文件；
- 用 `findChild()` 字符串查找标签，必须使用现有强类型 ViewBindings；
- 为 `*` 新增 QSS、颜色、图标、动画或 Tooltip；
- 为失败回退复制第二份 AppSettings、长期 previous settings 或参数镜像；
- 为单个按钮从全部 UI 控件重建 AppSettings，必须以 `current()` 为 base 且只覆盖本次 keys；
- 保留或重新引入 `MainWindow::saveSettings()`、`MachineSettingsPage::save()`、`syncImmediateSettings()` 或等价的整页通用保存函数；
- 让 `MachineSettingsPage` 通过回调请求 `MainWindow` 执行配置持久化；
- 用户手动切换检测模式时，让 `MachineSettingsPage` 和 `setupDetectModeChangeTracking()` 同时保存 `detect.mode`，或在保存成功前切换模板和纸巾页面状态；恢复默认的一次五字段批量保存不属于该禁止项；
- 在恢复默认或窗口关闭时遍历/复制所有 UI 设置控件；
- 对相机或 PLC 自动发送旧值作为补偿；
- 修改模板 Schema、AppSettings Schema、检测算法、Runtime、统计、存图、设备地址或采集时序；
- 保留旧接口转发到新接口的过渡层；实施完成时直接删除旧接口和全部调用点。

允许且必须保留的简单机制：

- 一份 `m_current`；
- 每次操作一个栈上局部 candidate；
- 现有 `SettingsEditState`；
- 模板制作需要的 `TemplateApplicationService::draft()`；
- 现有 `OperationResult` 和存储错误转换；
- 定向复制、定向恢复和一个硬件成功后保存的小型复用函数。

## 11. 静态门禁

### 11.1 状态模型

- `SettingsApplicationService` 中只有 `m_current`，不存在机器设置 `m_draft`。
- `draft()`、`editableDraft()`、`updateDraft()`、`applyDraft()`、`discardDraft()`、`hasUnappliedChanges()` 声明、定义和调用点全部删除。
- `TemplateApplicationService::draft()` 继续存在，不得因同名而误删。
- `MainWindow` 和 `MachineSettingsPage` 不再保存指向可变机器设置对象的引用或指针。
- 不存在 `m_current != m_draft` 或等价的第二套未应用判断。
- `SettingsApplicationService::restoreDefaults()` 声明和定义删除；恢复默认直接使用局部 candidate 和 `saveConfiguration()`。

### 11.2 未应用标记和启动

- `textEdit_targetText`、`lineEdit_imageThreshold` 由 `TemplateEditorPage` 跟踪；`lineEdit_tissueRoughnessThreshold` 由 `MachineSettingsPage` 跟踪；三者都有输入变化到 `SettingsEditState` 和对应 QLabel 的路径。
- `MachineSettingsPageViewBindings` 不再声明 `lineEdit_imageThreshold`，`MainWindow` 不再把该控件装配给 `MachineSettingsPage`；`TemplateEditorViewBindings` 是其唯一页面绑定。
- 图像阈值的 `0` 到 `100` 整数校验器、最大长度 `3` 和现有 Tooltip 由 `TemplateEditorPage` 设置，行为与迁移前一致。
- `TemplateEditorPage` 不再声明或访问 `lineEdit_tissueRoughnessThreshold`、`pushButton_applyTissueRoughnessThreshold`，也不存在 `applyCurrentTissueThreshold()`。
- `MachineSettingsPageViewBindings` 唯一持有纸巾阈值标签、输入框和应用按钮；固定 key 为 `tissue.roughness_threshold`，应用失败从 `current()` 恢复并清除 `*`。
- 纸巾阈值初始化和失败恢复显示 `current()`；恢复默认成功显示默认值但不提交该字段，并按与 `current()` 的差异显示 `*`。
- `plc.ip`、`plc.rack`、`plc.slot` 都是 `requireApply = true`，不属于立即提交列表。
- 所有需要点击确认/应用/设置/连接的参数都明确比较 UI 与 `current()` 或当前模板值。
- 用户手动改回已生效值时，对应 dirty 和 `*` 立即消失。
- 启动检查只使用 `command.unappliedChanges`；列表内容与 UI 可见 `*` 一致。
- 放弃未应用修改后从 `current()`/当前模板恢复并清除 `*`，不调用 draft 接口。
- Runtime 启动只读取 `current()`，不读取 UI 或配置文件。

### 11.3 提交和失败顺序

- 每个局部 candidate 都从 `current()` 创建，只覆盖本次按钮的 keys。
- `MainWindow::saveSettings()`、`MachineSettingsPage::save()`、`MachineSettingsPage::syncImmediateSettings()`、`MachineSettingsPage::Callbacks::saveSettings` 的声明、定义、调用和装配点全部删除。
- 不存在读取整页 UI 后统一调用 `saveConfiguration()` 的等价替代实现。
- 立即生效 key 只有 `detect.mode`、`image.save_mode`、`image.save_type`、`image.save_path`、`trigger.enabled`；每次变化只提交当前 key。
- 用户手动切换 `detect.mode` 时，只在 `MainWindow::setupDetectModeChangeTracking()` 中调用一次 `saveConfiguration()`；`MachineSettingsPage` 对该操作不执行自动保存。
- `MachineSettingsPage` 继续对检测模式下拉框执行初始化、`current()` 恢复和操作权限控制，不为此新增第二个状态或回调。
- 检测模式保存成功后才更新 `m_currentDetectModeId`、模板编辑状态、模板列表和纸巾阈值区域；保存失败时四者均与 `current().detectModeId` 一致。
- 检测模式失败恢复使用 `QSignalBlocker`，不得递归触发第二次保存或重复错误提示。
- 恢复默认是独立的批量操作，只调用一次 `saveConfiguration()`，允许 candidate 同时覆盖默认 `detectModeId` 和其他四个立即生效字段；不得逐项触发 `setupDetectModeChangeTracking()` 保存。
- 纯配置项必须先持久化成功再更新 `current()`；失败时 `current()` 保持旧值。
- 相机或 PLC 必须先确认硬件成功，再更新 `current()` 并尝试持久化。
- `MainWindow::saveAppliedHardwareSettings(const QStringList &keys)` 必须存在并供硬件成功分支统一调用；不得把它写成可选实现或复制出多套等价流程。
- 硬件失败时只恢复本次字段或既定参数组，不影响其他未应用输入和 `*`。
- 硬件成功但配置保存失败时，UI、`current()` 和硬件保持新值，磁盘保持旧值。
- 未持久化分支不恢复旧值、不发送反向硬件指令、不显示普通成功提示。
- 立即生效项保存失败时恢复 `current()` 对应值，不留下隐藏待应用状态。
- 恢复默认的持久化 candidate 只从默认值覆盖 `detectModeId`、`imageSaveModeId`、`imageSaveTypeId`、`imageSavePath`、`triggerEnabled`；其他字段保持 `current()`。
- 恢复默认成功后，需要点击操作的默认值只显示在 UI 并正确产生 `*`；恢复默认失败时不留下部分 UI 默认值。
- `closeEvent()` 的 candidate 只覆盖 `rightPanelSplitterState`；关闭前带 `*` 的机器参数不得进入磁盘配置。
- 相机自动曝光不再调用 draft API，且保持现有 CameraSession 成功/失败语义。

### 11.4 模板流程

- 模板保存不再无条件清除全部 template dirty。
- 单项失败只恢复本项；另一个未应用模板参数及其 `*` 保留。
- 批量保存只使用一个逐路径循环，不建立第二轮保存所需的 `QVector<EditableTemplate>`。
- 首个模板失败后继续处理后续模板；成功模板保留新值，失败模板保持原值。
- 所有批量失败在结束后只显示一次；随后统一重新加载当前模板并清除模板 dirty。
- 缺失字符提示使用 `“%1”`，明确本次未保存且已恢复原值。

### 11.5 工程与差异

- 不新增生产代码文件，`.pro` 清单不变。
- `main_window.ui` 零计划差异。
- `app/ui/controllers/settings_edit_state.h/.cpp` 零计划差异；纸巾 dirty key 只通过 `MachineSettingsPage` 使用现有动态注册接口增加。
- 不覆盖、暂存或提交其他现有用户修改。
- UTF-8、文件末尾换行和 `git diff --check` 通过。
- 静态检查不得声称 Qt 构建、相机、PLC 或现场验证已经通过。

## 12. 用户统一验证清单

用户已完成本节统一验证，并于 2026-08-24 确认验证无问题。

1. Run qmake、Clean、Rebuild 并启动程序。
2. 修改需要点击操作的任一参数，确认对应标签立即显示 `*`；手动改回原值后 `*` 立即消失。
3. 修改一个参数但不应用，点击开始识别，确认启动前列出的未应用参数与页面 `*` 完全一致。
4. 在启动提示中放弃未应用修改，确认控件恢复本次会话已生效值、`*` 清除，检测使用 `current()` 启动。
5. 修改立即生效软件设置，确认保存成功后本次运行使用新值且不显示 `*`；模拟保存失败时 UI 恢复旧值。
6. 目标字符改变后显示 `*`；确认成功后清除。
7. 输入模板缺少的字符 `0`，确认后明确提示缺少 `“0”`，控件恢复原内容，`*` 清除。
8. 同时修改目标字符和图像阈值，只应用其中一项，确认另一项输入和 `*` 保留。
9. 批量设置多个模板并制造部分失败，确认后续模板仍处理、成功模板保留、失败模板保持原值、最后只弹出一次汇总。
10. 批量结束后确认当前模板从磁盘重新加载，两个模板参数显示磁盘值并清除模板 `*`。
11. 图像阈值输入非法值，点击应用后恢复原阈值并清除 `*`。
12. 进入纸巾模式，确认纸巾阈值由整机参数页正常加载且不依赖任何模板；修改后显示 `*`，保存成功后 `current()` 使用新值，保存失败时恢复旧值并清除 `*`。
13. 曝光和增益分别修改，确认分别显示 `*`；其中一项设置失败时只恢复失败项，另一项输入和 `*` 不受影响。
14. 相机参数设置成功且配置保存成功时，确认 UI、`current()` 和重启后的配置都是新值。
15. 相机参数设置成功但配置保存失败时，确认 UI、相机和 `current()` 保持新值、`*` 清除，只显示未持久化警告。
16. 颜色通道和图像旋转分别修改；配置保存失败时确认恢复旧值，不把失败 candidate 写入 `current()`。
17. 修改 PLC IP、Rack 或 Slot 任一项，确认共用标签显示 `*`，且输入期间不写入配置。
18. PLC 手动连接成功时三项写入 `current()` 并清除 `*`；连接失败时三项恢复已生效值并清除 `*`。
19. 修改 PLC 连接参数后直接关闭程序，未连接成功的输入不得写入 `app_settings.json`。
20. PLC 触发模式应用失败时恢复原模式并清除 `*`。
21. 拍照距离设置失败时只恢复拍照距离，不清除其他 PLC 参数的 `*`。
22. PLC 过程参数应用失败时整组恢复并清除该组 `*`。
23. 模拟 PLC 操作成功但配置保存失败，确认 UI、PLC 实际值和 `current()` 保持新值，不发送旧值补偿，不显示普通成功提示。
24. 在硬件已生效但未持久化场景中继续启动检测、预览或再次设置参数，确认使用本次会话 `current()` 新值而不是磁盘旧值。
25. 在上述未持久化场景退出且没有后续成功保存，重新启动后确认使用磁盘中的旧值。
26. 验证相机自动调整曝光的打开、恢复和失败行为与修改前一致。
27. 修改曝光、增益、颜色通道、图像旋转、纸巾阈值或任一 PLC 参数但不应用，直接关闭并重启，确认这些 UI 输入均未写入 `app_settings.json`。
28. 调整右侧面板分隔位置后关闭并重启，确认 `rightPanelSplitterState` 正常恢复，且保存该状态没有连带保存任何带 `*` 的参数。
29. 点击恢复默认，确认检测模式、图片保存范围、保存内容、保存路径和硬件触发开关通过一次批量提交立即保存；纸巾阈值等需要点击操作的参数只显示默认值和 `*`，分别操作后才生效。
30. 模拟恢复默认配置保存失败，确认 `current()` 和 UI 回到操作前值，不出现部分字段已经恢复、部分字段未恢复的状态。
31. 切换检测模式并监视配置保存，确认每次用户切换只执行一次 `detect.mode` 保存，模式、模板列表和纸巾阈值区域同步切换。
32. 模拟检测模式保存失败，确认下拉框、`m_currentDetectModeId`、模板列表和纸巾阈值区域全部恢复到 `current().detectModeId` 对应状态，并且只提示一次失败。

## 13. 预期代码与架构结果

实施后机器设置只有一份长期内存状态：

```text
UI 尚未应用输入
    ↓ 比较并生成 *
SettingsEditState

UI 操作成功
    ↓ 提交
SettingsApplicationService::current
    ↓ 持久化
app_settings.json
```

界面参数只有四种用户可见状态：

```text
已生效：无 *
已修改但未生效：有 *
操作失败：恢复已生效值，无 *
硬件已生效但配置未持久化：保留硬件实际新值，无 *，显示明确警告
```

预计代码结果：

- 删除一个长期 `AppSettings m_draft`；
- 删除六个 draft/未应用判断接口及其调用链；
- 删除四个通用整页保存入口及其回调装配，删除未使用的 `SettingsApplicationService::restoreDefaults()`；
- 删除 MainWindow 和 MachineSettingsPage 对可变 draft 的引用；
- 用户手动切换检测模式时由一个现有 MainWindow 入口完成一次保存和页面同步；恢复默认继续使用一次独立批量保存，不新增协调类或重复的手动切换持久化链；
- 硬件成功分支统一复用一个 `MainWindow::saveAppliedHardwareSettings(keys)` 私有方法；
- 增加两个语义明确的设置提交入口和少量定向复制/恢复代码；
- 不增加生产代码文件，不改变 `.pro`；
- 总代码行数不保证大幅下降，因为需要补齐失败恢复和提示，但长期状态、分支来源和调用关系会明显减少；
- 不保留旧接口兼容层，不新增框架、事务、事件总线或防御性状态。

正常保存成功时，`current()` 与磁盘一致；硬件已生效但保存失败时，`current()` 与 UI、真实硬件一致，磁盘保持旧值。本次会话继续使用 `current()`，重启后重新使用磁盘值。
