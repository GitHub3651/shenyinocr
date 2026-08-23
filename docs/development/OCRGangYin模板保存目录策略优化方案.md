# OCRGangYin 模板保存目录策略优化方案

## 1. 文档状态

- 方案日期：2026-08-23。
- 当前状态：代码实施和静态门禁已完成，待用户统一构建与人工验证。
- 权威范围：新模板保存根目录的界面入口、持久化、启动恢复、新模板保存流程，以及字符模板只在已框选日期/文字检测区域内分割的交互。
- 当前进度：设置底座、保存弹窗与目录持久化、局部字符分割、保存后询问和最终静态收口均已实施；第一轮验证发现并修复了字符文件名被重复追加 `.png`、继而无法匹配目标字符的问题；未新增生产代码文件、设置对象或模板字段，模板 Schema、`.pro`、`main_window.ui`、Runtime 和 Detection 均未修改；`git diff --check` 已通过，其余人工验证待用户继续统一执行。
- 替代关系：本方案不替代《OCRGangYin 模板方案完全替换计划》，只在其“外部模板文件夹 + 唯一 `app_settings.json`”终局上增加一个模板编辑偏好。
- 实施门禁：用户已明确授权一次性实施；代码阶段已经完成，当前只等待统一构建和人工验证，不将静态检查表述为编译或运行通过。

## 2. 目标

恢复提交 `1c8d564fe42ce5717b7606513cc2b426a367ff4b` 对应源码快照中已经存在过的用户能力，但不复制旧版 `Widget` 内的实现结构：

1. 用户在“保存模板”弹窗中同时填写模板名称并选择模板保存根目录。
2. 目录输入框只读，只能通过“浏览”按钮选择已存在的目录。
3. 只有模板文件成功保存后，所用根目录才写入现有唯一配置文件 `app_settings.json`，软件下次启动时自动恢复。
4. 新模板按“保存根目录/模板名称”建立独立文件夹。
5. 保存模板弹窗同时显示模板名称和保存目录，并保留同名覆盖确认。
6. 修改保存根目录只影响以后新建的模板，不移动、不修改已有模板。
7. 编辑已有模板时仍保存回原模板目录，不受新模板保存根目录影响。
8. 刚印、字库和二维码+三期的字符模板只在已经框选的日期/文字检测区域局部图上分割，不再显示整张 `template_raw.png`。
9. `characterSourceSize` 和 `characterBoxes` 统一相对于该局部图保存，不再转换成完整原图坐标。
10. 钢印、字库和二维码+三期保存成功后，立即询问是否切割字符模板；深度 OCR 不询问。

本方案恢复旧版“保存窗口同时填写名称和目录”的用户功能，但继续使用当前 `TemplateApplicationService`、`TemplateStore` 和 `AppSettingsStore`，不复制旧版 `Widget` 内部实现。

## 3. 当前实现与问题

当前新建模板流程位于 `TemplateEditorPage::saveCurrentTemplate()`：

```text
点击保存模板
  → 检查模板绘制和参数
  → QFileDialog::getExistingDirectory() 选择父目录
  → QInputDialog 输入模板名称
  → 父目录/模板名称
  → TemplateApplicationService::save()
  → TemplateStore::save()
  → SettingsApplicationService::saveTemplatePaths()
```

当前问题：

- 每次保存新模板都必须重新选择父目录。
- 保存根目录没有进入 `AppSettings`，重启后无法恢复。
- 当前暂存差异只把该文件夹选择器改为 Qt 非原生窗口，没有解决目录记忆。
- 当前“分割字符模板”把完整 `EditableTemplate::rawImage` 直接传给 `CharacterTemplateEditorDialog`，实际在整张 `template_raw.png` 上框字符，和界面提示的“喷码区域图像”不一致。
- 当前新版代码因此把 `characterSourceSize` 和 `characterBoxes` 保存成完整原图尺寸与坐标；这不是检测算法的必要条件，也与字符窗口只显示日期/文字局部图的目标不一致。

当前 `AppSettings::detectionSchemes` 保存的是各检测模式已经选择的模板路径，不是新模板的保存根目录。不得把两个概念合并。

## 4. 最终用户交互

点击现有“保存模板”按钮后，打开一个简单模态弹窗：

```text
保存模板

产品模板文件夹名称：[                         ]
模板文件夹保存目录：[D:\ProductTemplates      ] [浏览]

                                      [确定] [取消]
```

其中：

- 名称输入框允许输入并执行基本文件夹名称校验。
- 目录输入框只读，禁止键盘输入和粘贴路径。
- “浏览”是改变本次保存目录的唯一入口，打开 Qt 自己的文件夹选择窗口，只允许选择已存在目录。
- “确定”使用“目录/模板名称”作为目标；名称为空时按钮不可用。
- “取消”不保存模板，也不修改已经记住的目录。
- 不在模板制作页面增加固定目录显示框或独立“选择目录”按钮。

### 4.1 弹窗默认目录

每次打开保存弹窗时按固定顺序确定目录输入框初值和浏览起点：

```text
已保存的 templateSaveDirectory 当前存在
  → 使用该目录

否则，Windows 桌面目录当前存在
  → 使用桌面目录

否则
  → 使用用户主目录
```

