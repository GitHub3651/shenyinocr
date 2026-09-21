# 字符分割页面引入深度 OCR 辅助功能实施方案

- 状态：代码已实施，静态门禁已通过，待用户使用 Qt Creator 构建并统一验证。
- 权威范围：字符模板编辑窗口中的 OCR 初始化、自动生成字符框、未命名字符辅助命名、相关 OCR 接口和 UI。
- 保持项：正式深度 OCR 检测、模板 Schema、手动画框方式、人工命名、返回框选、字符模板保存及其它检测模式保持不变。
- 关联约束：正式 OCR 检测继续服从 `PP-OCRv6_tiny_深度OCR重构实施方案.md` 和 `OCRGangYin深度OCR启动前同步初始化精简方案.md`；本方案规定字符模板编辑窗口持有的独立 OCR Engine 及新增接口。
- 实施门禁：第 10 节文件范围已经完成实现并通过静态检查；第 11 节构建和交互验收由用户统一执行。

## 1. 最终功能

字符模板编辑窗口增加两项 OCR 辅助能力：

1. 绘图页增加“自动分割”按钮。按钮使用现有 Paddle OCR 模型检测文字行，以 REC 的 CTC timestep 为字符位置锚点，并通过文字行前景投影修正字符级矩形框。
2. 点击“下一步命名”时，仅对 `name` 为空的字符框执行单字符 REC，将结果预填到命名输入框；已有名称直接保留。

两项能力只增加输入手段，不增加新的业务状态：

```text
鼠标手动画框 ──────────┐
                       ├→ CharacterCropLabel::m_items
OCR 自动生成框 ────────┘

人工填写名称 ──────────┐
                       ├→ 同一个 QLineEdit → box.name
OCR 自动识别名称 ──────┘
```

框或名称进入现有状态后只保留矩形和最终文本，后续操作、校验和保存规则完全一致。

最终行为固定为：

| 场景 | 行为 |
|---|---|
| 打开字符模板编辑窗口 | 创建并预热一份独立 OCR Engine |
| OCR Engine 创建失败 | 记录日志，窗口继续打开，手工流程可用 |
| 手动画框 | 保持现有绘制方式，宽度和高度均至少为 4 像素 |
| 点击“自动分割”且 OCR 可用 | 生成完整的临时字符框结果 |
| 点击“自动分割”且 OCR 不可用 | 提示自动分割不可用，现有框不变 |
| 自动分割推理失败 | 记录日志并提示失败，现有框不变 |
| 自动分割没有有效结果 | 提示继续手动框选，现有框不变 |
| 自动分割成功且画布没有字符框 | 直接绘制全部新框，所有新框的 `name` 为空 |
| 自动分割成功且画布已有字符框 | 提示新框将一次性替换现有全部字符框；确定后替换，取消则保持现有框不变 |
| 自动结果进入画布 | 与手动画出的框共同存放在 `CharacterCropLabel::m_items`，使用相同的绘制、删除上一个字符框、清空、编号、预览、排序、命名和保存流程 |
| 点击“下一步命名” | 先按现有规则排序，再识别空名称字符 |
| 字符已有名称 | 不执行 OCR |
| 单字符识别成功且通过字符过滤 | 将识别结果预填到普通命名输入框，用户可以继续修改 |
| 单字符无结果、未通过字符过滤或异常 | 保持空名称，不弹窗，继续处理其它字符 |
| 返回框选 | 放弃本轮未保存的 OCR 和人工命名 |
| 保存 | 使用现有校验、命名和字符图片保存逻辑 |
| Dialog 销毁 | 随成员自动销毁该窗口持有的 OCR Engine |

OCR 只替代逐个拖拽画框和首次填写空名称的过程，不改变现有画布、命名页或人工确认职责。自动生成和手动生成的框使用同一个 `TemplateCharacterBox`、同一个画布列表和同一套现有操作；OCR 预填和人工填写的名称使用同一个 `QLineEdit` 和同一套保存规则。

## 2. 现有状态与数据规则

### 2.1 字符区域图像

`TemplateEditorPage::showManualCharacterTemplateEditorDialog()` 已经根据 `datePolygon` 的外接矩形从模板原图裁出字符区域，并将该区域作为 `QImage` 传给 `CharacterTemplateEditorDialog`。

自动分割和单字符命名直接使用这张现有字符区域图，不增加新的区域数据、坐标链或图像预处理流程。

### 2.2 统一框状态与自动命名条件

`TemplateCharacterBox` 继续只保存矩形和名称。手动画框通过 `CharacterCropLabel::mouseReleaseEvent()` 追加到 `m_items`；自动分割在用户确认后通过 `CharacterCropLabel::setItems()` 写入同一个 `m_items`。之后可以继续手动画框，也可以删除上一个字符框、清空全部、查看编号和预览或进入下一步命名，所有操作只处理统一列表。

