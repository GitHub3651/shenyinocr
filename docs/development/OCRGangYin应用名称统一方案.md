# OCRGangYin 应用名称统一方案

## 状态

- 状态：代码实施完成，待用户验证。
- 权威范围：主程序 EXE 文件名、Windows 产品信息、主窗口标题、Release 部署脚本和现行用户、发布说明。
- 当前进度：已使用 ASCII 内部目标避免 qmake、nmake 和 Qt Creator 乱码，并为部署脚本加入 UTF-8 BOM，保证 Windows PowerShell 5.1 正确读取中文交付文件名；等待 Release Rebuild 和运行验证。

## 目标与范围

- qmake 使用 ASCII 内部目标 `ShengYin.exe`，避免 Makefile、资源文件和 Qt Creator 运行路径乱码。
- Windows 产品名称和文件描述统一为“晟崟AI视觉检测软件”。
- 主窗口标题统一为“晟崟AI视觉检测软件”。
- Release 部署脚本排除发布源中的两个 EXE 名称，对刚链接的 `ShengYin.exe` 执行运行库部署，并复制出最终交付文件 `晟崟AI视觉检测软件.exe`。
- `deploy_runtime.ps1` 使用带 BOM 的 UTF-8 编码，兼容当前 `powershell.exe` 5.1 调用。
- 现行用户说明标题和发布说明统一使用新名称。

## 保持项

- `dist/ShengYin` 发布目录不改名。
- `%APPDATA%/ShengYin` 配置目录不迁移。
- `ShengYin_*.log` 日志文件名前缀不变。
- `build/release/ShengYin.exe` 作为 qmake、nmake 和 Qt Creator 的内部构建运行目标保留；正式交付目录只保留中文名称 EXE。
- 公司名称、版本号、运行行为、算法、设备、模板、设置和许可证逻辑不变。

## 实施文件

- `app/AutoOCRproject.pro`
- `app/system_support/deployment/deploy_runtime.ps1`
- `app/ui/main_window/main_window.ui`
- `docs/release-package.md`
- `docs/使用文档/客户使用说明书.md`

## 验证门禁

1. 静态检查中文名称不再作为 qmake `TARGET`，部署脚本同时排除发布源中的内部目标和中文交付文件。
2. 检查 UI XML、部署脚本 UTF-8 BOM、Windows PowerShell 5.1 中文字面量解析和 `git diff --check`。
3. 用户删除本次失败产生的乱码 EXE、乱码 `_resource.rc` 和旧 Makefile 后，在 Qt Creator 中执行 Run qmake 和 Release Rebuild。
4. 确认 `build/release/ShengYin.exe` 与 `build/release/晟崟AI视觉检测软件.exe` 均为本次构建时间，Qt Creator 可以启动内部目标，中文 EXE 可以独立启动且窗口标题正确。
5. 验收后以中文 EXE 更新 `dist/ShengYin/`，删除其中旧的 `ShengYin.exe`。
