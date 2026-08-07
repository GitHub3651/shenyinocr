# 第三方依赖清单

本目录仅追踪本清单；实际二进制包、模型和 DLL 不纳入 Git。恢复开发环境时，应核对来源、版本、目标目录、许可证及 SHA-256，不能用“最新版”替代。下表的 SHA-256 是当前目录内核心二进制文件的指纹；重新引入依赖时，还应保留原始压缩包 URL 与其 SHA-256。

| 依赖 | 当前用途 | 目标目录 | 版本 / 架构 | 许可证 | 来源与校验 |
| --- | --- | --- | --- | --- | --- |
| Paddle Inference | OCR 推理链接与运行时 | `third_party/paddle_inference_install_dir` | Git `5c7ad3bcb482dd93ea21b3b66b5a7ff964e0f239`；CPU、MKL/MKLDNN、MSVC 19.0 x64 | Apache-2.0 | `paddle_inference.dll` (51,808,768 B): `E8BF9E6043CE6754CFD516162369A5CBCA22DCF15A0EAC8B41A68B4F14EE6113`; `.lib` (180,906 B): `854BA8471A6869E975B3E75778A5B35EAAB6BD772CE874518A75C62DC88F8673` |
| PaddleOCR 源码 | Qt 主程序内 OCR 预处理 / 后处理 | `app/PaddleOCR` | 当前项目源码 | Apache-2.0 | 由 Git 追踪；不属于二进制依赖 |
| OpenCV | 图像处理、模板匹配、跟踪 | `third_party/opencv/x64/vc15` | 3.4.1、MSVC 2017 x64 | Apache-2.0 | `opencv_world341.lib` (2,319,424 B): `E65B64E924E014AD33DE66046A251F455AD8170318F8AC9D3E9FE57BC6AC1981` |
| Halcon 头文件 | 为后续机器视觉功能保留接口；当前未接入编译或运行 | `third_party/halcon/include` | 13.0.2 build 5、头文件包 | MVTec 商业许可 | 从原混合头文件包分离；当前 `.pro` 和源码无 Halcon 引用，未包含 Halcon 库或运行时 |
| 海康 MVS SDK | 相机控制与取图 | `third_party/Libraries/win64`、`third_party/legacy/SDK` | 当前 Win64 SDK（安装包版本待从原始介质补录） | 海康 SDK 随附许可（非开源） | `MvCameraControl.lib` (65,656 B): `F61A4C23CA873FBF1AD2597F78AD85C710983C0FC1B7CAD4A967508DE5F7FE1E` |
| Snap7 | PLC 通讯 | `third_party/Libraries/win64` | 当前 Win64 包（发布版本待从原始介质补录） | LGPL-3.0-or-later | `snap7.lib` (26,188 B): `40FE5BF8639A9F4BBFD1B1EABF3A1F99C2364E9B70DF36053551DAFE2BC871BA` |
| 条码 DLL 构建依赖 | 独立二维码 / Data Matrix DLL | `tools/barcode_decoder` 构建时下载 | ZXing 3.1.0-rc1、libdmtx 0.7.8 | ZXing: Apache-2.0；libdmtx: LGPL-2.1-or-later | URL 与 SHA-256 固定在 `CMakeLists.txt` |

## 运行时模型

正式发布基线已锁定为 PP-OCRv3（英文检测、英文识别、方向分类器和英文词典）。模型不纳入 Git；实际发布副本位于 `dist/ShengYin/Model/`，每个文件的 SHA-256 记录在同目录 `manifest.sha256`。PP-OCRv5 和其他历史模型均已归档，不参与当前编译、运行或发布。

## 遗留依赖

`third_party/legacy/SDK/Includes/` 保留当前 Qt 编译所需的海康头文件；当前海康导入库和 Snap7 位于 `third_party/Libraries/win64/`。旧 OpenCV 4.x、旧海康头文件、重复导入库和历史模型已在独立包回归后移至 `archive/legacy-dependencies-20260807/` 与 `archive/legacy-model-cache-20260807/`，不参与当前构建。