自动分割替换前过滤与 `CharacterCropLabel` 的手动画框、绘制预览和 `setItems()` 使用相同的最小尺寸标准：宽度小于 4 像素或高度小于 4 像素的框不进入统一列表，`4 × 4` 像素及以上的框保留。`setItems()` 负责传入列表的矩形标准化和图像边界裁剪；手动画框使用画布已经限制在图像内的坐标。`m_items` 只保存有效框，`items()` 直接返回该列表。

“删除上一个字符框”每次执行一次 `m_items.removeLast()`。纯手动画框时删除最后画出的框；纯自动分割时删除 OCR 阅读顺序中的最后一个框；自动分割后继续手动画框时先删除最后追加的手动画框。

`TemplateCharacterBox::name` 是唯一判断条件：

```cpp
if (!box.name.trimmed().isEmpty()) {
    continue;
}
```

- 构造函数通过 `setItems(m_resultSettings.characterBoxes)` 载入已保存框并保留名称，因此已有名称不重新识别。
- `CharacterCropLabel::mouseReleaseEvent()` 创建的新框名称为空，因此进入命名页时执行 OCR。
- 自动分割生成的框也以空名称进入同一个列表，因此进入命名页时执行相同 OCR。

字符框状态仅使用现有 `TemplateCharacterBox::rect` 和 `TemplateCharacterBox::name`。

自动分割和单字符自动命名共同使用 `TemplateStore::templateTargetUnits()` 过滤 OCR 文本。只有完整文本恰好对应一个单字符模板目标单元时才保留；标点、空白、多字符结果和 `1(1)` 形式的重复模板引用均不作为 OCR 自动命名结果。

### 2.3 返回框选

现有“返回框选”连接保持原样：

```cpp
connect(ui->pushButton_backToCharacterDrawing,
        &QPushButton::clicked,
        this, [this]() {
    ui->stackedWidget->setCurrentWidget(ui->page_draw);
});
```

命名页继续是临时编辑状态：

```text
下一步命名
    → OCR 结果写入 m_sortedBoxes
    → 用户在 QLineEdit 中修改

保存
    → 正式写回字符框和字符图片

返回框选
    → 不把本轮名称写回 CharacterCropLabel
```

再次点击“下一步命名”时，从画布重新取得字符框；其中的空名称字符重新执行 OCR。

## 3. OCR Engine 生命周期

`CharacterTemplateEditorDialog` 自己持有一份独立 OCR Engine：

```cpp
std::unique_ptr<IOcrEngine> m_ocrEngine;
```

构造函数完成 `ui->setupUi(this)` 后，按现有部署路径直接创建 OCR Engine：

```cpp
const QString configPath =
    QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config_ocr.txt"));

try {
    m_ocrEngine.reset(
        new PaddleOcrEngine(configPath));
}
catch (const std::exception &error) {
    qCWarning(logTemplate).noquote()
        << QStringLiteral(
            "event=character_template.ocr_init_failed "
            "diagnostic=%1")
           .arg(QString::fromLocal8Bit(
                    error.what()));
}
```

初始化失败不弹窗、不关闭窗口、不禁用按钮。只有用户点击“自动分割”时才提示该功能不可用；进入命名页时静默跳过 OCR。

`m_ocrEngine` 是 `CharacterTemplateEditorDialog` 的 `std::unique_ptr` 成员，随 Dialog 析构并释放 DET/REC predictor。正式检测仍由 `DetectionRegistry` 为每次 OCR 检测运行创建自己的引擎，两者不共享实例。

## 4. OCR Engine 最终接口

`IOcrEngine` 增加两个本功能直接需要的入口：

```cpp
class IOcrEngine
{
public:
    virtual ~IOcrEngine() = default;

    virtual std::vector<OcrRecognitionItem>
    recognize(cv::Mat &image) = 0;

    virtual std::string
    recognizeCharacter(cv::Mat &image) = 0;

    virtual std::vector<OcrRecognitionItem>
    segmentCharacters(cv::Mat &image) = 0;
};
```

接口语义：

| 接口 | 输入 | 处理 | 输出 |
|---|---|---|---|
| `recognize()` | 完整文字图或 ROI | DET、排序、透视裁剪、REC | 每个文字框一个 `OcrRecognitionItem` |
| `recognizeCharacter()` | 已裁好的单字符图 | 只执行 REC，不执行 DET | 原始 REC 字符串 |
| `segmentCharacters()` | 完整字符区域图 | DET、详细 REC、字符坐标映射 | 每个已识别字符一个 `OcrRecognitionItem` |

