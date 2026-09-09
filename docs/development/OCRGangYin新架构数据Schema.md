# OCRGangYin 数据 Schema（阶段 8）

状态：AppSettings Schema 8 代码已实施，等待用户统一验证
实现依据：`app/system_support/settings/app_settings*` 与 `app/templates/template_store*`

## 1. 数据边界

新版只有两类正式持久化数据：

```text
%APPDATA%/ShengYin/
└─ settings/
   └─ app_settings.json          整机设置、界面设置、五模式检测方案

<用户选择的任意外部模板文件夹>/
├─ template_settings.json        单个模板的全部私有参数
├─ template_raw.png              编辑原图
├─ tracking_template.bmp         定位模板
├─ template_ring.bmp             仅刚印检测
└─ character_templates/          需要字符匹配的模式
   ├─ 0.png
   ├─ upper_A.png
   ├─ lower_a.png
   └─ ...
```

- `app_settings.json` 只保存整机设置和“各模式当前应用哪些模板”。
- 一个模板文件夹只代表一个模板，文件夹名就是显示名。
- 纸巾检测不使用模板，粗糙度阈值直接写入 `app_settings.json`。
- 旧格式不迁移、不兼容，也不作为缺字段时的回退来源。

## 2. AppSettings Schema 8

### 2.1 唯一内存结构

```cpp
struct DetectionSchemes {
    QString stampTemplatePath;
    QStringList wordTemplatePaths;
    QString ocrTemplatePath;
    double tissueRoughnessThreshold = 6.0;
    QStringList barcodeWordTemplatePaths;
};

struct AppSettings {
    static const int CurrentSchemaVersion = 8;
    // 已验证的整机和 UI 扁平成员
    QString templateSaveDirectory;
    QByteArray leftDrawerSplitterState;
    bool barcodeCsvEnabled;
    QString barcodeCsvOutputDirectory;
    DetectionSchemes detectionSchemes;
};
```

`SettingsApplicationService::current()` 是唯一正式内存值；`draft()` 只是设置页草稿。JSON 分区用于可读性，不对应额外的 C++ Store 或 Service。

### 2.2 完整 JSON 示例

```json
{
  "schemaVersion": 8,
  "camera": {
    "exposureMicroseconds": 300,
    "gain": 1,
    "triggerSource": "hardwareLine0",
    "rotation": "none",
    "colorChannel": "color"
  },
  "inspection": {
    "minimumIntervalMs": 300
  },
  "plc": {
    "connection": {
      "ip": "192.168.10.10",
      "rack": 0,
      "slot": 1
    },
    "process": {
      "triggerMode": "intermittent",
      "photoDistanceMm": 50,
      "photoTimeMs": 300,
      "rejectDistanceMm": 500,
      "rejectTimeMs": 300,
      "rejectPosition": 0
    },
    "addresses": {
      "triggerMode": { "db": 1, "byteOffset": 1032 },
      "result": { "db": 1, "byteOffset": 1033 },
      "photoDistanceDb": 924,
      "photoTimeDb": 982,
      "rejectDistanceDb": 920,
      "rejectTimeDb": 980
    }
  },
  "imageSaving": {
    "range": "none",
    "content": "annotatedOnly",
    "outputDirectory": ""
  },
  "ui": {
    "selectedDetectionMode": "word",
    "templateSaveDirectory": "",
    "leftDrawerSplitterStateBase64": ""
  },
  "detectionSchemes": {
    "stamp": { "templatePath": "" },
    "word": { "templatePaths": [] },
    "ocr": { "templatePath": "" },
    "tissue": { "roughnessThreshold": 6.0 },
    "barcodeWord": { "templatePaths": [] }
  },
  "barcodeCsv": {
    "enabled": false,
    "outputDirectory": ""
  }
}
```

### 2.3 枚举值

| 字段 | 合法值 |
|---|---|
| `camera.triggerSource` | `software`、`hardwareLine0` |
| `camera.rotation` | `none`、`clockwise90`、`counterclockwise90`、`rotate180` |
| `camera.colorChannel` | `color`、`red`、`green`、`blue` |
| `plc.process.triggerMode` | `continuous`、`intermittent` |
| `imageSaving.range` | `none`、`ngOnly`、`okOnly`、`all` |
| `imageSaving.content` | `annotatedAndRaw`、`annotatedOnly`、`rawOnly` |
| `ui.selectedDetectionMode` | `stamp`、`word`、`ocr`、`tissue`、`barcodeWord` |
| `ui.leftDrawerSplitterStateBase64` | 空字符串或 `QSplitter::saveState()` 的 Base64 字符串 |
| `barcodeCsv.enabled` | `true`、`false` |

