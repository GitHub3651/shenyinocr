# OCRGangYin 模板管理分组 UI 优化方案

## 1. 文档状态

- 编写日期：2026-09-19。
- 当前状态：代码实施完成，静态门禁已通过，待用户统一验证。
- 实施结果：模板管理分组已完成主操作整行、次操作双列、分隔线和底部辅助/退出操作布局；五个控件统一改为 `QPushButton` 和 `pushButton_*` 对象名，最小高度为 45px，图标与文字由控件原生整体居中；制作按钮为蓝底白字，选择和保存使用普通按钮样式，分割字符和退出制作为透明无边框样式，全部不使用圆角或渐变；独立 `template_exit.svg` 已登记并由“退出制作”使用。
- 静态验收：目标 UI、QRC、两份 TS 和五个模板 SVG 均可解析；按钮与布局对象唯一、行列、类型、命名和尺寸正确，资源存在且无重复，三类按钮状态完整，旧五个 `toolButton_*` 引用、旧退出按钮文字和旧图标引用已清理；三处 C++ 只同步生成 UI 成员名和信号类型，连接目标与行为不变；`stop.svg`、`.pro` 和其余 `.cpp/.h` 无本方案差异，`git diff --check` 通过。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `3dac004`；实施前只有本方案文档和计划索引处于已暂存状态，生产文件无差异。
- 参考文件：`D:\Browser Dowload\template_management_preview.html`。
- 权威范围：参数设定页 `groupBox_templateManagement` 的内部布局、按钮类型与对象名、按钮样式、模板操作 SVG 和直接相关资源说明。
- 实施边界：仅调整 UI 布局、控件类型与命名、视觉资源，并将退出按钮显示文字由“退出模板制作”收短为“退出制作”；不修改模板功能、状态、权限、连接目标与行为、显隐、Tooltip 或业务流程。
- 构建边界：实施后 Agent 只执行轻量静态检查；qmake、编译、主程序运行和人工视觉验证由用户在 Qt Creator 中完成。

## 2. 目标与结论

原 `groupBox_templateManagement` 使用固定 `3+2` 工具按钮阵列，操作层级和空间关系不清楚。本方案通过布局和克制的表面差异区分主操作、次操作、辅助操作和退出操作。

本方案只参考 HTML 的操作层级和空间关系，不复制 HTML 的页面背景、卡片阴影、说明文案、状态预览、脚本或动画。最终结构为：

```text
模板管理
┌────────────────────────────────┐
│          ＋ 制作模板           │  主操作，整行
├───────────────┬────────────────┤
│  ✓ 选择模板   │   ↓ 保存模板   │  次操作，等宽两列
├────────────────────────────────┤
│  ✂ 分割字符              退出制作 → │  辅助操作与退出操作
└────────────────────────────────┘
```

视觉继续服从项目现有浅灰工业主题：

- 分组背景、边框和标题继续与 `groupBox_currentTemplateSettings` 保持一致。
- 制作按钮使用现有主蓝 `#1769AA` 背景、白色文字和白色图标。
- 选择和保存使用项目普通按钮的白色表面、黑色文字和钢灰边框。
- 分割字符和退出制作使用透明无边框表面，退出操作保留现有危险红 `#C13F3F`。
- 五个按钮不使用圆角或渐变。
- 不增加阴影、装饰图形或动画。

## 3. 与现有方案的关系

本方案局部覆盖 `OCRGangYin模板管理按钮迁入参数设定页方案.md` 中以下两项结论：

1. `groupBox_templateManagement` 内部固定 `3+2` 行列和五个按钮的固定宽高。
2. 退出模板按钮与停止识别、停止预览共用 `stop.svg`。

以下既有结论继续有效：

- 五个模板按钮唯一位于 `DetectionSettingsPage`，不迁回主工具栏。
- `groupBox_templateManagement` 与 `groupBox_currentTemplateSettings` 保持同级。
- 五个按钮的对象名按实际类型由 `toolButton_*` 统一改为 `pushButton_*`；Tooltip、信号连接目标和操作语义不变；`pushButton_exitTemplate` 的可见文字为“退出制作”。
- `pushButton_createTemplate` 继续显示“制作模板 / 拍照框选 / 重新取景”三种既有动态文字。
- `pushButton_editCharacterTemplates` 继续按检测模式显示或隐藏。
- 纸巾模式继续隐藏整个模板管理分组。
- `TemplateEditorPage` 继续负责选择、制作、保存、分割字符及五个按钮的操作状态。
- `MainWindow` 继续负责退出模板制作的跨区域收口。
- `OperationUiPolicy`、设置状态、模板格式、算法、相机、PLC、统计、存图和 CSV 行为不变。

