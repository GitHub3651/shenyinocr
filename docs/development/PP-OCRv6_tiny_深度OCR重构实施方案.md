# PP-OCRv6 tiny 深度 OCR 重构实施方案

- 状态：代码、依赖、模型、部署和当前文档已实施；启动前 DET/REC 预热代码静态检查通过，待用户通过 Qt Creator 构建并在主程序统一验收。
- 权威范围：深度 OCR 从现有 PP-OCRv3 检测、识别和方向分类链切换为 `PP-OCRv6_tiny_det + PP-OCRv6_tiny_rec`，包括项目公共 DET+REC 入口、Paddle Inference、模型和字典、OCR 配置、OCR 引擎、旧实现清理、OCR Pipeline、qmake 工程登记、构建目录中的 OCR 产物、发布资源、当前有效文档与验收门禁。
- 当前进度：生产代码已切换为 PP-OCRv6 tiny DET+REC，Paddle Inference 3.0.0、模型、字典、配置、部署脚本、发布资源和当前文档已同步；OCR 引擎在每次 OCR 检测启动时由 `DetectionRegistry` 同步创建，并在构造阶段完成 DET/REC 预热，成功后才创建 Worker 和启动采集。当前预热改动待用户通过 Qt Creator 构建；主程序运行和真实设备整体验收统一由用户完成。
- 替代关系：本方案是深度 OCR 升级的唯一实施计划；正式产品失效安全、单帧完整耗时、日志系统、模板、UI、PLC、软硬触发和运行 Fault 的现行合同作为本方案约束。
- 实施门禁：工具链固定为当前 Qt Creator Kit、C++11 和 OpenCV 3.4.1；候选依赖固定为 Paddle Inference 3.0.0 Windows x64 CPU 包。源码、依赖和资源已完成，当前预热改动静态检查通过；Qt Creator Release 构建、DLL 加载、模型执行和主程序整体验收统一由用户执行。最终工程使用一套 V6 OCR 源码、依赖、配置和运行资产。

## 1. 最终目标

深度 OCR 最终固定为：

```text
相机原图
  → 现有模板定位和姿态计算
  → datePolygon 跟随产品并旋转校正
  → IOcrEngine::recognize(校正后的 ROI)
      → PP-OCRv6_tiny_det 检测文字框
      → 按 PaddleOCR v3.7.0 规则排序文字框
      → 逐框透视裁剪
      → PP-OCRv6_tiny_rec 识别
      → 返回按阅读顺序排列的原始文本片段
  → 按现有规则清洗并组合多行文本
  → 分别移除识别结果和 targetText 中的换行后，进行大小写敏感的字符级比较
  → AlgorithmVerdict::Ok / AlgorithmVerdict::Ng
```

使用 DET+REC 是因为当前模板只定义一个可能包含多行文字的 `datePolygon`，程序事先不知道每行的准确位置。现有模板格式和单个 `datePolygon` 保持为正式输入。

方向处理由现有模板定位、`prepareOrientedDateRoi()` 和官方识别裁剪中的窄高图旋转规则共同完成，生产 OCR 链固定为 tiny DET+REC。

`IOcrEngine::recognize()` 是项目唯一公共 DET+REC 入口。深度 OCR 在调用前完成定位和整块 ROI 校正；其他功能以后需要 OCR 时，准备自己的图片或 ROI 后调用同一入口，并在调用后完成自己的业务处理。

## 2. 当前工程基线

### 2.1 构建和依赖

- 当前本地 `app/.qtcreator/AutoOCRproject.pro.user` 选择 Qt 5.15.2/MSVC2019 64-bit Kit；`app/AutoOCRproject.pro` 本身只声明 Qt 模块和 C++11，不锁定 Qt 或 MSVC 版本。
- 主工程继续使用 qmake、C++11 和 OpenCV 3.4.1。
- 当前 Paddle Inference 位于 `third_party/paddle_inference_install_dir`，是旧 CPU、MKL/MKLDNN 运行时。
- 当前 OCR 启动入口把 `<exe>/config_ocr.txt` 路径交给 `DetectionRegistry`，由 Registry 在 OCR 检测 Worker 创建前构造 `PaddleOcrEngine`；构造函数创建 DET/REC predictor 并各执行一次预热。
- 当前运行资源唯一部署源是 `dist/ShengYin`；Release 构建后由 `deploy_runtime.ps1` 复制到构建输出目录。

### 2.2 当前生产合同

以下生产合同继续生效：

