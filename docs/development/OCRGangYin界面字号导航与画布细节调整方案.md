# OCRGangYin 界面字号、导航与画布细节调整方案

## 1. 文档状态

- 文档日期：2026-09-06。
- 最新决定：除 `label_templateGuideTitle` 和 `label_templateGuideBody` 外，正式 QSS 中当前所有 `18px` 字号统一改为 `16px`；两个模板向导标签统一为 `18px`。用户运行截图已确认 `pushButton_clearSoftwareData` 没有命中现有动态属性危险样式，四个指定按钮必须通过正式 QSS 中并列的固定对象名选择器可靠获得红色背景。右侧五个导航按钮在顶部连续排列，取消按钮边框和选中态左侧蓝条。`label_characterCropCanvas` 与 `inspectionImageCanvas` 使用相同的 `#202830` 底色，但不使用网格；`inspectionImageCanvas` 继续保留现有弱网格。
- 当前状态：代码已实施，Agent 静态门禁通过，待用户在 Qt Creator 统一验证。
- 当前进度：正式 QSS 已完成 `16/18/26/48px` 字号映射、四个固定危险按钮的共享红色状态选择器、右侧导航无按钮边框/无左侧蓝条和字符画布 `#202830` 纯色底；主窗口右侧导航已在第五个按钮后增加唯一垂直 Expanding Spacer；主检测画布网格、QRC、SVG、全部 C++ 和业务行为未改。
- 权威范围：正式 QSS 中本方案列出的字号、四个危险按钮的既有红色角色、右侧导航五个按钮的布局和边框、两个画布的背景，以及直接相关维护文档。
- 实施环境：Qt 5.14.2；Agent 只执行轻量静态检查，构建、运行和人工视觉验证由用户在 Qt Creator 完成。
- 基线：分支 `codex/ocrgangyin-refactor`，HEAD `d7e4135`；当前工作区已有工业视觉、资源整理和文字统一的待验证改动，实施时必须原位保护，不能覆盖、回退、暂存或提交无关差异。

### 1.1 覆盖关系

- 本方案在字号范围内覆盖 `OCRGangYin界面文字统一方案.md` 的“只保留 18/26/48px”结论：实施后正式 QSS 的字号集合为 `16/18/26/48px`，其中 `18px` 只供两个模板向导标签使用。
- 本方案在右侧导航视觉范围内覆盖 `OCRGangYin工业视觉风格UI改造方案.md` 和 `OCRGangYin右侧折叠导航栏界面优化方案.md` 的按钮分隔线及 4px 左侧蓝色选中条结论。
- 其他字体族、字重、文字颜色、控件尺寸、导航交互、画布网格、图标、业务状态和功能合同继续沿用当前已实施基线。

## 2. 实施前事实与问题

### 2.1 字号

实施前，正式生产样式 `app/resource/qss/app_theme.qss` 共有 8 处 `font-size: 18px`，分别用于：

- `QWidget` 根字体；
- `QToolTip`；
- `QGroupBox::title`；
- `label_templateGuideBody`；
- 普通 `QPushButton`；
- 主控 `QToolButton`；
- 右侧导航 `QToolButton`；
- 输入类控件。

实施前，`label_templateGuideTitle` 为 `26px`，`label_templateGuideBody` 为 `18px`。生产 `.ui` 和普通生产 C++ 没有另一套 18px 字号值。

### 2.2 危险按钮

以下四个按钮已经具有 `uiRole="danger"`：

- `pushButton_clearSoftwareData`；
- `pushButton_resetTotalCount`；
- `pushButton_resetNgCount`；
- `pushButton_resetRejectQueue`。

实施前，正式 QSS 已定义 `QPushButton[uiRole="danger"]`：Normal 为 `#C13F3F`，Hover、Pressed 和 Disabled 使用现有同系列红色状态；但用户运行截图确认 `pushButton_clearSoftwareData` 实际仍显示普通白色背景，因此“存在 `uiRole` 即可保证红色”的原判断不成立。

加载顺序为：`ApplicationStartup::run()` 先对 `QApplication` 设置全局 QSS，随后 `MainWindow` 执行各页面的 `setupUi()` 并写入 `.ui` 中的 `uiRole` 动态属性。Qt 样式表对设置样式表之后发生的属性变化不保证自动重新计算；当前 Qt 5.14.2 运行结果已经触发该限制。修复不能继续只依赖 `uiRole`，也不应为四个固定按钮增加 C++ `unpolish()/polish()` 重刷胶水。

