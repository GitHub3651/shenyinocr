# OCRGangYin 恢复默认设置范围收紧与直接应用方案

## 1. 文档状态

- 方案日期：2026-09-08。
- 当前状态：代码实施完成，待用户统一验证。
- 权威范围：主程序“恢复默认设置”的参数范围、左侧主 `QSplitter` 分隔位置、连接状态判断、确认提示、直接应用、持久化、失败反馈和旧实现清理。
- 当前进度：恢复入口已改为按连接状态生成确认内容，并直接处理本方案列出的图像、相机和 PLC 参数；图像设置单独保存一次，全部硬件成功结果最多统一提交一次。相机曝光默认值已设为 300，左侧主 `QSplitter` 的启动与恢复默认宽度已统一为 400px。旧 `MachineSettingsPolicy`、整页回填函数、恢复入口中的模板重载、全量 dirty 刷新和冗余运行状态刷新均已删除。生产代码、工程条目、UI 文案及现行说明已同步；静态门禁、UI XML、qmake 清单、UTF-8、尾随空白和差异检查通过。尚未执行构建、真实相机、真实 PLC 或人工验证。
- 替代关系：本方案只在“恢复默认设置”范围内覆盖以下旧结论：
  - `OCRGangYin配置未应用标记与失败回退优化方案.md` 中“恢复后把需要操作的默认值留在 UI 并产生 `*`”的结论；
  - `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 中“恢复默认同时关闭 CSV 并清空目录”的结论；
  - `OCRGangYin左侧导航与页面抽屉迁移方案.md` 中“无保存状态和恢复默认时使用 360px 左侧宽度”的结论，统一改为 400px。
- 保持基线：继续使用唯一 `SettingsApplicationService::current()`、`AppSettings::defaults()`、现有相机/PLC应用接口、现有设置持久化接口和运行中权限控制；不修改设置 Schema、设备接口、检测算法、模板格式或运行时状态机。
- 核心决定：两个入口同时保留，不合并、不互相转发；“恢复默认设置”只做非破坏性的部分参数复位，“清空当前软件数据”继续做完整设置重建。
- 实施门禁：生产代码已按本方案完成；Agent 已执行轻量静态检查。Qt Creator 构建、真实相机、真实 PLC 和人工交互由用户统一验证。

## 2. 目标

“恢复默认设置”调整为一次明确的直接操作：

1. 只处理相机参数、PLC 运行参数、图像设置参数和左侧主 `QSplitter` 分隔位置。
2. 操作前明确列出本次将恢复的参数；相机或 PLC 未连接时明确说明对应参数本次跳过。
3. 可处理的参数直接恢复、直接生效并保存，不再先写入 UI 等待用户逐项点击“设置”或“确认”。
4. 本次操作不得生成新的未应用状态或 `*`。
5. 相机未连接时不处理相机参数；PLC 未连接时不处理 PLC 参数，不自动打开设备、不自动连接 PLC。
6. 除左侧主 `QSplitter` 分隔位置外，不影响其他界面布局、检测模式、产品模板、纸巾阈值、二维码 CSV 及其他软件数据。
7. 删除只服务旧恢复语义的策略、分支、调用和文案，不保留兼容入口或转发层。

### 2.1 与“清空当前软件数据”的边界

两个功能解决不同问题，最终固定为：

| 功能 | 定位 | 行为 | 影响范围 |
|---|---|---|---|
| 恢复默认设置 | 小范围、在线、非破坏性复位 | 一次确认后直接应用本方案列出的参数，不退出软件 | 五个图像设置、左侧主 `QSplitter` 分隔位置、已打开相机的曝光/增益、已连接 PLC 的工作模式和过程参数 |
| 清空当前软件数据 | 完整设置重建 | 删除当前用户的 `app_settings.json`，立即退出，下次启动加载完整默认设置 | 整份 `AppSettings`；仍不删除产品模板文件、识别图片、授权文件或日志 |

“恢复默认设置”不得调用、包装或转发到清空数据入口；清空数据入口也不复用本方案的在线设备设置流程。两者各自保留一个直接入口，不增加公共 Reset Service 或模式参数。

## 3. 最终参数范围

参数默认值只从 `AppSettings::defaults()` 取得，不在恢复函数中复制参数常量。`QSplitter` 宽度不是 `AppSettings` 参数字段：无已保存状态时的启动宽度和恢复默认后的当前宽度统一为 400px，两处都直接使用 `400` 和当前 Splitter 总宽度减 `400`，不新增布局常量类或兼容逻辑。

### 3.1 始终直接恢复的图像设置和界面位置

| UI 参数 | AppSettings 字段 | 默认结果 |
|---|---|---|
| 图像保存范围 | `imageSaveModeId` | 全部不保存 |
| 保存图像类型 | `imageSaveTypeId` | 只保存带识别框图像 |
| 图像保存路径 | `imageSavePath` | 空 |
| 颜色通道 | `colorChannelId` | 彩色通道 |
| 图像旋转 | `imageRotationId` | 无旋转 |
| 左侧主 `QSplitter` 分隔位置 | `leftDrawerSplitterState` | 清空已保存状态，当前左侧宽度恢复为 400px |

以上内容不依赖外部设备。用户确认后，以 `current()` 为基础构造一个局部 candidate，只覆盖五个图像设置字段并清空 `leftDrawerSplitterState`，然后调用现有 `saveConfiguration()`。保存成功后更新对应图像 UI，并把当前左侧主 `QSplitter` 宽度设为 400px；保存失败时保持原 `current()`、原图像 UI 和当前分隔位置，不产生 `*`。

### 3.2 相机已打开时直接恢复的相机参数

| UI 参数 | AppSettings 字段 | 默认值 |
|---|---|---|
| 相机曝光 | `cameraExposure` | `300` |
| 相机增益 | `cameraGain` | `1` |

- 只有 `runtimeSnapshot().cameraOpen == true` 时才列入确认内容并直接下发。
- 相机未打开时整组跳过，不改 UI、不改 `current()`、不改配置文件，也不尝试打开相机。
- 参数下发成功后把接口返回的 `actualValue` 写入本次局部硬件 candidate；参数下发失败时保留 candidate 中的原生效值。曝光和增益都处理完后不单独保存，由全部相机和 PLC 操作共用的最多一次硬件提交统一更新 `current()` 并尝试持久化。

### 3.3 PLC 已连接时直接恢复的 PLC 参数

| UI 参数 | AppSettings 字段 | 默认值 | 直接处理方式 |
|---|---|---|---|
| PLC 工作模式 | `triggerModeId` | 连续触发模式 | 通过现有工作模式接口下发 PLC |
| 拍照距离 | `photoDistance` | `50` | 通过现有过程参数组下发 PLC |
| 拍照时间 | `photoTime` | `300` | 通过现有过程参数组下发 PLC |
| 硬触发延时 | `cameraDelay` | `300` | 不写 PLC；作为相机硬触发运行配置保存 |
| 剔除距离 | `rejectDistance` | `500` | 通过现有过程参数组下发 PLC |
| 剔除时间 | `rejectTime` | `300` | 通过现有过程参数组下发 PLC |
| 剔除位置 | `rejectPosition` | `0` | 不写 PLC；作为延迟剔除运行配置保存 |

- 只有 `runtimeSnapshot().plcConnected == true` 时才列入确认内容并处理。
- PLC 未连接时整组跳过，不改 UI、不改 `current()`、不改配置文件，也不尝试连接 PLC。
- 工作模式沿用现有单项 PLC 应用链，成功后把默认工作模式写入本次局部硬件 candidate。现有 `applyPlcRunSettings()` 只成组下发拍照距离、拍照时间、剔除距离和剔除时间四项；该组成功后才把六个默认过程配置字段写入 candidate，包括不写 PLC 的 `cameraDelay` 和 `rejectPosition`。
- 工作模式和过程参数组互相独立，某组失败不阻止另一组按自身结果处理。
- 过程参数组失败时不把六个默认过程配置字段写入 candidate，UI 和 `current()` 保持操作前值。现有接口按固定顺序写四项并在首个失败处返回，失败前可能已经写入部分 PLC 值；本方案不增加回读、逐项结果、补偿写入或重试，也不声称 PLC 内部已经回滚。

### 3.4 明确不恢复的内容

以下内容不属于本次“恢复默认设置”：

- 检测模式 `detectModeId`；
- PLC 硬触发启用开关 `triggerEnabled`；
- PLC 连接配置 `plcIp`、`plcRack`、`plcSlot`；
- 固定 PLC DB、偏移地址；
- 纸巾粗糙度阈值；
- 产品模板路径、模板参数、模板保存目录和模板文件；
- 二维码本机 CSV 开关及输出目录；
- 除左侧主 `QSplitter` 分隔位置以外的界面布局；
- 识别图片、授权文件、日志和其他软件数据。

PLC IP、Rack、Slot 不纳入恢复，是因为它们是建立连接所需的目标配置，不是当前已连接 PLC 可直接下发的运行参数；本方案不增加自动断开、改地址或重新连接流程。

## 4. 用户交互

### 4.1 可操作状态

继续使用现有操作权限：只有系统空闲时允许点击“恢复默认设置”。Starting、Running、Stopping、Fault 和模板操作忙碌状态下保持禁用，不增加第二套状态判断。

### 4.2 确认提示

点击按钮后先读取一次 `RuntimeSnapshot`，在修改 UI、设置或设备之前生成确认内容。提示固定包含：

```text
将直接恢复以下参数，恢复后立即生效，不会产生待应用标记：

