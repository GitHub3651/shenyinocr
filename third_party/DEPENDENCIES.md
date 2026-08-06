# 第三方依赖清单

本目录仅追踪本清单；实际二进制包、模型和 DLL 不纳入 Git。恢复开发环境时，应核对来源、版本、目标目录、许可证及 SHA-256，不能用“最新版”替代。下表的 SHA-256 是当前目录内核心二进制文件的指纹；重新引入依赖时，还应保留原始压缩包 URL 与其 SHA-256。

| 依赖 | 当前用途 | 目标目录 | 版本 / 架构 | 许可证 | 来源与校验 |
| --- | --- | --- | --- | --- | --- |
| Paddle Inference | OCR 推理链接与运行时 | `third_party/paddle_inference_install_dir` | Git `5c7ad3bcb482dd93ea21b3b66b5a7ff964e0f239`；CPU、MKL/MKLDNN、MSVC 19.0 x64 | Apache-2.0 | `paddle_inference.dll` (51,808,768 B): `E8BF9E6043CE6754CFD516162369A5CBCA22DCF15A0EAC8B41A68B4F14EE6113`; `.lib` (180,906 B): `854BA8471A6869E975B3E75778A5B35EAAB6BD772CE874518A75C62DC88F8673` |
| PaddleOCR 源码 | Qt 主程序内 OCR 预处理 / 后处理 | `app/PaddleOCR` | 当前项目源码 | Apache-2.0 | 由 Git 追踪；不属于二进制依赖 |
| OpenCV | 图像处理、模板匹配、跟踪 | `third_party/opencv/x64/vc15` | 3.4.1、MSVC 2017 x64 | Apache-2.0 | `opencv_world341.lib` (2,319,424 B): `E65B64E924E014AD33DE66046A251F455AD8170318F8AC9D3E9FE57BC6AC1981` |
| 海康 MVS SDK | 相机控制与取图 | `third_party/Libraries/win64`、`third_party/legacy/SDK` | 当前 Win64 SDK（安装包版本待从原始介质补录） | 海康 SDK 随附许可（非开源） | `MvCameraControl.lib` (65,656 B): `F61A4C23CA873FBF1AD2597F78AD85C710983C0FC1B7CAD4A967508DE5F7FE1E` |
| Snap7 | PLC 通讯 | `third_party/Libraries/win64` | 当前 Win64 包（发布版本待从原始介质补录） | LGPL-3.0-or-later | `snap7.lib` (26,188 B): `40FE5BF8639A9F4BBFD1B1EABF3A1F99C2364E9B70DF36053551DAFE2BC871BA` |
| 条码 DLL 构建依赖 | 独立二维码 / Data Matrix DLL | `tools/barcode_decoder` 构建时下载 | ZXing 3.1.0-rc1、libdmtx 0.7.8 | ZXing: Apache-2.0；libdmtx: LGPL-2.1-or-later | URL 与 SHA-256 固定在 `CMakeLists.txt` |

## 运行时模型

模型不纳入 Git，也不在本轮选择 OCR V3 或 V5。当前历史运行目录同时存在不同配置：正式 `dist/ShengYin/` 发布前，必须选定一种模型组合，并将模型文件清单、大小和 SHA-256 写入发布清单。

## 遗留依赖

`third_party/legacy/` 保存此次迁移前位于 Qt 源码目录内的 SDK、旧 OpenCV、头文件和库副本。它们先保留，待 Qt Creator 路径验证及五种模式回归通过后，再逐项归档或删除；不得在本次目录迁移中直接删除。
