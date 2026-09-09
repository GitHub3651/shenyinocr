# OCRGangYin 恢复默认设置功能完全删除方案

## 1. 文档状态

- 方案日期：2026-09-09。
- 当前状态：代码实施完成，待用户统一验证。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `9a66aded4487f604a47a87427118d26fe67d832b`；方案编写前工作区和暂存区干净。
- 权威范围：完全删除主程序“恢复默认设置”功能的 UI 入口、连接、权限刷新、声明、实现、专用日志和现行说明。
- 最新决定：用户确认“恢复默认设置”与“清空当前软件数据”职责重叠，前者不再保留；软件只保留“清空当前软件数据”作为完整回到默认配置的用户入口。
- 替代关系：本方案完整替代 `OCRGangYin恢复默认设置范围收紧与直接应用方案.md` 的目标、行为、验证和待验证事项；旧方案只作为历史记录，不再指导生产代码或人工验证。
- 当前进度：五个生产文件已完成按钮、连接、权限、声明、实现和失去职责包装的净删除；现行功能表、Schema、维护指南、计划索引、旧方案状态和执行记录已同步。生产零引用、保留链、UI XML、159 项 qmake 清单、本次 C++ include、严格 UTF-8、末尾换行和 Git 差异检查均通过，尚未执行 Qt Creator 构建或人工交互。
- 项目约束：只执行轻量静态检查；不运行 qmake、编译、链接、测试程序、主程序或真实相机、PLC。

## 2. 最终目标

实施完成后的软件必须满足：

1. 软件设置页不再显示“恢复默认设置”按钮、Tooltip 或任何等价入口。
2. 主程序中不存在恢复默认专用的槽函数、普通成员函数、信号连接、状态权限分支、日志上下文或提示文案。
3. 不把旧入口隐藏、禁用、改名或转发到“清空当前软件数据”；旧功能必须真正消失。
4. 不新增 Reset Service、兼容函数、弃用别名、空实现、模式参数、转发 lambda、动态查找或其他胶水层。
5. “清空当前软件数据”保持唯一完整重置入口：用户确认后删除当前 Windows 用户的 `settings/app_settings.json`，立即退出，下一次启动从 `AppSettings` 唯一默认值建立配置。
6. 清空数据仍不删除产品模板文件、识别图片、授权文件或日志，不改变其现有确认、失败提示和退出顺序。
7. 相机、PLC、图像、模板、CSV 和其他设置继续通过各自现有按钮及保存链生效；删除恢复入口不得削弱这些正常功能。

## 3. 当前真实调用链

### 3.1 待删除链路

```text
software_settings_page.ui
└─ pushButton_restoreDefaultSettings
   ├─ setupSoftwareSettingsPage() 中显式 connect
   │  └─ MainWindow::restoreDefaultMachineSettings()
   │     ├─ 读取 AppSettings::defaults() 和 RuntimeSnapshot
   │     ├─ saveConfiguration() 恢复图像配置和 Splitter 状态
   │     ├─ applyCameraExposure()/applyCameraGain()
   │     ├─ applyPlcTriggerMode()/applyPlcRunSettings()
   │     ├─ commitAppliedHardwareSettings()
   │     ├─ MachineSettingsPage::restoreAppliedValues()
   │     └─ 显示确认、成功、跳过或失败消息
   └─ updateOperationUiState() 中应用 generalSettings 权限
```

该链路全部位于 UI 协调层，没有仍需保留的独立恢复 Service、策略文件、命令类型或测试目标。此前的 `MachineSettingsPolicy` 和 `defaultsForHardwareState()` 已经删除，本方案不重新引入任何同类抽象。

### 3.2 删除后的保留链路

```text
software_settings_page.ui
└─ pushButton_clearSoftwareData
   └─ MainWindow::on_pushButton_clearSoftwareData_clicked()
      ├─ 二次确认
      ├─ 删除 <AppData>/settings/app_settings.json
      ├─ 删除失败时保留程序和现有状态并提示
      └─ 删除成功后提示并退出

下一次启动
└─ AppSettingsStore 加载不到设置文件
   └─ 使用 AppSettings 唯一默认值
```

清空入口继续使用现有直接实现，不包装恢复逻辑，也不新建公共“重置”层。

## 4. 生产代码删除范围

### 4.1 软件设置页 UI

修改 `app/ui/main_window/settings/software_settings_page.ui`：