- `TemplateStore`、`InspectionPositioner`、`TrackingPoseMatcher`、`DetectionPose` 和 `DetectionRoiGeometry` 的职责保持；`DetectionRegistry` 负责创建模式执行器，`DetectionWorker` 只负责逐帧执行。
- 模板 `trackingRoi`、`datePolygon`、模板 Schema 和设置 Schema 保持。
- 正式产品结果只包含 `AlgorithmVerdict::Ok/Ng`。
- 无定位、ROI 无效、OCR 空文本或文本不匹配是正常产品 NG。
- 模型、字典、predictor 创建或启动前预热异常沿现有启动失败链返回；逐帧 DET 或 REC 执行异常沿现有 Worker 链进入 Runtime Fault，不伪装成产品 NG。
- UI、统计、存图、PLC、软硬触发和结果发布顺序保持。
- UI 继续显示由 `processingStartedAt` 到 `ResultService` 发布前计算的完整单帧耗时。
- 生产 OCR 日志继续使用现有单帧最终摘要，以及启动和故障信息。

## 3. 固定技术方案

### 3.1 模型和参考版本

固定使用：

```text
PaddleOCR 源码参考：v3.7.0 tag
检测模型：PP-OCRv6_tiny_det_infer
识别模型：PP-OCRv6_tiny_rec_infer
识别字典：以同一 v3.7.0 tag 的 `PP-OCRv6_tiny_rec.yml` 中 `character_dict_path` 指向的官方字典为唯一来源，部署文件名统一为 `ppocrv6_tiny_dict.txt`
运行设备：Windows x64 CPU
```

Phase 0 先记录该 `character_dict_path` 对应的准确仓库路径和下载 URL，再校验字典至少包含 `A-Z`、`a-z` 和中文字符条目；无法取得该官方字典或无法确认其与 REC 模型一致时停止实施。第 6 节在主程序整体验收中验证中文、英文大写、英文小写和中英混排内容的最终识别能力。

模型来源：

```text
https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv6_tiny_det_infer.tar
https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv6_tiny_rec_infer.tar
```

实施时把 Paddle Inference 包、字典和实际 DLL/LIB 的 URL、版本及编译器信息写入 `third_party/DEPENDENCIES.md`。

仓库内的 PaddleOCR 相关源码由本项目实际使用的 DET 预处理、DB 后处理、透视裁剪、REC 预处理和 CTC 解码组成，来源固定为 v3.7.0 tag，并直接写入现有 OCR vendor 目录。

### 3.2 Paddle Inference 兼容性

当前 Qt Creator Kit 保持不变。测试前先记录候选 Paddle Inference Windows x64 CPU 包的准确 URL、版本和编译器信息，再用仓库外的临时最小程序完成以下验证：

1. 使用当前 Kit 和 OpenCV 3.4.1 完成编译、链接；
2. 直接使用 Paddle Inference API 创建 DET 和 REC predictor，加载两个 `inference.json + inference.pdiparams` 模型，并各完成一次 CPU 推理；
3. 从独立输出目录加载实际需要的运行时 DLL 并正常退出。

临时程序不写入仓库，验证完成后删除，也不成为生产 OCR 入口。它只验证 Paddle Inference 与当前 Kit/OpenCV 3.4.1 的编译、链接、加载和模型执行兼容性，不验收 OCR 准确率、文字框顺序、业务判定或主程序功能。三项验证全部通过后继续主程序整体验收；任一项失败即停止验证并由用户决定新的工具链方案，不改造项目去迁就不匹配的二进制包。正式生产代码仍只通过 `IOcrEngine::recognize()` 对业务提供 DET+REC。

### 3.3 发布资源目录

模型和字典固定放在现有部署源内部：

```text
dist/ShengYin/
├─ ShengYin.exe
├─ config_ocr.txt
├─ OCR/
│  └─ PP-OCRv6_tiny/
│     ├─ det/
│     │  ├─ inference.json
│     │  └─ inference.pdiparams
│     ├─ rec/
│     │  ├─ inference.json
│     │  └─ inference.pdiparams
│     └─ ppocrv6_tiny_dict.txt
├─ paddle_inference.dll
└─ 新运行时实际需要的其他 DLL
```

部署源固定为第 3.3 节目录。

### 3.4 OCR 配置

最终配置入口固定为 `<exe>/config_ocr.txt`，继续使用空格分隔的键值格式。配置只保留本次运行实际需要的字段，初始内容为：

```text
cpu_math_library_num_threads 4
use_mkldnn 1

max_side_len 960
det_db_thresh 0.2
det_db_box_thresh 0.4
det_db_unclip_ratio 1.4
det_model_dir ./OCR/PP-OCRv6_tiny/det/

rec_model_dir ./OCR/PP-OCRv6_tiny/rec/
char_list_file ./OCR/PP-OCRv6_tiny/ppocrv6_tiny_dict.txt
```

