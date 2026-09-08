# OCRGangYin 资源目录整理方案

## 1. 文档状态

- 文档日期：2026-09-05。
- 最新决定：用户已确认并授权按本方案整理 `app/resource/`。
- 当前状态：代码已实施，Agent 静态门禁通过，待用户 Qt Creator 验证。
- 权威范围：PNG 与 SVG 的实体目录、QRC 登记及直接资源路径。
- 保持项：视觉内容、业务逻辑、控件结构、翻译、ICO 和正式 QSS 内容不变。

## 2. 目标

1. PNG 统一归入 `app/resource/png/`。
2. 唯一在用的 `sample1.png` 直接放在 `png/`，其余十三个 PNG 放入 `png/unused/`。
3. 已使用 SVG 按主控操作、右侧导航和画布背景归类，未使用 SVG 放入 `svg/unused/`。
4. 未使用目录不登记到 `image.qrc`，不编译进程序。
5. 所有生产引用直接切换到新路径，不保留别名、复制文件、回退或兼容层。

## 3. 实施前盘点

| 类型 | 总数 | 实际使用 | 未使用 |
|---|---:|---:|---:|
| PNG | 14 | 1 | 13 |
| SVG | 19 | 14 | 5 |

`sample1.png` 由字符模板编辑器通过 QRC 直接加载。八个主控 SVG 和五个导航 SVG 由 `main_window.ui` 使用，画布网格由 `app_theme.qss` 使用。其余文件没有生产代码、`.ui` 或 QSS 引用。

## 4. 目标目录

```text
app/resource/
├─ png/
│  ├─ sample1.png
│  └─ unused/
│     ├─ 2.png、4.png、5.png、6.png、8.png、10.png、12.png
│     └─ H.png、background.png、delete.png、open.png、sample2.png、setting.png
├─ svg/
│  ├─ action/
│  │  └─ action_*.svg
│  ├─ navigation/
│  │  └─ nav_*.svg
│  ├─ canvas/
│  │  └─ canvas_background_grid.svg
│  └─ unused/
│     └─ camera.svg、camera-off.svg、file-upload.svg、plug-connected-x.svg、trash.svg
├─ qss/
│  └─ app_theme.qss
├─ image.qrc、README.md
├─ Img_Icon_qr.ico、Img_Icon_txt.ico、sy.ico
└─ Translate_CN.ts/.qm、Translate_EN.ts/.qm
```

## 5. 修改范围

| 文件 | 修改内容 |
|---|---|
| `app/resource/png/**` | 移入十四个 PNG，区分当前使用与未使用 |
| `app/resource/svg/**` | 按 `action/navigation/canvas/unused` 移入十九个 SVG |
| `app/resource/image.qrc` | 只登记在用 PNG、SVG、QSS、翻译和既有 ICO；移除十一项未使用 PNG 登记 |
| `app/ui/main_window/main_window.ui` | 更新八个主控图标和五个导航图标路径 |
| `app/resource/qss/app_theme.qss` | 更新画布网格路径 |
| `app/ui/main_window/template/character_editor/character_template_editor_dialog.cpp` | 只更新 `sample1.png` 的静态 QRC 路径 |
| `app/resource/README.md`、`app/ui/README.md` | 更新真实目录与调用说明 |
| `docs/development/OCRGangYin工业视觉风格UI改造方案.md` | 同步仍待用户验证的 SVG 实体路径 |

除表中路径外不修改生产文件。`character_template_editor_dialog.cpp` 不新增分支、检查、回退、成员或逻辑，只替换一个资源字符串。

## 6. 实施规则

1. 文件只移动，不改写图片内容或文件名。
2. `png/unused/` 和 `svg/unused/` 不进入 QRC。
3. 不删除本轮归档的 PNG 或 SVG。
4. 不调整 `Img_Icon_qr.ico`、`Img_Icon_txt.ico`、`sy.ico`、翻译文件或 QSS 目录层级。
5. 不新增资源管理器、路径包装、生成脚本、QRC alias 或运行时探测。

## 7. 静态门禁

1. 十四个 PNG 和十九个 SVG 全部位于目标目录，源位置不再残留同名文件。
2. `image.qrc` 中的每个 `<file>` 都存在，且每项只登记一次。
3. `png/unused/` 与 `svg/unused/` 在 `image.qrc` 中零登记。
4. 生产 `.cpp/.h/.ui/.qss` 中旧 QRC 路径零引用。
5. 全部十九个 SVG、`image.qrc` 和九个 `.ui` 均可作为 XML 解析。
6. 八个主控图标、五个导航图标、画布网格和 `sample1.png` 的生产路径均在 QRC 中登记。
7. 图片移动前后的 Git blob 内容一致。
8. `git diff --check` 通过。

## 8. 用户验证

1. 在 Qt Creator 执行 Run qmake 和 Rebuild。
2. 打开字符模板编辑器，确认内置示例图正常显示。
3. 检查八个主控图标和五个右侧导航图标无缺失。
4. 检查主图像画布的深色网格背景正常显示。

用户完成以上验证后，本方案才能标记为完成。
