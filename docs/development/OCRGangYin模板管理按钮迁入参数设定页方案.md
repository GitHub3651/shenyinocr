# OCRGangYin 模板管理按钮迁入参数设定页方案

## 1. 文档状态

- 编写日期：2026-09-16。
- 审查修订日期：2026-09-17。
- 当前状态：代码实施完成，待用户统一验证。
- 实施结果：五个按钮已唯一迁入 `DetectionSettingsPage`，按固定 3+2 排列且行列完全由 `.ui` 定义；旧工具栏容器、专用分隔线、旧生成 Ui 访问和一次性连接包装已删除；操作状态、QSS、翻译和指定现行文档已同步。
- 静态验收：两份 UI 和两份 TS 均可解析；控件唯一性、固定 3+2 行列、外层布局行号、固定尺寸、连接唯一性、旧符号零引用、资源路径、翻译上下文、UTF-8、末尾换行和 `git diff --check` 均通过；按项目约束未运行 qmake、编译、主程序或设备验证。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `8feafdf`，并以实施开始前重新记录的 Git 状态、暂存区和工作区差异作为对照基线；当前已经暂存的 UI、工具栏、资源和文档差异均属于既有源码事实，实施时不得覆盖、回退或混入无关差异。
- 权威范围：把 `toolButton_selectTemplate`、`toolButton_createTemplate`、`toolButton_saveTemplate`、`toolButton_editCharacterTemplates`、`toolButton_exitTemplate` 五个模板按钮从 `main_window.ui` 迁入 `DetectionSettingsPage`，建立标题为“模板管理”的同级分组，按固定 3+2 排列，并同步迁移直接控件访问、信号连接、状态应用、显隐、QSS 和当前说明；同时删除迁移后失去用途的连接包装和 include，并把制作按钮在实时取景状态下的过长文字由“拍照并开始框选”收短为“拍照框选”。
- 实施门禁：生产代码和现行说明已完成实施；Agent 只执行静态门禁，构建、运行和真实设备验证由用户统一完成。

## 2. 目标与结论

在 `detection_settings_page.ui` 中新增唯一分组：

```text
groupBox_templateManagement
title = 模板管理
```

该分组与 `groupBox_currentTemplateSettings` 同属于 `gridLayout_detectionSettings`，不嵌入当前模板设置分组。五个既有按钮完整迁入该分组，保留原对象名、`:/svg/...` 图标资源 URL、Tooltip、27×27px 图标和 `ToolButtonTextUnderIcon` 样式。选择、制作、保存和分割字符四个按钮的 `.ui` 初始文字及 76×62px 固定尺寸保持；制作按钮在模板实时取景状态下的动态文字由“拍照并开始框选”收短为“拍照框选”，避免 76px 固定宽度下截断，对应 Tooltip 和操作语义不变。`toolButton_exitTemplate` 的可见文字由“退出”改为“退出模板制作”，固定尺寸相应调整为 108×62px，保证 16px 字号下单行完整显示。由于目标 `.ui` 比 `main_window.ui` 多一层目录，`detection_settings_page.ui` 对 `image.qrc` 的 Designer 相对引用使用 `../../../resource/image.qrc`，不沿用原文件中的 `../../resource/image.qrc`。

分组内部使用 `QGridLayout` 固定为 3+2：

```text
模板管理
├─ [制作模板] [保存模板] [分割字符]
└─ [选择模板] [退出模板制作]
```

按钮行列完全由 `detection_settings_page.ui` 定义。分割字符按钮按原模式规则显示或隐藏；隐藏时保留固定网格中其他按钮的位置，不增加占位控件或水平滚动条。

主画面顶部工具栏迁出五个按钮后，最终只保留：

```text
[相机开关] [识别启停]             [相机状态] [PLC 状态] [硬触发]
```