运行后端固定为 Paddle Inference CPU 和 oneDNN，`cpu_math_library_num_threads` 固定为 4。

DET 参数固定使用 PaddleOCR v3.7.0 tiny 官方值：`max_side_len=960`、`det_db_thresh=0.2`、`det_db_box_thresh=0.4`、`det_db_unclip_ratio=1.4`。这些参数不作为验收阶段的调节项；主程序整体验收未通过时停止并由用户重新决定，本计划不自动调参。

### 3.5 DET 处理

DET 输入是 `prepareOrientedDateRoi()` 返回的完整多行 OCR ROI，继续使用当前 `padding=0`。

预处理、动态输入尺寸、归一化、DB 后处理和四点框恢复以 PaddleOCR v3.7.0 的 PP-OCRv6 tiny 配置及 C++ 推理实现为准。

DET 输出为四点文字框。低于 `det_db_box_thresh` 的框被过滤；无有效框返回正常空识别结果，不抛异常。

### 3.6 阅读顺序

阅读顺序以 PaddleOCR v3.7.0 `deploy/cpp_infer/src/common/processors.cc` 中的 `ComponentsProcessor::SortQuadBoxes` 为依据，把该排序算法移植到现有 `utility.h/.cpp`，不引入官方整套组件框架。排序后的每个 DET 框在 `vector<string>` 中对应一个 REC 输出项，识别为空时该项保留为空字符串。

### 3.7 REC 处理

每个四点框按 v3.7.0 实现进行透视裁剪；窄高裁剪沿用官方规则旋转后再识别。REC 输入高度固定为 48，按宽高比缩放和补齐，使用 `ppocrv6_tiny_dict.txt` 完成 CTC 解码。

### 3.8 文本清洗与判定

文本判定沿用当前生产规则：

- 每个识别片段只保留 ASCII 字母和数字、非 ASCII 字节，以及 `-`、`.`、`:`；其他字符包括空格被删除；
- ASCII 大小写保持且大小写敏感；
- Pipeline 跳过清洗后为空的项，并在其余相邻返回项之间插入一个换行；不对多个 DET 框另做物理行分组或合并，组合结果中的换行继续用于结果展示和日志；
- 比较前分别从组合后的识别文本和模板 `targetText` 中移除 `\r`、`\n`；`targetText` 除移除换行外不清洗、不拆分，也不自动改变其他字符；
- 去除换行后的识别文本非空，且与去除换行后的 `targetText` 大小写敏感地完全相等时为 OK，否则为 NG。除换行外，其他字符仍逐字符参与比较。

OCR Engine 返回识别片段，`OcrDetectionPipeline` 负责最终清洗和比较。

### 3.9 正常 NG 与 Runtime Fault

| 情况 | 结果 |
|---|---|
| 模板未定位、OCR ROI 无效 | 正常产品 NG |
| DET 无有效文字框或 REC 全部为空 | 返回空 `vector` 或内容全为空的返回项，由现有 Pipeline 形成正常产品 NG |
| 清洗后文本为空或与目标不一致 | 正常产品 NG |
| 模型、字典、配置或 predictor 加载失败 | `DetectionRegistry` 返回 OCR 启动失败，不创建检测 Worker，也不启动图像采集 |
| 启动前预热的 DET 或 REC `Predictor::Run()` 返回 `false` | 立即抛出异常，由 `DetectionRegistry` 返回 OCR 启动失败，不启动采集 |
| 逐帧 DET 或 REC 的 `Predictor::Run()` 返回 `false` | 立即抛出异常，经 DetectionWorker 进入 Runtime Fault |
| 逐帧 DET/REC/Paddle 异常 | 抛出异常，经 DetectionWorker 进入 Runtime Fault |

### 3.10 日志和耗时

- OCR 引擎完成 predictor 创建和 DET/REC 预热后记录一次 `ocr.engine_loaded` 摘要，内容为模型族和配置路径。
- OCR 初始化失败使用现有启动日志类别记录一次错误；运行期异常使用现有运行日志类别记录一次错误。
- 每帧最终识别文本、判定和诊断进入现有单帧最终摘要。
- 生产 `DetectionResult`、UI 和日志中的正式耗时继续只使用 `processingStartedAt` 到 `ResultService` 发布前的完整单帧耗时。

## 4. 生产代码改造范围

### 4.1 项目公共 DET+REC 接口