正式 OCR 检测继续只调用 `recognize()`。字符模板编辑窗口调用 `recognizeCharacter()` 和 `segmentCharacters()`。

`PaddleOcrEngine::recognizeCharacter()` 使用覆盖整张字符图的四点框，并关闭窄高文字行旋转：

```cpp
std::string PaddleOcrEngine::recognizeCharacter(
        cv::Mat &image)
{
    const std::vector<
        std::vector<std::vector<int>>> boxes = {
        {{0, 0},
         {image.cols - 1, 0},
         {image.cols - 1, image.rows - 1},
         {0, image.rows - 1}}
    };

    return m_impl->recognizer
        ->Run(boxes, image, false)
        .front();
}
```

单字符图直接执行 REC。`1`、`I` 等窄高字符不应用文字行的 90 度旋转规则。

## 5. CRNN 详细识别结果

### 5.1 数据结构和入口

在 `ocr_rec.h` 增加：

```cpp
struct RecCharacterResult
{
    std::string text;
    cv::Rect2f normalizedRect;
};

struct RecLineResult
{
    std::string text;
    std::vector<RecCharacterResult> characters;
    bool rotated90 = false;
};
```

`CRNNRecognizer` 最终提供：

```cpp
std::vector<std::string> Run(
    const std::vector<
        std::vector<std::vector<int>>> &boxes,
    const cv::Mat &image,
    bool allowRotate90);

std::vector<RecLineResult> RunDetailed(
    const std::vector<
        std::vector<std::vector<int>>> &boxes,
    const cv::Mat &image,
    bool allowRotate90,
    bool refineCharacterBounds);
```

`Run()` 直接调用 `RunDetailed(..., false)` 并提取每个 `line.text`；只有 `segmentCharacters()` 调用 `RunDetailed(..., true)` 执行前景边界修正。正式逐帧 OCR、单字符 REC 和预热不承担字符框精修开销。所有调用点明确传入旋转规则：

```text
构造函数 REC 预热       → true
正式 recognize()        → true
segmentCharacters()     → true，并启用边界精修
recognizeCharacter()    → false
```

`RunDetailed()` 与输入文字框保持等量、同序。即使某个框识别为空，也保留对应的空 `RecLineResult`。正式 `recognize()` 的返回文本和顺序不得改变。

`PaddleOcrEngine::recognize()` 按相同索引直接使用 `texts[i]`，由上述等量合同保证每个 Detector 框都有对应文本项。

### 5.2 REC resize 返回实际内容宽度

`RecResizeImg::Run()` 直接返回本次预处理实际使用的 `contentWidth`：

```cpp
int Run(
    const cv::Mat &image,
    cv::Mat &resizedImage,
    const std::vector<int> &imageShape =
        {3, 48, 320}) const;
```

现有缩放、归一化和右侧 padding 逻辑保持不变，函数末尾返回已有局部变量 `contentWidth`。

`RunDetailed()` 使用：

```cpp
const int contentWidth =
    resize_.Run(cropped, resizedImage);
const int targetWidth = resizedImage.cols;
```

不在识别代码中重复计算 resize 公式。

### 5.3 旋转状态

`GetRotateCropImage()` 改为：

```cpp
cv::Mat GetRotateCropImage(
    const cv::Mat &image,
    const std::vector<std::vector<int>> &box,
    bool allowRotate90,
    bool &rotated90) const;
```

函数先将 `rotated90` 设为 `false`。只有 `allowRotate90` 为真且裁图高宽比不小于 `1.5` 时，才执行现有逆时针旋转并将其设为 `true`。

## 6. CTC 字符位置

### 6.1 解码

字典索引 `0` 继续表示 blank。`RunDetailed()` 对 argmax 序列按连续非 blank 段解码：

1. 非 blank 类别开始一个字符段，记录 `firstStep` 和 `lastStep`。
2. 后续 timestep 类别相同，只更新 `lastStep`。
3. 遇到 blank 或不同类别时，提交当前字符段。
4. 不同非 blank 类别立即开始新字符段。
5. timestep 遍历结束后提交最后一个字符段。

提交字符段时同时追加：

```cpp
line.text += labels_[characterIndex];
line.characters.push_back(character);
```

因此正式文本和字符位置来自同一次 CTC 解码。相同字符被 blank 分隔时形成两个结果，例如：

```text
"0" "0" blank "0" "0"
```

解码为两个 `0`。

### 6.2 padding 换算

字符段中心先作为本次解码中的临时位置锚点：

```cpp
const float centerStep =
    (firstStep + lastStep) * 0.5f;
```

REC 输出包含右侧 padding 对应的 timestep，字符位置按有效内容宽度换算：

