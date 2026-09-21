# OCR 公共引擎与字符辅助功能分层隔离实施方案

- 状态：代码实施完成，轻量静态检查通过，待用户通过 Qt Creator 构建并统一验证。
- 权威范围：`app/engines/ocr` 的公共 OCR 能力、正式深度 OCR 子引擎、字符模板编辑子引擎，以及两条业务调用链的隔离边界。
- 最终目标：正式深度 OCR 与字符模板编辑分别使用独立的具体引擎和固定用途识别入口；两边共享 OCR 实现、模型、字典和预处理规则，但各自持有独立的 Detector、Predictor、调用结果和生命周期状态。
- 保持项：正式深度 OCR 的模型、参数、文字行裁图、旋转、REC 预处理、Predictor 调用、基础 CTC 解码、结果顺序和业务判定保持不变；字符模板编辑已经确认的自动分割、自动命名、字符边界、过滤、字符框、命名和保存行为保持不变。
- 实施门禁：允许为拆开混合调用链而调整 `CRNNRecognizer` 的接口和函数组织；不得改变任何一条路径的模型、阈值、公式、图像处理参数、解码语义、输出顺序或用户行为。若实施中确实需要改变这些算法内容，先向用户说明后再处理。

## 1. 最终结构

```text
app/engines/ocr
    IOcrEngine
    ├─ 读取 OCRConfig，持有 DBDetector 和 CRNNRecognizer
    ├─ 实现 recognize()
    ├─ 实现 recognizeCharacter()
    └─ 实现 segmentCharacters()

app/detection/detectionmode/ocr
    DeepOcrEngine final
    ├─ 继承 IOcrEngine
    └─ 只公开 recognize()

app/ui/main_window/template/character_editor
    CharacterOcrEngine final
    ├─ 继承 IOcrEngine
    ├─ 只公开 recognizeCharacter()
    └─ 只公开 segmentCharacters()
```

`IOcrEngine` 是公共实现基类，不是业务接口。三个 OCR 方法由它集中实现，并由两个具体子引擎选择性公开。

正式深度 OCR 和字符模板编辑分别创建自己的引擎实例。每个实例分别持有一个 Detector 和一个 Recognizer，不共享 Predictor、Detector、调用结果、缓存或生命周期状态。

## 2. 公共引擎合同

### 2.1 `IOcrEngine`

`app/engines/ocr/ocr_engine.h/.cpp` 定义公共模型生命周期和三项原始 OCR 能力：

```cpp
class IOcrEngine
{
protected:
    explicit IOcrEngine(const QString &configPath);
    ~IOcrEngine();

    std::vector<OcrRecognitionItem>
    recognize(cv::Mat &image);

    std::string
    recognizeCharacter(cv::Mat &image);

    std::vector<OcrRecognitionItem>
    segmentCharacters(cv::Mat &image);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
```

约束：

- 构造函数、析构函数和三个 OCR 方法均为 `protected`。
- 三个 OCR 方法均为非虚函数。
- `Impl` 只持有一个 `DBDetector` 和一个 `CRNNRecognizer`。
- 构造阶段加载配置、模型和字典，并通过公共底层预热流程完成 DET、REC 预热。
- 每个具体引擎实例在 DET、REC 预热成功后，通过 `logDevice` 记录一次 `event=ocr.engine_loaded model=PP-OCRv6_tiny_det+rec config=%1`，其中 `%1` 使用该实例的 `configPath`；预热失败时不记录成功日志。
- `IOcrEngine` 不读取模板，不执行正式产品判定，不操作字符画布，不显示消息框。
- 业务调用方不直接创建、持有或调用 `IOcrEngine`。

### 2.2 正式深度 OCR 子引擎

`DeepOcrEngine` 是可直接创建的具体类：

