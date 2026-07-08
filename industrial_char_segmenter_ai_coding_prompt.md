# 工业字符自动分割 Agent / Skill：AI Coding 完整提示词


## 主提示词

```text
帮我从零开发一个“工业字符自动分割 Agent/Skill”项目。

项目目标：
输入一张工业视觉图片和一个已知目标字符串，自动把图片中的每一个字符分割出来，并按目标字符串顺序保存为单字符图片。

核心场景：
我后续要使用 OpenCV 做模板字符匹配，所以分割出来的字符图片必须直接来自原图裁剪，不能做任何图像处理。

非常重要的硬性要求：
1. 输出字符图片必须从原图直接 crop。
2. 禁止对最终输出字符图片做任何处理，包括但不限于：
   - 灰度化
   - 二值化
   - 去噪
   - 锐化
   - 透明背景
   - resize
   - rotate
   - normalize
   - morphology
   - 色彩增强
   - 重绘
   - AI 生成
3. 允许在内部分析阶段使用 OpenCV 或视觉大模型来计算 bbox。
4. 但最终保存的字符图必须是：
   crop = original_image[y1:y2, x1:x2]
5. 目标字符串是已知的，大模型不需要重新识别字符，只需要按目标字符串顺序分割。
6. 每个字符一个 bbox。
7. 输出字符数量必须严格等于 len(target_text)。
8. 如果原图中有多行字符，输出顺序为从上到下、每行从左到右。
9. 不要把分隔符、背景、噪声、边框当作字符输出。
10. 如果字符粘连，需要根据目标字符串长度和字符宽度趋势进行切分。
11. bbox 要尽量贴合字符外接矩形，但允许保留 1 到 2 像素安全边距，避免切掉字符边缘。
12. 项目必须适合工业视觉批量处理，便于后续接入真实视觉大模型。

请使用 Python 开发，建议 Python 3.10+。

项目目录结构请设计为：

industrial_char_segmenter/
│
├── README.md
├── requirements.txt
├── .gitignore
│
├── data/
│   ├── input/
│   └── output/
│
├── prompts/
│   └── segmenter_prompt.txt
│
├── src/
│   ├── __init__.py
│   ├── cli.py
│   ├── agent.py
│   ├── vlm_client.py
│   ├── validator.py
│   ├── cropper.py
│   ├── preview.py
│   ├── json_utils.py
│   └── geometry_utils.py
│
└── examples/
    └── example_run.py

请完整实现这些文件。

一、整体功能要求

命令行调用方式：

python -m src.cli --image data/input/5.bmp --target "202803192028031912345ABC" --output data/output/run_001

运行后输出：

data/output/run_001/
│
├── chars_original_crop/
│   ├── 001_2.png
│   ├── 002_0.png
│   ├── 003_2.png
│   └── ...
│
├── segments_model.json
├── segments_final.json
├── preview_boxes.png
└── run_report.json

说明：
- chars_original_crop/ 保存直接从原图裁剪出来的字符。
- segments_model.json 保存视觉大模型输出的原始 bbox。
- segments_final.json 保存校验、修正后的 bbox。
- preview_boxes.png 是画框预览图，仅用于人工检查，不能作为模板库。
- run_report.json 保存本次运行状态、错误、警告、字符数量、输出路径等信息。

二、坐标规则

统一使用 OpenCV/Python 切片坐标：

bbox = [x1, y1, x2, y2]

含义：
- x1, y1：左上角坐标，包含。
- x2, y2：右下角外侧坐标，不包含。
- 裁剪时使用：image[y1:y2, x1:x2]

禁止使用“右下角包含”的坐标规则。

三、prompts/segmenter_prompt.txt

请创建一个视觉大模型提示词模板，内容大致如下，并可以适当优化：

你是工业视觉字符分割专家。

你的唯一任务：
根据输入图片和目标字符串，输出每一个目标字符在原图中的精确 bbox 坐标。

目标字符串：
{target_text}

严格要求：
1. 只做字符分割，不做字符识别、不纠错、不补全。
2. 输出字符数量必须严格等于目标字符串长度。
3. bbox 必须基于原图像素坐标。
4. 每个字符一个 bbox。
5. 输出顺序必须严格对应目标字符串顺序。
6. 如果图片中有多行字符，按从上到下、每行从左到右的顺序输出。
7. 不要输出分隔符、背景、噪声、边框。
8. bbox 应尽量贴合字符外接矩形，但可以保留 1 到 2 像素安全边距，避免切掉字符边缘。
9. 如果字符粘连，根据目标字符串长度和字符宽度趋势进行切分。
10. 不要对图像做任何增强、二值化、去噪、透明化、重绘、旋转或缩放。
11. 你只输出 JSON，不要输出解释。

坐标规则：
bbox = [x1, y1, x2, y2]

其中：
- x1, y1 是左上角坐标，包含该像素。
- x2, y2 是右下角外侧坐标，不包含该像素。
- 裁剪时会使用 image[y1:y2, x1:x2]。
- 所有 bbox 不得超出原图范围。

输出 JSON 格式：

{
  "image_width": 原图宽度,
  "image_height": 原图高度,
  "target_text": "{target_text}",
  "char_count": 目标字符串长度,
  "segments": [
    {
      "index": 1,
      "char": "第1个字符",
      "bbox": [x1, y1, x2, y2],
      "confidence": 0.0到1.0
    }
  ],
  "notes": ""
}

四、src/vlm_client.py

请实现一个视觉大模型客户端抽象层。

要求：
1. 提供 VLMClient 基类。
2. 提供 MockVLMClient，用于开发测试。
3. MockVLMClient 可以从用户提供的 JSON 文件读取 bbox，方便不用真实大模型也能测试完整流程。
4. 预留 RealVLMClient 接口，方便以后接入真实视觉大模型。
5. 真实模型接口暂时不用写死具体平台，但代码结构要方便后续扩展。
6. 提供 extract_json_from_text(text) 函数，用于从模型输出中提取 JSON。
7. 如果模型返回 ```json 包裹，也要能正确解析。
8. 如果模型返回非法 JSON，需要抛出清晰错误。