```cpp
const float effectiveSteps =
    static_cast<float>(outputShape[1])
    * static_cast<float>(contentWidth)
    / static_cast<float>(targetWidth);

centerRatios.push_back(std::max(
    0.0f,
    std::min(
        (centerStep + 0.5f)
            / effectiveSteps,
        1.0f)));
```

`centerRatios` 只参与当前文字行的像素边界计算，不进入 `RecCharacterResult`。最终结果只保存前景投影修正后的 `normalizedRect`。

## 7. 字符框生成

### 7.1 前景掩码

`segmentCharacters()` 启用 `refineCharacterBounds` 时，`RunDetailed()` 直接使用现有透视矫正文字行 `cropped`：

```cpp
cv::cvtColor(cropped, gray, cv::COLOR_BGR2GRAY);
cv::threshold(gray, foreground, 0.0, 255.0,
              cv::THRESH_BINARY | cv::THRESH_OTSU);
if (cv::countNonZero(foreground)
        > foreground.rows * foreground.cols / 2) {
    cv::bitwise_not(foreground, foreground);
}
```

二值图中像素数量较少的一类作为文字前景。该步骤不增加阈值配置、形态学流程、轮廓合并或多套预处理分支。

### 7.2 字符左右边界

同一文字行中的全部识别字符继续按 CTC 顺序处理。对于每两个相邻字符，在两个临时中心之间统计每一列的前景像素数量，选择前景像素最少的列作为分界；存在多列并列时，选择最接近原 CTC 中点的列。

首字符左边界和末字符右边界分别取文字行的首个及末个前景列，并保留文字行高度 `3%`、至少 `1` 像素的外边距。内部字符边界不越过投影得到的分界列。

日期中的 `-`、`/`、`:` 等字符在此阶段仍参与全部相邻边界计算，Dialog 在收到最终结果后才执行模板字符过滤。

### 7.3 字符上下边界

每个字符在确定的左右范围内独立查找首个和末个前景行，上下分别保留文字行高度 `3%`、至少 `1` 像素的背景。某个字符范围内没有前景像素时保留完整文字行高度，不增加第二套阈值或 OCR 重试。

最终将像素边界换算为透视矫正文字行中的归一化矩形：

```cpp
character.normalizedRect = cv::Rect2f(
    left / lineWidth,
    top / lineHeight,
    (right - left) / lineWidth,
    (bottom - top) / lineHeight);
```

### 7.4 映射回 Detector 四点框

Detector 点顺序为左上、右上、右下、左下：

```text
p0 -------- p1
|            |
p3 -------- p2
```

未旋转文字行把归一化点 `(horizontal, vertical)` 映射到 Detector 上下边缘之间：

```cpp
topEdge = p0 + (p1 - p0) * horizontal;
bottomEdge = p3 + (p2 - p3) * horizontal;
point = topEdge + (bottomEdge - topEdge) * vertical;
```

执行过 90 度旋转的文字行按旋转后的横纵方向映射：

```cpp
leftEdge = p0 + (p3 - p0) * horizontal;
rightEdge = p1 + (p2 - p1) * horizontal;
point = rightEdge + (leftEdge - rightEdge) * vertical;
```

每个字符输出：

```cpp
OcrRecognitionItem item;
item.text = character.text;
item.box = {
    roundedTopLeft,
    roundedTopRight,
    roundedBottomRight,
    roundedBottomLeft
};
```

坐标使用 `cvRound()` 转为现有 `cv::Point`，不改变公共数据结构。

### 7.5 `segmentCharacters()`

```cpp
std::vector<OcrRecognitionItem>
PaddleOcrEngine::segmentCharacters(
        cv::Mat &image)
{
    const auto boxes =
        PaddleOCR::Utility::SortQuadBoxes(
            m_impl->detector->Run(image));

    const auto lines =
        m_impl->recognizer->RunDetailed(
            boxes, image, true, true);

    std::vector<OcrRecognitionItem> result;

    // 对每个文字行：
    // 1. RunDetailed 已返回前景投影修正后的 normalizedRect；
    // 2. 按 rotated90 映射到原 Detector 框；
    // 3. 按原顺序追加每个字符结果。

    return result;
}
```

日期中的 `-`、`/`、`:` 等字符必须参与边界计算。Dialog 在收到完整字符结果后，才通过统一的 `filteredTemplateCharacter()` 过滤非模板目标字符：

```cpp
if (filteredTemplateCharacter(
        QString::fromStdString(item.text)).isEmpty()) {
    continue;
}
```

例如 `2026-09-08` 先按 10 个字符计算边界，最终生成 8 个数字框。

## 8. CharacterTemplateEditorDialog 改动

### 8.1 成员和方法

头文件增加：

