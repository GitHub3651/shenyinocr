# ui：界面层

本目录负责主窗口、页面交互、模板弹窗和检测结果呈现。界面层调用现有 Application Service，不直接实现检测算法、设备时序、设置存储或模板磁盘规则。

## 目录结构

```text
ui/
├─ README.md
├─ startup/
│  └─ activation_dialog.ui/.h/.cpp
└─ main_window/
   ├─ main_window.ui
   ├─ main_window.h/.cpp
   ├─ main_window_inspection.cpp
   ├─ main_window_settings.cpp
   ├─ inspection_image_canvas.h/.cpp
   ├─ verdict_result_label.h/.cpp
   ├─ operation_ui_policy.h/.cpp
   ├─ inspection/
   │  ├─ inspection_info_page.ui
   │  └─ inspection_page.h/.cpp
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

- 主窗口骨架和图像上方常驻工具栏：`main_window/main_window.ui`。
- 首次激活、许可证到期或设备不匹配时的激活窗口：`startup/activation_dialog.ui`。
- 检测信息、参数设定、图像设置、PLC 通讯和软件设置：分别打开对应功能目录中的五个页面 `.ui`；模板管理分组位于 `main_window/settings/detection_settings_page.ui`。
- 选择模板、保存模板和分割字符模板：分别打开三个模板子目录中的弹窗 `.ui`。

`main_window.ui` 只保留五个空页面根节点。`MainWindow` 依次对主窗口和五个页面生成 Ui 调用 `setupUi()`，再构造 `InspectionPage`、`MachineSettingsPage` 和 `TemplateEditorPage`。主窗口、五个页面和三个弹窗的生成 Ui 均由各自所有者使用 `std::unique_ptr` 管理。

## 页面协作

- `InspectionPage` 直接使用主窗口 Ui 和检测信息页 Ui，更新主图像、判定、运行状态和统计。
- `MachineSettingsPage` 直接使用四个设置页 Ui 和工具栏唯一硬触发开关，管理参数绑定、校验、未应用状态和运行时禁用规则；模板管理按钮的最终行列由 `detection_settings_page.ui` 定义，不由该类运行时调整。完整识别模式分组和纸巾粗糙度阈值位于参数设定页，图像设置页只保留图像保存与图像采集处理参数。
- `TemplateEditorPage` 直接使用主窗口 Ui、参数设定页 Ui 和图像设置页 Ui，直接连接参数页中的模板选择、制作、保存和分割字符按钮，并处理模板参数编辑。
- MainWindow 只保留跨页面或应用级协调；迁出页面且仍由 MainWindow 处理的按钮采用显式函数指针连接。

左侧五个导航按钮切换同一个 `QStackedWidget`。启动时默认显示检测信息；再次点击当前按钮会收起左侧面板；检测启动和故障自动停止均保持当前开合状态。

## 模板 UI

选择、制作、保存、分割字符和退出制作五个入口统一位于参数设定页的“模板管理”同级分组。制作模板作为整行蓝色主操作，选择模板和保存模板位于等宽双列并使用项目普通按钮样式，分割字符和退出制作分列底部左右两侧，默认透明且无可见边框，悬停和焦点状态显示底色与边框，按下时显示更深底色，文字颜色保持不变；五个控件统一使用 `pushButton_*` 对象名和 `QPushButton`，最小高度为 45px，图标位于文字左侧且整体居中，不使用圆角或渐变。`groupBox_templateManagement` 与 `groupBox_currentTemplateSettings` 使用同一个 QSS 卡片选择器。`TemplateEditorPage` 负责前四个入口及五个按钮的操作状态，`MainWindow` 只保留退出模板制作的跨区域收口。

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

单模板模式最多一项；多模板模式可包含多项；纸巾模式隐藏模板管理分组、当前模板设置和 ROI 制作控件，只显示粗糙度阈值。

四种模板模式统一使用主窗口的 `InspectionImageCanvas`，纸巾模式禁用模板绘图。字库和 OCR 使用“定位参考区域 → 检测多边形”，二维码使用“日期定位参考区域 → 二维码矩形 → 日期多边形”，钢印使用“吸管口定位参考区域 → 钢印多边形 → 日期定位参考区域 → 日期多边形”。钢印的两个参考区域和两个多边形分别保存、分别换算，不从日期多边形推导钢印区域，也不创建 OpenCV 原生交互窗口。

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

Startup 不知道 Page、主窗口生成 Ui、控件地址或页面状态。`ApplicationStartup` 只在许可证需要激活时创建 `ActivationDialog`，由该对话框调用 `RuntimeGuard::activate()` 并返回已验证的启动授权结果。MainWindow 直接持有自身和五个页面的生成 Ui；三个模板弹窗分别持有自身生成 Ui。逻辑 Page 使用明确的生成 Ui、服务和状态引用，不通过 ViewBindings、回调包装或控件查找访问界面。

## 操作状态

`OperationUiPolicy` 根据 `CameraClosed/CameraReady/CameraPreviewing/Detecting/Stopping/TemplatePreviewing/TemplateFrozen` 统一计算权限。普通预览和模板预览共用应用服务的预览采集能力，普通预览由主工具栏预览按钮控制，模板预览由 `TemplateEditorPage` 控制。运行故障对界面统一表现为 `Stopping`，自动停止完成后回到 `CameraReady` 或 `CameraClosed`。UI 禁用用于明确状态展示；真正影响设备和生产运行的操作仍由 Application/Runtime 检查。

## 专用控件

`InspectionImageCanvas` 是主窗口唯一检测图像和模板绘制画布，负责图像自适应显示、绘制步骤、显示坐标和强类型绘图事件，不负责模板保存、提示文案或检测算法。

`VerdictResultLabel` 是判定栏唯一显示控件，负责根据 `DetectionVerdictViewStyle` 选择 OK/NG SVG，按标签当前宽度的 70% 矢量绘制并水平、垂直居中。

`CharacterCropLabel` 只服务于字符模板编辑弹窗，负责字符框绘制、撤销、清空和坐标换算，并通过 `itemsChanged()` 通知弹窗刷新动态预览。

## 统一样式

唯一正式样式文件是 `app/resource/qss/app_theme.qss`，由 `ApplicationStartup` 在任何启动消息框出现前从 `:/qss/app_theme.qss` 加载一次。主题使用浅灰设备外壳、白灰内容面板、深色图像画布、钢灰边框和低饱和蓝色操作色；普通控件按控件类型自动继承样式，不在 `.ui` 或业务 C++ 中填写完整 `styleSheet`。

主窗口保持 1600×950 设计尺寸，主画面顶部是固定 74px 的常驻工具栏，相机和识别两个操作按钮固定为 76×62px、使用 27×27 图标和 16px 文字；相机、PLC 状态及唯一硬触发开关位于工具栏右侧。硬触发 QCheckBox 的勾选状态同时决定滑块图像和控件自身“开/关”文字，无已保存配置时默认关闭。图像/判定保持 `3:1`。最左侧导航栏固定 80px，五个导航按钮固定为 80×80px、使用 28×28 图标并在顶部连续排列，底部伸展项吸收剩余高度；导航按钮无边框和选中蓝条，当前入口由近白背景和蓝色文字表示。导航右侧的五页抽屉与主画面由水平 `QSplitter` 承载，无已保存状态时使用 400px 抽屉宽度，正常关闭时把完整 Splitter 状态保存到 AppSettings Schema 8。

主工具栏相机、识别和预览按钮直接使用 `:/svg/camera_on.svg`、`:/svg/camera_off.svg`、`:/svg/start.svg`、`:/svg/stop.svg` 和 `:/svg/preview.svg`；参数页模板管理分组使用 `:/svg/template_select.svg`、`:/svg/template_make.svg`、`:/svg/template_save.svg`、`:/svg/character_seg.svg` 和 `:/svg/template_exit.svg`。左侧导航使用 `:/svg/navigation/nav_*.svg`，硬触发 `QCheckBox` 通过正式 QSS 使用 `:/svg/toggle_off.svg` 和 `:/svg/toggle_on.svg`。`VerdictResultLabel` 根据 `DetectionVerdictViewStyle` 将 `:/svg/verdict_correct.svg` 或 `:/svg/verdict_wrong.svg` 按标签当前宽度的 70% 矢量绘制并水平、垂直居中，闲置和清空时不显示内容。`InspectionImageCanvas` 使用 `#202830` 底色，并通过正式 QSS 使用 `:/svg/canvas/canvas_background_grid.svg` 作为静态弱网格背景；`CharacterCropLabel` 使用相同的 `#202830` 纯色底，不使用网格。界面层不生成或换色这些资源。

