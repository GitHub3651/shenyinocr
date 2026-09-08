# OCRGangYin 主控操作台按钮分组界面优化方案

## 1. 文档状态

- 编写日期：2026-09-02。
- 最新修订：2026-09-05，提交 `cfc0528` 已按本方案完成主控分组、按钮迁移、状态接线和旧引用清理；主控区由三个水平按钮容器组成，八个主操作按钮保持同一行，外层比例为 `1:2:1`，两个迁入按钮暂时复用既有 QRC 图标。
- 当前状态：代码已实施，待用户 Qt Creator 构建与人工交互验证。
- 权威范围：`groupBox_mainControls` 内八个操作按钮按相机、模板、检测三个容器分组的一行排列，以及两个按钮迁出参数设定页后产生的旧布局和旧引用清理。
- 当前进度：提交 `cfc0528` 已修改计划内五个生产文件并完成一行布局、两个按钮迁移、模板容器显隐收敛和旧名称清理；当前源码保持该实现，后续工业视觉风格方案只替换八个按钮的静态图标和正式 QSS 视觉，不改变本方案布局与行为。
- 关联基线：继续遵守 `OCRGangYin主窗口UI文件拆分方案.md`、`OCRGangYinUI架构精简优化方案.md`、`OCRGangYin右侧折叠导航栏界面优化方案.md` 和统一 QSS 现状。

## 2. 目标

主控区保持一行水平排列。八个按钮由相机、模板、检测三个无标题容器分组；外层与三个容器内部均使用水平布局，不增加视觉标题或装饰性分组框：

1. 相机操作：打开相机、关闭相机。
2. 模板操作：选择模板、制作模板、保存模板、分割字符。
3. 检测操作：启动识别、停止识别。

原 `pushButton_saveTemplate` 和 `pushButton_editCharacterTemplates` 从参数设定页迁入 `groupBox_mainControls`，并正式改为：

- `toolButton_saveTemplate`。
- `toolButton_editCharacterTemplates`。

迁移改变两个按钮的界面父级、控件类型和对象名，但不改变点击结果、启用条件、禁用原因、模式可见性或业务行为。旧对象名不保留别名或兼容引用。

## 3. 最终界面结构

```text
┌──────────────────────── 主控操作台 ────────────────────────┐
│                                                            │
│ [打开相机] [关闭相机] [选择模板] [制作模板] [保存模板] [分割字符] [启动识别] [停止识别] │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

`app/ui/main_window/main_window.ui` 中的最终层级：

```text
groupBox_mainControls
└─ horizontalLayout_mainControls
   ├─ widget_cameraControls
   │  └─ horizontalLayout_cameraControls
   │     ├─ toolButton_openCamera
   │     └─ toolButton_closeCamera
   ├─ widget_templateControls
   │  └─ horizontalLayout_templateControls
   │     ├─ toolButton_selectTemplate
   │     ├─ toolButton_createTemplate
   │     ├─ toolButton_saveTemplate
   │     └─ toolButton_editCharacterTemplates
   └─ widget_inspectionControls
      └─ horizontalLayout_inspectionControls
         ├─ toolButton_startInspection
         └─ toolButton_stopInspection
