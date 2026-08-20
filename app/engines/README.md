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
│        │  ├─ ocr_cls.h
│        │  ├─ ocr_det.h
│        │  ├─ ocr_rec.h
│        │  ├─ postprocess_op.h
│        │  ├─ preprocess_op.h
│        │  └─ utility.h
│        └─ src/
│           ├─ clipper.cpp
│           ├─ config.cpp
│           ├─ ocr_cls.cpp
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
| `ocr/vendor/paddle_ocr_engine.h` | 声明 PaddleOCR 适配器，使用 PImpl 隐藏 Detector/Recognizer。 | 模型应在初始化时构造，不能每帧创建。 |
| `ocr/vendor/paddle_ocr_engine.cpp` | 读取 `config1.txt`，构造检测器/识别器，执行文本框检测与识别并合并字符串。 | 当前部署固定 V3 模型与字典；模型变化属于算法和部署变化。 |

OCR 正式调用链：

```text
OcrDetectionPipeline
→ IOcrEngine
→ PaddleOcrEngine
→ DBDetector 找文字框
→ Recognizer 识别文字
→ Pipeline 清洗并与目标字符串比较
```

## paddle/include 与 paddle/src

下面 16 个文件是 PaddleOCR 推理示例/第三方内部实现，应当按组件成对理解：

| 头/源 | 当前职责 |
|---|---|
| `config.h` / `config.cpp` | 解析模型、字典、CPU/GPU/MKLDNN、阈值和图像尺寸配置。 |
| `ocr_det.h` / `ocr_det.cpp` | DB 文本检测：预处理、推理、后处理，输出文字框。 |
| `ocr_rec.h` / `ocr_rec.cpp` | 文字框裁图、Resize、推理和字符解码。 |
| `ocr_cls.h` / `ocr_cls.cpp` | 文本方向分类，决定是否旋转文字图。 |
| `preprocess_op.h` / `preprocess_op.cpp` | Normalize、HWC→CHW、Detector/Recognizer/Classifier 的 Resize/Pad。 |
| `postprocess_op.h` / `postprocess_op.cpp` | DB 概率图二值化、轮廓、box score、unclip 和文字框回映。 |
| `utility.h` / `utility.cpp` | 字典读取、可视化和最大值索引等 vendor 内部工具。 |
| `clipper.h` / `clipper.cpp` | 第三方多边形裁剪/偏移库，供 DB 文字框 unclip 使用。 |

这些文件数量多，是因为仓库内保留了 Paddle 推理实现，并不代表项目有 16 个 OCR 模块。业务维护通常不应改动它们。

## 谁构造 Engine

`ApplicationStartup` 是具体实现唯一组合点：它创建 Barcode 适配器和 PaddleOCR 适配器，注入 `DetectionRegistry`。Pipeline 不应在运行中自己加载 DLL、模型或创建 Predictor。

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
- vendor 升级要记录来源、版本、库/DLL/头文件指纹并做部署验证。

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单、部署脚本、第三方依赖清单和开发者维护指南。
