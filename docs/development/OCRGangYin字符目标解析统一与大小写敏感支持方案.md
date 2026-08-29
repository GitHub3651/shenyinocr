# OCRGangYin 字符目标解析统一与大小写敏感支持方案

## 1. 文档状态

- 方案日期：2026-08-25。
- 最近修订：2026-08-29。
- 当前状态：生产代码已实施，Agent Release 全量构建通过，等待用户人工回归。
- 用户决定 1：字母批号严格区分大小写。
- 用户决定 2：目标文字只解析一次，统一使用 `TemplateStore::templateTargetUnits()`。
- 用户决定 3：保持方案简洁高效，不保留旧代码、旧字母资产或旧解析行为的兼容路径。
- 用户决定 4：除大小写和解析唯一化外，字符匹配、判定及外围业务行为不变。
- 用户决定 5：公共字符匹配算法保持唯一；字库和二维码＋三期通过多模板运行配置接收 `targetUnits`，钢印通过钢印运行配置接收 `targetUnits`，不为钢印改造运行快照结构。
- 用户决定 6：保留含义准确的 `PreparedTemplate` 和 `PreparedTemplateSnapshot`；将只服务多模板模式的 `DetectionTemplateSnapshot` / `DetectionTemplateSnapshotBuilder` 直接更名为 `MultiTemplateRuntimeSnapshot` / `MultiTemplateRuntimeSnapshotBuilder`。
- 用户决定 7：将 `DetectionModeWorkerTemplate` 直接更名为 `MultiTemplateRuntimeConfig`，将 `StampConfiguration` 直接更名为 `StampRuntimeConfig`，将 `buildStampConfiguration()` 直接更名为 `buildStampRuntimeConfig()`，所有旧类型、字段、文件和函数名均不保留别名。
- 用户决定 8：字符运行链删除原始 `targetText`，只传递 `targetUnits`；缺失字符校验必须消费同一次解析结果，不得在辅助函数内部再次解析。
- 用户决定 9：`targetUnits` 解析、非法目标校验和缺失字符校验只适用于钢印、字库、二维码＋三期；OCR 从单模板保存、批量更新、模板准备到运行比较均保留原始 `targetText`，不受字符解析规则约束。
- 实施结果：字符目标解析已收口为 `TemplateStore::templateTargetUnits()`；运行链、资产命名和多模板/钢印运行配置均按本文单一路径完成修改，未保留旧别名或兼容分支。
- 验证状态：2026-08-29 已完成 qmake、MSVC x64 Release 全量编译、链接和部署；第 14 节真实模板及三种字符模式人工验证仍待用户执行。

## 2. 目标

功能行为只改变两项：

1. `A` 和 `a` 是两个不同目标字符，必须使用两个不同字符模板。
2. `TemplateStore` 是目标文字解析的唯一权威，Detection Pipeline 只消费解析结果。

另完成一项不改变检测行为的内部命名收口：多模板快照、多模板运行参数和钢印运行参数改用职责明确的名称，旧名称直接删除。

### 2.1 概念边界

| 类型 | 唯一含义 |
|---|---|
| `PreparedTemplate` | 从一份模板目录加载并完成严格校验的完整模板数据 |
| `PreparedTemplateSnapshot` | 指向 `PreparedTemplate` 的只读智能指针，所有模板模式均使用 |
| `MultiTemplateRuntimeSnapshot` | 仅供字库和二维码＋三期使用的多模板定位及运行参数集合 |
| `MultiTemplateRuntimeConfig` | 多模板集合中与一个定位模板同下标对应的一份运行参数 |
| `StampRuntimeConfig` | 钢印单模板的字符匹配和重叠检测运行参数 |

这些类型都是数据边界，不是不同字符算法；三个字符模式继续调用同一个 `CharacterGlyphMatcher`。

### 2.2 最终数据流