现有 `app/engines/ocr/ocr_engine.h` 保持不变：

```cpp
virtual std::vector<std::string> recognize(cv::Mat &image) = 0;
```

该方法的合同固定为：

- 输入是调用方准备好的完整图片或 ROI；
- 方法内部一次完成 DET、文字框排序、透视裁剪和 REC；
- 输出是按阅读顺序排列的原始文本片段，每个 DET 框对应一个返回项，识别为空时该项为空字符串；
- DET 无框时返回空 `vector`，REC 全部为空时返回等量空字符串，由调用方形成空文本；引擎执行失败抛出异常；
- 公共入口不接收模板、定位结果或 `targetText`，不执行文本清洗、OK/NG 判定、统计、存图或 UI 更新；
- 业务调用方只依赖 `ocr_engine.h` 和 `IOcrEngine`，不直接包含 Paddle vendor 头文件，也不直接调用 Detector 或 Recognizer。

所有业务调用方统一通过 `IOcrEngine` 引用调用 `recognize()`。`PaddleOcrEngine` 是当前唯一具体实现，在构造时创建 DET 和 REC predictor，以固定三通道图和有效四点框分别执行一次预热，并在该接口方法的实现中完成上述正式识别流程。每次 OCR 检测启动时，`DetectionRegistry` 在创建检测 Worker 前同步创建一个 `PaddleOcrEngine`；本次运行的 OCR 执行器持有该实例并逐帧复用，Worker 释放后随执行器一起释放。以后其他功能复用 OCR 时，由其现有组合点取得 `IOcrEngine` 引用并直接调用该方法。

相关生产代码目录固定为：

```text
app/
├─ AutoOCRproject.pro
├─ startup/
│  └─ application_startup.cpp
│     └─ 读取 config_ocr.txt，向 DetectionRegistry 提供配置路径
├─ detection/
│  ├─ detection_registry.h
│  ├─ detection_registry.cpp
│  │  └─ 启动 OCR 时同步创建引擎，并由 OCR 执行器交给 Pipeline 使用
│  ├─ common/
│  │  └─ detection_roi_geometry.h
│  │     └─ prepareOrientedDateRoi() 完成定位后 ROI 校正
│  └─ detectionmode/ocr/
│     ├─ ocr_detection_pipeline.h
│     └─ ocr_detection_pipeline.cpp
│        └─ 定位和 ROI → IOcrEngine::recognize() → 清洗和判定
├─ engines/ocr/
│  ├─ ocr_engine.h
│  │  └─ 项目唯一公共 DET+REC 接口
│  └─ vendor/
│     ├─ paddle_ocr_engine.h
│     ├─ paddle_ocr_engine.cpp
│     │  └─ Paddle 实现和 DET+REC 编排
│     └─ paddle/
│        ├─ include/
│        │  └─ 配置、DET、REC、预处理、后处理、工具和 clipper 声明
│        └─ src/
│           └─ 配置、DET、REC、预处理、后处理、工具和 clipper 实现
└─ system_support/deployment/
   └─ deploy_runtime.ps1
      └─ 部署模型、字典和 Paddle 运行时
```

### 4.2 Paddle OCR vendor 实现

以下现有目录在同一实施批次直接替换为 V6 实现。最终 vendor 文件集合固定为：

```text
app/engines/ocr/vendor/paddle_ocr_engine.h
app/engines/ocr/vendor/paddle_ocr_engine.cpp
app/engines/ocr/vendor/paddle/include/clipper.h
app/engines/ocr/vendor/paddle/include/config.h
app/engines/ocr/vendor/paddle/include/ocr_det.h
app/engines/ocr/vendor/paddle/include/ocr_rec.h
app/engines/ocr/vendor/paddle/include/postprocess_op.h
app/engines/ocr/vendor/paddle/include/preprocess_op.h
app/engines/ocr/vendor/paddle/include/utility.h
app/engines/ocr/vendor/paddle/src/clipper.cpp
app/engines/ocr/vendor/paddle/src/config.cpp
app/engines/ocr/vendor/paddle/src/ocr_det.cpp
app/engines/ocr/vendor/paddle/src/ocr_rec.cpp
app/engines/ocr/vendor/paddle/src/postprocess_op.cpp
app/engines/ocr/vendor/paddle/src/preprocess_op.cpp
app/engines/ocr/vendor/paddle/src/utility.cpp
```

按 v3.7.0 直接重写配置、DET、REC、预处理、DB 后处理、透视裁剪和 CTC 解码。`clipper.h/.cpp` 继续供 DB unclip 使用。最终源码只保留从 `PaddleOcrEngine::recognize()` 可达的 DET+REC 路径，并完成以下清理：

