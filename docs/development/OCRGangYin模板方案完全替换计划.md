# OCRGangYin 模板方案完全替换计划

版本：1.8（阶段 8 分阶段实施定稿）
计划日期：2026-08-20
状态：待实施
实施阶段：阶段 8（内部拆分为 8A～8G，连续实施，全部完成后统一验证）

> 本文定义 OCRGangYin 撤销现有产品配方体系、恢复“外部模板文件夹 + 检测方案设置”的完整替换方案。本文获批准并进入实施后，涉及模板、模式私有参数和运行资源准备的内容以本文为准；《OCRGangYin 新架构完全替换执行方案》中阶段 0～7 的历史实施与验证证据继续有效，但其中要求继续保留 `ProductRecipe`、`RecipeStore`、配方 UUID、发布/重发等内容不再作为后续实现依据。

## 一、改造目标

本阶段不是把“产品配方”改一个中文名称，而是彻底删除当前配方模型，恢复更直接的模板使用方式：

1. 一个模板就是一个用户可见、可复制、可移动的外部文件夹。
2. 模板文件夹保存该模板自己的图片、ROI、目标文字、阈值、字符模板等私有配置。
3. 软件只保留一个 `app_settings.json`；其中 `detectionSchemes` 分区记录五种检测模式当前选用了哪些模板，以及无模板模式自己的私有参数。
4. 支持多模板的模式，把当前选择的多个模板文件夹按顺序组成“模板组”；模板组不是新的目录、UUID或数据库实体。
5. 运行时根据当前模式读取检测方案，再加载模板文件夹，形成一次检测运行使用的不可变快照。
6. 完全删除配方 UUID、发布、重新发布、已发布列表、产品配方选择和配方目录复制等概念。

最终用户只需要理解三个概念：

```text
应用设置：唯一的 app_settings.json，统一保存整机、界面和检测方案
检测方案：app_settings.json 中每种模式当前选择的模板路径，或无模板模式的私有参数
模板文件夹：某一个具体模板的资源和私有参数
```

### 1.1 简单设计的硬性约束

本阶段必须同时撤销配方和减少代码，不允许只把 `Recipe` 改名为另一套同等复杂的架构。实现固定遵守以下约束：

1. 五种检测模式是当前明确业务，不为假设中的插件化、动态模式或远程模板库预建通用框架。
2. 不新增 `DetectionSchemeStore`、`DetectionSchemeApplicationService`、`TemplateEditorSession`、`TemplateRepository`、`TemplateManager`、事件总线或工厂层。
3. 检测方案只是 `AppSettings` 的一个字段分区，不是独立领域对象、文件、目录、UUID、版本或发布实体。
4. 模板组只是 `QStringList` 表示的有序外部文件夹路径，不建立 `TemplateGroup` 文件夹或管理类。
5. `TemplateApplicationService` 继续使用现有文件，直接持有一次编辑所需的草稿和临时工作区；不再套一层 Session。
6. 模板磁盘能力只新增 `template_store.h/.cpp` 两个代码文件；模板数据、准备后数据和错误结构集中定义在同一头文件中。
7. `SettingsApplicationService` 是 `AppSettings` 唯一内存正式值持有者；模板选择直接保存其 `detectionSchemes` 分区，不再增加第二个应用服务。
8. 不恢复已删除的 `tests/` 工程；本阶段由 Agent 做静态门禁，最终由用户在 Qt Creator 一次性完成构建和人工回归。
9. 不提供旧配方、旧设置或旧模板目录兼容，不增加迁移器、双读取、回退路径和兼容开关。
10. 不删除或精简图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL及其他资源文件。
11. 模板的文字、阈值、ROI和区域点集统一保存在 `template_settings.json`；不再为三个区域点集维护 `calibrate_config.yaml`。
12. 模板选择不建立“全部模板库”、最近使用列表或目录扫描器；对话框只显示当前已选路径和本次新加入的路径，以勾选状态表达最终选择。
13. “模板索引”只允许作为一次运行内有效模板数组的临时连续下标，不写入 JSON、不显示给用户、不作为模板身份，也不建立索引映射表或管理类。
14. 不实现跨多个模板文件夹的全局事务、事务日志或 `saveBatch()`；每个模板文件夹独立安全保存，批量操作明确报告成功、失败和未处理项。
15. 已有模板编辑、新模板创建和同名模板覆盖必须调用同一个 `TemplateStore::save()` 和同一套目录提交逻辑，只用“是否继承目标目录原内容”区分输入，不分别实现更新函数和覆盖函数。
16. “当前编辑模板”后的移除按钮与模板选择对话框取消勾选并确认完全等价：只移除当前模式的路径引用，永远不删除、移动或修改外部模板文件夹，也不增加第二套移除逻辑。

本阶段与配方/模板/设置直接相关的代码文件数量目标如下：

```text
当前：recipes 8 个代码文件
目标：templates 2 个代码文件
净减少：至少 6 个 .h/.cpp

设置文件：4 个改名并改职责，不增加数量
模板选择对话框：2 个原位改名，不增加数量
TemplateApplicationService：复用现有 2 个文件，不新增服务
整个 app：当前 157 个 .h/.cpp，阶段完成后目标不超过 151 个
```

如实施过程中发现必须突破上述数量，只能因为当前已验证功能确实无法在现有文件中清晰承载，并在写代码前向用户说明原因；不得以“将来可能需要”为理由新增文件。

## 二、已确认的业务规则

### 2.1 五种检测模式

| 检测模式 | 内部模式 ID | 模板选择能力 | 检测方案中保存的内容 | 启动时的资源要求 |
|---|---|---|---|---|
| 刚印检测 | `stamp` | 0 或 1 个模板 | 一个模板文件夹路径 | 已选择且模板完整有效 |
| 字库匹配 | `word` | 0 或多个模板 | 有顺序的模板文件夹路径列表 | 至少一个有效模板；全部有效模板并行匹配并取最高分 |
| 深度 OCR | `ocr` | 0 或 1 个模板 | 一个模板文件夹路径 | 已选择且模板完整有效 |
| 纸巾检测 | `tissue` | 不支持模板 | `roughnessThreshold` 等本模式私有参数 | 不检查任何模板资源 |
| 二维码+三期 | `barcodeWord` | 0 或多个模板 | 有顺序的模板文件夹路径列表 | 至少一个有效模板；全部有效模板并行匹配并取最高分 |

“0 个模板”表示允许软件处于尚未配置状态，不表示该模式一定能够启动检测。刚印、字库、深度 OCR 和二维码+三期在没有可用模板时，启动预检必须给出明确提示并拒绝启动。纸巾检测永远不因模板缺失而被拒绝。

### 2.2 检测方案数量

- 每种检测模式只维护一份“当前检测方案”。
- 不增加检测方案名称、方案列表、方案 UUID、发布状态或版本管理。
- 切换模式时，软件读取该模式已经保存的当前选择。
- 支持多模板的模式中，当前有序路径列表就是该模式的模板组。
- 模板组不额外生成文件夹，也不复制模板资源。

### 2.3 模板路径

- 模板可以位于用户选择的任意外部目录，不强制复制到 AppData。
- `app_settings.json` 的 `detectionSchemes` 分区保存规范化后的绝对路径。
- Windows 下使用不区分大小写的路径去重规则。
- 多模板顺序必须持久化，运行时不得自行按名称或文件系统顺序重排。
- 多模板顺序只用于 UI 展示、稳定加载和同分时的确定性优先级，不表示“先匹配到谁就使用谁”。
- 模板显示名称直接使用文件夹名称，不再维护独立 `displayName` 和 UUID。
- 从检测方案中取消选择模板或点击“移除模板”，都只移除当前模式中的路径引用，绝不删除、移动或修改外部模板文件夹。

### 2.4 旧数据处理

- 当前 `<AppData>/recipes/` 下的配方数据不迁移、不读取、不继续写入，也不由软件自动删除。
- 当前 `<AppData>/editor-workspaces/` 等旧配方编辑数据不迁移、不读取，也不自动删除。
- 旧数据继续留在磁盘，只作为人工备份；用户确认不需要后可自行处理。
- 新版本不提供新旧双读取、运行时兼容开关或离线转换工具。
- Git 历史是旧源码备份，生产源码中不保留配方兼容桥或注释掉的旧实现。

## 三、新的数据目录与文件

### 3.1 AppData 目录

新版本只正式使用以下软件数据；不再创建第二个检测方案配置文件：

```text
<AppData>/ShengYin/
├─ settings/
│  └─ app_settings.json          # 整机、界面和五种模式当前检测方案的唯一设置文件
└─ template-editor-workspaces/   # 模板编辑过程的临时工作区
```

旧目录可能仍然存在，但不进入任何新调用链：

```text
<AppData>/ShengYin/
├─ recipes/                      # 旧配方数据，仅保留在磁盘
└─ editor-workspaces/            # 旧编辑工作区，仅保留在磁盘
```

### 3.2 唯一应用设置 `app_settings.json`

应用设置升级为 Schema 2。它统一保存：

- 当前检测模式；
- 相机曝光、增益、触发、旋转和颜色通道；
- PLC 连接参数和工艺参数；
- 检测节拍相关整机参数；
- 存图目录、范围、原图/标注图组合；
- 纯 UI 布局和显示状态；
- 五种检测模式当前选择的模板路径；
- 纸巾模式的粗糙度阈值。

Schema 2 必须删除：

- `publishedRecipeIdsByMode`；
- `lastRecipeIdByMode`；
- `recipesRootPath`；
- 任何配方 UUID、已发布列表、当前配方 ID、发布/重发记忆；
- 外部模板文件夹自己的目标文字、ROI、阈值、字符框和资源参数。

旧格式处理固定为：

1. 先读取根对象和 `schemaVersion`。
2. `schemaVersion != 2` 时不逐字段迁移，不猜测旧含义。
3. 返回“旧格式需要重置”的结构化状态，由 UI 明确提示；用户确认后才用 Schema 2 默认值覆盖，用户取消则不写文件、不继续启动正式业务。
4. 已经是 Schema 2 但 JSON 损坏、字段类型错误或约束非法时，启动拒绝继续，不自动覆盖损坏文件，避免破坏诊断证据。

### 3.3 `detectionSchemes` 分区

检测方案不是独立文件，也没有独立 Schema 版本。它是 `app_settings.json` Schema 2 根对象中的一个必填分区。完整根结构如下：

```json
{
  "schemaVersion": 2,
  "camera": {},
  "inspection": {},
  "plc": {},
  "imageSaving": {},
  "ui": {
    "selectedDetectionMode": "word",
    "rightPanelSplitterStateBase64": ""
  },
  "detectionSchemes": {
    "stamp": {
      "templatePath": ""
    },
    "word": {
      "templatePaths": []
    },
    "ocr": {
      "templatePath": ""
    },
    "tissue": {
      "roughnessThreshold": 6.0
    },
    "barcodeWord": {
      "templatePaths": []
    }
  }
}
```