界面文字只使用 16、18、20、26px 四个字号和常规 400、加重 600 两档字重。普通界面文字为 16px；18px 只用于模板制作向导标题和正文；识别内容和四个统计值为 20px；其他重要信息为 26px。普通正文、说明、单位、只读值和普通标题统一为纯黑色；禁用、主操作、成功、警告、危险和反白文字使用正式 QSS 中的固定角色色。列表、树、表格、下拉弹出项和非原生目录选择框统一通过 `QAbstractItemView` 获得普通与选中文字规则。

模板制作向导的背景、边框和基础字体由 `app_theme.qss` 的 `frame_templateGuide`、`label_templateGuideTitle` 和 `label_templateGuideBody` 选择器维护。标题和正文均为 18px，标题保持蓝色加重、正文保持黑色常规。`TemplateEditorPage` 保留现有 RichText，只在区域名称上使用四种既有强调色和加重显示；其他内容继承正式 QSS。

按钮只使用两个已有角色：

- `uiRole=primary`：当前页面或对话框的主要保存、确认或激活动作。
- `uiRole=danger`：删除软件设置、清零等危险动作；删除已保存的软件设置、总数清零、NG 清零和剔除复位同时由固定对象名直接使用同一组危险状态规则。

激活窗口的复制和退出按钮继承普通按钮的默认、焦点、悬停、按下和禁用状态，“激活”按钮使用现有 `uiRole=primary` 的对应完整状态；状态优先级保证按钮获得焦点后仍能显示悬停和按下效果。顶部状态提示统一使用危险红色和 600 加重，复制申请码成功后使用统一消息框提示。界面文件和业务 C++ 不保存颜色、字体、边框或按钮皮肤。

