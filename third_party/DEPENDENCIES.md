# 第三方依赖清单

本目录仅追踪本清单；实际二进制包、模型和 DLL 不纳入 Git。恢复开发环境时，应核对来源、版本、目标目录和许可证，不能用“最新版”替代。

| 依赖 | 当前用途 | 目标目录 | 版本 / 架构 | 许可证 | 来源 |
| --- | --- | --- | --- | --- | --- |
| Paddle Inference | PP-OCRv6 tiny 的 CPU 推理链接与运行时 | `third_party/paddle_inference_install_dir` | 3.0.0；Git `6ed5dd3833c32c3b21e14b1fb1a71f5a535a0fcc`；Windows x64 CPU；AVX、MKL、oneDNN；MSVC 19.29 / VS2019 | Apache-2.0 | `https://paddle-inference-lib.bj.bcebos.com/3.0.0/cxx_c/Windows/CPU/x86-64_avx-mkl-vs2019/paddle_inference.zip` |
| PaddleOCR vendor 源码 | DET 预处理、DB 后处理、文字框排序与裁剪、REC 预处理和 CTC 解码 | `app/engines/ocr/vendor/paddle` | PaddleOCR v3.7.0 中与当前 DET+REC 链直接相关的实现 | Apache-2.0 | `https://github.com/PaddlePaddle/PaddleOCR/archive/refs/tags/v3.7.0.zip` |
| PP-OCRv6 tiny DET 模型 | 多行文字框检测 | `dist/ShengYin/OCR/PP-OCRv6_tiny/det` | `PP-OCRv6_tiny_det_infer`，Paddle 3.0 推理模型 | Apache-2.0 | `https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv6_tiny_det_infer.tar` |
| PP-OCRv6 tiny REC 模型 | 文字框识别 | `dist/ShengYin/OCR/PP-OCRv6_tiny/rec` | `PP-OCRv6_tiny_rec_infer`，Paddle 3.0 推理模型 | Apache-2.0 | `https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-OCRv6_tiny_rec_infer.tar` |
| PP-OCRv6 tiny 字典 | REC CTC 解码 | `dist/ShengYin/OCR/PP-OCRv6_tiny/ppocrv6_tiny_dict.txt` | PaddleOCR v3.7.0 `ppocr/utils/dict/ppocrv6_tiny_dict.txt` | Apache-2.0 | `https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/v3.7.0/ppocr/utils/dict/ppocrv6_tiny_dict.txt` |
| OpenCV | 图像处理、模板匹配、定位与图像编解码 | `third_party/opencv` | 4.14.0；官方 Windows x64；MSVC 2019 `vc16`，由当前 MSVC 2022 Kit 链接 | Apache-2.0 | `https://github.com/opencv/opencv/releases/tag/4.14.0` |
| Halcon 头文件 | 为后续机器视觉功能保留接口；当前未接入编译或运行 | `third_party/halcon/include` | 13.0.2 build 5、头文件包 | MVTec 商业许可 | 从原混合头文件包分离；当前 `.pro` 和源码无 Halcon 引用，未包含 Halcon 库或运行时 |
| 海康 MVS SDK | 相机控制与取图 | `third_party/hikvision_mvs_sdk/include`、`third_party/hikvision_mvs_sdk/lib/win64` | 当前 Win64 SDK（安装包版本待从原始介质补录） | 海康 SDK 随附许可（非开源） | 当前项目随附的开发包 |
| Snap7 | PLC 通讯 | `app/devices/plc/vendor`、`third_party/Libraries/win64` | 当前 Win64 包（发布版本待从原始介质补录） | LGPL-3.0-or-later | Git 追踪 `snap7.cpp/.h` 包装；主工程链接 `snap7.lib`，运行时部署匹配的 `snap7.dll` |
| 条码 DLL 构建依赖 | 独立二维码 / Data Matrix DLL | `tools/barcode_decoder` 构建时下载 | ZXing 3.1.0-rc1、libdmtx 0.7.8 | ZXing: Apache-2.0；libdmtx: LGPL-2.1-or-later | 下载地址和版本固定在 `CMakeLists.txt` |

## 运行时模型

正式 OCR 运行资产为 PP-OCRv6 tiny DET+REC、同版本官方字典和 Paddle Inference 3.0.0 CPU 运行库，统一位于 `dist/ShengYin` 并由 Release 部署脚本复制到构建输出目录。

## 现有依赖边界

`third_party/hikvision_mvs_sdk/` 是当前 Qt 编译所需的海康头文件和导入库位置。Snap7 不维护第二份实现：主工程与适配器测试均编译 `app/devices/plc/vendor/snap7.cpp`，并链接 `third_party/Libraries/win64/snap7.lib`；运行时只部署匹配的 `snap7.dll`。`third_party/legacy/` 仅保留未参与当前 Qt 工程的旧 SDK 副本与说明。