图像设置：
- 图像保存范围：全部不保存
- 保存图像类型：只保存带识别框图像
- 图像保存路径：空
- 颜色通道：彩色通道
- 图像旋转：无旋转

界面位置：
- 左侧抽屉分隔位置：400px

相机参数：
- 相机曝光：300
- 相机增益：1

PLC 参数：
- 工作模式：连续触发模式
- 拍照距离：50
- 拍照时间：300
- 硬触发延时：300
- 剔除距离：500
- 剔除时间：300
- 剔除位置：0

其他设置不会改变。是否继续？
```

连接状态决定提示中的设备组：

- 相机未打开时不列出相机参数，改为显示“相机未连接，本次不处理相机参数”。
- PLC 未连接时不列出 PLC 参数，改为显示“PLC 未连接，本次不处理 PLC 参数”。
- 图像设置和左侧抽屉分隔位置始终列出。
- 用户选择“否”后直接返回，不产生任何副作用。

提示中的参数名称直接写在恢复函数中，目标值从本次取得的 `AppSettings::defaults()` 插入文本，不重复保存参数默认常量；`QSplitter` 位置使用固定 400px。不新增参数描述注册表、通用格式化器或确认对话框类。

### 4.3 完成反馈

本次操作结束后只显示一次结果提示：

- 全部成功：提示本次列出的参数已恢复默认。
- 有跳过：在成功提示中说明相机或 PLC 因未连接未处理。
- 有失败：列出失败的参数或参数组及现有错误信息；成功部分保留，不额外显示普通成功弹窗。
- 至少一个硬件参数已经成功应用、但本次唯一一次硬件配置提交保存失败：保留硬件实际值和 `current()`，明确提示重启后可能仍使用磁盘旧值。
- PLC 过程参数组失败时只报告该组和现有接口返回的失败字段，不推断失败前已经写入的 PLC 字段仍是原值。

不增加重试按钮、失败队列、自动补偿或恢复历史。

## 5. 直接应用与状态顺序

### 5.1 图像设置和左侧主 `QSplitter` 分隔位置

```text
candidate = current()
candidate 只覆盖五个图像设置字段并清空 leftDrawerSplitterState
    -> saveConfiguration(candidate)
        -> 成功：current 更新，图像 UI 显示默认值，当前分隔位置恢复为 400px
        -> 失败：current、图像 UI 和当前分隔位置保持原值