```cpp
class IOcrEngine;

void autoSplitCharacters();
void recognizeUnnamedCharacterNames();

std::unique_ptr<IOcrEngine> m_ocrEngine;
```

构造函数签名保持不变，不向 `MainWindow` 或 `TemplateEditorPage` 增加 OCR 参数。

画布设置源图后直接载入现有字符框：

```cpp
ui->label_characterCropCanvas->setSourceImage(m_sourceImage);
ui->label_characterCropCanvas->setItems(
    m_resultSettings.characterBoxes);
```

载入后以 `CharacterCropLabel::m_items` 作为唯一画布状态。

### 8.2 统一字符过滤与 QImage 转 OpenCV

在 Dialog 实现文件的匿名 namespace 中增加：

```cpp
QString filteredTemplateCharacter(
        const QString &text)
{
    const QString value = text.trimmed();
    const QStringList units =
        TemplateStore::templateTargetUnits(value);

    if (value.size() != 1
            || units.size() != 1) {
        return QString();
    }
    return value;
}

cv::Mat bgrMatFromQImage(const QImage &source)
{
    QImage rgb =
        source.convertToFormat(
            QImage::Format_RGB888);

    cv::Mat rgbView(
        rgb.height(),
        rgb.width(),
        CV_8UC3,
        rgb.bits(),
        rgb.bytesPerLine());

    cv::Mat bgr;
    cv::cvtColor(
        rgbView,
        bgr,
        cv::COLOR_RGB2BGR);
    return bgr;
}
```

`cv::cvtColor()` 生成独立的 BGR 图像，因此返回值不依赖局部 `QImage` 生命周期。

### 8.3 自动分割

构造函数连接：

```cpp
connect(ui->pushButton_autoSplitCharacters,
        &QPushButton::clicked,
        this,
        &CharacterTemplateEditorDialog::autoSplitCharacters);
```

处理顺序：

1. `m_ocrEngine` 为空时显示“自动分割不可用”并返回。
2. 将 `m_sourceImage` 转为 BGR。
3. 调用 `segmentCharacters()`。
4. 通过 `filteredTemplateCharacter()` 保留受支持的单字符结果。
5. 将保留字符的四点框转换为 `QRect`，过滤宽度小于 4 像素或高度小于 4 像素的框。
6. 为每个保留框设置空名称。
7. 临时结果为空时提示继续手动框选。
8. 画布没有字符框时，直接调用 `setItems(generatedBoxes)`。
9. 画布已有字符框时，提示自动分割结果将一次性替换现有全部字符框；用户确定后调用 `setItems(generatedBoxes)`，用户取消则保持现有框不变。

核心结构：

```cpp
void CharacterTemplateEditorDialog::autoSplitCharacters()
{
    if (!m_ocrEngine) {
        QMessageBox::information(
            this,
            QStringLiteral("自动分割不可用"),
            QStringLiteral(
                "深度 OCR 自动分割功能当前不可用，"
                "请继续手动框选字符。"));
        return;
    }

    cv::Mat source =
        bgrMatFromQImage(m_sourceImage);

    try {
        const auto result =
            m_ocrEngine->segmentCharacters(source);
        QVector<TemplateCharacterBox> generatedBoxes;

        for (const OcrRecognitionItem &item
             : result) {
            if (filteredTemplateCharacter(
                    QString::fromStdString(
                        item.text)).isEmpty()) {
                continue;
            }

            const cv::Rect detectedRect =
                cv::boundingRect(item.box);

            TemplateCharacterBox box;
            box.rect = QRect(
                detectedRect.x,
                detectedRect.y,
                detectedRect.width,
                detectedRect.height);

            if (box.rect.width() < 4
                    || box.rect.height() < 4) {
                continue;
            }

            generatedBoxes.append(box);
        }

        if (generatedBoxes.isEmpty()) {
            QMessageBox::information(
                this,
                QStringLiteral("自动分割"),
                QStringLiteral(
                    "没有识别到可分割的字符，"
                    "请继续手动框选。"));
            return;
        }

        if (!ui->label_characterCropCanvas
                ->items().isEmpty()
                && QMessageBox::question(
                    this,
                    QStringLiteral("替换字符框"),
                    QStringLiteral(
                        "自动分割将一次性替换现有全部字符框，"
                        "是否继续？"),
                    QMessageBox::Ok
                        | QMessageBox::Cancel,
                    QMessageBox::Cancel)
                    != QMessageBox::Ok) {
            return;
        }

        ui->label_characterCropCanvas
            ->setItems(generatedBoxes);
    }
    catch (const std::exception &error) {
        qCWarning(logTemplate).noquote()
            << QStringLiteral(
                "event=character_template.auto_split_failed "
                "diagnostic=%1")
               .arg(QString::fromLocal8Bit(
                        error.what()));

        QMessageBox::warning(
            this,
            QStringLiteral("自动分割失败"),
            QStringLiteral(
                "深度 OCR 自动分割失败，"
                "请继续手动框选字符。"));
    }
}
```