五、src/validator.py

请实现 bbox 校验功能。

函数建议：

validate_segments(data, target_text, image_width, image_height) -> (valid_data, report)

需要检查：
1. 是否有 segments 字段。
2. segments 数量是否等于 len(target_text)。
3. index 是否从 1 开始连续。
4. char 是否严格等于 target_text 对应位置字符。
5. bbox 是否为长度为 4 的整数数组。
6. bbox 是否越界。
7. x2 是否大于 x1。
8. y2 是否大于 y1。
9. bbox 宽度、高度是否过小。
10. 相邻 bbox 是否严重重叠。
11. bbox 顺序是否明显异常。
12. 是否存在可能切边风险。

处理策略：
- 可以自动修正轻微越界。
- 可以将 float 坐标转换成 int，建议 round 后转 int。
- 对严重错误要报错。
- 返回 report，包含 errors 和 warnings。

六、src/cropper.py

请实现原图直接裁剪功能。

函数建议：

crop_original_chars(image_path, segments_data, output_dir) -> dict

要求：
1. 使用 cv2.imread(image_path, cv2.IMREAD_UNCHANGED) 读取原图。
2. 不能对原图做任何处理。
3. 对每一个 bbox 执行：
   crop = image[y1:y2, x1:x2]
4. 保存到 chars_original_crop/。
5. 文件名格式：
   001_2.png
   002_0.png
   003_2.png
6. 如果字符是特殊字符，文件名中使用 unicode 编码形式，避免非法文件名。
7. 保存图片时不做 resize，不做转换，不做透明化。
8. segments_final.json 中要记录每个字符对应的 filename。
9. 返回输出目录、文件数量、文件列表。

七、src/preview.py

请实现画框预览图功能。

函数建议：

draw_preview(image_path, segments, output_path)

要求：
1. 读取原图。
2. 在原图副本上画 bbox。
3. 标注 index 和 char。
4. 保存 preview_boxes.png。
5. 预览图仅用于人工检查，不参与模板库。
6. 画框颜色可以固定为红色。
7. 不要覆盖原图。

八、src/agent.py

请实现主流程 Agent。

函数建议：

run_char_segmentation_agent(
    image_path: str,
    target_text: str,
    output_dir: str,
    model_json_path: str | None = None,
    max_retry: int = 2
) -> dict

流程：
1. 创建输出目录。
2. 读取原图尺寸。
3. 加载 prompts/segmenter_prompt.txt。
4. 调用 VLMClient 获取 bbox。
5. 保存 segments_model.json。
6. 调用 validator 校验 bbox。
7. 如果校验失败：
   - 如果是严重错误，记录到 report。
   - 如果启用了重试，允许重新调用模型。