```text
TemplateSettings.targetText
  ├─ Stamp / Word / BarcodeWord
  │    → TemplateStore::templateTargetUnits()   单次唯一解析
  │    → PreparedTemplate.targetUnits
  │        ├─ Word / BarcodeWord
  │        │    → MultiTemplateRuntimeSnapshot.runtimeConfigs
  │        │    → MultiTemplateRuntimeConfig.targetUnits
  │        │    → Pipeline
  │        └─ Stamp
  │             → buildStampRuntimeConfig()
  │             → StampRuntimeConfig.targetUnits
  │             → StampDetectionPipeline
  │    → targetCharacterCount = targetUnits.size()
  └─ OCR
       → 原始 TemplateSettings.targetText
       → OcrDetectionPipeline
```

三个字符模式的任何下游模块不得根据 `targetText` 再次运行正则表达式或计算字符串长度。OCR 不调用字符目标解析器，继续使用原始 `targetText`。

## 3. 保持不变的行为

- 数字和中文字符的目标语义。
- 当前 `TemplateStore` 对分隔符的忽略行为。
- `CharacterGlyphMatcher` 的模板匹配算法。
- `TM_CCOEFF_NORMED` 匹配方法。
- 字符模板和目标图缩小到 `0.5` 倍。
- `imageThresholdPercent` 的含义和范围。
- 每个目标组在完整日期 ROI 中选择一个最高分候选。
- 候选 IoU 大于 `0.3` 时拒绝。
- `detectedCharacterCount == targetCharacterCount` 的最终判定。
- 日期 ROI、20 像素 padding 和 Overlay。
- 钢印重叠、二维码解码、定位、统计、PLC、存图和 UI 流程。
- OCR 原始目标文字的保存、加载和运行比较行为。
- TemplateSettings Schema 1 和当前 JSON 字段。

## 4. 明确不做

- 不保留全局 `templateTargetUnits()` 包装或别名。
- 不保留 Pipeline 本地解析器。
- 不保留 `targetText.length()` 回退。
- 不兼容旧字母模板文件名 `A.png`、`a.png`。
- 不增加旧新字母文件名双读。
- 不增加自动迁移、自动重命名或大小写猜测。
- 不增加运行开关或失败回退。
- 不增加固定字符槽位、字符排序或额外字符检查。
- 不增加 OCR、字符分类模型或字符级阈值。
- 不修改字符残缺和肉眼可读性的判定公式。
- 不升级 JSON Schema。
- 不把多模板、二维码和钢印专有字段强行合并成一个大配置结构。
- 不保留 `DetectionTemplateSnapshot`、`DetectionTemplateSnapshotBuilder`、`templateSnapshot`、`DetectionModeWorkerTemplate`、`detectionTemplates`、`StampConfiguration` 或 `buildStampConfiguration()` 的类型别名、字段别名、包装函数或过渡入口。

实施后，旧字母模板必须重新分割并保存；不允许旧数据决定新代码结构。

## 5. 当前问题

### 5.1 同一目标被解析两次

当前：

```text
TemplateStore::templateTargetUnits()
  → 决定加载哪些字符模板和目标索引

WordDetectionPipeline::parseTargetUnits()
StampDetectionPipeline::countTargetCharacters()
  → 决定目标字符数量
```

两套正则不完全相同，Pipeline 还存在 `targetText.length()` 回退。模板组数量和最终目标数量可能不一致。

### 5.2 当前大小写被折叠

目标单位、字符资产基础名和资产匹配会调用 `toLower()`，因此：

```text
A == a
```

这不满足字母批号区分大小写的业务要求。

### 5.3 Windows 文件名不能只靠大小写区分

同一模板目录不能可靠地同时保存：

```text
A.png
a.png
```

因此新字母模板必须使用内容不同的文件名，不能只删除 `toLower()`。

## 6. 唯一目标解析

### 6.1 唯一接口

将当前全局函数改为：

```cpp
class TemplateStore
{
public:
    static QStringList templateTargetUnits(
        const QString &targetText);
};
```

删除全局声明和实现入口，不提供兼容包装。

### 6.2 解析规则

继续使用当前 TemplateStore 支持的字符集合，不扩展语法：

- 数字；
- ASCII 大写字母；
- ASCII 小写字母；
- 中文；
- 当前已有的数字括号单位。