原 `widget_templateActions`、`horizontalLayout_templateActions` 和只用于分隔模板按钮的 `line_toolbarActionSeparator` 一并删除，不保留空容器、空白宽度、隐藏副本或兼容控件。

## 3. 与现有方案的关系

本文在五个模板按钮的位置、生成 Ui 所有者和直接访问方式上，覆盖以下旧结论：

- `OCRGangYin图像上方常驻工具栏与设备状态重构方案.md` 中“五个模板按钮属于主画面 74px 常驻工具栏”和“主工具栏有七个操作入口”的结论。
- `OCRGangYin主窗口UI文件拆分方案.md` 中这五个按钮继续属于 `Ui::MainWindow`、通过 `m_mainWindowUi` 访问的结论。

以下既有结论继续保持：

- `main_window.ui` 中 74px 常驻工具栏继续存在。
- 相机开关、识别启停、相机状态、PLC 状态和唯一硬触发开关继续留在主工具栏。
- `DetectionSettingsPage` 继续由独立 `detection_settings_page.ui` 定义，并由 MainWindow 持有唯一 `Ui::DetectionSettingsPage`。
- `TemplateEditorPage` 继续负责模板选择、制作、保存、字符模板编辑、模板参数和模板向导。
- `MainWindow::exitTemplate()` 继续负责退出模板制作后的跨区域收口和运行状态文字刷新。
- `OperationUiPolicy`、`SettingsEditState`、模板应用服务和运行快照继续是现有唯一状态与业务边界；本次只收短 `templateCaptureText` 的一个显示值，不改变策略字段或判定规则。
- 左侧导航、可调宽抽屉、模板绘图、模板格式、五种检测模式、算法、相机、PLC、统计、CSV 和存图行为不变。

## 4. 当前代码事实

当前五个按钮只定义在 `app/ui/main_window/main_window.ui` 的 `widget_templateActions` 中，但状态和行为分散在三个对象：

| 职责 | 当前位置 |
|---|---|
| “退出”按钮连接 | `MainWindow` 构造函数连接到 `MainWindow::exitTemplate()` |
| “制作模板”按钮连接 | `MainWindow::initializePages()` 连接到 `TemplateEditorPage::handleTemplateCaptureButton()` |
| 选择模板权限 | `MainWindow::updateOperationUiState()` 直接应用 `snapshot.templateSelection` |
| 制作按钮动态文字、Tooltip、制作/退出权限 | `InspectionPage::applyOperationState()` |
| 选择、保存、分割字符连接 | `TemplateEditorPage` |
| 保存、分割字符权限 | `TemplateEditorPage::applyOperationState()` |
| 模板操作区整体显隐及分割字符按钮显隐 | `MainWindow::updateTissueRoughnessUiVisibility()` |

因此本次不能只剪切 `.ui` 节点。必须同步迁移所有生成 Ui 引用和状态应用，否则会出现编译失败、按钮无连接、按钮动态文字失效或禁用规则遗漏。

当前 `TemplateEditorPage::setupManualCharacterCropUi()` 只包装一条分割字符按钮连接，迁移后应把该连接直接并入现有 `connectPageActions()`，并删除这个无复用价值的声明、调用和定义。`InspectionPage` 删除模板按钮访问后，其 `#include <QToolButton>` 也将失去用途，应在同一差异中清理。

## 5. 最终 UI 结构

`DetectionSettingsPage` 外层结构调整为：

```text
gridLayout_detectionSettings
├─ row 0: groupBox_detectionMode
├─ row 1: groupBox_templateManagement
│  └─ gridLayout_templateManagement
│     ├─ row 0: toolButton_createTemplate → toolButton_saveTemplate
│     │         → toolButton_editCharacterTemplates
│     └─ row 1: toolButton_selectTemplate → toolButton_exitTemplate
├─ row 2: groupBox_currentTemplateSettings
├─ row 3: groupBox_barcodeCsv
├─ row 4: gridLayout_photoDistance
├─ row 5: gridLayout_tissueRoughnessThreshold
└─ row 6: verticalSpacer_detectionSettingsBottom
```