- 删除 `ocr_cls.h/.cpp`、`Classifier`、`Impl::classifier`、REC 的 `Classifier *` 参数和调用、`ClsResizeImg`，以及所有分类器工程条目；
- `OCRConfig` 只解析第 3.4 节字段，删除 `use_gpu`、`gpu_id`、`gpu_mem`、`use_angle_cls`、`cls_model_dir`、`cls_thresh`、`visualize`、`use_tensorrt`、`use_fp16` 的解析、成员和传参；
- DET、REC 和预处理直接执行固定 CPU/oneDNN 路径，删除 GPU、TensorRT、FP16、可视化分支及相关成员、参数和注释代码；
- REC 只保留一个正式识别入口和一个透视裁剪实现，删除 `RunOCR`、返回 `cv::Rect` 的重复裁剪重载、无用途的 `PostProcessor` 成员，以及未进入返回值的 score、计数和局部变量；
- Utility 和 DB 后处理删除 `VisualizeBboxes`、裸指针版 `Mat2Vec`、注释掉的 `PolygonScoreAcc` 实现和其他无调用声明；字典读取失败抛出异常，由 `DetectionRegistry` 按本节规定返回启动失败；
- 模型加载只使用 `inference.json + inference.pdiparams` 和 `paddle_inference_api.h`，删除 `inference.pdmodel`、`paddle_api.h` 与旧 Paddle API 残留；
- 仅在第 4.2 节本次替换的 Paddle OCR vendor 文件内，删除由移除 CLS、旧后端、重复入口和无调用工具直接产生的未使用 include、`using namespace`、成员、局部变量、空分支、重复实现和整段注释代码；不扩展为其他目录的通用代码清理。项目自有 vendor 头文件中的声明必须都有最终调用方或作为其直接实现依赖。

每次 OCR 检测运行创建独立的 `PaddleOcrEngine`。`DetectionRegistry` 在检测 Worker 创建前同步完成配置读取、DET/REC predictor 创建和两次预热；预热结果直接丢弃，不进入 Pipeline。本次运行的执行器持有该引擎，Worker 释放后引擎随执行器释放。predictor 创建或预热失败沿现有启动失败链返回，不启动图像采集。

按已锁定模型的输入输出合同直接执行。DET 和 REC 每次调用 `Predictor::Run()` 都必须检查返回值；预热期间返回 `false` 时进入启动失败，逐帧推理期间返回 `false` 时经现有 Worker 链进入 Runtime Fault，不得转成空识别结果或普通 NG。DET 正常完成但无框时返回空 `vector`；有框时每框保留一个 REC 返回项，包括空字符串。

### 4.3 OCR Pipeline

`app/detection/detectionmode/ocr/ocr_detection_pipeline.cpp` 按以下顺序组装公共 OCR 能力：

1. `detect(DetectionWorkItem, targetText, IOcrEngine)` 使用现有定位结果并调用 `prepareOrientedDateRoi(..., 0)` 取得校正后的 OCR ROI；
2. `detect(cv::Mat, targetText, IOcrEngine)` 把 ROI 传给 `IOcrEngine::recognize()`；
3. Pipeline 对返回的 `vector<string>` 执行现有清洗和换行连接，保留带换行的组合结果；比较时分别移除组合结果和 `targetText` 中的 `\r`、`\n`，再进行大小写敏感的精确比较；
4. 空结果形成现有正常 NG 诊断，Paddle 执行异常向外传播并进入 Worker Fault。

定位、模板和业务判定留在 Pipeline，DET+REC 留在公共 OCR 接口实现中。Pipeline 只增加上述忽略换行的比较规则，其他行为保持不变。

### 4.4 启动、Registry 和工程文件

- `application_startup.cpp` 把 OCR 配置路径改为 `<exe>/config_ocr.txt`，向 `DetectionRegistry` 提供配置路径，其余启动流程不变。
- `DetectionRegistry` 在 OCR 分支同步创建本次运行的 `PaddleOcrEngine`，构造函数完成 predictor 创建和 DET/REC 预热后才返回持有该引擎的检测执行器；非 OCR 分支不创建 OCR 引擎。
- `AutoOCRproject.pro` 的 Paddle vendor 条目严格等于第 4.2 节文件集合；Paddle include/lib 块按验证通过的依赖包实际目录和导入库重写，删除旧包专用的 include 路径、链接库和方向分类条目。
- Qt、C++11、OpenCV 3.4.1、其他第三方依赖和应用模块保持。

### 4.5 部署、构建产物和当前文档

