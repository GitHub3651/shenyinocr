# OCRGangYin 新架构数据 Schema

版本：1.1
冻结日期：2026-08-16
修订日期：2026-08-17
适用阶段：新架构完全替换阶段 1～7
状态：阶段 1 冻结；按用户确认的既有模板制作顺序修订运行完整性边界

## 1. 权威性与兼容边界

本文是 `MachineSettings`、`ProductRecipe`、`PreparedRecipe`、`MachineSettingsStore`、`RecipeStore` 和 `RecipeEditorSession` 的唯一数据合同。字段来自当前 `app/` 的 UI、旧持久化、模板资源、五种启动预检和运行消费者反向盘点；不是根据目标类名推测。

- 机器设置 Schema 与产品配方 Schema 均固定为 `schemaVersion = 1`。
- 新程序只识别本文定义的 JSON 和资源目录；`settings.ini`、`app_settings.appset`、外部旧模板目录及其他旧格式一律返回明确错误，不迁移、不修复、不回退读取。
- JSON 对象只接受本文列出的字段；未知字段、缺字段、类型不符、非有限数值均拒绝。
- 同一业务参数只在本文指定的归属中出现一次。UI 静态值、算法构造默认和兼容默认不得形成第二来源。
- `ProductRecipe` 是可编辑、可持久化的数据；`PreparedRecipe` 是成功加载并校验全部**已保存资产**后形成的不可变快照。模板基础几何、字符切割和目标字符是先后独立的编辑步骤，因此快照可以处于“可继续编辑、尚不可启动”状态；是否具备当前模式的全部运行资产只由启动资源预检判定。运行期间不得重新读取设置文件、配方目录或 UI 控件。

本次1.1修订依据当前代码历史行为和用户确认：产品模板先保存定位区域、检测区域及基础资源，随后可选择是否立即切割字符模板，目标字符可在之后单独确认；不得让目标字符为空阻断产品模板创建或基础保存。该修订不改变JSON `schemaVersion = 1`。

## 2. 应用数据根目录与目录合同

| 项目 | 正式规则 | 现有依据 | 失败错误码 |
|---|---|---|---|
| 应用数据根 | `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)` 的绝对规范路径；由 `startup` 求值后注入 Store；返回空路径即启动失败，不使用 Home 目录 fallback | 旧 `AppSettingsManager::globalDataDirPath()` 主路径；终局方案要求必选依赖构造失败即失败 | `DATA_ROOT_UNAVAILABLE` |
| 机器设置 | `<ApplicationData>/settings/app_settings.json` | 主计划 3.1/3.2 正式目录 | `SETTINGS_READ_FAILED` / `SETTINGS_WRITE_FAILED` |
| 配方根 | `<ApplicationData>/recipes/` | 已有 `RecipeStore` 和主计划 3.1 | `RECIPE_ROOT_UNAVAILABLE` |
| 单配方目录 | `<ApplicationData>/recipes/<canonical-lowercase-uuid>/` | 已有 `RecipeStore::recipeDirectoryPath` | `RECIPE_ID_INVALID` / `RECIPE_DIRECTORY_MISMATCH` |
| 配方描述 | `<recipe>/recipe.json`，UTF-8、缩进 JSON | 已有 `RecipeStore` | `RECIPE_JSON_MISSING` / `RECIPE_JSON_MALFORMED` |
| 配方资源 | `<recipe>/assets/...`；JSON 只保存以 `assets/` 开头的 `/` 分隔相对路径 | 已有路径门禁；终局方案禁止运行时外部绝对路径 | `RECIPE_ASSET_PATH_INVALID` |
| 编辑工作区 | `<ApplicationData>/recipe_editor/<session-uuid>/`，只属于活动 `RecipeEditorSession`，不得作为运行配方或旧模板目录被选择 | 当前 UI 需要先生成图片/YAML再事务发布；新架构不允许外部旧目录成为数据源 | `EDITOR_WORKSPACE_FAILED` |
| 诊断目录 | `<ApplicationData>/diagnostics/` | 主计划 3.1 | `DIAGNOSTICS_ROOT_UNAVAILABLE` |

路径交叉约束：禁止绝对资源路径、盘符、UNC、符号链接、`..` 越界、空路径、大小写折叠后的重复目标和指向配方目录外的规范路径。机器设置中的生产图片输出目录是用户选择的外部绝对目录，属于运行输出位置，不是配方资源路径。

## 3. MachineSettings

### 3.1 C++ 类型

```cpp
enum class CameraTriggerSource { Software, HardwareLine0 };
enum class ImageRotation { None, Clockwise90, Counterclockwise90, Rotate180 };
enum class ColorChannel { Color, Red, Green, Blue };
enum class PlcTriggerMode { Continuous, Intermittent };
enum class ImageSaveRange { None, NgOnly, OkOnly, All };
enum class ImageSaveContent { AnnotatedAndRaw, AnnotatedOnly, RawOnly };

struct MachineSettings;
typedef std::shared_ptr<const MachineSettings> MachineSettingsSnapshot;
```

JSON 枚举值固定使用下表的小写字符串，不接受旧 ID（例如 `word_detection`、`trigger_interval`）作为兼容输入。当前Qt页面适配层仍使用受严格校验的`QString`控件ID；`MachineSettingsStore`负责与下表逻辑枚举一一映射，旧ID永不进入新JSON，也不能作为JSON输入。

