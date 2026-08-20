# detection：五种检测模式及其共享算法

## 一句话理解

`detection` 是正式图像算法的唯一所有者：输入一帧图像和冻结的配方/设置，输出统一 `DetectionResult`。它不控制相机、不写 PLC、不统计、不存图，也不更新 UI。

## 为什么有 7 个子目录，却只有 5 种检测模式

这是本目录最重要的结构结论：

```text
detection/
├─ 5 个“模式目录”
│  ├─ stamp/          钢印检测
│  ├─ word/           字库匹配
│  ├─ ocr/            深度学习 OCR
│  ├─ tissue/         纸巾检测
│  └─ barcode_word/   二维码 + 三期
│
├─ 2 个“共享能力目录”，不是检测模式
│  ├─ common/         五模式可复用的预处理、几何、字符匹配、Profile 选优
│  └─ positioning/    定位、Pose 和统一结果数据合同
│
└─ 根部 4 个文件
   ├─ detection_registry.h/.cpp          五种 Pipeline 的唯一装配入口
   └─ detection_profile_snapshot.h/.cpp 多 Profile 运行快照
```

因此：7 个文件夹不等于 7 种检测模式；根部 4 个文件也不是 4 种模式。

## 当前文件树

```text
detection/
├─ detection_registry.h
├─ detection_registry.cpp
├─ detection_profile_snapshot.h
├─ detection_profile_snapshot.cpp
├─ common/
│  ├─ frame_preprocessor.h
│  ├─ frame_preprocessor.cpp
│  ├─ detection_roi_geometry.h
│  ├─ character_template_matcher.h
│  ├─ character_template_matcher.cpp
│  ├─ profile_pose_selector.h
│  └─ profile_pose_selector.cpp
├─ positioning/
│  ├─ detection_pose.h
│  ├─ tracking_pose_matcher.h
│  ├─ tracking_pose_matcher.cpp
│  ├─ inspection_positioner.h
│  └─ inspection_positioner.cpp
├─ stamp/
│  ├─ stamp_detection_pipeline.h
│  ├─ stamp_detection_pipeline.cpp
│  ├─ overlap_detector.h
│  └─ overlap_detector.cpp
├─ word/
│  ├─ word_detection_pipeline.h
│  └─ word_detection_pipeline.cpp
├─ ocr/
│  ├─ ocr_detection_pipeline.h
│  └─ ocr_detection_pipeline.cpp
├─ tissue/
│  ├─ tissue_detection_pipeline.h
│  ├─ tissue_detection_pipeline.cpp
│  ├─ tissue_roll_detector.h
│  └─ tissue_roll_detector.cpp
├─ barcode_word/
│  ├─ barcode_word_detection_pipeline.h
│  └─ barcode_word_detection_pipeline.cpp
└─ README.md
```

## 正式检测调用链

```text
InspectionRuntime
→ DetectionWorker（串行消费正式帧）
→ DetectionRegistry 创建的通用 executor
→ FramePreprocessor
→ InspectionPositioner
→ 对应的 1 个 Pipeline
→ DetectionResult
→ DetectionCompletion
→ ResultService（统计、PLC、存图、UI 呈现）
```

Detection 的工作在生成 `DetectionResult` 时结束。后面的统计、PLC、存图和 UI 都属于 Runtime。

## 根部文件

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `detection_registry.h` | 声明运行准备结果、Registry 请求、通用执行器和创建结果。 | Runtime 只能看到这一个通用入口，不能 include 五种 Pipeline。 |
| `detection_registry.cpp` | 冻结 Descriptor 策略，创建 Positioner，做正式预处理，按模式装配一个 Pipeline，并统一转换 Completion。 | 五模式装配分支集中在这里，不能复制到 UI/Runtime。 |
| `detection_profile_snapshot.h` | 定义同序的定位/检测 Profile 运行条目与 Builder。 | Profile 索引必须稳定对应；不能错配后退回第一个 Profile。 |
| `detection_profile_snapshot.cpp` | 从 `PreparedRecipe` 一次性构建字库/二维码多 Profile 快照并预编译字符模板。 | 启动前准备；运行中不重新读配方或加载模板。 |

## common：共享算法能力

| 文件 | 当前职责 | 维护要点 |
|---|---|---|
| `frame_preprocessor.h/.cpp` | 按整机设置完成 0/90/180/270 旋转及 B/G/R/灰度通道提取。 | 影响全部模式；BGR 语义、旋转方向变化必须五模式回归。 |
| `detection_roi_geometry.h` | header-only 纯几何：旋转 ROI、仿射正逆映射、裁边和 Overlay 回映。 | 不加入业务判定；边缘 ROI、空多边形和旋转角是高风险条件。 |
| `character_template_matcher.h/.cpp` | 预编译字符模板，执行 `matchTemplate`、阈值筛选、重叠抑制和排序。 | 算法行为高风险；阈值、缩放、NMS、排序不能当普通重构修改。 |
| `profile_pose_selector.h/.cpp` | 在多 Profile 定位候选中保留最佳 Pose 和原始 Profile 索引。 | 同分保持先出现 Profile 的现有语义。 |

