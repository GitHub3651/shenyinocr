# OCRGangYin 参数设定页控件迁移与运行交互调整方案

## 1. 文档状态

- 编写日期：2026-09-04。
- 当前状态：代码实施完成，待用户统一验证。
- 权威范围：完整识别模式分组与纸巾粗糙度阈值的页面归属、PLC 工作模式默认值，以及启动识别后的右侧边栏开合行为。
- 当前进度：完整识别模式分组和纸巾粗糙度阈值已迁入参数设定页，生成 Ui 访问已同步，粗糙度冗余预写入已删除，PLC 工作模式默认值已改为连续触发，启动识别后不再自动收起边栏；静态门禁已通过。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `e121bd8`。当前工作区存在既有未提交差异；实施时只继续本方案范围内的修改，不覆盖、回退或混入其他差异。
- 验证门禁：Agent 已完成项目允许的静态检查；Qt Creator 构建、程序运行及真实 PLC 验证由用户统一完成。

## 2. 目标

本方案只完成以下四项调整：

1. 将完整的纸巾粗糙度阈值设置从“图像设置”迁移到“参数设定”，包括：
   - `label_tissueRoughnessThreshold`；
   - `lineEdit_tissueRoughnessThreshold`；
   - `pushButton_applyTissueRoughnessThreshold`。
2. 将完整的 `groupBox_detectionMode` 迁移到“参数设定”页面顶部，包括：
   - `label_detectionMode`；
   - `comboBox_detectionMode`；
   - `checkBox_hardwareTriggerEnabled`。
3. 将 PLC 工作模式的唯一默认值由“间歇触发模式”改为“连续触发模式”。
4. 启动识别成功后保持右侧边栏原有开合状态，不再自动收起。

除上述内容及使其正常成立所必需的引用、布局和说明同步外，不调整其他功能、参数、页面、算法、设备时序或视觉样式。

## 3. 与现有方案的关系

本方案在以下范围内覆盖旧结论：

- 覆盖 `OCRGangYin主窗口UI文件拆分方案.md` 中“识别模式和纸巾阈值属于图像设置页”的页面归属。
- 覆盖 `OCRGangYin右侧折叠导航栏界面优化方案.md` 和 `OCRGangYin主窗口UI文件拆分方案.md` 中“启动识别成功后自动收起右侧边栏”的交互合同。

以下既有结论继续保持：

- 五个右侧页面继续由五个独立 `.ui` 文件维护，MainWindow 继续直接持有对应生成 Ui。
- 再次点击当前导航按钮仍可手动收起边栏。
- Fault 首次呈现时仍自动打开“检测信息”页面。
- 页面切换和边栏开合不应用、不保存、不丢弃参数，也不清除未应用 `*`。
- `SettingsEditState`、`OperationUiPolicy`、设置成功提交和失败回退仍使用现有唯一实现。

## 4. 实施前代码事实

### 4.1 识别模式

当前 `image_settings_page.ui` 的 `groupBox_detectionMode` 同时包含：

- 识别模式标签和下拉框；
- `checkBox_hardwareTriggerEnabled`。

识别模式下拉框由 `MachineSettingsPage` 填充五种模式并绑定正式设置；模式切换由 `MainWindow::setupDetectModeChangeTracking()` 立即保存，并联动模板状态、纸巾阈值可见性和二维码 CSV 区域。`TemplateEditorPage` 也直接读取该下拉框确定当前模板模式。

### 4.2 纸巾粗糙度阈值

当前三个控件位于 `image_settings_page.ui` 的 `gridLayout_imageAcquisitionAndProcessing`。阈值继续属于 `AppSettings::detectionSchemes.tissueRoughnessThreshold`，具有非负数校验、未应用 `*`、设置按钮、成功持久化和失败恢复逻辑。

`MainWindow::setupNonPersistentDefaults()` 当前还会向粗糙度输入框预写 `6.0`，但随后 `MachineSettingsPage::initialize()` 会立即用正式设置覆盖该值。这是无效的重复写入，迁移时直接删除，不改写到参数设定页。

### 4.3 PLC 工作模式默认值

`plc_settings_page.ui` 的下拉框顺序已经是：

1. 连续触发模式，对应 `trigger_continuous`；
2. 间歇触发模式，对应 `trigger_interval`。