### 3.2 根、相机与检测节拍字段

| JSON 路径 | C++ 类型 / 枚举值 | 单位 | 规则（必填、范围、交叉约束） | 唯一默认值与现有依据 | 归属 | 当前 UI 来源 | 旧持久化来源 | 运行消费者 | 保存格式 / 最终去向 |
|---|---|---:|---|---|---|---|---|---|---|
| `schemaVersion` | `int` | 无 | 必填；必须等于 `1` | `1`；新格式首版 | 机器设置 | 无 | `Meta/config_version=2`（只作字段盘点，不兼容） | `MachineSettingsStore` | JSON number → `MachineSettings` |
| `camera.exposureMicroseconds` | `int` | µs（MVS `ExposureTime`） | 必填；`>=0`；应用到相机时还必须处于设备查询范围 | `800`；旧 `GlobalSettings::cameraExposure` 构造值及功能表 SET-002/006。`widget.ui` 的 128 不是业务默认 | 机器设置 | `spinBox` | `Global/camera_exposure` | 相机打开、曝光应用、启动快照 | JSON number → `MachineSettings` |
| `camera.gain` | `int` | 设备增益单位 | 必填；`>=0`；应用时还必须处于设备查询范围 | `1`；旧构造值及 SET-002/007。UI 的 0 不是业务默认；旧代码加载后已强制整数 | 机器设置 | `lineEdit_14` | `Global/camera_gain` | 相机打开、增益应用、启动快照 | JSON number → `MachineSettings` |
| `camera.triggerSource` | `CameraTriggerSource`：`software` / `hardwareLine0` | 无 | 必填 | `hardwareLine0`；旧 `triggerEnabled=true` | 机器设置 | `checkBox`“启用触发” | `Global/trigger_enabled` | 启动计划、相机触发模式 | JSON string → `MachineSettings` |
| `camera.rotation` | `ImageRotation`：`none` / `clockwise90` / `counterclockwise90` / `rotate180` | 度 | 必填 | `none`；旧 `rotate_none` | 机器设置 | `comboBox_2` | `Global/image_rotation` | 采集前图像变换快照 | JSON string → `MachineSettings` |
| `camera.colorChannel` | `ColorChannel`：`color` / `red` / `green` / `blue` | 无 | 必填 | `color`；旧 `color` | 机器设置 | `comboBox_5` | `Global/color_channel` | 采集前图像变换快照 | JSON string → `MachineSettings` |
| `inspection.minimumIntervalMs` | `int` | ms | 必填；`>=0`；0 表示不增加软件等待 | `300`；旧 `cameraDelay=300`、RUN-005 | 机器设置 | `lineEdit_4`“相机延时(ms)” | `Global/camera_delay` | 软触发两次正式受理的最小间隔 | JSON number → `MachineSettings` |

### 3.3 PLC 字段

| JSON 路径 | C++ 类型 / 枚举值 | 单位 | 规则 | 唯一默认值与现有依据 | 归属 | UI / 旧来源 | 运行消费者 | 保存格式 / 最终去向 |
|---|---|---:|---|---|---|---|---|---|
| `plc.connection.ip` | `QString` | IPv4文本 | 必填；trim 后非空 | `192.168.10.10`；SET-002/PLC-001 | 机器设置 | `lineEdit` / `Global/plc_ip` | 延迟连接、手动连接、开相机伴随连接 | JSON string → `MachineSettings` |
| `plc.connection.rack` | `int` | 无 | 必填；`>=0` | `0` | 机器设置 | `lineEdit_2` / `Global/plc_rack` | Snap7连接 | JSON number → `MachineSettings` |
| `plc.connection.slot` | `int` | 无 | 必填；`>=0` | `1` | 机器设置 | `lineEdit_3` / `Global/plc_slot` | Snap7连接 | JSON number → `MachineSettings` |
| `plc.process.triggerMode` | `PlcTriggerMode`：`continuous` / `intermittent` | 无 | 必填 | `intermittent`；旧 `trigger_interval`；连续写0、间歇写1 | 机器设置 | `comboBox_3` / `Global/trigger_mode` 与重复的 `Global/plc_mode`；新格式只保留本字段 | PLC `DB1.DBB1032` | JSON string → `MachineSettings` |
| `plc.process.photoDistanceMm` | `int` | mm | 必填；`>=0` 且 `<=2147483647` | `50`；UI/旧构造值 | 机器设置 | `lineEdit_6` / `Global/photo_distance` | DB1.DBD924，大端 | JSON number → `MachineSettings` |
| `plc.process.photoTimeMs` | `int` | ms | 必填；`0..65535` | `300` | 机器设置 | `lineEdit_20` / `Global/photo_time` | DB1.DBW982，大端 | JSON number → `MachineSettings` |
| `plc.process.rejectDistanceMm` | `int` | mm | 必填；`>=0` | `500` | 机器设置 | `lineEdit_7` / `Global/reject_distance` | DB920 DWord，大端 | JSON number → `MachineSettings` |
| `plc.process.rejectTimeMs` | `int` | ms | 必填；`>=0` | `300` | 机器设置 | `lineEdit_8` / `Global/reject_time` | DB980 Word对应旧顺序中的剔除时间 | JSON number → `MachineSettings` |
| `plc.process.rejectPosition` | `int` | 产品位置计数 | 必填；`>=0` | `0` | 机器设置 | `lineEdit_12` / `Global/reject_position` | `ResultHandler` 用作 NG 目标产品的延迟位置计数 | JSON number → `MachineSettings` |
| `plc.addresses.triggerMode` | `{db:int, byteOffset:int}` | 字节地址 | 必填；只接受 `{1,1032}` | `{1,1032}`；PLC-003现合同 | 机器设置中的固定设备合同 | 无 / 旧代码常量 | `InspectionPlcController` | JSON object → `MachineSettings` |
| `plc.addresses.result` | `{db:int, byteOffset:int}` | 字节地址 | 必填；只接受 `{1,1033}` | `{1,1033}`；PLC-005/006现合同 | 机器设置中的固定设备合同 | 无 / 旧代码常量 | OK 0、NG 49→约100ms→0 | JSON object → `MachineSettings` |
| `plc.addresses.photoDistanceDb` | `int` | DB字节偏移 | 必填；必须为 `924` | `924`；`InspectionPlcController::applyRunSettings/writePhotoDistance` | 机器设置中的固定设备合同 | 无 / 旧代码常量 | DWord工艺参数写入 | JSON number → `MachineSettings` |
| `plc.addresses.photoTimeDb` | `int` | DB字节偏移 | 必填；必须为 `982` | `982`；PLC-004 | 同上 | 无 / 旧代码常量 | 工艺参数写入 | JSON number → `MachineSettings` |
| `plc.addresses.rejectDistanceDb` | `int` | DB字节偏移 | 必填；必须为 `920` | `920`；PLC-004 | 同上 | 无 / 旧代码常量 | 工艺参数写入 | JSON number → `MachineSettings` |
| `plc.addresses.rejectTimeDb` | `int` | DB字节偏移 | 必填；必须为 `980` | `980`；PLC-004当前合同 | 同上 | 无 / 旧代码常量 | 工艺参数写入 | JSON number → `MachineSettings` |

