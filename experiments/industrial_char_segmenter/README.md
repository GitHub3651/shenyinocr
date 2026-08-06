# 工业字符自动分割 Agent / Skill

这个项目用于把一张工业视觉图片中的字符，按照已知目标字符串顺序，分割保存为单字符图片。模型或 Mock JSON 只负责提供 bbox 坐标，最终字符图片始终从原图直接裁剪。

最关键原则：

```python
crop = original_image[y1:y2, x1:x2]
```

最终输出字符图不会做灰度化、二值化、去噪、锐化、透明背景、resize、rotate、normalize、morphology、色彩增强、重绘或 AI 生成。

## 项目结构

```text
industrial_char_segmenter/
├── README.md
├── requirements.txt
├── .gitignore
├── run_gui.py
├── run_gui.bat
├── data/
│   ├── input/
│   └── output/
├── prompts/
│   └── segmenter_prompt.txt
├── src/
│   ├── __init__.py
│   ├── gui.py
│   ├── cli.py
│   ├── agent.py
│   ├── vlm_client.py
│   ├── provider_presets.py
│   ├── validator.py
│   ├── cropper.py
│   ├── preview.py
│   ├── image_io.py
│   ├── json_utils.py
│   └── geometry_utils.py
└── examples/
    └── example_run.py
```

## 安装

建议使用 Python 3.10+。