### 2.4 detectionSchemes 规则

| 模式 | JSON 字段 | 数量与默认值 |
|---|---|---|
| 刚印检测 | `stamp.templatePath` | 空字符串或一个绝对路径 |
| 字库匹配 | `word.templatePaths` | 零到多个绝对路径 |
| 深度 OCR | `ocr.templatePath` | 空字符串或一个绝对路径 |
| 纸巾检测 | `tissue.roughnessThreshold` | 非负有限数，默认 `6.0` |
| 二维码+三期 | `barcodeWord.templatePaths` | 零到多个绝对路径 |

路径保存前执行清理、转绝对路径和 Windows 不区分大小写去重。多模板路径使用线性列表保存，运行时并行评价全部有效模板并选择最高分结果。

### 2.5 严格读取和保存

- 八个根字段和各分区字段都必须存在，未知字段、类型错误和约束错误会拒绝整个文件。
- 文件不存在时只在内存使用完整默认值，第一次保存时创建。
- `barcodeCsv.outputDirectory` 为空时 `barcodeCsv.enabled` 必须为 `false`；非空时必须是绝对路径。
- `schemaVersion != 8` 返回 `SETTINGS_RESET_REQUIRED`；用户确认后用默认 Schema 8 原子替换。
- 已是 Schema 8 但内容损坏时拒绝启动，不自动覆盖诊断证据。
- 所有写入统一经过 `AppSettingsStore::save()` 和 `QSaveFile`。
- 模板路径或纸巾阈值保存从最新 `current` 复制候选，只改目标字段；不会提交或丢弃未应用的整机草稿。
- 完整默认值只在设置文件不存在、严格 Schema 重建和“清空软件数据”后的下一次启动使用；主程序不提供在线恢复默认入口。“清空软件数据”只删除当前设置文件，不删除任何外部模板。

## 3. TemplateSettings Schema 1

### 3.1 二维码+三期完整示例

```json
{
  "schemaVersion": 1,
  "detectionMode": "barcodeWord",
  "targetText": "20260820",
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
  "characterBoxes": [
    {
      "name": "2",
      "rect": { "x": 300, "y": 200, "width": 32, "height": 48 }
    }
  ],
  "barcodeParameters": {
    "formatMask": 1,
    "roiPaddingPercent": 8,
    "maxDecodeTimeMs": 60,
    "enableFallback": true
  }
}
```

### 3.2 模式字段矩阵

| 字段 | 刚印 | 字库 | OCR | 纸巾 | 二维码+三期 |
|---|:---:|:---:|:---:|:---:|:---:|
| `schemaVersion`、`detectionMode`、`targetText` | 是 | 是 | 是 | 无模板 | 是 |
| `imageThresholdPercent` | 是 | 是 | 否 | — | 是 |
| `trackingRoi`、`regions.datePolygon` | 是 | 是 | 是 | — | 是 |
| `regions.stampPolygon` | 是 | 否 | 否 | — | 否 |
| `regions.barcodePolygon` | 否 | 否 | 否 | — | 是 |
| `characterSourceSize`、`characterBoxes` | 是 | 是 | 否 | — | 是 |
| `barcodeParameters` | 否 | 否 | 否 | — | 是 |

非适用字段不得写入。模板 JSON 同样严格拒绝未知字段。

### 3.3 坐标和数值约束

- `trackingRoi` 使用原图绝对像素坐标，`x/y >= 0`，宽高在运行模板中必须大于 0。
- `datePolygon` 是相对定位 ROI 中心的浮点像素偏移，运行时至少 3 点。
- `barcodePolygon` 是相对定位 ROI 中心的浮点像素偏移，二维码模式运行时恰好 4 点。
- `stampPolygon` 是相对刚印环中心的浮点像素偏移，刚印模式运行时至少 3 点。
- 所有坐标必须是有限数，不能取整或改变参考中心。
- `imageThresholdPercent` 是 `0..100` 的整数。
- 字符框必须是正尺寸整数矩形，并位于 `characterSourceSize` 内。
- 二维码 `formatMask > 0`、`roiPaddingPercent >= 0`、`maxDecodeTimeMs > 0`。

## 4. 模板资源

| 路径 | 使用模式 | 运行要求 |
|---|---|---|
| `template_raw.png` | 四种模板模式 | 必须存在且能解码 |
| `tracking_template.bmp` | 四种模板模式 | 必须存在、能解码且尺寸等于 `trackingRoi` 宽高 |
| `template_ring.bmp` | 仅刚印 | 必须存在且能解码 |
| `character_templates/*.(png/bmp/jpg/jpeg)` | 刚印、字库、二维码+三期 | 目标文字中的每个有效字符至少有一张可匹配图片 |

