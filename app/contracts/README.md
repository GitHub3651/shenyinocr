# contracts：稳定合同

本目录保存多个模块共同依赖、但不拥有运行副作用的稳定定义。当前只有五模式元数据和二维码默认参数。

## 文件

| 文件 | 职责 |
|---|---|
| `detection_mode.h/.cpp` | 唯一登记 `DetectionMode`、稳定 `modeId/uiId`、中文名、定位类型、资源要求、呈现和存图策略。 |
| `barcode_parameter_defaults.h` | 二维码格式掩码、ROI 外扩、解码预算和回退开关的唯一默认值。 |

## 五模式唯一分类

| 模式 | `modeId` | 定位类型 | 模板数量 |
|---|---|---|---|
| 刚印检测 | `stamp` | `SingleTemplate` | 0 或 1 |
| 字库匹配 | `word` | `MultipleTemplates` | 0 到多个 |
| 深度 OCR | `ocr` | `SingleTemplate` | 0 或 1 |
| 纸巾检测 | `tissue` | `WholeFrame` | 无模板 |
| 二维码+三期 | `barcodeWord` | `MultipleTemplates` | 0 到多个 |

UI、设置校验、启动预检和 Detection 都查询这张表，不再维护第二套单选/多选映射。

## 维护规则

- 改模式 ID 会影响持久化 Schema，不能只改界面文字。
- 中文显示名可修改；内部 `modeId` 和 `uiId` 不能随意改。
- 新增模式必须同步设置 Schema、模板规则、Detection Registry、UI 显隐和验证矩阵。
- 本目录不能依赖 Application、Runtime、UI、设备 SDK 或 Store。