当前唯一正式默认值位于 `AppSettings::AppSettings()`，为 `trigger_interval`。JSON 稳定值映射已经支持 `continuous` 和 `intermittent`，无需修改 Schema 或枚举集合。

### 4.4 启动识别后的边栏

当前工作区已经从 `MainWindow::on_toolButton_startInspection_clicked()` 的成功分支删除 `hideRightPanel()`；取消和失败分支本来就不收起。实施时保留并验证这一现有差异，无需新增状态字段。

## 5. 最终 UI 结构

### 5.1 参数设定页

`DetectionSettingsPage` 的外层顺序调整为：

```text
gridLayout_detectionSettings
├─ row 0: groupBox_detectionMode
│  └─ formLayout_detectionMode
│     ├─ label_detectionMode
│     ├─ comboBox_detectionMode
│     └─ checkBox_hardwareTriggerEnabled
├─ row 1: groupBox_currentTemplateSettings
├─ row 2: groupBox_barcodeCsv
├─ row 3: gridLayout_photoDistance
├─ row 4: gridLayout_tissueRoughnessThreshold
│  ├─ label_tissueRoughnessThreshold
│  ├─ lineEdit_tissueRoughnessThreshold
│  └─ pushButton_applyTissueRoughnessThreshold
└─ row 5: verticalSpacer_detectionSettingsBottom
```

要求：

- 完整的 `groupBox_detectionMode` 位于参数设定页最顶部，其容器、内部布局和三个业务控件保持唯一实例。
- 纸巾阈值使用独立的 `gridLayout_tissueRoughnessThreshold`，与 `gridLayout_photoDistance` 同级。
- 三个纸巾阈值控件作为一个完整设置行迁移，不拆散到不同页面。
- 所有业务控件 `objectName`、文字、尺寸、按钮语义和初始属性保持不变。
- 模式不适用时，纸巾阈值三个控件继续一起隐藏。

### 5.2 图像设置页

完整识别模式分组和纸巾阈值迁出后，图像设置页继续保留：

- 图像保存设置；
- 相机曝光、增益、颜色通道和旋转。

图像设置页不保留 `groupBox_detectionMode`、硬触发复选框、空容器或替代分组。`checkBox_hardwareTriggerEnabled` 随完整分组迁移到参数设定页，其对象名、持久化字段、启用状态和业务行为保持不变。

### 5.3 样式

- `groupBox_detectionMode` 保留现有正式 QSS 选择器，迁移后继续使用原样式。
- `checkBox_hardwareTriggerEnabled` 保留现有正式 QSS 选择器。
- 不修改 QSS，不增加内联样式、专用颜色或新主题规则。

## 6. C++ 引用迁移

### 6.1 完整识别模式分组

将 `comboBox_detectionMode`、`label_detectionMode` 和 `checkBox_hardwareTriggerEnabled` 的所有生成 Ui 访问从 `m_imageSettingsUi` 改为 `m_detectionSettingsUi`：

- `main_window_settings.cpp`：当前模式读取和模式切换信号连接；
- `machine_settings_page.cpp`：模式列表填充，以及识别模式和硬触发开关的设置绑定、初始化、复制、恢复和信号屏蔽；
- `template_editor_page.cpp`：索引转模式 ID 和当前模式读取。

不修改模式枚举、显示名称、保存入口、失败回退、模板清理、模式联动顺序或硬触发开关业务规则。

`TemplateEditorPage` 仍通过 `m_imageSettingsUi` 读取图像旋转和颜色通道，因此只迁移其中的识别模式访问，不修改其构造参数、成员或图像设置页依赖。

### 6.2 纸巾粗糙度阈值

将三个纸巾阈值控件的所有生成 Ui 访问从 `m_imageSettingsUi` 改为 `m_detectionSettingsUi`，覆盖：

- 可见性联动；
- 全局设置绑定和未应用标签；
- 设置按钮信号连接；
- 数值校验器；
- UI 初始化、值复制、恢复和失败回退；
- 运行状态下的控件权限应用；
- 成功、错误和失败提示的父控件。