保存与校验规则：

- `stamp` 和 `ocr` 使用一个字符串字段 `templatePath`；空字符串表示尚未选择，不用单元素数组表达单选。
- `word` 和 `barcodeWord` 的 `templatePaths` 可以有多个元素；数组顺序用于界面展示、稳定加载和匹配分数相同时的优先级，运行时仍对全部有效模板并行匹配并取最高分。
- `tissue` 不允许出现 `templatePaths`。
- 所有模板路径必须是规范化绝对路径，并按 Windows 不区分大小写的规则去重。
- `detectionSchemes` 是 Schema 2 必填字段；不得为它增加第二个 `schemaVersion`。
- `app_settings.json` 不存在时，内存中使用完整应用默认值；第一次用户确认保存时再创建文件。
- Schema、字段类型、模式键或数值约束错误时拒绝加载整个应用设置，并报告具体字段；不做分区级静默忽略或局部默认回退。
- 所有分区统一执行“内存候选完整校验 → 序列化 → `QSaveFile` 写临时文件并提交”；不再额外实现第二套备份/恢复框架。提交失败时原文件保持不变。

### 3.4 “清空软件数据”的新语义

“清空软件数据”只清理并重建 `settings/app_settings.json`。

清空后：

- 整机设置恢复默认值；
- 五种模式的模板选择全部为空；
- 纸巾粗糙度阈值恢复为 `6.0`；
- 当前运行必须停止，相机和模板预览必须处于允许清空的安全状态。

清空操作绝不删除：

- 用户选择的任何外部模板文件夹；
- 旧 `<AppData>/recipes/` 和旧编辑工作区；
- 检测图片、日志、授权文件、模型、DLL、图标、样式、翻译和部署资源。

## 四、模板文件夹格式

### 4.1 标准目录

每个模板文件夹只表示一个模板：

```text
<任意外部模板文件夹>/
├─ template_settings.json
├─ template_raw.png
├─ tracking_template.bmp
├─ character_templates/
│  ├─ 0.png
│  ├─ 1.png
│  └─ ...
└─ template_ring.bmp             # 仅刚印检测需要
```

不同模式只要求自己实际需要的文件，不要求创建无用占位资源。图片和字符模板文件名沿用当前已经验证的模板语义，不改图片格式和内容。原 `calibrate_config.yaml` 中的三个区域点集并入 `template_settings.json`，新模板不再生成 YAML。

### 4.2 `template_settings.json`

通用结构示例：

```json
{
  "schemaVersion": 1,
  "detectionMode": "barcodeWord",
  "targetText": "20260819",
  "imageThresholdPercent": 70,
  "trackingRoi": {
    "x": 100,
    "y": 80,
    "width": 320,
    "height": 180
  },
  "regions": {
    "datePolygon": [
      { "x": -120.5, "y": -30.0 },
      { "x": 110.0, "y": -28.0 },
      { "x": 105.0, "y": 35.0 }
    ],
    "barcodePolygon": [
      { "x": -150.0, "y": -60.0 },
      { "x": -80.0, "y": -60.0 },
      { "x": -80.0, "y": 10.0 },
      { "x": -150.0, "y": 10.0 }
    ]
  },
  "characterSourceSize": {
    "width": 1920,
    "height": 1080
  },
  "characterBoxes": []
}
```

区域字段固定为：

| JSON 字段 | 适用模式 | 坐标语义 | 约束 |
|---|---|---|---|
| `regions.datePolygon` | 刚印、字库、深度 OCR、二维码+三期 | 相对定位 ROI 中心的原图像素偏移 | 必填；至少 3 个有限点 |
| `regions.barcodePolygon` | 仅二维码+三期 | 相对定位 ROI 中心的原图像素偏移 | 必填；恰好 4 点，顺序保持左上、右上、右下、左下 |
| `regions.stampPolygon` | 仅刚印检测 | 相对刚印环标定中心的原图像素偏移 | 必填；至少 3 个有限点 |

非适用模式不得保存无意义的 `barcodePolygon` 或 `stampPolygon`。JSON 数字读取后转换为现有 `cv::Point2f`，不得取整或改变坐标参考中心。

合并规则：

- `template_settings.json` 是区域点集的唯一磁盘来源。
- 新版不创建、不读取、不复制 `calibrate_config.yaml`。
- 不保留“先读 JSON、缺字段再读 YAML”的兼容路径。
- 区域字段缺失、类型错误、点数错误或存在非有限数时，统一返回 `TEMPLATE_REGION_INVALID`，编辑校验或启动预检按模式给出具体中文原因。
- 模板保存使用同一次目录事务提交 JSON、图片和字符资源，不再单独提交校准文件。

正式实施前必须从当前已验证代码逐字段反向核对并冻结 Schema，不能根据示例臆造或丢弃字段。字段归属固定为：

| 模式 | 模板文件夹中的私有内容 |
|---|---|
| 刚印检测 | 目标文字、图像匹配阈值、定位 ROI、日期区域、刚印区域、字符框、字符模板、刚印环资源及当前算法实际使用的模式参数 |
| 字库匹配 | 目标文字、图像匹配阈值、定位 ROI、日期区域、字符框、字符模板及当前算法实际使用的模式参数 |
| 深度 OCR | 目标文字、定位 ROI、日期区域及当前 OCR 流程实际使用的模板参数；不保存无消费者的字符匹配阈值 |
| 二维码+三期 | 目标文字、图像匹配阈值、定位/二维码/日期区域、字符框、字符模板、二维码选项及当前流程实际使用的模式参数 |
| 纸巾检测 | 不创建模板文件夹；私有参数直接保存在 `app_settings.json.detectionSchemes.tissue` |

### 4.3 编辑校验与运行校验

模板保存分为两级校验：

- 编辑校验：允许保存尚未制作完整的模板，便于用户稍后继续编辑；必须保证 JSON、模式 ID、字段类型、坐标和已有资源本身合法。
- 运行校验：启动检测前严格检查该模式所需的全部参数和资源；不完整模板不得进入正式运行快照。

区域点集属于结构化模板参数，不属于独立资源。编辑和运行校验都直接检查 `template_settings.json.regions`，不得再检查 YAML 文件是否存在。

模式必须匹配：例如字库模式不能选择 `detectionMode=stamp` 的模板。模式不匹配属于明确错误，不允许通过字段相似进行兼容猜测。

## 五、目标代码架构

### 5.1 删除配方模块

完整删除 `app/recipes/` 及以下概念在生产代码、工程清单和 UI 中的全部实现：

- `ProductRecipe`；
- `RecipeProfile`；
- `PreparedRecipe`；
- `RecipeStore`；
- `RecipeEditorSession`；
- `RecipeCatalog`；
- 配方 UUID 和显示名；
- 发布、重新发布、已发布列表、当前配方 ID；
- 配方选择对话框和按模式记忆已发布配方；
- `recipe.json` 的读取、写入和运行准备；
- 用户可见的“产品配方”“配方”“Profile”“发布”“重发”等旧流程用语。

只有历史文档、迁移说明和本计划中的删除说明可以继续出现这些词。正式程序不保留第二套配方路径。

### 5.2 最小模板模块：只保留两个代码文件

完整删除 `app/recipes/` 后，只新增以下目录：

```text
app/templates/
├─ README.md
├─ template_store.h
└─ template_store.cpp
```

`template_store.h` 集中定义与模板磁盘边界直接相关的少量类型，不再为每个类型创建文件：

```text
TemplateSettings       一个模板文件夹的可编辑私有参数
TemplateCharacterBox   一个字符框
PreparedTemplate       图片、ROI和字符资产加载完成后的只读运行模板
TemplateSummary        选择对话框显示的名称、路径、模式和有效状态
TemplateStoreError     错误码、用户提示和诊断信息
TemplateStore          模板文件夹加载、校验和统一安全保存
```

`TemplateStore` 只公开四类直接能力：

```text
readSummary(folderPath, expectedMode)
loadEditable(folderPath)
loadPrepared(folderPath, expectedMode)
save(folderPath, settings, resourceSources, preserveExistingContents)
```

实现边界固定为：

- 一个文件夹就是一个模板，`TemplateSettings` 不再包含 `profiles` 数组、配方 UUID、显示名或发布状态。
- `TemplateSettings` 直接包含 `datePolygon`、`barcodePolygon` 和 `stampPolygon`；`PreparedTemplate` 直接从这些字段构造只读点集。
- 模板显示名直接取文件夹名称。
- `TemplateStore` 不扫描全盘、不维护模板目录列表、不保存当前选择、不提供模板删除接口，任何方法都不得删除外部模板文件夹。
- `TemplateStore` 不访问 UI、相机、PLC、Runtime或具体 Pipeline。
- 运行资源准备直接由 `loadPrepared()` 完成，不再拆出 `PreparedTemplateStore` 或 Builder 文件。
- 新建/编辑过程的草稿、临时资源和工作区直接由现有 `TemplateApplicationService` 的私有成员维护，不创建 `TemplateEditorSession`。
- `save()` 始终执行第 8.1 节的同一套目录保存流程；`preserveExistingContents` 只决定临时目录是否先完整继承目标目录，不为已有编辑、新建和同名覆盖分别建立函数、策略类或第二套提交代码。
- 目录安全提交使用 `template_store.cpp` 内部私有辅助函数实现，不对外暴露通用事务框架。
- `TemplateStore` 不提供跨目录 `saveBatch()`；批量操作由应用服务循环调用同一个 `save()`，不建立分布式回滚。

### 5.3 单一 `AppSettings` 与单一设置 Store

```text
app/system_support/settings/
├─ app_settings.h
├─ app_settings.cpp
├─ app_settings_store.h
└─ app_settings_store.cpp
```

上述四个文件由当前 `machine_settings.h/.cpp` 和 `machine_settings_store.h/.cpp` 原位改名而来，文件数量不变。`AppSettings` 直接承载当前所有整机、UI和检测方案字段，不再保留名称不准确的 `MachineSettingsStore`，也不新增 `DetectionSchemeStore`。

为避免通用 Map 和大量小结构，检测方案按当前固定五模式明确建模：

```cpp
struct DetectionSchemes
{
    QString stampTemplatePath;
    QStringList wordTemplatePaths;
    QString ocrTemplatePath;
    double tissueRoughnessThreshold = 6.0;
    QStringList barcodeWordTemplatePaths;
};

struct AppSettings
{
    static const int CurrentSchemaVersion = 2;

    // 当前 MachineSettings 中已经验证的整机和 UI 字段原样保留。
    // 不在本阶段为了“分层好看”再拆 CameraSettings、PlcSettings 等文件。
    // ...
    DetectionSchemes detectionSchemes;
};
```