```bash
cd industrial_char_segmenter
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

Linux/macOS 激活虚拟环境时使用：

```bash
source .venv/bin/activate
```

## GUI 使用

Windows 上可以直接双击：

```text
run_gui.bat
```

也可以从项目根目录运行：

```bash
python -m src.gui
```

界面操作：

1. 点击“图片”旁边的“浏览”，选择工业视觉图片。
2. 在“目标字符串”输入已知字符内容。
3. 选择“输出目录”，或点击“新输出目录”自动生成一个目录。
4. 在“模型厂商”中选择 `千问 / 阿里云百炼`、`智谱 GLM`、`Kimi / Moonshot` 或 `DeepSeek`。
5. 在“模型”中选择对应模型。
6. 只填写该厂商的 `API Key`。
7. 千问如果连接失败，可填写“千问业务空间ID（可选）”，程序会切换到阿里云官方推荐的业务空间域名。
8. 点击“测试连接”。绿色表示 API Key、厂商和模型可用；红色表示连接或权限失败。
9. 不选择“测试 bbox JSON”时，程序会调用真实视觉模型 API。
10. 点击“开始分割”。

运行成功后会生成 `chars_original_crop/`、`segments_model.json`、`segments_final.json`、`preview_boxes.png` 和 `run_report.json`。勾选“完成后打开输出目录”时，程序会自动打开结果目录。

“测试 bbox JSON”仍然保留，但只用于离线调试。选择它时，本次运行不会调用真实 API。

GUI 会显示明确进度，包括读取图片、调用模型、校验 bbox、生成预览、裁剪字符和写报告；长时间停留在“调用视觉模型”通常表示正在等待 API 返回。

GUI 中“失败后重试次数”表示首次调用失败后额外再试几次。填 `0` 就只调用一次；填 `2` 表示首次调用加最多 2 次重试。

如何判断卡在哪里：

- “测试连接”为绿色：API Key、厂商接口和模型基本可达。
- “API 连接超时”：还没稳定连上 API 服务器，优先查网络、代理、防火墙、DNS、接口地址或千问业务空间 ID。
- “API 读取超时”：连接已建立，请求已发出，但平台没在超时时间内返回完整结果，更可能是模型处理、排队、图片过大或模型响应慢。
- HTTP `401/403/400`：平台已经明确返回错误，分别检查鉴权、权限、模型名、请求参数。

如果看到“API 读取超时”，可以把 GUI 中 `API 超时(秒)` 改为 `180` 或 `300`，或换一个更快的模型重试。

## 真实 API 接入

项目已经内置 `RealVLMClient`，支持 OpenAI-compatible `/chat/completions` 视觉模型接口。GUI 已预设常用厂商的接口地址，普通用户不需要填写 base URL。

GUI 中需要填写：

- `模型厂商`：从下拉框选择。
- `模型`：从下拉框选择。
- `API Key`：填写对应厂商的密钥。

当前内置厂商：

- `千问 / 阿里云百炼`：推荐，使用 DashScope API Key。当前预设默认模型为 `qwen3.7-plus`。
- `智谱 GLM`：选择带 `v` 的视觉模型。
- `Kimi / Moonshot`：选择 `vision-preview` 视觉模型。
- `DeepSeek`：已列入厂商列表，但官方 API 当前主要是文本模型，不适合本项目的图片 bbox 分割；实际分割建议选千问、智谱或 Kimi。

也可以通过环境变量配置：

```bash
set VLM_PROVIDER=qwen
set VLM_API_KEY=你的APIKey
set VLM_API_WORKSPACE_ID=你的百炼业务空间ID
```

PowerShell 示例：

```powershell
$env:VLM_PROVIDER="qwen"
$env:VLM_API_KEY="你的APIKey"
$env:VLM_API_WORKSPACE_ID="你的百炼业务空间ID"
```

也可以使用厂商专用环境变量：

- `DASHSCOPE_API_KEY`
- `ZHIPUAI_API_KEY`
- `MOONSHOT_API_KEY`
- `DEEPSEEK_API_KEY`

注意：模型 API 只负责返回每个字符的 bbox。最终字符图仍然由程序从原图直接裁剪，不会使用 API 返回的图片，也不会做二值化、resize、重绘等处理。

## 命令行使用

从项目根目录运行：

```bash
python -m src.cli --image data/input/5.bmp --target "202803192028031912345ABC" --output data/output/run_001 --model-json data/input/test_segments.json
```

调用真实 API：

```bash
python -m src.cli --image data/input/5.bmp --target "202803192028031912345ABC" --output data/output/run_001 --provider qwen --api-key "你的APIKey"
```

参数：

- `--image`：输入图片路径，必填。
- `--target`：已知目标字符串，必填。
- `--output`：输出目录，必填。
- `--model-json`：可选，使用 MockVLMClient 从测试 bbox JSON 文件读取 bbox；传入后不会调用真实 API。
- `--api-key`：可选，真实视觉 API Key，也可用 `VLM_API_KEY`。
- `--provider`：可选，`qwen`、`zhipu`、`kimi`、`deepseek`，默认 `qwen`。
- `--api-model`：可选，视觉模型名；不填时使用所选厂商的默认推荐模型。
- `--api-workspace-id`：可选，阿里云百炼业务空间 ID，用于拼接官方推荐的地域业务空间域名。
- `--api-base-url`：高级选项，可选，用于覆盖内置接口地址。
- `--api-timeout`：可选，API 超时时间，命令行默认 60 秒；GUI 默认 180 秒。
- `--max-retry`：可选，真实视觉模型调用失败或校验失败时的额外重试次数，默认 2；总调用次数最多为 `1 + max_retry`。

如果不传 `--model-json`，程序会调用 `RealVLMClient`。如果 API Key 缺失，或选择了不支持图片输入的厂商，会给出明确错误。

## MockVLMClient 测试

Mock JSON 可以是完整模型输出：

```json
{
  "image_width": 640,
  "image_height": 120,
  "target_text": "ABC123",
  "char_count": 6,
  "segments": [
    {
      "index": 1,
      "char": "A",
      "bbox": [10, 20, 35, 65],
      "confidence": 0.98
    }
  ],
  "notes": ""
}
```

也可以直接是 `segments` 数组。程序会自动补齐图片宽高、目标字符串和字符数量。

内置示例：

```bash
cd industrial_char_segmenter
python examples/example_run.py
```

示例会生成一张测试图片、一个 Mock bbox JSON，并完整跑通输出流程。

## 输出目录

运行后输出：

```text
data/output/run_001/
├── chars_original_crop/
│   ├── 001_2.png
│   ├── 002_0.png
│   └── ...
├── segments_model.json
├── segments_final.json
├── preview_boxes.png
└── run_report.json
```

说明：

- `chars_original_crop/`：每个字符的原图直接裁剪结果，可用于后续 OpenCV 模板匹配。
- `segments_model.json`：视觉模型或 Mock JSON 输出的原始 bbox。
- `segments_final.json`：校验、轻微修正后的 bbox，并记录每个字符的输出文件名。
- `preview_boxes.png`：画框预览图，只用于人工检查，不能作为模板库。
- `run_report.json`：运行状态、错误、警告、校验结果和输出路径。

## 坐标规则

统一使用 OpenCV/Python 切片坐标：

```text
bbox = [x1, y1, x2, y2]
```

- `x1, y1` 是左上角坐标，包含。
- `x2, y2` 是右下角外侧坐标，不包含。
- 裁剪时使用 `image[y1:y2, x1:x2]`。

不要传入“右下角包含”的坐标，否则会少裁或多裁像素。

## 校验和修正策略

`src/validator.py` 会检查：

- 是否存在 `segments` 字段。
- `segments` 数量是否等于 `len(target_text)`。
- `index` 是否从 1 开始连续。
- `char` 是否与目标字符串对应位置一致。
- `bbox` 是否为长度 4 的数字数组。
- 坐标是否越界。
- `x2 > x1`、`y2 > y1`。
- bbox 是否过小。
- 相邻 bbox 是否严重重叠。
- 顺序是否明显异常。
- 是否触碰图像边界，提示可能切边。

轻微问题会自动修正，例如 float 坐标转 int、轻微越界 clamp、index 或 char 字段修正。严重问题会写入错误并使本次运行失败，不会默默产出错误模板。

## API 返回格式

真实模型应只返回 JSON，不要解释文字。结构如下：

```json
{
  "image_width": 640,
  "image_height": 120,
  "target_text": "ABC123",
  "char_count": 6,
  "segments": [
    {
      "index": 1,
      "char": "A",
      "bbox": [10, 20, 35, 65],
      "confidence": 0.98
    }
  ],
  "notes": ""
}
```

如果模型把 JSON 包在 Markdown 的 json 代码块里，程序也会尝试自动提取。

## 注意事项

- 输出字符数量必须严格等于目标字符串长度。
- 多行字符按从上到下、每行从左到右排序。
- 不要把分隔符、背景、噪声、边框当作字符输出。
- 字符粘连时，bbox 应根据目标字符串长度和字符宽度趋势切分。
- bbox 可以保留 1 到 2 像素安全边距，避免切掉字符边缘。
- 特殊字符文件名会被转换成 Unicode token，避免 Windows 非法文件名。

## 常见问题

### 为什么不能二值化或 resize 最终字符图？

后续要用 OpenCV 做模板匹配，模板必须和真实采集图像的像素分布一致。任何二值化、resize、去噪、锐化或重绘都会改变模板特征，降低匹配可靠性。

### preview_boxes.png 能不能当模板？

不能。它是带红框和文字标注的人工检查图，只能看 bbox 是否合理。

### bbox 越界怎么办？

轻微越界会被 clamp 到原图范围内，并写入 `run_report.json` 的 `autocorrections`。如果修正后 bbox 没有有效宽高，则运行失败。

### 模型返回 Markdown json 代码块怎么办？

`extract_json_from_text()` 支持从 Markdown JSON 代码块和带解释文本的模型回复中提取 JSON。