```cpp
class DeepOcrEngine final : public IOcrEngine
{
public:
    explicit DeepOcrEngine(const QString &configPath)
        : IOcrEngine(configPath)
    {
    }

    using IOcrEngine::recognize;
};
```

`using` 只调整 `recognize()` 的访问级别。正式调用方看不到 `recognizeCharacter()` 和 `segmentCharacters()`。

### 2.3 字符模板编辑子引擎

`CharacterOcrEngine` 是可直接创建的具体类：

```cpp
class CharacterOcrEngine final : public IOcrEngine
{
public:
    explicit CharacterOcrEngine(const QString &configPath)
        : IOcrEngine(configPath)
    {
    }

    using IOcrEngine::recognizeCharacter;
    using IOcrEngine::segmentCharacters;
};
```

字符模板编辑调用方看不到 `recognize()`。

### 2.4 类型范围

最终对项目公开的 OCR 引擎类型只有：

- `IOcrEngine` 公共实现基类；
- `DeepOcrEngine final` 正式深度 OCR 具体子引擎；
- `CharacterOcrEngine final` 字符模板编辑具体子引擎。

Paddle 实现只存在于 `IOcrEngine::Impl` 和 `app/engines/ocr/vendor/paddle` 内部。

## 3. 两条独立调用链

### 3.1 正式深度 OCR

```text
DetectionRegistry::create(Ocr)
    → 创建 DeepOcrEngine
    → 公共构造过程加载并预热 DET/REC
    → executor 独占持有 DeepOcrEngine
    → OcrDetectionPipeline::detect(..., DeepOcrEngine &)
    → DeepOcrEngine::recognize()
    → DBDetector::Run()
    → Utility::SortQuadBoxes()
    → CRNNRecognizer::Run(boxes, image)
    → 原始 OcrRecognitionItem
    → OcrDetectionPipeline 完成清洗、比较、判定和 Overlay
```

`CRNNRecognizer::Run()` 是正式识别专用入口，直接执行文字行裁图、旋转、REC 预处理、Predictor 和基础 CTC 文本解码。它不调用字符分割入口，也不构造字符位置结果。

`OcrDetectionPipeline` 继续负责：

- 接收 `DetectionWorkItem`；
- 准备和校正正式日期 ROI；
- 调用 `DeepOcrEngine::recognize()`；
- 清除非数字、非英文字母；
- 按文字框顺序组合结果；
- 忽略双方换行后执行大小写敏感目标比较；
- 生成 `AlgorithmVerdict::Ok/Ng`；
- 将文字框映射回原图；
- 生成正式 Overlay、识别文本和诊断。

### 3.2 字符模板编辑

自动分割：

```text
CharacterTemplateEditorDialog
    → CharacterOcrEngine::segmentCharacters()
    → DBDetector::Run()
    → Utility::SortQuadBoxes()
    → CRNNRecognizer::RunCharacters(boxes, image)
    → CTC 字符位置和前景边界
    → IOcrEngine 映射为原图字符四点框
    → Dialog 过滤、替换确认并写入统一画布
```

自动命名：

```text
CharacterTemplateEditorDialog
    → 取得单字符 ROI 视图
    → CharacterOcrEngine::recognizeCharacter()
    → CRNNRecognizer::RunCharacter(image)
    → Dialog 过滤并写入普通名称输入框
```

字符模板编辑窗口不调用 `recognize()`，不执行正式目标比较、OK/NG、Overlay、统计、PLC 或存图逻辑。

### 3.3 实例和职责边界

| 项目 | 正式深度 OCR | 字符模板编辑 |
|---|---|---|
| 具体类型 | `DeepOcrEngine` | `CharacterOcrEngine` |
| 对外方法 | `recognize()` | `segmentCharacters()`、`recognizeCharacter()` |
| 创建位置 | `DetectionRegistry` | `CharacterTemplateEditorDialog` |
| 生命周期 | 正式运行 executor | 字符模板编辑窗口 |
| Detector / Predictor | 独立实例 | 独立实例 |
| 业务处理位置 | `app/detection` | `character_editor` |