`app/system_support/deployment/deploy_runtime.ps1`：

- 仍从 `dist/ShengYin` 复制资源；
- 复制前在目标目录删除 OCR 自有的 `Model/`、`OCR/`、`config1.txt`、`en_dict.txt`，以及旧依赖包提供的 Paddle 运行时文件，再从发布源复制最终资源；现有 `ShengYin.exe` 和非 OCR 资源保持；
- 删除 V3 DET/REC/CLS 模型和 `en_dict.txt` 的必需文件检查；
- 增加 `config_ocr.txt`、第 3.3 节 V6 DET/REC/字典和新 Paddle 运行时 DLL 的必需文件检查；
- 从 `config_ocr.txt` 读取配置，并把 V3 配置内容检查替换为 V6 tiny 模型路径检查；
- 成功消息使用 V6 tiny 运行资源名称。

`third_party/paddle_inference_install_dir` 先清空原包内容，再放入验证通过的完整依赖包，禁止覆盖式复制；同时删除仓库中未被引用的 `third_party/en_dict.txt`。`dist/ShengYin` 中删除整个 `Model/`、`config1.txt`、`en_dict.txt` 和旧 Paddle 运行时文件，写入 `config_ocr.txt`，再复制第 3.3 节模型、字典及新依赖包实际需要的 DLL；相机、PLC、Qt、OpenCV、二维码等非 OCR 资源保持。

现有 `build/` 目录原地保留，不删除非 OCR 构建产物和 `build/release/logs/`。实施时只删除以下旧 OCR 产物：

- `build/release/Model/`、`build/release/config1.txt`、`build/release/en_dict.txt`；
- `build/release/ocr_cls.obj`、`ocr_det.obj`、`ocr_rec.obj`、`paddle_ocr_engine.obj`、`ocr_detection_pipeline.obj`、`config.obj`、`postprocess_op.obj`、`preprocess_op.obj`、`utility.obj`、`clipper.obj` 和 `application_startup.obj`；
- `build/release` 中旧依赖包提供的 Paddle 运行时文件，随后由部署脚本复制最终依赖包所需文件。

`build/Makefile`、`build/Makefile.Debug` 和 `build/Makefile.Release` 不手工编辑；Phase 3 在现有构建目录执行 Run qmake 原位重新生成，并检查其中不再登记旧 OCR 文件。Release Rebuild 重新生成上述对象和 `ShengYin.exe`。

以下当前有效文档与最终实现同步：

- `third_party/DEPENDENCIES.md`；
- `dist/ShengYin/README.txt`；
- `app/engines/README.md`；
- `docs/release-package.md`；
- `docs/development/OCRGangYin开发者代码结构与维护指南.md`；
- `docs/development/OCRGangYin现有功能对照表.md`。

这些文档只描述 V6 tiny DET+REC、`config_ocr.txt`、最终文件结构、依赖版本和 `PaddleOcrEngine` 具体实现。发布说明、依赖说明和部署脚本引用的发布文件集合必须与第 3.3 节及保留的非 OCR 资源一致，不保留最终文件集合之外的发布辅助文件引用。

## 5. 实施阶段

### Phase 0：依赖兼容性冒烟

1. 下载两个固定模型、REC 配置中 `character_dict_path` 指向的同 tag 官方字典和候选 Paddle Inference CPU 包。
2. 在测试前记录模型、字典和 Paddle Inference 包的准确 URL，以及依赖包版本和编译器信息。
3. 在仓库外编写临时最小程序，直接创建 DET/REC predictor，完成第 3.2 节的编译、链接、模型执行和 DLL 加载兼容性验证，然后删除临时程序；该程序不承担 OCR 功能验收。
4. 用户执行构建和运行；Agent 只负责源码、工程文件和静态检查。

门禁：三项兼容性验证全部通过后再继续主程序整体验收；失败则停止验证并由用户决定工具链方案。

### Phase 1：直接替换生产 OCR