- 删除 `pushButton_restoreDefaultSettings` 的完整 `<item>` 和 `<widget>`；
- 一并删除该按钮的最小高度、Tooltip 和“恢复默认设置”文字；
- `groupBox_softwareData` 只保留数据目录显示和“清空当前软件数据”按钮；
- 不用空白占位、隐藏控件、占位 Spacer 或新按钮补偿被删除的表单行；
- 页面底部原有伸展项继续承担剩余空间，布局由 `QFormLayout` 自然收缩。

`ui_software_settings_page.h` 是 qmake/uic 生成文件，不在仓库中手工修改，也不为已经删除的控件伪造成员。

### 4.2 MainWindow 声明与实现

修改 `app/ui/main_window/main_window.h`：

- 删除 `restoreDefaultMachineSettings()` 私有声明；
- 删除 `setupSoftwareSettingsPage()` 私有声明，因为去掉恢复按钮连接后该函数只剩一次数据目录赋值，已经失去独立职责；
- 不保留空函数、旧签名、转发声明、弃用标记或条件编译分支。

修改 `app/ui/main_window/main_window_settings.cpp`：

- 完整删除 `MainWindow::restoreDefaultMachineSettings()` 定义；
- 删除确认文案、连接状态分组、恢复字段 key 列表、图像 candidate、硬件 candidate、失败汇总、恢复专用日志和结果提示；
- 完整删除 `MainWindow::setupSoftwareSettingsPage()` 定义；
- 删除随连接和页面包装一起失去使用点的 `ui_software_settings_page.h` 与 `<QPushButton>` include；
- 不从被删除函数中提取 helper，不把任何恢复片段迁往其他文件。

修改 `app/ui/main_window/main_window.cpp`：

- 将 `initializePages()` 中唯一的 `setupSoftwareSettingsPage()` 调用替换为直接设置 `lineEdit_softwareDataDirectory` 的现有一行逻辑；
- 保留 `pushButton_clearSoftwareData` 到 `on_pushButton_clearSoftwareData_clicked()` 的直接连接；
- 不新增新的软件设置页初始化包装函数。

### 4.3 操作状态权限

修改 `app/ui/main_window/main_window_inspection.cpp`：

- 从 `updateOperationUiState()` 删除针对 `pushButton_restoreDefaultSettings` 的 `applyOperationUiAccess()` 调用；
- 保留 `pushButton_clearSoftwareData` 的 `snapshot.generalSettings` 权限；
- 保留 `OperationUiPolicy::generalSettings`，因为清空数据按钮及 `MachineSettingsPage` 的其他通用设置仍在使用；
- 不增加“控件不存在时跳过”的判空或动态查找兼容逻辑。

### 4.4 工程、资源和翻译

- `app/ui/main_window/settings/software_settings_page.ui` 文件本身继续由 `AutoOCRproject.pro` 登记，因此 `.pro` 不需要改动。
- 当前 `Translate_CN.ts`、`Translate_EN.ts` 和正式 QSS 中没有 `pushButton_restoreDefaultSettings`、`restoreDefaultMachineSettings` 或“恢复默认设置”条目，不为零引用文件制造机械改动。
- `Translate_CN.qm`、`Translate_EN.qm` 继续由现有 TS 生成和 QRC 加载；没有对应 TS 消息时不单独改写二进制 QM。
- 不新增或删除图片、SVG、QRC 条目、快捷键、菜单项或其他资源。

## 5. 删除后必须保留的共享代码

以下代码曾被恢复函数调用，但仍有正常业务调用者，不能随恢复功能一起删除：

| 保留对象 | 删除后用途 |
|---|---|
| `AppSettings::defaults()` 及构造默认值 | 首次启动、设置文件被清空后的下一次启动、严格 Schema 重置 |
| `AppSettings::cameraExposure == 300` | 当前唯一相机曝光默认值 |
| 主窗口无已保存 Splitter 状态时的 400px 左侧宽度 | 首次启动及清空设置后的正常布局 |
| `SettingsApplicationService::saveConfiguration()` | 图像、检测、模板、CSV 等现有设置保存 |
| `SettingsApplicationService::commitAppliedHardwareSettings()` | 手动应用相机和 PLC 参数后的真实值提交 |
| `InspectionApplicationService::applyCameraExposure()` / `applyCameraGain()` | 相机参数“设置”按钮 |
| `InspectionApplicationService::applyPlcTriggerMode()` / `applyPlcRunSettings()` | PLC 工作模式和过程参数按钮 |
| `MachineSettingsPage::restoreAppliedValues()` | 手动设置失败回退、未应用参数放弃及其他现有调用 |
| `RuntimeSnapshot` 和 `OperationUiPolicy::generalSettings` | 运行状态、清空数据按钮和其他设置控件权限 |
| `leftDrawerSplitterState` 的启动恢复与关闭保存 | 左侧抽屉正常布局持久化 |