### 2.3 右侧导航

实施前，`main_window.ui` 的 `verticalLayout_rightNavigationRail` 按顺序直接包含五个固定高度导航按钮，布局末尾没有伸展项；正式 QSS 为每个按钮设置底部分隔线和 4px 透明左边框，Checked 状态再把左边框改为蓝色。

### 2.4 两个画布与网格

- 实施前，`inspectionImageCanvas` 底色为 `#202830`，并通过 `:/svg/canvas/canvas_background_grid.svg` 重复平铺 32×32 的 `#2B3640` 弱网格。
- 实施前，`label_characterCropCanvas` 与字符示例、字符预览图片共用白色背景规则。
- `canvas_background_grid.svg` 已登记在 `image.qrc`，且只供 `inspectionImageCanvas` 的正式 QSS 使用。它是有效资源，不是未使用文件，不参与 OCR、模板坐标、图像处理或 Overlay 绘制。

## 3. 最终目标

### 3.1 字号

| 对象 | 最终字号 | 字重和颜色 |
|---|---:|---|
| `label_templateGuideTitle` | 18px | 保持 600 和 `#1769AA` |
| `label_templateGuideBody` | 18px | 保持 400 和 `#000000` |
| 当前其他所有 18px 规则 | 16px | 保持各自现有字重和颜色 |
| 当前其他 26px 规则 | 26px | 不变 |
| `label_verdictResult` 主判定 | 48px | 不变 |

实施后的正式字号集合严格为：

```text
16px  18px  26px  48px
```

其中 `18px` 只允许出现在 `label_templateGuideTitle` 和 `label_templateGuideBody` 两个选择器中。静态和动态创建的普通控件继续通过 `QWidget` 根规则及现有控件类型选择器继承 `16px`，不逐个修改 `.ui` 或 C++。

### 3.2 四个危险按钮

- 四个指定按钮继续使用现有 `uiRole="danger"`。
- 在现有危险按钮 Normal、Hover、Pressed 和 Disabled 四组规则的选择器列表中，并列加入四个固定对象名：`pushButton_clearSoftwareData`、`pushButton_resetTotalCount`、`pushButton_resetNgCount`、`pushButton_resetRejectQueue`。
- 每个状态仍只有一份共享颜色声明；动态属性选择器和四个对象名选择器共同命中同一规则，不复制四套样式块或颜色值。
- 固定对象名不依赖动态属性重算，确保四个按钮实际获得现有红色 Normal、Hover、Pressed 和 Disabled 背景。
- 不改变按钮文字、尺寸、启用条件、确认流程或点击行为。
- 其他已有危险按钮保持当前状态，不借本方案改变其角色或外观。

### 3.3 右侧导航

- 在 `verticalLayout_rightNavigationRail` 中第五个按钮之后增加一个垂直 `Expanding` Spacer，使剩余高度全部由底部 Spacer 吸收，五个按钮从顶部连续排列。
- 五个按钮的顺序、80px 导航宽度、72×68 最小尺寸、28×28 图标、文字、Tooltip、Checkable、初始选中状态和信号槽全部保持。
- 删除导航按钮规则中的 `border-bottom` 和 4px `border-left`，保留 `border: none`。
- 删除 Checked 规则中的 `border-left-color`，不增加 `:pressed` 蓝条或其他替代指示条。
- Checked 状态现有近白背景和蓝色文字继续保留；Hover 背景和蓝色文字继续保留，因此选中入口仍可辨识。
- 只取消五个按钮自身的边框；`widget_rightNavigationRail` 外侧现有 1px 面板分隔边框保持不变。

### 3.4 两个画布

- `inspectionImageCanvas` 继续使用 `#202830` 底色、现有网格背景、重复平铺和 `#111A22` 边框，相关 QSS 与 SVG 路径不变。
- 从字符画布、字符示例、字符预览的共用规则中拆出 `label_characterCropCanvas`。
- `label_characterCropCanvas` 的 `background-color` 改为 `#202830`，现有 `#B8C4CE` 边框保持不变。
- 不给 `label_characterCropCanvas` 添加 `background-image`，因此它只与主检测画布底色相同，不显示网格。
- `label_characterSample` 和 `label_characterPreviewImage` 继续使用白色背景和现有边框。
- `CharacterCropLabel` 的空提示、字符框、序号、橙色绘制中框线、坐标和交互全部不变。