PLC 数值交叉约束：写入 Word 的 `photoTimeMs/rejectTimeMs` 必须 `<=65535`，写入 DWord 的距离必须可表示为无符号32位；当前 UI 只允许非负整数。正常 PLC 结果合同不属于可配置字段：OK 固定写0；NG固定写49，约100ms后固定写0。

### 3.4 存图与纯 UI 字段

| JSON 路径 | C++ 类型 / 枚举值 | 单位 | 规则 | 唯一默认值与依据 | 归属 | UI / 旧来源 | 运行消费者 | 保存格式 / 最终去向 |
|---|---|---:|---|---|---|---|---|---|
| `imageSaving.range` | `ImageSaveRange`：`none` / `ngOnly` / `okOnly` / `all` | 无 | 必填 | `none`；SET-002/011 | 机器设置 | `comboBox` / `Global/image_save_mode` | `ResultHandler`存图决策 | JSON string → `MachineSettings` |
| `imageSaving.content` | `ImageSaveContent`：`annotatedAndRaw` / `annotatedOnly` / `rawOnly` | 无 | 必填 | `annotatedOnly`；SET-002/011 | 机器设置 | `comboBox_saveImageType` / `Global/image_save_type` | 每产品唯一存图任务 | JSON string → `MachineSettings` |
| `imageSaving.outputDirectory` | `QString` | 绝对路径 | 必填，可为空；`range!=none` 时必须为非空绝对目录 | 空字符串；旧未选目录行为 | 机器设置 | `lineEdit_imageSavePath`/浏览按钮 / `Global/image_save_path` | 图片目录创建与写入 | JSON string → `MachineSettings` |
| `imageSaving.jpegQuality` | `int` | JPEG质量 | 必填；必须等于 `92` | `92`；DIFF-009与已验证行为 | 机器设置中的固定生产合同 | 无 / 旧结果链常量 | 五模式生产JPG编码 | JSON number → `MachineSettings` |
| `ui.selectedDetectionMode` | `DetectionMode`：`stamp` / `word` / `ocr` / `tissue` / `barcodeWord` | 无 | 必填 | `word`；旧 `word_detection`、UI索引1 | 纯UI状态 | `comboBox_4` / `Global/detect_mode` | 启动时默认页面与模式 | JSON string → `MachineSettings` |
| `ui.lastRecipeIdByMode` | `QMap<DetectionMode, QString>` | UUID | 必填对象；值必须为空缺省或规范小写UUID；只记录已经存在且模式匹配的配方；加载失败移除该项但不读取旧目录 | 空对象；SET-002/TPL-016 | 纯UI状态 | 配方选择/发布 / `PublishedRecipeIds/*` | 模式切换和重启恢复 | JSON object → `MachineSettings` |
| `ui.rightPanelSplitterStateBase64` | `QByteArray` | Base64 | 必填；空字符串或可解码Base64 | 空字符串；无历史布局时使用 `.ui` 布局 | 纯UI布局 | `rightPanelSplitter` / `Ui/right_panel_splitter_state` | `QSplitter::restoreState` | JSON string → `MachineSettings` |

### 3.5 明确不进入 MachineSettings 的旧字段