JSON 继续使用易读的 `camera/inspection/plc/imageSaving/ui/detectionSchemes` 分区；C++ 不为这些 JSON 分区逐个建立文件或 Store。

`AppSettingsStore` 保持简单的磁盘适配器职责：

```text
load(AppSettings *)
save(const AppSettings &)
settingsFilePath()
```

它负责完整 JSON 的严格解析、校验和 `QSaveFile` 原子替换，但不保存第二份长期内存状态，不提供每个字段一套 setter，也不读取模板文件夹。

`SettingsApplicationService` 是唯一内存正式值和草稿持有者：

- `current()` 返回最后一次成功落盘的完整 `AppSettings`。
- `draft()` 只服务整机设置页面。
- `applyDraft()` 只替换整机和 UI 可编辑字段，保留最新 `detectionSchemes`。
- `saveTemplatePaths(mode, paths)` 只替换一个模式的模板路径，并同步更新 `current` 与 `draft` 中的检测方案分区，防止后续应用整机草稿覆盖模板选择。
- `saveTissueThreshold(value)` 只替换纸巾阈值，并采用同一保存规则。
- `restoreDefaults()` 只恢复整机字段，保留检测方案。
- `clearSettings()` 重置完整 `AppSettings`。

每个保存操作都从 `m_current` 复制候选值、只修改目标字段、完整校验并写入一次；写入成功后才同时替换 `m_current` 和相应草稿字段。这样只有一个内存真源，不需要让两个 Service 共享一个“有状态 Store”。

### 5.4 复用现有两个应用服务，不新增第三个

保留并修改现有：

```text
SettingsApplicationService
TemplateApplicationService
```

不创建 `DetectionSchemeApplicationService`。

`SettingsApplicationService` 负责“当前选了哪些模板”和纸巾阈值，因为这些内容本来就是 `app_settings.json` 的设置字段。

`TemplateApplicationService` 只负责模板文件夹本身：

```text
检查用户选择的模板文件夹
开始新建或编辑一个模板
持有本次编辑草稿和临时工作区
暂存取景图片、ROI、字符框和字符图片
调用 TemplateStore 保存模板文件夹
调用 TemplateStore 加载可编辑模板
返回结构化错误，不弹对话框
```

现有 `RecipeEditorSession` 的状态直接并入 `TemplateApplicationService` 私有成员；现有 `RecipeStore` 和 `PreparedRecipe` 能力直接收口到共享的 `TemplateStore`。这不是把旧三个类机械合并成巨型公共接口，而是删除 UUID、Catalog、发布、重发、配方目录和多 Profile 容器后，仅保留模板编辑真实需要的线性流程。

模板选择确认的直接调用链固定为：

```text
TemplateSelectionDialog
  → TemplateApplicationService 检查路径和模板状态
  → SettingsApplicationService::saveTemplatePaths()
  → AppSettingsStore::save()
```

模板保存成功后的选择规则：

- 单模板模式：用新路径替换当前路径。
- 多模板模式：把新路径追加到列表末尾并保持顺序，同时让它成为“当前编辑模板”。
- 模板文件夹保存成功但设置保存失败时，保留已保存模板并明确提示“模板已保存，但未加入当前检测方案”；绝不删除用户模板来回滚设置。

多模板批量目标文字或阈值更新由 `TemplateApplicationService` 负责组织：先验证全部目标，再按当前路径列表顺序逐个调用 `TemplateStore::save()`。它只汇总每项结果，不在 `TemplateStore` 上增加批量接口，不跨目录回滚已经成功保存的模板。

“移除当前模板”不经过 `TemplateApplicationService` 或 `TemplateStore`。主窗口只从当前模式的正式路径列表中删除下拉框当前路径，再复用与选择对话框确认相同的 `SettingsApplicationService::saveTemplatePaths(mode, remainingPaths)`。因此它与取消勾选并确认是同一设置调用链，不形成第二套业务能力。

正式检测启动不经过模板编辑服务，避免把 UI 编辑状态带入运行链：

```text
InspectionApplicationService
  → SettingsApplicationService::current()
  → TemplateStore::loadPrepared()
  → 创建本次不可变运行快照
```

### 5.5 必要改名与明确不新增的类型

消除配方词汇后，只做真实语义需要的改名：

| 旧类型或字段 | 新类型或字段 |
|---|---|
| `ProductRecipe` / `RecipeProfile` | `TemplateSettings` |
| `PreparedRecipeProfile` | `PreparedTemplate` |
| `PreparedRecipeSnapshot` | 本次运行持有的 `QVector<PreparedTemplate>` 或等价只读快照 |
| `DetectionProfileSnapshot` | `DetectionTemplateSnapshot` |
| `ProfilePoseSelector` | `TemplatePoseSelector` |
| `WordTemplateProfile` | `EditableTemplate` |
| `DetectionModeDescriptor::recipeId` | `DetectionModeDescriptor::modeId` |
| `recipeId` / `activeRecipeId` | 模板路径、命中模板名称或当前模式，按真实含义分别替换 |

模板选择能力直接复用五模式描述表中现有的“无定位/单模板/多模板”分类，不再增加一套 `TemplateSelectionKind` 枚举和第二张模式映射表。UI、启动预检和设置校验必须查询同一份模式描述。

明确禁止为了重命名旧类型而新增以下替代包装：

```text
TemplateCatalog
TemplateGroup
TemplateRepository
TemplateManager
TemplateEditorSession
DetectionSchemeStore
DetectionSchemeApplicationService
PreparedTemplateBuilder
```

若当前代码还有仅因配方 UUID、Catalog、发布或重发存在的信号、字段和 DTO，直接删除，不做一比一改名保留。

### 5.6 全仓 `Recipe/recipe` 字段清零

阶段 8 规划基线（1.7 版盘点）中，`app` 下 `.h/.cpp/.ui/.pro` 有 59 个文件、1175 行仍命中大小写不敏感的 `Recipe/recipe`。实施子阶段 8A 必须以当时实际工作树重新盘点并记录最新数字；这项基线说明清理范围不只是删除 `app/recipes/`，还覆盖 application、contracts、detection、runtime、startup、settings、UI和 qmake 工程清单。

实施时不得全局机械执行 `Recipe → Template`，必须先判断字段表达的真实含义，再按下表处理：

| 旧语义或代表符号 | 处理方式 |
|---|---|
| `ProductRecipe`、`RecipeProfile` | 合并为单模板文件夹的 `TemplateSettings` |
| `PreparedRecipeProfile` | 改为 `PreparedTemplate` |
| `PreparedRecipeSnapshot`、`activePreparedRecipe` | 改为本次运行直接持有的有序 `QVector<PreparedTemplate>` 或等价只读模板快照，不新增包装层 |
| `RecipeStore` | 真实的模板磁盘能力收口到唯一 `TemplateStore` |
| `RecipeEditorSession` | 类型删除；必要草稿字段并入现有 `TemplateApplicationService` 私有成员 |
| `RecipeCatalog`、`TemplateRecipeCatalogEntry`、`recipes` 列表 | 直接删除；不建立模板 Catalog、最近列表或全盘扫描 |
| `recipeId`、`activeRecipeId`、`DetectionModeDescriptor::recipeId` | 若实际表示模式则改为 `modeId`；若实际表示外部模板则改为 `templatePath`；若仅为配方 UUID则删除 |
| `publishedRecipeIdsByMode`、`lastRecipeIdByMode`、`rememberPublishedRecipe` | 全部删除；当前选择只来自 `AppSettings::detectionSchemes` 的路径字段 |
| `recipeDirectoryPath` | 真正表示外部模板目录时改为 `templateFolderPath`；只服务 AppData 配方目录时删除 |
| `recipesRootPath` | 删除；新模板可位于任意外部目录，不设置统一模板根目录 |
| `editorWorkspacesRootPath` | 仅保留模板编辑确实需要的工作区能力，并改为 `templateEditorWorkspacesRootPath` |
| `TissueRecipeParameters` | 删除；纸巾私有阈值直接使用 `AppSettings::detectionSchemes` 中的明确字段 |
| `PreparedRecipeMissing` 等错误码 | 按实际失败原因改为模板未选、模板路径失效或模板资源无效；不保留配方缺失错误 |
| `isSingleTemplateRecipeMode` | 改为 `isSingleTemplateMode`，并继续复用唯一模式描述表 |
| `applyRecipeProfileToUi`、`refreshRecipeProfileDirty` 等编辑函数 | 改为 `applyTemplateSettingsToUi`、`refreshTemplateSettingsDirty` 等真实模板语义 |
| `selectPublishedRecipeForCurrentMode`、发布、重发相关函数 | 选择入口改为 `selectTemplatesForCurrentMode`；发布和重发函数直接删除 |
| `recipeSelection`、`recipeEditing` 等 UI 权限字段 | 改为 `templateSelection`、`templateEditing` |
| `[RECIPE_*]` 日志标签及配方错误文字 | 改为对应模板操作标签和中文模板错误，不保留隐藏的配方运行概念 |

文件名和 UI 对象名同步处理：

```text
app/recipes/*                              → 删除
recipe_selection_dialog.h/.cpp            → template_selection_dialog.h/.cpp
template_editor_recipe.cpp                → template_editor_storage.cpp
RecipeSelectionDialog                     → TemplateSelectionDialog
toolButton_selectRecipe                    → toolButton_selectTemplate
groupBox_currentRecipe                     → groupBox_currentTemplate
lineEdit_currentRecipeName                 → lineEdit_currentTemplateName
tab_recipeSettings                         → tab_templateSettings
其余 *recipeSettings* UI 对象              → 对应 *templateSettings* 名称
on_toolButton_selectRecipe_clicked         → on_toolButton_selectTemplate_clicked
动态 pushButton_selectPublishedRecipe      → 删除，统一使用 toolButton_selectTemplate
```

清理完成的判断不是“主要类已经改名”，而是生产目录中大小写不敏感的 `recipe` 命中为零，包括文件名、目录名、类型、成员、参数、局部变量、信号槽、UI 对象名、日志标签、注释、用户文案和 `.pro` 工程项。只有历史文档和本计划的删除说明允许保留该词。

## 六、正式运行流程

### 6.1 唯一链路

```text
用户选择检测模式
  → SettingsApplicationService 提供内存中的当前检测方案
  → TemplateStore 加载并严格校验所选模板文件夹
  → InspectionApplicationService 创建不可变模板与整机设置运行快照
  → InspectionRuntime 启动正式采集
  → DetectionRegistry 选择对应 Pipeline
  → Pipeline 使用 PreparedTemplate 或纸巾私有参数
  → ResultService 统一完成统计、PLC、存图和呈现
```