两边都不通过 `IOcrEngine*` 或 `IOcrEngine&` 调用业务方法，也不持有另一边的子引擎。

## 4. Recognizer 固定用途入口

`CRNNRecognizer` 最终提供三个固定用途入口：

```cpp
std::vector<std::string> Run(
        const std::vector<std::vector<std::vector<int>>> &boxes,
        const cv::Mat &image);

std::string RunCharacter(const cv::Mat &image);

std::vector<RecLineResult> RunCharacters(
        const std::vector<std::vector<std::vector<int>>> &boxes,
        const cv::Mat &image);
```

边界如下：

- `Run()` 服务正式深度 OCR 的逐帧识别，并由 `IOcrEngine` 构造过程使用固定图像和固定四点框完成 REC 预热。
- `RunCharacter()` 只服务单字符自动命名。
- `RunCharacters()` 只服务自动分割。
- 三个入口不互相调用，不使用模式参数切换行为。
- `RecLineResult`、`RecCharacterResult`、字符中心和字符边界只存在于 `RunCharacters()` 路径。
- 三个入口只共享 `predictor_`、`labels_`、`resize_`、`permute_` 和 `Utility` 等底层状态与算子，不共享高层控制流。

### 4.1 正式 `Run()`

每个文字行固定执行：

1. 按现有四点框做透视裁图；
2. 窄高文字行按现有 `高度 / 宽度 >= 1.5` 规则逆时针旋转 90 度；
3. 调用现有 `RecResizeImg::Run()`；
4. 准备相同尺寸和布局的 Predictor 输入；
5. 执行一次 REC Predictor；
6. 使用原有 `previousIndex` 规则完成基础 CTC 解码；
7. 返回原始文字行字符串。

该路径不计算有效时间步、字符中心、字符矩形、Otsu 二值图、列投影或行投影，不创建 `RecLineResult` 和 `RecCharacterResult`。

### 4.2 单字符 `RunCharacter()`

固定执行：

1. 直接使用传入的单字符图像，不执行 DET；
2. 以图像四角构造覆盖整张图像的四点框；
3. 保持现有四点透视裁图；
4. 不执行窄高文字行 90 度旋转；
5. 使用同一 REC 模型、预处理和基础 CTC 解码语义；
6. 返回原始字符串。

### 4.3 自动分割 `RunCharacters()`

固定保留现有字符分割计算：

1. 对排序后的每个文字行做相同透视裁图和窄高旋转；
2. 使用相同 REC 模型和预处理执行 Predictor；
3. 按现有 CTC 时间步生成字符文本和字符中心；
4. 有效内容宽度继续按文字行宽高比、目标高度和目标宽度计算；
5. 使用现有 Otsu 极性、列投影、相邻中心之间最小前景列、3% 且至少 1 像素背景和上下行投影计算字符边界；
6. 保持重复字符、首尾边界、内部边界、取整和 `rotated90` 坐标映射；
7. 返回与输入 `boxes` 等量、同序的 `RecLineResult`；每个输入文字行固定对应一个结果，即使该行没有识别出字符也保留对应的空结果，不过滤或压缩空结果。

`RecResizeImg::Run()` 保持当前缩放、归一化和右侧补边处理，并返回函数中已经计算出的 `contentWidth`。正式 `Run()` 和 `RunCharacter()` 忽略该返回值；只有 `RunCharacters()` 使用它计算有效时间步，不在 Recognizer 中重复计算 Resize 公式。

## 5. 三项公共 OCR 方法

| 方法 | 固定处理 | 输出 |
|---|---|---|
| `IOcrEngine::recognize()` | DET、排序、`CRNNRecognizer::Run()` | 逐文字框原始文本和四点框 |
| `IOcrEngine::recognizeCharacter()` | `CRNNRecognizer::RunCharacter()` | 单字符原始文本 |
| `IOcrEngine::segmentCharacters()` | DET、排序、`CRNNRecognizer::RunCharacters()`、字符框坐标映射 | 逐字符原始文本和四点框 |