具体要求：

- `groupBox_templateManagement` 与 `groupBox_currentTemplateSettings` 平级，标题固定为“模板管理”。
- 五个按钮保持唯一实例，全部定义在 `detection_settings_page.ui`。
- 五个按钮的 `objectName`、图标路径、Tooltip 和操作语义保持不变；前四个按钮的 `.ui` 初始文字和 76×62px 尺寸保持不变，制作按钮在实时取景状态下的动态文字改为“拍照框选”，`toolButton_exitTemplate` 只把文字改为“退出模板制作”并把固定宽度改为 108px，高度仍为 62px。
- `detection_settings_page.ui` 新增对 `../../../resource/image.qrc` 的有效资源引用；按钮继续使用原 `:/svg/...` URL，不复制或改名图标。
- 不新增页面包装类、按钮映射表、控件 getter、别名成员、`findChild()` 或中转 Ui 结构。
- 模板模式下显示整个 `groupBox_templateManagement`；纸巾模式下隐藏整个分组，不留下空白行。
- `toolButton_editCharacterTemplates` 继续只在支持字符模板编辑的模式显示，其余四个按钮继续按现有模板模式和运行状态规则呈现。
- `gridLayout_templateManagement` 固定第一行为制作模板、保存模板、分割字符，第二行为选择模板、退出模板制作；C++ 不调整按钮行列。

## 6. 代码迁移

### 6.1 `TemplateEditorPage` 收拢模板按钮状态和连接

`TemplateEditorPage` 已经直接持有 `Ui::DetectionSettingsPage &`，因此五个迁入按钮中属于模板页的访问统一改为 `m_detectionSettingsUi`：

- `toolButton_selectTemplate` 连接 `selectTemplatesForCurrentMode()`。
- `toolButton_createTemplate` 连接 `handleTemplateCaptureButton()`。
- `toolButton_saveTemplate` 连接 `saveCurrentTemplate()`。
- `toolButton_editCharacterTemplates` 连接 `showManualCharacterTemplateEditorDialog()`。
- 上述四条连接全部直接放入现有 `connectPageActions()`；删除仅包装分割字符连接的 `setupManualCharacterCropUi()` 构造调用、头文件声明和函数定义。
- `applyOperationState()` 负责：
  - `snapshot.templateSelection`；
  - `snapshot.templateCapture`；
  - `snapshot.saveTemplate`；
  - `snapshot.templateEditing` 派生的字符模板编辑权限；
  - `snapshot.templateExit`；
  - “制作模板 / 拍照框选 / 重新取景”的动态文字和对应 Tooltip。

`applyOperationUiAccess()` 会在第一次应用权限时缓存控件原始 Tooltip，并在后续启用状态恢复该缓存值。因此制作模板按钮不能继续沿用“先设置动态 Tooltip、再应用权限”的现有顺序，否则动态 Tooltip 会被缓存值覆盖。最终实现固定采用以下顺序：

1. 设置 `templateCaptureText` 对应的按钮文字。
2. 调用 `applyOperationUiAccess()` 应用 `snapshot.templateCapture`，让禁用状态继续显示现有禁用原因。
3. 仅当 `snapshot.templateCapture.enabled` 为真时，再把当前状态对应的动态 Tooltip 写入按钮。

不访问或改写 `_operationOriginalToolTip` 动态属性，不修改 `applyOperationUiAccess()` 接口，不为 Tooltip 新增包装函数、第二份状态或兼容分支。

这样模板按钮的直接连接和状态呈现集中在已经拥有参数设定页 Ui 的模板页，不为 `InspectionPage` 增加 `Ui::DetectionSettingsPage` 依赖，也不增加新的状态转发函数。

### 6.2 `MainWindow` 保留退出模板的最终处理

