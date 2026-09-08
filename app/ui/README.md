# ui：界面层

本目录负责主窗口、页面交互、模板弹窗和检测结果呈现。界面层调用现有 Application Service，不直接实现检测算法、设备时序、设置存储或模板磁盘规则。

## 目录结构

```text
ui/
├─ README.md
└─ main_window/
   ├─ main_window.ui
   ├─ main_window.h/.cpp
   ├─ main_window_inspection.cpp
   ├─ main_window_settings.cpp
   ├─ inspection_image_canvas.h/.cpp
   ├─ operation_ui_policy.h/.cpp
   ├─ inspection/
   │  ├─ inspection_info_page.ui
   │  ├─ inspection_page.h/.cpp
   │  └─ inspection_fault_presenter.h/.cpp
   ├─ settings/
   │  ├─ detection_settings_page.ui
   │  ├─ image_settings_page.ui
   │  ├─ plc_settings_page.ui
   │  ├─ software_settings_page.ui
   │  ├─ machine_settings_page.h/.cpp
   │  └─ settings_edit_state.h/.cpp
   └─ template/
      ├─ template_editor_page.h/.cpp
      ├─ selection/
      │  └─ template_selection_dialog.ui/.h/.cpp
      ├─ save/
      │  └─ template_save_dialog.ui/.h/.cpp
      └─ character_editor/
         ├─ character_template_editor_dialog.ui/.h/.cpp
         └─ character_crop_label.h/.cpp
```

## Designer 编辑入口

- 主窗口骨架和主控操作区：`main_window/main_window.ui`。
- 检测信息、参数设定、图像设置、PLC 通讯和软件设置：分别打开对应功能目录中的五个页面 `.ui`。
- 选择模板、保存模板和分割字符模板：分别打开三个模板子目录中的弹窗 `.ui`。

`main_window.ui` 只保留五个空页面根节点。`MainWindow` 依次对主窗口和五个页面生成 Ui 调用 `setupUi()`，再构造 `InspectionPage`、`MachineSettingsPage` 和 `TemplateEditorPage`。主窗口、五个页面和三个弹窗的生成 Ui 均由各自所有者使用 `std::unique_ptr` 管理。

## 页面协作

- `InspectionPage` 直接使用主窗口 Ui 和检测信息页 Ui，更新主图像、判定、运行状态和统计。
- `MachineSettingsPage` 直接使用四个设置页 Ui，管理参数绑定、校验、未应用状态和运行时禁用规则；完整识别模式分组和纸巾粗糙度阈值位于参数设定页，图像设置页只保留图像保存与图像采集处理参数。
- `TemplateEditorPage` 直接使用主窗口 Ui、参数设定页 Ui 和图像设置页 Ui，处理模板选择、制作、保存和参数编辑。
- MainWindow 只保留跨页面或应用级协调；迁出页面且仍由 MainWindow 处理的按钮采用显式函数指针连接。

右侧五个导航按钮切换同一个 `QStackedWidget`。启动时默认显示检测信息；再次点击当前按钮会收起右侧面板；检测启动成功后保持原有开合状态；Fault 首次呈现时自动打开检测信息。

## 模板 UI

```text
选择模板
 → TemplateSelectionDialog 读取当前正式路径
 → 增加和勾选移除只修改对话框内列表
 → 确认时通过 SettingsApplicationService 保存模板路径

当前编辑模板下拉框
 → 项内部保存绝对路径
 → TemplateApplicationService::beginEdit(path)
 → 保存进入唯一 TemplateStore::save()

移除模板
 → 复制当前路径列表并移除当前项
 → 保存新的模板路径列表
 → 不读取、不移动、不删除模板文件夹
```

单模板模式最多一项；多模板模式可包含多项；纸巾模式隐藏模板选择、名称、保存、字符和 ROI 制作控件，只显示粗糙度阈值。

四种模板模式统一使用主窗口的 `InspectionImageCanvas`，纸巾模式禁用模板绘图。字库和 OCR 使用“定位锚点 → 检测多边形”，二维码使用“日期锚点 → 二维码矩形 → 日期多边形”，钢印使用“吸管口锚点 → 钢印多边形 → 日期锚点 → 日期多边形”。钢印的两个锚点和两个多边形分别保存、分别换算，不从日期多边形推导钢印区域，也不创建 OpenCV 原生交互窗口。

`InspectionImageCanvas` 只维护显示坐标、绘制步骤和框线，并向 `TemplateEditorPage` 发出强类型进度事件。模板页负责中文向导、二维码即时校验和保存确认；MainWindow 不转发原始鼠标事件或模板绘图事件。

## 页面所有权与调用流

```text
application_startup
  └─ 创建 MainWindow（传入应用服务）
       ├─ setupUi(main_window.ui)
       ├─ setupUi(五个独立页面 .ui)
       ├─ 创建 InspectionPage
       ├─ 创建 MachineSettingsPage
       └─ 创建 TemplateEditorPage
```

Startup 不知道 Page、生成 Ui、控件地址或页面状态。MainWindow 直接持有自身和五个页面的生成 Ui；三个模板弹窗分别持有自身生成 Ui。逻辑 Page 使用明确的生成 Ui、服务和状态引用，不通过 ViewBindings、回调包装或控件查找访问界面。

## 操作状态

`OperationUiPolicy` 根据 `CameraClosed/CameraReady/Detecting/Stopping/Fault/TemplatePreviewing/TemplateFrozen` 统一计算权限。UI 禁用用于明确状态展示；真正影响设备和生产运行的操作仍由 Application/Runtime 检查。

## 专用控件

`InspectionImageCanvas` 是主窗口唯一检测图像和模板绘制画布，负责图像自适应显示、绘制步骤、显示坐标和强类型绘图事件，不负责模板保存、提示文案或检测算法。