删除解析结果上的 `toLower()`，原样返回大小写。

示例：

| 输入 | 结果 |
|---|---|
| `AB12` | `A,B,1,2` |
| `ab12` | `a,b,1,2` |
| `Aa12` | `A,a,1,2` |
| `AB-12` | `A,B,1,2` |
| `中A1` | `中,A,1` |
| `2026-08-25` | `2,0,2,6,0,8,2,5` |

分隔符继续按当前 TemplateStore 规则忽略，不参与字符匹配。

### 6.3 只解析一次

“只解析一次”指同一份字符模式目标文字在一次模板准备、当前模板目标保存或批量校验流程中只调用一次唯一解析器，不是限制程序生命周期内只能调用一次。OCR 不进入字符解析流程。

三个字符模式由现有 `DetectionModeDescriptor::requiresCharacterTemplates` 判定，不新增第二套模式分类。`TemplateStore::loadPrepared()` 只在该值为 `true` 时执行：

```cpp
const QStringList targetUnits =
        TemplateStore::templateTargetUnits(
            editable.settings.targetText);
```

随后同一份 `targetUnits` 同时用于目标有效性、缺失字符校验、字符模板分组和 `PreparedTemplate`：

```cpp
const QString missingTarget =
        missingTemplateTargetUnit(
            expectedMode,
            targetUnits,
            editable.characterAssets);

candidate->targetUnits = targetUnits;
```

`missingTemplateTargetUnit()` 直接改为接收已解析列表：

```cpp
QString missingTemplateTargetUnit(
    DetectionMode mode,
    const QStringList &targetUnits,
    const QVector<TemplateCharacterAsset> &characterAssets);
```

该函数内部不得调用 `TemplateStore::templateTargetUnits()`，也不得接收 `TemplateSettings` 或原始 `targetText`。

当前模板单独保存目标文字时，`TemplateEditorPage::applyCurrentTargetText()` 按以下单一路径处理：

```text
读取待保存的 TemplateSettings.targetText
  → requiresCharacterTemplates == true
       → 调用一次 TemplateStore::templateTargetUnits()
       → 校验非空原文不能得到空 targetUnits
       → 将同一 targetUnits 传给 missingTemplateTargetUnit()
       → 校验通过后保存
  → requiresCharacterTemplates == false（OCR）
       → 不调用字符解析器和缺失字符校验
       → 按原有路径保存原始 targetText
```

`TemplateApplicationService::updateTemplates()` 批量更新时采用相同模式边界：钢印、字库、二维码＋三期各模板调用一次 `TemplateStore::templateTargetUnits()`，完成非法目标检查后把同一列表传给 `missingTemplateTargetUnit()`；OCR 跳过这两项字符校验，继续保存原始 `targetText`。

同一份 `targetUnits` 同时用于：

1. 校验目标有效性；
2. 校验字符模板是否齐全；
3. 构造 `characterTemplates`；
4. 构造 `characterTemplateTargetIndexes`；
5. 构造 `MultiTemplateRuntimeSnapshot` 或 `StampRuntimeConfig`；
6. 计算最终目标字符数量；
7. 生成缺失字符和识别诊断。

不得再次解析原始 `targetText`。

### 6.4 非法目标

仅对钢印、字库、二维码＋三期：如果去除首尾空白后的目标非空、但解析结果为空，三个入口均拒绝继续：

- `TemplateStore::loadPrepared()` 返回：

```text
code=TEMPLATE_TARGET_INVALID
userMessage=目标文字不包含可检测字符。
```

- `TemplateEditorPage::applyCurrentTargetText()` 不保存并恢复原目标文字；
- `TemplateApplicationService::updateTemplates()` 将该模板记录为批量更新失败，不写入无效目标。

字符模式不进入正式运行，也不回退到字符串长度。

OCR 不执行该检查。即使 OCR 原始目标不包含字符解析器支持的数字、ASCII 字母或中文，也不得因此拒绝保存或准备模板；OCR 继续把完整原文交给 `OcrDetectionPipeline` 比较。