`toolButton_exitTemplate` 的点击仍直接连接到 `MainWindow::exitTemplate()`，因为该方法同时处理：

- 停止模板预览并清理绘图；
- 更新检测信息页运行状态文字；
- 触发统一操作状态刷新。

连接对象从 `ui->toolButton_exitTemplate` 改为 `m_detectionSettingsUi->toolButton_exitTemplate`。不把现有跨区域收口复制到 `TemplateEditorPage`，也不新增一行转发槽。

`MainWindow::initializePages()` 中原制作模板按钮连接删除，由 `TemplateEditorPage::connectPageActions()` 直接连接迁入后的按钮。

### 6.3 删除迁移后失去职责的跨页访问

- `InspectionPage::applyOperationState()` 删除对制作模板和退出模板按钮的文字、Tooltip 和权限操作，只继续负责相机、识别和检测信息呈现。
- `InspectionPage` 同步删除迁移后不再使用的 `#include <QToolButton>`，不保留因旧模板按钮访问产生的无用 include。
- `MainWindow::updateOperationUiState()` 删除对主窗口“选择模板”按钮的权限应用；该权限由 `TemplateEditorPage::applyOperationState()` 直接应用。
- `MainWindow::updateTissueRoughnessUiVisibility()` 把 `ui->widget_templateActions` 显隐改为 `m_detectionSettingsUi->groupBox_templateManagement`，并把字符模板按钮引用改为参数设定页 Ui。
- `TemplateEditorPage` 中五个按钮的 `m_mainWindowUi.toolButton_*` 访问全部改为 `m_detectionSettingsUi.toolButton_*`；`m_mainWindowUi` 仍用于主图像画布和模板向导，不因本次迁移删除。

### 6.4 保持 `OperationUiPolicy` 状态规则不变

现有快照字段已经完整表达本次按钮状态：

```text
templateSelection
templateCapture
saveTemplate
templateEditing
templateExit
templateCaptureText
```

本次不修改字段、状态机、启用条件或禁用原因，不增加第二个模板按钮策略。只把 `OperationUiPolicy::create()` 在 `TemplatePreviewing` 状态输出的 `templateCaptureText` 从“拍照并开始框选”改为“拍照框选”，以适配现有 76×62px 固定按钮；对应 Tooltip 仍完整说明“拍照并开始框选模板区域”。`operation_ui_policy.h` 无需修改。

### 6.5 固定布局边界

`detection_settings_page.ui` 直接定义五个按钮的最终行列：第一行依次为制作模板、保存模板、分割字符，第二行依次为选择模板、退出模板制作。`MachineSettingsPage` 不承担模板按钮布局职责，`TemplateEditorPage` 只负责按钮业务连接和操作状态。

## 7. 样式与资源

正式样式继续只来自 `app/resource/qss/app_theme.qss`：

- 主工具栏相机和识别按钮的现有规则保持。
- `QGroupBox#groupBox_templateManagement` 与 `QGroupBox#groupBox_currentTemplateSettings` 必须位于同一个参数页卡片选择器中，使用完全相同的背景、边框和 padding，且不为其中任一 GroupBox 增加独立覆盖规则。
- 把 `QWidget#widget_templateActions QToolButton` 相关选择器改为限定 `QGroupBox#groupBox_templateManagement QToolButton`。
- 删除 `QToolButton#toolButton_exitTemplate` 的重复主工具栏专用选择器；退出按钮与同分组其他模板按钮使用同一规则。
- 删除已失去对象的 `QFrame#line_toolbarActionSeparator` 规则。
- 不增加内联样式、第二份 QSS、运行时样式拼接或新的主题对象。

现有 `template_select.svg`、`template_make.svg`、`template_save.svg`、`character_seg.svg` 和 `stop.svg` 继续复用，`image.qrc` 不需要新增、删除或改名资源。只在 `detection_settings_page.ui` 中登记正确的 `../../../resource/image.qrc` 相对位置；这属于 Designer 元数据迁移，不是新增资源或兼容路径。