8. 生成 segments_final.json。
9. 调用 preview.py 生成 preview_boxes.png。
10. 调用 cropper.py 从原图直接裁剪字符。
11. 生成 run_report.json。
12. 返回完整结果。

九、src/cli.py

请实现命令行入口。

支持参数：
--image       输入图片路径，必填
--target      目标字符串，必填
--output      输出目录，必填
--model-json  可选，用于 MockVLMClient，从已有 JSON 读取 bbox
--max-retry   可选，默认 2

示例：

python -m src.cli --image data/input/5.bmp --target "202803192028031912345ABC" --output data/output/run_001 --model-json data/input/test_segments.json

十、src/json_utils.py

请实现 JSON 读写工具：
- read_json(path)
- write_json(path, data)
- ensure_dir(path)

要求：
- UTF-8
- ensure_ascii=False
- indent=2

十一、src/geometry_utils.py

请实现几何工具函数：
- clamp_bbox(bbox, width, height)
- bbox_area(bbox)
- bbox_width(bbox)
- bbox_height(bbox)
- bbox_iou(b1, b2)
- sort_segments_reading_order(segments)
- has_large_overlap(b1, b2)

十二、README.md

请写完整说明，包含：
1. 项目用途。
2. 为什么最终字符图必须直接从原图裁剪。
3. 安装方式。
4. 命令行使用方式。
5. 使用 MockVLMClient 测试方式。
6. 如何接入真实视觉大模型。
7. 输出目录说明。
8. 注意事项。
9. 常见问题。

十三、requirements.txt

至少包含：
opencv-python
numpy

如有需要，可以加入：
pillow

十四、重要开发约束

请注意：
1. 不要把所有代码写在一个文件里。
2. 不要省略错误处理。
3. 不要只写伪代码。
4. 必须给出可运行的完整代码。
5. 所有函数要有清晰注释。
6. 工业视觉场景下，程序要尽量稳健。
7. 如果模型输出有小问题，程序能自动修正轻微问题。
8. 如果模型输出严重错误，程序要明确报错。
9. 不能默默失败。
10. 不能用处理后的图作为输出字符图。

十五、验收标准

完成后，请确保满足：

1. 使用 --model-json 输入已有 bbox JSON 时，可以完整跑通。
2. 输出 chars_original_crop/。
3. 输出字符数量等于 len(target_text)。
4. 输出字符图片均来自原图直接裁剪。
5. 输出 preview_boxes.png。
6. 输出 segments_final.json。
7. 输出 run_report.json。
8. bbox 坐标采用 [x1, y1, x2, y2]，其中 x2/y2 为外侧坐标。
9. 代码可以直接运行。
10. README 里有清晰使用说明。

十六、请先输出完整项目代码

请按照文件路径逐个给出代码，例如：

industrial_char_segmenter/requirements.txt
industrial_char_segmenter/src/cropper.py
industrial_char_segmenter/src/validator.py
...

不要只解释思路，要直接给我完整可落地代码。
```

---

## 代码生成后自检修复提示词

如果 AI Coding 工具第一次生成后跑不通，把下面这段继续发给它：

```text
请检查你刚才生成的项目代码，重点修复以下问题：

1. 确保 python -m src.cli 可以从项目根目录运行。
2. 确保所有 import 路径正确。
3. 确保 --model-json 模式不用真实大模型也可以跑通。
4. 确保 cv2.imread 使用 cv2.IMREAD_UNCHANGED。
5. 确保最终字符图只通过 image[y1:y2, x1:x2] 从原图裁剪。
6. 确保不对输出字符图做灰度化、二值化、resize、透明化等处理。
7. 确保 segments 数量必须等于 target_text 长度。
8. 确保 bbox 坐标规则是 [x1, y1, x2, y2]，x2/y2 为外侧坐标。
9. 确保输出目录包含：
   - chars_original_crop/
   - segments_model.json
   - segments_final.json
   - preview_boxes.png
   - run_report.json
10. 请直接修改代码，不要只给建议。
```

---

## 使用建议

先用主提示词让 AI Coding 工具生成项目，再用自检修复提示词让它检查导入路径、命令行入口、Mock 流程和原图裁剪约束。

本项目最关键的原则是：

```python
crop = original_image[y1:y2, x1:x2]
```

也就是说，大模型只负责给坐标，最终字符模板必须由程序从原图直接裁剪。