三个方法都不执行：

- `TemplateStore::templateTargetUnits()` 过滤；
- 目标文字比较；
- OK/NG 判定；
- 模板字段读写；
- 自动框替换确认；
- 画布操作；
- 用户提示。

### 5.1 正式识别保持项

- Detector 参数、文字框排序和四点框内容不变。
- 每个文字框的裁图、旋转、Resize、Predictor 和基础 CTC 解码顺序不变。
- 正式识别不进入字符位置解码和前景边界计算。
- 构造阶段仍创建一个 Detector 和一个 Recognizer；构造函数继续使用固定 `48 × 320` 三通道图直接调用一次 `DBDetector::Run()`，再使用覆盖该图的固定四点框直接调用一次 `CRNNRecognizer::Run()`。预热结果丢弃，不增加预热接口、配置、成员状态或辅助封装。
- 正式逐帧路径不增加字符功能带来的条件分支、结果对象、图像处理或 Predictor 调用。
- `OcrDetectionPipeline` 的 ROI、清洗、组合、比较、判定、诊断和 Overlay 逻辑不变。

### 5.2 字符辅助保持项

- 单字符自动命名继续只执行 REC，不执行 DET 和窄高旋转。
- 自动分割继续执行 DET、文字行排序、字符位置 CTC、前景边界和四点框映射。
- 字符分割的模型、阈值、公式、顺序和取整不变。
- 字符功能只使用字符窗口自己的引擎实例，不向正式引擎写入状态。

### 5.3 必要的底层代码调整

`app/engines/ocr/vendor/paddle/include/ocr_rec.h` 和 `src/ocr_rec.cpp` 的最终接口为 `Run()`、`RunCharacter()` 和 `RunCharacters()` 三条固定路径。

这三条路径保持各自现有的识别和字符边界算法。`ocr_det.*`、`utility.*` 的算法保持不变；`preprocess_op.*` 保持当前 Resize、归一化和补边处理，并把已计算的 `contentWidth` 返回给自动分割路径。

## 6. 字符模板编辑行为

### 6.1 引擎生命周期

- Dialog 构造时使用 `config_ocr.txt` 创建一份窗口独立的 `CharacterOcrEngine`。
- 创建失败时只记录一次模板日志，窗口继续打开，手动画框、命名和保存流程保持可用。
- 创建失败不弹窗、不关闭窗口、不禁用按钮；用户点击“自动分割”时提示功能不可用，进入命名页时静默跳过自动命名。
- 引擎随 Dialog 析构，不进入 Detection Worker，也不与正式深度 OCR 实例共享 Detector 或 Predictor。

### 6.2 统一文字过滤

自动分割和单字符自动命名使用同一个 Dialog 层过滤函数：

```cpp
const QString value = text.trimmed();
const QStringList units =
        TemplateStore::templateTargetUnits(value);
if (value.size() != 1 || units.size() != 1) {
    return QString();
}
return value;
```

`-`、`/`、`:` 等字符先参与相邻字符边界计算，再由 Dialog 过滤。

### 6.3 自动分割

- 使用字符区域图像调用 `CharacterOcrEngine::segmentCharacters()`。
- 自动框宽度和高度都必须至少为 4 像素。
- Dialog 在替换确认前执行一次 4 × 4 过滤。
- Dialog 不重复裁剪图像边界；`CharacterCropLabel::setItems()` 统一完成矩形标准化、图像边界裁剪和 4 × 4 过滤。
- 画布没有字符框时直接写入自动分割结果。
- 画布已有字符框时提示一次性替换，确定后替换，取消时保持原框。
- 没有有效自动结果或识别异常时保持现有字符框。
- 所有新自动框的 `name` 为空。