`app/resource/README.md` 同步说明五个模板操作图标由参数设定页的“模板管理”分组使用，`stop.svg` 仍同时供识别停止和退出模板制作使用；不增加资源副本或第二套调用路径。

两份 TS 不能只机械修改已有位置记录。当前 `DetectionSettingsPage` 上下文尚未包含这五个按钮的文字和 Tooltip，因此实施时应让“模板管理”、迁入后的五个按钮文字（退出按钮使用新文字“退出模板制作”）及五个静态 Tooltip 作为 `DetectionSettingsPage` 的有效消息存在于 `Translate_CN.ts`、`Translate_EN.ts`；旧 `MainWindow` 上下文不得保留这些消息的有效位置，也不得继续保留“退出”作为该按钮的有效源文字。动态 Tooltip 继续来自现有 C++ 字面量，本次不扩展翻译范围。

## 8. 文件级实施范围

| 文件 | 处理 |
|---|---|
| `app/ui/main_window/main_window.ui` | 删除五个模板按钮、`widget_templateActions`、其布局和专用分隔线；保留相机、识别、设备状态和硬触发工具栏 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 新增标题为“模板管理”的同级 GroupBox 和承载五个按钮的 `gridLayout_templateManagement`；第一行固定为制作、保存、分割字符，第二行固定为选择、退出模板制作；顺延后续外层行号，以 `../../../resource/image.qrc` 登记现有图标资源，并把退出按钮文字和固定尺寸设为“退出模板制作”、108×62px |
| `app/ui/main_window/main_window.cpp` | 退出按钮改用参数设定页 Ui；删除主窗口中的制作模板按钮连接 |
| `app/ui/main_window/main_window_settings.cpp` | 模板管理分组整体显隐和字符模板按钮显隐改用参数设定页 Ui |
| `app/ui/main_window/main_window_inspection.cpp` | 删除主窗口对选择模板按钮的直接权限应用 |
| `app/ui/main_window/inspection/inspection_page.cpp` | 删除制作和退出模板按钮的跨页文字、Tooltip 与权限操作，并删除随之失去用途的 `QToolButton` include |
| `app/ui/main_window/template/template_editor_page.h/.cpp` | 五个按钮改用参数设定页 Ui；四个模板业务按钮统一在 `connectPageActions()` 中直接连接；删除 `setupManualCharacterCropUi()` 的调用、声明和定义；集中应用模板按钮状态和动态文字；按“权限先应用、启用时再写动态 Tooltip”的顺序保留动态提示和禁用原因 |
| `app/ui/main_window/operation_ui_policy.cpp` | 只把模板实时取景状态的 `templateCaptureText` 从“拍照并开始框选”收短为“拍照框选”；字段、权限和状态判定保持不变 |
| `app/resource/qss/app_theme.qss` | 新分组加入同级参数卡片选择器；模板按钮选择器切到新 GroupBox；删除旧容器和分隔线选择器 |
| `app/resource/Translate_CN.ts`、`app/resource/Translate_EN.ts` | 在 `DetectionSettingsPage` 上下文登记“模板管理”、五个按钮文字和五个静态 Tooltip，清除旧 `MainWindow` 有效位置；不改无关翻译或动态 C++ 字面量 |
| `app/resource/README.md` | 同步模板 SVG 的最终使用位置，保留 `stop.svg` 的识别停止和退出模板制作双用途说明 |
| `app/ui/README.md` | 同步主工具栏和参数设定页模板管理职责 |
| `docs/development/OCRGangYin开发者代码结构与维护指南.md` | 同步最终控件位置和直接访问边界 |
| `docs/development/OCRGangYin现有功能对照表.md` | 同步模板入口由常驻工具栏迁入参数设定页 |
| `docs/development/OCRGangYin图像上方常驻工具栏与设备状态重构方案.md` | 只在文档状态中注明五个模板按钮的位置结论已由本文替代，保留历史实施正文 |
| `docs/development/OCRGangYin主窗口UI文件拆分方案.md` | 只在文档状态中注明五个按钮的生成 Ui 所有者和直接访问方式已由本文替代，保留历史实施正文 |
| `docs/development/OCRGangYin计划索引.md` | 同步本文状态、进度和任务路由；删除现行说明中的“工具栏七个操作入口”等已被本文替代的表述，并在主窗口 UI 拆分方案条目中注明局部替代关系 |

