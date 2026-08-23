# resource：Qt 内置图片、主题样式和翻译资源

## 一句话理解

`resource` 是纯资源目录，不包含业务代码。`image.qrc` 把图片、QSS 主题和编译后的翻译文件注册为 Qt 资源，程序可通过 `:/...` 路径读取，不依赖用户机器上的绝对文件路径。目录名表示这里包含图片、样式和翻译等多类程序内置资源；`image.qrc` 暂时保留原文件名，以避免没有收益的资源清单改名。

## 当前结构

```text
resource/
├─ image.qrc                 Qt 资源唯一清单
├─ 1.png ... 13.png          主界面已有编号图片
├─ background.png            背景图
├─ open.png / close.png      打开、关闭图标
├─ delete.png                删除图标
├─ setting.png               设置图标
├─ H.png                     现有业务图片
├─ sample1.png / sample2.png 示例图片
├─ Img_Icon_*.ico            camera/database/image/qr/txt 图标
├─ sy.ico                    Windows 应用图标（由 qmake 的 RC_ICONS 使用）
├─ Translate_CN.ts           中文翻译源文件（由 qmake 生成 .qm）
├─ Translate_EN.ts           英文翻译源文件（由 qmake 生成 .qm）
├─ Translate_CN.qm           已编译中文翻译资源
├─ Translate_EN.qm           已编译英文翻译资源
├─ qss/
│  ├─ app_theme.qss          唯一正式应用样式
│  ├─ black.css、blue.css、brown.css 等未启用历史主题
│  ├─ blacksoft.css / blacksoft/...
│  ├─ flatgray.css / flatgray/...
│  ├─ lightblue.css / lightblue/...
│  └─ lightbluepro.css / lightbluepro/...
└─ README.md
```

当前资源目录共 135 个文件（含本 README）。大量文件来自不同 QSS 主题各自配套的箭头、复选框、单选框、树分支和日历图标，不是业务模块，也不会参与检测流程。

## image.qrc 的作用

```text
磁盘资源文件
→ image.qrc 登记
→ qmake/rcc 编译进程序资源系统
→ 程序使用 :/前缀读取
```

`image.qrc` 是资源是否进入程序的唯一清单。目录里存在文件，并不代表程序一定能够通过 Qt 资源路径使用它；还必须在 qrc 中登记。

## 资源分类

### 1. 主界面图片和图标

- `1.png`～`13.png`：历史编号的界面图片；当前没有 `7.png` 和 `9.png`。
- `background.png`：界面背景。
- `open.png`、`close.png`、`delete.png`、`setting.png`：操作图标。
- `Img_Icon_camera3.ico`、`Img_Icon_database.ico`、`Img_Icon_image.ico`、`Img_Icon_qr.ico`、`Img_Icon_txt.ico`：功能入口图标。
- `H.png`、`sample1.png`、`sample2.png`：现有业务/示例资源。

资源名有历史痕迹，但在没有完成引用核对和 UI 回归前不能仅因“不好看”或“看似没用”删除。

### 2. QSS 主题

`qss/app_theme.qss`是程序唯一加载的正式样式，资源路径为`:/qss/app_theme.qss`。其余`qss/*.css`是未启用的历史主题，本轮按“资源不精简”约定继续保留，但生产代码不得扫描、回退或切换到这些文件。

部分历史主题有同名子目录，里面的图片用于：

- 滚动条和菜单箭头；
- TreeView 分支展开/收起；
- Checkbox/Radiobutton 的选中、未选中、禁用、半选状态；
- 日历上月/下月按钮；
- 可增减控件的上下左右图标。

CSS 文件和同名图片目录是一组资源，不能只替换其中一边。

### 3. 翻译资源

- `Translate_CN.qm`：由中文翻译源编译的运行时文件。
- `Translate_EN.qm`：由英文翻译源编译的运行时文件。

可编辑源文件位于 `app/resource/Translate_CN.ts` 和
`app/resource/Translate_EN.ts`。正常流程应修改 `.ts` 后重新生成 `.qm`，不要直接二进制修改 `.qm`。

## 谁使用这些资源

```text
ApplicationStartup
└─ 加载翻译 .qm

MainWindow::initStyle()
└─ 加载并应用唯一正式 QSS

main_window.ui 与 UI 代码
└─ 使用 qrc 中的图标和图片
```

Detection、Runtime、Templates 不应依赖本目录。检测用模板和产品图片属于外部模板资产，不应放进 `app/resource`。

## 允许放什么

- 随程序发布、与所有用户共享的 UI 图标和背景。
- Qt 样式表及其配套图片。
- 由 Qt 翻译流程生成的 `.qm`。
- `image.qrc` 资源登记。

## 禁止放什么

- 外部产品模板、字符模板、定位模板和运行时检测图片。
- 用户设置、日志、检测结果和临时文件。
- C++ 业务代码或 DLL。
- 仅某台设备使用的绝对路径资源。

## 修改资源的完整步骤

1. 确认资源属于程序内置 UI，而不是外部产品模板。
2. 新增或替换文件，保持文件名大小写稳定。
3. 同步修改 `image.qrc`。
4. 核对 QSS 中的 `url(...)` 路径。
5. 正式应用样式只修改`app_theme.qss`；不要在业务C++或`.ui`中复制完整样式。
6. 由用户执行 Run qmake、Rebuild，并检查 Debug/Release 显示。
7. 若是翻译，修改 `.ts` 源并通过 Qt 翻译工具生成 `.qm`。

## 删除资源前的红线

本项目当前约定“代码精简不精简资源”。删除任何资源前必须有用户明确授权，并同时检查：

- `image.qrc`；
- `.ui` 文件；
- QSS 的 `url(...)`；
- C++ 中的 `:/` 路径；
- Release 部署和语言切换。

新增、删除或移动本目录资源时，请同步更新本 README、`image.qrc`、相关 QSS/翻译源和发布说明。