不在调用 OCR 前清空画布。推理失败、空结果和用户取消替换都保持现有框不变；只有非空临时结果生成完成，并且画布为空或用户确认替换时，才一次调用 `setItems(generatedBoxes)`。

`generatedBoxes` 只在替换确认前保存临时结果。调用 `setItems()` 后，框只存在于 `CharacterCropLabel::m_items`；用户可以继续手动画框，删除上一个字符框、清空、编号、预览、排序、命名和保存流程统一作用于全部框。

### 8.4 未命名字符识别

`moveToNamingPage()` 在现有排序之后、`rebuildNamePage()` 之前调用：

```cpp
recognizeUnnamedCharacterNames();
```

识别逻辑：

```cpp
void CharacterTemplateEditorDialog::
recognizeUnnamedCharacterNames()
{
    if (!m_ocrEngine) {
        return;
    }

    cv::Mat source =
        bgrMatFromQImage(m_sourceImage);

    for (int index = 0;
         index < m_sortedBoxes.size();
         ++index) {
        TemplateCharacterBox &box =
            m_sortedBoxes[index];

        if (!box.name.trimmed().isEmpty()) {
            continue;
        }

        const QRect rect = box.rect;
        cv::Mat characterImage = source(
            cv::Rect(rect.x(),
                     rect.y(),
                     rect.width(),
                     rect.height()));

        try {
            const QString recognizedText =
                filteredTemplateCharacter(
                    QString::fromStdString(
                        m_ocrEngine
                            ->recognizeCharacter(
                                characterImage)));

            if (!recognizedText.isEmpty()) {
                box.name = recognizedText;
            }
        }
        catch (const std::exception &error) {
            qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=character_template.ocr_name_failed "
                    "index=%1 diagnostic=%2")
                   .arg(index)
                   .arg(QString::fromLocal8Bit(
                            error.what()));
        }
    }
}
```

自动分割和单字符自动命名都通过 `filteredTemplateCharacter()` 调用 `TemplateStore::templateTargetUnits()`，使用同一字符过滤规则。过滤通过的单字符文本直接作为命名初始值，命名页继续负责人工确认。

OCR 结果只在 `rebuildNamePage()` 前写入空的 `box.name`，随后与已有名称一样成为普通 `QLineEdit` 的初始文本。用户修改输入框后，`saveTemplates()` 统一读取输入框中的最终值；不记录名称来自 OCR 还是人工输入，也不采用不同的校验和保存规则。

## 9. UI 与现有流程

绘图页按钮区增加：

```text
[删除上一个字符框]
[清空所有字符框]
[自动分割]
                         [下一步命名] [取消]
```

按钮对象名：

```text
pushButton_removeLastCharacterBox
pushButton_autoSplitCharacters
```

现有连接调整为：

```cpp
connect(ui->pushButton_removeLastCharacterBox,
        &QPushButton::clicked,
        ui->label_characterCropCanvas,
        &CharacterCropLabel::removeLast);
```

`CharacterCropLabel::removeLast()` 直接删除 `m_items` 的最后一项。

`CharacterCropLabel::setItems()` 对传入矩形完成标准化和边界裁剪，再应用最小尺寸：

```cpp
normalizedItem.rect = item.rect.normalized()
    .intersected(imageBounds);
if (normalizedItem.rect.width() >= 4
        && normalizedItem.rect.height() >= 4) {
    m_items.append(normalizedItem);
}
```

`previewItems()` 和 `mouseReleaseEvent()` 直接使用已限制在图像内的 `m_currentRect.normalized()`，并采用相同的 `4 × 4` 像素最小尺寸。

`items()` 直接返回 `m_items`。

`m_sortedBoxes` 来自 `items()`；`rebuildNamePage()`、`refreshCharacterPreviewList()` 和 `saveTemplates()` 直接使用其中的 `box.rect` 完成裁图、预览和保存。

绘图页提示：

```text
在喷码区域图像上按住鼠标左键拖拽，逐个框选字符；
也可以点击“自动分割”，使用深度 OCR 辅助生成字符框。
两种方式生成的字符框后续操作完全一致。
字符框选顺序不限，系统会在命名前自动按位置排序。
```

命名页提示：

```text
未命名字符会尝试使用 OCR 自动识别名称，请逐个确认。
已有名称的字符不会重新识别，识别错误或未识别时可手动修改。
重复名称会自动生成 1(1)、1(2)。
```

保持以下现有逻辑：