无效的已保存目录不自动写回、不自动清空。只有本次模板最终保存成功后，实际使用的有效目录才覆盖配置。

### 4.2 保存与持久化时机

```text
打开“保存模板”弹窗
  → 填写名称
  → 必要时点击“浏览”选择目录
  → 确认保存
  → 模板文件完整保存成功
  → 将实际使用的根目录和当前模式模板路径一起写入 app_settings.json
```

以下情况均不得修改 `templateSaveDirectory`：

- 只打开弹窗后取消。
- 浏览选择目录后取消。
- 名称或目标路径校验失败。
- 用户取消同名覆盖。
- 模板文件保存失败。
- 用户通过模板选择窗口加载已有模板。

### 4.3 后续启动与改变目录

启动时由 `AppSettingsStore` 读取 `templateSaveDirectory`。下次打开保存弹窗时，如果该目录仍存在，就作为默认目录显示。

用户通过保存弹窗的“浏览”按钮选择另一个目录，并成功保存一个模板后，新目录才成为以后保存弹窗的默认目录。修改目录不移动、修改、扫描或整理任何已有模板。

## 5. 数据边界

### 5.1 两种路径必须分开

```text
AppSettings::templateSaveDirectory
  = 用户新建模板时使用的保存根目录

AppSettings::detectionSchemes.*TemplatePath(s)
  = 当前检测模式选择并用于编辑、启动检测的模板文件夹路径
```

`templateSaveDirectory` 不是：

- 模板库根目录；
- 模板扫描目录；
- 当前模板路径；
- 当前命中模板路径；
- 按检测模式分别维护的目录；
- 对外部模板位置的限制。

用户仍然可以通过现有模板选择对话框加入位于任意外部位置的模板文件夹。

### 5.2 C++ 字段

在现有 `AppSettings` 中直接增加：

```cpp
QString templateSaveDirectory;
```

不增加 `TemplateDirectorySettings`、Map、DTO、Manager、Repository 或第二个设置对象。

默认值为空字符串，表示用户尚未配置保存目录。

### 5.3 JSON 字段

字段放入现有 `ui` 分区：

```json
{
  "schemaVersion": 2,
  "ui": {
    "selectedDetectionMode": "word",
    "rightPanelSplitterStateBase64": "",
    "templateSaveDirectory": "D:/ProductTemplates"
  }
}
```

选择 `ui` 分区的原因：它是模板编辑界面的使用偏好，不是某一种检测模式的运行配置，也不是模板文件夹内部参数。

### 5.4 Schema 兼容规则

本次不提高 `AppSettings::CurrentSchemaVersion`，继续使用 Schema 2：

- 已有 Schema 2 配置没有 `ui.templateSaveDirectory` 时，读取为空字符串。
- 字段存在时必须是字符串，否则按现有严格设置错误返回。
- 保存设置时始终写出该字段。
- `ui` 分区继续拒绝其他未知字段。
- 不增加 Schema 迁移器、双读取路径或旧配置备份框架。

这样可以避免为了一个新增目录偏好要求用户清空全部现有整机设置和检测方案。

### 5.5 路径校验

配置层只检查：

- 空字符串允许存在，表示尚未配置。
- 非空值必须是规范化绝对路径。

配置加载时不要求目录当前存在。原因是外接磁盘、网络盘或临时不可用目录不应导致整份 `app_settings.json` 加载失败。

真正保存新模板时再检查：

- 路径非空；
- 目录当前存在；
- 路径确实是目录。

保存弹窗不得提供可编辑目录文本框，不接受手动输入不存在的根目录，也不负责创建根目录。已保存目录不存在时，仅把桌面目录或用户主目录作为本次弹窗默认值；模板名称对应的最后一级子目录仍由现有 `TemplateStore` 保存流程创建。

## 6. 设置保存设计

### 6.1 模板保存后的唯一设置接口

在现有 `SettingsApplicationService` 中增加一个直接方法：

```cpp
OperationResult saveTemplatePathsAndDirectory(
    DetectionMode mode,
    const QStringList &paths,
    const QString &directoryPath);
```

它只在 `TemplateApplicationService::save()` 已经成功后调用，一次完成两项设置更新：

```text
从 m_current 复制 AppSettings candidate
  → 设置当前模式模板路径
  → 设置 candidate.templateSaveDirectory
  → 复用 saveCandidate() 和 AppSettingsStore::save()
  → 一次写入 app_settings.json
  → 成功后同步 m_current 和 m_draft 中这两个分区字段
```

这样不会为了一个保存操作连续写两次设置文件，也不会出现目录已记住但新模板路径没有应用的中间状态。不新增 `TemplateDirectoryService`，也不让 `TemplateApplicationService` 保存整机设置。

### 6.2 与整机草稿的关系

必须调整现有分区保存语义：