本方案同时服从 `OCRGangYin工业视觉风格UI改造方案.md` 的单一正式 QSS、浅灰工业色板、无阴影和静态 QRC SVG 边界；模板管理五个按钮按本方案明确取消圆角。

## 4. 当前实现事实

- `groupBox_templateManagement` 位于 `app/ui/main_window/settings/detection_settings_page.ui`，内部使用 `gridLayout_templateManagement`。
- 制作按钮位于整行，选择和保存位于等宽双列，分割字符和退出制作位于底部操作行两侧。
- 五个控件统一使用 `QPushButton` 和 `pushButton_*` 对象名，最小高度为 45px，图标尺寸为 22px。
- `QPushButton` 原生将左侧图标和文字作为整体居中，不依赖 `QToolButton` 的标签绘制规则。
- 制作按钮为蓝底白字，选择和保存使用普通按钮样式，分割字符和退出制作为透明无边框样式。
- `stop.svg` 只用于停止识别和停止预览；退出制作使用独立 `template_exit.svg`。

## 5. 最终布局

### 5.1 布局结构

保留 `gridLayout_templateManagement` 对象名，在 `.ui` 中直接定义最终布局：

```text
gridLayout_templateManagement
├─ row 0, column 0, colspan 2: pushButton_createTemplate
├─ row 1, column 0:            pushButton_selectTemplate
├─ row 1, column 1:            pushButton_saveTemplate
├─ row 2, column 0, colspan 2: line_templateManagementSeparator
└─ row 3, column 0, colspan 2: horizontalLayout_templateManagementFooter
   ├─ pushButton_editCharacterTemplates
   ├─ horizontalSpacer_templateManagementFooter
   └─ pushButton_exitTemplate
```

具体要求：

- 制作模板按钮占满整行，是分组内唯一主操作。
- 选择模板和保存模板使用相同水平拉伸，形成等宽双列。
- 分隔线只表达操作层级，不承担功能或状态。
- 分割字符按钮位于底部左侧，退出制作按钮位于底部右侧。
- 分割字符按钮隐藏时，伸展项继续把退出按钮保持在右侧，不增加占位控件或 C++ 布局调整。
- 五个按钮删除固定最大宽度，按布局可用宽度伸展；不修改左侧抽屉的最小宽度、Splitter 状态或持久化规则。
- 不增加横向滚动条、响应式布局类、resizeEvent、事件过滤器或运行时重排。

### 5.2 尺寸与间距

- 分组内部边距继续与现有参数页卡片一致。
- 主按钮最小高度为 45px。
- 选择和保存按钮最小高度为 45px。
- 底部两个操作按钮最小高度为 45px。
- 主按钮与次操作行之间、次操作行内部使用约 8px 间距。
- 分隔线前后使用约 10px 留白。
- 图标显示尺寸统一为 20～22px，避免当前 27px 图标压缩文字空间。
- 按钮文字保持项目现有 16px、字重 600，不引入新的字号档位。
- 五个按钮不设置圆角，与项目普通直角按钮保持一致。

### 5.3 文本与图标排列

五个按钮均使用 `QPushButton`，图标位于文字左侧，图标与文字由控件原生作为一个整体在各自按钮内水平、垂直居中。底部两个按钮仍分别位于操作行左右两侧。

不新增 HTML 中的“模板制作”说明标题、“选择已有模板……”提示、状态预览按钮或底部说明。现有按钮文字和 Tooltip 已足够表达操作，不增加第二套状态提示。

## 6. QSS 视觉规则

所有 QWidget 视觉仍只写入 `app/resource/qss/app_theme.qss`，不在 `.ui` 或 C++ 中添加 `styleSheet`、颜色、字体或调色板。

### 6.1 制作按钮

`pushButton_createTemplate` 使用现有主操作状态：Normal 为 `#1769AA` 蓝底白字，Hover 为 `#247CB9`，Pressed 为 `#0E568E`，Disabled 为 `#A8C7DE`。图标固定为白色。