- `CharacterCropLabel` 的统一列表及绘制、删除上一个字符框、清空、编号和预览。
- 字符框现有排序规则。
- `rebuildNamePage()` 的控件结构。
- `QLineEdit` 人工修改。
- 空名称和非法文件名校验。
- `characterStorageStem()` 和重复名称编号。
- `saveTemplates()`、`TemplateSettings::characterBoxes` 和字符图片生成。
- `TemplateApplicationService::stageCharacterAssets()`。
- 返回框选时放弃本轮未保存名称。

实现继续使用现有 Dialog、`CharacterCropLabel`、`TemplateCharacterBox` 和 OCR Engine，不新增生产源码文件、模板字段、后台流程或业务层级。

## 10. 修改文件

| 文件 | 修改内容 |
|---|---|
| `app/engines/ocr/ocr_engine.h` | 增加 `recognizeCharacter()` 和 `segmentCharacters()` |
| `app/engines/ocr/vendor/paddle_ocr_engine.h` | 增加两个接口实现声明 |
| `app/engines/ocr/vendor/paddle_ocr_engine.cpp` | 更新 REC 调用参数，实现单字符 REC，并将归一化字符矩形映射回 Detector 框 |
| `app/engines/ocr/vendor/paddle/include/preprocess_op.h` | `RecResizeImg::Run()` 返回 `contentWidth` |
| `app/engines/ocr/vendor/paddle/src/preprocess_op.cpp` | 返回现有 `contentWidth` |
| `app/engines/ocr/vendor/paddle/include/ocr_rec.h` | 增加带归一化字符矩形的详细结果、显式旋转参数和 `RunDetailed()` |
| `app/engines/ocr/vendor/paddle/src/ocr_rec.cpp` | 单路 CTC 解码生成文本和临时位置锚点，通过前景投影修正最终字符矩形 |
| `app/ui/main_window/template/character_editor/character_crop_label.h/.cpp` | 提供 `removeLast()`；由画布入口统一完成矩形标准化、边界和 `4 × 4` 像素最小尺寸处理，`items()` 直接返回统一列表 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.h` | 增加 OCR 成员、自动分割和自动命名方法；画布字符框由 `m_resultSettings` 和 `CharacterCropLabel` 持有 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.cpp` | 构造时直接载入已有字符框并初始化 OCR，实现自动分割和自动命名并连接删除末项按钮；命名、预览和保存直接使用画布中的有效矩形 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.ui` | 增加自动分割按钮，更新删除末项按钮对象名和界面提示 |
| `app/resource/Translate_CN.ts`、`app/resource/Translate_EN.ts` | 更新界面文案 |
| `app/engines/README.md` | 记录三个 OCR 接口的最终职责 |
| `docs/development/PP-OCRv6_tiny_深度OCR重构实施方案.md` | 同步最终 OCR 接口和调用方 |
| `docs/development/OCRGangYin深度OCR启动前同步初始化精简方案.md` | 明确正式检测与字符模板窗口分别持有独立引擎 |
| `docs/development/OCRGangYin现有功能对照表.md` | 登记字符模板 OCR 辅助功能 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案和实施状态 |

不修改：

```text
app/startup/application_startup.cpp
app/ui/main_window/main_window.h/.cpp
app/ui/main_window/template/template_editor_page.h/.cpp
app/templates/template_store.h/.cpp
app/detection/
app/runtime/
模板 Schema
```

没有新增源码文件，`app/AutoOCRproject.pro` 的 SOURCES、HEADERS 和 FORMS 清单不变。

## 11. 验收

### 11.1 构建与静态检查

- 当前 Qt Creator Kit 执行 Run qmake 和 Release Rebuild 成功。
- `git diff --check` 通过。
- `ApplicationStartup`、`MainWindow` 和 `TemplateEditorPage` 不出现 OCR 配置转发参数。
- `IOcrEngine` 只有 `recognize()`、`recognizeCharacter()`、`segmentCharacters()` 三个业务入口。
- `CRNNRecognizer` 最终只有一个 `Run()` 和一个 `RunDetailed()`，旋转和字符边界精修参数由所有调用点显式传入，CTC 只解码一次；只有 `segmentCharacters()` 启用边界精修。
- `RecResizeImg::Run()` 的唯一签名直接返回 `contentWidth`。
- `PaddleOcrEngine::recognize()` 按 `Run()` 的等量结果直接读取 `texts[i]`。
- `RecCharacterResult` 只保存文本和最终 `normalizedRect`，CTC 中心不作为第二套结果状态保留。
- 前景边界修正只有一次 Otsu 二值化、列投影和字符范围内的行投影，不增加配置项、形态学流程、轮廓合并或模型回退。
- `TemplateCharacterBox`、模板 Schema 和 `CharacterCropLabel` 中没有自动或手动来源字段及来源分支。
- `CharacterCropLabel` 使用 `removeLast()` 删除列表末项，按钮对象名为 `pushButton_removeLastCharacterBox`，界面文字为“删除上一个字符框”。
- 自动分割替换前过滤、`CharacterCropLabel::setItems()`、手动画框和绘制预览统一采用 `4 × 4` 像素最小尺寸，`items()` 不重复过滤。
- `CharacterTemplateEditorDialog` 构造函数把 `m_resultSettings.characterBoxes` 直接写入画布，载入后的字符框只保存在 `CharacterCropLabel::m_items`。
- 命名页、预览和保存直接使用 `items()` 返回的有效矩形。

### 11.2 OCR 初始化和生命周期

- 打开字符模板窗口时成功创建一份独立 OCR Engine。
- 配置、模型或运行库不可用时，窗口仍可打开并完成手动画框、人工命名和保存。
- OCR 不可用时点击“自动分割”只提示本次操作不可用，现有框不变。
- `CharacterTemplateEditorDialog` 析构时由 `m_ocrEngine` 自动释放该窗口的 OCR Engine。
- 正式 OCR 检测仍按现有 Registry 生命周期独立创建、复用和销毁引擎。

### 11.3 自动分割

使用实际字符区域分别验证：

```text
2026-09-08
2026/09/08
12:30
00
11
水平文字
轻微倾斜文字
触发现有 90 度旋转规则的窄高文字行
```

要求：

- 画布没有字符框时，自动分割成功后直接绘制全部字符级矩形框，框名称全部为空。
- 画布已有字符框时，自动分割成功后提示将一次性替换现有全部字符框。
- 用户确定替换后一次绘制全部新框；用户取消时现有框保持不变。
- 自动分割结果进入画布后，可以继续手动画框；自动生成和随后手动生成的框使用相同样式、编号和预览。
- 自动分割替换前和 `CharacterCropLabel` 的画布入口使用相同的 `4 × 4` 像素最小尺寸标准；任一边小于 4 像素的框均被过滤。
- “删除上一个字符框”每次删除统一列表末项：纯自动结果删除 OCR 阅读顺序末项，追加手动画框后先删除最后追加项。
- “清空所有字符框”直接清空统一列表，不区分框的生成方式。
- `-`、`/`、`:` 参与边界计算后再被过滤。
- 相邻字符的内部边界落在两个 CTC 中心之间前景像素最少的列，不明显带入相邻字符笔画。
- 每个字符按自身前景得到独立上下边界，并保留文字行高度 `3%`、至少 `1` 像素的背景，不再统一占满整行高度。
- 标点、空白和多字符结果不生成字符框。
- `2026-09-08` 最终得到 8 个数字框。
- `00`、`11` 中被 blank 分隔的重复字符分别生成两个框。
- 短文本右侧 padding 不造成明显坐标偏移。
- 水平、倾斜和旋转文字映射方向正确。
- 推理异常、空结果和用户取消替换都不清除原有框。
- 结果不合适时可清空并回到手动画框。

### 11.4 自动命名

- 已保存且名称非空的字符完全跳过 OCR。
- 新手动画框和自动分割框使用 REC-only 识别。
- `1`、`I` 等窄高单字符图不旋转。
- 与自动分割使用相同的 `TemplateStore::templateTargetUnits()` 过滤规则。
- 过滤通过的单字符结果直接预填，用户可以修改。
- OCR 预填和人工填写使用同一个 `QLineEdit`；用户修改后的文本是保存时读取的唯一最终值。
- 名称进入输入框后不保留 OCR 或人工来源，也不使用不同的校验和保存规则。
- 标点、空白、多字符结果和 `1(1)` 形式的重复模板引用保持空名称。
- 单个字符无结果或异常时保持为空，其它字符继续处理，命名页正常打开。
- OCR 初始化失败时命名页仍正常打开并允许人工填写。

### 11.5 返回与保存

- 返回框选后不保留本轮 OCR 和人工命名。
- 再次进入命名页时空名称字符重新识别。
- 保存继续执行现有空名称、非法文件名、重复名称和字符图片规则。
- 保存后的 `TemplateSettings::characterBoxes`、字符图片和模板发布结果正确。

### 11.6 正式 OCR 回归

- 改造前后同一组样本的 `CRNNRecognizer::Run()` 文本、顺序一致。
- `PaddleOcrEngine::recognize()` 的返回项数量、文本和四点框语义不变。
- 正式 OCR 模式启动、识别、停止、再次启动正常。
- 正式 OCR 初始化失败仍不创建检测 Worker。
- 正式 OCR 逐帧 DET/REC 异常仍进入 Runtime Fault。
- 其它检测模式不受影响。
