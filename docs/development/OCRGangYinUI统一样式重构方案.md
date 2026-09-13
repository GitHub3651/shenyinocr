# OCRGangYin UI 统一样式重构方案

版本：1.17（目标字符大字体版）
方案日期：2026-08-20  
状态：S1～S3代码与静态门禁已完成；待用户统一验证
适用范围：`app/ui`、`app/resource`、当前正式应用样式表及其资源登记  

## 一、结论

当前 UI 已经使用应用级 QSS，但尚未形成唯一、稳定的样式管理规则。实际显示同时受以下四层控制：

```text
应用级 QSS
  ↓
MainWindow 根控件样式
  ↓
main_window.ui 中的控件级 styleSheet
  ↓
C++ 运行时 setStyleSheet()
```

后面的局部样式经常覆盖前面的全局规则，因此相同用途、相同类型的控件仍会出现边框、颜色、内边距、禁用状态不一致的问题。

本方案采用 Qt 原生能力完成统一，不增加主题管理器、皮肤插件、JSON 主题文件或按钮子类体系。目标结构固定为：

```text
一个正式全局 QSS：app_theme.qss
  + 控件类型默认样式
  + primary / danger 两个按钮角色
  + uiState / verdict / hasError 三个运行属性
  + objectName 单控件覆盖
```

普通控件不再单独编写样式；只有确实需要强调或具有特殊业务语义的控件声明一个简单属性。任何控件仍可通过对象名选择器获得独立样式，但独立样式也必须集中写在正式 QSS 中。

### 1.1 本轮实施结果

- 实施基线：`8b9ef7f`（`image → resource`目录改名已单独完成）。
- `app/resource/qss/1.css`已正式改名为`app_theme.qss`，`.qrc`和加载代码只保留新入口。
- `main_window.ui`原有50个`styleSheet`属性和最后1个控件字体覆盖已全部删除。
- 业务 UI C++ 只设置状态属性，全部正式视觉由 `qApp->setStyleSheet(qss)` 的唯一主题入口定义。
- 静态按钮只使用`primary/danger`；运行视觉只使用`uiState/verdict/hasError`。
- 模板制作按钮原来的两组布尔样式属性已合并为一个`uiState`。
- 当前模板、运行状态、判定结果和四个统计值使用统一可见边框；`label_recognitionText`按用户确认保持背景透明、无边框，由外层“识别内容”分组框提供区域边界。
- 深度OCR继续隐藏其算法不使用的图像阈值和字符模板操作；阈值标题、输入框、单位和按钮使用同一个显隐条件，避免仅残留`%`单位并撑开空白布局。
- 纸巾检测作为无模板模式，完整隐藏“当前产品模板设置”和“模板制作”两个分组框，不保留只有标题和空白边框的空容器。
- “当前编辑模板”和“目标字符内容”标签共用正式QSS规则；两者按当前字体和样式计算内容宽度并采用较大值，使输入区域起点对齐且不依赖硬编码宽度。
- 所有普通控件的启用和禁用只调用`setEnabled()`并由Qt保留控件自身的默认鼠标光标；机器设置页不再设置`ForbiddenCursor`、`IBeamCursor`或调用`unsetCursor()`，禁用视觉统一由QSS提供，禁用原因继续由工具提示提供。
- 识别总数和不合格数的两个清零按钮保留`danger`语义，并在正式QSS中通过对象名统一使用红色实底、白色文字及完整的悬停、按下、禁用状态；其他危险按钮继续使用红色描边样式。
- 运行状态和判定结果继承全局`QGroupBox`标题预留高度；识别总数、不合格数、耗时和合格率四个固定高度统计卡片统一使用`24px`紧凑标题预留，避免标题遮挡内容，也避免标题与数值框距离过大。
- `textEdit_targetText`在正式QSS中使用`40px`字体，其他输入控件继续继承全局`16px`字体，不在`.ui`或C++中增加字体覆盖。
- 按用户最终确认恢复`8b9ef7f`的字体习惯：正式主题全局使用16px粗体；不在`.ui`中恢复局部通配符样式。
- 模板制作向导改为位于图像上方的紧凑横向提示栏，长宽按标题和说明文字自适应，不参与图像区域剩余空间分配；提示栏背景透明、无边框。
- 恢复阶段8中被错误简化的模板制作向导：刚印、字库和深度OCR分别使用对应标题及两步提示，二维码+三期使用三步提示和即时二维码校验；区域过小、当前点数、闭合完成、Esc重置和保存询问均恢复。所有视觉属性只在`app_theme.qss`维护，C++不包含向导颜色、字体、背景或边框样式。
- 自定义图像控件中的ROI、框选框和多边形颜色继续由`QPainter`绘制；它们表达检测区域，不属于控件QSS，不迁移到主题文件。
- 10项静态门禁已通过；Agent未运行qmake、编译、测试或主程序，运行结果等待用户一次性验证。

## 二、重构范围与边界

### 2.1 本方案修改的内容

