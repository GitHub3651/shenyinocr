# 独立发布包约定

正式交付目录固定为 `dist/ShengYin/`；`build/` 只用于 Qt Creator 编译，`archive/legacy-output-20260807/` 保留迁移前运行目录，不作为正式发布来源。

## 当前 V3 发布基线

- OCR 固定为 PP-OCRv3：英文检测、英文识别、方向分类器和 `en_dict.txt`。
- 独立包位于 `dist/ShengYin/`，包含 `ShengYin.exe`、唯一 `config1.txt`、V3 模型、授权、二维码 DLL、Qt/Paddle/OpenCV/海康/Snap7 运行库及 `manifest.sha256`。
- 程序从 exe 同级加载 `config1.txt`；配置中的相对模型和词典路径以该配置文件所在目录为准。
- 2026-08-07 已从独立包直接启动，并完成五种检测模式、模板读写、相机、PLC、软硬触发、授权、二维码 DLL 和多相机入口的人工回归。
- PP-OCRv5 服务器模型保留在 `archive/model-upgrade-v5-20260807/`，不参与当前编译、运行或发布。

## 使用和校验

1. 直接运行 `dist/ShengYin/ShengYin.exe`，不通过 Qt Creator，也不运行 `build/` 中的 exe。
2. `manifest.sha256` 记录包内每个运行文件（不含清单自身）的 SHA-256，可用于拷贝后的完整性检查。
3. `output/` 已归档；如需回溯旧运行环境，使用 `archive/legacy-output-20260807/`，不要将其重新作为发布目录。
4. 现场模板和 AppData 配置保持原有位置，不随发布包迁移。