1. 保持 `IOcrEngine` 接口不变，原位替换 `PaddleOcrEngine` 和 vendor 实现。
2. 按第 4.2 节删除分类器、旧后端分支、重复入口、无调用工具和注释废代码。
3. `PaddleOcrEngine` 构造函数在创建 DET/REC predictor 后各执行一次预热；`PaddleOcrEngine::recognize()` 实现唯一公共接口 `IOcrEngine::recognize()`，向现有 `OcrDetectionPipeline` 返回排好序的识别结果；Pipeline 仅增加第 3.8 节规定的忽略换行比较规则；Registry 保存 OCR 配置路径，并在 OCR 检测 Worker 创建前同步创建本次运行的引擎；启动入口把 OCR 配置文件名改为 `config_ocr.txt`。
4. 重写 qmake 的 Paddle vendor 文件清单和 Paddle include/lib 块。
5. 一次性替换 `third_party/paddle_inference_install_dir`，将 OCR 配置统一为 `config_ocr.txt`，并替换 `dist/ShengYin` 中的 OCR 资产和 Paddle DLL。
6. 更新部署脚本、第 4.5 节列出的依赖清单和当前有效文档。
7. 定向删除第 4.5 节列出的旧 OCR 构建产物，保留 `build/` 中的非 OCR 文件和历史日志。

门禁：Paddle vendor 实现只有第 4.2 节文件集合；部署源只包含 `config_ocr.txt`、V6 tiny OCR 资产和新 Paddle 运行时依赖；第 4.5 节指定的旧 OCR 构建产物已删除。

### Phase 2：静态收口

Agent 执行：

- 在项目自有 OCR 源码、`config_ocr.txt`、`AutoOCRproject.pro`、`deploy_runtime.ps1` 和第 4.5 节列出的实现说明文档中，检查 `ocr_cls`、`Classifier`、`ClsResizeImg`、`RunOCR`、`use_angle_cls`、`cls_model_dir`、`cls_thresh`、`visualize`、`use_gpu`、`gpu_id`、`gpu_mem`、`use_tensorrt`、`use_fp16`、`inference.pdmodel`、`paddle_api.h`、`VisualizeBboxes`、`PostProcessor::Mat2Vec(`、`PolygonScoreAcc`、V3 模型名、`config1.txt` 和 `en_dict.txt` 等旧业务符号与路径，结果必须为零；`third_party/en_dict.txt` 必须不存在；
- 对 `third_party/paddle_inference_install_dir` 只核对候选包版本、完整文件集合及 qmake 的 include/lib 引用，不要求 Paddle Inference SDK 自身源码、头文件或二进制内部的 `use_gpu`、`use_tensorrt`、`paddle_api.h` 等 SDK 合法内容消失，也不全文扫描任何 Paddle DLL；
- 按第 4.5 节的明确文件清单检查 `build/release`，确认旧 OCR 模型、配置、字典、对象文件和旧运行库已删除；不对构建产物或 `build/release/logs/` 做关键词全文扫描；
- 检查 `dist/ShengYin` 的文件集合、文本配置和说明，并核对第 4.5 节列出的发布说明、依赖说明和部署脚本不再引用最终发布文件集合之外的辅助文件；不扫描 DLL，也不对尚未由本次构建替换的 `dist/ShengYin/ShengYin.exe` 做内容检查；该 EXE 在 Phase 3 替换后统一验收；
- 检查 `ocr_rec.*` 只有一个识别入口和一个透视裁剪实现，Recognizer 不持有 DB 后处理对象；
- 检查 `IOcrEngine::recognize()` 是唯一公共 DET+REC 入口；`app/engines/ocr/vendor/paddle/` 之外没有业务文件直接调用 Detector、Recognizer 或包含其头文件；
- 检查 `IOcrEngine` 无差异，`OcrDetectionPipeline` 仅包含第 3.8 节规定的忽略换行比较调整，并核对 qmake Paddle vendor 条目严格等于第 4.2 节、Paddle include/lib 仅来自最终依赖包、部署路径严格等于第 3.3 节；
- 对比验证包、`third_party/paddle_inference_install_dir`、qmake 链接项、发布源和部署输出的 Paddle 文件集合；旧包独有文件必须为零，最终包所需文件必须一致；
- 仅核对第 4.2 节本次替换的 Paddle OCR vendor 文件不存在由 CLS、旧后端、重复入口和无调用工具直接产生的无调用声明、重复实现、未使用成员和整段注释代码，不检查或清理其他目录的通用代码问题；
- 核对修改文件严格属于第 4 节范围，Pipeline 只有第 3.8 节规定的忽略换行比较调整，UI 和 Schema 文件无差异；`application_startup.cpp` 只提供 OCR 配置路径，OCR 初始化异常由 `DetectionRegistry` 转为启动失败；
- 检查预热只位于 `PaddleOcrEngine` 构造函数，直接复用现有 DET/REC `Run()`，不增加公开接口、配置、成员状态或兼容路径；
- 执行 `git diff --check`；
- 更新本计划和计划索引的实际进度。

不得把静态通过表述为编译或运行通过。

Git 历史及明确标记为历史记录的执行日志不属于生产残留检查范围。第三方完整 SDK 按版本、文件集合和实际引用检查，不把其内部实现关键词当作项目旧实现残留。