- 当前正式全局 QSS 的组织和命名。
- 资源物理目录由`app/image`统一改名为`app/resource`，但保留`image.qrc`文件名和现有Qt资源前缀。
- `main_window.ui` 中的控件级 `styleSheet`。
- UI C++ 文件中的 `setStyleSheet()`、样式字符串拼接和原样式保存/恢复代码。
- 静态控件的 `uiRole` 属性，仅允许 `primary` 和 `danger`。
- 运行时状态控件的 `uiState`、`verdict`、`hasError` 属性。
- 全局样式的UTF-8读取方式、资源路径和调色板处理。
- 动态创建控件的对象名和样式角色声明。
- `app/ui/README.md` 中的样式维护规则。

### 2.2 本方案不修改的内容

- 不改变主窗口布局、标签页结构和控件对象名，除非实施时发现明确重复或错误对象名并单独记录。
- 不改变五种检测模式的显隐规则。
- 不改变按钮的业务命令、点击行为、Application Service 调用或底层防御。
- 不改变相机、PLC、Runtime、Detection、模板和设置数据结构。
- 不改变算法、阈值语义、统计口径、图片保存规则和 PLC 合同。
- 不删除图片、图标、模型、DLL、翻译、`.qrc` 或其他资源。
- 本轮不实现运行时主题切换、深色主题、用户自定义主题或主题持久化。
- 不为每种按钮新建 C++ 子类。
- 不新增 `ThemeManager`、`StyleManager`、主题 Store、主题 Service 或事件总线。

### 2.3 必须保持的已验证行为

- `OperationUiPolicy` 继续决定控件是否可以操作以及禁用原因。
- UI 的 `setEnabled()` 语义保持，QSS 只负责视觉呈现。
- 相机关闭、相机打开、检测中、停止中、模板预览和模板冻结状态保持；Runtime 内部故障在界面层按停止中显示。
- 故障自动停止、模板移除、设置应用、模板保存和清空数据等业务行为保持。
- 所有禁用控件仍必须提供当前已有的禁用原因提示。
- UI 不直接拥有设备、生产线程、检测算法、PLC 时序或存图服务。

## 三、当前样式事实盘点

### 3.1 当前正式样式入口

当前 `MainWindow::initStyle()` 从资源路径读取：

```text
:/qss/1.css
```

并通过以下方式作用于整个应用：

```cpp
qApp->setStyleSheet(qss);
```

`app/resource/qss` 当前存在 15 个 CSS 文件，但运行时只加载 `1.css`。其余文件虽然登记在资源文件中，但没有正式主题选择入口。

### 3.2 当前样式数量

当前 `main_window.ui` 包含 121 个控件，其中：

- 50 个控件拥有自己的 `styleSheet`。
- 这些控件使用约 28 种不同的内嵌样式。
- UI C++ 代码中约有 29 处 `setStyleSheet()` 调用。
- 全局 QSS 约 204 行。

因此，当前并不是“没有全局样式”，而是全局样式没有成为唯一正式来源。

### 3.3 已确认的按钮差异原因

`pushButton_applyTargetText` 在 `.ui` 中拥有独立样式：

```css
QPushButton {
    background-color: #ffffff;
    border: 1px solid #ebeef5;
    border-radius: 4px;
    color: #333333;
    padding: 5px 10px;
}
```

`pushButton_applyImageThreshold` 没有控件级样式，因此继承全局规则：

```css
QWidget QPushButton {
    background-color: #ffffff;
    color: #333333;
    border: 1px solid #303133;
    border-radius: 4px;
    padding: 10px 15px;
}
```

两者虽然都是普通参数应用按钮，但边框颜色和内边距不同，最终显示自然不一致。

### 3.4 当前重复样式

已经确认的重复包括：

- 9 个设置区域重复使用相同卡片样式。
- 6 个统计或状态 GroupBox 重复使用 `border:none`。
- 3 个滚动区重复使用透明、无边框样式。
- 3 个滚动内容页重复使用白色背景。
- 3 个普通按钮复制同一套浅灰边框样式。
- 目标文字、图像阈值、拍照距离等标签和输入框存在多份相似样式。

这些重复应由父区域或控件类型选择器统一覆盖，不应继续保存在各控件的 `styleSheet` 中。

### 3.5 当前 C++ 样式覆盖

运行时代码还会覆盖以下视觉状态：

- 运行状态标签的正常、停止和警告颜色。
- OK/NG 判定文字的颜色和字号。
- 设置控件的硬件禁用样式。
- 清空软件数据与恢复默认按钮。
- 字符模板编辑器中的提示、错误、预览框和输入错误边框。
- 模板制作按钮的注意状态。

模板制作按钮已经采用动态属性选择器，这是正确方向；其余状态应迁移到同一机制。

### 3.6 当前字体和颜色问题

根控件中存在：

```css
* { font-weight: bold; }
```

它会让按钮、普通说明、单位、输入框和标题全部加粗。原问题不在于粗体本身，而在于该规则散落于`main_window.ui`并覆盖全局主题。用户最终决定保留全局粗体，因此实施时把规则集中迁移到唯一正式`app_theme.qss`，不恢复`.ui`局部样式。