### 6.4 自动命名

- 只识别名称为空的字符框，已有名称保持不变。
- 单字符裁图直接使用源图中的 `cv::Mat` ROI 视图。
- 每个字符调用 `CharacterOcrEngine::recognizeCharacter()`。
- 每个字符分别捕获识别异常；失败字符保持空名称，后续字符继续识别。
- 识别结果只作为普通命名输入框的初始值，用户可以修改。
- 保存继续读取现有 `QLineEdit` 中的最终文本。

### 6.5 统一字符框状态

- `TemplateCharacterBox` 只保存 `rect` 和 `name`。
- Dialog 构造时先设置字符区域图像，再通过 `setItems(m_resultSettings.characterBoxes)` 载入已保存字符框。
- `CharacterCropLabel::m_items` 是绘图页唯一字符框列表。
- 鼠标拖动期间的 `m_currentRect` 是橙色虚线预览；松开鼠标并通过 4 × 4 规则后，才追加为蓝色实线框。
- 自动框进入画布后与手动画框完全一致，不保存来源，不增加来源分支。
- 手动画框和自动框使用相同的绘制、编号、删除、清空、预览、排序、命名和保存逻辑。
- “删除上一个字符框”删除统一列表最后一项。
- 纯自动结果按 OCR 阅读顺序保存，因此删除其最后一个结果；自动分割后追加手动画框时，先删除最后追加的手动画框。
- “清空所有字符框”清空统一列表。

## 7. 目录职责

### 7.1 `app/engines/ocr`

只包含两边共享的原始 OCR 能力：

- `IOcrEngine`、`OcrRecognitionItem` 和公共模型生命周期；
- OCR 配置、模型和字典加载；
- Detector、Recognizer、Predictor 和预处理；
- 正式基础 CTC 文本解码；
- 字符辅助使用的单字符 REC、字符位置 CTC、字符前景边界和坐标映射；
- Paddle Inference 和 OpenCV 透视变换。

不得出现：

- `DetectionMode::Ocr`、`DetectionWorkItem` 或 `DetectionResult`；
- `TemplateStore`、`TemplateCharacterBox`、`CharacterCropLabel` 或 `QMessageBox`；
- 目标文字比较、OK/NG、Overlay、Runtime Fault、Worker、PLC、统计或存图逻辑。

### 7.2 `app/detection/detectionmode/ocr`

只包含：

- `DeepOcrEngine`；
- 正式 ROI、文字清洗和组合；
- 目标比较、OK/NG、诊断和 Overlay 映射。

不得出现字符模板编辑、自动框、字符位置结果、字符命名或字符保存逻辑。

### 7.3 `app/ui/main_window/template/character_editor`

只包含：

- `CharacterOcrEngine`；
- QImage 到 BGR `cv::Mat` 转换；
- 自动分割、自动命名和统一字符过滤；
- 替换确认、字符框、命名和保存交互。

不得出现正式目标文字比较、正式 OK/NG、`DetectionResult`、Runtime、Worker、PLC、统计或存图逻辑。

## 8. 实施阶段

### 阶段 1：拆开 Recognizer 路径

1. 将 `CRNNRecognizer::Run()` 收口为正式识别专用直接路径。
2. 新增 `RunCharacter()` 和 `RunCharacters()` 两个字符辅助专用入口。
3. `CRNNRecognizer` 对外只保留三个固定用途入口，不保留混合入口或模式参数。
4. 保持三条路径各自现有算法、公式、参数、输出和异常传播。
5. 保持 `RecResizeImg::Run()` 返回已有 `contentWidth`；正式识别忽略该值，自动分割直接使用该值。
6. 将 DET/REC 预热原样迁入 `IOcrEngine` 构造函数，直接调用 Detector 和正式 `Run()`，不新增预热接口或辅助层。

### 阶段 2：建立公共实现基类