- `applyDraft()` 应保留 `m_current.templateSaveDirectory`，不能被较早创建的整机草稿覆盖。
- `saveTemplatePathsAndDirectory()` 保存成功后同步更新 `m_draft.detectionSchemes` 和 `m_draft.templateSaveDirectory`，但不提交其他尚未应用的整机草稿字段。
- 现有 `saveTemplatePaths()` 继续用于选择、移除已有模板，只修改模板路径，绝不修改 `templateSaveDirectory`。
- `saveTissueThreshold()` 继续保持现有行为，同时不得丢失模板保存目录。
- `restoreDefaults()` 保留检测方案，但把模板保存目录恢复为空字符串。
- `clearSettings()` 使用完整默认值，因此清空模板保存目录，但绝不删除目录中的模板。

设置保存失败时：

- `m_current` 保持上一正式值。
- `m_draft` 保持原值。
- 新模板文件已经成功写入外部目录，但不会加入当前检测方案，也不会记住本次目录。
- 不删除或回滚已经成功保存的外部模板，向用户明确提示“模板已保存但未应用，保存目录也未记住”。

## 7. UI 与 Page 设计

### 7.1 不增加页面固定控件

本次不修改 `main_window.ui`，不增加页面目录显示框、独立“选择目录”按钮或新的 `TemplateEditorViewBindings`。现有“保存模板”按钮仍是唯一入口，继续服从现有 `snapshot.saveTemplate` 操作权限。

### 7.2 保存弹窗

`TemplateEditorPage::saveCurrentTemplate()` 完成现有保存前检查后，直接构造一个局部 `QDialog`。弹窗内只包含：

- 模板名称 `QLineEdit`。
- 只读保存目录 `QLineEdit`。
- “浏览”“确定”“取消”三个按钮。
- 当前模式需要的简短保存提示。

名称和目录只作为本次函数内的局部值使用，不增加 Page 成员、弹窗 DTO、Builder、Factory 或新的对话框代码文件。名称为空时禁用“确定”；目录框设置 `readOnly`，不接受用户键盘输入。

### 7.3 文件夹选择窗口

保存弹窗的“浏览”按钮使用：

```cpp
QFileDialog::getExistingDirectory(
    dialogParent(),
    QStringLiteral("选择模板保存目录"),
    startDirectory,
    QFileDialog::ShowDirsOnly
        | QFileDialog::DontUseNativeDialog);
```

起始目录规则：

- 从保存弹窗当前显示的有效目录打开。
- 只允许选择已存在文件夹。
- 用户取消文件夹窗口时，保存弹窗中的目录保持不变。
- 选择成功只更新本次弹窗的只读目录框，不立即写配置。

当前已经暂存于 `template_editor_page.cpp` 的 `DontUseNativeDialog` 行为保留在该“浏览”调用中，继续使用 Qt 自己的中文文件夹窗口。

### 7.4 操作状态

不增加新的 `OperationUiPolicy` 字段。只有现有“保存模板”按钮可用并通过保存前检查时才创建弹窗；检测中、停止中、Fault 或实时模板预览中沿用现有禁止保存行为。

## 8. 新模板保存流程

### 8.1 保存前保持现有检查

继续保持当前已经验证的检查和行为：

- 当前检测模式有效且不是纸巾模式。
- 模板画面已经冻结。
- 当前模式全部框选步骤已经完成。
- 二维码区域即时验证通过。
- 图像阈值、目标文字和模式几何转换保持现有语义。
- 钢印、二维码、日期和定位锚点的几何与资源关系不改变。

本方案只替换“目标目录从哪里取得”，不修改算法、ROI、模板字段或资源内容。

### 8.2 新流程

```text
保存模板
  → 按“已保存目录 → 桌面 → 用户主目录”计算默认目录
  → 打开同时包含名称和只读目录的保存弹窗
  → 用户可通过“浏览”选择另一个已存在目录
  → 校验模板名称和目录
  → target = 本次弹窗目录/模板名称
  → 同名时询问是否覆盖
  → TemplateApplicationService::beginNew()
  → 组装现有模板设置和资源
  → TemplateApplicationService::save(target, false, ...)
  → 计算保存后的当前模式模板路径
  → SettingsApplicationService::saveTemplatePathsAndDirectory()
  → 刷新当前编辑模板
  → 钢印、字库、二维码+三期询问是否立即切割字符模板
```

如果弹窗当前目录在用户确认前变得无效，固定提示：

```text
当前模板保存目录不存在，请重新选择。
```

提示后返回保存弹窗，用户只能通过“浏览”重新选择，不允许直接输入或自动创建根目录。

### 8.3 模板名称

名称输入框直接放在本次保存弹窗中，与只读目录框同时显示。

名称只做当前 Windows 文件夹保存必须的基本检查：

- 去除首尾空白后不能为空。
- 不能是 `.` 或 `..`。
- 不能包含 `\ / : * ? " < > |`。

不新增命名策略类、自动编号、产品编号生成器或重名重命名规则。

### 8.4 同名覆盖

目标目录已经存在时继续保留现有覆盖确认：

```text
模板“名称”已存在，是否完全覆盖？
确认后原模板文件夹中的全部内容都会被替换。
```

- 用户取消时不调用保存，原目录保持不变。
- 用户确认后继续调用现有唯一 `TemplateApplicationService::save()` 和 `TemplateStore::save()`。
- 继续使用 `TemplateStore` 已有临时目录、校验、备份替换和失败恢复能力。
- 不恢复旧版直接 `removeRecursively()` 后再写入的实现。