当前蓝色、边框色和状态色也未冻结。例如主色同时出现 `#0078D7`、`#1976D2`、`#409EFF`、`#1677D2`，边框同时出现 `#303133`、`#C0C4CC`、`#DCDFe6`、`#E4E7ED`、`#EBEEF5`。

### 3.7 当前样式加载隐患

现有代码通过固定字符位置提取调色板颜色：

```cpp
QString paletteColor = qss.mid(20, 7);
```

QSS 文件开头已经包含注释，固定位置并不能稳定表示颜色。该逻辑应删除；单一浅色主题直接由 QSS 定义即可。如果未来确实需要设置 `QPalette`，必须使用明确常量，不能解析文件固定偏移。

## 四、统一样式的设计原则

### 4.1 默认一致

同类型、同用途控件在没有额外声明时必须自动获得完全一致的样式。

例如以下按钮默认应一致：

- 确认字符。
- 设置图像阈值。
- 批量设置字符。
- 批量设置阈值。
- 应用拍照距离。
- 应用曝光、增益、颜色通道和旋转。
- 应用 PLC 参数。

### 4.2 语义优先于对象名

“主要动作”“危险动作”“次要动作”是可复用的语义，应通过动态属性复用，不应为每个对象名复制一套完整样式。

### 4.3 状态与业务分离

C++ 负责说明当前状态是什么，QSS 负责决定该状态显示成什么颜色、边框和背景。

```text
C++：verdict = ng
QSS：NG 使用红色文字和浅红背景
```

业务代码不应拼接颜色值。

### 4.4 个别控件仍可覆盖

确实独一无二的控件允许使用对象名选择器覆盖，但覆盖必须写在正式 QSS 的“单控件例外”区域，不写在 `.ui` 或业务 C++ 中。

### 4.5 布局与样式分离

- 控件位置、布局、伸缩比例和页面结构继续由 `.ui` 管理。
- 字体、颜色、边框、圆角、悬停、按下、禁用和焦点由 QSS 管理。
- 最小高度等确实需要跨页面统一的尺寸可以进入 QSS。
- 仅与某个布局有关的固定尺寸继续保留在 `.ui`，避免把布局参数全部塞入 QSS。

### 4.6 不增加抽象层

本方案不建立任何主题类层次。Qt 的 QSS、动态属性、伪状态和对象名选择器就是唯一机制。

## 五、目标样式架构

### 5.1 唯一正式入口

将当前含义不明确的 `1.css` 正式改名为：

```text
app/resource/qss/app_theme.qss
```

应用只加载这一个正式文件：

```text
:/qss/app_theme.qss
```

现有其他主题资源本轮不删除，但必须在文档和资源注释中明确标记为“未启用历史资源”，不得在生产代码中形成多个隐式入口。`1.css → app_theme.qss`是本方案已经冻结的必做项，不再保留旧文件名作为第二入口或兼容别名。

### 5.2 QSS 内部固定顺序

正式 QSS 按以下顺序组织：

```text
1. 主题说明与颜色规范
2. 全局字体和基础背景
3. 容器、卡片、分隔条和滚动区
4. QLabel
5. QPushButton
6. QToolButton
7. QLineEdit / QTextEdit / SpinBox / ComboBox
8. QCheckBox
9. QTabWidget / QTabBar
10. 语义角色
11. 运行状态
12. 单控件例外覆盖
```

后续维护不得打乱顺序，也不得在文件中间随意追加重复的 `QPushButton` 基础定义。

### 5.3 第一版颜色规范

第一版只冻结一套浅色主题：

| 用途 | 建议颜色 | 说明 |
|---|---|---|
| 主背景 | `#F5F7FA` | 主窗口背景 |
| 卡片背景 | `#FFFFFF` | GroupBox、设置卡片 |
| 次级背景 | `#FBFBFC` | 参数分组内部背景 |
| 主文字 | `#303133` | 正文、按钮、输入值 |
| 次级文字 | `#606266` | 说明文字 |
| 辅助文字 | `#909399` | 提示、单位、空状态 |
| 禁用文字 | `#A8ABB2` | disabled |
| 普通边框 | `#DCDFE6` | 输入框、按钮、卡片 |
| 浅边框 | `#E4E7ED` | 分割、禁用边框 |
| 主色 | `#1677D2` | 主按钮、运行强调 |
| 主色悬停 | `#409EFF` | hover/focus |
| 主色浅背景 | `#ECF5FF` | focus、选中、提示 |
| 成功色 | `#67C23A` | OK、成功 |
| 警告色 | `#E6A23C` | 警告 |
| 危险色 | `#D93025` | 故障、危险操作 |
| NG 色 | `#F56C6C` | NG 结果 |

QSS不支持原生CSS变量，第一版不增加字符串模板或变量替换器。颜色表作为维护规范，实际值在QSS中显式书写。

### 5.4 字体规范

建议统一：

```text
字体族：Microsoft YaHei、Segoe UI、sans-serif
正文：16px，Bold
按钮：16px，Bold
次级说明：14px，Bold
卡片标题：16px，Bold
运行状态：24px，Bold
判定结果：36～40px，Heavy
统计数值：20px，Bold
```