## 4. 文件范围与实施结果

### 4.1 生产文件

| 文件 | 实施结果 |
|---|---|
| `app/resource/qss/app_theme.qss` | 调整字号；在现有危险按钮四个状态规则中并列加入四个指定对象名；去除右侧五个导航按钮的边框和蓝条；拆分字符画布背景规则并改为 `#202830`；保留主画布网格 |
| `app/ui/main_window/main_window.ui` | 只在五个右侧导航按钮之后增加一个垂直 Expanding Spacer |

### 4.2 维护文档

| 文件 | 实施结果 |
|---|---|
| `app/ui/README.md` | 实施后同步 `16/18/26/48px` 字号、右侧导航无按钮边框/无蓝条、两个画布的最终背景关系 |
| `docs/development/OCRGangYin界面字号导航与画布细节调整方案.md` | 记录实施进度、静态检查结果和待用户验证状态 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案并同步状态 |

以下文件只核对、不修改：

```text
app/resource/image.qrc
app/resource/svg/canvas/canvas_background_grid.svg
app/resource/README.md
app/ui/main_window/inspection/inspection_info_page.ui
app/ui/main_window/settings/software_settings_page.ui
全部 .cpp/.h
```

## 5. 实施阶段

### 阶段 UVD-0：冻结基线（已完成）

1. 记录分支、HEAD、工作区和暂存区。
2. 保存两个生产文件的实施前差异，保护当前未提交的工业视觉、资源整理和文字统一成果。
3. 重新确认 8 处 18px、两个模板向导标签、五个导航按钮和四个危险按钮的真实状态。

### 阶段 UVD-1：字号与危险按钮（已完成）

1. 将 7 处非模板向导的 `18px` 改为 `16px`。
2. 将 `label_templateGuideTitle` 从 `26px` 改为 `18px`，`label_templateGuideBody` 保持 `18px`。
3. 不改变其他 26px、48px、400/600 字重、文字颜色和 RichText 区域强调。
4. 在现有危险按钮 Normal、Hover、Pressed 和 Disabled 规则中分别并列加入四个固定对象名，使它们与 `QPushButton[uiRole="danger"]` 共用同一组声明。
5. 不移动全局 QSS 加载位置，不增加 C++ 样式重刷、控件级 `setStyleSheet()` 或重复颜色块。

### 阶段 UVD-2：右侧导航（已完成）

1. 在第五个导航按钮后增加唯一的垂直 Expanding Spacer。
2. 删除五个导航按钮的底边框、左边框预留和 Checked 左侧蓝条。
3. 保留导航栏外边框、背景、Hover/Checked 背景与文字颜色及全部交互。

### 阶段 UVD-3：画布底色（已完成）

1. 拆分 `label_characterCropCanvas` 与两个白色预览标签的共用 QSS 规则。
2. 将字符框选画布底色改为 `#202830`，保留其现有边框。
3. 保留主检测画布的 `#202830` 底色、网格 SVG、QRC 登记和重复平铺，不把网格复制到字符画布。

### 阶段 UVD-4：静态门禁已完成，待用户验证

1. 执行第 7 节静态检查。
2. 同步直接相关维护文档。
3. Agent 不执行 qmake、编译、链接、测试或主程序。
4. 用户在 Qt Creator 执行 Run qmake、Rebuild 和第 8 节人工验证。

## 6. 明确保持与禁止

- 不修改任何 C++、业务逻辑、状态计算、信号槽、对象名、文字内容或翻译。
- 不改变四个危险按钮既有状态色阶；固定对象名只并入现有四个共享规则，不建立四份独立样式或运行时 `setStyleSheet()`。
- 不改变其他 26px、48px 字号，不增加新的字号或字重。
- 不改变模板向导 RichText、四种区域强调色、`emphasizedDrawingRegion()` 或 `toHtmlEscaped()`。
- 不改变五个导航按钮的尺寸、顺序、图标、文字、Tooltip、选中逻辑、抽屉开合或 Fault 自动打开规则。
- 不删除导航栏容器自身的 1px 分隔边框。
- 不删除、移动、改写或复制 `canvas_background_grid.svg`，不修改 `image.qrc`。
- 不把网格应用到 `label_characterCropCanvas`，不修改 `InspectionImageCanvas` 或 `CharacterCropLabel` 绘制代码。
- 不改变字符示例图和字符预览图的白色背景。
- 不新增主题管理器、字号配置、动态缩放、布局控制器、兼容层、回退路径、额外状态或防御性判断。
- 不为 `uiRole` 增加 C++ `unpolish()/polish()` 重刷胶水，不把全局 QSS 移回主窗口，也不增加第二次应用级样式加载。
- 不顺手调整计划外颜色、间距、尺寸、圆角、边框、资源或代码。