### 8.5 保存后的检测方案

保持当前行为：

- 刚印和深度 OCR：新模板替换当前模式唯一模板路径。
- 字库和二维码+三期：新模板追加到当前路径列表末尾，重复路径不再次加入。
- `saveTemplatePathsAndDirectory()` 失败时保留已经成功写入的外部模板，但不改变当前检测方案和已记住目录，并提示“模板已保存但未应用，保存目录也未记住”。
- 新模板成功加入检测方案后，成为当前编辑模板。

### 8.6 保存后立即切割字符模板

模板文件和应用设置均保存成功后，按模式执行：

- 钢印：询问“产品模板已保存成功，是否立即切割字符模板？”。
- 字库：询问同一问题。
- 二维码+三期：询问同一问题。
- 深度 OCR：不询问。
- 纸巾：不进入模板保存流程。

用户选择“确定”时直接调用现有 `showManualCharacterTemplateEditorDialog()`；选择“取消”时结束保存流程，已经保存的模板和设置保持不变。不新增后续任务队列、向导状态或字符切割调度器。

## 9. 编辑已有模板

现有 `saveCurrentDraft()` 不读取 `templateSaveDirectory`，继续保存到：

```cpp
m_templateService->currentDirectoryPath()
```

固定规则：

- 编辑已有模板永远原位保存。
- 改变新模板保存根目录不迁移当前模板。
- 选择、移除或原位保存已有模板都不修改 `templateSaveDirectory`。
- 不提供“另存为”、模板移动或目录重组功能。
- 不因当前模板位于保存根目录之外而提示或拒绝。

### 9.1 字符模板分割区域

#### 目标行为

刚印、字库和二维码+三期使用字符模板。点击“分割字符模板”时，字符编辑窗口必须显示已经框选的日期/文字检测区域局部图，不再显示完整 `template_raw.png`。

深度 OCR 不使用字符模板，纸巾检测不使用模板，两种模式不进入该流程。

#### 区域来源

继续使用模板中已经存在的：

```text
trackingRoi
datePolygon
template_raw.png
```

当前 `datePolygon` 保存的是相对 `trackingRoi` 中心的原图像素坐标。打开字符编辑器时直接在 `TemplateEditorPage::showManualCharacterTemplateEditorDialog()` 中完成以下转换：

```text
trackingCenter = trackingRoi.center()
absoluteDatePolygon = datePolygon 每个点 + trackingCenter
characterRegionRect = absoluteDatePolygon 的最小外接矩形
characterRegionRect 与 template_raw.png 边界相交
characterRegionImage = template_raw.png.copy(characterRegionRect)
```

字符编辑窗口只接收 `characterRegionImage`。

为了保持实现简单，本次固定使用日期/文字多边形的最小外接矩形作为字符分割画布，不增加像素级多边形遮罩、自由形状裁剪控件或第二套 ROI 编辑器。该局部图只覆盖用户已经框选的日期/文字区域附近，不再展示完整产品画面。

如果 `trackingRoi`、`datePolygon` 或转换后的外接矩形无效，直接提示：

```text
当前模板的日期/文字检测区域无效，请重新制作或编辑模板。
```

不得回退到完整 `template_raw.png`，避免用户误在整张图上继续分割字符。

#### 统一使用局部坐标

`CharacterTemplateEditorDialog` 继续按传入图像坐标工作，不修改其保存职责。传入图像改成 `characterRegionImage` 后，窗口产生的数据直接作为正式字符配置保存：

```text
characterSourceSize
  = characterRegionImage.size()

characterBoxes
  = 相对于 characterRegionImage 左上角 (0, 0) 的矩形
```

字符窗口确认后不再给字符框增加 `characterRegionRect.topLeft()`，也不把 `characterSourceSize` 恢复成完整原图尺寸。`character_templates/*.png` 继续由字符窗口从同一张局部图中直接裁剪，因此字符框、来源尺寸和字符图片始终属于同一坐标系。

该坐标合同固定为：

- `template_settings.json` 中的 `characterSourceSize` 是日期/文字区域最小外接矩形裁剪图的尺寸。
- `template_settings.json` 中的 `characterBoxes` 是相对于该裁剪图左上角的局部坐标。
- 完整原图中的对应位置只在需要解释时通过 `characterRegionRect.topLeft() + characterBox.topLeft()` 计算，不写入字符配置。
- Runtime 当前只使用已经生成的字符模板图片，不依赖 `characterBoxes` 返回完整原图定位。
- 不增加字符区域原点、坐标标志、DTO 或第二套字段。

#### 不保留旧坐标兼容

实施后只认上述局部坐标合同：

- 不根据 `characterSourceSize` 猜测字符框属于原图还是局部图。
- 读取 `characterBoxes` 时始终按局部坐标解释，不识别、转换或迁移当前新版曾经保存的完整原图坐标。
- 不增加兼容分支、旧格式检测、版本判断或自动修复。
- 已有原图坐标字符配置不在本方案支持范围内，必须由用户重新打开字符分割窗口并重新框选、命名和保存。

