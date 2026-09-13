# OCRGangYin 图像提示栏模板来源与状态隔离修复方案

## 1. 文档状态

- 文档用途：指导图像显示区提示栏恢复模板图片来源，并隔离正式识别状态与模板制作向导。
- 编写日期：2026-08-24。
- 最新修订：2026-08-24，根据用户统一验证反馈补充正式检测状态；图像上下文提示栏在检测中显示“【当前状态】正在检测中...”，不再把“正式识别”误等同于“提示栏隐藏”。
- 当前状态：代码实施完成，静态门禁通过，等待用户在 Qt Creator 统一构建和人工验证。
- 当前基线：分支 `codex/ocrgangyin-refactor`，实施前 HEAD 为 `a7fa961`；创建计划前工作区和暂存区均无差异。
- 权威范围：`InspectionPage` 的正式识别图像状态、`TemplateEditorPage` 的模板原图来源与模板制作提示、现有 `frame_templateGuide` 的显示规则。
- 上位计划：`OCRGangYinUI架构精简优化方案.md`。本文是该计划 UI 提示栏范围的独立纠偏，不替代五模式 `ImageLabel` 绘图流程、强类型向导绑定或钢印四步制作合同。
- 历史依据：提交 `1c8d564fe42ce5717b7606513cc2b426a367ff4b` 时刻的代码快照会在模板原图显示后提示“正在显示模板【名称】的产品图像”；该提交本身只新增计划文档，不应误写为该行为的引入提交。
- 实施门禁：用户已于 2026-08-24 明确授权生产代码修改；构建和主程序人工验证仍由用户在 Qt Creator 完成。

## 2. 结论

继续复用 `main_window.ui` 中现有的：

- `frame_templateGuide`；
- `label_templateGuideTitle`；
- `label_templateGuideBody`。

该 Frame 在界面上的准确职责是“图像上下文提示栏”：

1. 空闲且画面确实来自已加载模板原图时，显示模板图片来源。
2. 正式检测时，显示与模板制作无关的检测状态。
3. 模板实时取景或框选时，显示当前模式的模板制作状态和步骤。
4. 停止、故障、纸巾模式或没有有效模板原图时隐藏。

不新增控件、类、状态机、Presenter、Manager、配置字段或长期图片来源状态。来源文字只在实际显示模板原图的公共加载入口生成。

## 3. 当前问题与原因

### 3.1 正式识别状态误入模板向导

当前调用链为：

```text
InspectionPage::present()
    ↓ updateImageDisplayStatus("正在显示相机采集图像...")
MainWindow::inspectionPageCallbacks()
    ↓
TemplateEditorPage::updateImageDisplayStatusText()
    ↓
更新普通运行状态 + 按当前检测模式显示模板制作标题
```

`updateImageDisplayStatusText()` 同时修改普通运行状态和模板提示栏，但没有检查 `CaptureState`。因此正式识别虽然已经取消模板绘图，第一帧检测结果仍会重新显示“刚印检测模板制作”等错误标题。

### 3.2 模板原图仍显示，但来源提示丢失

当前四种模板模式已经统一经过：

```text
TemplateEditorPage::loadTemplateAtIndex()
    ↓
TemplateApplicationService::beginEdit()
    ↓
EditableTemplate::rawImage
    ↓ displayPreviewFrame()
主图像控件
```

当前代码会设置 `m_currentTemplateDisplayName` 和“当前编辑模板”控件，也会显示 `editable.rawImage`，但没有再把“当前画面来自哪个模板”写入图像区提示栏。

### 3.3 历史行为的准确边界

在 `1c8d564` 代码快照中，字库类当前 Profile 切换会：

1. 加载对应目录的 `template_raw.png`；
2. 在主图像控件显示该原图；
3. 在同一提示区域显示“【当前状态】正在显示模板【目录名】的产品图像”。

本方案将这一有用行为通过当前统一的 `loadTemplateAtIndex()` 扩展到刚印、字库、深度 OCR 和二维码＋三期四种模板模式。纸巾模式没有模板，不显示来源。

## 4. 目标行为

| 场景 | 标题 | 正文 | Frame |
|---|---|---|---|
| 空闲且刚显示有效模板原图 | `【当前图像】` | `正在显示模板【名称】的产品图像。` | 显示 |
| 模板实时取景 | 当前模式模板制作标题 | `实时取景中，调整产品位置后点击拍照并开始框选。` | 显示 |
| 模板冻结并框选 | 当前模式模板制作标题 | 当前步骤、区域、点数或错误提示 | 显示 |
| 正式识别 | `【当前状态】` | `正在检测中...` | 显示 |
| 停止中或故障自动停止 | 无 | 统一按停止中状态呈现 | 隐藏 |
| 停止识别后仍显示最后一帧相机图像 | 无 | 不把相机图像冒充成模板原图 | 隐藏 |
| 模板不存在、加载失败或没有原图 | 无 | 原有错误提示保持 | 隐藏 |
| 纸巾模式 | 无 | 纸巾不使用模板 | 隐藏 |