删除根控件局部的`* { font-weight: bold; }`，在唯一正式`app_theme.qss`的`QWidget`基础规则中统一设置`font-weight: bold`；状态、判定结果和关键数值继续使用更大的字号或更高字重。

## 六、控件类型默认样式

### 6.1 普通按钮

所有没有 `uiRole` 的 `QPushButton` 使用统一默认样式：

```css
QPushButton {
    min-height: 38px;
    background-color: #ffffff;
    color: #303133;
    border: 1px solid #dcdfe6;
    border-radius: 4px;
    padding: 5px 12px;
    font-size: 16px;
    font-weight: 600;
}

QPushButton:hover {
    background-color: #ecf5ff;
    border-color: #409eff;
}

QPushButton:pressed {
    background-color: #d9ecff;
    border-color: #1677d2;
}

QPushButton:disabled {
    background-color: #f5f7fa;
    color: #a8abb2;
    border-color: #e4e7ed;
}
```

### 6.2 输入控件

`QLineEdit`、`QTextEdit`、`QSpinBox`、`QDoubleSpinBox`和`QComboBox`共享基础边框、背景、字体、focus和disabled规则；只在确有控件结构差异时补充下拉按钮或步进按钮样式。

必须覆盖：

- 正常。
- hover（适用时）。
- focus。
- disabled。
- readOnly。
- validation error。

### 6.3 复选框

`QCheckBox`统一字体、间距和indicator尺寸。`checkBox_hardwareTriggerEnabled`不再持有完整局部QSS；如果它需要更醒目，只设置一个语义角色或对象名覆盖。

### 6.4 卡片和分组

设置页中的以下容器使用统一卡片规则：

- 当前模板设置。
- 模板制作。
- 检测模式。
- 图片保存。
- 图像采集与处理。
- PLC触发模式。
- PLC连接。
- PLC工艺参数。
- 软件数据。

不再在9个控件中复制同一段背景、边框、圆角和内边距。

### 6.5 滚动区和内容页

三个设置滚动区、三个滚动内容页使用父级或对象属性统一处理，删除各自重复的透明背景、无边框和白色背景内嵌样式。

## 七、语义角色设计

### 7.1 允许的角色

第一版只允许默认样式和两个角色：

| `uiRole` | 用途 | 默认视觉 |
|---|---|---|
| 不设置 | 普通确认、应用、浏览、连接、恢复、移除引用 | 白底、普通边框 |
| `primary` | 保存、启动等当前页面主要动作 | 主色背景、白字 |
| `danger` | 清空、清零、危险操作 | 危险色文字或背景 |

不为每个业务动作建立新角色。例如不增加 `saveTemplateButton`、`applyThresholdButton`、`plcButton` 等一次性角色。

### 7.2 角色选择器示例

```css
QPushButton[uiRole="primary"] {
    background-color: #1677d2;
    color: #ffffff;
    border-color: #1677d2;
}

QPushButton[uiRole="primary"]:hover {
    background-color: #409eff;
    border-color: #409eff;
}

QPushButton[uiRole="primary"]:disabled {
    background-color: #a0cfff;
    color: #f5f7fa;
    border-color: #a0cfff;
}

QPushButton[uiRole="danger"] {
    background-color: #ffffff;
    color: #d93025;
    border-color: #d93025;
}

QPushButton[uiRole="danger"]:hover {
    background-color: #fff2f0;
}
```

### 7.3 当前按钮角色映射

| 控件 | 角色 |
|---|---|
| `pushButton_applyTargetText` | 默认 |
| `pushButton_applyBatchTargetText` | 默认 |
| `pushButton_applyImageThreshold` | 默认 |
| `pushButton_applyBatchImageThreshold` | 默认 |
| `pushButton_applyPhotoDistance` | 默认 |
| 相机参数应用按钮 | 默认 |
| PLC参数应用按钮 | 默认 |
| `pushButton_saveTemplate` | `primary` |
| `pushButton_clearSoftwareData` | `danger` |
| `pushButton_restoreDefaultSettings` | 默认 |
| `pushButton_resetTotalCount` | `danger`，正式QSS对象名覆盖为红色实底 |
| `pushButton_resetNgCount` | `danger`，正式QSS对象名覆盖为红色实底 |
| `pushButton_resetRejectQueue` | `danger` |
| 当前编辑模板的“移除模板” | 默认，文字和提示继续明确“只移除引用” |
| 左侧六个主操作 `QToolButton` | 不设置角色，继续由父容器统一选择器管理 |

左侧主操作按钮已经被 `groupBox_mainControls` 限定，可以继续使用父容器选择器，不要求为六个按钮逐个写属性。

## 八、运行状态样式

### 8.1 运行状态标签

C++只设置：

```text
uiState = idle
uiState = ready
uiState = running
uiState = stopping
uiState = warning
```

QSS示例：

