# engines：OCR 与二维码识别能力的端口和实现

## 一句话理解

`engines` 封装“外部识别能力”，目前只有二维码解码和 PaddleOCR 两类。它们不是完整检测模式：Detection Pipeline 会把定位后的 ROI 交给 Engine，再把返回文字与产品目标、阈值和业务规则组合成最终 OK/NG。

## 与 detection 的区别

```text
Detection Pipeline
├─ 决定取哪个 ROI
├─ 决定目标文字和判定规则
├─ 调用 Engine 获取识别结果
└─ 生成统一 DetectionResult

Engine
├─ 接收图像/解码选项
├─ 调用 DLL 或模型推理
└─ 返回文字、角点、状态和诊断
```

因此 `ocr/` 和 `barcode/` 是可替换的技术能力，不是另外两套 Runtime。

## 当前文件树

```text
engines/
├─ barcode/
│  ├─ barcode_types.h
│  ├─ barcode_decoder.h
│  └─ vendor/
│     ├─ barcode_decoder_api.h
│     ├─ barcode_decoder_adapter.h
│     └─ barcode_decoder_adapter.cpp
├─ ocr/
│  ├─ ocr_engine.h
│  └─ vendor/
│     ├─ paddle_ocr_engine.h
│     ├─ paddle_ocr_engine.cpp
│     └─ paddle/
│        ├─ include/
│        │  ├─ clipper.h
│        │  ├─ config.h
│        │  ├─ ocr_det.h
│        │  ├─ ocr_rec.h
│        │  ├─ postprocess_op.h
│        │  ├─ preprocess_op.h
│        │  └─ utility.h
│        └─ src/
│           ├─ clipper.cpp
│           ├─ config.cpp
│           ├─ ocr_det.cpp
│           ├─ ocr_rec.cpp
│           ├─ postprocess_op.cpp
│           ├─ preprocess_op.cpp
│           └─ utility.cpp
└─ README.md
```

## barcode：二维码能力

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `barcode/barcode_types.h` | 格式枚举、读取状态、解码选项和结果；结果包含原始字节、文字、角点、诊断和策略 ID。 | 值类型可跨层使用，但不能携带 DLL 句柄。 |
| `barcode/barcode_decoder.h` | 定义 `IBarcodeDecoder` 的加载状态、错误和灰度图解码端口。 | Detection 只依赖接口；失败返回结构化状态，不弹窗。 |
| `barcode/vendor/barcode_decoder_api.h` | 声明 `BarcodeDecoder.dll` 的导出函数、参数和 ABI 常量。 | 必须与 DLL 构建端严格一致。 |
| `barcode/vendor/barcode_decoder_adapter.h` | 声明 Windows DLL 适配器、函数指针表和 PImpl。 | 隔离 `LoadLibrary/GetProcAddress`。 |
| `barcode/vendor/barcode_decoder_adapter.cpp` | 从应用目录加载 DLL，尝试原图、反色、阈值、旋转等 fallback，转换结果和角点，并缓存成功策略。 | fallback 顺序、超时预算和失败清理属于已验证行为。 |

二维码正式调用链：

```text
BarcodeWordDetectionPipeline
→ IBarcodeDecoder
→ BarcodeDecoderAdapter
→ BarcodeDecoder.dll
→ BarcodeDecodeResult
→ Pipeline 组合日期字符结果并判定
```

## ocr：OCR 能力

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `ocr/ocr_engine.h` | 定义最小 `IOcrEngine::recognize(cv::Mat)` 端口。 | 不暴露模型目录、Predictor 或 Paddle 类型。 |
| `ocr/vendor/paddle_ocr_engine.h` | 声明 PaddleOCR 适配器，使用 PImpl 隐藏 Detector/Recognizer。 | 每次检测运行创建一组模型。 |
| `ocr/vendor/paddle_ocr_engine.cpp` | 读取 `config_ocr.txt`，构造并预热 PP-OCRv6 tiny DET/REC predictor，完成文字框检测、阅读顺序排序、透视裁剪和逐框识别。 | 预热发生在采集开始前且结果直接丢弃；每个正式 DET 框对应一个返回项；不在 Engine 内清洗文字或执行 OK/NG 判定。 |

OCR 正式调用链：

```text
OcrDetectionPipeline
→ IOcrEngine
→ PaddleOcrEngine（每次检测运行独立实例）
→ DBDetector 找文字框
→ SortQuadBoxes 按阅读顺序排序
→ Recognizer 逐框识别文字
→ Pipeline 清洗、组合，并在忽略双方换行后与目标字符串比较
```

## paddle/include 与 paddle/src

下面 14 个文件是当前 DET+REC 路径直接使用的 PaddleOCR vendor 实现，应当按组件成对理解：

| 头/源 | 当前职责 |
|---|---|
| `config.h` / `config.cpp` | 解析 CPU/oneDNN、DET 参数、模型和字典路径。 |
| `ocr_det.h` / `ocr_det.cpp` | PP-OCRv6 tiny DB 文本检测：预处理、推理和后处理，输出四点文字框。 |
| `ocr_rec.h` / `ocr_rec.cpp` | 文字框透视裁剪、窄高图旋转、动态宽度预处理、推理和 CTC 解码。 |
| `preprocess_op.h` / `preprocess_op.cpp` | DET/REC 的 Resize、Normalize 和 HWC→CHW。 |
| `postprocess_op.h` / `postprocess_op.cpp` | DB 概率图二值化、轮廓、box score、unclip 和文字框回映。 |
| `utility.h` / `utility.cpp` | 字典读取、官方四点框阅读顺序排序和最大值索引。 |
| `clipper.h` / `clipper.cpp` | 第三方多边形裁剪/偏移库，供 DB 文字框 unclip 使用。 |

这些文件数量多，是因为仓库内保留了 Paddle 推理实现，并不代表项目有 14 个 OCR 模块。业务维护通常不应改动它们。

## 谁构造 Engine

`ApplicationStartup` 创建 Barcode 适配器，并把 `config_ocr.txt` 路径交给 `DetectionRegistry`。每次启动 OCR 检测时，`DetectionRegistry` 在创建检测 Worker 前同步创建一个 `PaddleOcrEngine`；构造函数创建并预热 DET/REC predictor，成功后再由本次运行的 OCR 执行器持有并逐帧复用。Pipeline 不加载 DLL、模型或创建 Predictor。

## 允许放什么

- 对外部识别技术的稳定端口和值类型。
- DLL/模型/第三方库适配器。
- 与引擎本身直接相关的预处理、后处理和错误映射。

## 禁止放什么

- 产品 OK/NG、目标文字比较、统计、PLC 和存图。
- CameraSession、Runtime 状态机和 QWidget。
- 模板持久化和 UI 模式逻辑。
- Pipeline 直接 include `vendor` 头或动态加载 DLL。

## 维护红线

- Barcode ABI 改动必须同步 DLL 两端并验证位数、调用约定和结构布局。
- Paddle 模型、字典、配置和预后处理必须整体匹配。
- `clipper.*`、Snap7 类似的第三方源码不按自研代码风格机械重构。
- vendor 升级要记录来源、版本、库、DLL 和头文件对应关系并做部署验证。

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单、部署脚本、第三方依赖清单和开发者维护指南。