提示栏状态优先级为：

```text
模板制作状态
    > 正式检测状态
    > 空闲且实际显示模板原图
    > 其他场景隐藏
```

“当前选择了模板”不等于“当前画面来自模板”。只有模板原图已经成功送入图像控件后才能显示来源。

## 5. 简洁实现原则

1. `InspectionPage` 直接更新自己已经持有的 `label_runtimeStatus`，`OperationUiPolicy` 继续管理相机、识别、停止和模板取景等统一操作状态；故障自动停止按停止中呈现。
2. `TemplateEditorPage` 只管理图像上下文提示栏中的正式检测状态、模板来源、模板实时取景说明和绘图向导，不写普通运行状态栏。
3. 四种模板模式只在公共 `loadTemplateAtIndex()` 增加一次来源显示，不按模式复制代码。
4. 模板名称复用现有 `QFileInfo(path).fileName()` 和 `m_currentTemplateDisplayName`，不新增正式状态字段。
5. 保持现有 `.ui` 对象名和 QSS 选择器，不做布局或主题调整。
6. 不使用通用字符串事件、事件总线或新的状态枚举；现有 `CaptureState` 和 `ImageLabel` 绘图状态已经足够。

## 6. 计划修改

### 6.1 `InspectionPage` 收回正式识别状态

文件：

- `app/ui/pages/inspection_page.h`
- `app/ui/pages/inspection_page.cpp`

操作：

1. `present()` 显示有效检测图像后，直接把“正在显示相机采集图像...”写入现有 `label_runtimeStatus`。
2. 删除唯一用途为转发该文字的 `InspectionPage::Callbacks::updateImageDisplayStatus`。
3. 如果 `Callbacks` 删除该字段后为空，则删除整个 `Callbacks` 结构、构造参数和成员，不保留空壳。
4. 保持现有 ROI 警告、普通运行状态样式、识别结果、模板名称、统计和耗时呈现不变。

目标调用链：

```text
InspectionPage::present()
    ├── 显示检测结果图
    └── 直接更新普通运行状态栏
```

正式识别状态不得再调用 `TemplateEditorPage`。

### 6.2 删除 MainWindow 的跨页文字转发

文件：

- `app/ui/main_window.h`
- `app/ui/main_window.cpp`

操作：

1. 删除 `MainWindow::inspectionPageCallbacks()` 中的图像状态转发。
2. 若 `InspectionPage` 不再需要任何 Callback，则删除该工厂函数及声明。
3. 同步简化 `initializePages()` 中 `InspectionPage` 的构造调用。
4. 保留 MainWindow 对检测模式切换、应用服务、相机、PLC、模板制作启停等真正跨页面行为的协调。

### 6.3 拆除含混的模板页通用状态函数

文件：

- `app/ui/pages/template_editor_page.h`
- `app/ui/pages/template_editor_page.cpp`

删除：

```cpp
updateImageDisplayStatusText(const QString &body)
```

增加两个页面私有小函数：

```cpp
void showTemplateImageSource(const QString &templateName);
void showTemplateCaptureStatus(const QString &body);
```

用户验证修正后再增加一个固定文案的小函数：

```cpp
void showInspectionStatus();
```

职责固定为：

- `showTemplateImageSource()`：标题为“【当前图像】”，正文为模板来源，只操作图像上下文提示栏。
- `showTemplateCaptureStatus()`：只用于 `Previewing/Frozen`，标题使用当前检测模式的模板制作标题，也只操作图像上下文提示栏。
- `showInspectionStatus()`：只由统一操作状态入口在 `Detecting` 时调用，标题为“【当前状态】”，正文为“正在检测中...”。
- `updateTemplateGuideText()`：继续只负责冻结后 `ImageLabel` 绘图步骤。
- `hideTemplateGuide()`：继续作为统一隐藏入口。

上述四个显示函数和一个隐藏函数均不得写 `label_runtimeStatus`。检测模式和模板取景对应的普通运行状态继续由现有入口呈现，不在模板页复制。

`showTemplateCaptureStatus()` 必须检查：