删除“恢复默认设置”不等于删除“默认值”。默认值仍是新配置的唯一来源，并通过“清空当前软件数据”在下次启动生效。相机曝光 300 和左侧抽屉 400px 已成为当前默认合同，本方案不回退为旧数值。

## 6. 明确禁止的残留形式

实施中不得出现：

- 隐藏或永久禁用的 `pushButton_restoreDefaultSettings`；
- 名称变更后的“部分重置”“快速重置”“恢复图像设置”等等价入口；
- `restoreDefaultMachineSettings()` 空实现、内联转发、lambda 转发或兼容别名；
- 把旧按钮转接到 `on_pushButton_clearSoftwareData_clicked()`；
- 为兼容旧生成头而使用 `findChild()`、字符串对象名、宏或判空；
- `context=restore_defaults` 日志、恢复专用字段列表、candidate、错误聚合或提示文案；
- 新的 Reset/Restore Service、Controller、Policy、Manager、Command、DTO 或状态字段；
- 恢复按钮的旧 QSS、翻译、快捷键、菜单或帮助入口；
- 删除共享应用接口后再为现有手动设置链补转发层；
- 修改设置 Schema 或为旧恢复行为增加迁移字段。

## 7. 文档与计划治理

### 7.1 当前说明

实施代码删除时同步修改：

| 文档 | 最终处理 |
|---|---|
| `OCRGangYin计划索引.md` | 本方案更新为代码实施状态；旧范围收紧方案标记为已被本方案替代；任务路由只指向本方案 |
| `OCRGangYin开发者代码结构与维护指南.md` | `main_window_settings.cpp` 职责删除“直接恢复”，只保留设置保存、清空、模式显隐和硬件参数应用 |
| `OCRGangYin新架构数据Schema.md` | 删除在线恢复行为说明，明确完整默认值只用于首次启动、设置重建和既有严格 Schema 流程；清空数据仍不删除外部模板 |
| `OCRGangYin现有功能对照表.md` | 删除顶部现行恢复行为说明；将 `SET-013` 更新为用户确认删除并记录零入口事实；从当前验证范围中移除恢复默认，仅保留首次启动、保存/重启、清空和手动设置 |
| `OCRGangYin重构执行记录.md` | 实施后追加实际删除文件、符号、静态证据和待用户验证项，不改写既有历史记录 |
| `OCRGangYin恢复默认设置范围收紧与直接应用方案.md` | 文档状态改为已被本方案替代，正文只作历史追溯，不再作为当前行为或验证依据 |

### 7.2 历史资料边界

旧阶段计划和执行记录中的“恢复默认设置”描述用于追溯当时行为，不属于生产兼容层，不批量改写或删除。所有现行入口、现行功能说明和任务路由必须以本方案的“功能不存在”结论为准，不能让历史文档继续表现为待执行目标。

## 8. 预计实施文件

### 8.1 生产文件

| 文件 | 预计净修改 |
|---|---|
| `app/ui/main_window/settings/software_settings_page.ui` | 删除恢复按钮完整控件 |
| `app/ui/main_window/main_window_settings.cpp` | 删除恢复连接、完整恢复实现、失去职责的页面初始化包装及专用 include |
| `app/ui/main_window/main_window.h` | 删除两个失去实现的私有声明 |
| `app/ui/main_window/main_window.cpp` | 原位保留软件数据目录赋值，删除包装调用 |
| `app/ui/main_window/main_window_inspection.cpp` | 删除恢复按钮权限刷新 |

不新增生产文件，不删除现有源文件，不修改 `AutoOCRproject.pro`、应用层、Runtime、设备层、设置 Store、Schema 字段、模板或检测代码。

### 8.2 文档文件

- 本方案；
- 计划索引；
- 当前开发者指南；
- 当前数据 Schema；
- 当前功能对照表；
- 重构执行记录；
- 被替代的旧恢复方案状态行。

## 9. 实施顺序

1. 实施前重新记录分支、HEAD、工作区和暂存区，保护所有无关用户修改。
2. 从 `software_settings_page.ui` 删除完整恢复按钮，确认清空数据按钮和页面布局仍完整。
3. 删除 `restoreDefaultMachineSettings()` 的声明与整个定义，不移动或复用其中任何恢复逻辑。
4. 删除显式连接和操作状态权限调用。
5. 删除失去独立职责的 `setupSoftwareSettingsPage()`，把唯一仍需保留的数据目录赋值原位放入 `initializePages()`。
6. 删除由本次变更产生的未使用 include，不顺手清理范围外代码。
7. 更新现行文档、旧方案状态、功能 ID 和执行记录，使当前说明只呈现删除后的最终状态。
8. 执行第 10 节静态门禁；构建和人工交互留给用户统一验证。