## 7. 目标单位的内存传递

### 7.1 `PreparedTemplate`

增加纯内存字段：

```cpp
QStringList targetUnits;
```

不写入 JSON，不改变 Schema。

该字段只在钢印、字库、二维码＋三期中填充。OCR 的 `PreparedTemplate::targetUnits` 保持为空，OCR 仍从 `PreparedTemplate::settings.targetText` 取得原始目标。

`PreparedTemplate` 保持现名，继续表示模板层从磁盘加载、完成严格校验后的通用完整模板；它是各模式运行配置的共同输入，不承担模式专有运行状态。

### 7.2 字库和二维码＋三期：`MultiTemplateRuntimeSnapshot`

将当前：

```cpp
struct DetectionTemplateSnapshot;
class DetectionTemplateSnapshotBuilder;
struct DetectionModeWorkerTemplate;
std::vector<DetectionModeWorkerTemplate> detectionTemplates;
```

直接更名为：

```cpp
struct MultiTemplateRuntimeSnapshot;
class MultiTemplateRuntimeSnapshotBuilder;
struct MultiTemplateRuntimeConfig;
std::vector<MultiTemplateRuntimeConfig> runtimeConfigs;
```

对应文件直接从：

```text
detection_template_snapshot.h/.cpp
```

更名为：

```text
multi_template_runtime_snapshot.h/.cpp
```

所有 include 和 qmake 清单同步更新，不保留旧文件。`DetectionRegistryRequest::templateSnapshot`、Runtime 启动参数及运行上下文中的 `templateSnapshot` 直接更名为 `multiTemplateSnapshot`。

`MultiTemplateRuntimeConfig` 表示字库和二维码＋三期在多模板定位后，可按模板索引选中的一份运行参数。将其中当前的：

```cpp
QString targetText;
```

直接替换为：

```cpp
QStringList targetUnits;
```

不同时保留两个字段。由 `MultiTemplateRuntimeSnapshotBuilder::create()` 从对应 `PreparedTemplate::targetUnits` 直接复制。

`MultiTemplateRuntimeSnapshot::trackingTemplates` 与 `runtimeConfigs` 继续保持同长度、同下标对应；定位得到的 `wordTemplateIndex` 仍只选择同下标运行参数，不改变多模板最高分逻辑。

### 7.3 钢印：`StampRuntimeConfig`

钢印是单模板定位模式，不经过 `MultiTemplateRuntimeSnapshot::runtimeConfigs`。在 `detection_registry.cpp` 中，将当前：

```cpp
struct StampConfiguration;
bool buildStampConfiguration(...);
```

直接更名为：

```cpp
struct StampRuntimeConfig;
bool buildStampRuntimeConfig(...);
```

不保留旧类型名或旧函数入口。同时将运行配置中的：

```cpp
QString targetText;
```

直接替换为：

```cpp
QStringList targetUnits;
```

`buildStampRuntimeConfig()` 直接复制已准备值：

```cpp
runtimeConfig->targetUnits = prepared.targetUnits;
```

钢印继续使用现有单模板配置、字符模板和重叠检测回调，不改造成多模板运行快照，不新增第二套字符匹配算法。

### 7.4 Pipeline

`WordDetectionPipeline`、`StampDetectionPipeline` 的所有字符检测入口都将 `const QString &targetText` 直接替换为 `const QStringList &targetUnits`；`BarcodeWordDetectionPipeline` 同样替换参数并原样透传给 Word Pipeline。

删除：

- `WordDetectionPipeline::parseTargetUnits()`；
- `StampDetectionPipeline::countTargetCharacters()`；
- 两处本地正则表达式；
- 两处 `targetText.length()` 回退。

字符运行链不再携带原始 `targetText`。`TemplateSettings::targetText` 继续作为磁盘配置和 OCR 模式的原始目标字符串，不在本专项中删除。

最终判定保持：

```cpp
targetCharacterCount = targetUnits.size();
isOk = detectedCharacterCount == targetCharacterCount;
```