```cpp
m_captureState == CaptureState::Previewing
|| m_captureState == CaptureState::Frozen
```

不满足时只允许隐藏或返回，不能显示模板制作标题。

### 6.4 在统一模板加载入口显示来源

文件：`app/ui/pages/template_editor_page.cpp`

在 `loadTemplateAtIndex()` 中：

1. 从 `path` 计算一次模板显示名。
2. 成功取得非空 `editable.rawImage`。
3. 先通过现有 `displayPreviewFrame(editable.rawImage)` 显示模板原图。
4. 再调用 `showTemplateImageSource(templateName)`。
5. 使用同一个 `templateName` 更新 `m_currentTemplateDisplayName`，避免两处名称来源不一致。

公共调用关系：

```text
启动恢复 / 模式切换 / 当前模板切换 / 选择模板确认 / 保存后重载
    ↓
loadTemplateAtIndex()
    ↓
显示 editable.rawImage
    ↓
showTemplateImageSource(QFileInfo(path).fileName())
```

该唯一入口同时覆盖：

- 刚印检测；
- 字库匹配；
- 深度 OCR；
- 二维码＋三期。

不得为四种模式分别增加分支或重复函数。

### 6.5 清理与切换规则

以下场景必须最终隐藏旧提示：

- 模板索引无效；
- `beginEdit()` 失败；
- 模板原图为空或无法显示；
- 移除最后一个模板；
- 切换到纸巾模式；
- 正式识别停止中或触发故障自动停止；
- 关闭相机；
- 模板预览失败或退出模板制作且当前画面不是磁盘模板原图。

现有 `cancelTemplateDrawing()` 已统一隐藏 Frame，继续作为运行状态切换时的唯一取消/隐藏路径。模板索引无效、加载失败或没有原图等模板页局部失败，在公共加载/清理出口统一隐藏。不得在启动、停止、故障、关闭相机等每个处理函数中重复散落 `hideTemplateGuide()`。

停止识别后不自动重载模板原图。本次只保证提示与实际画面一致；若画面仍是最后一帧相机图像，提示栏保持隐藏。以后若单独决定“停止后自动恢复模板原图”，应作为另一项可观察行为修改处理。

## 7. 文件级范围

| 文件 | 计划修改 |
|---|---|
| `app/ui/pages/inspection_page.h` | 删除图像状态转发 Callback；若结构为空则整体删除 |
| `app/ui/pages/inspection_page.cpp` | 检测图像状态直接写普通运行状态栏 |
| `app/ui/main_window.h` | 删除无用的 InspectionPage Callback 工厂声明 |
| `app/ui/main_window.cpp` | 删除检测页到模板页的状态转发并简化页面构造 |
| `app/ui/main_window_inspection.cpp` | 把强类型 `OperationUiState` 传给模板页统一操作状态入口 |
| `app/ui/pages/template_editor_page.h` | 删除含混公开函数，声明两个私有小函数 |
| `app/ui/pages/template_editor_page.cpp` | 统一检测状态、模板来源、模板取景和绘图向导职责；公共加载入口显示来源 |
| `docs/development/OCRGangYin图像提示栏模板来源与状态隔离修复方案.md` | 实施后更新进度、静态证据和待验项 |
| `docs/development/OCRGangYin计划索引.md` | 同步计划状态和进度 |

预计不修改：

- `app/ui/main_window.ui`；
- `app/resource/qss/app_theme.qss`；
- `app/ui/widgets/image_label.*`；
- `app/ui/controllers/operation_ui_policy.*`；
- `app/application/*`；
- `app/templates/*`；
- Detection、Runtime、Devices、PLC、测试工程和 qmake 清单。

实施中若发现必须修改上述预计不修改范围，应停止并重新核对计划，不能顺手扩大。

## 8. 行为保持

以下行为不得改变：

- 五种检测模式名称、顺序和切换结果。
- 刚印、深度 OCR 单模板；字库、二维码＋三期多模板；纸巾无模板。
- 模板选择、预勾选、取消、确认、当前编辑模板和移除引用。
- 模板原图内容、模板参数、字符资产、模板保存和覆盖语义。
- `ImageLabel` 的字库/OCR 两步、二维码三步、钢印四步绘图状态。
- 二维码即时解码校验和保存询问。
- 模板 Schema、AppSettings、模板目录和资源文件名。
- 正式识别算法、结果、阈值、模板最高分选择、统计、耗时、PLC 和存图合同。
- OperationUiPolicy 的按钮文字、启用状态和禁用原因。
- 故障自动停止和现场安全提示。
- 正式样式继续只来自 `app_theme.qss`。