```

图像设置不再借用各控件的 change 信号逐字段保存，也不调用整页 `applyMachineSettingsToUi()`；`QSplitter` 只在设置保存成功后调整一次。

### 5.2 相机参数

```text
相机已打开
    -> 直接调用应用服务下发默认曝光
    -> 成功后把返回的 actualValue 写入局部硬件 candidate
    -> 直接调用应用服务下发默认增益
    -> 成功后把返回的 actualValue 写入同一个 candidate

相机未打开
    -> 整组不处理
```

曝光和增益直接使用现有应用服务接口与 `CameraParameterResultDto`，不借用读取控件值的 `applyCameraExposureFromUi()`、`applyCameraGainFromUi()` 或 `saveAppliedHardwareSettings()`。恢复函数原位记录结果，不为相机单独调用 `commitAppliedHardwareSettings()`，也不新增相机恢复 Service、命令类型、批量事务或转发 helper。

### 5.3 PLC 参数

```text
PLC 已连接
    -> 直接应用默认工作模式
    -> 成功后把默认工作模式写入局部硬件 candidate
    -> 直接整组下发拍照距离、拍照时间、剔除距离、剔除时间四项
    -> 四项组成功后把六个默认过程配置字段写入同一个 candidate
       其中 cameraDelay 和 rejectPosition 只保存配置，不写 PLC