三个字符 Pipeline 不 include `template_store.h`，只消费已准备值。Snapshot Builder 和 Registry 位于模板数据转换为模式运行参数的组装边界，可以读取 `PreparedTemplate`，但不得重新解析目标文字。

## 8. 大小写敏感字符资产

### 8.1 唯一文件名规则

新保存的 ASCII 字母模板只允许以下内部名称：

```text
大写 A → upper_A.png
小写 a → lower_a.png
大写 B → upper_B.png
小写 b → lower_b.png
```

同一字符的多个模板：

```text
upper_A.png
upper_A(1).png
lower_a.png
lower_a(1).png
```

数字和中文继续使用当前直接名称：

```text
2.png
2(1).png
中.png
```

旧字母名称 `A.png`、`a.png` 一律不识别。字符编辑器保存新字母模板时直接生成唯一新格式。

### 8.2 唯一存储名函数

新增一个小型纯函数，由字符编辑器和 TemplateStore 共用：

```cpp
QString characterStorageStem(const QString &unit);
```

规则：

```text
ASCII 大写字母 → upper_<原字母>
ASCII 小写字母 → lower_<原字母>
其他当前合法单位 → 原单位
```

该函数只有一份实现，不在 UI 和 TemplateStore 各复制一套。

### 8.3 资产结构

将含义模糊的：

```cpp
normalizedBaseName
```

直接替换为：

```cpp
storageStem
```

不保留旧字段别名。

加载和暂存字符图片时保留实际文件基础名，不调用 `toLower()`。

### 8.4 唯一匹配规则

目标单位先转换成唯一存储名，再进行大小写敏感比较：

```text
targetUnit
  → characterStorageStem(targetUnit)
  → 与 asset.storageStem 精确匹配
```

只接受：

```text
精确名称
精确名称 + (数字) 变体后缀
```

不接受旧 `_...`、`-...` 或直接字母文件名兼容规则。

示例：

| 目标 | 资产 | 结果 |
|---|---|---|
| `A` | `upper_A.png` | 通过 |
| `A` | `lower_a.png` | 拒绝 |
| `A` | `A.png` | 拒绝 |
| `a` | `lower_a.png` | 通过 |
| `a` | `upper_A.png` | 拒绝 |
| `a` | `a.png` | 拒绝 |
| `Aa` | `upper_A.png`、`lower_a.png` | 通过 |

## 9. 旧字母模板处理

实施后，含旧字母资产的模板在 `TemplateStore::loadPrepared()` 中直接失败：

```text
code=TEMPLATE_CHARACTER_INVALID
userMessage=字母字符模板文件名无效，请重新分割并保存字符模板。
```

处理原则：

- 不读取旧字母文件名。
- 不自动重命名。
- 不自动迁移。
- 不猜测图片中的实际大小写。
- 不提供批量转换工具。
- 不保留旧新双路径。

用户在新版字符编辑器中重新分割并保存字母模板，生成唯一新格式。

## 10. 文件级修改计划

