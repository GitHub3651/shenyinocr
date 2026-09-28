# OCRGangYin 应用名称统一方案

## 状态

- 状态：代码实施完成，待用户验证。
- 权威范围：主程序 EXE 文件名、Windows 产品信息、主窗口标题、Release 部署脚本和现行用户、发布说明。
- 当前进度：构建与交付统一使用 ASCII 文件名 `ShengYin.exe`，Windows 产品名称和主窗口标题保持“晟崟AI视觉检测软件”；等待 Release Rebuild 和运行验证。

## 目标与范围

- qmake、Qt Creator 和正式交付统一使用 `ShengYin.exe`，避免 Makefile、资源文件和运行路径乱码。
- Windows 产品名称和文件描述统一为“晟崟AI视觉检测软件”。
- 主窗口标题统一为“晟崟AI视觉检测软件”。
- Release 部署脚本排除发布源中的两个旧 EXE 文件，对刚链接的 `ShengYin.exe` 执行运行库部署，不复制其他名称的 EXE。
- `deploy_runtime.ps1` 使用带 BOM 的 UTF-8 编码，兼容当前 `powershell.exe` 5.1 调用。
- 现行用户说明标题和发布说明统一使用新名称。

## 保持项

- `dist/ShengYin` 发布目录不改名。
- `%APPDATA%/ShengYin` 配置目录不迁移。
- `ShengYin_*.log` 日志文件名前缀不变。
- `build/release/ShengYin.exe` 作为 qmake、nmake、Qt Creator 和正式交付使用的唯一 EXE 文件。
- 公司名称、版本号、运行行为、算法、设备、模板、设置和许可证逻辑不变。

## 实施文件

- `app/AutoOCRproject.pro`
- `app/system_support/deployment/deploy_runtime.ps1`
- `app/ui/main_window/main_window.ui`
- `docs/release-package.md`
- `docs/使用文档/客户使用说明书.md`

## 验证门禁

1. 静态检查 qmake `TARGET` 和正式交付文件均为 `ShengYin.exe`，部署脚本不会复制出其他名称的 EXE。
2. 检查 UI XML、部署脚本 UTF-8 BOM、Windows PowerShell 5.1 中文字面量解析和 `git diff --check`。
3. 用户删除本次失败产生的乱码 EXE、乱码 `_resource.rc` 和旧 Makefile 后，在 Qt Creator 中执行 Run qmake 和 Release Rebuild。
4. 确认 `build/release/ShengYin.exe` 为本次构建时间，Qt Creator 可以启动该文件且窗口标题正确，构建输出目录不生成其他名称的 EXE。
5. 验收后以 `ShengYin.exe` 更新 `dist/ShengYin/`，删除其中旧的中文名称 EXE。