1. 在 `ocr_engine.h` 中把三个 OCR 方法定义为 `protected` 非虚函数。
2. 新增 `ocr_engine.cpp`，集中实现配置加载、模型创建、预热和三个 OCR 方法。
3. `recognize()` 只调用正式 `Run()`。
4. `recognizeCharacter()` 只调用 `RunCharacter()`。
5. `segmentCharacters()` 只调用 `RunCharacters()` 并完成字符框坐标映射。

### 阶段 3：建立两个具体子引擎

1. 在正式 OCR 目录新增 `DeepOcrEngine final`，只公开 `recognize()`。
2. 在字符编辑目录新增 `CharacterOcrEngine final`，只公开 `recognizeCharacter()` 和 `segmentCharacters()`。
3. 两个子引擎只包含构造函数和 `using` 声明，不增加第二份业务实现或转发函数。

### 阶段 4：收口正式调用方

1. `DetectionRegistry` 创建并持有 `std::shared_ptr<DeepOcrEngine>`。
2. `OcrDetectionPipeline` 接收 `DeepOcrEngine &` 并调用 `recognize()`。
3. 保持正式 ROI、清洗、组合、比较、判定、诊断和 Overlay 逻辑不变。
4. 确认正式逐帧路径没有字符位置结果、字符边界计算或字符辅助分支。

### 阶段 5：收口字符调用方

1. `CharacterTemplateEditorDialog` 持有 `std::unique_ptr<CharacterOcrEngine>`。
2. 自动分割只调用 `segmentCharacters()`。
3. 自动命名只调用 `recognizeCharacter()`。
4. 保持统一过滤、4 × 4、替换确认、逐字符异常处理和统一字符框行为。

### 阶段 6：删除旧结构并同步说明

1. 删除 `app/engines/ocr/vendor/paddle_ocr_engine.h/.cpp`，同时删除源码、工程文件和有效说明文档中的全部 `PaddleOcrEngine` 引用，不保留同名空壳、别名或转发类型。
2. 直接用最终公共实现基类替换 `ocr_engine.h` 中原有的公开纯虚接口，不并存旧虚方法、`override` 实现或第二套 OCR 接口。
3. 从 Recognizer 的声明、实现和调用处删除 `RunDetailed()`、带 `allowRotate90` 参数的 `Run()`、`allowRotate90` 和 `refineCharacterBounds` 行为开关，只保留 `Run()`、`RunCharacter()` 和 `RunCharacters()` 三个固定用途入口。
4. 不保留废弃重载、兼容头文件、兼容别名、适配器、工厂、转发函数、胶水层、重复结果结构、未使用成员或仅为旧调用链服务的辅助代码。
5. 更新 `app/AutoOCRproject.pro`，只登记最终实际存在的源文件和头文件。
6. 更新 OCR Engine README、开发者结构说明、功能对照表和计划索引，使有效说明只描述最终结构。
7. 执行轻量静态检查，不执行构建、链接或主程序运行。

## 9. 修改文件范围

### 9.1 公共引擎与底层 Recognizer

| 文件 | 最终状态 |
|---|---|
| `app/engines/ocr/ocr_engine.h` | 声明公共实现基类、三个 `protected` 非虚方法和 `Impl` |
| `app/engines/ocr/ocr_engine.cpp` | 实现公共模型生命周期和三个固定用途 OCR 方法 |
| `app/engines/ocr/vendor/paddle_ocr_engine.h/.cpp` | 删除 |
| `app/engines/ocr/vendor/paddle/include/ocr_rec.h` | 声明 `Run()`、`RunCharacter()` 和 `RunCharacters()` |
| `app/engines/ocr/vendor/paddle/src/ocr_rec.cpp` | 实现三条互不转调的固定用途路径 |
| `app/engines/ocr/vendor/paddle/include/preprocess_op.h` | `RecResizeImg::Run()` 返回已有 `contentWidth` |
| `app/engines/ocr/vendor/paddle/src/preprocess_op.cpp` | 保持现有 Resize、归一化和补边处理，只返回已计算的 `contentWidth` |