| 文件 | 修改 |
|---|---|
| `app/templates/template_store.h/.cpp` | 静态唯一解析器、`targetUnits`、`storageStem`、严格新字母名校验和大小写敏感匹配 |
| `app/application/template_application_service.cpp` | 三个字符模式的批量更新各模板只解析一次并把同一 `targetUnits` 传给缺失校验；OCR 跳过字符解析和缺失校验；暂存资产使用新存储名且不转小写 |
| `app/ui/pages/template_editor_page.cpp` | `applyCurrentTargetText()` 在三个字符模式中只解析一次并复用 `targetUnits`，OCR 直接保存原始 `targetText` |
| `app/ui/dialogs/character_template_editor_dialog.cpp` | 使用统一存储名函数生成 `upper_/lower_` 文件名 |
| `app/detection/detection_template_snapshot.h/.cpp` → `app/detection/multi_template_runtime_snapshot.h/.cpp` | 文件、Snapshot、Builder、单模板运行项和数组字段直接更名，并从 `PreparedTemplate` 传递 `targetUnits` |
| `app/AutoOCRproject.pro` | 删除旧快照文件条目并登记新文件名 |
| `app/application/inspection_application_service.cpp` | 只为多模板模式构造 `MultiTemplateRuntimeSnapshot`，局部变量改为 `multiTemplateSnapshot` |
| `app/runtime/inspection_runtime.h/.cpp` | Runtime 启动参数和运行上下文改为 `MultiTemplateRuntimeSnapshot multiTemplateSnapshot`，仅同步命名，不改变所有权或线程行为 |
| `app/detection/detection_registry.h/.cpp` | 请求字段改为 `multiTemplateSnapshot` 并消费 `runtimeConfigs`；钢印运行结构和 Builder 直接更名，再从 `PreparedTemplate` 复制 `targetUnits` |
| `app/detection/detectionmode/word/word_detection_pipeline.h/.cpp` | 所有字符入口以 `targetUnits` 替换 `targetText`，删除本地解析和长度回退 |
| `app/detection/detectionmode/stamp/stamp_detection_pipeline.h/.cpp` | 所有字符入口以 `targetUnits` 替换 `targetText`，删除本地解析和长度回退 |
| `app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h/.cpp` | 以 `targetUnits` 替换 `targetText` 并原样透传 |
| `app/detection/README.md`、`docs/development/OCRGangYin开发者代码结构与维护指南.md` | 更新新文件名、类型名和多模板快照职责说明；不修改历史计划正文 |

不新增兼容类、迁移器、适配器、配置开关或第二套字符资产结构。

## 11. 实施顺序

### 阶段 U1：解析唯一化

1. 收口 `TemplateStore::templateTargetUnits()`。
2. 保留目标大小写。
3. 增加 `PreparedTemplate::targetUnits`。
4. `missingTemplateTargetUnit()` 改为只消费已解析 `targetUnits`；`loadPrepared()`、`TemplateEditorPage::applyCurrentTargetText()` 和 `TemplateApplicationService::updateTemplates()` 在三个字符模式中各流程只解析一次，OCR 全程跳过字符解析和缺失校验。
5. 多模板 Snapshot、Builder、文件、请求字段和 Runtime 字段直接改为 `MultiTemplateRuntime...` / `multiTemplateSnapshot` 单一路径。
6. `DetectionModeWorkerTemplate` / `detectionTemplates` 直接更名为 `MultiTemplateRuntimeConfig` / `runtimeConfigs`，并以 `targetUnits` 替换 `targetText`。
7. `StampConfiguration` / `buildStampConfiguration()` 直接更名为 `StampRuntimeConfig` / `buildStampRuntimeConfig()`，并以 `targetUnits` 替换 `targetText`。
8. 三个字符 Pipeline 以 `targetUnits` 替换 `targetText`，删除所有本地解析和回退。

门禁：生产代码只有一个字符目标解析实现；同一准备或校验流程不重复调用；OCR 不进入字符解析流程；旧快照、运行配置和字符链 `targetText` 名称全部清零。

### 阶段 U2：字母资产唯一化

1. 增加唯一 `characterStorageStem()`。
2. `normalizedBaseName` 直接替换为 `storageStem`。
3. 字符编辑器只写 `upper_/lower_` 字母资产。
4. TemplateStore 只读新字母资产。
5. 大小写错误和旧字母资产启动前拒绝。

门禁：不存在旧字母文件名兼容分支。

### 阶段 U3：验证和文档收口

1. 执行第 12 节验证矩阵。
2. 执行静态搜索和 `git diff --check`。
3. 更新数据 Schema 文档的字符资产命名说明，但 Schema 版本不变。
4. 更新开发者指南、功能对照表和执行记录。
5. 用户在 Qt Creator 完整构建，验证三种字符模式并执行 OCR 原始目标回归。

## 12. 验证矩阵

### 12.1 解析

