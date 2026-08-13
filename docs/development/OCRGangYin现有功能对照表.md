# OCRGangYin 现有功能对照表

> 基线版本：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`。本表按当前生产源码、UI、工程、资源和脚本人工反向核对；不以目标架构推测现有行为。

## 状态与证据约定

- `待盘点`：尚未追踪完整调用链。
- `已基线`：已记录当前输入、输出、副作用和可重复验证方法；若证据写“U”，仍需用户提供实际运行值，Stage 0门禁尚不因此自动通过。
- `迁移中`：新旧路径同时存在，尚未完成对照。
- `已验证`：新路径通过约定验证，旧路径可进入引用清理。
- `已延期`：升级计划明确延期，现有实现保持原位。
- `已确认删除`：用户明确同意，且已记录影响。
- 证据缩写：`S`=已在基线HEAD完成源码/UI/工程静态核对；`T`=已有离线测试源码、等待Qt Creator执行；`U`=等待用户从原入口、真实设备或固定样本确认；`P`=升级计划明确延期。

## 1. 启动、系统保护与部署

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SYS-001 | 应用启动与本地化 | 双击主程序 | 部署目录完整 | `main.cpp::main`→安装中英文翻译→`Widget`→`initStyle`→最大化显示 | Qt资源中的`Translate_*.qm`、`:/qss/1.css`；启动生效 | 主窗最大化、中文UI和浅蓝样式；资源缺失时对应翻译/样式不加载 | 创建QApplication和主窗 | 启动外观、窗口状态和文本均需保持 | `startup/`+`ui/` | 保留 | Release启动，核对窗口、中文按钮、禁用态样式 | 已基线 | S；U |
| SYS-002 | 授权校验 | 启动及每24小时定时器 | 可执行文件旁有`license.ini` | `main.cpp`→`RuntimeGuard::check`→XOR/SHA256解密→解析`expires` | 当前日期；固定密钥；启动和24h周期生效 | 有效则继续；缺失、格式错或过期弹模态错误并退出 | 读取授权文件；创建24h定时器 | 失败必须阻止继续运行 | `startup/runtime_guard.*` | 保留 | 分别使用有效、缺失、损坏、过期授权启动；记录提示与退出 | 已基线 | S；U |
| SYS-003 | 单实例 | 第二次启动 | 首实例共享内存`ecust`仍存在 | `main.cpp`→`QSharedMemory::create(1)` | 固定键`ecust`；启动生效 | 首实例继续；第二实例提示“程序已经运行”并退出 | 创建进程间共享内存 | 同一用户会话只允许一个实例 | `startup/single_instance_guard.*` | 保留 | 连续启动两次，第二次提示且不出现第二主窗 | 已基线 | S；U |
| SYS-004 | 日志写入与保留 | 启动后任意Qt日志 | 可执行目录可写 | `main.cpp::setupLogging`→`qInstallMessageHandler` | `<exe>/log/app_log_yyyy-MM-dd.txt`；保留3个月 | 写入时间、级别、文件/行；目录或文件不可写时不能落盘 | 创建日志目录/文件；删除3个月前日志 | 日志格式和保留策略纳入回归 | `system_support/logging/` | 保留 | 启动后触发一次提示，核对当日日志；放置过期测试日志后重启核对清理 | 已基线 | S；U |
| SYS-005 | 崩溃记录 | Windows未处理异常 | 日志目录可写 | `main.cpp`→`SetUnhandledExceptionFilter`→`CCrashStack` | Windows异常上下文；进程崩溃时生效 | 尝试写崩溃信息后进程终止；写盘失败无业务恢复 | 写崩溃日志 | 保留现有诊断，不用测试性崩溃污染生产 | `system_support/crash/` | 保留 | 仅在隔离调试构建按现场批准方案验证；Stage 0记录源码链 | 已基线 | S；U |
| SYS-006 | OCR模型初始化 | 主窗口构造时装配设备 | `config1.txt`、模型和字典存在 | `Widget::Widget`→`PaddleOcrEngine`→`OCRConfig`→`DBDetector`/`Classifier`/`CRNNRecognizer`构造；本切片由适配器独占具体模型 | 配置相对路径以配置文件目录解析；启动生效 | 模型加载成功后OCR可用；配置键/模型异常保持构造期失败 | 适配器加载并持有Paddle模型内存，窗口只持有`IOcrEngine` | 模型、阈值、路径解析和初始化日志顺序保持 | `devices/ocr/`+后续`startup/`装配 | Stage 2已适配 | 有效部署启动、日志2..5；缺配置/模型副本失败表现 | 已验证 | S；T；U；用户确认Fake测试、主工程模型初始化日志及实际逐帧OCR调用均正常 |
| SYS-007 | 公共设置与模板恢复 | `Widget`构造 | 用户AppData可读 | `Widget::loadSettings`→`AppSettingsManager::loadGlobalSettings`→`applyGlobalSettingsToUi`→`restoreTemplatesForMode` | `AppDataLocation/settings.ini`，配置v2；无效时默认 | 恢复模式、保存、相机/PLC、模板历史和分隔条；读失败使用默认并记日志 | 读取设置和模板资源 | 详见SET/TPL功能ID | `recipes/`+`system_support/settings/` | 保留后拆分 | 修改并应用设置、退出重启，逐项核对；损坏INI核对默认回退 | 已基线 | S；U |
| SYS-008 | 启动PLC延迟连接 | 主窗构造后1秒 | PLC设备接口存在 | `Widget::Widget`→`QTimer::singleShot(1000)`→`IPlcDevice::connectTo`→`Snap7PlcDevice` | 保存的IP/Rack/Slot；1秒后生效 | 成功连接并刷新硬件控件；失败仅日志/状态，不阻止主窗 | 建立PLC网络连接 | Stage 1-3不改变连接时序 | `devices/plc/` | Stage 2已适配 | 有PLC/无PLC各启动一次，记录1秒后状态和可编辑控件 | 已基线 | S；T；U；2026-08-13用户确认PLC适配测试和主程序门禁无问题；真实PLC连接证据仍按原风险接受结论延期 |
| SYS-009 | 正常退出与资源释放 | 关闭主窗/进程退出 | 可有运行线程、相机、PLC | `Widget::closeEvent`/析构→协作停止线程→`ICameraDevice::requestStop/close`→断开PLC；`aboutToQuit`→`cv::destroyAllWindows` | 线程等待上限和当前设备状态 | 正常关闭；线程未及时退出仅记录警告，不调用`terminate()` | 停线程、关设备、断PLC、释放OpenCV窗 | 必须保持协作停止、无残留线程 | `runtime/`+`startup/`+`devices/camera/` | Stage 2相机适配、Stage 3收敛 | 检测中、模板预览中、空闲时分别关闭；确认提示、进程退出和设备释放 | 已基线 | S；T；U；2026-08-13用户确认相机适配测试及主程序集中门禁均无问题 |
| SYS-010 | Release运行时部署校验 | Qt Creator Release链接后 | `dist/ShengYin`完整 | `AutoOCRproject.pro::QMAKE_POST_LINK`→`deploy_runtime.ps1` | 源`dist/ShengYin`、目标构建`release`；Release链接后 | 校验清单/哈希并复制DLL、模型、配置；缺失或不一致使部署脚本失败 | 写Release运行目录 | 保持部署可复现；构建只由用户执行 | `system_support/deployment/` | 保留 | Qt Creator Run qmake+Release Rebuild，核对部署结果和脚本报错 | 已基线 | S；U |

## 2. 主界面与交互

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UI-001 | 模式与参数页面 | “识别模式”下拉框 | 非检测/非模板忙碌状态 | `comboBox_4::currentIndexChanged`→`setupDetectModeChangeTracking`→存旧模式路径→`restoreTemplatesForMode`→更新可见参数；本切片统一钢印与字库家族的当前模板编辑区并恢复钢印字符切割入口 | 界面固定顺序：模板匹配、字库匹配、深度模型、纸巾检测、二维码+三期；内部ID依次为stamp/word/ocr/tissue/barcode_word | 切换对应参数和模板历史；钢印显示单Profile编辑选择及字符切割；无效历史保留空状态并可提示 | 保存当前模式和模板历史 | 五个现有入口不可丢失，钢印不能退化为仅重叠检测 | `ui/pages/` | 保留 | 依次切换五模式，核对控件显隐、模板名、历史恢复；钢印编辑区与字库家族布局一致 | 已基线 | S；U；2026-08-13用户确认钢印编辑区、字符裁切入口及两种单模板模式切换均正常 |
| UI-002 | 操作状态与按钮使能 | 开相机、预览、冻结、检测、停止、关闭 | 任意主流程状态变化 | `updateOperationUiState`+`updateHardwareParameterUiEnabled` | `CameraClosed/CameraReady/TemplatePreviewing/TemplateFrozen/Detecting/Stopping` | 只允许当前状态合法动作；非法重复启动/关相机给提示 | 控件enable/style变化 | 状态机可观察行为保持 | `ui/controllers/main_page_controller.*` | 保留后抽离 | 逐状态截图并尝试每个顶栏按钮，核对禁用/提示 | 已基线 | S；U |
| UI-003 | 图像自适应显示 | 相机帧、检测结果、模板原图 | `ImageLabel`有图像 | `ImageLabel::setPixmap/resizeEvent`→按宽高比缩放居中 | 当前控件尺寸 | 缩放但不改变原图；空图清空 | 仅UI缓存QPixmap | 保持缩放、居中和重绘 | `ui/widgets/image_label.*` | 保留后移动 | 用横图/竖图并调整窗口，核对比例、居中和Overlay位置 | 已基线 | S；U |
| UI-004 | 结果帧绑定显示 | 字库家族产生检测结果 | 检测运行 | `handleStreamingFrame`→`shouldSuppressStreamingFrame`；检测完成更新`m_latestAnnotatedResult` | 字库/二维码模式启用结果绑定 | 结果出现后流帧不覆盖上一完整结果；新正式结果替换 | 持有最近标注图 | 保持画面、框、OK/NG来自同帧 | `ui/presenters/result_presenter.*` | 保留 | 连续移动产品，确认结果图不被实时帧覆盖且下一结果可替换 | 已基线 | S；U |
| UI-005 | 结果与状态展示 | 任一检测完成或状态变化 | 已启动检测 | 五模式槽→`resultlabel/resultlabel_7/speedLabel/currentTemplateName/statusLabel` | 识别文本、模板Profile、耗时和判定 | 展示OK/NG、文本、模板名、耗时；停止保留最后正式结果 | 更新UI文本 | 文本和保留时机纳入回归 | `ui/presenters/` | 保留 | 每模式各跑OK/NG，记录全部字段；停止后核对是否保留 | 已基线 | S；U |
| UI-006 | 模板引导与提示 | 制作模板、绘图事件、悬停 | 模板预览或冻结 | `setupTemplateGuide`→`handleTemplateGuideEvent`→`updateTemplateGuideText`；`eventFilter`延迟500ms工具提示 | 当前模式与已画点数 | 显示分步引导和模式专用说明；离开隐藏 | 创建/调整引导Frame | 保持中文提示和步骤含义 | `ui/template_editor/` | 保留 | 五模式进入制作模板，悬停按钮并执行绘图，核对引导变化 | 已基线 | S；U |
| UI-007 | 防滚轮误改参数 | 鼠标滚轮经过下拉框/SpinBox | 主窗活动 | `Widget::eventFilter`拦截`QComboBox/QAbstractSpinBox`的Wheel | 所有安装事件过滤器的控件 | 滚轮被丢弃，点击/键盘仍可修改 | 无 | 防误操作行为保持 | `ui/` | 保留 | 记录值，滚轮后不变；点击选择后可变 | 已基线 | S；U |
| UI-008 | 软件数据目录快捷打开 | 双击只读目录框 | AppData目录可创建/打开 | `eventFilter`→`QDir::mkpath`→`QDesktopServices::openUrl` | `AppSettingsManager::globalSettingsDirPath()` | 打开目录；创建/打开失败弹提示 | 可能创建目录并启动资源管理器 | 保留入口 | `ui/settings_page.*` | 保留 | 双击目录，核对资源管理器路径；只读失败场景记录提示 | 已基线 | S；U |
| UI-009 | 右侧分隔条记忆 | 拖动参数/结果区域分隔条 | 主窗已加载 | `QSplitter::saveState`→全局设置；启动`restoreState` | 字节状态；退出/保存后生效 | 重启恢复；无效状态回默认并记日志 | 写全局设置 | 保持布局记忆 | `ui/layout/` | 保留 | 拖动、退出、重启；再注入无效状态核对回退 | 已基线 | S；U |

## 3. 设置与参数应用

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SET-001 | 全局设置读取与安全保存 | 启动及各“设置/确认”按钮 | AppData可访问 | `AppSettingsManager::loadGlobalSettings/saveGlobalSettings`→临时文件验证→替换/回滚；本切片把按模式已发布配方UUID恢复从字库家族扩展到钢印与深度OCR | 配置版本2；当前用户AppData；每个模式至多一个已发布配方UUID | 合法值加载；未知模式的UUID记忆被丢弃；旧配置没有新键时保持兼容；保存失败保留旧文件并报告/日志 | 读写`settings.ini`及临时/备份 | 整机设置语义保持，新增键为可选兼容字段 | `system_support/settings/` | 保留后封装 | 四种模板模式分别选择配方、切换及重启恢复；损坏配置和不可写目录回退 | 已基线 | S；T；U；2026-08-13用户确认钢印/OCR切换恢复、模式隔离及旧入口覆盖正常 |
| SET-002 | 整机默认值 | 首次启动或恢复默认 | 无有效全局设置 | `defaultGlobalSettings`→`applyGlobalSettingsToUi`；恢复默认同时清空已发布配方模式记忆 | 字库模式、全不保存、仅标注、彩色、无旋转、触发启用、间歇、曝光800、增益1、PLC `192.168.10.10/0/1`、纸巾6.0；配方UUID记忆默认为空 | UI采用默认且不自动选择配方；硬件未必立即写入 | 默认可被保存 | Stage 1保持既有默认并增加空配方记忆 | `recipes/machine_settings.*` | 保留/计划内优化 | 备份并清除设置后启动，确认不自动选配方；不覆盖用户现有文件 | 已基线 | S；T；U；空UUID默认与恢复默认清理已由静态检查覆盖，设置往返测试和主程序回归通过 |
| SET-003 | 模板私有设置 | 选择、保存、编辑模板 | 模板目录可读写且没有外部程序正在浏览其内部目录 | 旧INI继续映射为规范`RecipeProfile`；本切片将Widget对草稿/编辑会话的候选复制、参数/资产校验及发布成功提交收口到统一`TemplateRecipeWorkflow`，四模板模式共享相同事务边界 | Profile目标、阈值、定位框、字符框、二维码参数、顺序和模式特有资产保持；纸巾不进入模板会话 | 工作流只在完整校验和正式发布成功后替换会话状态；失败保留上一会话与正式配方 | 旧Profile继续写INI和原目录；新配方由RecipeStore整目录替换 | 后续迁入Recipe，不要求兼容旧格式 | recipes工作流事务+Widget薄桥 | Stage 1分步迁移 | 四模板模式发布/重发；失败后继续编辑；旧入口回归 | 已基线 | S；T；U；2026-08-13用户确认统一工作流测试及可执行主程序路径无问题；深度OCR因当前无旧模板未单独执行配方重发 |
| SET-004 | 未应用标记与启动确认 | 编辑带绑定的参数 | 参数可编辑 | `setupGlobalSettingBindings`/模板dirty跟踪→标签加`*`→启动`dirtySettingsMessage` | UI值与`m_appliedGlobalSettings`/Profile比较 | 启动前列出未应用项；取消不启动；继续会恢复已应用值后运行 | 标签变化；可能丢弃未应用UI值 | 保持“应用”和“编辑”边界 | `ui/settings_controller.*` | 保留 | 修改相机、PLC、阈值但不确认，启动后分别选取消/继续 | 已基线 | S；U |
| SET-005 | 运行中参数禁用 | 检测/模板状态变化 | 硬件或模板操作进行中 | `updateHardwareParameterUiEnabled`→相机运行状态+`IPlcDevice::isConnected`+`registerHardwareAction` | 相机打开、PLC连接、操作状态 | 运行中禁止会改变设备/关键参数的控件；不同连接状态允许不同字段 | 控件状态改变 | 防止中途改运行快照 | `ui/settings_controller.*`+设备窄接口 | Stage 2分步适配 | 在空闲、相机开/关、PLC连/断、检测中逐项核对 | 已基线 | S；T；U；PLC适配门禁及2026-08-13相机适配测试、主程序集中门禁均无问题 |
| SET-006 | 相机曝光应用 | 曝光“设置”或检测启动 | 相机已打开 | `on_sureButton_clicked`→`queryCameraExposureRange`→`ICameraDevice::get/setFloatValue` | 整数曝光；相机SDK给最小/最大；默认800 | 范围内写入；越界/SDK失败提示；打开相机时保存值会按范围调整并提示 | 写相机`ExposureTime`；成功保存设置 | 数值与生效时机保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 最小、最大、越界、正常值各一次；重启开相机核对 | 已基线 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 |
| SET-007 | 相机增益应用 | 增益“设置”或检测启动 | 相机已打开 | `on_pushButton_12_clicked`→`applyCameraGainFromUi`→`ICameraDevice::get/setFloatValue` | 整数；SDK范围；默认1 | 合法写入并保存；空/越界/SDK失败提示 | 写相机`Gain` | 保持 | `devices/camera/`+Recipe | Stage 2薄适配 | 边界/越界/正常值，重启核对 | 已基线 | S；T；U；2026-08-13用户确认相机参数与主程序集中门禁无问题 |
| SET-008 | 颜色通道应用 | “颜色通道→确认” | 线程存在 | `on_pushButton_7_clicked`→`emit choosechannel`→`MyThread/CameraThread::receivecolorchannel*` | 彩色/红/绿/蓝；默认彩色 | 后续帧按选定通道处理；无效索引回彩色 | 更新线程参数并保存 | 通道映射保持 | Recipe+detection input transform | 保留后迁移 | 同一固定彩色场景切换四项，记录处理图与重启恢复 | 已基线 | S；U |
| SET-009 | 图像旋转应用 | “图像旋转→设置” | 线程存在 | `on_pushButton_9_clicked`→`emit rotate`→采集线程旋转 | 无/顺90/逆90/180；默认无 | 后续采集图旋转；无效索引回无 | 更新线程参数并保存 | 旋转方向保持 | Recipe+detection input transform | 保留后迁移 | 带方向标记固定场景切四项，核对方向和重启恢复 | 已基线 | S；U |
| SET-010 | 纸巾粗糙度阈值 | 纸巾模式“设置”及启动 | 值>0 | `applyTissueRoughnessThresholdFromUi/applyRuntimeThreadSettingsFromUi`→显式构造`TissueRecipeParameters`→启动前复制到`MyThread/CameraThread` | 唯一代码默认由`TissueRecipeParameters`提供6.0；有已保存整机设置时使用保存值 | 合法值固定为本次运行线程的参数副本；非法提示且不启动 | 保存整机设置；不再更新进程级检测器默认 | Stage 1统一唯一6.0来源并改为运行副本 | `recipes/product_recipe.*` | 计划内优化 | 冷启动、无设置、已有设置分别记录UI阈值；运行离线测试 | 已验证 | S；T；U；2026-08-12用户Qt Creator Release主程序启动/退出码0、纸巾阈值显示正常，Pipeline测试`6 passed, 0 failed` |
| SET-011 | 存图策略设置 | 保存模式、类型、路径浏览 | 主窗空闲 | UI改变→`syncImmediateGlobalSettingsFromUi`→`saveSettings`；浏览按钮选目录 | 不保存/NG/OK/全部；两类都存/仅标注/仅原图；默认不保存+仅标注 | 选项控制后续存图；未选目录时依现有路径逻辑；浏览取消不变 | 写全局设置 | 详见SAVE功能 | `recipes/save_policy.*` | 保留后迁移 | 逐组合选择、重启，核对显隐和实际文件 | 已基线 | S；U |
| SET-012 | 清空当前软件数据 | 设置页按钮 | 用户二次确认 | `clearCurrentSoftwareData`→删除全局设置→重置UI/状态 | 只针对当前用户软件数据 | 确认后恢复默认公共设置；取消不变；失败提示 | 删除`settings.ini`；不删除模板、图片、授权、日志 | 删除范围必须保持 | `system_support/settings/` | 保留 | 在测试用户数据中确认/取消各一次，核对保留项 | 已基线 | S；U |
| SET-013 | 恢复默认设置 | 设置页按钮 | 用户确认；设备状态决定可立即应用项 | `restoreDefaultGlobalSettings`→按相机/PLC连接状态选择性恢复→dirty刷新 | `defaultGlobalSettings` | 可立即项恢复；不能立即写硬件项保持`*`待应用并提示 | 改UI/已应用设置，可能写设置 | 状态相关语义保持 | `ui/settings_controller.*` | 保留 | 相机开/关、PLC连/断四组合执行并核对星号/提示 | 已基线 | S；U |

## 4. 模板制作、加载与编辑

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| TPL-001 | 实时模板预览 | “制作模板”第一次点击 | 相机已打开、未检测 | `on_VideoShoot_clicked`→`startTemplatePreview`→软触发`MyThread`→`ICameraDevice`→`signal_templatePreviewFrame` | 当前曝光/增益/旋转/通道 | 显示实时画面和引导；相机/线程错误提示并退出预览 | 启动采集线程；不统计/PLC | 预览入口保持 | `ui/template_editor/`+`devices/camera/` | Stage 2相机适配 | 五模式分别进入预览，核对状态、按钮、实时画面 | 已基线 | S；T；U；2026-08-13用户确认模板预览与主程序集中门禁无问题 |
| TPL-002 | 冻结、重拍与退出 | 预览中再次点制作模板；已有选择时重拍；退出按钮/Esc | 有最近预览帧 | `freezeTemplatePreview`/`resetTemplateCaptureState`/`stopTemplatePreview`→相机接口停止唤醒 | 最近克隆帧 | 冻结当前帧并允许绘图；重拍有确认；退出清理选择并回相机就绪 | 协作停止预览线程、清缓存 | 保持状态转换 | `ui/template_editor/`+`devices/camera/` | Stage 2相机适配 | 冻结、重拍取消/确认、Esc/退出各一次 | 已验证 | S；T；U；既有深度OCR冻结/绘图/保存及2026-08-13相机适配主程序集中门禁均无问题 |
| TPL-003 | 定位矩形绘制 | 冻结模板图后左键拖动 | 钢印/字库/深度OCR/二维码模式 | `ImageLabel::mousePress/Move/Release`→`m_trackingRect`→引导事件 | 显示坐标，保存时换算到原图 | 形成归一化定位框；过小/无框不能完成保存 | 仅UI选择状态 | 独立登记ImageLabel行为 | `ui/widgets/image_label.*` | 保留后移动 | 不同比例窗口画框，核对显示与保存后物理框 | 已验证 | S；U；深度OCR实际模板发布证明定位框已形成并完成坐标保存 |
| TPL-004 | 二维码矩形与即时读码 | 二维码模式第二个矩形 | 已有定位框 | `ImageLabel`→`validateBarcodeTemplateRect`→`IBarcodeDecoder::decode`；DLL与通用策略已迁入设备适配器 | BarcodeOptions默认DataMatrix、padding8%、预算60ms、fallback开 | 可读则保留框并继续日期多边形；不可读弹原因、清二维码/日期但保留定位框 | 适配器动态加载/调用`BarcodeDecoder.dll` | 二维码必须先验证 | `ui/template_editor/`+`devices/barcode/` | Stage 2已适配 | 可读、不可读、越界二维码框各一次，核对清理范围 | 已基线 | S；T；U；2026-08-13用户确认假ABI测试、实际框选即时读码和主工程均无问题 |
| TPL-005 | 日期多边形绘制与闭合 | 左键逐点、右键闭合 | 已有前置框 | `ImageLabel`多边形状态→`signal_templateGuideEvent` | 至少3点 | 闭合后可保存/提示；点数不足保持绘制；Esc清理当前选择 | UI状态 | 保持鼠标/键盘语义 | `ui/widgets/image_label.*` | 保留后移动 | 2点右键、3+点右键、Esc，核对状态与提示 | 已验证 | S；U；深度OCR实际模板发布及随后检测启动证明日期多边形已闭合并落盘 |
| TPL-006 | 坐标换算与通用模板保存 | “保存模板”及动态“发布当前模板/模板组” | 单模板保存需框完整；模板组发布需字库家族已加载至少两个旧Profile | 坐标/资源生成不改；草稿发布及成功后编辑会话建立由统一工作流一次完成，Widget不再分别编排两类会话 | 显示名、模式、Profile顺序、参数和资产命名空间保持；钢印配方包含独立`stampRing`角色 | 发布或编辑会话建立失败不替换Widget当前工作会话；正式目录仍由RecipeStore保证 | 旧目录只读作为发布源；成功时事务写入`AppDataLocation/recipes/<UUID>` | 计划内改为Recipe整目录安全保存 | recipes workflow/publisher/store+Widget薄命令 | Stage 1分步迁移 | 四模板模式首次发布、随后编辑、UUID和失败保持 | 已基线 | S；T；U；统一工作流自动测试及字库/二维码/钢印实际发布回归通过；深度OCR当前无旧模板未单独发布 |
| TPL-007 | 钢印环与钢印区域标定 | 钢印模板保存及已发布配方选择 | 通用框已保存 | 旧入口仍写`template_ring.bmp`；本切片选择钢印配方时从`stampRing`和标定资产原子初始化候选重叠引擎 | OpenCV交互ROI；相对吸管口中心坐标；配方内目标为`assets/profiles/<n>/template_ring.bmp` | 环图、YAML或钢印区域无效时拒绝选择且保留当前引擎 | 旧入口仍写模板/YAML；新发布只事务复制到配方目录 | 保持标定次序、坐标及检测行为 | recipes资产合同+Widget运行资源桥 | Stage 1分步迁移 | 钢印配方选择后重叠引擎可启动；缺环/坏环/缺钢印区拒绝 | 已基线 | S；T；U；2026-08-13用户确认钢印配方选择、启动/停止及旧模板回归正常 |
| TPL-008 | 单模板选择与校验 | 旧“选择模板”及动态“已发布配方” | 空闲状态 | 本切片把钢印和深度OCR已发布配方选择接入统一列表，并在完整解码定位图/YAML/钢印环后一次安装；旧目录选择保留 | UUID、当前`DetectionMode`和模式必需角色；钢印需要tracking/calibration/stampRing，OCR需要tracking/calibration | UUID/Schema/资源/模式、图像解码或运行标定任一无效即拒绝且不改写当前模板 | 新路径只读配方目录；旧入口仍更新历史和引擎 | 保持失败不误启动和旧入口兼容 | recipes selection/assembler/store+Widget薄桥 | Stage 1分步迁移 | Stamp/OCR选择、模式错误、坏资源、输出保持及旧入口回归 | 已基线 | S；T；U；2026-08-13用户确认两模式已发布配方选择、参数显示、启停及旧入口覆盖正常 |
| TPL-009 | 字库家族多Profile选择 | 旧“选择模板”多目录入口；新增“发布当前模板组”和“已发布配方” | 空闲状态 | 编辑态保存完整有序Profile；启动时深拷贝为本次运行专用Profile快照，定位选择和检测只读快照 | 配方内部Profile顺序、名称、参数、资源路径、资产键及UUID保持；运行期间编辑缓存与运行缓存隔离 | 快照准备或原有预检失败不启动；停止/线程结束释放运行快照；旧入口保持 | 发布/选择继续更新编辑会话，启动仅复制内存资源且不写盘 | 新格式以一个产品配方承载一个或多个Profile并自动选择 | recipes选择/编辑会话+Widget启动薄桥，后续迁入runtime | Stage 1分步迁移 | 两模式启动后Profile选择、停止/再次启动、重选/重启和旧入口 | 已基线 | S；T；U；2026-08-13用户确认两个字库家族及旧多目录入口启停/再次启动均正常 |
| TPL-010 | 当前Profile编辑器 | 多Profile加载或模板组发布后下拉选择 | 至少一个Profile | 编辑器继续读编辑态`m_wordTemplateProfiles`；启动前形成独立运行Profile快照，运行检测不再回读编辑态Profile | Profile顺序、名称、目标、阈值、资源键及原图绝对路径保留 | 运行中控件继续禁用；停止后编辑缓存保持并可再次启动 | 编辑显示与运行资源在内存中分离；不新增磁盘写入 | 保持编辑对象与运行快照边界 | recipes编辑会话+Widget资源桥+后续`ui/template_editor/` | Stage 1分步迁移 | 启动/停止前后编辑显示保持，检测只读快照 | 已基线 | S；T；U；2026-08-13用户确认停止后Profile、目标字符和阈值保持且可再次启动 |
| TPL-011 | 单Profile目标字符 | “确认字符”及当前Profile“分割字符模板” | 当前模板有效且检测停止 | 目标解析和资源预检保持；当前或完整Profile候选由统一工作流校验并同UUID发布，Widget只安装成功结果 | `RecipeProfile::targetText`、字符框及当前Profile内部字符资产 | 字符缺失、Schema或事务失败时工作会话、正式配方和运行字符缓存均保持上一版本 | 旧Profile行为不变；新Profile不写旧INI，成功时同UUID事务替换 | 保持字符解析和提示 | recipes workflow+Widget资源桥 | Stage 1分步迁移 | 四模板模式单Profile目标、同UUID和失败保持 | 已基线 | S；T；U；参数事务测试和字库/二维码/钢印实际同UUID重发无问题；深度OCR人工项未执行 |
| TPL-012 | 批量目标字符 | 多Profile配方“批量确认字符”及单Profile裁切 | 已加载多个Profile | 完整有序Profile候选的原子校验和单次发布收口到统一工作流，Widget不再先原地更新编辑会话 | 每Profile目标、顺序和资源命名空间独立；一次操作最多一次配方发布 | 任一验证或事务失败时工作会话和正式配方同时保留上一版本 | 旧目录逐项写INI；新配方只经一次整目录事务提交 | 保持旧批量结果与多Profile资产隔离 | recipes workflow+Widget薄接入 | Stage 1分步迁移 | 多Profile批量目标、单次发布、失败后重试和UUID保持 | 已基线 | S；T；U；统一工作流自动测试及字库家族批量重发回归无问题 |
| TPL-013 | 单Profile图像阈值 | 阈值“设置” | 当前Profile有效且检测已停止 | 阈值范围和生效顺序保持；候选Profile由统一工作流同UUID发布成功后Widget才更新运行阈值 | `RecipeProfile::imageThreshold`，保持0..100百分比语义和当前生效顺序 | Schema或事务失败时工作会话、正式配方和运行阈值保持上一版本 | 旧Profile行为不变；新Profile不写旧INI | 保持百分比语义 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 四模板模式阈值、同UUID和失败回退 | 已基线 | S；T；U；参数事务测试和可用模板模式实际同UUID重发无问题；深度OCR人工项未执行 |
| TPL-014 | 批量图像阈值 | 多Profile配方“批量设置阈值” | 多Profile已加载 | 原按钮校验、缓存/`ssim`/dirty顺序保持；完整Profile更新和单次事务提交由统一工作流完成 | 每Profile阈值不被重排；一次按钮操作最多一次配方发布 | 任一Profile验证或事务失败时工作会话与正式配方均保留上一版本 | 旧目录逐项写INI；新Profile只经一次整目录事务提交 | 保持旧逐项结果和整配方原子提交 | recipes workflow+Widget保存桥 | Stage 1分步迁移 | 多Profile批量阈值、`ssim`、单次发布和失败重试 | 已基线 | S；T；U；统一工作流自动测试及字库家族批量阈值回归无问题 |
| TPL-015 | 手动字符模板切割 | “分割字符模板” | 字库家族或钢印模式、Profile有原图/定位框/date_poly且检测已停止 | 裁切对话框和临时工作区保持；Profile资产替换、整配方校验及同UUID发布由统一工作流的候选副本完成 | 框、变体名、Profile顺序、目标路径、UUID和对话框交互保持；钢印按原精确字符文件名规则重载 | 取消不发布；资产映射、Schema或事务失败时工作会话、正式配方和当前缓存均保持原值 | 旧字符文件仍写旧目录；已发布配方只写临时工作区和RecipeStore事务目录 | 保持绘图、排序、变体和清理范围 | 原对话框+recipes workspace/workflow+Widget交互 | Stage 1分步迁移 | 字库/钢印已发布配方裁切、失败保持、同UUID及重选 | 已基线 | S；T；U；字符资产事务测试及钢印/字库实际保存、同UUID重发回归无问题 |
| TPL-016 | 按模式记忆模板 | 配方发布/选择、切换模式、退出和下次启动 | 全局设置可写 | 本切片将钢印和深度OCR单配方UUID纳入同一按模式恢复规则，旧路径保留回退 | 每个模式的UUID和配方Profile彼此保持 | 发布后安装失败不覆盖旧路径记忆；恢复失败清理失效UUID并回退旧路径 | 读写全局`settings.ini` | 保持模式隔离、失败原子性和旧路径兼容 | 设置桥+recipes选择装配+Widget薄接入 | Stage 1分步迁移 | 四模板模式切换/重启恢复及旧路径覆盖规则 | 已基线 | S；T；U；2026-08-13用户确认钢印/OCR来回切换恢复、配方不串模式及旧路径覆盖正常 |

## 5. 五种检测与共同算法行为

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| DET-001 | 共同定位与位姿 | 软/硬采集线程获得帧 | 非纸巾模式且模板有效 | `MyThread/CameraThread`→`TrackingPoseMatcher::setTemplate/match`→`DetectionPose`→`dispatchDetectionByMode` | tracking模板；-45..45度、步长2、金字塔0.2、阈值0.3 | 匹配成功映射日期/二维码多边形；失败产生对应NG路径或跳过 | 线程持有模板旋转缓存；无直接PLC | 定位范围、阈值和坐标变换保持 | `detection/common/pose_matcher.*` | 保留后迁移 | 固定角度/位移/无目标样本，记录pose、Profile和结果 | 已基线 | S；U |
| DET-002 | 模板匹配模式（内部钢印+字符检测） | 界面“模板匹配”（索引0）检测帧 | 有tracking、date、ring/stamp和字符模板 | 检测Pipeline和判定不改；本切片只让已发布配方资源原子装配回原运行字段和重叠引擎 | 目标字符数、图像阈值、定位位姿和钢印资源 | 字符数等于目标且零重叠才OK；资源/定位/匹配/重叠异常均当前按NG | Overlay、统计、存图、PLC/剔除队列 | 两条件AND、失败文本与硬件时序不变 | `detection/stamp/stamp_detection_pipeline.*`+Widget资源桥 | Stage 1拆解 | 原Pipeline测试、配方选择后启停及旧模板回归 | 已验证 | S；T；U；统一DetectionCompletion帧交接及主程序集中门禁由用户确认无问题 |
| DET-003 | 字库多Profile模式 | 界面“字库匹配”（索引1）检测帧 | 至少一个完整Profile | 启动前深拷贝全部Profile定位/字符模板和参数；线程并行匹配→`ProfilePoseSelector`→运行快照Profile→`WordDetectionPipeline`→`TemplateMatch::run3` | 每Profile目标、字符图、阈值；最佳定位Profile；同分保留先出现Profile | 快照准备或原预检失败不启动；运行判定、无定位和NG路径不变 | Profile名/框显示、统计、存图、PLC | 自动选择和字符计数语义保持，仅收紧数据所有权 | 公共Selector+字库Pipeline+运行Profile快照薄桥 | Stage 1拆解 | 已发布/旧多Profile启动停止、再次启动及现有Pipeline测试 | 已验证 | S；T；U；统一DetectionCompletion帧交接及主程序集中门禁由用户确认无问题 |
| DET-004 | 深度OCR模式 | 模式2检测帧 | OCR设备已初始化、模板日期区域有效 | Widget继续准备同一ROI与目标；`OcrDetectionPipeline`改依赖`IOcrEngine`，`PaddleOcrEngine`内部仍先`DBDetector::Run`再`CRNNRecognizer::Run` | 目标文本；按字节清洗规则、换行拼接和Paddle返回顺序保持 | 清洗文本非空且精确相等OK；资源装配失败不替换当前模板 | 识别文本、统计、异步存图、PLC | 精确比较、清洗、Overlay和收尾保持，只迁移模型边界 | `detection/ocr/`+`devices/ocr/`+Widget结果桥 | Stage 2已适配 | Fake OCR Pipeline、有效模型主程序、OCR启停和结果回归 | 已验证 | S；T；U；DetectionCompletion携带同一检测原帧及主程序集中门禁由用户确认无问题 |
| DET-005 | 纸巾卷粗糙度模式 | 模式3采集线程 | 相机帧；不需传统模板 | `MyThread/CameraThread`→运行内`TissueDetectionPipeline`→`TissueRollDetector::processImage`→`slot_handleTissueResult` | 运行参数副本中的粗糙度阈值；算法找内孔、外圆和环粗糙度 | 找到卷且score<threshold为OK；空图、无圆、外轮廓失败或score>=阈值为NG并带诊断 | Overlay、统计、存图、PLC | 当前边界是`>=`判NG；结果收尾和外部副作用不变 | `recipes/product_recipe.*`+`detection/tissue/tissue_detection_pipeline.*` | Stage 1先迁移 | TISSUE三类样本；Qt Creator运行离线测试；记录score/阈值/圆框 | 已验证 | S；T；U；统一DetectionCompletion帧交接及主程序集中门禁由用户确认无问题 |
| DET-006 | 二维码优先+三期模式 | 界面“二维码+三期”（索引4）检测帧 | Profile含tracking、二维码4点、日期多边形、字符模板，DLL可用 | 启动预检与运行快照不变；`ProfilePoseSelector`→Widget薄桥→`IBarcodeDecoder::decode`→二维码优先Pipeline | DataMatrix/QR格式掩码、padding8%、预算60ms、fallback、运行内首选策略；同分保留先出现Profile | 配方装配、资源、DLL、ROI或快照预检失败不启动；读码失败短路不变 | 显示码内容/日期状态、统计、存图、PLC | “最高分Profile、读码优先、失败短路”保持；只迁移解码设备边界 | 公共Selector+二维码Pipeline+`devices/barcode/`适配器 | Stage 2已适配 | 已发布/旧Profile启动停止、再次启动、固定码及Pipeline测试 | 已验证 | S；T；U；统一DetectionCompletion帧交接及主程序集中门禁由用户确认无问题 |
| DET-007 | 定位失败收尾 | 字库家族采集时无有效pose | 已启动检测 | 软触发`MyThread`节流发失败；硬触发二维码模式逐触发发结果→`finalizeWordTrackingNg/finalizeBarcodeWordNg` | 软触发检测间隔；硬触发每个新回调帧 | 显示定位失败NG；二维码硬触发保证本次触发有收尾 | 增总数/NG、可存图、PLC或排队 | 软硬触发差异必须保持到Stage 4 | `runtime/`+Pipeline | 保留 | 移出视野：软触发观察频率；硬触发逐次打光记录结果数和PLC | 已基线 | S；U |
| DET-008 | 算法/系统失败当前统计语义 | 模板缺失、读码失败、无圆、无定位等到达收尾 | 检测已启动或启动预检 | 各Pipeline失败分支→现有NG收尾 | 当前没有独立SystemError统计 | 当前把到达正式收尾的失败计入总数和产品NG；部分启动预检失败不计数 | 影响合格率、存图和PLC | Stage 1-3保持；计划的故障分类在后续阶段处理 | `detection/types`+`runtime/result_handler` | 保留当前行为 | 对每模式失败输入记录是否计数/存图/PLC，形成固定证据 | 已基线 | S；U |

## 6. 相机、采集线程与运行控制

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CAM-001 | 扫描并打开首台相机 | “打开相机” | 无运行任务、相机未开 | `on_HandwareDetect_clicked`→`ICameraDevice::enumerateDevices`→先调`IPlcDevice::connectTo`→`openDevice(0)`→应用曝光→注册回调→启动抓图 | 保存的曝光/增益及PLC地址；固定选第0设备 | 成功进入CameraReady；无设备/打开/曝光失败提示并回滚相机；PLC失败不阻止继续开相机 | 连接PLC、创建海康设备对象、启动抓图 | 固定第0台及伴随PLC连接是当前行为 | `devices/camera/`+`devices/plc/` | Stage 2薄适配 | 0/1/多相机场景及PLC在线/离线组合，记录选择和状态 | 已基线 | S；T；U；PLC边界门禁及2026-08-13相机适配测试、主程序集中门禁均无问题 |
| CAM-002 | 关闭相机 | “关闭相机” | 非检测、非停止、非模板制作 | `on_CloseCamera_clicked`→`ICameraDevice::close`→清图/文本/计数→CameraClosed | 当前状态 | 成功释放相机并清零显示；忙碌时拒绝并提示先停止 | 关闭SDK、清UI统计和缓存 | 清理范围保持 | `devices/camera/`+UI控制器 | Stage 2薄适配 | 空闲关闭；检测中/模板中尝试关闭，核对拒绝；再正常关闭 | 已基线 | S；T；U；2026-08-13用户确认相机开关与主程序集中门禁无问题 |
| CAM-003 | 软触发采集 | 未勾PLC触发后“启动识别” | 相机开、模板预检通过 | 设置TriggerSource=7→`MyThread::run`→`ICameraDevice::executeCommand("TriggerSoftware")`→`waitForImage`→变换/定位/检测 | 约100ms循环；纸巾按配置间隔；旋转/通道 | 正常持续产生帧/结果；连续3次取图失败等路径停止/日志 | 单采集线程；无硬触发回调依赖 | 软触发频率和节流基线 | `devices/camera/`+`runtime/` | Stage 2薄适配、Stage 3迁移 | 固定运行60秒，记录帧/结果频率、失败停止和线程状态 | 已基线 | S；T；U；2026-08-13用户确认软触发启停与主程序集中门禁无问题 |
| CAM-004 | 硬触发采集 | 勾“启用触发”并启动，外部Line触发 | PLC已连、相机支持Line0 | 设备接口设置TriggerSource=0/注册原回调；`CameraThread::run`→相机接口读取回调图→定位/检测 | LineDebouncerTime=5000、TriggerDelay=0；线程sleep配置，二维码特殊 | 每个有效硬触发处理新帧；无新帧等待；初始化失败提示 | 原`CMvCamera`回调写图像/序号；线程经接口消费 | 正常硬触发行为到Stage 4前不改 | `devices/camera/`+`runtime/` | Stage 2仅适配、Stage 3迁移 | 逐次硬触发并记录frameSeq、结果数、时间；断触发观察无虚假结果 | 已基线 | S；T；U；2026-08-13用户确认相机适配主程序集中门禁无问题，现场量化证据仍延期 |
| CAM-005 | 帧读取与停止唤醒 | 采集线程调用或停止 | 相机抓图中 | `CMvCamera`原回调与条件变量保留→`HikvisionCameraDevice`委托帧读取/序号/停止唤醒→软硬触发线程 | 超时/非阻塞模式 | 返回克隆最新帧；停止请求唤醒等待；空帧/超时返回失败 | 持有最新cv::Mat和序号 | 不允许`QThread::terminate()` | `devices/camera/hikvision_camera_device.*` | Stage 2薄适配 | 连续采集、无帧超时、等待中停止，核对退出延迟 | 已基线 | S；T；U；2026-08-13用户确认适配测试和主程序停止/退出门禁无问题 |
| CAM-006 | 采集前图像变换 | 每帧进入定位/算法前 | 已设置旋转/通道 | `MyThread/CameraThread`→rotate/channel分支→定位/检测 | SET-008/009应用值 | 输出彩色或单通道派生图、指定方向；异常帧不进入正常检测 | 新cv::Mat临时内存 | 顺序与方向保持 | `detection/input_transform.*` | 保留后迁移 | 同一固定场景跑4通道×4旋转的代表组合 | 已基线 | S；U |
| RUN-001 | 启动预检与快照 | “启动识别” | 相机开、非忙碌 | 启动顺序和线程逻辑不改；二维码由`IBarcodeDecoder`、PLC由`IPlcDevice`、相机控制与线程注入由`ICameraDevice`提供，其余配方与快照检查不变 | 已应用设置；定位图、标定区、钢印环/字符和参数 | 任一配方资源装配失败不替换当前模板；设备预检失败仍按原文本不启动；启用PLC触发且未连接时仍拒绝启动 | 清统计；可能写相机和PLC参数；创建线程 | 检查顺序、失败不启动和硬件时序保持 | Widget启动薄桥+`devices/*`，后续runtime协调器 | Stage 2分步适配 | 启用PLC触发的连接/未连接启动预检；二维码DLL存在/缺失；五模式启停回归 | 已验证 | S；T；U；每次成功启动建立运行ID和结果序号，主程序集中门禁由用户确认无问题 |
| RUN-002 | 停止识别 | 顶栏“停止识别” | 检测中/线程可能运行 | `on_cancel_clicked`→Stopping→线程经共享`ICameraDevice`请求停止/等待→关闭并重新打开原第0相机→CameraReady | 等待上限；相机当前状态 | 停止后状态相机已打开，保留最后正式结果；超时仅警告 | 停/删线程，清运行Profile，不清最终结果 | 保持协作停止和展示 | `devices/camera/`+后续`runtime/inspection_coordinator.*` | Stage 2适配、Stage 3收敛 | 软/硬/五模式检测中停止，记录延迟、结果保留和再次启动 | 已基线 | S；T；U；2026-08-13用户确认相机适配主程序启停门禁无问题 |
| RUN-003 | 线程重建与信号接回 | 启动前、旧线程结束后 | 主窗存活 | `ensureThreadsReady`/`reinitializeMyThread`/`reinitializeCameraThread`→断连接→协作等待→new→注入共享`ICameraDevice`→重连信号 | 相机接口、模板、运行参数 | 新线程可再次运行；等待超时记录警告但不强杀；共享设备寿命覆盖迟退线程 | 删除/创建QThread对象和信号连接 | 不重复连接、不残留线程 | `devices/camera/`+后续`runtime/` | Stage 2适配、Stage 3优化 | 连续启动/停止10次，核对每帧只收一次结果和无残留线程 | 已基线 | S；T；U；2026-08-13用户确认相机适配主程序启停与退出门禁无问题 |
| RUN-004 | 流帧与结果内存持有 | 相机持续采集/检测完成 | 主窗活动 | 采集线程克隆Mat→Queued signal→`handleStreamingFrame`；检测缓存标注图 | 当前无显式有界FrameQueue；每次信号传值 | 正常显示；高帧率/慢UI可能形成事件积压风险 | cv::Mat/QPixmap短期或队列持有 | Stage 0记录，Stage 3引入有界队列 | `runtime/frame_queue.*` | 计划内优化 | 记录分辨率、单帧字节和10分钟内存曲线；待用户 | 已基线 | S；U |
| RUN-005 | 检测间隔与节流 | 软触发循环/组织检测 | 运行参数已应用 | `applyRuntimeThreadSettingsFromUi`→`sendDataTo`→线程`received`→间隔判断 | `cameraDelay`等现有UI/PLC值 | 按当前间隔发检测；非法参数在启动前提示 | 影响吞吐和失败NG频率 | 单位和生效时机保持 | Recipe+runtime | 保留后迁移 | 固定输入分别设边界值，记录结果间隔和提示 | 已基线 | S；U |
| RUN-006 | 最近Overlay随位姿逻辑 | 非结果绑定模式收到新pose | 有上一检测框和pose | `slot_saveBoxesFromThread`→按角差/中心差旋转平移`g_lastDrawResults/g_lastStampPoly` | 新旧`DetectionPose` | pose无效清框；有效时框跟随；字库结果绑定时直接抑制 | 更新全局Overlay缓存 | 保持模式差异 | `ui/presenters/` | 保留后拆分 | 移动/旋转产品并移出视野，核对框跟随和清除 | 已基线 | S；U |

## 7. PLC与剔除

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLC-001 | 连接PLC | 启动延迟、打开相机附带连接、或“连接PLC” | IP/Rack/Slot可用 | `Widget`三入口→`IPlcDevice::connectTo`→`Snap7PlcDevice`→`TS7Client::ConnectTo` | 默认`192.168.10.10/0/1` | 成功显示/使能状态；失败弹错误或日志，连接按钮路径不伪装成功 | 建立TCP/S7会话、写设置 | 三个入口和原顺序均保留 | `devices/plc/plc_device.h`+`snap7_plc_device.*` | Stage 2已适配 | 在线/错误IP/错误rack-slot分别走三个入口 | 已基线 | S；T；U；2026-08-13用户确认适配器测试和主程序门禁无问题；真实PLC连接仍待现场证据 |
| PLC-002 | 断开PLC | “断开PLC”或退出 | 设备接口存在 | `on_DisconnectpushButton_clicked`/析构→`IPlcDevice::disconnect`→`TS7Client::Disconnect` | 当前连接状态 | 成功断开并开放连接配置；失败提示 | 关闭PLC会话 | 保持 | `devices/plc/` | Stage 2已适配 | 在线/离线点击断开及退出，核对状态 | 已基线 | S；T；U；2026-08-13用户确认适配器测试和主程序门禁无问题；真实PLC断开仍待现场证据 |
| PLC-003 | 触发工作模式下发 | 工作模式“确认”或启动 | PLC已连接 | `applyPlcTriggerModeFromUi`→`IPlcDevice::writeDbArea`→Snap7 DB1偏移1032一字节 | 连续=0、间歇=1 | 成功保存设置；无连接/写失败提示且不更新已应用值 | PLC写`DB1.DBB1032` | 地址和值在Stage 1-3不变 | `devices/plc/` | Stage 2已适配 | PLC模拟/现场读回0、1及失败情况 | 已基线 | S；T；U；假后端已锁定Byte映射；真实PLC读回仍延期 |
| PLC-004 | 工艺参数下发 | “PLC参数→设置”或启动 | PLC已连接、整数可解析 | `applyPlcRunSettingsFromUi`→原大端编码→`IPlcDevice::writeDbArea`→Snap7 WriteArea | 剔除时间DB980 Word、剔除距离DB920 DWord、拍照时间DB982 Word、拍照距离DB924 DWord；相机延时仅线程参数 | 全部成功后保存；任一写失败提示并返回失败 | 多次PLC写入 | 地址、长度、顺序和值保持 | `devices/plc/`+后续runtime | Stage 2已适配 | 边界值、正常值和中途写失败，抓取/读回写序列 | 已基线 | S；T；U；假后端已锁定Word/DWord映射，静态核对写入顺序；真实PLC读回仍延期 |
| PLC-005 | OK输出 | 正式结果OK且PLC连接 | 检测收尾 | 结果槽→`rightremove`→`IPlcDevice::writeDbArea` | DB1偏移1033一字节，值0 | 成功复位输出；写失败警告；断开时静默返回 | PLC写0；停止100ms复位Timer | 正常OK写值保持 | `runtime/result_handler`+已适配PLC边界 | Stage 2分步迁移 | 单个OK，抓取DB1033写入次数/值；断线记录无写 | 已基线 | S；T；U；适配器映射和主程序门禁通过；真实PLC写0仍延期 |
| PLC-006 | NG脉冲输出 | 立即剔除NG | PLC连接 | 结果槽→`wrongremove`写49→成功启动100ms timer→`rightremove`写0；两次写均通过`IPlcDevice` | DB1.DBB1033；49持续约100ms | 写49成功后复位0；首次写失败不启动复位；断线静默 | PLC两次写和Timer | 值、顺序、100ms保持 | `runtime/result_handler`+已适配PLC边界 | Stage 2分步迁移 | 单个NG抓取49→约100ms→0；模拟首写/复位写失败 | 已基线 | S；T；U；适配器映射及49/100ms/0静态链通过；真实PLC脉冲仍延期 |
| PLC-007 | 延迟剔除队列与复位 | NG结果、每次产品收尾、“剔除复位” | `wrongindex`可能>0 | NG入`removalQueue(total,target=total+wrongindex)`；后续结果检查队首并`wrongremove`；按钮清队列 | 剔除位置输入；设置按钮/选模板后更新 | 到目标计数触发一次剔除；复位清除所有未发信号 | 内存队列、未来PLC脉冲 | 队列公式和清理语义保持到Stage 4 | `runtime/reject_scheduler.*` | 保留 | wrongindex=0/1/3连续样本，记录目标序号；中途复位确认无后续旧脉冲 | 已基线 | S；U |

## 8. 结果、统计与存图

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RES-001 | 总数、NG与合格率 | 每个正式检测收尾 | 结果到达UI槽 | 各模式结果槽→`totalImages++`；NG时`ngImages++`→`(1-ng/total)*100` | 当前产品和系统失败共同口径见DET-008 | 更新总数、NG数、两位小数合格率 | 内存统计/UI | 当前公式固定 | `runtime/statistics.*` | 保留到计划允许分类 | 每模式OK、NG、失败各1，记录0→3的所有显示值 | 已基线 | S；U |
| RES-002 | 总数清零 | 总数旁“清零” | 任意空闲/运行状态当前可点击性依UI状态 | `on_cut_cancelButton_2_clicked` | 无 | 总数和NG同时置0并更新两框；合格率框未在该槽显式重算 | 清内存统计 | 精确清理范围保持 | `ui/controllers/`+statistics | 保留 | 先产生多结果再清零，核对三个统计字段和下一帧 | 已基线 | S；U |
| RES-003 | NG数清零 | NG旁“清零” | 同上 | `on_cut_cancelButton_3_clicked` | 无 | 仅NG置0；总数保持；合格率未在该槽显式重算 | 改内存NG | 当前不对称行为纳入基线 | `ui/controllers/`+statistics | 保留 | 产生2NG/1OK后清NG，核对总数/NG/合格率及下一帧 | 已基线 | S；U |
| RES-004 | 检测耗时 | 五模式检测完成 | 正式检测执行 | 算法计时→`speedLabel`/结果文本 | 各模式当前计时范围不同 | 显示毫秒耗时；失败分支按其路径记录或缺失 | UI/日志 | Stage 0需固定P50/P95实际值 | `detection/result` | 保留并统一类型 | 每模式固定样本Release运行30次，记录P50/P95 | 已基线 | S；U |
| RES-005 | 当前模板名 | 选择/保存/Profile命中/停止 | 模板状态存在 | `updateCurrentTemplateName`及各模式结果槽 | 单模板目录名或命中Profile名 | 显示当前/命中模板；无模板`--`或隐藏策略 | UI | 保持名称来源 | `ui/presenters/` | 保留 | 单/多模板选择与自动命中，核对名称和停止后状态 | 已基线 | S；U |
| SAVE-001 | 按判定选择存图 | 正式结果完成 | 已设置保存模式和目录 | 五模式结果槽→`DetectionCompletion`→单个`ImageSaveTask`→`ImageSaveService::submit` | 不保存/NG/OK/全部 | 只保存策略允许的判定；队列满时等待空位，不因容量丢弃 | 向容量32存图队列提交一个产品任务 | 选择语义保持；用户确认正常检测宁可减速也不能漏存 | `runtime/image_save_service.*` | Stage 2当前切片 | 五模式各跑OK/NG，核对检测数量与应存产品文件组数相等 | 已验证 | S；T；U；统一由DetectionCompletion提供判定对应原帧，用户确认满时等待及主程序存图无问题 |
| SAVE-002 | 标注图/原图组合 | 存图被允许 | 保存类型已设置 | `shouldSaveRecognitionBoxImage/shouldSaveNoRecognitionBoxImage`→同一产品任务内按标注图、原图顺序写盘 | 两者都存/仅标注/仅原图 | 生成对应组合；一件产品的多个文件不再占多个队列名额 | 单任务短期持有标注QImage和/或只读原帧 | 组合与文件内容保持 | `runtime/image_save_service.*` | Stage 2当前切片 | 三种组合各跑1次，像素比对标注/原图及同名时间戳 | 已验证 | S；T；U；一个产品只提交一个有序存图任务，用户确认主程序无问题 |
| SAVE-003 | 目录和命名 | 任一保存任务 | 根目录可写 | UI生成原时间戳和`selectedDir/{ok,ng,ok_raw,ng_raw}`目标→存图工作线程建目录并写PNG/JPG | 当前时间和结果分类 | 成功写对应目录；创建/写入失败累计并合并提示 | 后台创建目录和写文件 | 正常路径、扩展名和命名保持 | `runtime/image_save_service.*` | Stage 2当前切片 | 核对四类目录、扩展名、同产品基名和不可写目录红色警告 | 已验证 | S；T；U；路径和命名规则不变，失败告警编码修复后用户确认无问题 |
| SAVE-004 | OCR原图来源 | OCR结果触发原图保存 | 相机仍可取图 | OCR槽→`DetectionCompletion`→`saveImage2Async`直接持有本次检测只读原帧，不再向相机另取一帧 | 本次检测帧 | 保存帧与产生OCR判定的输入帧一致；空帧拒绝并记录日志 | 后台短期持有只读帧并写盘 | 计划内修复检测与存图错帧风险 | `TrackingTypes.h`+Widget临时结果桥 | Stage 2当前切片 | 移动物体连续OCR，像素对照检测图和raw文件 | 已验证 | S；T；U；DetectionCompletion测试及主程序同帧存图门禁由用户确认无问题 |
| SAVE-005 | 异步写盘容量与失败 | 每个需保存结果 | 磁盘正常/慢/满 | `ImageSaveService`两个工作线程；在写+待写产品任务合计容量32 | 正常异步；容量满时提交者持有当前图并等待空位，不丢任务；实际写失败累计 | 短期持有最多32个服务内产品任务，另由当前提交者持有等待任务；失败向UI报警 | 队列满可降低检测吞吐；实际写失败不改变已产生的算法结论、统计或PLC | `runtime/image_save_service.*` | Stage 2当前切片 | Fake慢盘锁定满队列提交等待、腾位后全部写入、失败累计；主程序连续检测核对检测数量与文件组数 | 已验证 | S；T；U；用户确认接受存图反压，更新后的测试和主程序门禁均无问题；不增加断电恢复等持久队列 |

## 9. 多相机与独立工具

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| MC-001 | 多相机窗口入口 | 顶栏“多相机模式” | 非检测/非停止/非模板制作 | `on_MultiCameraMode_clicked`→new/show`MultiCameraWidget`；返回按钮→close | 无持久参数 | 打开独立窗口；忙碌时提示；重复点击复用现存窗口；关闭后指针清空 | 创建/销毁窗口 | 计划要求入口不被单相机重构破坏 | 原位 | 保留且延期迁移 | 空闲打开/返回/重开；检测中点击核对提示 | 已基线 | S；U；P |
| MC-002 | 多相机可见控件现状 | 多相机窗口内扫描/打开/采集/停止/触发/保存按钮 | 窗口已打开 | `multicamerawidget.ui`；`MultiCameraWidget`只初始化两行和连接“返回”，其余按钮无信号接线 | UI静态默认值 | 当前点击其余按钮无业务动作，预览/状态保持占位；不能记录为已实现功能 | 无设备/统计/PLC副作用 | 可达但未接线的真实现状 | 原位 | 计划明确延期，不在本轮补齐 | 打开窗口逐按钮点击，确认只有返回有动作并记录截图 | 已延期 | S；U；P |
| MC-003 | 双相机底层API | 当前无UI/脚本运行入口，仅编译进主工程 | 需另行代码调用 | `MultiCameraController`→2个`MultiCameraUnit`→Hikvision；`MultiCameraSyncManager`校验shotId/frameId/时间差 | 默认2台、软件触发、最大时间差5000us、要求相同frameId | API可扫描/open/start/trigger/grab；当前窗口未实例化Controller，生产统计/PLC未接入 | 若被调用会开2相机并持有帧 | 计划明确保持源码原位，不迁移/扩建 | 原位 | 延期 | 本轮只做静态零入口核对；后续独立任务建立专用测试 | 已延期 | S；P |
| TOOL-001 | 授权生成/读取工具 | 单独打开`tools/license_tool/LicenseTool.pro`构建的程序 | 与主程序相同Qt；输出目录可写 | 工具UI→`makeLicenseFile`/`licenseInfoText`，算法与RuntimeGuard同密钥/格式 | 到期日默认当前+1年；默认输出工具目录`license.ini` | 生成加密授权并立即读回；无效/不可写提示 | 写授权文件 | 独立工程必须保留 | `tools/license_tool`原位 | 保留 | Qt Creator构建工具；生成未来/过期授权并由主程序分别验证 | 已基线 | S；U |
| TOOL-002 | BarcodeDecoder.dll重建与ABI | 独立构建脚本/主程序设备接口 | VS2022+CMake+网络仅重建时；本轮Agent不执行 | `tools/barcode_decoder`→ZXing+libdmtx→固定C ABI；`BarcodeDecoderAdapter`独占DLL装载、符号解析、版本日志和返回码映射 | v2.1.0；DataMatrix+QR；libdmtx fallback15ms；静态CRT | DLL可返回码内容/角点/耗时；参数错、未找到、内部错返回固定码；主程序缺DLL预检失败 | 生成DLL；适配器管理运行时生命周期 | ABI、版本日志、错误文本和调用约定保持 | `devices/barcode/` | Stage 2已适配 | 假ABI自动测试；用户固定二维码、缺DLL和版本日志回归 | 已基线 | S；T；U；用户确认适配器测试、真实DLL加载版本日志和实际读码均正常 |

## 无独立可观察功能ID的源码候选

以下内容已检查普通调用、Qt自动槽、显式连接、定时器、回调、工程清单和动态入口。它们当前不对应独立可观察功能，因此不制造虚假功能ID；本轮也不删除。

| 候选 | 当前引用事实 | 处理决定 |
|---|---|---|
| `Zhuizong::createTrackerByName/getRandomColors` | `Widget`、`MyThread`和`CameraThread`会构造`Zhuizong`对象，但全仓没有调用这两个方法；构造本身无副作用 | 保留原文件和构造，待后续清理阶段再次做零引用确认；不得在当前基线切片删除 |
| `Widget::timer1` | 只在构造初始化，未连接、未启动、未读取 | 保留，当前无可观察行为 |
| `Widget::on_eliminatebutton_clicked` | `.ui`不存在名为`eliminatebutton`的控件，全仓无显式连接；实际剔除位置通过PLC参数应用和`wrongindex`路径生效 | 保留孤立槽，当前不登记成可达按钮功能 |
| `lineBoxIndex` | `.ui`中明确`visible=false`，源码无读写；实际合格率使用`lineBoxIndex_6` | 保留隐藏占位，不登记成当前可见功能 |
| PaddleOCR和Snap7内部实现 | 作为第三方/现有集成源码分别由DET-004和PLC-001..007的调用链覆盖，无额外用户入口 | 保留原位；本轮不把库内部辅助函数逐一伪装成业务功能 |

## 入口覆盖检查

- [x] 所有主页面、顶栏按钮、设置页按钮、输入控件、状态显示和对话框入口已从`widget.ui`反向核对。
- [x] `ImageLabel`的自适应显示、定位框、二维码框、多边形、闭合、重置、坐标换算、状态清理和事件转发已拆分登记。
- [x] 显式连接、Qt自动槽、定时器、软硬采集线程、相机回调和PLC写入已核对。
- [x] 模板制作、加载、字符切割、多Profile编辑、模式记忆、文件覆盖和失败回退已核对。
- [x] 设置默认、保存、加载、清除、恢复、dirty状态和硬件可编辑性已核对。
- [x] 五模式的正常、产品NG和失败收尾调用链已登记；固定实际样本仍待用户提供。
- [x] 统计、清零、存图组合、目录、日志、授权、单实例、部署和退出已核对。
- [x] 多相机入口/未接线控件、授权工具和BarcodeDecoder独立工程已登记。

## 状态统计

| 状态 | 数量 | 功能ID/说明 |
|---|---:|---|
| 待盘点 | 0 | 无 |
| 已基线 | 72 | 当前未进入DetectionCompletion结果帧交接切片且未达到已验证状态的功能ID |
| 迁移中 | 11 | DET-002..006、RUN-001、SAVE-001..005；五模式统一结果帧所有权和OCR同帧存图等待门禁 |
| 已验证 | 5 | SYS-006、SET-010、TPL-002、TPL-003、TPL-005；既有验证证据保留 |
| 已延期 | 2 | MC-002、MC-003；依据升级计划3.6 |
| 已确认删除 | 0 | 无删除授权 |

## 未决差异与已知基线风险

| ID | 计划规定/期望 | 源码当前行为 | 是否影响结果/硬件 | 处理决定 | 确认人/证据 |
|---|---|---|---|---|---|
| DIFF-001 | Stage 1形成纸巾阈值唯一默认6.0 | 当前代码已删除`.ui`静态5.2和检测器进程级5.2默认；`GlobalSettings`默认引用`TissueRecipeParameters`的6.0，线程启动前复制显式参数 | 计划内行为修正；可消除绕过主窗时的阈值分歧 | 2026-08-12 Agent静态核对和用户Qt Creator主程序/Pipeline测试门禁通过，差异已关闭 | 计划3.1/阶段1；SET-010/DET-005 |
| DIFF-002 | 模板阈值由Recipe唯一来源 | 私有设置和初始化代码默认70，`.ui`静态文本80 | 可能影响首次显示，但构造后通常为70 | 记录，迁移时以当前构造后实际值和模板私有值为基线 | S；TPL-013 |
| DIFF-003 | 未来系统故障不进入产品质量分母 | 当前到达正式收尾的读码失败、无定位、无纸卷等多按产品NG计数并可能触发PLC | 是 | Stage 1-3保持当前；硬件/故障策略不得提前进入Stage 4 | 计划3.5/阶段4；DET-008 |
| DIFF-004 | 检测帧通过`DetectionCompletion`短期交接 | 当前切片已删除OCR结果后另取相机帧，五模式统一携带判定对应原帧 | 修复存图对应性，不改判定 | Stage 2实现完成，等待Qt Creator门禁后关闭差异 | SAVE-004 |
| DIFF-005 | 存图队列容量32、双写线程且满时反压不丢图 | 原实现每图一次`QtConcurrent::run`，无统一容量；首版容量8拒新在实际运行中累计漏存24个任务 | 影响内存/吞吐；用户确认正常生产宁可降低检测速度也不能漏存 | Stage 2实现满时等待；连续检测核对结果数与文件组数 | SAVE-005 |
| DIFF-006 | 多相机本轮保持原状 | 窗口可打开，但除“返回”外可见按钮均未接Controller；底层类无当前运行入口 | 不影响单相机；误认为可用会影响操作预期 | 明确登记并按计划延期，不补做多相机开发 | 计划3.6；MC-001..003 |
| DIFF-007 | 相机和PLC边界后续分离 | “打开相机”会先尝试连接PLC，PLC失败仍继续开相机 | 影响设备操作时序 | Stage 1-3保持，Stage 2只用适配器复现现有顺序 | CAM-001/PLC-001 |

## 基线资源

| 资源 | 路径/来源 | 覆盖功能 | 可重复条件 | 当前状态 |
|---|---|---|---|---|
| 五模式样本清单 | `tests/baseline/sample_manifest.tsv` | DET-002..008、RES、SAVE、PLC | 用户填写每模式OK/NG/FAILURE的固定图、模板、文本、Overlay、计数、存图、PLC和耗时 | 已建清单，15份实际证据待用户 |
| 纸巾离线基线测试 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | SET-010、DET-005 | Qt Creator打开`tests/tests.pro`，Run qmake、Build并运行测试 | 2026-08-12用户确认纸巾Pipeline切片的4项业务测试全部通过，汇总`6 passed, 0 failed` |
| 生产主程序构建 | `app/AutoOCRproject.pro` | 全部主程序功能 | Qt 5.14.2/MSVC2017 x64 Release，Run qmake、Rebuild、Run | 2026-08-12用户先后确认含纸巾Pipeline和深度OCR Pipeline的主程序正常构建运行；纸巾阈值与OCR模式切换正常 |
| 运行与性能记录 | `docs/development/OCRGangYin重构执行记录.md` | 内存、P50/P95、慢盘、停止/重启 | 用户按执行记录步骤填写真实数值 | 待用户验证 |