ASCII 字母字符模板严格区分大小写：大写 `A` 使用 `upper_A.png`，小写 `a` 使用 `lower_a.png`；同字符变体仅使用 `upper_A(1).png`、`lower_a(1).png` 形式。旧 `A.png`、`a.png` 以及 `_`、`-` 变体不兼容、不迁移。数字和中文仍使用 `2.png`、`2(1).png`、`中.png` 等直接名称。该规则只改变字符资源文件名，不改变 `TemplateSettings` Schema 1。

区域点集只来自 `template_settings.json.regions`，不读取其他校准文件。

## 5. 三个读取级别

### 5.1 readSummary

用于模板选择和“当前编辑模板”列表：

- 检查路径、JSON Schema、模式和已有资源是否可读；
- 通过 `loadPrepared()` 判断“可运行”或“不完整”；
- 路径失效或内容损坏保留为异常项，不能静默从已选列表移除；
- 合法但尚未完整的模板仍可加入检测方案，启动时再严格拒绝或跳过。

### 5.2 loadEditable

用于编辑模板：

- 严格解析 JSON；
- 读取已经存在的图片和字符资源，已有但损坏的资源会报错；
- 允许必需资源暂时缺失，以便稍后继续制作。

### 5.3 loadPrepared

用于每次开始检测前：

- 严格检查该模式全部字段、区域点数、图片、字符覆盖和尺寸关系；
- 只在全部通过后创建不可变 `PreparedTemplate`；
- 单模板失败时阻止启动；多模板失败项带名称、完整路径和原因警告后跳过，至少一个有效模板即可继续。

## 6. PreparedTemplate

```cpp
struct PreparedTemplate {
    QString directoryPath;
    QString displayName;
    TemplateSettings settings;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    cv::Mat stampRingTemplate;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    std::vector<cv::Point2f> stampPolygon;
    std::vector<TemplateCharacterAsset> characterAssets;
    std::vector<cv::Mat> characterTemplates;
    std::vector<int> characterTemplateTargetIndexes;
};
```

一个对象同时携带路径、名称、参数和资源。多模板运行数组只包含有效对象，运行下标仅是本次启动期间访问同一紧凑数组的内部细节，不持久化、不建立索引映射。

## 7. TemplateStore 保存事务

`TemplateStore` 只公开 `readSummary/loadEditable/loadPrepared/save`。新建、编辑和同名覆盖都进入同一个 `save(directory, value, preserveExistingContents)`：

1. 在目标同级创建唯一临时目录。
2. 编辑时完整复制旧目录，包括未知附加文件；新建/覆盖时从空目录开始。
3. 覆盖写入 JSON 和本次变化资源；明确替换字符集合时先清理旧受管字符目录。
4. 从临时目录执行编辑级重新加载校验。
5. 旧目标改名为唯一备份，临时目录再改名为正式目标。
6. 成功后删除备份；提交失败则恢复，恢复也失败时保留临时目录和备份并报告完整路径。

用户取消同名覆盖时不会调用 `save()`。移除当前模板只更新 `app_settings.json` 中的路径引用，永远不访问或删除模板文件夹。

## 8. 错误码

模板错误至少包括：

- `TEMPLATE_PATH_INVALID`
- `TEMPLATE_DIRECTORY_MISSING`
- `TEMPLATE_SETTINGS_MISSING`
- `TEMPLATE_SETTINGS_MALFORMED`
- `TEMPLATE_SCHEMA_UNSUPPORTED`
- `TEMPLATE_MODE_MISMATCH`
- `TEMPLATE_FIELD_INVALID`
- `TEMPLATE_REGION_INVALID`
- `TEMPLATE_RESOURCE_MISSING`
- `TEMPLATE_IMAGE_CORRUPT`
- `TEMPLATE_CHARACTER_INVALID`
- `TEMPLATE_SAVE_FAILED`
- `TEMPLATE_COMMIT_FAILED`
- `TEMPLATE_ROLLBACK_FAILED`

设置错误至少包括旧格式重置、JSON 损坏、字段缺失/类型/范围/组合约束以及写入提交失败。所有模板错误对象包含 `code/userMessage/diagnostic/path`。

## 9. 当前阶段状态

- Schema 和生产代码已经一致收口。
- 未增加旧格式迁移、双读或双写逻辑。
- 已完成 qmake 和 MSVC x64 Release 编译、链接及运行库部署。
- 功能状态保持“迁移中”，等待用户在 Qt Creator 完成人工统一验证。