| 模式 | 输入 | 唯一结果 |
|---|---|---|
| 钢印、字库、二维码＋三期 | `AB12` | `A,B,1,2` |
| 钢印、字库、二维码＋三期 | `ab12` | `a,b,1,2` |
| 钢印、字库、二维码＋三期 | `Aa12` | `A,a,1,2` |
| 钢印、字库、二维码＋三期 | `AB-12` | `A,B,1,2` |
| 钢印、字库、二维码＋三期 | `中A1` | `中,A,1` |
| 钢印、字库、二维码＋三期 | `2026-08-25` | `2,0,2,6,0,8,2,5` |
| 钢印、字库、二维码＋三期 | `---` | 当前模板保存、批量更新和模板准备均失败 |
| OCR | `---` | 不调用字符解析器；原始目标可保存、加载并进入 OCR 比较 |

三个字符模式接收到的单位内容、顺序和数量必须完全一致。

### 12.2 字母资产

| 目标 | 模板文件 | 结果 |
|---|---|---|
| `A` | `upper_A.png` | 加载成功 |
| `A` | `lower_a.png` | 加载失败 |
| `A` | `A.png` | 加载失败 |
| `a` | `lower_a.png` | 加载成功 |
| `a` | `upper_A.png` | 加载失败 |
| `a` | `a.png` | 加载失败 |
| `Aa` | `upper_A.png`、`lower_a.png` | 加载成功，两个目标组 |
| `AA` | `upper_A.png`、`upper_A(1).png` | 加载成功，两个目标组共享同一字符模板组 |

### 12.3 Pipeline

- 字库 `AB12`：目标数 4，匹配数 4 时 OK，3 时 NG。
- 钢印 `AB12`：字符计数不变，仍与重叠结果 AND。
- `MultiTemplateRuntimeSnapshot::trackingTemplates` 与 `runtimeConfigs` 数量相同、下标严格对应。
- `MultiTemplateRuntimeSnapshot::runtimeConfigs` 中每份 `MultiTemplateRuntimeConfig` 均从对应 `PreparedTemplate` 复制 `targetUnits`，且不保留 `targetText`。
- `StampRuntimeConfig` 从 `prepared.targetUnits` 复制，`StampDetectionPipeline` 不接收或解析 `targetText`。
- 二维码+三期 `AB12`：二维码行为不变，日期目标数为 4。
- OCR 不接收 `targetUnits`，仍从 `TemplateSettings::targetText` 取得原始目标并执行原有精确比较。
- `recognizedText`、`missingUnits` 和诊断保留大小写。
- 三个字符 Pipeline 不根据 `targetText` 计算目标数。

### 12.4 不变行为

- 纯数字模板结果和分数不变。
- 中文字符结果不变。
- 分隔符继续忽略。
- 匹配位置、IoU、阈值和 Overlay 不变。
- 钢印重叠与二维码解码不变。
- OCR 原始目标保存、模板准备和运行比较行为不变。
- 统计、PLC、存图和 UI 流程不变。

## 13. 静态门禁

1. 全局 `templateTargetUnits()` 声明和实现为零。
2. `TemplateStore::templateTargetUnits()` 是唯一解析入口。
3. `missingTemplateTargetUnit()` 只接收 `DetectionMode`、`targetUnits` 和字符资产；函数内部解析调用为零。
4. `TemplateStore::loadPrepared()`、`TemplateApplicationService::updateTemplates()` 和 `TemplateEditorPage::applyCurrentTargetText()` 在每个字符模式流程中对同一原文最多调用一次 `TemplateStore::templateTargetUnits()`。
5. 上述三个入口均通过 `DetectionModeDescriptor::requiresCharacterTemplates` 限定字符解析；OCR 分支的字符解析和 `missingTemplateTargetUnit()` 调用为零。
6. `parseTargetUnits` 和 `countTargetCharacters` 生产引用为零。
7. Word、Stamp、BarcodeWord Pipeline 的 `targetText` 参数以及目标计数相关 `targetText.length()` 为零。
8. `DetectionTemplateSnapshot`、`DetectionTemplateSnapshotBuilder`、`templateSnapshot`、`detection_template_snapshot` 生产引用和 qmake 条目为零。
9. `MultiTemplateRuntimeSnapshot`、`MultiTemplateRuntimeSnapshotBuilder`、`multiTemplateSnapshot` 和 `multi_template_runtime_snapshot` 成为唯一新名称。
10. `DetectionModeWorkerTemplate` 和 `detectionTemplates` 生产引用为零；`MultiTemplateRuntimeConfig` 和 `runtimeConfigs` 成为唯一新名称。
11. `MultiTemplateRuntimeConfig::targetText` 为零，只保留 `targetUnits`。
12. `StampConfiguration` 和 `buildStampConfiguration` 生产引用为零；`StampRuntimeConfig` 和 `buildStampRuntimeConfig` 成为唯一新名称。
13. `StampRuntimeConfig::targetText` 为零，只保留 `targetUnits`。
14. 字符目标和资产身份路径中的 `toLower()` 为零。
15. `normalizedBaseName` 生产引用为零。
16. 旧字母文件名识别和兼容分支为零。
17. 自动迁移、运行开关和大小写回退为零。
18. `TemplateSettings::targetText` 和 OCR 原始目标字符串保持存在；OCR Pipeline 继续接收原始字符串，TemplateSettings Schema 仍为 1。
19. `git diff --check`、UTF-8、结尾换行和 qmake 清单检查通过。