### Phase 3：用户构建与主程序整体验收

用户保留现有 `build/` 目录，在第 4.5 节的定向清理完成后，使用当前 Qt Creator Kit 执行 Run qmake、Release Rebuild 和运行。Run qmake 后检查三个 Makefile 不再登记旧 OCR 源文件；构建后检查 `build/release` 中与 OCR 有关的活动文件只包括 `config_ocr.txt`、V6 tiny OCR 资产和最终 Paddle 运行时，不存在旧 OCR 文件或目录，非 OCR 文件和历史日志保持。全部实现完成后，用户使用真实相机、软触发、硬触发和实际产品，通过主程序完整链路统一执行第 6 节整体验收，不设置 `IOcrEngine::recognize()` 独立功能验收程序。全部验收通过后，把本次生成的 `ShengYin.exe` 写入 `dist/ShengYin`。

替换新 `ShengYin.exe` 后，最终核对项目自有源码、qmake、配置、部署脚本、实现说明文档，以及 `build/release` 和 `dist/ShengYin` 的最终文件集合；不得存在旧 OCR 业务路径或资产。Paddle Inference 完整 SDK 和运行时仍按版本、文件集合与实际引用核对，不扫描其内部实现关键词；`build/release/logs/`、Git 历史和明确作为历史记录的执行日志除外。全部门禁通过后，把本计划更新为完成状态并同步计划索引。

提交、标签、推送或分支操作不属于自动步骤，只按用户单独授权执行。

## 6. 验收标准

### 6.1 功能和判定

- 功能和判定验收全部使用主程序完整链路执行：定位 → ROI → `IOcrEngine::recognize()` → 清洗和换行组合 → 移除双方换行后与 `targetText` 比较 → OK/NG；
- 中文、英文全大写、英文全小写和中英混排内容均在主程序中完成识别和判定；
- 每个 DET 框按官方阅读顺序对应一个 REC 返回项，深度 OCR Pipeline 使用现有定位功能取得 ROI 并调用唯一公共 OCR 入口完成识别；
- Pipeline 跳过清洗后的空项，并在其余返回项之间插入一个换行；组合结果保留换行用于展示和日志，但比较时忽略识别结果和 `targetText` 中的 `\r`、`\n`，因此 `targetText` 可写成一行或多行，换行位置不影响 OK/NG；
- 主程序最终 OK/NG 与实际产品预期一致；
- 多框文本、位置变化和模板允许范围内的旋转均按官方排序规则输出；多框分行位置变化不影响最终文本判定；
- 无框、空识别和文本不匹配返回普通 NG；运行异常进入 Runtime Fault；
- 除 `\r`、`\n` 不参与比较外，ASCII 大小写和 `- . :` 等其他字符均按清洗后的内容严格参与比较。

### 6.2 稳定性和回归范围

- 主程序在整体验收期间连续运行稳定，结果数量、统计和图像生命周期一致；
- 其余四种检测模式的创建和执行路径不变；
- 模板制作、选择、保存、加载和 Schema 不变；
- UI 布局、结果呈现、完整单帧耗时和日志格式不变；
- predictor 创建和首次 DET/REC `Predictor::Run()` 在采集开始前完成，不计入首帧完整处理耗时；
- 相机、PLC、软硬触发、统计、存图和 Fault 收口不变；
- Release 输出只使用 `<exe>/config_ocr.txt`，在不依赖开发机源码目录的情况下可启动并完成 OCR。
- 源码、qmake、配置、当前有效文档、依赖说明、部署脚本、构建活动产物和发布目录均通过 Phase 3 的最终零残留检查。

## 7. 固定参考

- PaddleOCR v3.7.0：`https://github.com/PaddlePaddle/PaddleOCR/releases/tag/v3.7.0`
- Windows C++ 推理：`https://github.com/PaddlePaddle/PaddleOCR/blob/v3.7.0/docs/version3.x/inference_deployment/local_inference/cpp/OCR_windows.md`
- tiny DET 配置：`https://github.com/PaddlePaddle/PaddleOCR/blob/v3.7.0/configs/det/PP-OCRv6/PP-OCRv6_tiny_det.yml`
- tiny REC 配置：`https://github.com/PaddlePaddle/PaddleOCR/blob/v3.7.0/configs/rec/PP-OCRv6/PP-OCRv6_tiny_rec.yml`

上述 v3.7.0 tag 和已锁定模型资产是本方案的实现依据；识别字典以 tiny REC 配置中的 `character_dict_path` 为准。