```css
QLabel#label_runtimeStatus[uiState="idle"],
QLabel#label_runtimeStatus[uiState="ready"] {
    color: #303133;
    background-color: #eef1f6;
}

QLabel#label_runtimeStatus[uiState="running"] {
    color: #1677d2;
    background-color: #ecf5ff;
}

QLabel#label_runtimeStatus[uiState="warning"] {
    color: #e6a23c;
    background-color: #fdf6ec;
}

```

### 8.2 判定结果

C++只设置：

```text
verdict = idle
verdict = ok
verdict = ng
```

QSS负责字体、背景和颜色：

```css
QLabel#label_verdictResult[verdict="idle"] {
    color: #909399;
}

QLabel#label_verdictResult[verdict="ok"] {
    color: #67c23a;
}

QLabel#label_verdictResult[verdict="ng"] {
    color: #f56c6c;
}

```

### 8.3 输入校验状态

字符模板名称等输入错误不再调用 `setStyleSheet()`，只在错误时设置布尔属性：

```text
hasError = true
```

```css
QLineEdit[hasError="true"] {
    border-color: #d93025;
    background-color: #fff2f0;
}
```

### 8.4 硬件禁用状态

普通可交互控件继续使用 `setEnabled(false)`，视觉统一由 `:disabled`完成。

关联标签同步调用 `setEnabled(enabled)`，并由 `QLabel:disabled`统一变灰，不再增加`uiDisabled`属性。因此可以删除当前 `MachineSettingsPage::disabledStyle()` 生成整段样式，以及保存和恢复每个控件原 `styleSheet` 的逻辑。

### 8.5 动态属性刷新

静态角色在 `.ui` 或控件创建时设置，无需反复刷新。

运行时属性变化后，如果Qt没有立即重新匹配QSS，应执行：

```cpp
widget->style()->unpolish(widget);
widget->style()->polish(widget);
widget->update();
```

第一版不新增专门的样式管理类；只在现有少量运行状态更新点执行刷新。不得为此建立新的 Controller、Manager 或事件系统。

## 九、单控件独立样式能力

### 9.1 允许方式

任何控件都可以在正式QSS最后的“单控件例外”区域通过对象名覆盖：

```css
QPushButton#pushButton_saveTemplate {
    min-height: 46px;
}
```

该按钮仍然继承 `uiRole="primary"` 的背景、颜色、边框和状态，只覆盖自己确实不同的高度。

### 9.2 禁止方式

普通生产控件不得：

- 在 `.ui` 中粘贴完整 `styleSheet`。
- 在槽函数或Presenter中拼接颜色字符串。
- 为一个按钮建立专用C++子类只为改颜色。
- 复制一整套 hover、pressed、disabled 规则。
- 通过父控件局部QSS无意覆盖所有子控件。

### 9.3 例外审批规则

新增对象名覆盖前必须回答：

1. 它是否可以使用现有默认样式？
2. 它是否可以使用现有 `uiRole`？
3. 差异是否只是布局尺寸，应放在 `.ui`？
4. 该差异是否在 hover、pressed、disabled 时仍然合理？

只有前三种方式均不能表达需求时，才增加对象名覆盖。

## 十、样式加载方案

### 10.1 加载时机

保留当前 `MainWindow::initStyle()`入口，不把样式加载迁移到`ApplicationStartup`，避免为了视觉统一扩大启动层修改范围。`initStyle()`仍在主窗口显示前执行，之后创建的自定义对话框继续继承应用级QSS。

### 10.2 UTF-8读取

统一使用：

```cpp
const QString qss = QString::fromUtf8(file.readAll());
qApp->setStyleSheet(qss);
```

不再使用 `QLatin1String` 读取正式UTF-8资源。

### 10.3 调色板

删除：

```cpp
qss.mid(20, 7)
```

第一版优先只使用QSS，不额外设置QPalette。如果个别系统控件必须依赖QPalette，再使用显式常量设置相关角色，不从QSS文本中猜测颜色。

### 10.4 加载失败

正式QSS资源属于程序包内资源，加载失败时：

- 写入明确日志。
- 程序可以继续使用Qt默认样式启动，避免纯视觉资源问题阻止生产功能。
- 不在运行时扫描磁盘寻找另一个CSS作为隐藏回退。
- 不自动切换到15个历史CSS中的任意一个。

## 十一、动态创建控件的处理

当前以下控件由C++动态创建：

- 当前编辑模板下拉框和移除按钮。
- 模板制作引导框。
- 检测信息滚动区和分隔条提示。
- 模板选择对话框控件。
- 字符模板编辑对话框控件。

动态控件创建后只允许设置：

- `objectName`。
- `uiRole`，且只允许`primary`或`danger`。
- `uiState`。
- `verdict`。
- `hasError`。
- 与布局有关的尺寸、边距和伸缩策略。

示例：

```cpp
m_removeCurrentTemplateButton->setObjectName(
            QStringLiteral("toolButton_removeCurrentTemplate"));
```

移除模板使用默认按钮样式，不增加`secondary`角色。不得在动态创建点继续写完整QSS字符串。字符预览框、提示和错误标签使用对话框范围选择器、明确对象名或`hasError`，不继续扩展全局`uiRole`枚举。