以下文件预计不修改：

- `app/ui/main_window/main_window.h`：现有 `exitTemplate()` 和 Ui 成员足够。
- `app/ui/main_window/operation_ui_policy.h`：现有状态字段和接口保持。
- `app/AutoOCRproject.pro`：不新增、删除或移动文件。
- `app/resource/image.qrc` 和 SVG：继续复用现有资源。

## 9. 明确保持的行为

- 选择模板继续在空闲状态可用，忙碌时使用原禁用原因。
- 制作模板继续要求相机已打开，并在空闲、预览、冻结状态分别显示“制作模板 / 拍照框选 / 重新取景”；Tooltip 保持现有完整说明。
- 保存模板继续只在模板画面已冻结时可用。
- 分割字符继续服从当前模式、模板有效性和 `templateEditing` 权限。
- 退出继续只在模板预览或冻结状态可用，并执行现有完整退出收口。
- 纸巾模式继续不显示模板操作和当前模板设置。
- 模板选择、模板制作、二维码即时校验、字符分割、模板保存、失败回退和磁盘格式均不变。
- 左侧抽屉的打开、收起、拖动和宽度持久化规则不变；启动、停止和 Fault 不新增自动切页或自动开合行为。

## 10. 明确不做

- 不移动相机、识别、设备状态或硬触发控件。
- 不把五个按钮复制到参数页后保留主工具栏副本。
- 不保留 `widget_templateActions` 空容器、分隔线、隐藏按钮或对象名别名。
- 不重命名五个按钮，不改变图标、Tooltip 或操作语义；除 `toolButton_exitTemplate` 改为“退出模板制作”及 108×62px、制作按钮的实时取景动态文字收短为“拍照框选”外，不改变其他按钮文字或尺寸。
- 不把“当前产品模板设置”并入“模板管理”，两个 GroupBox 保持同级。
- 不新增模板管理页面、弹窗、Controller、Manager、Facade、Bindings、路由表或事件总线。
- 不修改 `OperationUiPolicy` 字段和状态规则、模板 Schema、AppSettings Schema、模板目录、检测算法、线程、相机或 PLC 协议。
- 不顺手调整参数设定页其他分组、控件文字、间距、颜色或功能。

## 11. 实施顺序

1. 在两个 `.ui` 中一次性完成唯一控件迁移：先把五个按钮写入 `DetectionSettingsPage` 的新分组并登记 `../../../resource/image.qrc`，再从主工具栏删除原定义、容器和分隔线。
2. 将 `TemplateEditorPage` 的按钮连接、状态和动态文字改到 `m_detectionSettingsUi`，四个模板业务按钮统一在 `connectPageActions()` 中直接连接，删除 `setupManualCharacterCropUi()` 包装，并按“权限先应用、启用时再写动态 Tooltip”的固定顺序处理制作按钮。
3. 将 MainWindow 的退出连接、模板管理分组显隐和字符按钮显隐改到参数设定页 Ui。
4. 删除 `InspectionPage` 和 `MainWindow::updateOperationUiState()` 中失去职责的模板按钮访问及无用 include，并把 `templateCaptureText` 的预览状态文字收短为“拍照框选”。
5. 同步同级 GroupBox 与按钮 QSS、两份 TS 的完整有效消息、资源说明，以及当前维护说明和两份被局部替代方案的文档状态。
6. 执行静态门禁；不构建、不运行主程序。