| 旧字段/来源 | 最终处理 | 原因 |
|---|---|---|
| `templateBaseDirPath` / `Global/template_base_dir` | 删除 | 新配方固定写应用数据根的UUID目录，不再选择旧模板父目录 |
| `templateDirPathsByMode` / `TemplatePaths/*` | 删除 | 旧目录明确不兼容；不得作为 fallback |
| `tissueRoughnessThreshold` / `Global/tissue_roughness_threshold` | 移入纸巾 `ProductRecipe.parameters.roughnessThreshold` | 产品私有参数，不属于机器 |
| 重复的 `plcModeId` | 删除 | 与 `plc.process.triggerMode` 同义，禁止双来源 |
| 相机设备标识 | 本阶段确认不持久化 | 当前正式行为固定枚举并打开第0台设备，没有用户可配置字段；不得凭目标类名新增 |

### 3.6 MachineSettingsStore 行为

- `load()`：文件不存在返回 `MachineSettings::defaults()` 和 `FirstRun`；不得自动搜索 `settings.ini`。
- 文件存在但 JSON、Schema、枚举、范围或交叉约束错误时返回失败且不覆盖调用方现有快照；不得使用默认值继续运行。
- `save()`：使用同目录临时文件，写完后重新读取并逐字段相等校验，再用可回滚替换提交；失败保持上一完整文件。
- `restoreDefaults()`：返回并事务保存唯一默认对象。
- `clear()`：只删除 `settings/app_settings.json`；不删除配方、生产图片、授权、日志或诊断。内存当前值回到唯一默认对象。
- 所有错误返回 `{code, userMessage, diagnostic}`；Store 不弹框。

机器设置错误码：`DATA_ROOT_UNAVAILABLE`、`SETTINGS_NOT_FOUND`（仅首启状态，不是故障）、`SETTINGS_JSON_MALFORMED`、`SETTINGS_SCHEMA_UNSUPPORTED`、`SETTINGS_FIELD_MISSING`、`SETTINGS_FIELD_TYPE_INVALID`、`SETTINGS_FIELD_RANGE_INVALID`、`SETTINGS_CONSTRAINT_VIOLATION`、`SETTINGS_READ_FAILED`、`SETTINGS_WRITE_FAILED`、`SETTINGS_VERIFY_FAILED`、`SETTINGS_COMMIT_FAILED`、`SETTINGS_ROLLBACK_FAILED`。

## 4. ProductRecipe

### 4.1 根对象

| JSON 路径 | C++ 类型 / 枚举值 | 单位 | 规则 | 唯一默认值 / 依据 | 归属 | UI来源 | 旧来源 | 运行消费者 | 最终去向 |
|---|---|---:|---|---|---|---|---|---|---|
| `schemaVersion` | `int` | 无 | 必填；必须为1 | 1 | 产品 | 无 | 已有 recipe v1 | `RecipeStore` | `ProductRecipe` |
| `recipeId` | `QString` | UUID | 必填；规范小写、无花括号；创建后不可修改；必须与目录名一致 | `QUuid::createUuid()`；已有行为 | 产品 | 新建时系统生成 | 已有 recipeId | Store、模式记忆、重发 | `ProductRecipe` |
| `displayName` | `QString` | 文本 | 必填；trim 后非空 | 无默认，用户必填 | 产品 | “产品模板名称”输入 | 旧文件夹名 | 目录列表和当前模板名 | `ProductRecipe` |
| `detectionMode` | `DetectionMode`：`stamp` / `word` / `ocr` / `tissue` / `barcodeWord` | 无 | 必填；创建后不可跨模式修改 | 当前UI所选模式 | 产品 | `comboBox_4` | 旧模式索引/已有模式ID | Pipeline选择与模式预检 | `ProductRecipe` |
| `parameters` | 模式专用对象 | 无 | 必填；只允许当前模式对应结构 | 无通用默认 | 产品 | 模式编辑页 | 旧INI/YAML/内存 | `PreparedRecipe` | `ProductRecipe` |
| `assets` | `QMap<QString, QString>` | 相对路径 | 必填对象；纸巾必须为空；键非空且唯一；值满足第2节路径合同 | 空对象 | 产品资源清单 | 模板制作/字符切割 | 旧模板文件 | Store复制与准备 | `ProductRecipe` |

JSON 模式值采用短稳定值；旧 `stamp_detection` 等字符串只用于 UI 映射，不作为新 JSON 输入。

### 4.2 Profile 公共字段

以下 `Profile` 用于钢印、字库、OCR、二维码+三期。钢印和OCR必须恰好1个；字库和二维码+三期允许1个或多个且顺序有业务意义；同分仍选择先出现的 Profile。