允许改变的用户可观察行为只有：

1. 四种模板模式显示模板原图时统一显示正确来源。
2. 正式识别途中显示“【当前状态】正在检测中...”，不显示任何“模板制作”标题。
3. 无有效模板原图时不残留上一模板的来源文字。

## 9. 禁止的过度设计

- 不新增 `ImageStatusManager`、`GuideManager`、Presenter、Coordinator 或状态机类。
- 不新增图像来源枚举、DTO、事件总线或共享全局变量。
- 不为四种模板模式分别写来源显示函数。
- 不新增或重命名 `.ui` 控件。
- 不修改 QSS，只复用现有提示栏外观。
- 不让 `ImageLabel` 生成中文文案或读取模板路径。
- 不把模板来源保存到 AppSettings、模板 Schema 或磁盘。
- 不自动重载、复制或转换模板图片。
- 不顺带重构模板选择、检测结果呈现、相机线程或 Runtime。
- 不用代码行数目标驱动额外删除；只删除本次已经确认无用的转发。

## 10. 实施步骤

### 步骤 1：隔离正式识别状态

1. 让 `InspectionPage::present()` 直接更新普通运行状态栏。
2. 删除 `InspectionPage` 的图像状态 Callback。
3. 删除 MainWindow 对该 Callback 的创建和模板页转发。
4. 确认正式识别链不再引用模板提示函数。

### 步骤 2：恢复统一模板来源

1. 删除含混的 `updateImageDisplayStatusText()`。
2. 增加 `showTemplateImageSource()` 与 `showTemplateCaptureStatus()`。
3. 在公共 `loadTemplateAtIndex()` 成功显示原图后显示来源。
4. 在无模板、失败、纸巾和清理入口隐藏旧提示。

### 步骤 3：保持模板制作向导

1. 实时取景改用 `showTemplateCaptureStatus()`。
2. 冻结后的强类型绘图事件继续使用现有 `updateTemplateGuideText()`。
3. 核对 Previewing/Frozen 门禁，确保 Idle/Detecting 不出现模板制作标题。
4. 确认模板页四个显示函数均不写普通运行状态栏，模板取景状态继续由 `OperationUiPolicy` 呈现。

### 步骤 3.1：补充正式检测状态

1. 让 `TemplateEditorPage::applyOperationState()` 接收现有强类型 `OperationUiState`。
2. `Detecting` 时显示固定检测状态；停止中（包括故障自动停止）复用 `cancelTemplateDrawing()` 隐藏。
3. 不新增状态字段、字符串回调、管理类、`.ui` 控件或 QSS。

### 步骤 4：静态收口

1. 执行零引用、文件范围、文案唯一性和依赖边界检查。
2. 执行 `git diff --check`。
3. 更新本文与计划索引的实际进度。
4. 不执行或宣称 Qt 构建和人工交互已通过。

### 10.1 实施结果（2026-08-24）

- 步骤 1 已完成：`InspectionPage` 直接更新普通运行状态，空的 Callback 结构及 MainWindow 转发工厂已删除。
- 步骤 2 已完成：四种模板模式统一在 `loadTemplateAtIndex()` 实际显示非空原图后显示来源；无模板、加载失败、无原图和纸巾清理路径统一隐藏。
- 步骤 3 已完成：实时取景改用带 `Previewing/Frozen` 门禁的 `showTemplateCaptureStatus()`，冻结后的强类型绘图向导保持原路径。
- 步骤 3.1 已完成：根据用户验证反馈，在统一操作状态入口补充检测中显示和停止/故障隐藏。
- 步骤 4 的静态收口已完成；未执行 Qt 构建、主程序运行、真实相机、PLC 或现场验证。

## 11. 静态门禁

- `updateImageDisplayStatusText` 在生产代码中零引用。
- `InspectionPage::Callbacks::updateImageDisplayStatus` 零引用。
- MainWindow 不再把检测图像状态转发给 `TemplateEditorPage`。
- “正在显示相机采集图像...”只由检测页写普通运行状态。
- “正在显示模板【…】的产品图像”只由模板页的公共加载成功链生成。
- 模板来源不按四种模式复制实现。
- `showTemplateCaptureStatus()` 有明确的 Previewing/Frozen 门禁。
- `Detecting` 只在统一 `applyOperationState()` 入口调用一次 `showInspectionStatus()`；文案“正在检测中...”在生产代码中唯一。
- `showInspectionStatus()`、`showTemplateImageSource()`、`showTemplateCaptureStatus()` 和 `updateTemplateGuideText()` 均不写 `label_runtimeStatus`。
- 启动、停止、故障和关闭相机等运行入口不重复增加 `hideTemplateGuide()`；继续复用现有 `cancelTemplateDrawing()`。
- 纸巾模式、无模板和加载失败路径隐藏提示栏。
- `main_window.ui`、QSS、`ImageLabel`、OperationUiPolicy、Application、Templates、Detection、Runtime、Devices、PLC 和 qmake 清单差异为零。
- 没有新增生产代码文件、类、状态机、持久化字段或资源文件。
- UTF-8 中文、文件末尾换行和 `git diff --check` 通过。
- 只把静态检查报告为静态证据，不代替构建和人工验证。

