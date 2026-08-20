# ui：界面层

本目录负责控件、对话框、页面交互和结果展示。它只调用 Application Service，不直接访问 Runtime、设备 SDK、设置 Store 或模板磁盘。

## 文件树

```text
ui/
├─ main_window.ui
├─ main_window.h/.cpp                    组合根、绑定和薄转发
├─ main_window_inspection.cpp            检测/相机/PLC槽和操作状态
├─ main_window_settings.cpp              设置、模式显隐和参数应用
├─ pages/
│  ├─ inspection_page.h/.cpp
│  ├─ machine_settings_page.h/.cpp
│  ├─ template_editor_page.h/.cpp        模板选择/编辑/取景集中页面
│  └─ template_editor_support.h/.cpp
├─ dialogs/
│  ├─ template_selection_dialog.h/.cpp   预勾选、单/多选、排序
│  └─ character_template_editor_dialog.h/.cpp
├─ controllers/
│  ├─ operation_ui_policy.h/.cpp
│  └─ settings_edit_state.h/.cpp
├─ presenters/inspection_fault_presenter.h/.cpp
└─ widgets/image_label.h/.cpp
```

## 模板 UI

```text
选择模板
 → TemplateSelectionDialog 读取当前正式路径并预勾选
 → 增加/取消/排序只改对话框草稿
 → 确认时 saveTemplatePaths，一次成功后关闭

当前编辑模板下拉框
 → 项内部保存绝对路径
 → TemplateApplicationService::beginEdit(path)
 → 保存进入唯一 TemplateStore::save()

移除模板
 → 复制当前路径列表并移除当前项
 → saveTemplatePaths
 → 不读取、不移动、不删除模板文件夹
```

单模板模式最多一项；多模板模式可排序多项；纸巾模式隐藏选择、名称、新建、保存、字符和 ROI 制作控件，只显示粗糙度阈值。

## 操作状态

`OperationUiPolicy` 根据 `CameraClosed/CameraReady/Detecting/Stopping/Fault/TemplatePreviewing/TemplateFrozen` 统一计算权限。UI 禁用用于明确状态展示；真正影响设备和生产运行的操作仍由 Application/Runtime 检查。

## 统一样式

唯一正式样式文件是`app/resource/qss/app_theme.qss`，运行时只从`:/qss/app_theme.qss`加载。普通控件按控件类型自动继承样式，不在`.ui`或业务C++中填写完整`styleSheet`。当前主题按用户确认恢复旧版视觉习惯：全局默认使用16px粗体，重要状态和结果仍可在正式QSS中使用更大的字号或更高字重。

模板制作向导的背景、边框、字体、字号和颜色只允许在`app_theme.qss`的`frame_templateGuide/label_templateGuideTitle/label_templateGuideBody`选择器中维护。C++只负责把提示栏放在图像上方、按文字收缩，以及根据检测模式和框选事件更新标题、步骤、错误和点数文案。

只允许两个按钮角色：

- `uiRole=primary`：当前页面的主要保存或确认动作。
- `uiRole=danger`：清空、清零等危险动作。

运行时视觉只使用三个属性：

- `uiState`：运行、停止、警告、故障和模板取景状态。
- `verdict`：`idle/ok/ng/fault`检测判定。
- `hasError`：输入校验错误。

C++只设置状态属性，并在属性变化后执行`unpolish/polish/update`；颜色、边框、字体和禁用视觉全部由`app_theme.qss`决定。确实需要单控件差异时，只在`app_theme.qss`末尾使用对象名选择器，不在控件创建点恢复样式字符串。

## 维护规则

- `.ui` 对象名、自动槽声明和实现必须同步。
- 当前编辑模板是临时 UI 状态，不写入设置。
- 无效已选路径要保留显示并允许移除，不能静默切换或删除。
- 新增页面行为优先放现有页面对象；不要把 MainWindow 再拆成大量一函数文件。
- 所有用户文字直接使用 UTF-8 中文，不写人为 Unicode 转义。
- 新增普通控件不要设置`styleSheet`；先使用默认样式，再考虑现有角色，最后才考虑对象名覆盖。
- `setEnabled()`和禁用原因 Tooltip 属于交互规则，不能因为样式重构而删除。
- 固定控件说明直接使用 Qt 原生`setToolTip()`，列表项和下拉项的完整路径使用`Qt::ToolTipRole`；不要用`eventFilter`、定时器或`QToolTip::showText()`重复实现悬停显示。
- 运行状态导致的禁用原因统一通过`applyOperationUiAccess()`覆盖，控件重新启用后恢复原 Tooltip；不为 Tooltip 新增 Manager、配置文件或字符串注册层。