`CharacterCropLabel` 只服务于字符模板编辑弹窗，负责字符框绘制、撤销、清空和坐标换算，并通过 `itemsChanged()` 通知弹窗刷新动态预览。

## 统一样式

唯一正式样式文件是 `app/resource/qss/app_theme.qss`，由 `ApplicationStartup` 在任何启动消息框出现前从 `:/qss/app_theme.qss` 加载一次。主题使用浅灰设备外壳、白灰内容面板、深色图像画布、钢灰边框和低饱和蓝色操作色；普通控件按控件类型自动继承样式，不在 `.ui` 或业务 C++ 中填写完整 `styleSheet`。

主窗口保持 1600×950 设计尺寸，主控区固定 200px，相机/模板/检测三组保持 `1:2:1`，图像/判定保持 `3:1`。最左侧导航栏固定 80px，五个导航按钮固定为 80×80px、使用 28×28 图标并在顶部连续排列，底部伸展项吸收剩余高度；导航按钮无边框和选中蓝条，当前入口由近白背景和蓝色文字表示。导航右侧的五页抽屉与主画面由水平 `QSplitter` 承载，首次使用 360px 抽屉宽度，正常关闭时把完整 Splitter 状态保存到 AppSettings Schema 8。

八个主控按钮直接使用 `:/svg/action/action_*.svg` 静态资源，左侧导航使用 `:/svg/navigation/nav_*.svg`。`InspectionImageCanvas` 使用 `#202830` 底色，并通过正式 QSS 使用 `:/svg/canvas/canvas_background_grid.svg` 作为静态弱网格背景；`CharacterCropLabel` 使用相同的 `#202830` 纯色底，不使用网格。界面层不生成、换色或动态绘制这些资源。

界面文字只使用 16、18、20、26、56px 五个字号和常规 400、加重 600 两档字重。普通界面文字为 16px；18px 只用于模板制作向导标题和正文；识别内容和四个统计值为 20px；其他重要信息为 26px；最终判定为 56px。普通正文、说明、单位、只读值和普通标题统一为纯黑色；禁用、主操作、成功、警告、危险和反白文字使用正式 QSS 中的固定角色色。列表、树、表格、下拉弹出项和非原生目录选择框统一通过 `QAbstractItemView` 获得普通与选中文字规则。

模板制作向导的背景、边框和基础字体由 `app_theme.qss` 的 `frame_templateGuide`、`label_templateGuideTitle` 和 `label_templateGuideBody` 选择器维护。标题和正文均为 18px，标题保持蓝色加重、正文保持黑色常规。`TemplateEditorPage` 保留现有 RichText，只在区域名称上使用四种既有强调色和加重显示；其他内容继承正式 QSS。

按钮只使用两个已有角色：

- `uiRole=primary`：当前页面的主要保存或确认动作。
- `uiRole=danger`：清空、清零等危险动作；清空软件数据、总数清零、NG 清零和剔除复位同时由固定对象名直接使用同一组危险状态规则。

运行时视觉属性保持为：

- `uiState`：运行、停止、警告、故障和模板取景状态。
- `verdict`：`idle/ok/ng/fault` 检测判定。
- `hasError`：输入校验错误。

C++ 只设置状态属性并触发样式刷新；颜色、边框、字体和禁用视觉由 `app_theme.qss` 决定。模板向导既有区域名称强调色和字符框选画布既有标注色除外；`CharacterCropLabel` 只通过 `QPainter::setFont(font())` 继承控件字体，不保存字号或字重值。五个左侧抽屉页面的 8px 纵向滚动条只通过对应 `QScrollArea` 对象名限定，不影响文本编辑框和模板对话框的内部滚动条。

## 维护规则

- 固定布局和固定控件放在对应 `.ui`；模板树节点、字符命名行和预览卡片等数据驱动内容可动态创建。
- 页面类直接使用生成 Ui 引用，不增加 ViewBindings、控件 getter、Facade、页面工厂或路由器。
- `.ui` 对象名、提升控件类名、C++ 引用、QSS 选择器和 qmake 清单必须同步。
- 主图像提升控件固定为 `InspectionImageCanvas`，对象名固定为 `inspectionImageCanvas`。
- `OperationUiPolicy` 是主控按钮状态的唯一规则，`SettingsEditState` 是未应用设置状态的唯一实现。
- Startup 只负责进程初始化、加载翻译与正式 QSS，并创建应用级服务和 MainWindow；不创建或回挂三个逻辑 Page。
- 页面自有按钮直接连接所属 Page；MainWindow 只保留跨页面或应用级协调。
- 不新增 PageManager、UiManager、MainWindowBuilder、UiCompositionRoot、事件总线、页面注册表或控件访问层。
- 当前编辑模板是临时 UI 状态，不写入设置。
- 无效已选模板路径保留显示并允许移除，不静默切换或删除。
- 所有用户文字直接使用 UTF-8 中文，不写人为 Unicode 转义。
- 新增普通控件不设置内联 `styleSheet`；先使用正式 QSS 的类型样式和已有角色。
- 不新增第二套 QSS、主题管理器、兼容主题、控件皮肤层或资源生成脚本。
- 主控和导航图标只引用 `image.qrc` 中登记的静态 SVG，不保留 PNG/ICO 副本或回退路径。
- `setEnabled()` 和禁用原因 Tooltip 属于交互规则，不能因为界面重排而删除。
- 固定控件说明使用 Qt 原生 `setToolTip()`；列表项和下拉项的完整路径使用 `Qt::ToolTipRole`。
- 运行状态导致的禁用原因通过 `applyOperationUiAccess()` 更新；控件重新启用后恢复原 Tooltip。