现有 `SettingsApplicationService::saveTissueThreshold()`、非负数规则、`AppSettings` 中的正式默认阈值 `6.0` 和运行快照读取全部保持不变。删除 `MainWindow::setupNonPersistentDefaults()` 中随后会被正式设置覆盖的粗糙度界面预写入，不迁移、不替代该冗余代码。

## 7. PLC 默认模式

在 `AppSettings::AppSettings()` 中将：

```text
trigger_interval
```

改为：

```text
trigger_continuous
```

默认值语义固定为：

- 全新安装或设置文件不存在时使用连续触发模式。
- 清空软件设置并重新启动后使用连续触发模式。
- 点击“恢复默认设置”时继续服从现有硬件状态和“带 `*` 的参数需点击确认后生效”规则；不绕过 PLC 写入和失败回退。
- 已有合法 `app_settings.json` 中明确保存的间歇触发模式继续保留，不在启动时强制覆盖。

不修改：

- `AppSettings::CurrentSchemaVersion`；
- JSON 字段名或稳定枚举值；
- PLC 地址、写入值、字节顺序和连接条件；
- 运行开始时冻结正式设置的逻辑。

## 8. 启动识别后的边栏行为

最终代码保持启动识别成功分支不调用 `hideRightPanel()`，行为为：

- 启动前边栏展开：启动成功后保持展开和当前页面。
- 启动前边栏收起：启动成功后保持收起。
- 取消未应用设置确认或启动失败：继续保持原状态。
- 运行中仍可通过导航按钮手动打开、切换或收起页面。
- Fault 首次呈现仍自动打开检测信息页。
- 停止识别不新增自动开合行为。

不增加“启动前边栏状态”成员、运行状态监听或延迟恢复逻辑。

## 9. 文件修改范围

| 文件 | 处理方式 |
|---|---|
| `app/ui/main_window/settings/detection_settings_page.ui` | 在顶部接收完整 `groupBox_detectionMode`；接收完整纸巾阈值设置行，并与拍照距离布局同级 |
| `app/ui/main_window/settings/image_settings_page.ui` | 删除完整识别模式分组和纸巾阈值旧位置，不保留替代分组或空容器 |
| `app/ui/main_window/main_window_settings.cpp` | 同步迁移控件的 Ui 所有者；删除 `setupNonPersistentDefaults()` 中冗余的粗糙度界面预写入；保持模式切换和阈值显隐行为 |
| `app/ui/main_window/settings/machine_settings_page.cpp` | 同步识别模式、硬触发开关和纸巾阈值的设置绑定、连接、校验、复制、恢复及权限应用中的 Ui 所有者 |
| `app/ui/main_window/template/template_editor_page.cpp` | 当前识别模式读取改用参数设定页生成 Ui |
| `app/system_support/settings/app_settings.cpp` | PLC 工作模式默认 ID 改为 `trigger_continuous` |
| `app/ui/main_window/main_window_inspection.cpp` | 保留并验证当前工作区中已经完成的自动收起调用删除 |
| `app/ui/README.md` | 保留当前已有的“启动成功不自动收起”说明，并同步迁移后的页面职责 |
| `docs/development/OCRGangYin计划索引.md` | 同步本方案状态、当前实施进度和覆盖关系 |

不新增生产代码文件，不修改 QSS、qmake 工程清单、翻译文件、资源清单或设置 Schema。

## 10. 实施顺序

1. 在两个独立 `.ui` 中一次性移动完整 `groupBox_detectionMode` 和完整纸巾阈值设置行，保留唯一实例；图像设置页不留下替代分组或空容器。
2. 同步 `MainWindow`、`MachineSettingsPage` 和 `TemplateEditorPage` 的生成 Ui 访问，并删除 `setupNonPersistentDefaults()` 中冗余的粗糙度界面预写入。
3. 将 PLC 工作模式唯一默认 ID 改为连续触发。
4. 保留并检查当前工作区中已经完成的启动后不自动收起改动。
5. 同步 UI README，不修改 QSS。
6. 执行静态验收，不构建、不运行主程序。

不设置新旧控件并存阶段，不保留旧对象名别名、隐藏副本、查找回退或转发层。

## 11. 静态验收