## 10. 静态门禁

### 10.1 生产零引用

以下内容在 `app/` 中必须为零：

```text
pushButton_restoreDefaultSettings
restoreDefaultMachineSettings
setupSoftwareSettingsPage
context=restore_defaults
恢复默认设置
将直接恢复以下参数
本次列出的参数已恢复默认并生效
恢复默认设置未完全完成
```

同时确认：

- `main_window_settings.cpp` 不再包含仅服务旧连接的 `<QPushButton>`；
- `software_settings_page.ui` 不含隐藏恢复控件、空占位行或等价按钮；
- 生成 Ui 成员没有任何手写引用；
- 没有新增 restore/reset 兼容类、函数、字段或工程文件。

### 10.2 保留链闭环

- `pushButton_clearSoftwareData` 的 UI 控件、MainWindow 连接、槽声明、槽定义、`generalSettings` 权限和危险按钮 QSS 全部仍存在；
- 清空入口仍只删除 `settings/app_settings.json`，成功后退出，失败时不退出；
- `AppSettings` 相机曝光默认值仍为 300；
- 无已保存 Splitter 状态时仍使用 400px，关闭时仍保存 `leftDrawerSplitterState`；
- 相机、PLC、图像设置、CSV、模板和 dirty 回退所需共享接口仍有其原调用者；
- `SettingsApplicationService`、`InspectionApplicationService`、`MachineSettingsPage` 和 `OperationUiPolicy` 不产生因误删导致的声明/定义或调用缺口。

### 10.3 文件与格式

- `software_settings_page.ui` 可按 XML 严格解析；
- qmake 清单中的所有源、头、UI 和资源路径存在且无重复，已删除的只是控件而不是 `.ui` 文件；
- 本地 quoted include 解析通过，头文件声明与实现核对通过；
- 修改文本为严格 UTF-8、有文件末尾换行、无尾随空白；
- `git diff --check` 和 `git diff --cached --check` 通过；
- 只报告实际完成的静态证据，不表述为构建或运行通过。

## 11. 用户统一验证

1. 在 Qt Creator 执行 Run qmake、Rebuild 并启动主程序，确认 uic 重新生成的软件设置页不存在旧控件成员相关编译错误。
2. 打开“软件设置”页，确认只显示当前软件数据文件夹和“清空当前软件数据”，页面中没有恢复按钮、Tooltip、空白占位或其他等价入口。
3. 在 Idle、Starting、Running、Stopping、Fault 和模板操作状态切换后，确认不会出现恢复入口；“清空当前软件数据”仍按原权限启用或禁用。
4. 点击“清空当前软件数据”后选择取消，确认文件、界面和运行状态均不改变。
5. 再次操作并确认，核对只删除当前用户的 `settings/app_settings.json`、软件立即退出；产品模板、识别图片、授权文件和日志保持存在。
6. 重新启动，确认完整默认配置生效，相机曝光默认显示 300，无已保存抽屉状态时左侧宽度为 400px。
7. 分别验证图像保存、颜色通道、旋转、相机曝光/增益、PLC 连接/工作模式/过程参数、模板和 CSV 的现有设置入口，确认它们不依赖已删除功能。
8. 修改一个带未应用标记的参数后取消或正常应用，确认 dirty、失败回退和启动前提示仍按现有合同工作。

## 12. 完成标准

- 用户界面、MainWindow 声明/实现、连接、状态权限、日志和当前说明中均不存在可执行的“恢复默认设置”功能。
- 生产代码对旧控件名、函数名和恢复日志上下文零引用，不保留隐藏入口、转发、兼容层或胶水层。
- 删除后产生的一行软件数据目录初始化已原位保留，旧 `setupSoftwareSettingsPage()` 包装同时消失。
- “清空当前软件数据”是唯一完整回到默认配置的用户入口，删除范围、退出时机和保留数据不变。
- 默认值、设置 Store、相机/PLC 手动应用、图像设置、模板、CSV、dirty 回退和左侧抽屉持久化均保持现有行为。
- 现行文档和计划路由只把该功能描述为已删除；旧方案仅作历史追溯。
- 静态门禁通过，Qt Creator 构建和人工交互由用户确认。