## 十二、计划涉及的文件

### 12.1 主要修改文件

| 文件 | 计划修改 |
|---|---|
| `app/image/` → `app/resource/` | 改名程序内置资源物理目录；保留Qt运行时资源前缀和`image.qrc`文件名 |
| `app/resource/qss/1.css` → `app/resource/qss/app_theme.qss` | 改名并建立唯一正式样式；旧路径不保留兼容入口 |
| `app/resource/image.qrc` | 删除`qss/1.css`登记并唯一登记`qss/app_theme.qss` |
| `app/resource/README.md` | 明确唯一正式QSS与未启用历史主题边界 |
| `app/ui/main_window.ui` | 删除普通控件内嵌样式，增加少量动态角色属性 |
| `app/ui/main_window.cpp` | 模板取景按钮的两组样式布尔属性合并为`uiState` |
| `app/ui/main_window_inspection.cpp` | `initStyle()`改读`:/qss/app_theme.qss`和UTF-8；删除QSS拼接、调色板固定偏移和状态样式字符串 |
| `app/ui/main_window_settings.cpp` | 清空/恢复按钮改用角色属性 |
| `app/ui/pages/inspection_page.cpp` | 运行状态和判定改用状态属性 |
| `app/ui/pages/machine_settings_page.cpp` | 删除禁用样式生成和原样式保存/恢复 |
| `app/ui/pages/template_editor_page.cpp` | 动态模板控件声明角色，不写局部样式 |
| `app/ui/dialogs/template_selection_dialog.cpp` | 动态控件接入统一样式 |
| `app/ui/dialogs/character_template_editor_dialog.cpp` | 错误、提示、预览改用属性选择器 |
| `app/ui/README.md` | 写入样式维护规则和角色清单 |

### 12.2 不需要新增的文件

- 不新增 `theme_manager.*`。
- 不新增 `style_manager.*`。
- 不新增 `ui_theme.json`。
- 不新增每种按钮、标签、输入框的子类。
- 不新增主题Store或Application Service。

## 十三、分阶段实施计划

按用户要求，`app/image → app/resource`物理目录重命名先行完成。其余内容仍固定为一次UI样式重构、一次用户统一验证，不建立六个独立阶段、中间提交或长期过渡状态；内部只按以下三步连续完成。

### 阶段 S1：建立唯一正式样式（已完成）

#### 工作

1. 已先行完成：将物理目录`app/image`改名为`app/resource`，同步qmake、`.ui`、`.gitignore`和文档中的磁盘路径；保留`image.qrc`文件名及现有`:/...`运行时资源路径。
2. 将`app/resource/qss/1.css`改名为`app/resource/qss/app_theme.qss`。
3. 同步`image.qrc`和`MainWindow::initStyle()`样式资源路径，不保留旧样式路径兼容入口。
4. 按第五节顺序整理QSS，完成控件默认样式、`primary/danger`、`uiState/verdict/hasError`和对象名例外区。
5. 使用`QString::fromUtf8()`读取QSS。
6. 删除固定偏移解析调色板颜色和C++追加禁用QSS的逻辑。

#### 退出条件

- 应用只加载`:/qss/app_theme.qss`。
- QSS资源路径唯一、文件存在、UTF-8有效。
- 不存在隐藏主题扫描或回退。

### 阶段 S2：清理局部样式并接入统一规则（已完成）

#### 工作

1. 删除`main_window.ui`中普通按钮、输入框、卡片、滚动区和标签的重复完整样式。
2. 删除根控件局部字体通配符，把全局16px粗体集中迁移到正式`app_theme.qss`。
3. 仅给保存模板等主要动作设置`uiRole=primary`，给清空、清零等危险动作设置`uiRole=danger`。
4. 运行状态改用`uiState`，判定改用`verdict`，输入错误改用`hasError=true`。
5. 设置控件和关联标签统一使用`setEnabled()`与`:disabled`，删除`disabledStyle()`和原样式保存/恢复。
6. 清理主窗口、设置页、检测页、模板页和两个对话框中的样式字符串。
7. 动态控件只设置对象名、已批准属性和布局参数。

#### 退出条件

- `pushButton_applyTargetText`与`pushButton_applyImageThreshold`走同一默认规则。
- 业务C++不包含颜色、边框、圆角和完整QSS字符串。
- 动态控件、Designer静态控件和自定义对话框使用同一视觉语言。
- `OperationUiPolicy`、`setEnabled()`、tooltip和全部业务槽保持原行为。

### 阶段 S3：静态门禁与文档已完成，等待用户统一验证

#### 工作

1. 更新`app/ui/README.md`中的唯一入口、两个角色、三个运行属性和对象名覆盖规则。
2. 执行第十五节10项静态门禁。
3. 用户在Qt Creator执行Run qmake、Rebuild和第十四节视觉/功能验证。

#### 退出条件

- 10项静态门禁全部通过。
- 用户确认主界面、动态控件、对话框、运行状态和禁用状态视觉一致。
- 五种模式、相机、PLC、模板、设置和检测业务没有回归。

## 十四、用户验证矩阵