```

外层布局的三个直接布局项分别是相机、模板和检测容器；每个容器内部的按钮保持同一行。三个容器在 `.ui` 中静态创建，不增加 C++ 类、状态对象或运行时装配代码；`widget_templateControls` 用于按检测模式整体显示或隐藏模板操作。

## 4. 布局参数

### 4.1 外层布局

- 保留 `groupBox_mainControls` 和 `horizontalLayout_mainControls`。
- 保留现有外边距：左、右各 8px，上、下各 12px。
- 三个容器之间使用 18px 间距；不增加嵌套标题、说明标签、装饰性分组框或分隔控件。
- 相机、模板、检测容器的水平伸展比例采用 `1:2:1`，使八个按钮的可用宽度接近一致。
- `groupBox_mainControls` 继续保持固定 200px 高度，不额外占用主图像显示区域。

### 4.2 容器与按钮布局

- 外层和三个容器内部均为水平布局，八个按钮视觉上只占一行；顺序固定为打开相机、关闭相机、选择模板、制作模板、保存模板、分割字符、启动识别、停止识别。
- `horizontalLayout_cameraControls`、`horizontalLayout_templateControls` 和 `horizontalLayout_inspectionControls` 的左、上、右、下边距均显式设为 0，避免嵌套布局默认边距影响 `1:2:1` 分配和按钮宽度。
- 相机、模板、检测容器内的按钮间距均为 8px。
- 八个按钮全部使用 `QToolButton`，统一采用 `Qt::ToolButtonTextUnderIcon`。
- 八个按钮统一使用现有可扩展尺寸策略和 85px 最小高度，在固定 200px 的主控操作台内保持同一行等高显示。
- `toolButton_saveTemplate` 删除原 `uiRole="primary"`，不再单独使用 `QPushButton` 主按钮样式。
- 六个现有 `QToolButton` 保持对象名；两个迁入按钮使用新的 `toolButton_` 对象名。
- 八个按钮继续使用现有正式 QSS 的同一组普通、悬停、按下和禁用样式。

六个既有按钮保持当前图标资源和图标属性。两个迁入按钮暂时复用 `app/resource/image.qrc` 中的既有资源：

- `toolButton_saveTemplate` 使用 `:/Img_Icon_database.ico`。
- `toolButton_editCharacterTemplates` 使用 `:/Img_Icon_image.ico`。

两个临时图标均使用与现有主控按钮一致的 48×48px 图标尺寸。不新增、下载、复制或修改图标资源文件和 QRC 清单；后续更换图标时另行修改这两个既有资源路径。

`toolButton_editCharacterTemplates` 的显示文本改为“分割字符”。当它因检测模式不适用而隐藏时，`horizontalLayout_templateControls` 中的保存模板按钮自动扩展，不留下空白占位。

## 5. 按钮行为保持

### 5.1 保存模板

`toolButton_saveTemplate` 迁入主控操作台后继续：

- 由 `TemplateEditorPage::connectPageActions()` 使用 `QToolButton::clicked` 直接连接 `saveCurrentTemplate()`。
- 由 `TemplateEditorPage::applyOperationState()` 直接执行 `applyOperationUiAccess(m_mainWindowUi.toolButton_saveTemplate, snapshot.saveTemplate)`。
- `snapshot.saveTemplate` 继续由现有 `OperationUiPolicy::create()` 计算，只在 `OperationUiState::TemplateFrozen` 时启用。
- 其他状态继续禁用，并保留“请先获取并冻结模板画面。”的现有禁用原因。
- 不改用 `snapshot.templateSelection`、`snapshot.templateEditing` 或其他权限字段，基础外观改为主控操作台统一工具按钮样式。

不新增 MainWindow 转发槽、回调包装、直接 `setEnabled()`、额外状态判断或禁用原因副本。

### 5.2 分割字符

`toolButton_editCharacterTemplates` 迁入主控操作台后继续：

- 由 `TemplateEditorPage::setupManualCharacterCropUi()` 使用 `QToolButton::clicked` 直接连接字符模板编辑窗口。
- 由 `TemplateEditorPage::applyOperationState()` 继续先以 `snapshot.templateEditing` 初始化局部 `editAccess`；当前模板异常时，继续将 `editAccess.enabled` 设为 `false`，并使用“当前模板状态异常，请移除或重新选择模板。”作为禁用原因。
- 直接执行 `applyOperationUiAccess(m_mainWindowUi.toolButton_editCharacterTemplates, editAccess)`，不改用选择模板的 `snapshot.templateSelection`，也不绕过现有模板异常覆盖。
- `snapshot.templateEditing` 继续由现有 `OperationUiPolicy::create()` 计算：`CameraClosed` 和 `CameraReady` 空闲状态允许编辑；检测、停止、故障、模板预览和模板冻结状态使用现有忙碌原因禁用。
- 只在当前检测模式需要字符模板时显示。

按钮文本改为“分割字符”，不改变字符模板编辑流程，不新增跨页面信号、中间接口、直接 `setEnabled()`、额外状态判断或禁用原因副本。

### 5.3 其余按钮

相机、模板选择、模板制作和检测启停按钮只重新排列，现有信号槽、Page 绑定、动态文字和 `OperationUiPolicy` 全部保持不变。

## 6. 检测模式显示规则

在 `MainWindow::updateTissueRoughnessUiVisibility()` 中：

1. 使用 `ui->widget_templateControls->setVisible(usesTemplate)` 统一控制整个模板操作容器。
2. 删除对 `toolButton_selectTemplate`、`toolButton_createTemplate` 和原 `pushButton_saveTemplate` 的三个重复可见性设置。
3. 保留 `ui->toolButton_editCharacterTemplates->setVisible(showCharacterSettings)`，因为它只适用于需要字符模板的检测模式。
4. 删除已经不存在的 `m_detectionSettingsUi->groupBox_templateCreation` 的可见性设置。

最终效果：

- 模板型检测模式显示模板操作容器；需要字符模板的模式额外显示分割字符按钮。
- 纸巾等不使用模板的模式隐藏整个模板操作容器，相机和检测容器由外层水平布局自动利用空出的宽度。
- 不为显示状态增加成员变量、设置字段或持久化数据。

## 7. 必须完全删除的旧结构

### 7.1 `app/ui/main_window/main_window.ui`

将现有六个直接布局项替换为相机、模板、检测三个水平按钮容器，并将八个按钮放入对应容器。八个按钮只在新布局中各保留一个实例，不保留隐藏副本、占位控件、空分组或多余嵌套布局。

### 7.2 `app/ui/main_window/settings/detection_settings_page.ui`

迁移完成后完全删除：

- 参数设定页中的 `groupBox_templateCreation`。
- `formLayout_templateCreation`。
- 原保存模板按钮在旧“模板制作”分组中的布局项。
- 原分割字符模板按钮在 `groupBox_currentTemplateSettings` 中的旧布局项。

生产代码中的旧名称必须全部删除：

- `pushButton_saveTemplate`。
- `pushButton_editCharacterTemplates`。

两个迁入按钮必须在 UI 所有权和代码引用上完全脱离 `DetectionSettingsPage`：

- 两个新对象只定义在 `Ui::MainWindow`，不在 `Ui::DetectionSettingsPage` 中保留定义、隐藏副本或同名替代控件。
- `TemplateEditorPage` 对两个按钮的状态应用和点击连接只使用 `m_mainWindowUi.toolButton_saveTemplate`、`m_mainWindowUi.toolButton_editCharacterTemplates`；`MainWindow` 的模式可见性只使用 `ui->` 的同名成员。
- `TemplateEditorPage` 中的 `m_detectionSettingsUi.pushButton_saveTemplate`、`m_detectionSettingsUi.pushButton_editCharacterTemplates`，以及 `MainWindow` 中的 `m_detectionSettingsUi->pushButton_saveTemplate`、`m_detectionSettingsUi->pushButton_editCharacterTemplates` 均不得保留；也不保留这两个旧对象的别名、转发槽、lambda 包装、控件查找或兼容路径。

`TemplateEditorPage` 仍直接持有 `Ui::DetectionSettingsPage`，仅用于该页保留的当前模板、目标文字、图像阈值、批量应用和移除模板等真实控件；这不是两个迁入按钮的兼容层，不为此新建中间接口、绑定对象或包装类。

`groupBox_currentTemplateSettings` 保留，因为其中仍有当前模板、目标文字和阈值等有效设置；删除字符模板按钮后由现有布局自然收紧。

### 7.3 `app/ui/main_window/main_window_settings.cpp`

完全删除：

- `m_detectionSettingsUi->groupBox_templateCreation->setVisible(usesTemplate);`

只保留 `ui->widget_templateControls->setVisible(usesTemplate)` 的模板容器可见性设置，以及分割字符按钮的 `showCharacterSettings` 可见性设置。不增加额外显示状态。

### 7.4 `app/resource/qss/app_theme.qss`

从现有组合选择器中删除：

```text
QGroupBox#groupBox_templateCreation
```

不为新布局增加新的主题系统或专用样式。八个按钮直接使用现有 `groupBox_mainControls QToolButton` 规则。保存模板控件删除 `uiRole="primary"` 属性，但全局 `QPushButton[uiRole="primary"]` 规则仍供其他现有确认按钮使用，不得误删。

## 8. 文件修改范围

| 文件 | 修改内容 |
|---|---|
| `app/ui/main_window/main_window.ui` | 静态建立相机、模板、检测三个水平按钮容器并将其内部布局四边距设为 0；将两个迁入按钮创建为 `QToolButton` 并重命名；八个按钮保持一行、统一尺寸策略与 85px 最小高度；将字符按钮显示文本改为“分割字符”；为两个迁入按钮设置既有 QRC 临时图标和 48×48px 图标尺寸 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 删除两个旧按钮布局项、`groupBox_templateCreation` 和 `formLayout_templateCreation` |
| `app/ui/main_window/main_window_settings.cpp` | 将模板按钮的重复显示控制收敛为模板容器显示控制；分割字符按钮可见性改用主窗口生成 Ui 的新对象名；删除旧分组引用 |
| `app/ui/main_window/template/template_editor_page.cpp` | 两个按钮的状态应用和点击连接改为 `Ui::MainWindow` 的新对象名及 `QToolButton::clicked`；保存模板仍直接应用 `snapshot.saveTemplate`，分割字符仍应用由 `snapshot.templateEditing` 加现有模板异常覆盖得到的 `editAccess` |
| `app/resource/qss/app_theme.qss` | 删除已不存在的 `groupBox_templateCreation` 选择器 |

除表内文件外，不修改其他生产代码；不修改 `.pro`、页面所有权、头文件、`InspectionPage`、图标资源文件或 QRC 清单。两个迁入按钮只复用已存在的 QRC 资源路径，后续图标调整另行处理。

## 9. 明确不做

- 不修改 `AppSettings`、JSON Schema 或任何持久化字段。
- 不增加兼容层、迁移代码、旧对象名别名或双路径处理。
- 不增加信号转发、回调包装、适配器或胶水函数。
- 不增加 Manager、Controller、Factory、Builder、事件总线或命令类。
- 不在 `.cpp` 中动态创建固定控件或布局。
- 不新增三个已定义容器之外的包装控件、嵌套层级、分隔控件或视觉分组。
- 不复制按钮，不保留旧按钮作为隐藏备用。
- 不为两个迁入按钮保留任何 `DetectionSettingsPage` 控件引用、别名、转发槽、lambda 包装、`findChild()` 查找或兼容路径。
- 不改变 Page 所有权、生成 Ui 的直接引用边界或现有业务调用链；只同步两个按钮的 Ui 所有者、控件类型和名称。
- 不修改 `OperationUiSnapshot`、`OperationUiPolicy::create()` 或 `applyOperationUiAccess()`，不新增权限字段、状态变量、禁用原因副本或第二套启用逻辑。
- 不让保存模板或分割字符复用 `snapshot.templateSelection`：保存模板固定使用 `snapshot.saveTemplate`，分割字符固定使用带现有模板异常覆盖的 `editAccess`。
- 不对两个迁入按钮增加直接 `setEnabled()`、额外空指针检查、运行时查找、回退路径或其他防御性处理。
- 不修改模板、相机、检测、PLC、统计和故障处理逻辑。
- 不新增、下载、复制或修改图标资源文件、QRC 清单或六个既有按钮的图标属性。
- 不顺带重构其他参数设定区域或主窗口布局。

## 10. 实施顺序

1. 在 `app/ui/main_window/main_window.ui` 建立相机、模板、检测三个静态容器及各自的水平按钮布局，并按最终顺序排列八个按钮。
2. 将保存模板和分割字符模板迁入模板容器，直接改为 `QToolButton` 并使用新的 `toolButton_` 名称；分割字符按钮显示文本写为“分割字符”；分别设置 `:/Img_Icon_database.ico` 与 `:/Img_Icon_image.ico` 临时图标及 48×48px 图标尺寸。
3. 在 `app/ui/main_window/settings/detection_settings_page.ui` 删除参数设定页中的两个旧按钮布局项、已经变空的 `groupBox_templateCreation` 和 `formLayout_templateCreation`。
4. 在 `TemplateEditorPage` 中把两个旧按钮引用直接替换为 `m_mainWindowUi.toolButton_*`，点击连接改用 `QToolButton::clicked`；状态应用按原映射原位迁移：保存模板使用 `snapshot.saveTemplate`，分割字符使用由 `snapshot.templateEditing` 初始化并保留 `m_selectedTemplateInvalid` 覆盖的 `editAccess`。
5. 精简检测模式显示代码，使用 `widget_templateControls` 控制模板操作容器。
6. 删除保存模板的 `uiRole="primary"` 属性，并删除 QSS 中失效的旧分组选择器。
7. 执行静态检查，确认旧结构和旧名称引用清零。

不设置过渡阶段，不保留新旧布局并存状态。

## 11. 静态验收

Agent 实施后只执行项目允许的静态检查：

- `app/ui/main_window/main_window.ui` 与 `app/ui/main_window/settings/detection_settings_page.ui` 均为合法 XML；Qt Designer 能显示三个水平按钮容器中的八个单行按钮。
- 八个按钮均为 `QToolButton`，新的 `objectName` 各出现一次，不存在重复实例。
- `horizontalLayout_mainControls` 直接包含相机、模板、检测三个容器；三个容器的水平按钮布局合计包含八个按钮，不存在隐藏副本、空容器或额外嵌套布局。
- 三个容器内部水平布局的左、上、右、下边距均为 0，容器间距和按钮间距分别为 18px、8px，外层伸展比例为 `1:2:1`。
- `groupBox_templateCreation` 和 `formLayout_templateCreation` 在生产代码、UI 和 QSS 中零引用。
- `pushButton_saveTemplate` 和 `pushButton_editCharacterTemplates` 在生产代码中零引用。
- `pushButton_saveTemplate` 和 `pushButton_editCharacterTemplates` 在 `detection_settings_page.ui` 中零出现；`toolButton_saveTemplate` 和 `toolButton_editCharacterTemplates` 只在 `main_window.ui` 中各出现一次。
- `m_detectionSettingsUi.pushButton_saveTemplate`、`m_detectionSettingsUi.pushButton_editCharacterTemplates`、`m_detectionSettingsUi->pushButton_saveTemplate` 和 `m_detectionSettingsUi->pushButton_editCharacterTemplates` 在生产代码中均为零引用。
- `toolButton_saveTemplate` 和 `toolButton_editCharacterTemplates` 均为 `Ui::MainWindow` 的 `QToolButton`，且状态应用和连接类型均为 `QToolButton`。
- `toolButton_saveTemplate` 和 `toolButton_editCharacterTemplates` 的图标路径分别为 `:/Img_Icon_database.ico` 和 `:/Img_Icon_image.ico`，图标尺寸均为 48×48px；两个路径均存在于 `app/resource/image.qrc`，且没有图标资源文件或 QRC 清单差异。
- `TemplateEditorPage` 中两个按钮的状态应用和点击连接均直接使用 `m_mainWindowUi.toolButton_*`；`MainWindow` 的可见性控制直接使用 `ui->toolButton_editCharacterTemplates` 与 `ui->widget_templateControls`；不增加控件成员别名、`ViewBindings`、转发槽、lambda 包装或 `findChild()`。
- 两个迁入按钮在参数设定页的旧布局项不存在。
- 三个通用模板按钮的单独 `usesTemplate` 可见性设置为零；只保留 `widget_templateControls` 的 `usesTemplate` 可见性设置和分割字符按钮的 `showCharacterSettings` 可见性设置；`groupBox_templateCreation` 引用为零。
- `TemplateEditorPage::applyOperationState()` 中恰有一处 `applyOperationUiAccess(m_mainWindowUi.toolButton_saveTemplate, snapshot.saveTemplate)`；保存模板不使用 `snapshot.templateSelection` 或 `snapshot.templateEditing`。
- `TemplateEditorPage::applyOperationState()` 中的 `editAccess` 继续由 `snapshot.templateEditing` 初始化，继续保留 `m_selectedTemplateInvalid` 的禁用覆盖及原禁用原因，并最终应用到 `m_mainWindowUi.toolButton_editCharacterTemplates`；分割字符不使用 `snapshot.templateSelection`。
- 两个迁入按钮不存在直接 `setEnabled()`、额外状态判断或第二套禁用原因；`OperationUiSnapshot`、`OperationUiPolicy::create()` 和 `applyOperationUiAccess()` 无改动。
- 现有按钮连接、生成 Ui 直接引用和 OperationUiPolicy 引用保持成立。
- 没有新增 AppSettings 字段、JSON 字段、业务状态、动态布局代码或中间转发。
- `git diff --check` 通过。

Agent 不执行构建和程序运行。Qt Creator 构建与交互验证由用户完成。

## 12. 用户验证

1. 在 Qt Designer 中打开 `app/ui/main_window/main_window.ui`，确认相机、模板、检测三个容器依次水平排列，八个按钮保持一行；再打开 `settings/detection_settings_page.ui`，确认其中不再存在两个迁出按钮和空的模板制作分组。
2. 启动软件，确认按钮顺序为打开相机、关闭相机、选择模板、制作模板、保存模板、分割字符、启动识别、停止识别，且文字不截断、布局不溢出。
3. 确认保存模板和分割字符按钮分别显示现有数据库、图像临时图标，图标未缺失。
4. 切换五种检测模式，确认不使用模板的模式隐藏整个模板容器。
5. 检查不需要字符模板的模板模式：保存模板保持显示，分割字符隐藏，模板容器内的保存模板按钮自动分配可用宽度。
6. 检查需要字符模板的模式：保存模板和显示为“分割字符”的按钮均存在于同一行。
7. 在显示模板容器的模式下验证保存模板：仅 `TemplateFrozen` 状态启用；`CameraClosed`、`CameraReady`、`Detecting`、`Stopping`、`Fault` 和 `TemplatePreviewing` 均禁用，悬停显示现有原因“请先获取并冻结模板画面。”。
8. 在需要字符模板的模式下验证分割字符：当前模板有效时仅 `CameraClosed`、`CameraReady` 启用，`Detecting`、`Stopping`、`Fault`、`TemplatePreviewing` 和 `TemplateFrozen` 按现有忙碌原因禁用；当前模板异常时保持禁用并显示“当前模板状态异常，请移除或重新选择模板。”。
9. 验证打开/关闭相机、选择/制作/保存模板、启动/停止识别的原有功能不变。
10. 在右侧抽屉展开和收起状态下检查按钮文字不截断、布局不溢出。

## 13. 完成标准

同时满足以下条件才算实施完成：

- 相机、模板、检测三个容器静态存在于 `app/ui/main_window/main_window.ui` 的同一水平布局；八个按钮按固定业务顺序在三个容器内保持一行，并全部使用 `QToolButton`。
- `pushButton_saveTemplate` 和 `pushButton_editCharacterTemplates` 的旧名称、旧类型和旧连接全部清零。
- 两个迁入按钮只在 `Ui::MainWindow` 中定义；`MainWindow` 和 `TemplateEditorPage` 分别只通过 `ui->toolButton_*` 和 `m_mainWindowUi.toolButton_*` 直接引用，不保留任何 `DetectionSettingsPage` 定义、引用、别名、转发或兼容路径。
- 保存模板仍只应用 `snapshot.saveTemplate`；分割字符仍应用由 `snapshot.templateEditing` 加现有模板异常覆盖得到的 `editAccess`。两者没有直接 `setEnabled()`、新增权限字段、附加状态条件或第二套禁用原因。
- 两个迁入按钮分别使用 `:/Img_Icon_database.ico` 和 `:/Img_Icon_image.ico` 的 48×48px 临时图标，不新增图标资源文件或 QRC 清单条目。
- 参数设定页不再存在保存模板和分割字符按钮。
- 空的模板制作分组及其布局、C++ 引用和 QSS 选择器全部删除。
- 没有兼容层、胶水层、重复控件、重复状态或无效旧代码。
- Agent 静态检查通过，并由用户完成 Qt Creator 构建和人工交互验证。