这样生产代码始终只有一套坐标语义和一条保存路径。

#### 最小实现边界

局部字符坐标只修改 `TemplateEditorPage::showManualCharacterTemplateEditorDialog()` 附近的页面编排：

- 页面根据已有模板字段计算局部矩形。
- 页面构造局部图并把局部尺寸、局部字符框直接交给字符窗口。
- 继续复用现有 `CharacterTemplateEditorDialog`。
- 页面直接保存字符窗口返回的局部尺寸和局部字符框，不再平移回完整原图坐标。
- 继续调用现有 `stageCharacterAssets()`、`replaceDraft()` 和 `saveCurrentDraft()`。

不新增 helper 类、坐标 DTO、字符分割 Service、遮罩类或代码文件。实现保持为该页面方法中的一段线性转换代码；只有确认重复计算真实出现时才考虑匿名命名空间小函数，本方案不预先增加抽象。

第一轮验证确认现有字符文件名链存在一处独立错误：字符窗口传给 `stageCharacterAssets()` 的键已经是 `1.png`，应用服务却再次追加 `.png`，并把 `1.png` 当成目标字符名称。修复固定为原样保存窗口生成的文件名，并用 `QFileInfo::completeBaseName()` 得到用于检测匹配的 `1`；不增加双格式兼容或迁移逻辑。

## 10. 文件级修改清单

| 文件 | 计划修改 |
|---|---|
| `app/system_support/settings/app_settings.h` | 增加 `templateSaveDirectory` 字段 |
| `app/system_support/settings/app_settings.cpp` | 将字段纳入默认值和相等比较 |
| `app/system_support/settings/app_settings_store.cpp` | 在 `ui` 分区保存字段；兼容读取缺少字段的 Schema 2；校验空值或规范化绝对路径 |
| `app/application/settings_application_service.h` | 增加 `saveTemplatePathsAndDirectory()` |
| `app/application/settings_application_service.cpp` | 在模板文件保存成功后一次保存当前模式模板路径和目录；同步正式设置与草稿；恢复默认时清空目录 |
| `app/application/template_application_service.cpp` | 原样保存字符窗口已经生成的 `.png` 文件名，并从文件名提取不含扩展名的目标字符匹配名称 |
| `app/ui/pages/template_editor_page.cpp` | 在现有保存方法内构造名称和只读目录弹窗；按已保存目录、桌面、用户目录确定默认值；保存成功后更新设置并按三种字符模式询问立即切割；从 `datePolygon` 计算字符分割局部图并直接保存局部坐标 |

明确不修改：

- `TemplateStore`；
- `InspectionApplicationService`；
- Runtime；
- Detection；
- `application_startup.cpp`；
- `AutoOCRproject.pro`；
- `main_window.ui`；
- `main_window.cpp`；
- `template_editor_page.h`；
- `template_settings.json` Schema；
- 五种检测算法及几何语义；
- `CharacterTemplateEditorDialog` 的窗口结构、字符命名和图片裁剪实现；
- 模板选择对话框现有“添加外部模板文件夹”能力。

本方案不增加、删除或移动任何生产代码文件，因此 `.pro` 文件不需要变化。

## 11. 禁止的过度设计

实施中明确禁止：

- 新增 `TemplateManager`、`TemplateDirectoryManager`、`TemplateRepository` 或目录 Store。
- 新增第二个设置文件或使用 `QSettings` 平行保存目录。
- 为四种模板模式分别保存四个根目录。
- 把保存目录写入 `DetectionSchemes` 或模板文件夹自己的 `template_settings.json`。
- 在 Page 中保存第二份正式目录成员。
- 增加模板库扫描、最近目录列表、收藏目录或目录历史。
- 自动移动、复制、删除或整理已有模板。
- 自动按模式建立子目录。
- 除“已保存目录无效时使用桌面、桌面无效时使用用户主目录”外，增加其他自动回退目录。
- 允许直接输入、粘贴或自动创建模板保存根目录。
- 用户选择、移除或编辑已有模板时顺带改变模板保存目录。
- 新增保存策略类、命令类、事件总线或状态机。
- 修改 TemplateStore 已有统一安全保存实现。
- 为字符分割新增 PolygonMask、CharacterCropService、坐标 DTO、第二套字符编辑窗口或新的模板字段。
- 保存字符配置时再把局部字符框加回完整原图偏移，或者把正式 `characterSourceSize` 写成完整原图尺寸。
- 为已有原图坐标增加识别、转换、迁移、自动修复、版本字段或双写坐标。
- 除本方案明确的字符分割画布裁剪外，顺带修改模板绘图、二维码验证、字符识别规则、检测算法、Runtime、PLC、统计或存图流程。

## 12. 分阶段实施

实施按以下顺序进行。每个阶段只处理自己的文件和行为，完成本阶段静态门禁后再进入下一阶段；不得为了阶段衔接增加临时字段、兼容分支、双写逻辑或过渡类。完整构建和人工验证统一放在所有代码阶段完成之后。

### 12.1 阶段一：设置底座

目标：让现有设置体系能够保存唯一模板根目录，并提供一次保存目录和模板路径的应用接口；本阶段不改变模板页面交互。