## 7. 静态门禁

1. `app_theme.qss` 的 `font-size` 唯一值集合严格为 `16px/18px/26px/48px`。
2. `font-size: 18px` 恰好出现两次，且只位于 `label_templateGuideTitle` 和 `label_templateGuideBody`。
3. 正式 QSS 的字重仍只有 400 和 600；字体族、文字颜色和模板向导强调色不因本方案变化。
4. 四个指定按钮均继续具有 `uiRole="danger"`；现有危险按钮 Normal、Hover、Pressed 和 Disabled 四组共享规则分别同时包含 `QPushButton[uiRole="danger"]` 和四个固定对象名，不存在每按钮独立颜色块。
5. `verticalLayout_rightNavigationRail` 中五个按钮保持原顺序，第五个按钮之后恰好有一个垂直 Expanding Spacer。
6. 右侧导航按钮规则保留 `border: none`，不再包含 `border-bottom`、按钮 `border-left` 或 Checked `border-left-color`；导航栏容器自身的 1px 分隔边框仍存在。
7. `inspectionImageCanvas` 继续具有 `background-color: #202830`、网格 `background-image` 和 `background-repeat: repeat`。
8. `label_characterCropCanvas` 具有 `background-color: #202830`，但没有网格背景；两个字符预览标签继续为 `#FFFFFF`。
9. `canvas_background_grid.svg` 文件内容、QRC 登记和主检测画布引用保持不变，全仓没有新增第二处网格使用。
10. `main_window.ui` XML 可解析，QSS 花括号平衡，对象名和资源路径有效。
11. 本方案相对冻结基线不产生任何 `.cpp/.h` 差异，其他生产文件无新增差异。
12. UTF-8、文件末尾换行和 `git diff --check` 通过；静态检查不得表述为构建或视觉验证通过。

## 8. 用户验证清单

1. 检查主窗口、右侧五页和三个模板对话框，确认普通界面文字统一为 16px，现有重要信息 26px、最终判定 48px 保持。
2. 检查模板制作向导标题和正文，确认两者都是 18px；标题仍为蓝色加重，正文仍为黑色常规，区域名称强调色和步骤内容不变。
3. 检查清空软件数据、总数清零、NG 清零和剔除复位按钮，确认 Normal、Hover、Pressed、Disabled 均保持现有红色危险语义。
4. 展开右侧抽屉，确认五个导航入口从顶部连续排列，剩余空间位于底部。
5. 检查五个导航按钮之间没有边框或分隔线，Checked 状态左侧没有蓝条；蓝色文字和近白背景仍能区分当前入口。
6. 逐个切换、收起和重新打开五个页面，确认现有抽屉交互、初始选中、运行时保持和 Fault 自动打开行为不变。
7. 检查主检测画布仍为深蓝灰弱网格，字符框选画布为相同的 `#202830` 纯色底；字符示例和字符预览仍为白色。
8. 加载真实图像并操作模板绘制、字符框选和检测 Overlay，确认背景不覆盖图像，坐标、框线、序号和交互不变。
9. 在 1600×950、1366×768 以及 Windows 100%/125%/150% 缩放下检查文字裁剪、导航排列和画布显示。

## 9. 完成标准

- 两个生产文件完成第 3 节固定调整，没有其他生产文件改动。
- 字号、危险按钮、导航和双画布全部满足第 7 节静态门禁。
- 网格只保留在 `inspectionImageCanvas`，两个画布底色均为 `#202830`。
- 没有新增兼容层、胶水层、防御性判断、运行时样式或多余抽象。
- Agent 静态检查通过。
- 用户在 Qt Creator 完成构建和第 8 节人工验证，并明确确认通过。