### 6.2 选择和保存

`pushButton_selectTemplate` 和 `pushButton_saveTemplate` 直接沿用项目普通 `QPushButton` 的白色表面、钢灰边框及 hover、pressed、focus、disabled 状态。

### 6.3 底部操作

`pushButton_editCharacterTemplates` 和 `pushButton_exitTemplate` 默认背景透明并使用 1px 透明边框占位，视觉上无边框且状态切换时内容不位移。Hover 与 Focus 显示浅色底和可见边框，Pressed 显示更深底色并恢复透明边框；Normal、Hover、Focus 和 Pressed 的文字颜色保持不变。分割字符使用黑色文字、蓝灰 Hover 边框和主蓝 Focus 边框，退出制作使用红色文字、浅红 Hover 边框和危险红 Focus 边框，禁用时恢复透明背景、透明边框和灰色文字。

五个按钮均不设置 `border-radius`，不使用渐变，并通过 `QPushButton` 的居中绘制保证图标和文字整体居中。

### 6.4 分隔线

`QFrame#line_templateManagementSeparator`：

- 固定为 1px 水平线。
- 使用现有次边框色 `#D4DDE5`。
- 无阴影、渐变或装饰端点。

## 7. SVG 重绘方案

### 7.1 统一规范

模板管理图标统一采用：

- `viewBox="0 0 24 24"`。
- 透明背景。
- `fill="none"`。
- 约 `1.8` 的描边宽度。
- `stroke-linecap="round"`。
- `stroke-linejoin="round"`。
- 不使用滤镜、阴影、外部引用、字体图标、Base64 或运行时换色。

Qt SVG 不依赖 HTML 的 `currentColor`。图标直接使用项目固定语义色，避免增加运行时图标处理代码。

### 7.2 图标内容

| 文件 | 图形 | 固定颜色 | 使用方 |
|---|---|---|---|
| `template_make.svg` | 四角取景框加中心加号 | 白色 | 蓝色制作模板按钮 |
| `template_select.svg` | 模板卡片或文件夹加勾选符号 | `#1769AA` | 选择模板 |
| `template_save.svg` | 向下箭头加保存基线 | `#1769AA` | 保存模板 |
| `character_seg.svg` | 简洁剪刀图形 | `#1769AA` | 分割字符 |
| `template_exit.svg` | 门框加向外箭头 | `#C13F3F` | 退出制作 |

图形构成参考 HTML 中相应图标的识别方式，但按 Qt 24×24 小尺寸重新整理比例和路径，不直接复制网页 SVG 代码。

### 7.3 `stop.svg` 边界

`stop.svg` 继续只表达停止识别和停止预览，不改成退出箭头。`pushButton_exitTemplate` 使用新增的 `:/svg/template_exit.svg`。

新增独立退出图标是为解决真实语义冲突，不建立多套主题图标、状态副本或兼容回退。

## 8. 保持不变的行为

实施后必须保持：

- 五个按钮对象名统一为 `pushButton_createTemplate`、`pushButton_selectTemplate`、`pushButton_saveTemplate`、`pushButton_editCharacterTemplates` 和 `pushButton_exitTemplate`。
- 除退出按钮由“退出模板制作”收短为“退出制作”外，其他按钮可见文字不变；五个 Tooltip 均保持不变。
- 制作模板按钮三种动态文字及对应 Tooltip 不变。
- 选择模板仅在现有允许状态可用。
- 制作模板继续要求现有相机和运行状态条件。
- 保存模板继续只在现有模板冻结条件下可用。
- 分割字符继续服从检测模式、模板有效性和 `templateEditing` 权限。
- 退出制作按钮继续只在模板预览或冻结状态可用，并执行现有完整退出收口。
- 禁用时继续显示 `OperationUiPolicy` 给出的禁用原因。
- 纸巾模式继续隐藏模板管理分组和当前模板设置。
- 五种检测模式、模板向导、模板绘图、模板保存、失败回退和磁盘格式不变。
- 左侧导航、抽屉宽度、Splitter 状态、相机、PLC、检测、统计、CSV、存图和日志行为不变。

## 9. 文件级实施范围