修改文件：

- `app/system_support/settings/app_settings.h`
- `app/system_support/settings/app_settings.cpp`
- `app/system_support/settings/app_settings_store.cpp`
- `app/application/settings_application_service.h`
- `app/application/settings_application_service.cpp`

实施内容：

1. 增加 `AppSettings::templateSaveDirectory`，默认值为空。
2. 在现有 `ui` JSON 分区读取和保存该字段，不提高 Schema 版本。
3. 增加 `SettingsApplicationService::saveTemplatePathsAndDirectory()`，一次保存新模板路径和本次根目录。
4. `applyDraft()` 保留当前正式目录；普通 `saveTemplatePaths()` 不修改目录。
5. `restoreDefaults()` 和 `clearSettings()` 清空目录字段，但不删除外部模板。
6. `saveCandidate()` 保留整机草稿时，只同步检测方案和模板保存目录两个已正式保存的分区字段。

阶段门禁：

- `templateSaveDirectory` 只有一份正式字段。
- 设置文件只增加一个 JSON 字段，不增加新设置对象或文件。
- 新方法只写一次 `app_settings.json`。
- 选择已有模板所用的 `saveTemplatePaths()` 没有目录副作用。
- 本阶段不修改 `TemplateEditorPage`、`.ui`、ViewBindings、模板格式或检测代码。
- 本阶段差异执行 `git diff --check`。

### 12.2 阶段二：保存弹窗与目录持久化

目标：完成模板名称和目录同窗填写、只读目录选择、成功后记忆目录，以及保存后的模板列表更新；本阶段不改变字符框坐标。

修改文件：

- `app/ui/pages/template_editor_page.cpp`

实施内容：

1. 在现有 `saveCurrentTemplate()` 内局部构造名称、只读目录和“浏览”按钮，不增加对话框类或代码文件。
2. 按“有效已保存目录 → 有效桌面目录 → 用户主目录”设置弹窗默认目录。
3. “浏览”使用 Qt 非原生文件夹窗口，只选择已存在根目录，不允许手动输入。
4. 使用本次弹窗目录和模板名称构造目标路径，继续复用现有名称校验和安全覆盖链。
5. 只有 `TemplateApplicationService::save()` 成功后，才计算当前模式最终模板路径并调用一次 `saveTemplatePathsAndDirectory()`。
6. 取消弹窗、取消覆盖或模板保存失败时，不修改正式目录。
7. 刚印和深度 OCR 替换唯一模板；字库和二维码+三期追加且不重复。
8. 选择、移除和编辑已有模板继续走原有接口，不修改模板保存目录。

阶段门禁：

- 保存弹窗同时显示名称和目录，目录框为只读。
- 页面没有新增固定控件、ViewBindings、成员目录副本或新 UI 文件。
- `QFileDialog::getExistingDirectory()` 只由弹窗“浏览”按钮调用。
- 模板文件保存之前没有设置写入。
- 模板文件保存成功后只有一次设置写入。
- 本阶段暂不增加保存后字符切割询问，避免在局部字符画布完成前进入旧的整图分割流程。
- 本阶段差异执行 `git diff --check`。

### 12.3 阶段三：局部字符分割与保存后提示

目标：字符模板始终在日期/文字局部图上分割并保存局部坐标，随后接通三种字符模式的保存后立即切割入口。

修改文件：

- `app/ui/pages/template_editor_page.cpp`

实施内容：

1. 从 `trackingRoi.center()` 和 `datePolygon` 计算完整原图绝对多边形。
2. 取得多边形最小外接矩形并裁剪 `template_raw.png`。
3. 将局部图以及按局部坐标解释的现有字符配置直接传给 `CharacterTemplateEditorDialog`。
4. 直接保存字符窗口返回的局部 `characterSourceSize` 和 `characterBoxes`，不增加原图偏移。
5. 继续复用现有字符图片保存、`stageCharacterAssets()`、`replaceDraft()` 和 `saveCurrentDraft()`。
6. 钢印、字库和二维码+三期模板保存并应用成功后，立即询问是否切割字符模板；确认后调用现有字符编辑入口。
7. 深度 OCR 不询问，纸巾不进入模板保存和字符分割流程。

阶段门禁：

- 字符窗口不再接收完整 `value.rawImage`。
- 代码只存在局部坐标，不存在原图坐标识别、转换、迁移或双写。
- `characterSourceSize` 等于局部图尺寸，`characterBoxes` 相对于局部图左上角。
- 无效日期/文字区域直接阻止打开，不回退完整原图。
- 三种提示模式和两种不提示模式与本方案一致。
- 本阶段不修改 `CharacterTemplateEditorDialog`、`TemplateStore`、模板 Schema、Runtime 或 Detection。
- 本阶段差异执行 `git diff --check`。

### 12.4 阶段四：统一静态收口与用户验证

