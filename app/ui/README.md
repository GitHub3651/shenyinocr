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

## 维护规则

- `.ui` 对象名、自动槽声明和实现必须同步。
- 当前编辑模板是临时 UI 状态，不写入设置。
- 无效已选路径要保留显示并允许移除，不能静默切换或删除。
- 新增页面行为优先放现有页面对象；不要把 MainWindow 再拆成大量一函数文件。
- 所有用户文字直接使用 UTF-8 中文，不写人为 Unicode 转义。