运行开始后不得重新读取 UI、`app_settings.json` 或外部模板目录。用户在检测过程中修改磁盘文件，不影响本次运行；修改结果只在下一次成功启动时进入新的运行快照。

### 6.2 单模板模式

刚印检测和深度 OCR：

- 未选择模板：启动预检失败。
- 路径不存在：启动预检失败并显示完整路径。
- 模式不匹配、JSON 损坏、字段非法或必要资源缺失：启动预检失败并列出原因。
- 只有严格校验通过后才创建运行快照。

### 6.3 多模板模式

字库匹配和二维码+三期：

1. 按 `templatePaths` 的保存顺序逐个加载；路径不存在、模式不匹配、内容损坏或运行资源不完整的模板不进入运行数组，其完整路径和失败原因进入独立警告列表。
2. 将全部有效模板紧凑放入同一个 `QVector<PreparedTemplate>` 或等价只读数组。例如所选路径为 `[A, 损坏的B, C]`，运行数组就是 `[A, C]`，临时下标自然为 `0、1`，不存在 `0、2` 的空洞。
3. 只要至少一个模板严格有效，启动前一次性警告“以下模板状态异常，已跳过加载”，列出每个异常模板的名称、完整路径和原因，然后继续启动；不再增加额外的停机、确认或降级保护逻辑。
4. 如果没有任何有效模板，启动失败并一次列出全部失败原因。
5. 每帧对运行数组中的全部有效模板并行执行定位匹配，比较全部结果后选择定位/匹配分数最高的模板；不得按列表顺序找到第一个成功项就提前结束。
6. 分数严格更高时才替换当前最佳项；分数相同则保留路径列表中较早的模板，使结果稳定可复现。
7. 命中结果中的临时下标只访问产生该匹配结果的同一个运行数组元素；`PreparedTemplate` 自身同时携带模板路径、名称、设置和资源，因此不会发生“模板索引与检测参数错位”。
8. 不持久化模板索引，不增加模板索引 JSON 字段、索引映射、ID或管理类。模板身份始终是规范化绝对路径，运行下标只是一次启动期间的内部实现细节。
9. 不允许静默调整已保存路径顺序，也不允许自动从设置中删除无效项。

### 6.4 纸巾模式

- 不加载 `TemplateStore`。
- 不检查模板路径、模板图片、字符目录或校准文件。
- 直接从 `AppSettings.detectionSchemes.tissueRoughnessThreshold` 的运行快照读取阈值。
- 纸巾阈值唯一默认值继续为 `6.0`，检测器内部不得再维护第二套业务默认值。

### 6.5 结果中的模板名称

- 单模板模式显示当前模板文件夹名称。
- 多模板模式显示实际命中的模板文件夹名称。
- 无定位或未命中时按当前已验证规则显示 `--` 或保留规定状态。
- 纸巾模式不显示模板名称。

本阶段不改变五种算法、并行匹配与最高分选择规则、其他判定逻辑、阈值语义、Overlay、正常统计、PLC 0/49→约 100 ms→0、存图策略或 Fault 合同。

## 七、统一模板选择与编辑 UI

### 7.1 模板选择对话框

单模板和多模板模式共用同一个 `TemplateSelectionDialog`，避免为每种模式维护不同入口。该对话框不扫描全盘，也不维护“所有已知模板”目录；打开时只加载当前模式已经保存在 `AppSettings::detectionSchemes` 中的路径，随后允许用户在本次对话框中继续加入外部模板文件夹。

打开 `toolButton_selectTemplate` 时的固定流程：

1. 根据当前模式从 `SettingsApplicationService::current().detectionSchemes` 取得当前有序路径列表。
2. `TemplateApplicationService` 调用 `TemplateStore::readSummary()` 检查每个路径，路径即使已经失效也必须保留为一行并显示错误，不能悄悄删除。
3. 将当前路径传给对话框；这些行初始全部打勾，用户一眼即可看出当前选择。
4. 用户通过“添加模板文件夹”选择新目录；校验通过后追加为已勾选行，Windows 路径按不区分大小写规则去重。
5. 用户取消某行勾选表示从最终检测方案中移除该模板路径，但在点击“确认”前只修改对话框内部草稿。

对话框表格固定显示：

| 列 | 内容 |
|---|---|
| 选择 | 复选框；当前已选模板初始打勾 |
| 顺序 | 已勾选模板的展示、加载和同分优先顺序；单模板固定为 1 |
| 模板名称 | 模板文件夹名称 |
| 完整路径 | 规范化绝对路径 |
| 状态 | 有效、不完整、路径不存在、模式不匹配、配置损坏等 |

统一按钮：

- 添加模板文件夹；
- 上移；
- 下移；
- 确认；
- 取消。

行为规则：

- 单模板模式仍使用同一对话框；勾选一行时自动取消其他行的勾选，最终允许 0 或 1 个模板。
- 多模板模式允许同时勾选多行；上移、下移调整行顺序，确认时只按界面顺序保存已勾选行。
- 添加时立即读取基础配置并检查模式；模式不匹配直接拒绝加入。
- 编辑校验通过但运行资源不完整的模板可以加入，并在状态列显示“不完整”；启动时仍由严格预检决定能否使用。
- 路径失效时显示红色错误状态，不自动从列表移除。
- 取消勾选只代表移除路径引用，永远不删除外部模板文件夹。
- “取消”丢弃整个对话框草稿，添加、取消勾选和排序均不改内存正式值与磁盘文件。
- “确认”收集已勾选行后，只调用一次 `SettingsApplicationService::saveTemplatePaths(mode, paths)`；保存成功才关闭对话框，保存失败保持原正式选择并显示错误。
- 允许确认空选择；没有模板时由开始检测前的统一预检提示并拒绝需要模板的模式，不在选择对话框中重复实现启动门禁。
- 未勾选行不写入设置，也不另存为候选模板；下次打开只显示当时正式选择的路径。用户需要重新选择已移除模板时，再次使用“添加模板文件夹”。这是不增加 Catalog 和最近列表的最简实现。

该选择功能本身属于低到中等复杂度：复用并原位改名现有两个对话框文件，不新增代码文件、不新增 Store 或 Service；主要改动是复选框草稿、单选互斥、多选排序、路径去重以及确认时的一次保存。预计对话框约重写或增加 120～220 行，主窗口和服务调用约调整 30～60 行。阶段 8 的主要复杂度仍是全仓配方类型清零和运行链替换，不是这个选择界面。

### 7.2 “当前编辑模板”

模板编辑区域把现有多 Profile 下拉框原位改为“当前编辑模板”，建议对象名为 `comboBox_currentEditTemplate`；在下拉框右侧增加“移除模板”按钮，建议对象名为 `toolButton_removeCurrentTemplate`。不使用“删除模板”文案，避免让用户误以为会删除外部文件夹；不增加窗口、控制器、模型类或配置字段。

固定规则如下：

- 下拉项只来自当前模式已经保存在 `detectionSchemes` 中的模板路径，顺序与选择对话框一致。
- 界面显示模板文件夹名称；每一项内部数据保存规范化绝对路径；工具提示显示完整路径，避免同名文件夹无法区分。
- 单模板模式最多只有一项；多模板模式允许用户从多个已选模板中指定当前要编辑的一个。
- 没有已选模板时显示“未选择模板”，并禁用模板编辑操作。
- 路径不存在、配置损坏或无法编辑的项仍显示并带“状态异常”标记；选中该项时禁用编辑并显示具体路径和原因，不静默切换到其他模板。“移除模板”仍可用于清理该路径引用，因为它不读取或修改模板目录。
- 点击“编辑模板”时，只加载下拉框当前项内部保存的绝对路径，不使用运行时命中模板，也不依赖临时数组下标。
- 切换检测模式时默认选择该模式路径列表中的第一项；模板选择列表变化后，如果原当前路径仍被选择则保留它，否则选择新列表第一项。
- 新建模板保存成功并加入多模板检测方案后，新模板自动成为当前编辑模板；单模板模式按新路径替换唯一项。
- “当前编辑模板”只是本次界面运行期间的临时 UI 状态，不写入 `app_settings.json`、`template_settings.json` 或其他文件，退出软件后无需恢复上一次下拉选择。
- 字库匹配和二维码+三期的批量目标文字、阈值操作始终作用于当前模式全部已选模板；它们不受下拉框当前项限制。
- “移除模板”只作用于当前模式下拉框当前项，效果与在模板选择对话框取消该项勾选并点击“确认”完全相同；不修改其他模式的选择，也不修改任何模板目录。
- 移除按钮复用现有 `templateEditing` 操作权限：检测运行中、没有当前项或模板编辑器存在未处理修改时禁用，不新增一套移除状态机。

点击“移除模板”后的固定流程：

1. 从当前模式正式路径列表复制候选列表，按 Windows 不区分大小写的路径规则移除下拉框当前项。
2. 直接调用与模板选择对话框确认相同的 `SettingsApplicationService::saveTemplatePaths(currentMode, remainingPaths)`；不新增 `removeTemplatePath()` 或其他移除接口。
3. 保存失败时保持正式路径列表、下拉框当前项和界面状态不变，并显示设置保存错误。
4. 保存成功后刷新当前模式下拉框：如果还有模板，选择剩余列表第一项；否则显示“未选择模板”并禁用编辑和移除。
5. 整个流程不调用 `TemplateApplicationService`、`TemplateStore`、`QDir` 或任何文件删除 API；被移除模板的文件夹和全部内容保持原样，用户以后可通过“添加模板文件夹”重新选择。

因为该操作可逆且不删除磁盘数据，不增加永久删除警告或目录安全分支；按钮工具提示固定为“从当前检测方案移除模板，不会删除模板文件夹”。

这一区分了两个不同概念：“当前编辑模板”决定单模板编辑器打开谁；“当前命中模板”是检测运行后最高分匹配结果。二者不得共用状态或互相覆盖。

### 7.3 各模式 UI 显隐

| 模式 | 模板相关 UI | 私有参数 UI |
|---|---|---|
| 刚印检测 | 显示选择、新建、编辑、移除、当前编辑模板；单模板 | 显示刚印所需目标、阈值、ROI、字符和刚印环编辑；隐藏批量模板编辑 |
| 字库匹配 | 显示选择、新建、编辑、移除、当前编辑模板和当前命中模板；多模板 | 显示目标、阈值、ROI、字符和批量模板参数编辑 |
| 深度 OCR | 显示选择、新建、编辑、移除、当前编辑模板；单模板 | 显示目标文字、ROI和校准相关编辑；隐藏字符匹配阈值、字符切割和批量模板编辑 |
| 纸巾检测 | 隐藏全部模板选择、模板名、新建、编辑、取景、冻结、ROI引导、字符和批量编辑 UI | 只显示粗糙度阈值以及通用整机、PLC、存图设置 |
| 二维码+三期 | 显示选择、新建、编辑、移除、当前编辑模板和当前命中模板；多模板 | 显示二维码、日期、字符、阈值和批量模板参数编辑 |