| JSON 路径（`P`代表Profile路径） | C++类型 | 单位 | 必填/范围/交叉约束 | 唯一默认值与依据 | 归属 | UI来源 | 旧持久化来源 | 运行消费者 | 最终去向 |
|---|---|---:|---|---|---|---|---|---|---|
| `P.name` | `QString` | 文本 | 必填、trim非空；同一配方内大小写不敏感唯一 | 新建时使用产品显示名；现有Profile名/目录名行为 | Profile | Profile下拉框/新建名称 | 旧目录名、当前 recipe profile name | Profile呈现与同分顺序 | `ProductRecipe` |
| `P.targetText` | `QString` | 文本 | JSON必填字符串；保存产品模板时允许为空；Stamp/Word/Ocr/BarcodeWord启动前必须trim非空；匹配解析规则保持现有正则 | 唯一默认空字符串，表示尚未确认目标；不是运行fallback | Profile | `dateEdit` / “确认字符” | `Template/target_text` | 四种模式目标比较/字符数量；启动完整性预检 | `ProductRecipe` |
| `P.imageThresholdPercent` | `int` | % | Stamp/Word/BarcodeWord必填，0..100；Ocr JSON 中禁止出现 | `70`；旧构造后实际值及 DIFF-002，UI静态80不是默认 | Profile | `lineEdit_yuzhi` | `Template/image_threshold` | CharacterTemplateMatcher | `ProductRecipe` |
| `P.trackingRoi.{x,y,width,height}` | `double` | 原图像素 | 必填、有限；x/y>=0，width/height>0；必须位于 rawImage 尺寸内 | 无默认；必须由画布框选 | Profile | `ImageLabel::trackingRect`换算 | `Template/tracking_box_*` | 定位模板对应的原图区域 | `ProductRecipe` |
| `P.characterSourceSize.{width,height}` | `int` | 像素 | Stamp/Word/BarcodeWord的JSON必填；尚未切割时必须同时为0；存在任一字符框或字符资源时宽高必须>0，并与字符来源图一致；Ocr必须为0 | 唯一默认`0,0`，表示尚未切割；字符切割后替换 | Profile | 字符裁切对话框 | `CharacterTemplateBoxes/source_*` | 字符框校验、再次编辑；启动完整性预检 | `ProductRecipe` |
| `P.characterBoxes[]` | `{name:QString, rect:{x,y,width,height}}` | 源图像素 | JSON必填数组；尚未切割时必须为空；存在字符数据时必须非空，name非空、rect宽高>0且完全在sourceSize内，顺序按现有位置排序；Ocr必须为空 | 唯一默认空数组，表示尚未切割 | Profile | 字符裁切对话框 | `CharacterTemplateBoxes/*` | 编辑恢复、资产名称校验；启动完整性预检 | `ProductRecipe` |
| `P.assetKeys` | `QMap<QString, QString>` | 资源键 | 必填；角色和资源键都非空；资源键必须存在于根 `assets`；同资源不得承担两个Profile角色 | 无默认 | Profile资源清单 | 模板制作/裁切 | 旧目录文件名 | PreparedRecipe资源解析 | `ProductRecipe` |

`hasValidBoxes` 被删除：它与 tracking ROI 有效性重复。新格式以 `trackingRoi` 的完整范围校验为唯一事实，不再自动“修复”布尔标志。

### 4.3 五种模式的类型化 parameters

| 模式 | JSON结构 | Profile数量 | 必需字段/资源角色 | 现有业务依据 | 缺失/损坏错误 |
|---|---|---:|---|---|---|
| `stamp` | `parameters.profile` | 恰好1 | 基础保存必须有threshold、trackingRoi及tracking/calibration/raw/stampRing；targetText和整组字符数据可后补。启动前必须有targetText，并且每个目标单元至少匹配一个字符模板 | DET-002、TPL-007/011/013/015及既有“先保存、后切割、再确认字符”流程 | `RECIPE_STAMP_PROFILE_INVALID` / `RECIPE_ASSET_IMAGE_CORRUPT` / `RECIPE_CALIBRATION_INVALID`；不完整时启动预检拒绝 |
| `word` | `parameters.profiles[]` | >=1 | 每Profile基础保存必须有threshold、trackingRoi及tracking/calibration/raw；targetText和整组字符数据可后补。启动前每个Profile必须有targetText及完整目标字符模板映射 | DET-003、TPL-009..015及既有编辑顺序 | `RECIPE_WORD_PROFILE_INVALID`；不完整时`WordProfilesIncomplete` |
| `ocr` | `parameters.profile` | 恰好1 | 基础保存必须有trackingRoi及tracking/calibration/raw；targetText可后补，启动前必填；禁止字符字段、阈值和stampRing | DET-004、TPL-008/011及目标字符独立确认入口 | `RECIPE_OCR_PROFILE_INVALID`；目标为空时启动预检拒绝 |
| `tissue` | `parameters.roughnessThreshold` | 0 | `double`有限且>0；根assets必须空 | DET-005、主计划明确纸巾无模板 | `RECIPE_TISSUE_PARAMETERS_INVALID` |
| `barcodeWord` | `parameters.profiles[]` | >=1 | 基础保存遵循Word规则，另必须有barcode参数、YAML中4点barcode ROI和>=3点date ROI；targetText和字符数据可后补。启动前还必须有完整目标字符模板映射且读码组件就绪 | DET-006、TPL-004/009..015及既有编辑顺序 | `RECIPE_BARCODE_PROFILE_INVALID` / `RECIPE_BARCODE_ROI_INVALID`；不完整时启动预检拒绝 |

纸巾 `roughnessThreshold` 唯一默认是 `6.0`，来自已确认主计划、`TissueRecipeParameters` 和既有门禁；检测器、UI、MachineSettings不得再定义另一个默认。

二维码 Profile 额外字段：