运行时视觉属性保持为：

- `uiState`：运行、停止、警告和模板取景状态。
- `hasError`：输入校验错误。
- `connectionState`：相机和 PLC 状态圆点的 `connected/disconnected` 显示。

C++ 只设置状态属性并触发样式刷新；颜色、边框、字体和禁用视觉由 `app_theme.qss` 决定。模板向导既有区域名称强调色和字符框选画布既有标注色除外；`CharacterCropLabel` 只通过 `QPainter::setFont(font())` 继承控件字体，不保存字号或字重值。五个左侧抽屉页面的 8px 纵向滚动条只通过对应 `QScrollArea` 对象名限定，不影响文本编辑框和模板对话框的内部滚动条。

## 维护规则

- 固定布局和固定控件放在对应 `.ui`；模板树节点、字符命名行和预览卡片等数据驱动内容可动态创建。
- 页面类直接使用生成 Ui 引用，不增加 ViewBindings、控件 getter、Facade、页面工厂或路由器。
- `.ui` 对象名、提升控件类名、C++ 引用、QSS 选择器和 qmake 清单必须同步。
- 主图像提升控件固定为 `InspectionImageCanvas`，对象名固定为 `inspectionImageCanvas`。
- 判定栏提升控件固定为 `VerdictResultLabel`，对象名固定为 `label_verdictResult`。
- `OperationUiPolicy` 是主控按钮状态的唯一规则，`SettingsEditState` 是未应用设置状态的唯一实现。
- Startup 只负责进程初始化、加载翻译与正式 QSS，并创建应用级服务和 MainWindow；不创建或回挂三个逻辑 Page。
- 页面自有按钮直接连接所属 Page；MainWindow 只保留跨页面或应用级协调。
- 不新增 PageManager、UiManager、MainWindowBuilder、UiCompositionRoot、事件总线、页面注册表或控件访问层。
- 当前编辑模板是临时 UI 状态，不写入设置。
- 无效已选模板路径保留显示并允许移除，不静默切换或删除。
- 所有用户文字直接使用 UTF-8 中文，不写人为 Unicode 转义。
- 弹窗、状态栏和检测结果正文只显示操作员可执行提示；错误码、路径、原生错误和内部诊断先写入现有分类日志。
- 新增普通控件不设置内联 `styleSheet`；先使用正式 QSS 的类型样式和已有角色。
- 不新增第二套 QSS、主题管理器、兼容主题、控件皮肤层或资源生成脚本。
- 主控和导航图标只引用 `image.qrc` 中登记的静态 SVG，不保留 PNG/ICO 副本或回退路径。
- `setEnabled()` 和禁用原因 Tooltip 属于交互规则，不能因为界面重排而删除。
- 固定控件说明使用 Qt 原生 `setToolTip()`；列表项和下拉项的完整路径使用 `Qt::ToolTipRole`。
- 运行状态导致的禁用原因通过 `applyOperationUiAccess()` 更新；控件重新启用后恢复原 Tooltip。