正式 UI 中不再出现“产品配方”“配方”“发布”“重新发布”“已发布配方”“Profile”等旧概念。需要表达多模板集合时统一使用“模板组”，表达单个资源时统一使用“模板”。

### 7.4 应用、放弃、恢复默认与清空

- 模板选择对话框打开时读取当前模式路径并预勾选；“确认”只保存当前模式已勾选的有序路径列表。
- 模板选择对话框的新增、取消勾选和排序均为局部草稿；“取消”后当前路径、勾选状态和顺序保持原样。
- 纸巾参数的“应用”保存到 `app_settings.json.detectionSchemes.tissue`。
- 模板编辑器的“保存”保存当前外部模板文件夹。
- 未应用参数确认、开始检测前的脏状态确认和取消规则继续保持当前已验证行为。
- “恢复默认设置”只恢复整机设置，不清空模板选择，不修改外部模板。
- “清空软件数据”按 3.4 节执行，重建唯一的 `app_settings.json`，但永不删除模板文件夹。

## 八、保存事务和失败恢复

### 8.1 单模板文件夹保存

已有模板编辑、新模板创建和同名模板覆盖全部调用同一个：

```text
TemplateStore::save(folderPath, settings, resourceSources, preserveExistingContents)
```

内部只有一套“准备临时目录 → 写入 → 校验 → 替换目标”的保存逻辑：

1. 校验目标绝对路径、模板设置和资源来源，在目标文件夹同级创建唯一临时目录。
2. `preserveExistingContents=true` 时，要求目标目录存在，并把目标目录全部内容完整复制到临时目录，包括程序不认识的附加文件；为 `false` 时临时目录保持为空，不复制旧内容。
3. 在临时目录中完整覆盖写入新的 `template_settings.json`，再覆盖本次提供的图片、字符模板和其他受管资源。
4. 只有编辑器明确标记删除的受管字符资源才从临时目录删除；继承模式下未涉及的资源和未知文件保持原样。
5. 从临时目录重新加载并执行编辑级完整校验。
6. 目标目录存在时先将其改名为唯一备份目录；目标不存在时直接进入下一步。
7. 将临时目录改名为正式目标目录。
8. 提交成功后递归删除备份；提交失败时恢复原目标目录，并报告任何残留临时目录或备份目录的完整路径。

三种调用场景只决定一个布尔输入，不拥有各自的保存实现：

| 调用场景 | `preserveExistingContents` | 调用前 UI 行为 | 同一保存流程的结果 |
|---|---:|---|---|
| 编辑“当前编辑模板” | `true` | 无覆盖提示 | 先继承完整旧目录，再覆盖发生变化的 JSON 和资源；未修改资源及未知文件保留 |
| 新建模板，目标不存在 | `false` | 无覆盖提示 | 从空临时目录构建完整新模板并提交 |
| 新建模板，目标已存在 | `false` | 显示模板名称和完整路径，明确警告旧目录全部内容将被新模板替换；用户确认后才调用 | 从空临时目录构建并校验，随后使用同一备份替换步骤覆盖目标；最终不残留任何旧内容 |

用户取消同名覆盖时不调用 `save()`，目标目录完全不变。`preserveExistingContents` 只是一次调用参数，不写入配置；禁止增加 `updateTemplate()`、`overwriteTemplate()`、第二套目录复制函数或保存策略类。任何保存失败都不得留下“JSON 已更新但图片未更新”或“部分字符模板已替换”的半成品正式目录。

### 8.2 移除当前模板引用

“当前编辑模板”右侧按钮只是模板选择对话框“取消当前项勾选并确认”的快捷入口，不是磁盘删除功能：

1. UI 从当前模式正式路径列表复制候选列表，移除下拉框当前路径。
2. 调用既有 `SettingsApplicationService::saveTemplatePaths(currentMode, remainingPaths)` 保存当前模式剩余的有序路径列表。
3. 保存成功后刷新下拉框，选择剩余第一项；没有剩余项时显示“未选择模板”。
4. 保存失败时正式设置和 UI 选择保持不变。

该流程与选择对话框确认共享同一个方法、同一个校验和同一个 `AppSettingsStore::save()`，不新增 `removeTemplatePath()`。它不访问模板文件夹，不检查模板是否损坏，也不调用任何删除、移动或写入文件 API。路径从当前模式移除后，其他模式的设置和外部模板目录均保持原样；用户可以随时再次添加该模板。

### 8.3 多模板批量参数保存（逐模板事务）

字库和二维码+三期的批量目标文字、阈值编辑作用于当前模式全部已选择模板，但不建立跨目录的“全有或全无”事务：

1. `TemplateApplicationService` 先加载并验证全部目标模板是否可以编辑，构造每个模板的新设置；预验证有任何失败时不开始写入，并一次列出全部问题。
2. 预验证全部通过后，按当前模板路径列表顺序逐个调用 `TemplateStore::save(..., preserveExistingContents=true)`。
3. 每个模板都独立执行第 8.1 节的同一目录保存流程；单个模板失败不会破坏它自己的上一完整版本。
4. 实际保存中任一模板失败时立即停止后续保存，不回滚此前已经成功提交的其他模板。
5. UI 必须明确分别列出“已成功”“保存失败”“尚未处理”的模板名称和完整路径，不得提示整批成功，也不得隐藏部分成功事实。

`TemplateStore` 不提供 `saveBatch()`，不维护跨目录备份集合、事务日志或分布式回滚。这个取舍让故障结果对用户透明，同时避免为了极少发生的跨文件夹原子性引入大量难以维护和恢复的代码。

### 8.4 检测方案保存

检测方案选择和第 8.2 节“移除模板”都只修改 `app_settings.json` 的 `detectionSchemes` 分区，不复制、不移动、不删除任何模板文件夹。`SettingsApplicationService` 必须基于唯一的 `m_current` 复制候选根对象、修改这一分区，再调用 `AppSettingsStore` 事务写入完整文件；不得从 UI 草稿或其他对象复制一份可能过期的完整设置。保存失败时：

- 内存正式选择继续使用上一份已成功保存的值；
- UI 显示结构化错误；
- 外部模板内容不回滚、不删除；
- 下次启动继续读取上一份完整有效的 `app_settings.json`。

### 8.5 单文件分区更新

单一文件不代表所有页面共用同一份草稿：

- 整机设置页面修改的值先进入 `SettingsApplicationService` 的设置草稿。
- 模板选择对话框确认后，只调用 `saveTemplatePaths(mode, ...)`。
- 保存模板选择时，不得把尚未应用的整机草稿写入磁盘。
- 应用整机设置时，只提交草稿中的整机与 UI 字段，必须保留 `m_current` 中刚保存的模板路径和纸巾阈值。
- “恢复默认设置”只替换整机分区，保留 `detectionSchemes` 和外部模板。
- “清空软件数据”调用 `clearSettings()`，一次重置整个 `AppSettings`。

所有分区写入最终都复用 `SettingsApplicationService` 的候选根对象构造和 `AppSettingsStore::save()`，既减少代码，也防止跨分区丢失更新。

## 九、阶段 8 分阶段实施计划

阶段 8 仍是一个对外开发阶段和一个最终交付轮次，但内部固定拆分为 `8A～8G` 七个实施子阶段。子阶段用于控制依赖、缩小排错范围和留下清晰静态证据，不代表七次用户验收，也不允许把临时双路径当作阶段成果长期保留。

```text
8A Schema与基线冻结
  → 8B AppSettings设置底座
  → 8C TemplateStore与模板编辑存储
  → 8D 启动、Runtime与Detection切换
  → 8E 统一模板UI切换
  → 8F Recipe彻底清零与工程收口
  → 8G 文档、总静态门禁与统一交付
  → 用户一次性 Run qmake、Rebuild 和人工回归
  → 验证通过后更新状态并创建一个本地提交
```

### 9.1 子阶段共同执行规则

1. 必须按 `8A → 8B → 8C → 8D → 8E → 8F → 8G` 顺序执行；前一子阶段退出门禁未通过时不得进入下一子阶段。
2. 每个子阶段开始时记录输入状态、涉及文件和预计删除项；结束时执行该子阶段静态检查，并把结果追加到阶段 8 执行记录。
3. 子阶段之间不要求用户运行 qmake、编译、测试或主程序；Agent 同样不得运行这些操作。
4. 子阶段之间不执行 `git add`、不创建临时阶段提交、不把功能状态改为`已验证`。全部差异作为同一个阶段 8 工作树连续累积，用户统一验证通过后只创建一个本地提交。
5. 8B～8E 期间允许旧 Recipe 类型在尚未迁移的调用链中短暂存在，但只能是当前阶段工作树中的受控过渡状态：不得新增兼容开关、不得形成可交付的双路径、不得继续扩展旧类型，最迟在 8F 全部删除。
6. 每个子阶段都必须保持“当前已修改范围”内部声明、定义、include、对象名和工程项自洽；如果因跨子阶段改名导致整个工程暂时不能静态闭合，必须在同一子阶段继续完成必要调用方修改，不能把明显断裂留给下一阶段。
7. 子阶段失败时停留在当前子阶段修复；如果遇到 Schema、算法、资源归属或用户修改冲突，保留现场并向用户报告，不自行扩大范围。
8. 只有 8G 总静态门禁通过后才交付用户统一验证。用户验证失败后的修复仍属于阶段 8，不新增 8H 或另一套兼容实现。

### 9.2 子阶段 8A：冻结 Schema、调用链与基线

目标：在修改生产代码前，把“保存什么、谁读取、如何校验、哪些符号必须删除”一次确定，避免后续边改边猜。

主要工作：

- 从五种模式、模板编辑器、设置页面、启动预检、运行快照和 Pipeline 反向盘点全部真实字段、默认值、单位、资源和消费者。
- 更新《OCRGangYin 新架构数据 Schema》，冻结 `AppSettings` Schema 2、五模式 `detectionSchemes`、`TemplateSettings` Schema 1、`PreparedTemplate`、资源清单、坐标语义和错误码。
- 明确 `readSummary()`、编辑校验和运行校验分别读取、检查到什么程度；区域点集只来自 `template_settings.json.regions`。
- 列全实际受影响功能 ID；预计至少覆盖 `SYS-007、UI-001..002、SET-001..013、TPL-001..016、DET-001..008、CAM-003..004、RUN-001..006、RES-005`，实际以代码追踪为准。
- 重新统计 `Recipe/recipe` 命中文件数、命中行数、生产 `.h/.cpp` 总数、当前 Recipe 文件和资源调用点，形成阶段 8 最新清理基线。
- 把每个 Recipe 命中归类为“直接删除、模式 ID、模板路径、模板设置、运行模板、历史文档允许保留”，禁止全局机械替换。