## 14. 用户验证

用户在 Qt Creator 中执行：

1. qmake、Clean、Rebuild。
2. 新建目标 `AB12`，确认生成 `upper_A.png`、`upper_B.png`。
3. 新建目标 `ab12`，确认生成 `lower_a.png`、`lower_b.png`。
4. 新建目标 `Aa12`，确认大写和小写资产可同时保存和加载。
5. 放入旧 `A.png` 或 `a.png`，确认模板启动前明确拒绝。
6. 字库、钢印、二维码+三期各验证一次字母数字目标。
7. 验证既有纯数字模板的匹配和外围行为不变。
8. 在 OCR 模式保存和加载原始目标 `---`，确认不会因字符解析结果为空而失败，并确认运行时仍按原始字符串比较。

## 15. 与其他计划的关系

- 本专项可独立实施，不依赖核心算法计划 D2。
- 后续字符算法只能消费本专项形成的 `targetUnits`，不得恢复本地解析。
- 本专项不改变模板单 Store、严格 JSON 和运行快照所有权；只把多模板专用快照边界改为准确名称。
- 若其他计划同时修改 Snapshot、Registry 或 Pipeline 签名，必须串行实施并基于先完成者重核调用点。

## 16. 完成定义

1. 生产代码只有 `TemplateStore::templateTargetUnits()` 一个字符目标解析器。
2. 三个字符模式的同一目标文字在每次模板准备、当前模板保存或批量更新流程中只解析一次。
3. 三个字符模式只消费已准备的 `targetUnits`；字库和二维码＋三期经 `MultiTemplateRuntimeSnapshot::runtimeConfigs` 传递，钢印经 `StampRuntimeConfig` 传递。
4. `A` 和 `a` 形成不同目标单位和不同模板资产。
5. 字母模板只使用 `upper_/lower_` 新文件名。
6. 旧字母文件名、旧字段、兼容分支、迁移器和回退全部不存在。
7. 错误大小写和旧字母模板在启动前拒绝。
8. 识别和诊断文本保留大小写。
9. 字符匹配算法、最终数量判定和外围业务行为不变。
10. TemplateSettings Schema 未升级。
11. 静态门禁全部通过。
12. 用户完成 Qt Creator 构建、三种字符模式人工验证和 OCR 原始目标回归验证。
13. `PreparedTemplate` 与 `PreparedTemplateSnapshot` 保持原职责；多模板快照统一为 `MultiTemplateRuntimeSnapshot`，旧文件、类型、字段、函数和兼容别名全部不存在。
14. 缺失字符校验和字符模板分组使用同一次解析产生的 `targetUnits`；字符运行结构和 Pipeline 不携带原始 `targetText`。
15. `TemplateEditorPage::applyCurrentTargetText()` 已纳入单次解析链；非法 `targetUnits` 只拒绝钢印、字库、二维码＋三期，OCR 的原始目标保存、模板准备和运行比较不受字符解析规则影响。