| JSON路径 | C++类型 | 单位 | 规则 | 唯一默认值 / 来源 | 归属 | UI/旧来源 | 运行消费者 |
|---|---|---:|---|---|---|---|---|
| `P.barcode.formatMask` | `quint32` 位掩码：1 DataMatrix、2 QR、3两者 | 位 | 必填；1..3且不得有其他位 | `1`；当前 DataMatrix 默认 | Profile | 当前即时验证选项 / `Template/barcode_format_mask` | BarcodeDecoder |
| `P.barcode.roiPaddingPercent` | `int` | % | 必填；0..100 | `8`；现代码与TPL-004 | Profile | 当前无独立控件 / 旧私有设置 | BarcodeDecoder ROI预处理 |
| `P.barcode.maxDecodeTimeMs` | `int` | ms | 必填；1..60000 | `60`；现代码与TPL-004 | Profile | 当前无独立控件 / 旧私有设置 | BarcodeDecoder预算 |
| `P.barcode.enableFallback` | `bool` | 无 | 必填 | `true`；现代码 | Profile | 当前无独立控件 / 旧私有设置 | BarcodeDecoder fallback策略 |

上述四个二维码默认值的唯一代码来源是`contracts/barcode_parameter_defaults.h`；`BarcodeRecipeParameters`与设备侧`BarcodeDecodeOptions`只引用这些常量，不再各自定义第二套默认值。

### 4.4 资源角色与资源内容 Schema

| Profile角色 | 相对路径格式 | 保存格式 | 适用模式 | 内容与交叉约束 | 当前来源 | PreparedRecipe结果 | 错误码 |
|---|---|---|---|---|---|---|---|
| `trackingTemplate` | `assets/profiles/<index>/tracking_template.bmp` | 可由OpenCV解码的非空BMP | Stamp/Word/Ocr/BarcodeWord | 彩色解码成功，宽高>0；尺寸等于 trackingRoi 裁剪尺寸 | 当前冻结原图裁剪 | 只读 `cv::Mat` | `RECIPE_ASSET_MISSING` / `RECIPE_ASSET_IMAGE_CORRUPT` / `PREPARE_TRACKING_TEMPLATE_FAILED` |
| `calibration` | `assets/profiles/<index>/calibrate_config.yaml` | OpenCV YAML | 四模板模式 | 文件可完整解析；`date_poly`恰有>=3个有限点；Barcode还要求`barcode_poly`恰好4点，顺序左上/右上/右下/左下；Stamp还要求`stamp_poly`>=3点 | 模板画布与钢印标定 | date/barcode/stamp相对点数组 | `RECIPE_CALIBRATION_INVALID` |
| `rawImage` | `assets/profiles/<index>/template_raw.png` | 可解码PNG | 四模板模式 | 彩色解码成功；trackingRoi必须完全位于其尺寸内 | 当前冻结原帧 | 只读编辑资产；运行预检只校验 | `RECIPE_ASSET_MISSING` / `RECIPE_ASSET_IMAGE_CORRUPT` / `PREPARE_TRACKING_TEMPLATE_FAILED` |
| `stampRing` | `assets/profiles/0/template_ring.bmp` | 可解码BMP | 仅Stamp | 灰度解码成功，宽高均>=10；与YAML stamp_poly共同可初始化OverlapDetector | 钢印吸管口ROI | 只读环模板/准备后的重叠资产 | `RECIPE_ASSET_MISSING` / `RECIPE_ASSET_IMAGE_CORRUPT` / `PREPARE_STAMP_ASSETS_FAILED` |
| `character/<fileName>` | `assets/profiles/<index>/character_templates/<fileName>` | PNG/JPG/JPEG/BMP/TIFF | Stamp/Word/BarcodeWord | 未切割时允许整组不存在；一旦存在，所有文件都必须灰度解码成功，并与正尺寸sourceSize及非空boxes形成完整一组；文件完整baseName按当前规则匹配target unit，变体按大小写不敏感文件名稳定排序；每个target unit至少一个模板是**启动前**约束，不是基础保存约束 | 字符裁切对话框 | 中立命名模板矩阵始终保留；target非空时另生成target index映射，由Runtime交给Detection准备匹配器私有结构 | `RECIPE_CHARACTER_TEMPLATE_INVALID` / `RECIPE_ASSET_IMAGE_CORRUPT` / `PREPARE_CHARACTER_ASSETS_FAILED`；目标覆盖不足由启动预检拒绝 |

YAML 点坐标单位均为像素偏移：`date_poly`、`barcode_poly` 相对 tracking ROI 中心；`stamp_poly` 相对 stampRing 标定中心。它们不在 JSON 中重复保存，以校准资源为唯一来源。`trackingRoi` 是原图绝对像素矩形；字符框是字符来源裁剪图内像素矩形。

## 5. PreparedRecipe

### 5.1 不可变结构

```cpp
struct PreparedRecipeCharacterAsset {
    QString fileName;
    QString normalizedBaseName;
    cv::Mat image;
};

struct PreparedRecipeProfile {
    RecipeProfile definition;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    std::vector<cv::Point2f> stampPolygon;
    cv::Mat stampRingTemplate;
    std::vector<PreparedRecipeCharacterAsset> characterAssets;
    std::vector<cv::Mat> characterTemplates;
    std::vector<int> characterTemplateTargetIndexes;
};

struct PreparedRecipe {
    ProductRecipeSnapshot recipe;
    QVector<PreparedRecipeProfile> profiles;
    TissueRecipeParameters tissue;
};

typedef std::shared_ptr<const PreparedRecipe> PreparedRecipeSnapshot;
```