退出门禁：

- Schema 中每个字段都有唯一归属、唯一默认值、校验规则和实际消费者；示例字段不能冒充已冻结字段。
- 四种模板模式和纸巾模式的资源要求已分别列清；单模板、多模板和无模板规则无冲突。
- 所有未决问题已由源码、现有已验证行为或用户决定消除；仍有未决项时阶段 8 停在 8A。
- 已记录开始 HEAD、阶段开始前用户修改和本阶段不得触碰的资源文件；8A 不修改生产代码。

### 9.3 子阶段 8B：建立唯一 AppSettings 设置底座

目标：先解决“软件设置保存在哪里、谁持有正式值”，让后续模板选择和纸巾参数只有一个可靠写入口。

主要工作：

- 将 `machine_settings.h/.cpp`、`machine_settings_store.h/.cpp` 原位改名为 `app_settings.h/.cpp`、`app_settings_store.h/.cpp`，同步修改全部 include、类型名和 qmake 项，不复制并行文件。
- `AppSettings` 保留已验证整机/UI字段并加入固定五模式 `DetectionSchemes`；不建立通用 Map、独立 Schema 或动态模式层。
- `AppSettingsStore` 实现 Schema 2 严格读取、完整校验和 `QSaveFile` 原子保存；旧 Schema 只支持用户确认后整体重置，损坏的 Schema 2 不自动覆盖。
- `SettingsApplicationService` 成为唯一完整内存正式值和设置草稿持有者，完成 `current/draft/applyDraft/saveTemplatePaths/saveTissueThreshold/restoreDefaults/clearSettings` 的分区更新语义。
- 所有保存都从 `m_current` 复制候选根对象，只修改目标分区；模板路径更新不能提交未应用整机草稿，应用整机草稿不能覆盖最新检测方案。

退出门禁：

- 生产代码中 `MachineSettingsStore`、旧文件名和 `detection_schemes.json` 为零；设置磁盘入口只有 `AppSettingsStore`。
- `detectionSchemes` 包含且只包含五种固定模式，纸巾无模板路径，单模板字段不用数组，多模板顺序可持久化。
- `SettingsApplicationService` 之外没有第二份可独立写盘的完整 `AppSettings` 长期状态。
- 本子阶段不创建 DetectionScheme Store、Service、Repository 或兼容读取层；`git diff --check` 对累计差异通过。

### 9.4 子阶段 8C：建立 TemplateStore 与模板编辑存储

目标：把一个外部模板文件夹的读取、两级校验和安全保存收敛到两个生产代码文件。

主要工作：

- 新增且只新增 `app/templates/template_store.h/.cpp`；在同一头文件集中定义 `TemplateSettings`、`TemplateCharacterBox`、`PreparedTemplate`、`TemplateSummary` 和 `TemplateStoreError`。
- 实现且只公开 `readSummary/loadEditable/loadPrepared/save` 四类直接能力，不扫描模板库、不保存当前选择、不访问 UI/Runtime/设备。
- `template_settings.json` 统一保存目标文字、阈值、ROI、字符框和三个区域点集；删除新路径中的 YAML 读取、写入、复制和回退。
- `TemplateStore::save()` 使用一套“临时目录 → 写入 → 校验 → 备份替换 → 失败恢复”实现；`preserveExistingContents` 只决定是否先完整继承旧目录。
- `TemplateApplicationService` 直接持有编辑草稿和临时工作区，调用 `TemplateStore` 完成新建、编辑、同名覆盖和逐模板批量保存；不新增 Session 或批量 Store 接口。
- 多模板批量操作先全量预验证，再按路径顺序逐项 `save()`；失败后报告已成功、失败和未处理项，不跨目录回滚。

退出门禁：

- `app/templates/` 只有两个生产代码文件；无 TemplateRepository、TemplateManager、TemplateEditorSession、PreparedTemplateBuilder 或 `saveBatch()`。
- 编辑、新建和同名覆盖的所有调用都进入同一个 `save()` 和同一个目录提交函数；没有 `updateTemplate()`、`overwriteTemplate()` 或复制粘贴的第二套流程。
- 编辑保存保留未修改资源和未知文件；确认同名覆盖不继承旧内容；保存失败能够保留上一完整正式目录。
- 模板新路径不依赖 `calibrate_config.yaml` 或 OpenCV `FileStorage`；本子阶段累计差异通过 `git diff --check`。

### 9.5 子阶段 8D：切换启动、Runtime 与 Detection

目标：让正式检测完全消费 `AppSettings + PreparedTemplate`，不再依赖配方、Profile 或模板编辑器状态。

主要工作：

- `InspectionApplicationService` 从 `SettingsApplicationService::current()` 取得当前检测方案，并直接调用共享 `TemplateStore::loadPrepared()` 创建不可变运行快照。
- 将 application、contracts、runtime、detection 中的运行输入从 Recipe/Profile 类型改为 `PreparedTemplate` 有序列表或纸巾阈值快照；按真实语义处理 `recipeId`，不机械改名。
- 刚印和深度 OCR 严格要求一个完整有效模板；纸巾完全不调用 `TemplateStore`。
- 字库和二维码+三期按保存顺序加载，坏模板一次性警告并跳过，至少一个有效模板即可继续，全部失效才拒绝启动。
- 有效模板形成紧凑数组，每个元素自带路径、名称、设置和资源；不保存模板索引字段或平行参数数组。
- 每帧评估全部有效模板并取最高分，不允许首个成功提前返回；分数相同保留路径列表中较早模板。
- Runtime、相机、正式帧队列、结果服务、统计、PLC、存图和 Fault 的已验证边界保持不变。

退出门禁：

- 正式启动链固定为“当前 AppSettings → TemplateStore → 不可变运行快照 → Runtime/Pipeline”，不经过 TemplateApplicationService 或 UI。
- application 启动链、contracts、runtime 和 detection 中不再消费 ProductRecipe、PreparedRecipe 或 RecipeProfile。
- 多模板不存在顺序短路、持久化索引、索引 Map 或参数错位；命中信息来自产生最高分的同一 `PreparedTemplate`。
- 纸巾启动链不访问模板路径、模板图片、字符目录或校准文件；累计差异通过 `git diff --check`。

### 9.6 子阶段 8E：切换统一模板 UI

目标：用户只看到“检测方案、模板文件夹、当前编辑模板”，所有模板模式复用一套选择和编辑入口。

主要工作：

- 将 `recipe_selection_dialog.h/.cpp` 原位改名为 `template_selection_dialog.h/.cpp`，打开时读取当前模式正式路径并全部预勾选。
- 对话框支持添加、取消勾选、排序、确认和取消；单模板勾选互斥，多模板保存勾选行顺序，纸巾不能打开。
- “确认”只调用一次 `SettingsApplicationService::saveTemplatePaths()`；“取消”不修改正式内存值或磁盘。
- 将多 Profile 下拉框原位改为“当前编辑模板”，从当前模式路径列表生成；编辑入口使用当前项绝对路径，不使用运行命中项。
- 增加 `toolButton_removeCurrentTemplate`，只构造剩余路径并复用 `saveTemplatePaths()`；不调用 TemplateApplicationService、TemplateStore 或文件 API。
- 批量目标文字和阈值仍作用于当前模式全部已选模板；当前编辑项只决定单模板编辑目标。
- 纸巾模式隐藏全部模板 UI；其他模式按唯一检测模式描述表显示无模板/单模板/多模板能力。
- 删除发布、重发、已发布列表和重复选择入口，保持现有布局风格、操作权限矩阵及已确认中文显示。

退出门禁：

- UI 对象名、槽函数和正式文案中不再出现配方、发布、重发或 Profile 语义；选择、新建、编辑和移除入口只有一套。
- 已选路径预勾选、取消不保存、确认后持久化、单选互斥、多选排序和无效路径保留显示的调用链明确。
- “当前编辑模板”不写任何配置字段；“移除模板”与取消勾选确认共享同一个设置入口且不删除目录。
- 纸巾模式不存在可见或可触发的模板操作；累计差异通过 `git diff --check`。

### 9.7 子阶段 8F：Recipe 清零与工程收口

目标：删除全部旧生产路径，使模板方案成为唯一正式实现，而不是与配方方案并存。

主要工作：

- 删除整个 `app/recipes/` 及 ProductRecipe、RecipeProfile、PreparedRecipe、RecipeStore、RecipeEditorSession、RecipeCatalog。
- 删除 `recipe.json`、AppData recipes、旧编辑工作区、UUID、发布/重发、Catalog、配方记忆和兼容读取的生产调用链。
- 按 5.6 节真实语义删除或改名剩余类型、字段、参数、局部变量、错误码、日志标签、注释、文件名和 UI 对象名。
- 将 `template_editor_recipe.cpp` 原位改名为 `template_editor_storage.cpp`，不保留复制文件。
- 更新 `AutoOCRproject.pro`、include、前置声明、成员和信号槽；删除 recipes 工程项并加入 templates 文件，不创建或恢复测试工程。
- 删除 `app/recipes/README.md`，新增 `app/templates/README.md`；修正直接受影响目录 README 的当前调用链。
- 核对资源差异：除生产代码停止使用 YAML 外，不修改或删除任何图片、模型、DLL、图标、QSS/CSS、翻译、`.qrc` 或用户外部模板。

退出门禁：

- `app/recipes/` 不存在；`app` 的 `.h/.cpp/.ui/.pro`、文件名、目录名和 UI 对象名中大小写不敏感 `recipe` 零命中。
- `calibrate_config.yaml`、OpenCV `FileStorage` 校准读写、旧 AppData recipes/工作区生产访问和旧配方 UI 文案零命中。
- qmake 清单中的文件全部存在、没有重复、没有已删项；`app/templates/` 只有两个生产代码文件。
- `app` 生产 `.h/.cpp` 不超过 151 个，仓库没有新增 `tests/` 或测试 qmake；累计差异通过 `git diff --check`。

### 9.8 子阶段 8G：文档、总静态门禁与统一交付

目标：证明 8A～8F 已形成一套完整、无旧路径、无计划外资源变化的最终差异，再一次性交付用户验证。

主要工作：

- 更新架构终局方案、数据 Schema、开发者代码结构与维护指南以及 application/runtime/ui/settings/templates 等目录 README。
- 更新功能对照表的实际受影响 ID，在用户验证前保持为`迁移中`；“移除当前模板”仍归入 `TPL-008/TPL-010/TPL-016`，不新增功能 ID。
- 在重构执行记录写入 8A～8G 的修改、删除、静态证据、开始 HEAD、用户文件隔离和统一验证清单。
- 完整执行第十一节全部 Agent 静态门禁，而不是只复用各子阶段的局部检查。
- 逐项对照第十节验证矩阵和第十二节人工步骤，整理为一次用户交付说明。