- `detection_settings_page.ui` 和 `image_settings_page.ui` 均为合法 XML。
- `groupBox_detectionMode` 及其内部布局、识别模式标签、下拉框、硬触发复选框和三个纸巾阈值控件全仓各存在一个 `.ui` 实例。
- 上述完整分组及六个业务控件在 `image_settings_page.ui` 中零引用，在 `detection_settings_page.ui` 中各唯一存在。
- `checkBox_hardwareTriggerEnabled` 只位于 `detection_settings_page.ui`，对象名和业务行为保持不变。
- 生产 C++ 中对识别模式、硬触发开关和纸巾阈值控件不存在 `m_imageSettingsUi` 旧访问，全部改用 `m_detectionSettingsUi`。
- 不存在 `groupBox_hardwareTrigger`、`formLayout_hardwareTrigger`、替代分组、空容器、对象别名、隐藏副本、动态查找回退或转发层。
- `setupNonPersistentDefaults()` 中不存在粗糙度阈值界面预写入，正式默认值只由 `AppSettings` 提供。
- `TemplateEditorPage` 继续直接持有并使用图像设置页 Ui，不引入新的参数转发或适配层。
- 识别模式仍只有一条立即保存和失败恢复路径；纸巾阈值仍只有一条按钮应用路径。
- `AppSettings` 默认 `triggerModeId` 为 `trigger_continuous`，有效 ID 列表和 JSON 映射保持原样。
- `main_window_inspection.cpp` 中不再调用 `hideRightPanel()`；`main_window.cpp` 中手动导航收起和 Fault 打开逻辑保持存在。
- `app/ui/README.md` 不再描述“检测启动成功后自动收起”。
- 无 QSS、`.pro`、`.pri`、`.qrc`、Schema、翻译、算法、Runtime 或设备实现差异。
- `git diff --check` 通过。

静态检查通过只表示源文件、引用和配置关系已核对，不代表构建、运行或真实 PLC 验证通过。

## 12. 用户验证

1. 在 Qt Creator 执行 Run qmake 和 Release 构建。
2. 打开参数设定页，确认完整识别模式分组位于最顶部，识别模式下拉框和 PLC 硬触发开关均在该分组内。
3. 切换五种识别模式，确认模式仍可保存，模板区域、二维码 CSV 区域和纸巾阈值区域按原规则显示或隐藏。
4. 进入纸巾模式，确认标签、输入框和设置按钮完整位于参数设定页；输入合法值后可保存，非法值仍恢复并提示。
5. 打开图像设置页，确认完整识别模式分组、PLC 硬触发开关和纸巾阈值均不再出现；回到参数设定页确认硬触发开关行为不变。
6. 使用无设置文件的环境启动，确认 PLC 工作模式显示为连续触发模式。
7. 使用已保存为间歇触发的合法设置重启，确认原值仍被保留。
8. 在真实 PLC 可控条件下应用连续和间歇模式，确认原写入、成功和失败回退行为不变。
9. 分别在边栏展开和收起状态点击“启动识别”，确认启动成功后保持原开合状态。
10. 验证启动取消、启动失败、运行中手动导航、正常停止和 Fault 首次自动打开检测信息均符合第 8 节合同。

## 13. 风险与结论

| 项目 | 风险判断 | 控制方式 |
|---|---|---|
| 完整识别模式分组和纸巾阈值跨 `.ui` 迁移 | 低；遗漏生成 Ui 所有者会导致编译失败 | 全仓清零旧 `m_imageSettingsUi` 引用，并保持控件对象名不变 |
| 纸巾阈值业务 | 低；只改变界面父级 | 保留原绑定、校验、未应用状态、提交和回退入口 |
| 识别模式和硬触发业务 | 低；被设置页、模板页和主窗口读取 | 调用方统一改为参数设定页 Ui，不改变保存、权限和联动顺序 |
| PLC 默认值 | 小范围行为变化，不是纯 UI | 只改唯一默认 ID，不覆盖已有配置、不改 PLC 写入协议 |
| 启动后边栏 | 小范围交互变化，不影响检测流程 | 保留当前已完成的调用删除，继续保留手动收起和 Fault 自动打开 |

总体上，本方案风险较低，不修改检测算法、模板格式、运行线程、相机或 PLC 通讯协议；但由于包含 PLC 默认值和启动后边栏行为两项用户可见规则变化，不能视为单纯的 UI 布局微调。