迁移必须在一个实施差异中形成终局，不建立新旧按钮并存的过渡状态。

## 12. Agent 静态验收

- `main_window.ui` 和 `detection_settings_page.ui` 均为合法 XML。
- `groupBox_templateManagement` 在 `detection_settings_page.ui` 中唯一存在，标题为“模板管理”，与 `groupBox_currentTemplateSettings` 同属于外层网格。
- 五个模板按钮在全仓 `.ui` 中各定义一次，只位于 `detection_settings_page.ui`。
- `detection_settings_page.ui` 对 `image.qrc` 的资源引用为存在的 `../../../resource/image.qrc`，五个按钮继续使用原 `:/svg/...` URL；旧 `../../resource/image.qrc` 不随按钮迁入目标文件。
- `toolButton_exitTemplate` 的 `text` 唯一为“退出模板制作”，固定尺寸为 108×62px；其他四个按钮仍为 76×62px，生产 `.ui` 中不再存在该按钮显示文字为“退出”的定义。
- `gridLayout_templateManagement` 的精确行列为：制作 `(0,0)`、保存 `(0,1)`、分割字符 `(0,2)`、选择 `(1,0)`、退出模板制作 `(1,1)`。
- 模板按钮行列只在 `detection_settings_page.ui` 中定义，生产 C++ 不改变其行列。
- `main_window.ui` 中 `widget_templateActions`、`horizontalLayout_templateActions`、`line_toolbarActionSeparator` 和五个模板按钮均为零引用。
- 生产 C++ 中五个模板按钮不存在 `ui->toolButton_*` 或 `m_mainWindowUi.toolButton_*` 旧访问，全部通过 `m_detectionSettingsUi` 直接访问。
- `InspectionPage` 不访问五个模板按钮，也不新增参数设定页 Ui 依赖。
- `setupManualCharacterCropUi()` 的声明、调用和定义全仓零引用；分割字符按钮直接在 `connectPageActions()` 中连接。
- `inspection_page.cpp` 不再包含未使用的 `QToolButton` 头文件；迁移涉及文件中不存在因旧按钮访问遗留的无用 include。
- 五个按钮各只有一条可追踪的最终点击连接；制作模板和退出模板不存在自动槽、显式连接或中转槽的重复路径。
- `OperationUiPolicy` 的模板权限字段和判定条件无计划外差异；`TemplatePreviewing` 状态的 `templateCaptureText` 唯一为“拍照框选”，生产代码不再输出“拍照并开始框选”作为按钮文字。
- 制作模板按钮可用时，三种操作状态分别显示对应动态 Tooltip；禁用时显示 `snapshot.templateCapture.disabledReason`。实现不访问 `_operationOriginalToolTip`，不修改 `applyOperationUiAccess()`，也不增加 Tooltip 包装层。
- `widget_templateActions` 和 `line_toolbarActionSeparator` 的 QSS 选择器零引用；`groupBox_templateManagement` 与 `groupBox_currentTemplateSettings` 位于同一个卡片选择器中且没有独立 GroupBox 覆盖规则；新分组内五个按钮具有正常、悬停、按下、焦点和禁用样式。
- 五个 SVG 和 QRC 条目保持原样，没有重复资源或新旧图标并存。
- `app/resource/README.md` 不再把模板图标限定为主工具栏使用，并准确记录参数设定页模板管理分组及 `stop.svg` 的双用途。
- `Translate_CN.ts` 和 `Translate_EN.ts` 的 `DetectionSettingsPage` 上下文均包含“模板管理”、迁入后的五个按钮文字和五个静态 Tooltip 的有效消息；退出按钮的有效源文字为“退出模板制作”，旧 `MainWindow` 上下文没有这些消息的有效位置，也没有该按钮旧文字“退出”的有效位置。
- 没有隐藏副本、控件别名、`findChild()`、动态创建固定按钮、转发层或新抽象。
- 常驻工具栏方案与主窗口 UI 拆分方案均已在文档状态中注明局部替代关系；计划索引的现行说明不再宣称主工具栏有七个操作入口或五个模板按钮属于 `Ui::MainWindow`。
- 相对实施开始前记录的暂存区和工作区基线，`.pro`、Schema、Runtime、Detection、Devices、PLC 和算法不产生本方案引入的新增差异。
- UTF-8、文件末尾换行和 `git diff --check` 通过。