退出门禁：

- 第十一节 22 项静态门禁全部有明确结果；任何一项失败都退回对应子阶段修复，不能交给用户用构建碰运气。
- 工作树只包含阶段 8 差异和已记录的阶段前用户修改；不触碰、不回退、不误暂存其他文件。
- Agent 明确记录未运行 qmake、编译、测试目标或主程序；功能状态仍为`迁移中`，尚未创建阶段 8 提交。
- 达到以上条件后，才让用户一次性执行 Run qmake、Rebuild 和第十二节人工回归。

用户确认统一验证通过后，不新增实施子阶段：将受影响功能恢复为`已验证`，更新最终证据，再执行一次静态门禁和 cached diff 检查，精确暂存阶段 8 文件并创建一个本地提交；不自动推送。

### 9.9 各子阶段依赖和交付物总表

| 子阶段 | 前置依赖 | 核心交付物 | 允许的临时状态 | 进入下一阶段的硬门禁 |
|---|---|---|---|---|
| 8A | 当前计划与已验证代码 | 冻结 Schema、调用链、功能 ID、清理基线 | 不修改生产代码 | 无未决字段或资源归属 |
| 8B | 8A | AppSettings、AppSettingsStore、设置分区更新 | 未迁移调用链仍可保留 Recipe | 设置只有一个正式值和磁盘入口 |
| 8C | 8A、8B | TemplateStore、模板编辑存储、统一保存 | 旧运行链尚未切换 | 模板磁盘能力只有两个文件和一个保存实现 |
| 8D | 8B、8C | PreparedTemplate 运行快照、五模式启动和匹配 | UI 仍可暂时是旧入口 | 正式运行链不再消费 Recipe/Profile |
| 8E | 8B、8C、8D | 统一模板选择、当前编辑模板、移除引用 | 旧 Recipe 文件尚待物理删除 | UI 生产入口和文案已经切换为模板语义 |
| 8F | 8B～8E | Recipe 零引用、文件删除、qmake 收口 | 不允许任何 Recipe 生产残留 | 零命中、文件数和资源差异门禁通过 |
| 8G | 8A～8F | 文档、总静态证据、统一验证说明 | 功能仍为迁移中，尚未提交 | 第十一节全通过后才交付用户 |

### 9.10 计划内代码文件增删结果

实施完成时，直接相关文件应收敛为：

```text
删除  app/recipes/product_recipe.h/.cpp
删除  app/recipes/prepared_recipe.h/.cpp
删除  app/recipes/recipe_store.h/.cpp
删除  app/recipes/recipe_editor_session.h/.cpp

新增  app/templates/template_store.h/.cpp

改名  machine_settings.h/.cpp       → app_settings.h/.cpp
改名  machine_settings_store.h/.cpp → app_settings_store.h/.cpp
改名  recipe_selection_dialog.h/.cpp → template_selection_dialog.h/.cpp
改名  template_editor_recipe.cpp     → template_editor_storage.cpp

保留并修改  template_application_service.h/.cpp
保留并修改  settings_application_service.h/.cpp
保留并修改  inspection_application_service.h/.cpp
```

上述清单的目标是“删除 8、增加 2，其余原位改名或修改”，而不是先新增并行实现再把旧文件留到以后。阶段 8 交付用户验证前，配方生产路径必须已经为零。

## 十、验证矩阵（不新增测试代码文件）

仓库当前没有 `tests/`，并且此前已经明确删除测试工程。本阶段不为本次重构恢复 QtTest、Fake 框架或测试 qmake 文件，以免在取消配方的同时重新增加一套维护代码。下列项目仍是强制验证内容，但分为 Agent 静态核对和用户最终人工回归。

### 10.1 设置与检测方案

- 缺失 `app_settings.json` 使用完整默认值。
- Schema 2 能保存并重载当前模式、整机设置、模板路径顺序和纸巾阈值。
- 单模板模式最多一个路径，多模板保持用户排序，Windows 路径按不区分大小写规则去重。
- 纸巾模式不生成、不读取模板路径。
- 保存模板选择不提交尚未应用的整机草稿。
- 应用整机设置不丢失刚保存的模板路径、顺序和纸巾阈值。
- 恢复整机默认值保留检测方案；清空软件数据重置完整设置。
- 设置保存失败时，内存正式值和磁盘旧文件均保持不变。
- 旧 Schema 不兼容重置，损坏的 Schema 2 拒绝且不覆盖诊断证据。
- 打开模板选择对话框时，当前模式已经保存的所有路径按原顺序出现并全部预勾选。
- 新增模板、取消勾选或排序后点击“取消”，再次打开仍恢复原正式选择和顺序。
- 新增模板、取消勾选或排序后点击“确认”，退出重启后恢复新的勾选结果和顺序。
- 单模板模式勾选新行时旧行自动取消勾选；多模板模式允许多行同时勾选。
- 全部取消勾选并确认后保存空选择；需要模板的模式在开始检测时统一拒绝，不由对话框擅自恢复旧选择。
- “当前编辑模板”随当前模式已选路径刷新，保持同一路径或回到第一项，但不在 JSON 中产生记忆字段。
- 点击“移除模板”后，只从当前模式路径列表移除当前项，效果与选择对话框取消勾选并确认一致；其他模式和外部模板目录保持不变。
- 移除操作直接复用 `saveTemplatePaths()`；设置保存失败时正式值和下拉框保持不变，模板目录无论成功失败都不改变。

### 10.2 模板文件夹

- 刚印、字库、深度 OCR、二维码+三期分别加载一个合法模板。
- JSON字段、模式、图片、ROI、三个区域点集、字符框和字符资源按真实模式要求校验。
- 日期区域至少3点；二维码区域恰好4点；刚印区域至少3点；坐标参考中心和浮点精度保持。
- 编辑级允许合法但未制作完整的模板；运行级拒绝不完整模板。
- 模式不匹配、路径不存在、文件损坏和资源缺失都有明确中文错误及完整路径。
- 单模板目录保存失败后仍能读取上一完整版本。
- 仅修改已有模板阈值、目标文字或单项资源后，所有未修改资源和程序未知的附加文件仍完整保留。
- 新建同名模板选择“取消”时原目录逐文件不变；选择“确认”后最终目录只包含新模板完整内容，旧模板和未知附加文件全部清除。
- 同名完整覆盖提交失败时恢复旧模板；成功前不直接删除旧目录。
- 编辑、新建和同名覆盖均经过同一个 `TemplateStore::save()` 与同一目录提交函数，只由 `preserveExistingContents` 决定是否复制原目录。
- 多模板批量参数保存预验证失败时不写任何模板；实际保存中途失败时，已成功模板保持成功，失败模板保持上一完整版本，后续模板不处理，并准确显示三类清单。

### 10.3 五模式启动

- 刚印和深度 OCR：单模板成功、未选择、路径失效和资源不完整。
- 字库和二维码+三期：单个失效时一次性警告具体名称、路径和原因并跳过；只要仍有有效模板就继续启动，全部失效时拒绝启动。
- 多模板运行数组只紧凑包含有效模板，不存在持久化索引或空洞；命中名称、路径、设置和资源始终来自同一个 `PreparedTemplate`。
- 每帧并行匹配全部有效模板并选择最高分；不能按列表顺序首个成功即停止；分数相同保留列表中较早模板。
- 纸巾：模板 UI 隐藏，启动不访问 `TemplateStore`，只使用粗糙度阈值。
- 运行快照形成后，检测过程中修改 `app_settings.json` 或模板文件不影响本次运行。
- 停止并再次启动后才重新读取最新检测方案和模板。

### 10.4 旧配方清零

- 新版不创建、读取或写入 `recipe.json`、`recipes/`、旧编辑工作区或配方 UUID。
- 新版不创建、读取、复制或写入 `calibrate_config.yaml`，区域点集只来自 `template_settings.json`。
- 正式 UI 不再出现产品配方、配方、发布、重新发布、已发布等流程。
- 旧 AppData 数据留在磁盘但无生产引用。
- `.pro`、include、前置声明、成员、信号槽和文档当前架构树均不再登记旧配方类型。
- 生产文件内容、文件名、目录名和 UI 对象名中大小写不敏感的 `recipe` 均为零，不以旧注释、旧日志或临时变量形式残留。

## 十一、Agent 静态门禁

交付用户统一验证前，Agent 必须至少确认：

1. `app/recipes/` 不存在。
2. 对 `app` 的 `.h/.cpp/.ui/.pro` 执行大小写不敏感的 `recipe` 搜索，结果为零；以下代表性生产符号也必须为零：

```text
ProductRecipe
PreparedRecipe
RecipeProfile
RecipeStore
RecipeEditorSession
RecipeCatalog
publishedRecipeIdsByMode
lastRecipeIdByMode
recipeId
RecipeSelectionDialog
toolButton_selectRecipe
recipeSelection
recipeEditing
```

