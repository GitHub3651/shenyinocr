# OCRGangYin 左侧导航与页面抽屉迁移方案

## 1. 文档状态

- 编写日期：2026-09-06。
- 最后更新：2026-09-07。
- 当前状态：代码已实施，待用户统一验证。
- 实施原则：直接调整现有 Qt 控件和调用点，不复制页面，不保留旧名称兼容层，不增加 UI 管理框架。

## 2. 改造目标

本次改造包含以下内容：

1. 将右侧导航栏迁移到主窗口最左侧。
2. 将导航栏对应的五个页面整体迁移到导航栏右侧、主画面左侧。
3. 使用 `QSplitter` 让页面抽屉宽度可由鼠标拖动，并保存用户调整后的布局状态。
4. 将旧方向性对象名和方法名改为左侧导航对应的新名称。
5. 将五个导航按钮固定为 80×80px，导航栏宽度固定为 80px。
6. 调整指定文字的字号和长文本换行。
7. 调整判定栏的底色、文字和闲置状态显示。

五个页面自身的控件、内容和业务逻辑不变；检测、模板、相机、PLC、统计、存图和设置应用逻辑不属于本次改造范围。

## 3. 最终界面与行为

### 3.1 左侧导航和页面抽屉

展开时的主窗口顺序为：

```text
左侧导航栏 | 五页内容抽屉 | 主画面
```

抽屉收起后的顺序为：

```text
左侧导航栏 | 主画面
```

- 导航栏固定宽度为 80px。
- 五个导航按钮固定为 80×80px，在导航栏顶部连续排列，底部保留 Spacer。
- 页面抽屉仍包含现有五个 `page_*`，页面顺序和按钮对应关系不变。
- 页面抽屉与主画面由水平 `QSplitter` 承载，初始抽屉宽度为 360px。
- QSS 中涉及 `rightNavigation`、`rightPanel` 和 `rightDrawer` 的选择器全部同步为左侧新名称。
- 导航栏的 `border-left` 改为 `border-right`，分隔线显示在导航栏与页面抽屉之间。
- 五个导航按钮保持无边框。
- 分隔柄使用透明背景和无边框样式；界面不显示拖动提示，鼠标进入分隔柄区域时使用 Qt 自带的水平调宽光标。
- 点击导航按钮、再次点击当前按钮收起抽屉；运行故障自动停止保持当前抽屉开合状态。
- 检测启动成功、取消或失败后保持抽屉原有开合状态。

### 3.2 抽屉宽度保存

- 使用 `QSplitter::saveState()` 保存完整布局状态。
- 状态保存到 `AppSettings::leftDrawerSplitterState`，并以 Base64 字符串写入 `ui.leftDrawerSplitterStateBase64`。
- 启动时恢复已保存状态；没有状态时使用 360px 初始宽度。
- 正常关闭窗口时保存一次；拖动过程中不持续写配置。
- “恢复默认设置”清空已保存状态，并把当前抽屉恢复为 360px。
- AppSettings Schema 从 7 升级为 8；旧开发配置沿用现有整体重置流程，不编写迁移或兼容读取代码。

### 3.3 字号和长文本换行

以下控件字号统一改为 20px：

- `label_recognitionText`。
- `lineEdit_totalCount`。
- `lineEdit_ngCount`。
- `lineEdit_detectionDuration`。
- `lineEdit_passRate`。

`label_recognitionText` 显示连续二维码或序列号时允许从任意字符位置换行。实现只处理写入标签的显示副本，不改变原始识别结果。

`textEdit_targetText` 的换行模式改为 Qt 自带的 `QTextOption::WrapAtWordBoundaryOrAnywhere`。

### 3.4 判定栏

- `inspectionImageCanvas` 继续使用 `#202830` 底色和现有弱网格。
- `label_verdictResult` 的默认、闲置、OK 和 NG 状态使用 `#202830` 底色，不显示独立边框。
- 软件启动、检测视图清空和故障自动停止完成后，判定文字为空，不显示“等待检测”。
- 删除检测启动后的“等待结果”逻辑：检测启动成功时不再改写判定栏，软件首次启动时判定栏为空；现有 `clearInspectionView()` 调用位置和清理范围保持不变。
- OK 显示绿色 56px“正确”，NG 显示红色 56px“错误”。
- 运行故障不占用判定栏；自动停止期间沿用 Stopping 状态，完成后回到空闲样式。

## 4. 名称调整

### 4.1 `.ui` 对象和布局

| 当前名称 | 最终名称 |
|---|---|
| `widget_leftPanel` | `widget_mainPanel` |
| `verticalLayout_leftPanel` | `verticalLayout_mainPanel` |
| `widget_rightPanel` | `widget_leftDrawer` |
| `verticalLayout_rightPanel` | `verticalLayout_leftDrawer` |
| `stackedWidget_rightDrawer` | `stackedWidget_leftDrawer` |
| `widget_rightNavigationRail` | `widget_leftNavigationRail` |
| `verticalLayout_rightNavigationRail` | `verticalLayout_leftNavigationRail` |
| `verticalSpacer_rightNavigationRailBottom` | `verticalSpacer_leftNavigationRailBottom` |

新增水平分隔控件：

| 对象名 | 类型 |
|---|---|
| `splitter_leftDrawerMain` | `QSplitter` |

五个导航按钮、五个 `page_*` 页面根节点和页面内部对象名保持不变。

### 4.2 C++ 方法

