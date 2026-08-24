# OCRGangYin 图像与检测信息生命周期绑定修复方案

## 1. 状态与基线

- 状态：已完成，用户统一验证通过。
- 代码基线：`73de80c`（`fix: 绑定检测覆盖层与图像生命周期`）。
- 本方案只处理主图像与右侧当前检测信息不同步的问题。
- 生产代码已按第 7 节六个文件完成修改，静态门禁已通过。
- Agent 按项目构建纪律未执行 qmake、构建和主程序；用户已完成统一验证并确认无问题。

## 2. 问题与根因

正式检测结果通过：

```text
InspectionRuntime::presentationReady
    -> InspectionPage::present(InspectionPresentation)
```

这条路径会在一次调用中更新检测图片、判定结果、识别内容、当前模板名称、统计和耗时。

模板原图、实时预览和冻结图像通过：

```text
TemplateEditorPage::displayPreviewFrame
    -> MainWindow::slot_displayAndDetect
    -> InspectionPage::presentPreviewImage(QImage)
```

`presentPreviewImage()` 目前只替换主图像，没有清理上一张检测图片的判定、识别内容、
耗时和当前模板名称，因此界面会出现“新图片 + 旧检测信息”。

当前模板名称还有第二个写入者：`TemplateEditorPage`。模板图片显示后，
`loadTemplateAtIndex()` 会再次通过 `updateCurrentTemplateName()` 把编辑模板名称写入右侧
“当前使用产品模板文件夹名称”。因此只在预览入口清空该控件不能形成稳定结果。

此外，关闭相机和硬触发启动流程仍直接调用 `imageLabel_inspection->clear()`，没有统一经过
图片呈现边界，也可能留下空图片和旧检测信息。

## 3. 状态归属

### 3.1 与当前显示图片绑定

以下字段必须与主图像同时更新或同时清空：

1. 判定结果及其样式。
2. 识别内容。
3. 检测耗时。
4. 当前使用产品模板文件夹名称。

### 3.2 预览图片刷新时不清空

以下字段不属于单张检测图片，模板原图和相机预览刷新时保持现状：

1. 运行状态。
2. 识别总数。
3. NG 数。
4. 合格率。
5. 模板编辑区的当前编辑模板下拉框。
6. 图像区中的模板来源提示和模板制作向导。

关闭相机和硬触发启动属于完整检测数据重置，会清空图片、识别总数、NG 数和合格率；
运行状态仍由原有操作状态流程单独设置。

## 4. 简洁实现原则

1. `InspectionPage` 是主图像及右侧图片级检测信息的唯一 UI 所有者。
2. `TemplateEditorPage` 不再持有或写入 `lineEdit_currentTemplateName`。
3. 检测图片继续使用现有 `InspectionPresentation`，不新增 DTO 或图片序号。
4. 只增加一个包含两个固定值的清理范围枚举，不使用多个布尔参数、位标志或任意字段组合。
5. 非检测图片继续使用现有 `presentPreviewImage()`，由该入口同步清空图片级检测信息。
6. 所有检测数据清理统一走一个参数化方法，不增加事件总线、回调或跨页状态同步。
7. 不增加兜底状态、重复校验、恢复分支或“以防万一”的防御性代码。
8. 不修改检测算法、统计、存图、PLC、模板数据和线程模型。

## 5. 计划修改

### 5.1 `InspectionPage` 统一图片与信息生命周期

文件：

- `app/ui/pages/inspection_page.h`
- `app/ui/pages/inspection_page.cpp`

修改：

1. 增加两个固定语义的清理范围：

   ```cpp
   enum class InspectionClearScope
   {
       ImageMetadata,
       AllDetectionData
   };
   ```

2. 用一个方法统一清理：

   ```cpp
   void clearInspectionView(InspectionClearScope scope);
   ```

3. 两个范围的固定含义：
   - `ImageMetadata`：清空判定结果及样式、识别内容、检测耗时和当前模板名称；
   - `AllDetectionData`：清空上述内容，并清空主图像、识别总数、NG 数和合格率。
4. 删除现有 `clearResultView()`；不再新增原计划中的 `clearPresentedImage()`。
5. `presentPreviewImage()` 在成功替换非空图片时调用
   `clearInspectionView(InspectionClearScope::ImageMetadata)`。