静态检查通过只表示文件、引用和结构关系已核对，不表示构建、运行或交互验证通过。

## 13. 用户统一验证

1. 在 Qt Creator 执行 Run qmake、Clean 和 Release Rebuild。
2. 打开主界面，确认顶部工具栏只保留相机、识别、相机状态、PLC 状态和硬触发；模板按钮原位置没有空容器、分隔线或异常留白。
3. 打开参数设定页，确认“模板管理”与“当前产品模板设置”是两个同级、同样式分组；第一行依次为制作模板、保存模板、分割字符，第二行依次为选择模板、退出模板制作；退出按钮文字完整且按钮不截断、不重叠。
4. 拖窄、拖宽并重新打开左侧抽屉，确认按钮始终保持固定 3+2 行列，其他参数分组不受影响。
5. 切换字库、深度 OCR、二维码＋三期、钢印和纸巾五种模式，确认模板管理分组、当前模板设置和分割字符按钮按原规则显示或隐藏。
6. 在相机关闭、相机打开、识别中、停止中、模板实时取景和模板冻结六种状态下，逐一核对五个按钮的启用状态、禁用原因、动态文字和 Tooltip；重点确认制作按钮分别显示“制作模板 / 拍照框选 / 重新取景”，对应 Tooltip 仍显示完整操作说明，禁用时仍显示策略给出的禁用原因。
7. 完整验证选择模板、制作模板、拍照框选、重新取景、保存模板、分割字符和退出模板制作。
8. 验证无效模板时字符模板编辑仍被禁用，移除或重新选择有效模板后恢复。
9. 回归模板参数应用、模板向导、二维码即时读码、字符框选、取消、失败回退和模板磁盘结果。
10. 回归相机、识别、硬触发、PLC 状态、抽屉状态、五模式检测、统计、CSV 和存图，确认本次位置迁移没有改变业务合同。

## 14. 完成条件

- 五个模板按钮的唯一实例全部属于 `Ui::DetectionSettingsPage`。
- 参数设定页存在标题为“模板管理”的同级分组；按钮固定为第一行制作、保存、分割字符，第二行选择、退出模板制作。
- 退出按钮唯一显示“退出模板制作”，108×62px 尺寸下文字完整且与其他按钮保持同一图标、字号和样式体系。
- 主工具栏不再包含任何模板按钮、模板按钮容器或专用分隔线。
- 模板按钮的连接、显隐、权限和退出收口保持现有行为；制作按钮只将实时取景状态的显示文字收短为“拍照框选”，操作语义和 Tooltip 不变。
- `groupBox_templateManagement` 与 `groupBox_currentTemplateSettings` 使用同一个 QSS 卡片选择器和完全相同的 GroupBox 样式；`.ui` 中的 QRC 相对位置有效，两份 TS 的新上下文消息完整且没有旧有效位置残留。
- `InspectionPage` 不再承担模板按钮状态，`TemplateEditorPage` 直接管理迁入按钮，MainWindow 只保留退出模板的真实跨区域协调。
- 没有兼容层、隐藏副本、重复连接、镜像状态、一次性连接包装、布局胶水层或计划外功能变化。
- Agent 静态门禁通过，并由用户完成 Qt Creator 构建和第 13 节人工交互验证。