1. 按第 13 节逐项检查完整文件引用、设置写入、保存调用链、模式分支和局部坐标语义。
2. 对全部计划差异执行最终 `git diff --check`。
3. 核对没有新增生产代码文件，`.pro` 文件无需变化。
4. 核对实施前已有暂存修改没有被取消、覆盖或混入错误范围。
5. Agent 不把静态检查表述为编译或运行通过。
6. 所有代码阶段完成后，由用户在 Qt Creator 统一执行 Run qmake、Rebuild 和第 14 节人工验证；不要求用户在阶段一、二之间重复验证。

### 12.5 第一轮验证反馈：字符文件名修复

用户验证发现字符图片被保存为 `1.png.png`，启动检测时目标字符 `1` 无法匹配。根因是 `CharacterTemplateEditorDialog` 返回的 Map 键已经包含 `.png`，`TemplateApplicationService::stageCharacterAssets()` 又追加了一次扩展名。

本次只统一现有接口的数据含义：

1. `TemplateCharacterAsset::fileName` 直接使用窗口生成的 `1.png`。
2. `TemplateCharacterAsset::normalizedBaseName` 使用 `QFileInfo(fileName).completeBaseName()` 得到 `1`。
3. 不识别或兼容已经生成的 `.png.png` 文件；现有错误模板需要重新切割并保存一次字符模板。
4. 不修改字符命名规则、检测匹配规则、模板 Schema 或 `TemplateStore`。

## 13. 静态门禁

实施完成后必须确认：

1. `saveCurrentTemplate()` 的保存弹窗同时包含模板名称和只读保存目录。
2. `QFileDialog::getExistingDirectory()` 只由该弹窗的“浏览”按钮调用，并保留 `DontUseNativeDialog`。
3. `templateSaveDirectory` 只在 `AppSettings` 中保存一份正式值。
4. `detectionSchemes` 中没有保存目录字段。
5. `TemplateApplicationService` 和 `TemplateStore` 没有新增目录配置职责。
6. 现有 Schema 2 配置缺少新字段时可以正常加载，字段默认空。
7. 非空字段不是字符串或不是规范化绝对路径时继续返回明确设置错误。
8. `applyDraft()`、`saveTemplatePaths()` 和 `saveTissueThreshold()` 不会意外覆盖已保存目录；`restoreDefaults()` 和 `clearSettings()` 明确清空该目录字段。
9. 恢复默认和清空设置都不删除任何外部模板文件夹。
10. 选择、移除和编辑已有模板不修改 `templateSaveDirectory`；编辑已有模板仍只使用 `currentDirectoryPath()`。
11. 用户浏览目录后取消保存、取消覆盖或模板文件保存失败时，不写 `templateSaveDirectory`。
12. 新模板文件保存成功后，只调用一次 `saveTemplatePathsAndDirectory()` 同时保存本次目录和当前模式模板路径。
13. 刚印和深度 OCR 保存后替换唯一模板路径；字库和二维码+三期追加且不重复。
14. 钢印、字库和二维码+三期保存并应用成功后询问立即切割字符模板；深度 OCR 不询问。
15. 没有新增 Manager、Store、Service、Schema 迁移器或生产代码文件。
16. `main_window.ui`、ViewBindings、`.pro` 文件和模板磁盘 Schema 没有变化。
17. 当前工作区原有暂存修改没有被取消、覆盖或混入错误提交。
18. `CharacterTemplateEditorDialog` 不再直接接收完整 `value.rawImage`，而是接收从 `datePolygon` 计算出的局部图。
19. `datePolygon` 转换为绝对坐标时只增加 `trackingRoi.center()`，不改变模板几何服务或运行时坐标语义。
20. 代码不存在依据来源尺寸识别、转换或迁移原图坐标的兼容分支。
21. 字符窗口返回后不增加局部矩形左上角；正式 `characterSourceSize` 等于局部图尺寸，正式 `characterBoxes` 是局部坐标。
22. 日期/文字区域无效时阻止打开字符窗口，不回退整张原图。
23. 深度 OCR 和纸巾模式不进入字符分割流程。
24. `CharacterTemplateEditorDialog`、`TemplateApplicationService`、`TemplateStore` 和模板 Schema 结构没有为局部画布增加新职责或字段，也不存在局部/原图坐标双写。
25. 新保存字符图片的文件名只有一个 `.png`，用于检测匹配的名称不含扩展名。
26. `git diff --check` 通过。

## 14. 用户验证清单

用户在 Qt Creator 完成构建后验证：

