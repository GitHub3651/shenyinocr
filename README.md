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

## `app/`核心架构

```text
app/
├─ startup/          程序入口和对象组装
├─ ui/               窗口、页面、对话框和纯显示逻辑
├─ application/      启动检测、设置、模板编辑等用户用例
├─ contracts/        跨层共享的检测模式、相机结果和呈现数据合同
├─ templates/        模板数据、持久化和运行准备
├─ detection/        五种检测算法、定位、预处理和唯一模式装配
├─ runtime/          采集/检测线程、队列、运行状态、结果、PLC和存图（21个代码文件）
├─ devices/          相机、PLC端口和供应商适配器
├─ engines/          OCR、二维码引擎端口和供应商适配器
└─ system_support/   设置、日志、授权、崩溃记录和部署
```

正式检测只需沿下面一条路径阅读：

```text
MainWindow
→ InspectionApplicationService（启动预检和用例编排）
→ CameraSession → CaptureWorker（相机与采集线程）
→ FrameQueue → DetectionWorker（容量1队列与串行检测线程）
→ DetectionRegistry → 当前模式Pipeline（五模式装配与算法）
→ ResultService（每产品唯一结算）
→ PLC / ImageSaveService / ResultPresentationMailbox → UI
```

`templates/` 的核心类型是可保存的 `TemplateSettings`、已校验的 `PreparedTemplate` 和唯一磁盘入口 `TemplateStore`；编辑状态由 `TemplateApplicationService` 持有。`runtime/`不再知道五种具体模式，也不保存模板内部算法字段；它只负责“什么时候采集、检测、结算、停止和进入故障”。

完整的163个工程/代码文件逐项说明、调用链和维护规则见[`docs/development/OCRGangYin开发者代码结构与维护指南.md`](docs/development/OCRGangYin开发者代码结构与维护指南.md)。

## Qt Creator

请在 Qt Creator 中打开 `app/AutoOCRproject.pro`。构建目录统一使用 `build/qt/<kit>/<configuration>`；不要在命令行执行 qmake 或完整构建。

独立发布包尚未生成：正式发布前必须先确定 OCR 模型使用 V3 还是 V5，并完成五种检测模式的现场回归。