3. `DetectionSchemeStore`、`DetectionSchemeApplicationService`、`TemplateEditorSession`、`MachineSettingsStore`、`detection_scheme_store.h/.cpp` 和 `machine_settings_store.h/.cpp` 为零；设置层只保留统一 `AppSettingsStore`。
4. 生产代码不创建、读取或写入 `detection_schemes.json`。
5. 生产代码不再读取或写入 `recipe.json`、AppData `recipes/` 和旧 `editor-workspaces/`。
6. `calibrate_config.yaml`、OpenCV `FileStorage` 校准读写、`calibration`资源角色及校准文件错误提示为零。
7. 正式 UI 中“产品配方、配方、发布、重新发布、已发布、Profile”等旧流程文案为零。
8. 五种模式的无模板/单模板/多模板分类只来自现有唯一检测模式描述表；没有新增第二套枚举或映射。
9. `SettingsApplicationService` 是唯一完整 `AppSettings` 内存正式值持有者；`AppSettingsStore` 不持有长期副本，其他服务没有可独立写盘的完整设置副本。
10. 主 qmake 清单中的文件都存在、无重复、无已删配方项；文件名、目录名和 qmake 条目中大小写不敏感的 `recipe` 为零；仓库不新增 `tests/` 和测试 qmake。
11. 除本计划明确取消的 YAML 外，无资源文件、模型、DLL、图片、图标、QSS/CSS、翻译、`.qrc` 或用户外部模板差异。
12. 所有本阶段文本严格 UTF-8、无尾随空白并有文件末尾换行。
13. `git diff --check` 通过，暂存清单不混入阶段开始前的用户修改和本阶段范围外的未跟踪 README。
14. `app/templates/` 只有 `template_store.h/.cpp` 两个生产代码文件；`app` 总代码文件数不超过 151。
15. `TemplateCatalog/TemplateGroup/TemplateRepository/TemplateManager/PreparedTemplateBuilder` 等禁止包装类型为零。
16. 生产代码中 `TemplateStore::saveBatch` 和对模板 Store 的 `saveBatch(` 调用为零；不存在跨模板文件夹事务日志、批次备份管理器或全局回滚类。
17. `AppSettings`、`template_settings.json` 和其他持久化 Schema 中不存在模板索引字段；生产代码不新增模板索引 Map、索引管理器或以索引作为模板身份的接口。
18. 字库和二维码+三期的定位链会评估全部有效模板并取最高分；不存在“第一个成功即返回”的顺序短路路径，同分规则保持较早路径优先。
19. “当前编辑模板”只从当前模式路径列表构建；不存在相应 AppSettings 字段或模板 JSON 字段，编辑入口按下拉项的绝对路径加载。
20. `TemplateStore` 只有一个模板保存入口和一套目录提交实现；`updateTemplate()`、`overwriteTemplate()`、保存策略类或复制粘贴的第二套更新/覆盖流程为零，所有调用都明确传入 `preserveExistingContents`。
21. `toolButton_removeCurrentTemplate` 的处理函数只复制当前模式路径列表、移除当前项并调用 `saveTemplatePaths()`；不调用 `TemplateApplicationService`、`TemplateStore`、`QFile`、`QDir` 或文件删除 API。
22. `toolButton_deleteCurrentTemplate`、`SettingsApplicationService::removeTemplatePath()`、`TemplateStore::remove()` 和模板永久删除文案为零；选择对话框取消勾选与下拉框移除按钮共享同一个设置保存入口。

## 十二、用户最终统一验证

Agent 完成阶段 8 全部差异后，用户在 Qt Creator 一次性执行：

1. 打开主工程，执行 Run qmake。
2. Rebuild 主程序。
3. 使用已有旧 `app_settings.json` 首次启动，确认出现格式重置提示并能够重新配置整机参数。
4. 依次切换五种模式，核对模板 UI 显隐和单选/多选能力。
5. 分别在单模板和多模板模式打开选择对话框，确认当前已经选择的模板按原顺序显示并打勾。
6. 新增模板、取消已有勾选和调整顺序后点击“取消”，再次打开确认原选择完全未变。
7. 重复新增、取消勾选和排序后点击“确认”，退出重启后确认新选择、勾选状态和顺序正确恢复。
8. 单模板模式勾选新模板时确认旧模板自动取消；多模板模式确认可以同时勾选多项。
9. 检查“当前编辑模板”：单模板模式只有一项，多模板模式可切换多项；“编辑模板”打开当前下拉项而不是上次命中项。
10. 切换检测模式时确认默认选择该模式第一项；在同一模式修改模板列表后，原当前路径仍存在时继续选中，已移除时回到第一项；无模板时显示“未选择模板”并禁用编辑，退出重启后无需恢复上次下拉项。
11. 刚印检测：新建、保存、选择一个模板，启动、停止、再次启动。
12. 字库匹配：选择多个模板，确认全部有效模板都参与匹配并由最高定位分模板胜出，命中模板名称正确；构造同分条件时确认列表中较早模板胜出。
13. 深度 OCR：选择一个模板，验证目标文字、定位和代表帧检测。
14. 纸巾检测：确认无任何模板 UI，设置粗糙度阈值并启动检测。
15. 二维码+三期：选择多个模板，验证即时读码、全部模板并行匹配、最高分选择和代表帧检测。
16. 打开四种模板文件夹，确认区域点集存在于 `template_settings.json.regions`，且目录中没有 `calibrate_config.yaml`。
17. 分别制造日期点数不足、二维码不是4点、刚印点数不足，确认保存或启动被拒绝且提示具体区域原因。
18. 多模板模式加入一个失效模板：确认启动前一次警告其名称、完整路径和原因，随后跳过并使用其他有效模板；全部失效时确认阻止启动。
19. 在失效模板前后各放一个有效模板，确认跳过后命中模板的名称、路径、参数和资源仍完全对应，不出现索引错位。
20. 编辑已有模板时只修改阈值或目标文字，确认未修改图片、字符资源和额外附加文件均保留。
21. 新建一个与已有模板同名的模板：先取消覆盖并确认旧目录完全不变；再次确认覆盖并确认旧目录全部内容被清除，最终只剩完整新模板内容。
22. 对多模板批量目标文字或阈值先制造预验证失败，确认没有模板被写入；再制造保存中途失败，确认界面准确列出已成功、失败和未处理模板，且失败模板上一完整版本仍可读取。
23. 退出并重启，确认当前模式、模板路径顺序、区域点集和纸巾阈值恢复正确。
24. 执行“恢复默认设置”，确认整机字段恢复默认，但模板选择和纸巾阈值保持。
25. 执行“清空软件数据”，确认唯一 `app_settings.json` 的整机、UI和检测方案全部重置，但所有外部模板文件夹仍存在且内容未改变。
26. 故意取消模板选择、取消编辑和制造一次无效模板保存，确认正式设置与上一完整模板不被破坏。
27. 回归软触发、硬触发、停止/重启、统计、存图、PLC正常合同、Fault提示和正常退出。
28. 在模板选择对话框取消勾选一个模板，确认只移除当前模式引用、外部模板目录仍存在；重新添加后能够继续使用。
29. 在“当前编辑模板”选择一项并点击“移除模板”，确认只从当前模式选择中移除、下拉框回到剩余第一项，其他模式选择和外部模板文件夹全部不变；重新添加后仍能正常使用。
30. 对路径不存在或配置损坏的当前项点击“移除模板”，确认仍能正常清理当前模式引用，且不会出现文件删除或目录安全提示。
31. 制造一次 `app_settings.json` 保存失败，确认正式路径列表和下拉框当前项保持原样，外部模板文件夹没有任何变化。

用户确认全部通过后，Agent 才能：

- 将受影响功能从 `迁移中`恢复为`已验证`；
- 更新执行记录中的最终证据；
- 执行最后一次静态门禁；
- 精确暂存阶段 8 自己的文件并创建一个本地提交；
- 报告提交哈希和标题，不自动推送。

真实 PLC 在线读写、机械剔除和现场异常恢复继续单列待验，不得用无 PLC、Fake 或普通主程序回归冒充完整现场生产验收。

## 十三、不在本阶段修改的内容

- 不改变五种检测算法、判定规则、目标比较、阈值语义、ROI几何和 Overlay。
- 不改变相机打开、曝光、增益、软硬触发、硬触发延时和采集线程语义。
- 不改变正常 PLC 地址、编码、OK 写 0、NG 写 49 并约 100 ms 后写 0。
- 不改变统计口径、存图范围、目录、命名和 JPEG 质量 92。
- 不改变结果邮箱、帧队列、存图队列和 Fault 合同。
- 不升级 Qt、qmake、MSVC、C++、OpenCV、Paddle OCR 或二维码 DLL。
- 除明确取消新模板中的 `calibrate_config.yaml` 外，不清理或改名图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL、部署资源或用户外部模板；“移除模板”只修改路径引用。
- 不迁移、读取或自动删除当前 AppData 中的旧配方数据。
- 不顺带进行与模板/检测方案替换无关的目录大改、UI视觉重设计或算法优化。

## 十四、最终完成定义

只有同时满足以下条件，才能宣布“配方体系已经完全撤销，模板方案已经成为唯一正式路径”：

- `app/recipes/` 和所有配方生产类型、工程项、UI入口均已删除。
- 生产代码、UI对象、文件名、目录名、日志、注释和工程清单中的大小写不敏感 `Recipe/recipe` 命中为零。
- 整机设置、检测方案和模板私有参数三者边界唯一，没有同一字段多处保存。
- AppData 正式设置只剩 `settings/app_settings.json`；没有 `detection_schemes.json` 和第二套设置 Store。
- `SettingsApplicationService` 是唯一设置内存正式值；`AppSettingsStore` 是唯一磁盘读写入口；分区更新不会互相覆盖。
- 纸巾检测完全不依赖模板；单模板和多模板模式使用统一选择 UI，打开时当前路径预勾选，取消不落盘，确认后重启可恢复。
- “当前编辑模板”下拉框从当前模式所选路径产生，只影响本次 UI 编辑目标，不写入任何配置；批量参数仍作用于全部已选模板；右侧移除按钮只从当前模式路径列表移除当前项。
- 多模板对全部有效模板并行匹配并取最高分，同分时较早路径优先；失效模板带完整警告后跳过，全部失效时阻止启动。
- 运行有效模板数组紧凑且每项自带路径、名称、设置和资源；没有持久化模板索引、索引映射或模板参数错位。
- 外部模板文件夹能够独立复制、移动、编辑和重新选择；软件不提供永久删除模板目录的功能，取消勾选和下拉框移除按钮都只删除当前模式的路径引用。
- `template_settings.json.regions` 是三个区域点集的唯一来源；模板目录和生产代码均不再依赖 `calibrate_config.yaml`。
- 编辑、新建和同名覆盖共享唯一 `TemplateStore::save()` 与同一目录提交实现；编辑时先继承完整目录再覆盖变化，未修改资源和未知文件不会丢失；用户确认同名新建覆盖后，最终目录不残留任何旧内容。
- 单个模板与检测方案保存失败不会破坏其上一份完整有效数据；多模板批量保存不做跨目录回滚，并准确报告成功、失败和未处理项。
- 下拉框移除按钮与选择对话框取消勾选并确认共享 `saveTemplatePaths()`；设置失败不改变正式选择，任何移除、恢复默认和清空设置操作都不删除模板目录。
- 启动运行只使用一次准备完成的不可变检测方案快照。
- 正式代码不读取旧配方数据，不存在兼容开关、双路径和隐藏回退。
- `app/templates/` 只有 `template_store.h/.cpp` 两个生产代码文件，没有用新名字复制旧配方层级。
- `TemplateStore` 没有 `saveBatch()`，生产代码没有跨模板目录事务日志或全局回滚管理器。
- `SettingsApplicationService` 是唯一 `AppSettings` 内存正式值持有者，没有检测方案 Store 或第三个应用服务。
- `app` 生产 `.h/.cpp` 数量不超过 151，且未恢复 `tests/` 维护代码。
- 阶段 8 静态门禁和用户 Qt Creator 统一验证全部完成。
- 功能表恢复为已验证 87、已确认删除 3、其他状态 0；移除按钮作为 `TPL-008/TPL-010/TPL-016` 的统一选择 UI 证据，不新增功能 ID。
- 真实 PLC 现场项仍如实记录为待验，没有被软件侧验证冒充完成。
