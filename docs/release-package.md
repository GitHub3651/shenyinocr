# 独立发布包约定

正式交付目录固定为 `dist/ShengYin/`；`build/` 只用于 Qt Creator 编译，`archive/legacy-output-20260807/` 只用于历史回溯，不作为正式发布来源。

## 当前发布基线

- 深度 OCR 固定使用 `PP-OCRv6_tiny_det + PP-OCRv6_tiny_rec` 和 PaddleOCR v3.7.0 官方 tiny 字典。
- OCR 配置文件固定为 exe 同级的 `config_ocr.txt`，其中模型和字典相对路径以该配置文件所在目录为基准。
- OCR 资产固定放在 `OCR/PP-OCRv6_tiny/`：DET 和 REC 各包含 `inference.json`、`inference.pdiparams`，字典文件为 `ppocrv6_tiny_dict.txt`。
- Paddle Inference 运行时固定为 3.0.0 Windows x64 CPU 包所需的 `paddle_inference.dll`、`common.dll`、`mklml.dll`、`mkldnn.dll` 和 `libiomp5md.dll`。
- 客户发布包不预置 `license.ini`；主程序首次运行时显示激活申请码，验证激活码后在可执行文件同级生成许可证。
- 二维码 DLL、Qt、OpenCV、海康、Snap7 和 Microsoft x64 运行库继续随独立包提供。

## 使用和部署

1. Release 以 ASCII 内部目标 `ShengYin.exe` 完成链接，`app/system_support/deployment/deploy_runtime.ps1` 从 `dist/ShengYin/` 复制运行资源、检查必需文件，并在构建输出目录生成内容相同的 `晟崟AI视觉检测软件.exe`。
2. `build/release/ShengYin.exe` 只供 qmake、nmake 和 Qt Creator 构建运行；在构建输出目录使用 `晟崟AI视觉检测软件.exe` 完成主程序整体验收。
3. 验收通过后，用本次生成的 `晟崟AI视觉检测软件.exe` 更新 `dist/ShengYin/`，正式交付目录不保留 `ShengYin.exe`。
4. 正式交付时直接运行 `dist/ShengYin/晟崟AI视觉检测软件.exe`，不依赖开发机源码目录。
5. 现场模板和 AppData 配置保持原有位置，不随发布包迁移。
6. `LicenseTool` 仅供供应商内部根据 16 位激活申请码签发 24 位激活码，不放入客户发布包。