| 文件 | 实施修改 |
|---|---|
| `app/ui/main_window/settings/detection_settings_page.ui` | 将固定 `3+2` 阵列改为主操作整行、次操作双列、分隔线和底部操作行；五个按钮改为 `QPushButton` 和 `pushButton_*` 对象名；调整 sizePolicy、高度、图标尺寸和退出图标路径；将退出按钮文字改为“退出制作” |
| `app/resource/qss/app_theme.qss` | 设置蓝色制作按钮、普通选择/保存按钮、透明无边框底部按钮、居中规则和完整控件状态，并设置分隔线样式 |
| `app/resource/svg/template_make.svg` | 重绘为白色取景框加号 |
| `app/resource/svg/template_select.svg` | 重绘为蓝色模板选择图形 |
| `app/resource/svg/template_save.svg` | 重绘为蓝色保存图形 |
| `app/resource/svg/character_seg.svg` | 重绘为蓝色剪刀图形 |
| `app/resource/svg/template_exit.svg` | 新增红色退出图形 |
| `app/resource/image.qrc` | 唯一登记 `svg/template_exit.svg` |
| `app/resource/Translate_CN.ts`、`app/resource/Translate_EN.ts` | 将退出按钮的有效源文字同步为“退出制作”，中文翻译为“退出制作”，英文翻译为 `Exit Creation`；退出 Tooltip 保持不变 |
| `app/resource/README.md` | 同步五个模板图标用途，删除退出模板继续复用 `stop.svg` 的现行说明 |
| `app/ui/README.md` | 同步模板管理最终布局、尺寸策略、图标路径和视觉职责 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案状态、权威范围和任务路由 |
| `app/ui/main_window/main_window.cpp`、`app/ui/main_window/main_window_settings.cpp`、`app/ui/main_window/template/template_editor_page.cpp` | 仅同步五个生成 UI 成员名及相应信号类型，连接目标和行为不变 |

以下生产文件未修改：

- 除上述生成 UI 成员名和信号类型同步外的所有 `.cpp/.h`。
- `app/ui/main_window/operation_ui_policy.*`。
- `app/AutoOCRproject.pro`。
- 模板、Runtime、Detection、Devices、PLC、设置和日志代码。

## 10. 明确不做

- 不增加 HTML 中的说明文字、状态预览、演示按钮或 JavaScript 状态。
- 除退出按钮显示文字改为“退出制作”、五个对象名改为 `pushButton_*` 及信号类型同步外，不改变其他按钮文字、任何 Tooltip、连接目标与行为、权限、状态、显隐或操作顺序。
- 不新增业务状态、动态属性、信号槽、事件过滤器或运行时布局代码。
- 不修改模板页面之外的分组、表单、导航、抽屉或主工具栏布局。
- 不修改左侧抽屉最小宽度和 Splitter 持久化。
- 不建立图标管理器、主题管理器、SVG 生成器、资源别名或回退路径。
- 不新增第三方图标库或依赖。
- 不把样式写入 `.ui` 或业务 C++。
- 不顺手重构模板业务、设置绑定、操作策略或资源目录。

## 11. 实施顺序（已完成）

1. 记录实施前分支、HEAD、暂存区和工作区差异，保护当前 `app_theme.qss` 及其他用户修改。
2. 在 `detection_settings_page.ui` 中一次性完成最终布局，将五个按钮改为 `QPushButton` 和 `pushButton_*` 对象名，保留 Tooltip，只将退出按钮文字改为“退出制作”。
3. 在 `app_theme.qss` 中设置蓝色制作按钮、普通选择/保存按钮、透明无边框底部按钮及完整状态。
4. 原位重绘四个专用模板 SVG，新增 `template_exit.svg`，再更新 `.ui` 和 `image.qrc` 路径。
5. 同步两份 TS 中退出按钮的源文字和翻译，并同步两份直接相关 README；不修改其他翻译或业务文档。
6. 执行静态门禁；不运行 qmake、编译、主程序或设备测试。

实施必须一次形成终局，不保留旧布局副本、旧退出图标引用、注释掉的 QSS 或临时资源。

## 12. Agent 静态门禁