6. `present()` 每次都写入当前模板名称：
   - `updatesTemplateName == true` 时显示本次名称；
   - `updatesTemplateName == false` 时清空，禁止沿用上一张检测图片的名称。

不改变检测结果统计计算、运行状态和故障呈现。

### 5.2 删除 `TemplateEditorPage` 对右侧模板名称的写入

文件：

- `app/ui/pages/template_editor_page.h`
- `app/ui/pages/template_editor_page.cpp`

删除：

1. `TemplateEditorViewBindings::lineEdit_currentTemplateName`。
2. `updateCurrentTemplateName()`。
3. 无调用者的 `setCurrentTemplateNameVisible()`。
4. `m_currentTemplateNameVisible`。
5. `m_currentTemplateDisplayName`。
6. 模板加载、失败和清理流程中只为右侧名称控件服务的赋值与调用。

模板名称仍由以下现有位置明确显示：

- 当前编辑模板下拉框；
- 图像区 `【当前图像】正在显示模板【名称】的产品图像。` 提示。

不新增替代字段或通知回调。

### 5.3 删除 MainWindow 的旁路写入

文件：

- `app/ui/main_window.cpp`
- `app/ui/main_window_inspection.cpp`

修改：

1. `templateEditorViewBindings()` 不再向模板编辑页传入右侧模板名称控件。
2. 删除 MainWindow 对 `TemplateEditorPage::updateCurrentTemplateName()` 的调用。
3. 关闭相机和硬触发启动时，删除逐控件 `clear()`，统一调用
   `clearInspectionView(InspectionClearScope::AllDetectionData)`。
4. 故障结果清理和故障恢复继续只清图片级检测信息，改为调用
   `clearInspectionView(InspectionClearScope::ImageMetadata)`。
5. 完整清理补齐现有遗漏的合格率和当前模板名称，不改变统计计算和重置业务。

### 5.4 `clearTransientView()` 保持独立

`clearTransientView()` 不是检测数据清理方法。它只在恢复正常故障界面时执行：

1. 将 `m_detectionRoiWarningActive` 复位为 `false`；
2. 将 `m_imageSaveWarningScheduled` 复位为 `false`；
3. 清空 `label_runtimeStatus` 的临时运行状态文字。

它不清理主图像、判定、识别内容、耗时、当前模板名称或生产统计。本方案保留该方法，
不把它合并进 `clearInspectionView()`，避免把运行告警状态和检测数据混成一个清理概念。

## 6. 最终调用关系

### 6.1 正式检测图片

```text
InspectionPresentation
    -> InspectionPage::present()
        -> 同次显示图片
        -> 同次写入判定、识别内容、耗时、当前模板名称
        -> 更新累计统计
```

### 6.2 模板原图、相机预览和冻结图片

```text
QImage
    -> InspectionPage::presentPreviewImage()
        -> 同次显示图片
        -> clearInspectionView(ImageMetadata)
        -> 清空判定、识别内容、耗时、当前模板名称
        -> 保留运行状态和累计统计
```

### 6.3 关闭相机和硬触发启动

```text
InspectionPage::clearInspectionView(AllDetectionData)
    -> 清空主图像、判定、识别内容、耗时、当前模板名称
    -> 清空识别总数、NG 数和合格率
    -> 不清空运行状态
```

## 7. 文件范围

只修改以下六个生产文件：

1. `app/ui/pages/inspection_page.h`
2. `app/ui/pages/inspection_page.cpp`
3. `app/ui/pages/template_editor_page.h`
4. `app/ui/pages/template_editor_page.cpp`
5. `app/ui/main_window.cpp`
6. `app/ui/main_window_inspection.cpp`

同步更新本方案状态和计划索引。生产代码净减少，不新增生产文件。

## 8. 禁止事项