PLC 未连接
    -> 整组不处理
```

工作模式与四项过程参数是两个现有设备操作边界。恢复函数直接调用应用服务，不借用读取控件值的 `applyPlcTriggerModeFromUi()`、`applyPlcRunSettingsFromUi()` 或 `saveAppliedHardwareSettings()`。两组只共享局部硬件 candidate，不合并成新的 PLC 批量协议，也不在一组失败时回滚另一组；四项过程写入内部失败时不增加回读或补偿。

### 5.4 硬件结果统一提交

```text
hardwareCandidate = 图像设置处理完成后的 current()
相机和 PLC 各操作只把成功生效的结果写入 hardwareCandidate
    -> 至少一个硬件字段成功：全部硬件操作结束后调用一次 commitAppliedHardwareSettings(hardwareCandidate)
    -> 没有硬件字段成功：不调用 commitAppliedHardwareSettings()
    -> 最后按本次实际处理的字段从 current 定向回填 UI
```

同一次恢复中，`commitAppliedHardwareSettings()` 最多调用一次。这样既保留“硬件已经生效时 `current()` 服从硬件结果”的现有语义，也避免按字段反复写盘以及后一次保存覆盖前一次保存失败所造成的结果歧义。局部 candidate、成功标记、处理字段和错误列表均留在恢复函数内，不新增持久化协调器、批量 Service 或结果 DTO。

图像设置仍单独先调用一次 `saveConfiguration()`：保存成功时，硬件 candidate 以已恢复的图像配置为基础；保存失败时，硬件 candidate 以未改变的原 `current()` 为基础。因此后续硬件提交不会把保存失败的图像默认值带入配置。

### 5.5 未应用状态

- 恢复操作不得把默认值先写入 UI 再调用 `refreshAllDirty()`。
- 全部硬件操作和最多一次硬件提交完成后，成功和失败字段统一从 `current()` 回填 UI，结果必须与 `current()` 一致且无 `*`。
- 每个失败字段的 UI 恢复原 `current()` 值且无 `*`；PLC 过程组内部已经完成的前序硬件写入不属于 UI 可确认的回滚结果。
- 未连接而跳过的设备字段完全不参与本次恢复；本次操作不为它们新增或清除状态。
- 不影响范围外已有的模板参数或其他设置状态。

## 6. 代码修改与旧实现删除

### 6.1 `MainWindow` 恢复入口

重写 `MainWindow::restoreDefaultMachineSettings()`：

- 删除旧的 `editableDefaults` 混合对象；
- 删除检测模式、硬触发和 CSV 的恢复赋值；
- 保留并收口 Splitter 恢复：candidate 清空 `leftDrawerSplitterState`，保存成功后用 `400` 和当前 Splitter 总宽度减 `400` 设置两侧尺寸；
- 删除恢复入口对 `applyMachineSettingsToUi()` 的全部调用，并删除失去所有调用者的声明和定义；
- 删除模板重载和 `clearTemplateDirty()`；
- 删除恢复后 `refreshAllDirty()` 产生待应用 `*` 的流程；
- 删除恢复入口中的 `updateOperationUiState()` 调用；恢复操作不改变运行状态或设备连接状态，已处理字段由 `restoreAppliedValues()` 定向回填，不再触发额外的整页权限与设备 UI 刷新；
- 删除“带 * 的参数需要点击对应【设置】后才会生效”旧提示；
- 改为动态确认，按图像与界面位置、相机、PLC 三组直接处理，并只显示一次最终结果提示；
- 图像设置单独保存一次；相机与 PLC 的成功结果汇总到一个局部硬件 candidate，全部硬件操作结束后最多调用一次 `commitAppliedHardwareSettings()`；
- 恢复入口不调用任何 `*FromUi()` 或 `saveAppliedHardwareSettings()`；直接复用现有 `saveConfiguration()`、应用服务硬件接口、`commitAppliedHardwareSettings()`、`restoreAppliedValues()` 和现有日志类别。

不新增恢复控制器、参数注册表、默认设置 DTO、事务类或跨层回调。

### 6.2 删除旧硬件状态默认策略

新逻辑不再需要“根据硬件状态生成一份部分默认、部分 current 的完整 `AppSettings`”，因此完整删除：

- `MachineSettingsPage::defaultsForHardwareState()` 声明和定义；
- `system_support/machine_settings_policy.h`；
- `system_support/machine_settings_policy.cpp`；
- 对 `machine_settings_policy.h` 的 include；
- `AutoOCRproject.pro` 中两个工程条目；
- `app/system_support/README.md` 和当前开发者指南中的现行模块说明。

历史计划和执行记录保留原文用于追溯，不在旧文档中伪造新历史。

### 6.3 UI 文案

更新软件设置页中恢复按钮 Tooltip，使其明确只恢复图像设置、左侧抽屉分隔位置，以及已连接相机和 PLC 的可直接应用参数；不再称为笼统的“软件公共界面设置”。按钮文字仍保持“恢复默认设置”，不新增按钮或页面。

### 6.4 预计实施文件范围

| 文件 | 修改 |
|---|---|
| `app/system_support/settings/app_settings.cpp` | 将 `cameraExposure` 的唯一默认值设为 300 |
| `app/ui/main_window/main_window.cpp` | 无已保存 Splitter 状态时使用 `400` 和当前总宽度减 `400` 设置两侧尺寸 |
| `app/ui/main_window/main_window_settings.cpp` | 重写恢复入口，直接调用应用服务；图像设置单独保存一次，硬件成功结果统一提交一次；删除整页设置回填函数定义和恢复入口中的冗余运行状态刷新调用 |
| `app/ui/main_window/main_window.h` | 删除 `applyMachineSettingsToUi()` 声明；不新增恢复抽象 |
| `app/ui/main_window/settings/machine_settings_page.h/.cpp` | 删除旧硬件状态默认接口及 include |
| `app/ui/main_window/settings/software_settings_page.ui` | 更新恢复按钮 Tooltip |
| `app/system_support/machine_settings_policy.h/.cpp` | 删除文件 |
| `app/AutoOCRproject.pro` | 删除两个已删除文件条目 |
| `app/system_support/README.md` | 删除旧策略模块说明 |
| `app/ui/README.md` | 将现行首次抽屉宽度说明更新为 400px |
| `docs/development/OCRGangYin新架构数据Schema.md` | 同步曝光默认值 300、Splitter 默认 400px 和当前恢复范围说明，不修改 Schema 版本 |
| `docs/development/OCRGangYin开发者代码结构与维护指南.md` | 更新现行恢复调用链与文件职责 |
| `docs/development/OCRGangYin现有功能对照表.md` | 按现有功能项同步最终行为和验证状态 |
| `docs/development/OCRGangYin重构执行记录.md` | 实施后记录实际差异、静态证据和待验证项 |
| `docs/development/OCRGangYin计划索引.md` | 更新本方案实施状态 |

历史计划和既有执行记录中的历史数值保留原文；只更新现行说明和本次实施记录，不为统一搜索结果改写历史。

## 7. 保持不变

- `AppSettings` 字段、Schema 8、JSON 键、严格读取和原子保存方式；
- `SettingsApplicationService` 的单一 `current()` 模型；
- 普通手动设置时的 `*`、失败回退和硬件已生效但未持久化语义；
- 相机打开、关闭、预览、采集和故障时序；
- PLC 连接、断开、写入地址和生产控制时序；
- 检测模式、模板选择、模板编辑、模板文件和纸巾阈值；
- 二维码 CSV 设置和运行写入；
- 左侧导航和关闭时的布局保存机制；恢复默认时仅按本方案把当前左侧主 `QSplitter` 分隔位置设为 400px，并清空其已保存状态；
- “清空当前软件数据”的删除设置文件并退出流程。

## 8. 禁止项

- 不新增 `RestoreDefaultsService`、Manager、Controller、Policy、参数注册表或命令框架。
- 不新增机器设置 draft、恢复快照、撤销栈、事务协调器、失败队列或持久化重试器。
- 不自动打开相机、连接 PLC、断开 PLC 或重新连接默认地址。
- 不为未连接设备预写默认值，不产生等待以后应用的 `*`。
- 不通过逐个触发 UI change 信号模拟用户点击。
- 不从恢复入口调用 `applyCameraExposureFromUi()`、`applyCameraGainFromUi()`、`applyPlcTriggerModeFromUi()`、`applyPlcRunSettingsFromUi()` 或 `saveAppliedHardwareSettings()`。
- 不读取整页 UI 后统一保存，不把范围外字段带入 candidate。
- 不按相机字段或 PLC 参数组反复调用 `commitAppliedHardwareSettings()`；同一次恢复最多提交一次硬件结果。
- 不在恢复入口调用 `updateOperationUiState()`；恢复过程不改变运行状态或设备连接状态。
- 不为 PLC 过程组增加字段回读、逐项结果接口、失败重试或旧值补偿，不伪造组内或跨设备原子性。
- 不保留 `MachineSettingsPolicy` 空壳、旧接口转发、弃用别名、兼容分支或胶水层。
- 不增加与当前真实调用链无关的输入检查、连接探测、回退机制或未来扩展点。
- 不顺手修改检测、模板、CSV、`QSplitter` 默认 400px 以外的布局、日志格式或设备协议。

## 9. 实施顺序

1. 实施前记录分支、HEAD、工作区和暂存区，保护所有无关用户修改。
2. 将 `AppSettings::defaults()` 中的相机曝光默认值改为 300，并将无已保存 Splitter 状态时的启动默认左侧宽度改为 400px。
3. 重写恢复入口的范围、动态确认和直接应用顺序；图像设置只构造自身 candidate 并单独保存一次。
4. 相机与 PLC 直接调用应用服务，不经过 UI 包装；PLC 过程组只下发四个设备字段，成功后把六个默认过程配置字段写入局部硬件 candidate，失败时不写 candidate、不回读、不补偿。
5. 全部硬件操作结束后，有成功字段时统一调用一次 `commitAppliedHardwareSettings()`，没有成功字段时不调用；随后只定向回填处理后的 UI，确保本次范围不产生 dirty 或 `*`，并删除恢复入口中的 `updateOperationUiState()` 调用。
6. 删除 `defaultsForHardwareState()`、`MachineSettingsPolicy`、`applyMachineSettingsToUi()` 及全部工程和现行文档引用。
7. 更新 Tooltip、数据 Schema、UI README、当前开发者指南、功能对照表、执行记录和计划索引。
8. 执行静态门禁；由用户在 Qt Creator 和真实设备环境中统一验证。

## 10. 静态门禁

- `MachineSettingsPolicy`、`defaultsForHardwareState`、`machine_settings_policy` 和 `applyMachineSettingsToUi` 在生产代码、工程文件以及现行架构和功能说明中零引用；本方案、计划索引及仅用于追溯的历史计划和执行记录不计入零引用检查。
- 恢复入口不再写入 `detectModeId`、`triggerEnabled`、PLC 连接配置、纸巾阈值、模板或 CSV 字段。
- 恢复入口不再调用 `applyMachineSettingsToUi()`、`restoreTemplatesForMode()`、`clearTemplateDirty()` 或 `refreshAllDirty()`。
- 恢复入口不再调用 `updateOperationUiState()`，也不通过整页状态刷新间接改写未连接设备的 UI。
- 恢复入口不调用四个 `*FromUi()` 包装或 `saveAppliedHardwareSettings()`，也不通过临时改写 UI 传递默认值。
- 图像设置 candidate 只覆盖五个图像设置字段并清空 `leftDrawerSplitterState`。
- 同一次恢复中，图像设置调用 `saveConfiguration()` 最多一次；相机和 PLC 共用一个局部硬件 candidate，`commitAppliedHardwareSettings()` 最多调用一次，没有硬件成功结果时不调用。
- `AppSettings::defaults().cameraExposure` 为 300，恢复确认内容和已连接相机的下发目标均取该值。
- 无已保存 Splitter 状态时，启动和恢复入口都使用 `400` 与当前总宽度减 `400` 设置两侧尺寸；原 360px 默认值在当前启动与恢复实现中零残留。
- 当前 `QSplitter` 分隔位置只在设置保存成功后恢复为 400px；保存失败时保持不变。
- 相机未打开分支不存在相机下发、相机 candidate 或相机 UI 改写。
- PLC 未连接分支不存在 PLC 写入、PLC candidate 或 PLC UI 改写。
- PLC 过程参数命令只包含拍照距离、拍照时间、剔除距离和剔除时间；`cameraDelay` 与 `rejectPosition` 不写 PLC，只在四项组成功后写入六字段硬件 candidate。
- PLC 过程组失败时六个配置字段和对应 UI 保持操作前值；代码中不存在回读、补偿、重试或声称 PLC 已回滚的分支。
- 相机和 PLC 成功分支继续先服从真实硬件结果，再更新 `current()`；纯图像设置继续保存成功后更新 `current()`。
- 本次恢复完成后，所有实际处理字段的 UI 值等于 `current()` 且 dirty 为 false。
- 未新增生产代码文件、兼容接口、第二份设置状态或通用恢复框架。
- `.pro` 文件条目存在、无重复、无已删除文件残留；本地 include 解析通过。
- `.ui` XML、严格 UTF-8、文件末尾换行和 `git diff --check` 通过。
- 数据 Schema、UI README、功能对照表和开发者指南中的现行默认值及恢复说明与生产代码一致；历史计划和旧执行记录不改写。
- Agent 不执行 qmake、编译、链接、主程序运行或硬件测试。

## 11. 用户统一验证

1. Qt Creator 执行 Run qmake、Rebuild 并启动程序。
2. 相机和 PLC 均未连接时点击恢复，确认提示只列出图像设置和左侧抽屉分隔位置，并明确两组设备参数跳过；确认后五个图像设置恢复、当前分隔位置变为 400px，且没有 `*`。
3. 只打开相机时点击恢复，确认提示列出曝光 300、增益 1，并跳过 PLC；确认后两个参数直接下发、保存并且没有 `*`。
4. 只连接 PLC 时点击恢复，确认提示列出图像和 PLC 参数、跳过相机；确认后工作模式与四个设备过程参数写入 PLC，六个过程配置字段保存并且没有 `*`。
5. 相机与 PLC 均连接时点击恢复，确认提示完整列出图像设置、界面位置、相机参数和 PLC 参数，全部直接生效；静态确认全部硬件成功结果只执行一次统一持久化。
6. 在任一待恢复字段已有未应用输入时执行恢复，确认成功字段直接变为默认生效值并清除本字段 `*`。
7. 保留一个范围外模板参数的未应用输入，执行恢复后确认其输入和 `*` 不变，模板没有重新加载。
8. 模拟图像设置保存失败，确认图像设置和当前 `QSplitter` 分隔位置保持原值、无新增 `*`，设备组仍按各自实际结果处理。
9. 模拟相机曝光或增益下发失败，确认失败项保持原生效值，另一项按自身结果处理，不发送补偿值。
10. 模拟 PLC 工作模式失败，确认过程参数组仍按自身结果处理；模拟四项过程写入失败，确认六个过程配置字段和 UI 保持操作前值，只报告现有接口返回的失败字段，不执行回读、补偿或重试。
11. 模拟至少一个硬件参数应用成功但唯一一次硬件配置提交保存失败，确认 UI、`current()` 和硬件保持实际生效值，提示重启后可能使用磁盘旧值，并且不存在后续重复提交。
12. 恢复后确认检测模式、PLC 硬触发开关、PLC IP/Rack/Slot、纸巾阈值、模板和 CSV 设置均未改变；左侧抽屉分隔位置恢复为 400px。
13. 检测运行、停止、Fault 和模板操作忙碌期间确认恢复按钮仍不可用。
14. 关闭并重启，确认成功持久化的恢复值和左侧抽屉 400px 分隔位置保持；未连接而跳过的设备参数保持原配置。
15. 清空软件数据后重新启动，确认相机曝光配置默认显示 300，并因无已保存 Splitter 状态而使用 400px 默认左侧宽度。

## 12. 完成标准

- “恢复默认设置”只影响本方案列出的图像设置、左侧主 `QSplitter` 分隔位置、相机参数和 PLC 参数。
- 相机曝光的全局默认值和已连接相机的恢复目标均为 300。
- PLC 已连接时只向设备下发工作模式和四个现有可写过程参数；六个过程配置字段只在四项组成功后写入局部硬件 candidate。
- 相机和 PLC 的全部成功结果在所有硬件操作结束后统一提交，`commitAppliedHardwareSettings()` 每次恢复最多调用一次。
- 所有可处理参数在一次确认后直接生效，不需要用户再次点击设置，不产生新的 dirty 或 `*`。
- 未连接设备整组跳过，不发生设备操作、配置预写或 UI 假恢复。
- 成功、跳过和失败结果与现有接口可确认的执行结果一致；不推断 PLC 组内失败前字段状态，部分成功不回滚、不补偿。
- 旧混合默认策略及其生产文件、工程条目、调用和现行说明全部删除，恢复入口中的冗余 `updateOperationUiState()` 调用一并删除。
- 范围外设置和用户数据保持不变。
- 静态门禁通过，真实相机、PLC 和人工交互由用户确认。
