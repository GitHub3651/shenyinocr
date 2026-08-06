# 独立发布包约定

正式交付目录固定为 `dist/ShengYin/`，不得以 `build/` 或历史 `output/` 作为发布来源。当前只建立约定，不复制可执行文件、模型或 DLL。

发布前需要：

1. 先确定 OCR 模型版本（V3 或 V5），并完成对应模型组合的现场验证。
2. 在 `dist/ShengYin/` 放入 `ShengYin.exe`、唯一运行配置、唯一模型目录、`BarcodeDecoder.dll`、`license.ini`、Qt 插件及 Paddle/OpenCV/海康/Snap7 所需运行库。
3. 生成包含每个 DLL、模型和配置文件的路径、大小、SHA-256 的发布清单；发布前校验缺失和重复项。
4. 从 `dist/ShengYin/` 直接启动，确认不依赖 Qt Creator 工作目录、源目录或 `output/`。
5. 完成模板匹配、字库匹配、深度模型、纸巾检测、二维码+三期，以及相机、PLC、软硬触发、授权、二维码 DLL 和多相机入口的人工回归。

在以上条件通过前，`output/` 仅作为历史运行/构建目录保留，不能删除或作为正式交付依据。