### 14.1 普通控件状态

| 控件 | normal | hover | pressed | focus | disabled | readOnly | error |
|---|---:|---:|---:|---:|---:|---:|---:|
| QPushButton | 必验 | 必验 | 必验 | 可选 | 必验 | 不适用 | 不适用 |
| QToolButton | 必验 | 必验 | 必验 | 可选 | 必验 | 不适用 | 不适用 |
| QLineEdit | 必验 | 可选 | 不适用 | 必验 | 必验 | 必验 | 必验 |
| QTextEdit | 必验 | 可选 | 不适用 | 必验 | 必验 | 必验 | 必验 |
| QSpinBox | 必验 | 可选 | 不适用 | 必验 | 必验 | 不适用 | 必验 |
| QComboBox | 必验 | 必验 | 必验 | 必验 | 必验 | 不适用 | 不适用 |
| QCheckBox | 必验 | 必验 | 必验 | 必验 | 必验 | 不适用 | 不适用 |

### 14.2 运行状态

依次验证：

1. 相机关闭。
2. 相机打开、未检测。
3. 正在检测。
4. 正在停止。
5. 故障自动停止。
7. 模板实时预览。
8. 模板冻结等待框选。
9. 模板保存完成。
10. 模板加载异常。

检查每种状态下：

- 主操作按钮文字和样式。
- 设置控件禁用和禁用原因。
- 模板控件禁用和禁用原因。
- 运行状态标签。
- 判定标签。
- 鼠标悬停和按下状态。

### 14.3 五种模式

分别切换：

- 刚印检测。
- 字库匹配。
- 深度OCR。
- 纸巾检测。
- 二维码+三期。

检查：

- 控件显隐保持。
- 普通应用按钮样式一致。
- 单模板、多模板和无模板UI样式一致。
- 当前编辑模板、移除模板、批量按钮和阈值控件状态合理。

### 14.4 结果与故障

- 未检测空状态。
- OK结果。
- NG结果。
- ROI警告。
- 图片保存失败。
- PLC或采集异常触发自动停止。
- 故障自动停止完成后的空闲状态。

确认颜色变化由状态属性控制，不改变统计、存图或PLC行为。

### 14.5 对话框

- 模板选择对话框。
- 字符模板编辑对话框。
- QMessageBox提示、警告和错误。
- QFileDialog和目录选择。
- 模板同名覆盖确认。

原生Windows文件对话框可能继续采用系统样式。本方案不为了统一外观强制切换为Qt非原生文件对话框，避免无必要改变用户体验。

### 14.6 分辨率和缩放

至少验证：

- 当前生产分辨率。
- 1600×950设计尺寸。
- Windows 100%缩放。
- Windows 125%缩放。
- Windows 150%缩放。

检查文字截断、按钮高度、Tab标题、下拉箭头、滚动区域和动态控件位置。

## 十五、静态门禁

实施完成后确认以下10项：

1. `app/image`物理目录及生产引用为零；`app/resource`是唯一程序内置资源目录；`app_theme.qss`存在并在`.qrc`中唯一登记，`1.css`正式路径和生产引用均为零。
2. 应用只加载`:/qss/app_theme.qss`，不存在扫描多个历史CSS、兼容别名或自动主题回退。
3. QSS使用`QString::fromUtf8()`读取；`qss.mid(20, 7)`等固定偏移颜色解析为零。
4. `main_window.ui`中的普通控件没有完整内嵌QSS；根控件局部字体通配符和9份重复卡片样式为零；全局16px粗体只在正式`app_theme.qss`定义一次；两个示例按钮走同一默认规则。
5. 除全局`qApp->setStyleSheet(qss)`外，控件样式相关C++中的颜色值、边框QSS和完整`setStyleSheet()`为零，单控件例外只在正式QSS末尾；`QPainter`绘制的ROI、框选框和检测多边形颜色不属于控件皮肤，保持原语义。
6. `uiRole`只允许`primary`和`danger`；`secondary/mainAction/preview/hint/errorText`等扩展角色为零。
7. 运行动态属性只使用`uiState/verdict/hasError`；`validationState/uiDisabled`为零。
8. `MachineSettingsPage::disabledStyle()`及原样式保存/恢复逻辑删除，禁用视觉来自`:disabled`，`OperationUiPolicy`、`setEnabled()`和禁用原因保持。
9. `main_window.ui` XML、QSS资源、UTF-8、末尾换行和`git diff --check`全部通过；除`app/image → app/resource`目录改名、正式样式表及其`.qrc`登记外无其他资源差异。
10. 没有新增ThemeManager、StyleManager、主题Store、主题Service或控件子类体系，也没有修改检测、Runtime、设备、模板、设置Schema或PLC合同；Agent不运行qmake、编译、测试程序或主程序，最终运行门禁由用户在Qt Creator完成。

## 十六、风险与处理

### 16.1 QSS优先级变化

删除控件级样式后，某些控件会开始继承父容器或应用级规则。必须按页面逐项检查，而不能只检查两个示例按钮。

处理：每移除一类局部样式，都确认正式QSS已经存在对应默认规则。

