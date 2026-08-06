# 第三方依赖清单

本目录仅追踪本清单；实际二进制包、模型和 DLL 不纳入 Git。恢复开发环境时，应核对来源、版本、目标目录及 SHA-256。未完成的 SHA-256 项必须在下一次依赖包交付时补齐，不能用“最新版”替代。

| 依赖 | 当前用途 | 目标目录 | 版本 / 架构 | 来源与校验 |
| --- | --- | --- | --- | --- |
| Paddle Inference | OCR 推理链接与运行时 | `third_party/paddle_inference_install_dir` | 当前 x64 Windows 包 | 记录原始压缩包 URL、版本与 SHA-256：待补齐 |
| PaddleOCR 源码 | Qt 主程序内 OCR 预处理 / 后处理 | `app/PaddleOCR` | 当前项目版本 | 由 Git 追踪；不属于二进制依赖 |
| OpenCV | 图像处理、模板匹配、跟踪 | `third_party/opencv/x64/vc15` | 3.4.1、MSVC 2017 x64 | 链接名为 `opencv_*341`；原始包 SHA-256：待补齐 |
| 海康 MVS SDK | 相机控制与取图 | `third_party/Libraries/win64`、`third_party/legacy/SDK` | win64 | `MvCameraControl.lib` 与头文件；SDK 安装包 SHA-256：待补齐 |
| Snap7 | PLC 通讯 | `third_party/Libraries/win64` | win64 | `snap7.lib`；原始包 SHA-256：待补齐 |
| 条码 DLL 构建依赖 | 独立二维码 / Data Matrix DLL | `tools/barcode_decoder` 构建时下载 | ZXing 3.1.0-rc1、libdmtx 0.7.8 | URL 与 SHA-256 固定在 `CMakeLists.txt` |

## 运行时模型

模型不纳入 Git，也不在本轮选择 OCR V3 或 V5。当前历史运行目录同时存在不同配置：正式 `dist/ShengYin/` 发布前，必须选定一种模型组合，并将模型文件清单、大小和 SHA-256 写入发布清单。

## 遗留依赖

`third_party/legacy/` 保存此次迁移前位于 Qt 源码目录内的 SDK、旧 OpenCV、头文件和库副本。它们先保留，待 Qt Creator 路径验证及五种模式回归通过后，再逐项归档或删除；不得在本次目录迁移中直接删除。
