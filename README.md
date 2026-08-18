# OCRGangYin

OCRGangYin 是神银现场 OCR 检测程序。Qt 主程序位于 `app/`；目录结构调整不改变五种检测模式的算法、判定、运行时序或现场配置格式。

## 目录

- `app/`：Qt 5 主程序、UI、资源和参与编译的 PaddleOCR 源码。
- `app/devices/`：相机和PLC硬件端口及供应商实现。
- `app/engines/`：OCR和二维码识别引擎端口及供应商实现。
- `third_party/`：Paddle Inference、OpenCV、海康 SDK、Snap7 等非 Git 二进制依赖；准确版本与校验项见 `third_party/DEPENDENCIES.md`。
- `tools/barcode_decoder/`：二维码 / Data Matrix 解码 DLL 的独立 CMake 源码。
- `tools/license_tool/`：独立 Qt 授权工具。
- `experiments/industrial_char_segmenter/`：不被主程序调用的 Python 字符切分实验。
- `docs/`：开发约束、历史方案和发布说明。
- `build/`、`dist/`、`archive/`：本地构建、交付与过渡归档目录，默认不纳入 Git。

## Qt Creator

请在 Qt Creator 中打开 `app/AutoOCRproject.pro`。构建目录统一使用 `build/qt/<kit>/<configuration>`；不要在命令行执行 qmake 或完整构建。

独立发布包尚未生成：正式发布前必须先确定 OCR 模型使用 V3 还是 V5，并完成五种检测模式的现场回归。