### 9.2 正式深度 OCR

| 文件 | 最终状态 |
|---|---|
| `app/detection/detectionmode/ocr/deep_ocr_engine.h` | 新增具体子引擎，只公开 `recognize()` |
| `app/detection/detectionmode/ocr/ocr_detection_pipeline.h/.cpp` | 接收 `DeepOcrEngine &`，正式业务逻辑保持不变 |
| `app/detection/detection_registry.cpp` | 创建并持有 `DeepOcrEngine` |

### 9.3 字符模板编辑

| 文件 | 最终状态 |
|---|---|
| `app/ui/main_window/template/character_editor/character_ocr_engine.h` | 新增具体子引擎，只公开两个字符方法 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.h/.cpp` | 持有并调用 `CharacterOcrEngine` |

以下文件已形成的字符框和界面行为保持：

```text
app/ui/main_window/template/character_editor/character_crop_label.h/.cpp
app/ui/main_window/template/character_editor/character_template_editor_dialog.ui
app/resource/Translate_CN.ts
app/resource/Translate_EN.ts
```

### 9.4 工程与说明

实施时同步：

- `app/AutoOCRproject.pro`；
- `app/engines/README.md`；
- `docs/development/OCRGangYin开发者代码结构与维护指南.md`；
- `docs/development/OCRGangYin现有功能对照表.md`；
- `docs/development/OCRGangYin计划索引.md`。

## 10. 静态门禁

### 10.1 类型与实例

- `IOcrEngine` 的三个 OCR 方法均为 `protected` 非虚函数。
- `DeepOcrEngine` 和 `CharacterOcrEngine` 均为 `final` 具体类。
- `DeepOcrEngine` 只公开 `recognize()`。
- `CharacterOcrEngine` 只公开 `recognizeCharacter()` 和 `segmentCharacters()`。
- 调用方不持有 `IOcrEngine*`、`IOcrEngine&` 或 `std::shared_ptr<IOcrEngine>`。
- 不存在公开的 `PaddleOcrEngine`、额外 Paddle 子引擎、工厂或适配层。
- `app/engines/ocr/vendor/paddle_ocr_engine.h/.cpp` 不再存在，工程文件、源码和有效说明中均无对应路径、类型或头文件引用。
- `IOcrEngine` 不保留公开纯虚 OCR 方法，两个具体子引擎不实现转发或 `override` 方法。
- 两个具体引擎不共享 Detector、Predictor 或结果状态。

### 10.2 Recognizer 路径

- `CRNNRecognizer` 只有 `Run()`、`RunCharacter()` 和 `RunCharacters()` 三个业务入口。
- 源码中不存在 `RunDetailed`、`allowRotate90`、`refineCharacterBounds` 或旧 `Run()` 重载。
- 三个入口都不使用模式参数切换行为。
- 三个入口互不调用。
- 正式 `Run()` 不引用 `RecLineResult`、`RecCharacterResult`、字符中心、有效时间步、Otsu 或投影边界。
- `RunCharacter()` 不调用 Detector，保留整图四点透视裁图，并且不执行窄高旋转。
- `RunCharacters()` 独占字符位置和前景边界计算。
- `RecResizeImg::Run()` 返回已有 `contentWidth`；正式 `Run()` 忽略返回值，`RunCharacters()` 直接使用返回值。
- 预热直接位于 `IOcrEngine` 构造过程，只调用现有 Detector 和正式 `Run()`，不新增预热接口、配置或成员状态。
- 每个引擎实例只在 DET、REC 预热成功后记录一次 `event=ocr.engine_loaded model=PP-OCRv6_tiny_det+rec config=...` 成功日志，事件名、模型文本和配置路径内容保持不变。

### 10.3 调用与业务边界

- `DetectionRegistry` 只创建 `DeepOcrEngine`。
- `OcrDetectionPipeline` 只调用 `DeepOcrEngine::recognize()`。
- `CharacterTemplateEditorDialog` 只创建 `CharacterOcrEngine`。
- 字符 Dialog 只调用 `segmentCharacters()` 和 `recognizeCharacter()`。
- `app/detection` 不引用字符详细结果、字符编辑控件或字符模板保存逻辑。
- `character_editor` 不引用正式 Detection 结果、目标比较、OK/NG 或 Overlay。

### 10.4 行为与工程检查

- 正式识别的模型、参数、处理顺序、基础 CTC 文本、文字框顺序和业务结果保持一致。
- 自动分割的字符顺序、字符边界和坐标映射保持一致。
- 自动命名的 REC 结果、过滤和逐字符异常行为保持一致。
- `app/AutoOCRproject.pro` 包含所有新增文件，不包含已删除文件，路径无重复。
- 不存在已删除类型、方法或头文件引用。
- UTF-8、UI XML 和翻译资源格式保持有效。
- `git diff --check` 通过。

## 11. 用户构建与验收

完整构建和程序验证由用户使用当前 Qt Creator Kit 执行。

### 11.1 正式深度 OCR

- 启动、停止和再次启动正常；
- 初始化失败仍在创建 Worker 前返回；
- 识别文本、顺序和四点框保持一致；
- 文字清洗、目标比较、OK/NG、诊断和 Overlay 保持一致；
- 执行期 DET/REC 异常仍进入现有 Runtime Fault；
- 正式每帧不执行字符位置、字符边界或字符框映射代码；
- 连续处理耗时没有因字符功能增加固定开销。

### 11.2 自动分割

验证：

```text
2026-09-08
2026/09/08
12:30
00
11
水平文字
轻微倾斜文字
触发文字行旋转规则的窄高文字行
```

要求：

- 标点参与边界计算后再过滤；
- `2026-09-08` 最终生成 8 个数字框；
- 重复字符分别生成独立框；
- 每个字符框尽量贴近自身前景；
- 所有有效框至少为 4 × 4；
- 空画布直接写入，有框时先确认替换；
- 取消、异常和空结果保持现有框；
- 自动框可以按统一规则删除上一个、清空、编号、预览、排序、命名和保存；
- 自动框与手动画框没有状态或行为差异。

### 11.3 自动命名

- 仅识别空名称字符；
- 单字符不执行 DET；
- 窄高字符不旋转；
- 自动分割和自动命名使用相同字符过滤函数；
- 单字符异常不影响后续字符；
- 结果可在普通输入框中修改；
- 返回框选后不保留本轮未保存名称；
- 保存继续使用现有文件名、重复名称和字符图片规则。

## 12. 完成条件

同时满足以下条件后，本方案完成：

1. `IOcrEngine` 集中持有公共 OCR 状态并实现三项固定用途能力。
2. 正式深度 OCR 只通过具体 `DeepOcrEngine` 和正式 `CRNNRecognizer::Run()` 执行。
3. 字符模板编辑业务只调用具体 `CharacterOcrEngine` 公开的 `recognizeCharacter()` 和 `segmentCharacters()`，分别进入 `RunCharacter()` 和 `RunCharacters()`。
4. 正式路径不包含字符位置、字符边界、字符结果结构或字符辅助行为分支。
5. 字符路径不包含正式目标比较、OK/NG、诊断、Overlay 或运行控制。
6. 两条路径的模型、阈值、公式、解码语义、输出顺序和用户行为保持不变。
7. 最终代码没有混合入口、模式开关、额外抽象层、转发层、兼容层、胶水层、冗余状态或失效引用。
8. 静态门禁通过，并由用户完成 Qt Creator 构建和统一交互验证。