## positioning：统一数据与定位

| 文件 | 当前职责 | 维护要点 |
|---|---|---|
| `detection_pose.h` | 定义 `ProductKey`、`FrameData`、`DetectionResult`、`DetectionCompletion`、Overlay、Pose 和 WorkItem 等核心合同。 | `FrameData` 必须独立持有图像；Result 不做副作用，也不长期持有原图。 |
| `tracking_pose_matcher.h/.cpp` | 单模板多角度/尺度定位，输出中心、角度、尺寸和得分。 | 角度、尺度、分数及坐标约定属于定位算法。 |
| `inspection_positioner.h/.cpp` | 统一处理 WholeFrame、SingleTemplate、MultipleProfiles 三种定位策略。 | 只定位，不执行字符、OCR、二维码或纸巾判定。 |

## 五个模式目录

### stamp：钢印检测

| 文件 | 当前职责 |
|---|---|
| `stamp_detection_pipeline.h/.cpp` | 构造旋转日期 ROI，执行字符模板匹配和钢印重叠检测，组合文字、Overlay、判定和耗时。 |
| `overlap_detector.h/.cpp` | 使用校准多边形、边缘/模板匹配和掩膜重叠计算钢印位置及重叠状态。 |

`overlap_detector.cpp` 是高风险纯算法文件，形态学、边缘、阈值或几何调整都需要钢印 OK/NG 样本验证。

### word：字库匹配

| 文件 | 当前职责 |
|---|---|
| `word_detection_pipeline.h/.cpp` | 构造日期 ROI，调用公共字符匹配器，按目标字符数量和顺序生成文字、Overlay 与 OK/NG。 |

多 Profile 定位已经在 Positioner 完成，本 Pipeline 不应重复定位。

### ocr：深度学习 OCR

| 文件 | 当前职责 |
|---|---|
| `ocr_detection_pipeline.h/.cpp` | 提取日期 ROI，通过 `IOcrEngine` 识别、清洗文字、与目标字符串比较并生成统一结果。 |

它只依赖 OCR 接口，不能 include Paddle 具体实现。

### tissue：纸巾检测

| 文件 | 当前职责 |
|---|---|
| `tissue_detection_pipeline.h/.cpp` | 调用纸卷检测器并把圆/椭圆、粗糙度和诊断转换成统一结果。 |
| `tissue_roll_detector.h/.cpp` | 灰度/阈值/形态学、轮廓与环形展开、Laplacian/Sobel 粗糙度分析。 |

纸巾模式使用 WholeFrame，不需要模板。`tissue_roll_detector.cpp` 是本项目最高风险算法文件之一。

### barcode_word：二维码 + 三期

| 文件 | 当前职责 |
|---|---|
| `barcode_word_detection_pipeline.h/.cpp` | 从 Pose 构造二维码和日期 ROI，先解二维码，再匹配日期字符，组合两部分状态、文字、Overlay 和判定。 |

必须保持“二维码优先短路”和“一次触发一次最终结果”；解码策略状态跟随 Profile，不能全局串用。

## 允许放什么

- 图像预处理、定位、ROI 几何和五种检测算法。
- 只读配方资源转换成算法输入。
- 纯算法结果、Overlay 和诊断信息。
- 与具体模式直接相关的 Pipeline。

## 禁止放什么

- `QWidget`、`QMessageBox`、按钮状态和 UI 字符串拼装。
- 相机打开/取流、PLC 写入、统计、存图和日志生命周期。
- 运行中读取 `recipe.json`、模板目录或全局设置。
- 海康、Snap7、Paddle、Barcode DLL 的具体类型。
- 为每种模式新建 Runtime Worker 或结果处理链。

## 新增第六种模式

1. 在 `contracts/detection_mode.*` 增加稳定模式和完整 Descriptor。
2. 在 `recipes` 增加确实需要的字段/资源，并由 `PreparedRecipe` 预加载。
3. 在 `detection/<new_mode>/` 新增一组 Pipeline，优先复用 `common` 和 `positioning`。
4. 只在 `DetectionRegistry::create` 增加一次装配。
5. 必要时在启动 Preflight 增加资源门禁。
6. Runtime、ResultService 和 CameraSession 不增加模式专用代码。

判断设计是否正确：新增模式以后，`runtime/` 不应出现该模式的 Pipeline include 或专用 Worker。

## 推荐阅读顺序

```text
positioning/detection_pose.h
→ detection_registry.h/.cpp
→ common/frame_preprocessor.*
→ positioning/inspection_positioner.*
→ 目标模式的 *_detection_pipeline.*
→ ../runtime/result_service.*
```

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单和开发者维护指南。