### 11.1 静态证据（2026-08-24）

- `updateImageDisplayStatusText`、`InspectionPage::Callbacks`、`updateImageDisplayStatus` 和 `inspectionPageCallbacks` 在生产代码中均为零引用。
- “正在显示相机采集图像...”在生产代码中仅剩 `InspectionPage::present()` 一处；模板来源文案仅剩 `TemplateEditorPage` 一处。
- `showTemplateImageSource()` 只有公共模板加载成功链的一处运行调用；`showTemplateCaptureStatus()` 只有实时取景的一处运行调用并包含 `Previewing/Frozen` 门禁。
- `showInspectionStatus()` 只有统一操作状态入口的一处运行调用，停止中（包括故障自动停止）复用 `cancelTemplateDrawing()` 隐藏。
- `TemplateEditorPage` 已不再持有或访问 `label_runtimeStatus`，四个显示函数均只操作现有图像上下文提示栏。
- 生产代码差异严格限定为第 7 节所列七个 C++ 文件；预计不修改模块差异为零，没有新增生产文件、类、状态、持久化字段或资源。
- 严格 UTF-8 解码、文件末尾换行和 `git diff --check` 已通过。

## 12. 用户统一验证清单

用户在 Qt Creator 完成 Run qmake、Clean、Rebuild 和主程序运行后，逐项验证：

1. 刚印模式启动恢复后，模板原图与“模板【名称】”一致。
2. 深度 OCR 模式启动恢复后，模板原图与来源名称一致。
3. 字库模式切换每个当前模板，图片和来源名称同步更新。
4. 二维码＋三期模式切换每个当前模板，图片和来源名称同步更新。
5. 四种模板模式相互切换时不残留上一模式的图片来源。
6. 模板选择窗口取消后保持原模板图片和来源；确认后显示最终当前模板来源。
7. 保存新模板或覆盖模板后，重新加载的原图和来源名称正确。
8. 移除当前模板后显示新的当前模板来源；移除最后一个模板后来源提示隐藏。
9. 模板损坏、加载失败或缺少原图时，不显示虚假来源，不残留上一模板名称。
10. 纸巾模式提示栏隐藏。
11. 启动软触发识别后立即显示“【当前状态】正在检测中...”，识别途中不出现“模板制作”。
12. 启动硬触发识别后、等待和处理实际触发帧期间均显示“【当前状态】正在检测中...”，不出现“模板制作”。
13. 停止识别后若仍显示最后一帧相机图像，不显示模板来源或“正在检测中...”。
14. 点击“制作模板”后，实时取景标题和说明正确。
15. 冻结后逐项检查字库/OCR 两步、二维码三步和钢印四步向导。
16. Esc、重新取景、退出模板制作、关闭相机和模式切换均不残留错误标题。
17. 正式识别结果、当前模板名称、识别文字、统计、耗时、PLC 和存图行为不变。

## 13. 预期代码量与提交边界

- 不新增生产文件。
- 删除检测页到模板页的 Callback、MainWindow 转发和含混通用函数。
- 新增三个页面私有小函数和各自唯一的强类型状态/公共加载入口调用。
- C++ 生产代码只增加固定检测状态显示所需的小函数和强类型状态分支；不新增状态字段或通用框架。
- 文档行数增加不计入生产代码复杂度。
- 实施、验证和提交必须与其他计划差异分开审查；未经用户授权不提交。

## 14. 完成标准

同时满足以下条件后，本文整体才可标记完成：

1. 四种模板模式只要实际显示已加载模板原图，就显示正确模板来源。
2. 正式识别显示“【当前状态】正在检测中...”，停止、故障和纸巾模式不残留该状态或模板制作标题。
3. 模板制作实时取景和绘图向导保持正确。
4. 无模板、模板失败和来源切换不出现陈旧或虚假文字。
5. 全部静态门禁通过。
6. 用户完成 Qt Creator 构建和第 12 节人工验证并明确确认通过。