1. 不新增图片 ID、版本号、generation token 或缓存状态。
2. 不新增通用清屏事件、信号、回调或消息总线。
3. 不使用多个布尔参数、`QFlags`、选项结构或控件列表表达清理范围。
4. 不为尚不存在的清理组合预留枚举值。
5. 不让 `TemplateEditorPage` 反向调用 `InspectionPage`。
6. 不在多个模板模式中分别复制清理代码。
7. 模板原图和相机预览刷新时不清空识别总数、NG 数和合格率。
8. 不把 `clearTransientView()` 合并进检测数据清理。
9. 不修改当前模板选择、模板加载、检测模板匹配和最高分选择逻辑。
10. 不顺带修改 UI 布局、QSS、日志、存图、PLC 或算法代码。

## 9. 实施步骤

1. 先删除 `TemplateEditorPage` 对右侧模板名称的绑定、状态和调用。
2. 在 `InspectionPage` 增加两值清理范围和唯一 `clearInspectionView()` 方法。
3. 让检测呈现和预览呈现使用统一的图片级信息边界。
4. 替换 MainWindow 中故障清理、关闭相机和硬触发启动的分散清理代码。
5. 执行静态零引用、差异和构建前门禁。
6. 由用户在 Qt Creator 完整构建并统一交互验证。

## 10. 静态门禁

1. `TemplateEditorPage` 中不存在 `lineEdit_currentTemplateName`、
   `updateCurrentTemplateName`、`setCurrentTemplateNameVisible`、
   `m_currentTemplateNameVisible` 和 `m_currentTemplateDisplayName`。
2. MainWindow 不直接调用 `ui->imageLabel_inspection->clear()`。
3. MainWindow 不再逐个清理判定、识别内容、耗时、模板名称和统计控件。
4. `InspectionClearScope` 只有 `ImageMetadata` 和 `AllDetectionData` 两个值。
5. `clearInspectionView()` 是唯一检测数据清理方法。
6. `presentPreviewImage()` 调用 `ImageMetadata`，并保留累计统计。
7. 关闭相机和硬触发启动调用 `AllDetectionData`，并补齐合格率和当前模板名称。
8. `InspectionPage::present()` 不允许模板名称沿用上一张检测图片。
9. `clearTransientView()` 保持独立且行为不变。
10. 生产代码修改严格限制在第 7 节六个文件。
11. `git diff --check` 通过。

## 11. 统一验证

1. 完成一次有判定、识别内容、耗时和模板名称的检测。
2. 切换模板，确认模板原图显示后以下内容立即清空：
   - 判定结果；
   - 识别内容；
   - 检测耗时；
   - 当前使用产品模板文件夹名称。
3. 确认识别总数、NG 数、合格率和运行状态没有被图片刷新清空。
4. 启动模板实时预览并冻结图像，确认四项图片级检测信息保持为空。
5. 关闭相机，确认图片、四项图片级检测信息、识别总数、NG 数和合格率一起清空。
6. 启动硬触发识别，确认等待新图期间图片、四项图片级检测信息和三项统计均为空。
7. 再完成一次检测，确认新图片与新判定、识别内容、耗时和模板名称同时出现。
8. 抽查钢印、OCR、二维码和纸巾模式；纸巾模式的模板名称应为空，不沿用其他模式名称。
9. 制造并恢复一次可控故障，确认临时运行警告被清除，检测数据清理范围没有被扩大。

## 12. 实施结果

1. `InspectionPage` 已增加 `InspectionClearScope` 和唯一的
   `clearInspectionView()`，图片级信息与完整检测数据按两个固定范围清理。
2. `presentPreviewImage()` 已在显示模板原图、相机预览或冻结图片时同步清空判定、
   识别内容、耗时和当前模板名称，并保留生产统计。
3. `present()` 已在每次正式检测呈现时明确写入或清空本次模板名称，不再沿用旧值。
4. `TemplateEditorPage` 对右侧模板名称的绑定、状态、方法和调用已全部删除。
5. 关闭相机和硬触发启动已统一执行完整清理，补齐合格率和当前模板名称。
6. 故障结果清理和恢复已改用图片级信息范围；`clearTransientView()` 保持独立且未修改。
7. 六个生产文件合计新增 32 行、删除 62 行，生产代码净减少 30 行。
8. 旧符号、模板编辑页名称绑定、MainWindow 直接清图和逐控件清理均为零引用；
   `git diff --check` 通过。
9. 2026-08-25 用户反馈统一验证无问题，本方案完成。
