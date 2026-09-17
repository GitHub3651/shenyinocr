# OCRGangYin 资源目录整理方案

## 1. 文档状态

- 文档日期：2026-09-16。
- 当前状态：代码已实施，Agent 静态门禁通过，待用户 Qt Creator 验证。
- 权威范围：`app/resource/` 下 PNG 与 SVG 的实体目录、QRC 登记及直接资源路径。
- 视觉边界：工具栏图标造型以《OCRGangYin工业视觉风格 UI 改造方案》为准；工具栏布局、状态切换和硬触发开关以《OCRGangYin图像上方常驻工具栏与设备状态重构方案》为准。

## 2. 目标

1. PNG 统一归入 `app/resource/png/`，当前使用的 `sample1.png` 直接位于 `png/`，其余十三个 PNG 位于 `png/unused/`。
2. 工具栏和硬触发开关 SVG 直接位于 `svg/`，导航、画布和未使用资源分别归入 `svg/navigation/`、`svg/canvas/` 和 `svg/unused/`。
3. `svg/unused/` 与 `png/unused/` 不登记到 `image.qrc`，不编译进程序。
4. 所有生产引用直接使用最终 QRC 路径，不保留别名、复制文件、回退或兼容层。

## 3. 当前资源盘点

| 类型 | 总数 | 实际使用 | 未使用 |
|---|---:|---:|---:|
| PNG | 14 | 1 | 13 |
| SVG | 23 | 18 | 5 |

`sample1.png` 由字符模板编辑器通过 QRC 加载。八个工具栏状态图标由 `main_window.ui` 和 `InspectionPage` 使用，两个硬触发开关图标由 `app_theme.qss` 使用，两个判定图标由 `InspectionPage` 使用，五个导航图标由 `main_window.ui` 使用，画布网格由 `app_theme.qss` 使用。

## 4. 目录结构

```text
app/resource/
├─ png/
│  ├─ sample1.png
│  └─ unused/
│     ├─ 2.png、4.png、5.png、6.png、8.png、10.png、12.png
│     └─ H.png、background.png、delete.png、open.png、sample2.png、setting.png
├─ svg/
│  ├─ camera_on.svg、camera_off.svg
│  ├─ template_select.svg、template_make.svg、template_save.svg、character_seg.svg
│  ├─ start.svg、stop.svg、toggle_off.svg、toggle_on.svg
│  ├─ verdict_correct.svg、verdict_wrong.svg
│  ├─ navigation/nav_*.svg
│  ├─ canvas/canvas_background_grid.svg
│  └─ unused/
│     └─ camera.svg、camera-off.svg、file-upload.svg、plug-connected-x.svg、trash.svg
├─ qss/app_theme.qss
├─ image.qrc、README.md
├─ Img_Icon_qr.ico、Img_Icon_txt.ico、sy.ico
└─ Translate_CN.ts/.qm、Translate_EN.ts/.qm
```

## 5. 文件边界

| 文件 | 职责 |
|---|---|
| `app/resource/png/**` | 保存当前使用和未使用的 PNG |
| `app/resource/svg/**` | 保存工具栏、开关、导航、画布和未使用 SVG |
| `app/resource/image.qrc` | 只登记生产使用的 PNG、SVG、QSS、翻译和 ICO |
| `app/ui/main_window/main_window.ui` | 使用工具栏静态图标和五个导航图标 |
| `app/ui/main_window/inspection/inspection_page.cpp` | 根据现有相机和检测状态切换工具栏图标，并根据正式结果显示判定图标 |
| `app/resource/qss/app_theme.qss` | 使用画布网格和硬触发开关 SVG |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.cpp` | 使用 `sample1.png` 的 QRC 路径 |
| `app/resource/README.md`、`app/ui/README.md` | 记录实际目录和调用关系 |

## 6. 实施规则

1. 用户指定的 SVG 作为最终实体资源直接登记和引用。
2. `png/unused/` 和 `svg/unused/` 不进入 QRC。
3. 不调整 ICO、翻译文件或 QSS 目录层级。
4. 不新增资源管理器、路径包装、生成脚本、QRC alias 或运行时探测。

## 7. 静态门禁

1. 十四个 PNG 和二十三个 SVG 全部位于目录结构列明的位置。
2. `image.qrc` 中的每个 `<file>` 都存在，且每项只登记一次。
3. `png/unused/` 与 `svg/unused/` 在 `image.qrc` 中零登记。
4. 生产 `.cpp/.h/.ui/.qss` 和当前生效文档中被替换的 QRC 路径零引用。
5. 全部二十三个 SVG、`image.qrc` 和九个 `.ui` 均可作为 XML 解析。
6. 八个工具栏图标、两个开关图标、两个判定图标、五个导航图标、画布网格和 `sample1.png` 均在 QRC 中唯一登记。
7. `git diff --check` 通过。

## 8. 用户验证

1. 在 Qt Creator 执行 Run qmake 和 Rebuild。
2. 打开字符模板编辑器，确认内置示例图正常显示。
3. 检查工具栏八个状态图标、硬触发开关、两个判定图标和五个导航图标无缺失。
4. 切换相机、检测和硬触发状态，并产生 OK/NG 结果，确认对应 SVG 正常切换。
5. 检查主图像画布的深色网格背景正常显示。

用户完成以上验证后，本方案才能标记为完成。