- `detection_settings_page.ui`、`image.qrc` 和五个模板 SVG 均为合法 XML。
- `groupBox_templateManagement`、五个按钮及 `gridLayout_templateManagement` 均保持唯一。
- 制作按钮唯一位于整行；选择和保存唯一位于等宽双列；分割字符和退出唯一位于底部操作行。
- 分割字符隐藏时不需要 C++ 重排，退出按钮仍由伸展项保持右对齐。
- 五个按钮均为唯一的 `QPushButton` 和 `pushButton_*` 对象名，不再保留旧 `toolButton_*`、固定最大宽度或 `toolButtonStyle` 属性。
- 制作、普通和底部透明按钮分别具有完整的 normal、hover、pressed、focus、disabled 样式，且不存在圆角或渐变。
- 底部两个按钮默认无可见边框，hover、focus 显示对应语义边框，pressed 恢复透明边框；交互状态不改变文字颜色。
- 五个按钮的图标和文字均由 `QPushButton` 原生整体居中。
- 模板管理分组之外的按钮类型与样式无计划外变化。
- 四个原有模板 SVG 和新增退出 SVG 的 viewBox、描边、颜色及外部引用符合第 7 节。
- `template_exit.svg` 在 `image.qrc` 中唯一登记，并由 `pushButton_exitTemplate` 唯一引用。
- `pushButton_exitTemplate` 不引用 `stop.svg`；停止识别和停止预览继续引用 `stop.svg`。
- 三个 `.cpp` 只同步五个生成 UI 成员名及相应信号类型，其余生产 `.cpp/.h` 相对实施前基线无本方案引入的差异。
- `pushButton_exitTemplate` 的有效源文字唯一为“退出制作”，两份 TS 分别提供“退出制作”和 `Exit Creation`；旧显示文字“退出模板制作”不再作为该按钮的有效消息。
- 除上述退出按钮文字、控件类型和对象名外，其他按钮文字、全部 Tooltip、动态文字来源、连接目标与行为、权限和显隐逻辑无差异。
- `.ui` 中不新增 `styleSheet`、`font`、`palette` 或颜色属性。
- 仓库不新增生成脚本、图标副本、中间文件或第三方依赖。
- README 只描述实施后的最终布局和资源关系。
- UTF-8、文件末尾换行、资源路径存在性和 `git diff --check` 通过。

静态检查通过只表示文件、资源和结构关系已核对，不表示构建、运行或人工视觉验证通过。

## 13. 用户验证

1. 在 Qt Creator 执行 Run qmake 和 Release Rebuild。
2. 打开参数设定页，确认模板管理分组呈现主操作整行、次操作双列、分隔线和底部操作行。
3. 确认视觉属于现有浅灰工业主题，没有网页卡片阴影、圆角胶囊或额外说明文字。
4. 在默认 400px 抽屉宽度下确认所有中文文字和图标清晰、对齐且不截断。
5. 拖窄和拖宽抽屉，确认按钮不重叠、不跳位，页面不新增横向滚动条。
6. 在 Windows 100%、125% 和 150% 缩放下确认文字、图标、分隔线和按钮边界清晰。
7. 切换字库、深度 OCR、二维码＋三期、钢印和纸巾五种模式，确认分组和分割字符按钮继续按原规则显隐。
8. 在相机关闭、相机打开、检测中、停止中、模板实时取景和模板冻结状态下核对按钮权限、动态文字和禁用原因。
9. 完整验证选择模板、制作模板、拍照框选、重新取景、保存模板、分割字符和退出制作。
10. 确认主工具栏停止识别和停止预览仍显示停止图标，退出制作显示独立退出箭头。
11. 回归左侧抽屉、模板参数、相机、PLC、检测、统计、CSV 和存图，确认没有功能变化。

## 14. 完成条件

- `groupBox_templateManagement` 的最终布局与第 5 节一致。
- 主操作、次操作、辅助操作和退出操作通过布局及克制的表面差异形成清楚层级。
- 五个模板图标与 HTML 参考的简洁线性图形接近，同时符合项目现有固定色板。
- 退出按钮唯一显示“退出制作”并使用独立退出图标，`stop.svg` 的停止语义不被破坏。
- 除退出按钮显示文字外，所有现有功能、状态、权限、显隐、文字、Tooltip 和连接保持不变。
- 没有新增功能、业务状态、运行时布局代码、主题框架、生成脚本或第三方依赖。
- Agent 静态门禁通过，并由用户完成 Qt Creator 构建和第 13 节人工验证。