### 16.2 动态属性不立即刷新

运行时修改属性后，部分Qt控件可能不会立即重绘。

处理：在现有状态切换点执行`unpolish/polish/update`，不新增主题管理层。

### 16.3 disabled被局部规则覆盖

如果对象名覆盖或角色规则写在`:disabled`之后且未限制状态，可能重新覆盖禁用颜色。

处理：每个角色必须明确提供自己的`:disabled`；单控件覆盖原则上只改尺寸，不覆盖禁用颜色。

### 16.4 全局选择器影响对话框

应用级 `QWidget`、`QPushButton`规则会同时作用于自定义对话框和QMessageBox。

处理：把统一作用视为预期；只有Qt原生系统对话框作为明确例外。不要用过宽的父级局部QSS意外覆盖所有子控件。

### 16.5 视觉优化误改业务状态

删除样式代码时可能误删`setEnabled()`、tooltip或状态判断。

处理：只迁移颜色、边框、背景和字体；`OperationUiPolicy`、`setEnabled()`、禁用原因和业务槽函数必须保持。

### 16.6 历史主题资源造成误解

资源目录仍保留多个CSS文件。

处理：本轮不删除资源；只建立唯一生产入口，并在README中说明其他文件未启用。是否清理资源由后续单独决策。

## 十七、回滚原则

- 样式重构不迁移设置Schema，不写用户数据，不需要数据回滚。
- 正式QSS、`.ui`属性和C++状态属性必须作为同一轮提交，避免半套样式长期存在。
- 如果用户验证发现视觉问题，在同一轮修复对应选择器，不恢复逐控件复制QSS。
- 如果某个控件确实无法由统一规则表达，先增加正式QSS对象名覆盖，不在C++中恢复样式字符串。
- 回滚整个提交即可恢复原视觉；不得为新旧样式增加运行时兼容开关。

## 十八、完成判据

全部满足才算完成：

- 一个正式QSS成为唯一生产样式来源。
- 普通控件不再拥有额外样式。
- 同类型、同用途控件默认完全一致。
- `pushButton_applyTargetText`和`pushButton_applyImageThreshold`视觉一致。
- 特殊按钮只通过`primary/danger`两个`uiRole`表达语义。
- 运行状态、判定和输入错误只通过`uiState/verdict/hasError`表达。
- 所有禁用状态统一。
- C++业务代码不包含颜色和完整QSS字符串。
- 每个控件仍可通过正式QSS中的对象名选择器独立覆盖。
- 没有增加主题管理框架或大量新文件。
- 主界面、动态控件和自定义对话框使用同一套视觉语言。
- 五种检测模式的显隐、按钮使能和业务操作不变。
- 用户完成Qt Creator构建和统一视觉/功能验证。

## 十九、后续维护指南

### 19.1 新增普通按钮

1. 在`.ui`或现有页面中创建按钮。
2. 设置合理对象名和文字。
3. 不填写`styleSheet`。
4. 自动继承普通按钮样式。

### 19.2 新增主按钮

1. 不填写`styleSheet`。
2. 设置动态属性`uiRole=primary`。
3. 检查normal、hover、pressed和disabled。

### 19.3 新增危险按钮

1. 确认操作确实具有清空、清零或破坏性含义。
2. 设置`uiRole=danger`。
3. 危险确认逻辑继续属于业务代码，不属于QSS。

### 19.4 新增运行状态

1. 先确认现有`uiState`能否表达。
2. C++只设置状态值。
3. 在正式QSS中增加对应选择器。
4. 不在C++写颜色。

### 19.5 修改单个控件

1. 优先使用默认样式。
2. 其次使用现有角色。
3. 再判断是否只是布局尺寸。
4. 最后才在QSS例外区添加对象名选择器。
5. 不在Qt Designer的`styleSheet`属性中复制完整QSS。

### 19.6 修改全局视觉

只修改正式QSS中对应控件类型或角色规则，并一次性检查所有页面和对话框，不逐控件修补。

## 二十、冻结决定

本方案冻结以下决定，实施时不得另建平行机制：

1. 只维护一个正式浅色主题。
2. 普通控件依赖类型默认样式。
3. 特殊按钮只使用`primary/danger`两个角色；普通、恢复、移除引用和左侧主操作按钮不新增角色。
4. 运行状态、判定和输入错误只使用`uiState/verdict/hasError`；普通状态通过属性缺省表达。
5. 单控件独立样式使用正式QSS中的对象名选择器。
6. `.ui`不保存普通控件完整QSS。
7. 业务C++不保存、恢复或拼接样式字符串。
8. 不增加ThemeManager、StyleManager、主题配置文件或控件子类体系。
9. 物理目录必须由`app/image`改名为`app/resource`；`image.qrc`文件名和现有Qt运行时资源前缀保持不变。
10. `1.css`必须改名为`app_theme.qss`并成为唯一正式入口；其他历史CSS资源本轮不删除，但不形成第二个生产入口。
11. 本轮只改变视觉表达和资源物理目录命名，不改变任何已验证业务行为。