| 当前名称 | 最终名称 |
|---|---|
| `showRightPanelPage(QWidget *page, QToolButton *button)` | `showLeftDrawerPage(QWidget *page, QToolButton *button)` |
| `hideRightPanel()` | `hideLeftDrawer()` |

现有调用点直接使用新名称，旧声明和定义删除。

## 5. 修改文件

### 5.1 生产文件

| 文件 | 修改内容 |
|---|---|
| `app/ui/main_window/main_window.ui` | 移动并改名导航栏、抽屉和主画面，加入 `QSplitter`，直接设置导航尺寸、初始页、画布和判定栏静态属性 |
| `app/ui/main_window/main_window.cpp` | 同步名称、抽屉开合调用、Splitter 状态恢复和目标文字换行，删除与 `.ui` 重复的初始页、选中状态和伸展系数设置 |
| `app/ui/main_window/main_window.h` | 同步方法声明 |
| `app/ui/main_window/main_window_inspection.cpp` | 同步名称、删除检测启动成功后的 `showWaitingResult()` 调用，并在关闭时保存 Splitter 状态 |
| `app/ui/main_window/main_window_settings.cpp` | 恢复默认设置时清空并重置 Splitter 状态 |
| `app/ui/main_window/inspection/inspection_page.h` | 删除 `showWaitingResult()` 声明 |
| `app/ui/main_window/inspection/inspection_page.cpp` | 删除 `showWaitingResult()` 实现，调整判定栏闲置文字和识别结果换行，删除画布和判定栏静态属性的重复设置 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 将当前模板一行直接放入外层网格，并保存图像阈值的静态输入属性 |
| `app/ui/main_window/settings/image_settings_page.ui` | 保存浏览按钮的固定提示文字 |
| `app/ui/main_window/settings/software_settings_page.ui` | 保存软件数据目录和两个按钮的只读、光标及固定提示属性 |
| `app/ui/main_window/settings/machine_settings_page.cpp` | 删除固定可见状态、固定按钮文字和布局强制刷新 |
| `app/ui/main_window/template/template_editor_page.h/.cpp` | 删除标签宽度计算、重复显隐控制和模板向导静态初始化函数 |
| `app/resource/qss/app_theme.qss` | 将右侧方向选择器改为左侧新名称，将导航栏左边框改为右边框，保持导航按钮无边框和 Splitter Handle 透明无边框，调整字号和判定栏样式，并删除已移除中间容器的选择器 |
| `app/system_support/settings/app_settings.h` | Schema 8 和 Splitter 状态字段 |
| `app/system_support/settings/app_settings.cpp` | Splitter 状态相等比较 |
| `app/system_support/settings/app_settings_store.h` | 同步当前 Schema 8 的 Store 声明说明 |
| `app/system_support/settings/app_settings_store.cpp` | Base64 状态的 JSON 读写 |

`inspection_info_page.ui`、`plc_settings_page.ui`、图片资源、QRC 和工程文件不需要修改。

### 5.2 当前文档

实施完成后同步其中已经过期的方位、对象名和 Schema 信息：

- `docs/development/OCRGangYin计划索引.md`。
- `docs/development/OCRGangYin开发者代码结构与维护指南.md`。
- `docs/development/OCRGangYin现有功能对照表.md`。
- `docs/development/OCRGangYin新架构数据Schema.md`。
- `app/system_support/README.md`。
- `app/ui/README.md`。
- `app/resource/README.md`。

历史方案保留原有记录。

## 6. 实施步骤

1. 调整 `main_window.ui` 的真实区域顺序，加入水平 `QSplitter`，完成对象改名、80px 导航尺寸和静态控件属性设置。
2. 同步 C++ 调用和 QSS，删除 `showWaitingResult()` 的声明、实现和调用，完成抽屉开合、文字换行及判定栏样式调整，并删除与 `.ui` 重复的属性设置和布局宽度计算。
3. 将 Splitter 状态接入 AppSettings Schema 8，完成启动恢复、关闭保存和恢复默认设置。
4. 更新当前说明文档，检查旧方向名称和过期说明。

## 7. 验证清单

1. `main_window.ui` 可以正常加载，项目可以正常构建。
2. 导航栏位于最左侧且宽度为 80px，五个按钮均为 80×80px。
3. 五个导航按钮能够打开、切换和收起原有页面，故障自动停止以及检测启动前后的抽屉状态符合本文规则。
4. 抽屉与主画面之间没有明显分隔标志，鼠标靠近时可以拖动调宽。
5. 调整宽度后关闭并重新启动，Splitter 布局能够恢复；恢复默认设置后回到初始宽度。
6. 旧方向性对象名和方法名在生产代码中不再使用。
7. 五个指定控件的字号为 20px，两处长文本可以在空间不足时换行。
8. 判定栏不显示“等待检测”或“等待结果”；首次启动和现有视图清空入口保持为空，OK、NG 显示符合第 3.4 节，运行故障不占用判定栏。
9. Schema 8 能够保存和读取 Base64 Splitter 状态，Schema 7 继续按现有流程整体重置。
10. 页面内容、设置草稿及检测相关业务功能没有因位置迁移发生变化。
11. Qt Designer 与运行时的静态布局和属性一致，不再引用已删除的当前模板中间容器，也不再通过 C++ 计算标签固定宽度。

生产代码已经通过 qmake 和 MSVC x64 Release 构建、链接及运行库部署；以上交互和视觉项目由用户完成 Qt Creator 统一验证后，本方案完成。
