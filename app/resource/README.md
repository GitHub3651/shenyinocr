# resource：Qt 内置视觉与翻译资源

## 一句话理解

`resource` 保存随程序发布的界面资源。`image.qrc` 是唯一 Qt 资源清单，程序通过 `:/...` 路径访问资源，不依赖用户机器上的绝对路径。

## 当前结构

```text
resource/
├─ image.qrc                 Qt 资源唯一清单
├─ qss/
│  └─ app_theme.qss          唯一正式工业视觉主题
├─ png/
│  ├─ sample1.png            字符模板编辑器示例图
│  └─ unused/                未登记到 QRC 的历史 PNG
├─ svg/
│  ├─ navigation/            五个左侧导航图标
│  ├─ canvas/                主图像画布背景
│  ├─ camera_on.svg          打开相机
│  ├─ camera_off.svg         关闭相机
│  ├─ start.svg、stop.svg    识别启停和预览停止
│  ├─ preview.svg            主工具栏预览画面
│  ├─ template_*.svg         模板选择、制作、保存和退出
│  ├─ character_seg.svg      分割字符
│  ├─ toggle_off.svg         硬触发关闭状态
│  ├─ toggle_on.svg          硬触发开启状态
│  ├─ verdict_correct.svg    OK 判定
│  ├─ verdict_wrong.svg      NG 判定
│  └─ unused/                未登记到 QRC 的旧 SVG
├─ Img_Icon_qr.ico、Img_Icon_txt.ico
├─ sy.ico                    Windows 应用图标
├─ Translate_CN.ts/.qm
├─ Translate_EN.ts/.qm
└─ README.md
```

目录中的文件只有登记到 `image.qrc` 后才能通过 Qt 资源路径访问；`sy.ico` 由 qmake 的 `RC_ICONS` 使用，翻译源 `.ts` 由 Qt 翻译工具生成 `.qm`。

## 正式样式

`qss/app_theme.qss` 是程序唯一加载的 QSS，资源路径为 `:/qss/app_theme.qss`。它统一维护：

- 浅灰工业设备外壳、面板、抽屉和导航层级；
- `Microsoft YaHei UI` 字体栈，16/18/20/26px 四档字号和 400/600 两档字重；
- 普通黑色、禁用灰色、主操作蓝以及成功、警告、危险、反白七种通用文字角色色；
- 边框、圆角、内边距和控件通用高度；
- Normal、Hover、Pressed、Focus、Disabled、ReadOnly 和 Error；
- `uiRole`、`uiState`、`hasError`、`connectionState` 的视觉表现；
- 主图像画布的深色背景和静态网格。

控件样式不得复制到 `.ui` 或业务 C++，也不建立主题切换、兼容主题或运行时资源生成路径。

## PNG 资源

`png/sample1.png` 是字符模板编辑器当前使用的内置示例图。其余 PNG 集中保存在 `png/unused/`，仅供历史追溯，不登记到 `image.qrc`，也不编译进程序。

## SVG 资源

### 界面操作图标

以下图标由项目直接定义并以静态 SVG 交付，不依赖第三方路径或许可：

| 文件 | 用途 |
|---|---|
| `svg/camera_on.svg` | 打开相机 |
| `svg/camera_off.svg` | 关闭相机 |
| `svg/template_select.svg` | 选择模板 |
| `svg/template_make.svg` | 制作模板 |
| `svg/template_save.svg` | 保存模板 |
| `svg/character_seg.svg` | 分割字符 |
| `svg/template_exit.svg` | 退出制作 |
| `svg/start.svg` | 启动识别 |
| `svg/stop.svg` | 停止识别和停止实时预览 |
| `svg/preview.svg` | 主工具栏开始或停止实时预览 |
| `svg/verdict_correct.svg` | OK 判定 |
| `svg/verdict_wrong.svg` | NG 判定 |
| `svg/unused/trash.svg` | 删除字符模板预览中的字符框 |

`camera_*.svg`、`start.svg`、`stop.svg` 和 `preview.svg` 由主工具栏按钮使用；`template_*.svg` 和 `character_seg.svg` 由参数设定页“模板管理”分组使用。`template_exit.svg` 只服务“退出制作”，`stop.svg` 继续服务停止识别和停止实时预览。

`svg/canvas/canvas_background_grid.svg` 是 32×32 的透明弱网格，只供 `InspectionImageCanvas` 通过正式 QSS 平铺使用。

`svg/toggle_off.svg` 和 `svg/toggle_on.svg` 是 30×16 的硬触发关闭、开启状态，只供工具栏唯一硬触发 `QCheckBox` 通过正式 QSS 切换使用。

### 左侧导航图标

`nav_*.svg` 来自 [Tabler Icons](https://tabler.io/icons)，采用 MIT License：

| 文件 | 用途 |
|---|---|
| `svg/navigation/nav_inspection.svg` | 检测信息 |
| `svg/navigation/nav_parameters.svg` | 参数设定 |
| `svg/navigation/nav_image.svg` | 图像设置 |
| `svg/navigation/nav_plc.svg` | PLC 通讯 |
| `svg/navigation/nav_software.svg` | 软件设置 |

`svg/unused/` 中的 `trash.svg` 由字符模板预览删除按钮使用并登记到 `image.qrc`。其余四个旧 SVG：`camera.svg`、`camera-off.svg`、`file-upload.svg` 和 `plug-connected-x.svg` 不登记到 `image.qrc`。

## 资源调用边界

```text
ApplicationStartup
├─ 加载翻译 .qm
└─ 在任何启动消息框之前加载 :/qss/app_theme.qss

main_window.ui
├─ 使用主工具栏相机、识别和预览图标
└─ 使用 :/svg/navigation/nav_*.svg

detection_settings_page.ui
└─ 使用“模板管理”分组的模板选择、制作、保存、分割字符和退出图标

InspectionPage
├─ 在现有状态更新处切换相机和检测图标
└─ 根据正式检测判定显示 :/svg/verdict_correct.svg 或 :/svg/verdict_wrong.svg

app_theme.qss
├─ 使用 :/svg/canvas/canvas_background_grid.svg
└─ 使用 :/svg/toggle_off.svg 与 :/svg/toggle_on.svg

CharacterTemplateEditorDialog
└─ 加载 :/png/sample1.png
```

Detection、Runtime 和 Templates 不依赖本目录。检测模板、字符模板、产品图片、用户设置、日志和检测结果属于外部运行数据，不放入 Qt 资源。

## 修改规则

1. 只添加所有安装实例共享的内置 UI 或翻译资源。
2. 新增、删除或改名时同步修改 `image.qrc` 和本 README。
3. `png/unused/` 和 `svg/unused/` 不登记到 `image.qrc`。
4. QSS 的 `url(...)` 和 `.ui` 的图标只使用 QRC 路径。
5. 正式控件样式只修改 `app_theme.qss`，唯一加载入口保持在 `ApplicationStartup`。
6. 主控 SVG 直接维护实体文件，不增加生成脚本、PNG 副本、别名或运行时回退。
7. 修改翻译时编辑 `.ts`，再由用户使用 Qt 工具生成 `.qm`。
8. 资源变更后由用户执行 Run qmake、Rebuild，并检查 Debug/Release 显示。