- `PreparedRecipe` 只保存自身拥有的内存资产与不可变 `ProductRecipeSnapshot`，不暴露外部绝对资源路径给 Runtime。
- 准备过程逐个解码所有已保存图片并解析YAML；任一已声明资源缺失或损坏都失败，不产生部分快照。尚未切割字符、目标字符为空或目标覆盖不足不属于资源损坏：快照保留按文件名排序的中立`characterAssets`，目标非空时另外生成可匹配的`characterTemplates/targetIndexes`，并由启动预检判断是否足以运行。Detection自己的 `TemplateMatchPreparedTemplates` 只能在Runtime/Detection边界内从这份中立资产构造，不能进入recipes头文件或JSON。
- Tissue准备结果没有Profile或资源，只携带明确的 `roughnessThreshold`。
- 五模式启动资源预检只接受成功的 `PreparedRecipeSnapshot`，并在此处拒绝目标字符未确认、字符尚未切割或目标覆盖不足的快照；不得在启动函数重新读目录或从 UI/旧字段补缺失资源。
- Barcode Decoder/OCR Engine属于设备依赖，不进入配方；准备结果分别记录模式资产完整性，启动还要检查相应设备端口就绪。

准备错误码：配方结构先返回4.3和7节定义的`RECIPE_*`错误；资源解码可返回`RECIPE_ASSET_MISSING`、`RECIPE_ASSET_IMAGE_CORRUPT`、`RECIPE_CALIBRATION_INVALID`和`RECIPE_CHARACTER_TEMPLATE_INVALID`；已保存资产的交叉约束返回`PREPARE_TRACKING_TEMPLATE_FAILED`、`PREPARE_CHARACTER_ASSETS_FAILED`、`PREPARE_STAMP_ASSETS_FAILED`或`PREPARE_BARCODE_ASSETS_FAILED`。目标字符/字符模板的运行不完整由`ProductTemplateIncomplete`、`WordProfilesIncomplete`或`BarcodeResourcesInvalid`启动预检结果表达，不再用`PREPARE_TARGET_TEXT_MISSING`阻止编辑加载或事务保存。不使用泛化成功fallback。

## 6. RecipeEditorSession

`RecipeEditorSession` 是当前模板编辑 UI 与正式 `RecipeStore` 之间的唯一编辑状态，替代平行的 Draft/Edit Session 和旧目录/INI状态。

| 字段 | 类型 | 规则 |
|---|---|---|
| `sessionId` | canonical UUID | 每次 beginNew/beginEdit 新建；用于工作区目录 |
| `state` | `Inactive/New/Editing` | 非活动会话不得保存 |
| `draft` | `ProductRecipe` | New创建一次recipeId；Editing保持已加载recipeId；不得跨模式改ID |
| `assetSourcePaths` | `QMap<QString,QString>` | 只允许当前工作区文件或已加载正式配方内资源；发布前全部复制到事务临时目录 |
| `originalRecipe` | `ProductRecipeSnapshot` | 仅Editing持有；失败时用于证明正式数据和会话未被部分修改 |
| `preparedPreview` | `PreparedRecipeSnapshot` | 每次已保存字段与资源完整校验成功后替换；允许业务步骤尚未完成，失败保留上一份 |

操作合同：

1. `beginNew(mode, displayName)` 创建UUID、模式默认结构和独占工作区；不创建正式配方目录。
2. `beginEdit(recipeId)` 通过 `RecipeStore` 加载并准备所有已保存资产，再复制为编辑草稿；允许目标或字符切割尚未完成，旧目录不能作为输入。
3. 编辑字符框、目标文本、阈值和资源只修改候选副本/工作区。
4. `save()` 先校验候选中已填写字段及已声明资源，再调用 `RecipeStore` 整目录事务保存；Store成功并重新准备正式目录后才替换会话状态。目标或字符步骤未完成不阻止保存，但不能通过启动预检。
5. 任一步失败，正式配方、活动会话的上一有效草稿和运行快照均保持不变。
6. `cancel()` 删除本会话工作区并清空内存；不修改正式配方。

## 7. RecipeStore 事务与错误返回

保存固定顺序：

1. 校验 recipeId、模式专用 JSON、Profile数量、字段范围和资源相对路径。
2. 在配方根同级创建 `<uuid>.tmp.<transaction-uuid>`。
3. 写 `recipe.json` 并复制全部资源；资源源文件只在保存调用期间允许是绝对路径。
4. 从临时目录重新加载 JSON，逐字段比较，完整解码全部已声明图片、解析YAML并校验字符数据内部一致性，形成 `PreparedRecipe`；目标字符为空或目标覆盖不足不是事务失败，留给启动预检拒绝。
5. 正式目录存在时将其原子改名为 `<uuid>.bak.<transaction-uuid>`。
6. 将临时目录改名为正式UUID目录；失败则恢复备份。
7. 提交成功后删除备份；清理备份失败记录诊断但不回滚已验证的新正式版本。

加载固定拒绝：

- 请求ID不是规范UUID、目录名大小写/ID不匹配、目录/资源为符号链接。
- `recipe.json`缺失、不是对象、未知/缺失字段、Schema不等于1、模式参数形状错误。
- 资源路径越界、资源缺失/空文件/不可解码、YAML缺节点或点数错误。
- 配方根下非UUID目录（包括旧模板名目录）不进入目录列表；显式请求返回 `RECIPE_LEGACY_FORMAT_REJECTED`。
- 不调用 `QSettings`，不搜索 `app_settings.appset`，不从外部目录补资源。