1. 使用当前已有 Schema 2 `app_settings.json` 启动，确认不会要求因本字段清空全部设置。
2. 没有已保存目录时打开“保存模板”，确认弹窗同时显示名称和目录，目录默认为有效桌面目录；桌面无效时使用用户主目录。
3. 确认目录框只读，不能输入或粘贴路径；点击“浏览”后打开 Qt 自己的中文文件夹选择窗口。
4. 浏览选择新目录后取消保存并重启，确认新目录没有被记住。
5. 浏览选择新目录后取消同名覆盖，确认新目录没有被记住。
6. 浏览选择新目录后制造模板文件保存失败，确认新目录没有被记住。
7. 成功保存模板后重启，确认实际使用的根目录自动恢复到保存弹窗。
8. 删除或断开已保存根目录后打开保存弹窗，确认显示桌面目录；桌面无效时显示用户主目录。
9. 确认最终模板目录严格为“弹窗目录/模板名称”。
10. 刚印、字库、深度 OCR 和二维码+三期共用同一个保存目录字段。
11. 纸巾模式不进入模板保存流程。
12. 通过模板选择窗口加载其他目录的已有模板，确认不会改变下次保存弹窗的默认目录。
13. 编辑一个位于其他目录的已有模板，确认仍保存回原目录，也不改变模板保存目录。
14. 使用同名模板分别验证取消覆盖和确认完全覆盖。
15. 刚印和深度 OCR 保存后确认替换当前模式唯一模板；字库和二维码+三期确认追加到当前列表且不重复。
16. 新模板保存成功后确认成为当前编辑模板。
17. 制造一次 `app_settings.json` 保存失败，确认外部模板保留、当前检测方案和已记住目录仍保持上一成功值。
18. 执行“恢复默认设置”，确认模板保存目录清空，但外部模板和检测方案未被删除。
19. 执行“清空软件数据”，确认目录配置和检测方案清空，但外部模板文件夹没有被删除。
20. 钢印、字库和二维码+三期分别保存模板，确认保存成功后立即询问是否切割字符模板；分别验证取消和确定。
21. 深度 OCR 保存成功后确认不显示字符切割询问。
22. 回归模板选择、移除模板引用、已有模板编辑、模板制作绘图和启动检测。
23. 刚印模式打开字符分割，确认窗口只显示已框选的生产日期区域局部图，不显示整张产品原图。
24. 字库模式打开字符分割，确认窗口只显示已框选的文字检测区域局部图。
25. 二维码+三期模式打开字符分割，确认窗口只显示日期区域局部图，不显示二维码区域或整张原图。
26. 在局部图中框选、命名并保存字符，确认 `character_templates/*.png` 内容对应用户框选字符，并且文件名为 `1.png` 而不是 `1.png.png`。
27. 关闭后重新打开字符窗口，确认已有字符框在局部图中的位置正确，没有整体偏移。
28. 打开 `template_settings.json`，确认 `characterSourceSize` 等于日期/文字裁剪图尺寸，`characterBoxes` 坐标以该局部图左上角为 `(0, 0)`。
29. 确认代码不会识别或转换已有原图坐标配置；此类模板按要求重新分割并保存字符模板。
30. 制造无效或缺失的 `datePolygon`，确认字符窗口拒绝打开且不会退回完整原图。
31. 深度 OCR 和纸巾模式确认不显示、不触发字符模板分割入口。
32. 重新切割并保存当前错误模板的字符图片，重启检测后确认不再出现“模板缺少目标字符所需字符”的提示。

## 15. 预期代码规模

本方案不追求通过删除功能减少代码，而是用最少改动补齐目录记忆能力：

- 新增生产代码文件：0。
- 删除生产代码文件：0。
- `.pro` 文件变化：0。
- 新增持久化字段：1。
- 新增应用服务方法：1。
- 新增页面固定 UI 控件：0。
- 新增对话框代码文件：0。
- 新增字符分割数据类型或持久化字段：0。
- 修改字符编辑窗口代码文件：0。
- 新增 Manager、Store、Service、事件总线或状态机：0。

代码行数会小幅增加，但不增加架构层级：保存弹窗直接复用上次成功目录，用户需要时再浏览改变；目录和模板路径在模板文件成功后一次持久化；字符分割、字符框配置和字符图片统一使用日期/文字局部图坐标。

## 16. 完成定义

只有同时满足以下条件，才能宣布本方案完成：

- 保存模板弹窗同时提供模板名称和只读保存目录，目录只能通过“浏览”选择。
- 模板文件成功保存后，实际根目录和当前模式模板路径一次写入唯一 `app_settings.json`，并在重启后恢复。
- 已保存目录无效时使用桌面目录，桌面无效时使用用户主目录；不允许输入或自动创建根目录。
- 新模板保存到“本次弹窗目录/模板名称”。
- 修改目录不移动或修改已有模板。
- 编辑已有模板仍原位保存。
- 选择、移除和编辑已有模板不改变模板保存目录。
- 恢复默认和清空软件数据都清空模板保存目录，但不删除外部模板文件夹。
- 字库和二维码+三期把新模板追加到当前列表；刚印和深度 OCR 替换当前唯一模板。
- 钢印、字库和二维码+三期保存成功后立即询问是否切割字符模板。
- 刚印、字库和二维码+三期的字符窗口只显示 `datePolygon` 对应的日期/文字局部图。
- `characterSourceSize` 和 `characterBoxes` 正式保存为相对于日期/文字裁剪图的尺寸与局部坐标，不再恢复成完整原图坐标。
- 不识别、不转换、不迁移已有完整原图坐标配置，只保留一套局部坐标实现。
- `template_settings.json` Schema 结构保持不变，不新增坐标字段或版本迁移器。
- 当前检测方案模板路径与保存根目录边界保持独立。
- 当前 TemplateStore 安全保存和同名覆盖合同保持不变。
- 未新增第二套配置、目录管理层或生产代码文件。
- Agent 静态门禁通过，用户 Qt Creator 构建和人工验证通过。