Recipe错误码：`RECIPE_ROOT_UNAVAILABLE`、`RECIPE_LEGACY_FORMAT_REJECTED`、`RECIPE_ID_INVALID`、`RECIPE_DIRECTORY_MISMATCH`、`RECIPE_JSON_MISSING`、`RECIPE_JSON_MALFORMED`、`RECIPE_SCHEMA_UNSUPPORTED`、`RECIPE_FIELD_MISSING`、`RECIPE_FIELD_TYPE_INVALID`、`RECIPE_FIELD_RANGE_INVALID`、`RECIPE_CONSTRAINT_VIOLATION`、`RECIPE_TISSUE_PARAMETERS_INVALID`、`RECIPE_STAMP_PROFILE_INVALID`、`RECIPE_WORD_PROFILE_INVALID`、`RECIPE_OCR_PROFILE_INVALID`、`RECIPE_BARCODE_PROFILE_INVALID`、`RECIPE_ASSET_PATH_INVALID`、`RECIPE_ASSET_MISSING`、`RECIPE_ASSET_IMAGE_CORRUPT`、`RECIPE_CHARACTER_TEMPLATE_INVALID`、`RECIPE_CALIBRATION_INVALID`、`RECIPE_VERIFY_FAILED`、`RECIPE_BACKUP_FAILED`、`RECIPE_COMMIT_FAILED`、`RECIPE_ROLLBACK_FAILED`。

## 8. 新建默认、加载与损坏处理汇总

- `MachineSettings::defaults()` 是机器字段唯一默认来源；首次启动只返回该对象，不读取UI静态值。
- `createProductRecipe(Tissue)` 唯一写入 `roughnessThreshold=6.0`。
- 模板Profile唯一写入 `imageThresholdPercent=70`、二维码 `1/8/60/true`；targetText、characterSourceSize和characterBoxes分别以空字符串、`0,0`和空数组明确表示尚未执行对应编辑步骤，不伪造业务内容。基础产品模板可以事务发布并继续编辑，但未完成当前模式启动必需项时必须由启动预检拒绝。
- 正式设置或配方一旦存在但损坏，必须明确拒绝；不能默默恢复默认或继续上一控件值启动。
- Store返回结构化错误，UI保留当前中文操作语义并决定如何展示；Store、Recipes与PreparedRecipe不得依赖 `QMessageBox`、Widget或UI类型。
- 正常运行只取得一份 `MachineSettingsSnapshot` 与一份 `PreparedRecipeSnapshot`。编辑、恢复默认、切换配方在下一次启动才形成新快照。

## 9. 旧到新字段映射与删除

| 旧数据/类型 | 新唯一位置 | 阶段1处理 |
|---|---|---|
| `GlobalSettings` | `MachineSettings`；纸巾阈值例外移入ProductRecipe | 删除旧类型 |
| `AppSettingsManager`全局INI | `MachineSettingsStore` JSON | 删除旧类、INI读写、版本升级和Home fallback |
| `TemplatePrivateSettings` | `ProductRecipe`模式专用Profile | 删除旧类型和 `app_settings.appset` |
| `TemplateRecipeDraftSession` / `TemplateRecipeEditSession` | `RecipeEditorSession` | 合并并删除平行会话 |
| `RecipeSelection`中绝对路径运行桥 | `PreparedRecipeSnapshot` | Prepared后Runtime不持有路径 |
| `template_runtime_profile.h`对设置/detection实现的反向依赖 | `prepared_recipe.*`；实现准备细节留在cpp | 删除反向依赖头 |
| `templateBaseDirPath`、`templateDirPathsByMode` | 无 | 删除旧目录选择/恢复/fallback |
| `publishedRecipeIdsByMode` | `MachineSettings.ui.lastRecipeIdByMode` | 只记UUID，不兼容旧路径 |
| UI静态曝光128、增益0、阈值80 | 无 | 构造后从唯一Store/Recipe值覆盖；不作为default |

## 10. Schema 静态冻结检查

- [x] 根对象、`schemaVersion`、应用数据根和目录规则已固定。
- [x] 曝光、增益、触发、旋转、通道、检测间隔已固定。
- [x] PLC连接、固定地址、工艺参数、单位和正常0/49时序边界已固定。
- [x] 存图范围、目录、原图/标注图组合和JPEG质量92已固定。
- [x] 纯UI模式/配方记忆和分隔条字段已与业务参数分离。
- [x] 五种DetectionMode、单/多Profile、纸巾无模板和唯一默认已固定。
- [x] tracking ROI、日期ROI、二维码ROI、目标文本、字符框、匹配阈值和二维码参数已固定。
- [x] tracking/raw/stampRing/character图片与 `calibrate_config.yaml` 资源合同已固定。
- [x] PreparedRecipe不可变资产、五模式准备结果和错误码已固定。
- [x] 首启、新建、加载拒绝、整目录事务、回滚和结构化错误返回已固定。
- [x] 旧设置、旧模板、旧目录和所有兼容读取明确删除。

本次反向盘点没有遗留会改变已确认算法判定、阈值、正常PLC时序或额外用户功能去留的未决字段；后续实现不得自行偏离本文。
