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
| SYS-006 | OCR模型初始化 | `Widget`构造 | `config1.txt`、模型和字典存在 | `Widget::Widget`→`OCRConfig`→`DBDetector`/`Classifier`/`CRNNRecognizer`构造 | 配置相对路径以配置文件目录解析；启动生效 | 模型加载成功后OCR可用；配置键/模型异常可能在构造期失败 | 加载Paddle模型并占用内存 | 保持模型、阈值和路径解析 | `detection/ocr/`+`startup/` | 保留后抽离 | 有效部署启动；再在副本中缺失模型启动并记录失败表现 | 已基线 | S；U |
| SYS-007 | 公共设置与模板恢复 | `Widget`构造 | 用户AppData可读 | `Widget::loadSettings`→`AppSettingsManager::loadGlobalSettings`→`applyGlobalSettingsToUi`→`restoreTemplatesForMode` | `AppDataLocation/settings.ini`，配置v2；无效时默认 | 恢复模式、保存、相机/PLC、模板历史和分隔条；读失败使用默认并记日志 | 读取设置和模板资源 | 详见SET/TPL功能ID | `recipes/`+`system_support/settings/` | 保留后拆分 | 修改并应用设置、退出重启，逐项核对；损坏INI核对默认回退 | 已基线 | S；U |
| SYS-008 | 启动PLC延迟连接 | 主窗构造后1秒 | Snap7客户端存在 | `Widget::Widget`→`QTimer::singleShot(1000)`→`TS7Client::ConnectTo` | 保存的IP/Rack/Slot；1秒后生效 | 成功连接并刷新硬件控件；失败仅日志/状态，不阻止主窗 | 建立PLC网络连接 | Stage 1-3不改变连接时序 | `devices/plc/` | 保留到Stage 2适配 | 有PLC/无PLC各启动一次，记录1秒后状态和可编辑控件 | 已基线 | S；U |
| SYS-009 | 正常退出与资源释放 | 关闭主窗/进程退出 | 可有运行线程、相机、PLC | `Widget::closeEvent`/析构→协作停止线程→关闭相机→断开PLC；`aboutToQuit`→`cv::destroyAllWindows` | 线程等待上限和当前设备状态 | 正常关闭；线程未及时退出仅记录警告，不调用`terminate()` | 停线程、关设备、断PLC、释放OpenCV窗 | 必须保持协作停止、无残留线程 | `runtime/`+`startup/` | 保留并在Stage 3收敛 | 检测中、模板预览中、空闲时分别关闭；确认提示、进程退出和设备释放 | 已基线 | S；U |
| SYS-010 | Release运行时部署校验 | Qt Creator Release链接后 | `dist/ShengYin`完整 | `AutoOCRproject.pro::QMAKE_POST_LINK`→`deploy_runtime.ps1` | 源`dist/ShengYin`、目标构建`release`；Release链接后 | 校验清单/哈希并复制DLL、模型、配置；缺失或不一致使部署脚本失败 | 写Release运行目录 | 保持部署可复现；构建只由用户执行 | `system_support/deployment/` | 保留 | Qt Creator Run qmake+Release Rebuild，核对部署结果和脚本报错 | 已基线 | S；U |

## 2. 主界面与交互

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| UI-001 | 模式与参数页面 | “识别模式”下拉框 | 非检测/非模板忙碌状态 | `comboBox_4::currentIndexChanged`→`setupDetectModeChangeTracking`→存旧模式路径→`restoreTemplatesForMode`→更新可见参数 | 界面固定顺序：模板匹配、字库匹配、深度模型、纸巾检测、二维码+三期；内部ID依次为stamp/word/ocr/tissue/barcode_word | 切换对应参数和模板历史；无效历史保留空状态并可提示 | 保存当前模式和模板历史 | 五个现有入口不可丢失 | `ui/pages/` | 保留 | 依次切换五模式，核对控件显隐、模板名、历史恢复 | 已基线 | S；U |
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
| SET-001 | 全局设置读取与安全保存 | 启动及各“设置/确认”按钮 | AppData可访问 | `AppSettingsManager::loadGlobalSettings/saveGlobalSettings`→临时文件验证→替换/回滚 | 配置版本2；当前用户AppData | 合法值加载；未知ID归一化；保存失败保留旧文件并报告/日志 | 读写`settings.ini`及临时/备份 | 整机设置语义保持 | `system_support/settings/` | 保留后封装 | 保存、重启；损坏配置；模拟不可写目录，核对回退 | 已基线 | S；U |
| SET-002 | 整机默认值 | 首次启动或恢复默认 | 无有效全局设置 | `defaultGlobalSettings`→`applyGlobalSettingsToUi` | 字库模式、全不保存、仅标注、彩色、无旋转、触发启用、间歇、曝光800、增益1、PLC `192.168.10.10/0/1`、纸巾6.0等 | UI采用默认；硬件未必立即写入 | 默认可被保存 | Stage 1仅按计划统一纸巾唯一来源 | `recipes/machine_settings.*` | 保留/计划内优化 | 备份并清除设置后启动，逐项记录；不覆盖用户现有文件 | 已基线 | S；U |
| SET-003 | 模板私有设置 | 选择、保存、编辑模板 | 模板目录可读写 | 旧INI字段继续经`template_profile_mapper`进入规范化`RecipeProfile`；每Profile建立资产清单后，新`template_recipe_assembler`可按顺序组装完整候选配方 | 组装保留配方UUID、名称、模式、Profile顺序/参数，并合并资产键、目标相对路径和源路径 | 缺少Profile、必需资产、源映射或发生键/目标冲突时拒绝，失败不改写输出对象 | 旧入口继续读写模板INI；候选组装仅内存数据，无新磁盘写入 | 后续迁入Recipe，不要求兼容旧格式 | `recipes/template_profile_mapper.*`+`recipes/template_profile_assets.*`+`recipes/template_recipe_assembler.*` | Stage 1分步迁移 | 多Profile组装/冲突/失败不改写测试；主工程字库家族模式门禁 | 已基线 | S；T；U；2026-08-12用户确认配方组装测试及主工程模式切换全部通过，旧INI和旧入口仍为当前事实来源 |
| SET-004 | 未应用标记与启动确认 | 编辑带绑定的参数 | 参数可编辑 | `setupGlobalSettingBindings`/模板dirty跟踪→标签加`*`→启动`dirtySettingsMessage` | UI值与`m_appliedGlobalSettings`/Profile比较 | 启动前列出未应用项；取消不启动；继续会恢复已应用值后运行 | 标签变化；可能丢弃未应用UI值 | 保持“应用”和“编辑”边界 | `ui/settings_controller.*` | 保留 | 修改相机、PLC、阈值但不确认，启动后分别选取消/继续 | 已基线 | S；U |
| SET-005 | 运行中参数禁用 | 检测/模板状态变化 | 硬件或模板操作进行中 | `updateHardwareParameterUiEnabled`+`registerHardwareAction` | 相机打开、PLC连接、操作状态 | 运行中禁止会改变设备/关键参数的控件；不同连接状态允许不同字段 | 控件状态改变 | 防止中途改运行快照 | `ui/settings_controller.*` | 保留 | 在空闲、相机开/关、PLC连/断、检测中逐项核对 | 已基线 | S；U |
| SET-006 | 相机曝光应用 | 曝光“设置”或检测启动 | 相机已打开 | `on_sureButton_clicked`→`queryCameraExposureRange`→`applyCameraExposureValue` | 整数曝光；相机SDK给最小/最大；默认800 | 范围内写入；越界/SDK失败提示；打开相机时保存值会按范围调整并提示 | 写相机`ExposureTime`；成功保存设置 | 数值与生效时机保持 | `devices/camera/`+Recipe | 保留后适配 | 最小、最大、越界、正常值各一次；重启开相机核对 | 已基线 | S；U |
| SET-007 | 相机增益应用 | 增益“设置”或检测启动 | 相机已打开 | `on_pushButton_12_clicked`→`applyCameraGainFromUi`→SDK | 整数；SDK范围；默认1 | 合法写入并保存；空/越界/SDK失败提示 | 写相机`Gain` | 保持 | `devices/camera/`+Recipe | 保留后适配 | 边界/越界/正常值，重启核对 | 已基线 | S；U |
| SET-008 | 颜色通道应用 | “颜色通道→确认” | 线程存在 | `on_pushButton_7_clicked`→`emit choosechannel`→`MyThread/CameraThread::receivecolorchannel*` | 彩色/红/绿/蓝；默认彩色 | 后续帧按选定通道处理；无效索引回彩色 | 更新线程参数并保存 | 通道映射保持 | Recipe+detection input transform | 保留后迁移 | 同一固定彩色场景切换四项，记录处理图与重启恢复 | 已基线 | S；U |
| SET-009 | 图像旋转应用 | “图像旋转→设置” | 线程存在 | `on_pushButton_9_clicked`→`emit rotate`→采集线程旋转 | 无/顺90/逆90/180；默认无 | 后续采集图旋转；无效索引回无 | 更新线程参数并保存 | 旋转方向保持 | Recipe+detection input transform | 保留后迁移 | 带方向标记固定场景切四项，核对方向和重启恢复 | 已基线 | S；U |
| SET-010 | 纸巾粗糙度阈值 | 纸巾模式“设置”及启动 | 值>0 | `applyTissueRoughnessThresholdFromUi/applyRuntimeThreadSettingsFromUi`→显式构造`TissueRecipeParameters`→启动前复制到`MyThread/CameraThread` | 唯一代码默认由`TissueRecipeParameters`提供6.0；有已保存整机设置时使用保存值 | 合法值固定为本次运行线程的参数副本；非法提示且不启动 | 保存整机设置；不再更新进程级检测器默认 | Stage 1统一唯一6.0来源并改为运行副本 | `recipes/product_recipe.*` | 计划内优化 | 冷启动、无设置、已有设置分别记录UI阈值；运行离线测试 | 已验证 | S；T；U；2026-08-12用户Qt Creator Release主程序启动/退出码0、纸巾阈值显示正常，Pipeline测试`6 passed, 0 failed` |
| SET-011 | 存图策略设置 | 保存模式、类型、路径浏览 | 主窗空闲 | UI改变→`syncImmediateGlobalSettingsFromUi`→`saveSettings`；浏览按钮选目录 | 不保存/NG/OK/全部；两类都存/仅标注/仅原图；默认不保存+仅标注 | 选项控制后续存图；未选目录时依现有路径逻辑；浏览取消不变 | 写全局设置 | 详见SAVE功能 | `recipes/save_policy.*` | 保留后迁移 | 逐组合选择、重启，核对显隐和实际文件 | 已基线 | S；U |
| SET-012 | 清空当前软件数据 | 设置页按钮 | 用户二次确认 | `clearCurrentSoftwareData`→删除全局设置→重置UI/状态 | 只针对当前用户软件数据 | 确认后恢复默认公共设置；取消不变；失败提示 | 删除`settings.ini`；不删除模板、图片、授权、日志 | 删除范围必须保持 | `system_support/settings/` | 保留 | 在测试用户数据中确认/取消各一次，核对保留项 | 已基线 | S；U |
| SET-013 | 恢复默认设置 | 设置页按钮 | 用户确认；设备状态决定可立即应用项 | `restoreDefaultGlobalSettings`→按相机/PLC连接状态选择性恢复→dirty刷新 | `defaultGlobalSettings` | 可立即项恢复；不能立即写硬件项保持`*`待应用并提示 | 改UI/已应用设置，可能写设置 | 状态相关语义保持 | `ui/settings_controller.*` | 保留 | 相机开/关、PLC连/断四组合执行并核对星号/提示 | 已基线 | S；U |

## 4. 模板制作、加载与编辑

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| TPL-001 | 实时模板预览 | “制作模板”第一次点击 | 相机已打开、未检测 | `on_VideoShoot_clicked`→`startTemplatePreview`→软触发`MyThread`→`signal_templatePreviewFrame` | 当前曝光/增益/旋转/通道 | 显示实时画面和引导；相机/线程错误提示并退出预览 | 启动采集线程；不统计/PLC | 预览入口保持 | `ui/template_editor/` | 保留后抽离 | 五模式分别进入预览，核对状态、按钮、实时画面 | 已基线 | S；U |
| TPL-002 | 冻结、重拍与退出 | 预览中再次点制作模板；已有选择时重拍；退出按钮/Esc | 有最近预览帧 | `freezeTemplatePreview`/`resetTemplateCaptureState`/`stopTemplatePreview` | 最近克隆帧 | 冻结当前帧并允许绘图；重拍有确认；退出清理选择并回相机就绪 | 协作停止预览线程、清缓存 | 保持状态转换 | `ui/template_editor/` | 保留 | 冻结、重拍取消/确认、Esc/退出各一次 | 已基线 | S；U |
| TPL-003 | 定位矩形绘制 | 冻结模板图后左键拖动 | 钢印/字库/二维码模式 | `ImageLabel::mousePress/Move/Release`→`m_trackingRect`→引导事件 | 显示坐标，保存时换算到原图 | 形成归一化定位框；过小/无框不能完成保存 | 仅UI选择状态 | 独立登记ImageLabel行为 | `ui/widgets/image_label.*` | 保留后移动 | 不同比例窗口画框，核对显示与保存后物理框 | 已基线 | S；U |
| TPL-004 | 二维码矩形与即时读码 | 二维码模式第二个矩形 | 已有定位框 | `ImageLabel`→`validateBarcodeTemplateRect`→`decodeBarcodeRoi` | BarcodeOptions默认DataMatrix、padding8%、预算60ms、fallback开 | 可读则保留框并继续日期多边形；不可读弹原因、清二维码/日期但保留定位框 | 动态加载/调用`BarcodeDecoder.dll` | 二维码必须先验证 | `ui/template_editor/`+`detection/barcode/` | 保留 | 可读、不可读、越界二维码框各一次，核对清理范围 | 已基线 | S；U |
| TPL-005 | 日期多边形绘制与闭合 | 左键逐点、右键闭合 | 已有前置框 | `ImageLabel`多边形状态→`signal_templateGuideEvent` | 至少3点 | 闭合后可保存/提示；点数不足保持绘制；Esc清理当前选择 | UI状态 | 保持鼠标/键盘语义 | `ui/widgets/image_label.*` | 保留后移动 | 2点右键、3+点右键、Esc，核对状态与提示 | 已基线 | S；U |
| TPL-006 | 坐标换算与通用模板保存 | “保存模板” | 必需框完整，用户给产品名和根目录 | 旧保存仍先写原目录并重载；新组装器将已规范化Profile与资产清单合并为`ProductRecipe`+`assetSourcePaths`对 | 旧入口使用产品目录名；候选配方保留调用方提供的UUID/显示名/模式，并建立`assets/profiles/<index>/...`完整映射 | 旧保存/覆盖/重载结果不变；组装不执行RecipeStore复制或提交 | 旧目录副作用不变；只新增内存候选配方与源资产映射 | 计划内改为Recipe整目录安全保存 | `recipes/template_recipe_assembler.*`+`recipes/recipe_store.*` | Stage 1分步迁移 | 组装器成功/失败合同测试；原保存入口与主工程保持 | 已基线 | S；T；U；2026-08-12用户确认组装测试与主工程门禁通过，候选数据已具备交给RecipeStore的形状，旧保存入口未切换 |
| TPL-007 | 钢印环与钢印区域标定 | 钢印模板保存过程中 | 通用框已保存 | `on_pushButton_5_clicked`→`getQuickRectROI`→`getPolygonROI`→写`template_ring.bmp`和`stamp_poly`→`OverlapDetector::init` | OpenCV交互ROI；相对吸管口中心坐标 | 有效ROI初始化重叠引擎；无效选择不生成完整钢印资源 | 额外写模板/YAML，打开OpenCV交互窗 | 保持标定次序与坐标 | `recipes/stamp/` | 保留后迁移 | 有效/取消/过小ROI，核对文件、YAML和后续检测 | 已基线 | S；U |
| TPL-008 | 单模板选择与校验 | “选择模板”非字库家族 | 空闲状态 | 旧`on_pushButton_4_clicked`仍调用`loadSettingsFromDir`；新`RecipeStore::loadRecipe`按UUID读取`recipe.json`并由`ProductRecipe`校验完整Profile及资源引用 | 旧目录私有配置和资源；新格式为Schema 1、类型化Profile及`assets/` | 新Store仅在JSON、ID、Profile字段、资源引用和全部文件均有效时返回配方，失败不修改输出对象；旧选择入口暂不变 | 新Store只读配方目录；旧入口仍更新历史和引擎 | 保持失败不误启动，后续切换新接口 | `recipes/product_recipe.*`+`recipes/recipe_store.*` | Stage 1分步迁移 | 两个配方测试复验全部PASS，主窗口启动正常；原“选择模板”入口留到后续接入验证 | 已基线 | S；T；U；Profile合同基础门禁通过；旧选择入口尚未迁移 |
| TPL-009 | 字库家族多Profile选择 | “选择模板”字库/二维码模式，可勾多目录 | 空闲状态 | 逐目录加载旧Profile→保留有效项→按最终加载顺序生成资产清单；新组装器以该顺序生成单份多Profile候选配方 | Profile顺序继续决定检测优先级；组装不重排Profile，每个Profile引用自己的资产命名空间 | 至少一个有效仍加载；全无效仍保留旧模板；候选组装失败不参与旧成功判定 | 旧Mat/配置/历史副作用不变；新增纯内存配方候选组装边界 | 保持部分成功策略 | `recipes/template_profile_assets.*`+`recipes/template_recipe_assembler.*`，后续`recipes/profile_repository.*` | 保留后迁移 | 两Profile顺序/参数/资产合并及失败不改写测试；主程序模式切换 | 已基线 | S；T；U；2026-08-12用户确认组装测试与模式切换通过，旧选择和部分成功策略不变 |
| TPL-010 | 当前Profile编辑器 | 多Profile加载后下拉选择 | 至少一个Profile | `refreshWordTemplateEditorCombo`→`setCurrentWordTemplateEditIndex`→应用目标/阈值/原图 | Profile顺序和目录名 | 切换只改变当前编辑对象，不改变其余Profile | UI切换，持有Profile缓存 | 保持编辑对象边界 | `ui/template_editor/` | 保留 | 加载2+Profile，往返切换核对文本、阈值、原图 | 已基线 | S；U |
| TPL-011 | 单Profile目标字符 | “确认字符” | 字库家族且当前Profile有效 | `on_textsure_btn_clicked`→解析基本字符/变体→保存私有设置→加载字符模板→刷新旧缓存及规范化RecipeProfile缓存 | `dateEdit`；允许括号变体命名 | 全部模板存在则成功；缺图片或写入失败仍不更新任何缓存 | 写Profile配置、加载Mat；成功后同步内存RecipeProfile | 保持字符解析和部分失败提示 | `recipes/word/`+`recipes/template_profile_mapper.*` | 保留后迁移 | 映射往返测试`7 passed, 0 failed`；主工程正常 | 已基线 | S；T；U；成功分支同步新缓存的构建门禁通过，原按钮行为未替换 |
| TPL-012 | 批量目标字符 | “批量确认字符” | 已加载多个Profile | `on_batchTextsure_btn_clicked`→逐Profile保存并重载→每个成功项同步RecipeProfile缓存 | 当前输入应用所有选择Profile | 汇总成功/失败；失败Profile不更新旧或新缓存 | 批量写多个配置、重载缓存 | 保持逐项结果 | `recipes/word/`+`recipes/template_profile_mapper.*` | 保留后迁移 | 映射往返测试`7 passed, 0 failed`；主工程正常 | 已基线 | S；T；U；逐项成功同步新缓存的构建门禁通过，原批量入口未替换 |
| TPL-013 | 单Profile图像阈值 | 阈值“设置” | 当前Profile有效 | `on_pushButton_3_clicked`→校验0..100→保存旧私有设置→成功后同步旧缓存及RecipeProfile缓存 | 整数百分比；私有默认70；UI初始文件80但初始化代码设70 | 合法保存并同步；非法或写失败不更新缓存 | 写私有配置；成功后同步内存RecipeProfile | 保持百分比语义；记录重复默认差异 | `recipes/word/`+`recipes/template_profile_mapper.*` | 保留后迁移 | 映射往返测试`7 passed, 0 failed`；主工程正常 | 已基线 | S；T；U；成功分支同步新缓存的构建门禁通过，原阈值入口未替换 |
| TPL-014 | 批量图像阈值 | “批量设置阈值” | 多Profile已加载 | `on_batchImageThresholdButton_clicked`→逐Profile保存→每个成功项同步RecipeProfile缓存 | 同一0..100整数 | 汇总逐项成功/失败；失败项不更新缓存 | 批量写配置；成功项同步内存RecipeProfile | 保持 | `recipes/word/`+`recipes/template_profile_mapper.*` | 保留后迁移 | 映射往返测试`7 passed, 0 failed`；主工程正常 | 已基线 | S；T；U；成功项同步新缓存的构建门禁通过，原批量入口未替换 |
| TPL-015 | 手动字符模板切割 | “分割字符模板” | 字库家族、Profile有原图/定位框/date_poly | 原切割保存/重载成功→同步RecipeProfile字段→重新枚举字符图片并刷新全部Profile资产命名空间 | 框和变体命名仍按旧规则；资产清单保留实际文件名及排序 | 原保存、失败和取消行为不变；只有重载成功才刷新内存资产清单 | 原配置/字符图写入不变；新增内存清单刷新 | 保持绘图、排序、变体和清理范围 | `ui/template_editor/`+`recipes/template_profile_assets.*` | 保留后迁移 | `A/A(1)/A_2`等枚举测试；原切割入口保持 | 已基线 | S；T；U；2026-08-12用户确认资产清单测试及主工程门禁通过，原切割行为不变 |
| TPL-016 | 按模式记忆模板 | 切换模式、选择/保存模板、退出 | 全局设置可写 | `storeCurrentTemplatePathsForMode`/`currentTemplatePathsForMode`/`restoreTemplatesForMode` | `templateDirPathsByMode`；字库为多路径，其余单路径 | 切回模式恢复上次路径；失效路径不当作可用模板 | 写全局设置、加载模板资源 | 保持模式隔离 | `recipes/repository/` | 保留后迁移 | 五模式各选不同目录，重启并切换核对 | 已基线 | S；U |

## 5. 五种检测与共同算法行为

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| DET-001 | 共同定位与位姿 | 软/硬采集线程获得帧 | 非纸巾模式且模板有效 | `MyThread/CameraThread`→`TrackingPoseMatcher::setTemplate/match`→`DetectionPose`→`dispatchDetectionByMode` | tracking模板；-45..45度、步长2、金字塔0.2、阈值0.3 | 匹配成功映射日期/二维码多边形；失败产生对应NG路径或跳过 | 线程持有模板旋转缓存；无直接PLC | 定位范围、阈值和坐标变换保持 | `detection/common/pose_matcher.*` | 保留后迁移 | 固定角度/位移/无目标样本，记录pose、Profile和结果 | 已基线 | S；U |
| DET-002 | 模板匹配模式（内部钢印+字符检测） | 界面“模板匹配”（索引0）检测帧 | 有tracking、date、ring/stamp和字符模板 | `dispatchDetectionByMode`→`slot_readAndDetect3`→`TemplateMatch::run3`+`OverlapDetector::processImage`→合并判定 | 目标字符数、图像阈值、定位位姿和钢印资源 | 字符数等于目标且零重叠才OK；资源/定位/匹配/重叠异常均当前按NG | Overlay、统计、存图、PLC/剔除队列 | 两条件AND及失败文本需固定样本锁定 | `detection/stamp/stamp_detection_pipeline.*` | Stage 1拆解 | Pipeline纯逻辑测试；主程序“模板匹配”入口；STAMP固定样本风险按用户决定延期 | 已验证 | S；T；U；2026-08-12 14:58测试`6 passed, 0 failed`、退出码0；用户截图确认主程序正常运行且界面“模板匹配”入口存在；原算法和结果收尾保持旧入口 |
| DET-003 | 字库多Profile模式 | 界面“字库匹配”（索引1）检测帧 | 至少一个完整Profile | 采集线程并行匹配所有Profile→`ProfilePoseSelector`严格选最高score→`runWordTemplateDetection`→`WordDetectionPipeline`窄回调→`TemplateMatch::run3` | 每Profile目标、字符图、阈值；最佳定位Profile；同分保留先出现Profile | 匹配字符数等于目标数OK；否则NG；无定位按节流策略生成NG | Profile名/框显示、统计、存图、PLC | 自动选择和字符计数语义保持 | `detection/common/profile_pose_selector.*`+`detection/word/word_detection_pipeline.*` | Stage 1拆解 | Profile选择与Pipeline纯逻辑测试；主程序“字库匹配”入口；固定样本风险按用户决定延期 | 已验证 | S；T；U；2026-08-12 17:19 Profile选择测试`6 passed, 0 failed`、退出码0；用户确认主工程正常；同分顺序、Profile索引和二维码多边形映射由公共Selector锁定 |
| DET-004 | 深度OCR模式 | 模式2检测帧 | OCR模型已加载、模板日期区域有效 | `dispatchDetectionByMode`→`slot_readAndDetect`→旋转裁剪日期ROI→`DBDetector::Run`→`CRNNRecognizer::Run`→清洗/换行拼接→非空且精确比较 | 目标文本；按字节保留ASCII字母数字、所有高位字节及`- . :`；非空行用`\n`拼接 | 清洗拼接文本非空且与目标完全相等OK；空或不等为NG；无效图/日期ROI当前直接返回 | 识别文本、统计、异步存图、PLC | 精确比较、字节清洗、无OCR框Overlay和现有收尾保持 | `detection/ocr/ocr_detection_pipeline.*` | Stage 1拆解 | OCR纯逻辑测试；原入口OCR样本记录原识别列表、清洗文本和最终判定 | 已验证 | S；T；U；2026-08-12 14:24 OCR Pipeline测试及纸巾子工程回归均`6 passed, 0 failed`；用户随后确认主程序Run qmake/Rebuild/Run及OCR模式切换通过 |
| DET-005 | 纸巾卷粗糙度模式 | 模式3采集线程 | 相机帧；不需传统模板 | `MyThread/CameraThread`→运行内`TissueDetectionPipeline`→`TissueRollDetector::processImage`→`slot_handleTissueResult` | 运行参数副本中的粗糙度阈值；算法找内孔、外圆和环粗糙度 | 找到卷且score<threshold为OK；空图、无圆、外轮廓失败或score>=阈值为NG并带诊断 | Overlay、统计、存图、PLC | 当前边界是`>=`判NG；结果收尾和外部副作用不变 | `recipes/product_recipe.*`+`detection/tissue/tissue_detection_pipeline.*` | Stage 1先迁移 | TISSUE三类样本；Qt Creator运行离线测试；记录score/阈值/圆框 | 已验证 | S；T；U；2026-08-12纸巾Pipeline的6.0默认、显式阈值、空图和纯黑图4项业务测试全部通过，汇总`6 passed, 0 failed` |
| DET-006 | 二维码优先+三期模式 | 界面“二维码+三期”（索引4）检测帧 | Profile含tracking、二维码4点、日期多边形、字符模板，DLL可用 | 采集线程并行匹配→`ProfilePoseSelector`严格选最高score并映射二维码多边形→`runBarcodeWordDetection`→组合旋转ROI→`decodeBarcodeRoi`→`BarcodeWordDetectionPipeline`→成功后`runWordTemplateDetection` | DataMatrix/QR格式掩码、padding8%、预算60ms、fallback、缓存首选策略；同分保留先出现Profile | 读码失败立即NG且不执行日期；读码成功再做三期，二者共同形成结果 | 显示码内容/日期状态、统计、存图、PLC | “最高分Profile、读码优先、失败短路”保持 | `detection/common/profile_pose_selector.*`+`detection/barcode_word/barcode_word_detection_pipeline.*` | Stage 1拆解 | Profile选择与Pipeline纯逻辑测试；主程序“二维码+三期”入口；固定样本风险按用户决定延期 | 已验证 | S；T；U；2026-08-12 17:19 Profile选择测试`6 passed, 0 failed`、退出码0；用户确认主工程正常；ROI、DLL、缓存、结果收尾和硬件主链未改 |
| DET-007 | 定位失败收尾 | 字库家族采集时无有效pose | 已启动检测 | 软触发`MyThread`节流发失败；硬触发二维码模式逐触发发结果→`finalizeWordTrackingNg/finalizeBarcodeWordNg` | 软触发检测间隔；硬触发每个新回调帧 | 显示定位失败NG；二维码硬触发保证本次触发有收尾 | 增总数/NG、可存图、PLC或排队 | 软硬触发差异必须保持到Stage 4 | `runtime/`+Pipeline | 保留 | 移出视野：软触发观察频率；硬触发逐次打光记录结果数和PLC | 已基线 | S；U |
| DET-008 | 算法/系统失败当前统计语义 | 模板缺失、读码失败、无圆、无定位等到达收尾 | 检测已启动或启动预检 | 各Pipeline失败分支→现有NG收尾 | 当前没有独立SystemError统计 | 当前把到达正式收尾的失败计入总数和产品NG；部分启动预检失败不计数 | 影响合格率、存图和PLC | Stage 1-3保持；计划的故障分类在后续阶段处理 | `detection/types`+`runtime/result_handler` | 保留当前行为 | 对每模式失败输入记录是否计数/存图/PLC，形成固定证据 | 已基线 | S；U |

## 6. 相机、采集线程与运行控制

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CAM-001 | 扫描并打开首台相机 | “打开相机” | 无运行任务、相机未开 | `on_HandwareDetect_clicked`→枚举GigE/USB→先尝试PLC连接→`CMvCamera::Open`第0台→应用曝光→注册回调→StartGrabbing | 保存的曝光/增益及PLC地址；固定选第0设备 | 成功进入CameraReady；无设备/打开/曝光失败提示并回滚相机；PLC失败不阻止继续开相机 | 连接PLC、创建相机对象、启动抓图 | 固定第0台及伴随PLC连接是当前行为 | `devices/camera/` | 保留后Stage 2适配 | 0/1/多相机场景及PLC在线/离线组合，记录选择和状态 | 已基线 | S；U |
| CAM-002 | 关闭相机 | “关闭相机” | 非检测、非停止、非模板制作 | `on_CloseCamera_clicked`→`CMvCamera::Close`→delete→清图/文本/计数→CameraClosed | 当前状态 | 成功释放相机并清零显示；忙碌时拒绝并提示先停止 | 关闭SDK、清UI统计和缓存 | 清理范围保持 | `devices/camera/`+UI控制器 | 保留后适配 | 空闲关闭；检测中/模板中尝试关闭，核对拒绝；再正常关闭 | 已基线 | S；U |
| CAM-003 | 软触发采集 | 未勾PLC触发后“启动识别” | 相机开、模板预检通过 | 设置TriggerSource=7→`MyThread::run`→`CommandExecute("TriggerSoftware")`→`timesGetImage`→变换/定位/检测 | 约100ms循环；纸巾按配置间隔；旋转/通道 | 正常持续产生帧/结果；连续3次取图失败等路径停止/日志 | 单采集线程；无硬触发回调依赖 | 软触发频率和节流基线 | `devices/camera/`+`runtime/` | Stage 2/3迁移 | 固定运行60秒，记录帧/结果频率、失败停止和线程状态 | 已基线 | S；U |
| CAM-004 | 硬触发采集 | 勾“启用触发”并启动，外部Line触发 | PLC已连、相机支持Line0 | 设置TriggerSource=0、回调、`CameraThread::run`→新frameSeq→读取回调图→定位/检测 | LineDebouncerTime=5000、TriggerDelay=0；线程sleep配置，二维码特殊 | 每个有效硬触发处理新帧；无新帧等待；初始化失败提示 | 相机回调写`m_image/m_frameseq`；线程消费 | 正常硬触发行为到Stage 4前不改 | `devices/camera/`+`runtime/` | Stage 2/3仅适配 | 逐次硬触发并记录frameSeq、结果数、时间；断触发观察无虚假结果 | 已基线 | S；U |
| CAM-005 | 帧读取与停止唤醒 | 采集线程调用或停止 | 相机抓图中 | `CMvCamera`回调克隆图→条件变量；`timesGetImage`等新帧；`requestStop`置原子并notify | 超时/非阻塞模式 | 返回克隆最新帧；停止请求唤醒等待；空帧/超时返回失败 | 持有最新cv::Mat和序号 | 不允许`QThread::terminate()` | `devices/camera/hikvision_adapter.*` | 保留后适配 | 连续采集、无帧超时、等待中停止，核对退出延迟 | 已基线 | S；U |
| CAM-006 | 采集前图像变换 | 每帧进入定位/算法前 | 已设置旋转/通道 | `MyThread/CameraThread`→rotate/channel分支→定位/检测 | SET-008/009应用值 | 输出彩色或单通道派生图、指定方向；异常帧不进入正常检测 | 新cv::Mat临时内存 | 顺序与方向保持 | `detection/input_transform.*` | 保留后迁移 | 同一固定场景跑4通道×4旋转的代表组合 | 已基线 | S；U |
| RUN-001 | 启动预检与快照 | “启动识别” | 相机开、非忙碌 | `on_plcbtn_clicked`→dirty确认→PLC触发检查→模式模板/DLL/Profile预检→克隆运行Profile→应用相机/线程/PLC参数→启动线程 | 已应用设置而非未确认UI值 | 合法进入检测；任一预检/参数写入失败提示且不启动 | 清统计；可能写相机和PLC参数；创建线程 | 检查顺序和失败不启动保持 | `runtime/inspection_coordinator.*` | 保留后迁移 | 对每个前置条件逐一制造失败，确认未启动/未误计数 | 已验证 | S；T；U；纸巾参数已在软/硬触发线程`start()`前复制；2026-08-12用户确认主程序启动正常，其他预检和硬件顺序未改 |
| RUN-002 | 停止识别 | 顶栏“停止识别” | 检测中/线程可能运行 | `on_cancel_clicked`→Stopping→请求线程停止/等待→停抓图→恢复触发和相机抓图→CameraReady | 等待上限；相机当前状态 | 停止后状态相机已打开，保留最后正式结果；超时仅警告 | 停/删线程，清运行Profile，不清最终结果 | 保持协作停止和展示 | `runtime/inspection_coordinator.*` | Stage 3收敛 | 软/硬/五模式检测中停止，记录延迟、结果保留和再次启动 | 已基线 | S；U |
| RUN-003 | 线程重建与信号接回 | 启动前、旧线程结束后 | 主窗存活 | `ensureThreadsReady`/`reinitializeMyThread`/`reinitializeCameraThread`→断连接→协作等待→new→重连信号 | 相机指针、模板、运行参数 | 新线程可再次运行；等待超时记录警告但不强杀 | 删除/创建QThread对象和信号连接 | 不重复连接、不残留线程 | `runtime/` | Stage 3优化 | 连续启动/停止10次，核对每帧只收一次结果和无残留线程 | 已基线 | S；U |
| RUN-004 | 流帧与结果内存持有 | 相机持续采集/检测完成 | 主窗活动 | 采集线程克隆Mat→Queued signal→`handleStreamingFrame`；检测缓存标注图 | 当前无显式有界FrameQueue；每次信号传值 | 正常显示；高帧率/慢UI可能形成事件积压风险 | cv::Mat/QPixmap短期或队列持有 | Stage 0记录，Stage 3引入有界队列 | `runtime/frame_queue.*` | 计划内优化 | 记录分辨率、单帧字节和10分钟内存曲线；待用户 | 已基线 | S；U |
| RUN-005 | 检测间隔与节流 | 软触发循环/组织检测 | 运行参数已应用 | `applyRuntimeThreadSettingsFromUi`→`sendDataTo`→线程`received`→间隔判断 | `cameraDelay`等现有UI/PLC值 | 按当前间隔发检测；非法参数在启动前提示 | 影响吞吐和失败NG频率 | 单位和生效时机保持 | Recipe+runtime | 保留后迁移 | 固定输入分别设边界值，记录结果间隔和提示 | 已基线 | S；U |
| RUN-006 | 最近Overlay随位姿逻辑 | 非结果绑定模式收到新pose | 有上一检测框和pose | `slot_saveBoxesFromThread`→按角差/中心差旋转平移`g_lastDrawResults/g_lastStampPoly` | 新旧`DetectionPose` | pose无效清框；有效时框跟随；字库结果绑定时直接抑制 | 更新全局Overlay缓存 | 保持模式差异 | `ui/presenters/` | 保留后拆分 | 移动/旋转产品并移出视野，核对框跟随和清除 | 已基线 | S；U |

## 7. PLC与剔除

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLC-001 | 连接PLC | 启动延迟、打开相机附带连接、或“连接PLC” | IP/Rack/Slot可用 | `TS7Client::ConnectTo`→成功更新已应用设置和控件 | 默认`192.168.10.10/0/1` | 成功显示/使能状态；失败弹错误或日志，连接按钮路径不伪装成功 | 建立TCP/S7会话、写设置 | 三个入口均保留到Stage 2适配 | `devices/plc/snap7_adapter.*` | 保留后适配 | 在线/错误IP/错误rack-slot分别走三个入口 | 已基线 | S；U |
| PLC-002 | 断开PLC | “断开PLC”或退出 | 客户端存在 | `on_DisconnectpushButton_clicked`/析构→`TS7Client::Disconnect` | 当前连接状态 | 成功断开并开放连接配置；失败提示 | 关闭PLC会话 | 保持 | `devices/plc/` | 保留后适配 | 在线/离线点击断开及退出，核对状态 | 已基线 | S；U |
| PLC-003 | 触发工作模式下发 | 工作模式“确认”或启动 | PLC已连接 | `applyPlcTriggerModeFromUi`→写DB1偏移1032一字节 | 连续=0、间歇=1 | 成功保存设置；无连接/写失败提示且不更新已应用值 | PLC写`DB1.DBB1032` | 地址和值在Stage 1-3不变 | `devices/plc/` | 保留后适配 | PLC模拟/现场读回0、1及失败情况 | 已基线 | S；U |
| PLC-004 | 工艺参数下发 | “PLC参数→设置”或启动 | PLC已连接、整数可解析 | `applyPlcRunSettingsFromUi`→大端编码→WriteArea | 剔除时间DB980 Word、剔除距离DB920 DWord、拍照时间DB982 Word、拍照距离DB924 DWord；相机延时仅线程参数 | 全部成功后保存；任一写失败提示并返回失败 | 多次PLC写入 | 地址、长度、顺序和值保持 | `devices/plc/`+Recipe | 保留后适配 | 边界值、正常值和中途写失败，抓取/读回写序列 | 已基线 | S；U |
| PLC-005 | OK输出 | 正式结果OK且PLC连接 | 检测收尾 | 结果槽→`rightremove`→WriteArea | DB1偏移1033一字节，值0 | 成功复位输出；写失败警告；断开时静默返回 | PLC写0；停止100ms复位Timer | 正常OK写值保持 | `runtime/result_handler`+PLC adapter | 保留到Stage 4前 | 单个OK，抓取DB1033写入次数/值；断线记录无写 | 已基线 | S；U |
| PLC-006 | NG脉冲输出 | 立即剔除NG | PLC连接 | 结果槽→`wrongremove`写49→成功启动100ms timer→`rightremove`写0 | DB1.DBB1033；49持续约100ms | 写49成功后复位0；首次写失败不启动复位；断线静默 | PLC两次写和Timer | 值、顺序、100ms保持 | `runtime/result_handler`+PLC adapter | Stage 4前保留 | 单个NG抓取49→约100ms→0；模拟首写/复位写失败 | 已基线 | S；U |
| PLC-007 | 延迟剔除队列与复位 | NG结果、每次产品收尾、“剔除复位” | `wrongindex`可能>0 | NG入`removalQueue(total,target=total+wrongindex)`；后续结果检查队首并`wrongremove`；按钮清队列 | 剔除位置输入；设置按钮/选模板后更新 | 到目标计数触发一次剔除；复位清除所有未发信号 | 内存队列、未来PLC脉冲 | 队列公式和清理语义保持到Stage 4 | `runtime/reject_scheduler.*` | 保留 | wrongindex=0/1/3连续样本，记录目标序号；中途复位确认无后续旧脉冲 | 已基线 | S；U |

## 8. 结果、统计与存图

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RES-001 | 总数、NG与合格率 | 每个正式检测收尾 | 结果到达UI槽 | 各模式结果槽→`totalImages++`；NG时`ngImages++`→`(1-ng/total)*100` | 当前产品和系统失败共同口径见DET-008 | 更新总数、NG数、两位小数合格率 | 内存统计/UI | 当前公式固定 | `runtime/statistics.*` | 保留到计划允许分类 | 每模式OK、NG、失败各1，记录0→3的所有显示值 | 已基线 | S；U |
| RES-002 | 总数清零 | 总数旁“清零” | 任意空闲/运行状态当前可点击性依UI状态 | `on_cut_cancelButton_2_clicked` | 无 | 总数和NG同时置0并更新两框；合格率框未在该槽显式重算 | 清内存统计 | 精确清理范围保持 | `ui/controllers/`+statistics | 保留 | 先产生多结果再清零，核对三个统计字段和下一帧 | 已基线 | S；U |
| RES-003 | NG数清零 | NG旁“清零” | 同上 | `on_cut_cancelButton_3_clicked` | 无 | 仅NG置0；总数保持；合格率未在该槽显式重算 | 改内存NG | 当前不对称行为纳入基线 | `ui/controllers/`+statistics | 保留 | 产生2NG/1OK后清NG，核对总数/NG/合格率及下一帧 | 已基线 | S；U |
| RES-004 | 检测耗时 | 五模式检测完成 | 正式检测执行 | 算法计时→`speedLabel`/结果文本 | 各模式当前计时范围不同 | 显示毫秒耗时；失败分支按其路径记录或缺失 | UI/日志 | Stage 0需固定P50/P95实际值 | `detection/result` | 保留并统一类型 | 每模式固定样本Release运行30次，记录P50/P95 | 已基线 | S；U |
| RES-005 | 当前模板名 | 选择/保存/Profile命中/停止 | 模板状态存在 | `updateCurrentTemplateName`及各模式结果槽 | 单模板目录名或命中Profile名 | 显示当前/命中模板；无模板`--`或隐藏策略 | UI | 保持名称来源 | `ui/presenters/` | 保留 | 单/多模板选择与自动命中，核对名称和停止后状态 | 已基线 | S；U |
| SAVE-001 | 按判定选择存图 | 正式结果完成 | 已设置保存模式和目录 | 五模式结果槽→`saveResultImages/saveWordResultImages/saveImage2Async` | 不保存/NG/OK/全部 | 只保存策略允许的判定；目录失败多为日志 | 启动异步写盘 | 选择语义保持 | `runtime/image_save_service.*` | Stage 2优化 | 四模式各跑OK/NG，核对文件数量 | 已基线 | S；U |
| SAVE-002 | 标注图/原图组合 | 存图被允许 | 保存类型已设置 | `shouldSaveRecognitionBoxImage/shouldSaveNoRecognitionBoxImage`→UI pixmap或cv::Mat写盘 | 两者都存/仅标注/仅原图 | 生成对应组合；无可用图时该文件缺失/日志 | 异步写文件 | 文件内容基线后引入有界队列 | `runtime/image_save_service.*` | Stage 2计划内优化 | 三种组合各跑1次，像素比对标注/原图 | 已基线 | S；U |
| SAVE-003 | 目录和命名 | 任一保存任务 | 根目录可写 | 保存函数→`selectedDir/{ok,ng,ok_raw,ng_raw}`→时间戳文件名 | 当前时间和结果分类 | 成功写对应目录；创建/写入失败主要qDebug，不弹统一故障 | 创建目录和PNG/JPG文件 | 正常路径/命名保持 | `runtime/image_save_service.*` | Stage 2优化错误报告 | 固定时刻附近跑结果，核对目录、扩展名、命名和失败日志 | 已基线 | S；U |
| SAVE-004 | OCR原图来源 | OCR结果触发原图保存 | 相机仍可取图 | OCR槽→`saveImage2Async`→后台调用相机`ReadBuffer/GetImage`，不是直接复用检测输入帧 | 当前相机时序 | 可能保存检测后新取的一帧；取图失败仅日志 | 后台访问相机并写盘 | 这是现状差异，Stage 2统一临时交接帧属计划优化 | `runtime/DetectionCompletion` | 计划内优化 | 移动物体连续OCR，时间戳/像素对照检测图和raw文件 | 已基线 | S；U |
| SAVE-005 | 异步写盘容量与失败 | 每个需保存结果 | 磁盘正常/慢/满 | 多处`QtConcurrent::run`每次提交一个任务，无统一容量 | 当前无容量上限；主计划目标容量8 | 正常不阻塞UI；慢盘可能任务/内存持续增长；失败多日志 | 无界后台任务短期持有图像 | Stage 0测量，Stage 2改为容量8有界队列 | `runtime/image_save_service.*` | 计划内优化 | 正常盘/限速盘连续运行，记录提交率、队列代理指标、内存和失败日志 | 已基线 | S；U |

## 9. 多相机与独立工具

| ID | 功能分类 | 当前入口/触发 | 前置条件和操作步骤 | 当前文件、关键函数和调用链 | 输入/设置、默认值及生效时机 | 当前正常结果和失败路径 | 副作用（磁盘/统计/PLC/线程） | 当前基线 | 目标模块/位置 | 动作 | 从原入口执行的验证方法 | 状态 | 证据 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| MC-001 | 多相机窗口入口 | 顶栏“多相机模式” | 非检测/非停止/非模板制作 | `on_MultiCameraMode_clicked`→new/show`MultiCameraWidget`；返回按钮→close | 无持久参数 | 打开独立窗口；忙碌时提示；重复点击复用现存窗口；关闭后指针清空 | 创建/销毁窗口 | 计划要求入口不被单相机重构破坏 | 原位 | 保留且延期迁移 | 空闲打开/返回/重开；检测中点击核对提示 | 已基线 | S；U；P |
| MC-002 | 多相机可见控件现状 | 多相机窗口内扫描/打开/采集/停止/触发/保存按钮 | 窗口已打开 | `multicamerawidget.ui`；`MultiCameraWidget`只初始化两行和连接“返回”，其余按钮无信号接线 | UI静态默认值 | 当前点击其余按钮无业务动作，预览/状态保持占位；不能记录为已实现功能 | 无设备/统计/PLC副作用 | 可达但未接线的真实现状 | 原位 | 计划明确延期，不在本轮补齐 | 打开窗口逐按钮点击，确认只有返回有动作并记录截图 | 已延期 | S；U；P |
| MC-003 | 双相机底层API | 当前无UI/脚本运行入口，仅编译进主工程 | 需另行代码调用 | `MultiCameraController`→2个`MultiCameraUnit`→Hikvision；`MultiCameraSyncManager`校验shotId/frameId/时间差 | 默认2台、软件触发、最大时间差5000us、要求相同frameId | API可扫描/open/start/trigger/grab；当前窗口未实例化Controller，生产统计/PLC未接入 | 若被调用会开2相机并持有帧 | 计划明确保持源码原位，不迁移/扩建 | 原位 | 延期 | 本轮只做静态零入口核对；后续独立任务建立专用测试 | 已延期 | S；P |
| TOOL-001 | 授权生成/读取工具 | 单独打开`tools/license_tool/LicenseTool.pro`构建的程序 | 与主程序相同Qt；输出目录可写 | 工具UI→`makeLicenseFile`/`licenseInfoText`，算法与RuntimeGuard同密钥/格式 | 到期日默认当前+1年；默认输出工具目录`license.ini` | 生成加密授权并立即读回；无效/不可写提示 | 写授权文件 | 独立工程必须保留 | `tools/license_tool`原位 | 保留 | Qt Creator构建工具；生成未来/过期授权并由主程序分别验证 | 已基线 | S；U |
| TOOL-002 | BarcodeDecoder.dll重建与ABI | 独立构建脚本/主程序动态加载 | VS2022+CMake+网络仅重建时；本轮Agent不执行 | `tools/barcode_decoder`→ZXing+libdmtx→导出`GetVersion/DecodeLuma8`；主程序`QLibrary`解析 | v2.1.0；DataMatrix+QR；libdmtx fallback15ms；静态CRT | DLL可返回码内容/角点/耗时；参数错、未找到、内部错返回固定码；主程序缺DLL预检失败 | 生成DLL；运行时动态加载 | ABI和版本日志保持 | `devices/barcode/`适配器包裹ABI | 保留后Stage 2适配 | 用户按README重建后替换，核对版本日志及固定二维码样本 | 已基线 | S；U |

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
| 已基线 | 81 | 除已验证功能及MC-002、MC-003外的功能ID；多Profile配方组装门禁通过后相关完整模板功能恢复基线状态 |
| 迁移中 | 0 | 无 |
| 已验证 | 7 | SET-010、DET-002、DET-003、DET-004、DET-005、DET-006、RUN-001；对应切片已通过Agent静态检查和用户Qt Creator门禁 |
| 已延期 | 2 | MC-002、MC-003；依据升级计划3.6 |
| 已确认删除 | 0 | 无删除授权 |

## 未决差异与已知基线风险

| ID | 计划规定/期望 | 源码当前行为 | 是否影响结果/硬件 | 处理决定 | 确认人/证据 |
|---|---|---|---|---|---|
| DIFF-001 | Stage 1形成纸巾阈值唯一默认6.0 | 当前代码已删除`.ui`静态5.2和检测器进程级5.2默认；`GlobalSettings`默认引用`TissueRecipeParameters`的6.0，线程启动前复制显式参数 | 计划内行为修正；可消除绕过主窗时的阈值分歧 | 2026-08-12 Agent静态核对和用户Qt Creator主程序/Pipeline测试门禁通过，差异已关闭 | 计划3.1/阶段1；SET-010/DET-005 |
| DIFF-002 | 模板阈值由Recipe唯一来源 | 私有设置和初始化代码默认70，`.ui`静态文本80 | 可能影响首次显示，但构造后通常为70 | 记录，迁移时以当前构造后实际值和模板私有值为基线 | S；TPL-013 |
| DIFF-003 | 未来系统故障不进入产品质量分母 | 当前到达正式收尾的读码失败、无定位、无纸卷等多按产品NG计数并可能触发PLC | 是 | Stage 1-3保持当前；硬件/故障策略不得提前进入Stage 4 | 计划3.5/阶段4；DET-008 |
| DIFF-004 | 未来检测帧通过`DetectionCompletion`短期交接 | OCR原图保存会从相机另取一帧；其余路径来源也不统一 | 影响存图对应性，不改判定 | Stage 0测量；Stage 2按计划优化，不在基线切片改 | SAVE-004 |
| DIFF-005 | 存图队列容量8且慢盘不积压 | 当前每图一次`QtConcurrent::run`，无统一容量 | 影响内存/吞吐 | Stage 0测量；Stage 2计划内优化 | SAVE-005 |
| DIFF-006 | 多相机本轮保持原状 | 窗口可打开，但除“返回”外可见按钮均未接Controller；底层类无当前运行入口 | 不影响单相机；误认为可用会影响操作预期 | 明确登记并按计划延期，不补做多相机开发 | 计划3.6；MC-001..003 |
| DIFF-007 | 相机和PLC边界后续分离 | “打开相机”会先尝试连接PLC，PLC失败仍继续开相机 | 影响设备操作时序 | Stage 1-3保持，Stage 2只用适配器复现现有顺序 | CAM-001/PLC-001 |

## 基线资源

| 资源 | 路径/来源 | 覆盖功能 | 可重复条件 | 当前状态 |
|---|---|---|---|---|
| 五模式样本清单 | `tests/baseline/sample_manifest.tsv` | DET-002..008、RES、SAVE、PLC | 用户填写每模式OK/NG/FAILURE的固定图、模板、文本、Overlay、计数、存图、PLC和耗时 | 已建清单，15份实际证据待用户 |
| 纸巾离线基线测试 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | SET-010、DET-005 | Qt Creator打开`tests/tests.pro`，Run qmake、Build并运行测试 | 2026-08-12用户确认纸巾Pipeline切片的4项业务测试全部通过，汇总`6 passed, 0 failed` |
| 生产主程序构建 | `app/AutoOCRproject.pro` | 全部主程序功能 | Qt 5.14.2/MSVC2017 x64 Release，Run qmake、Rebuild、Run | 2026-08-12用户先后确认含纸巾Pipeline和深度OCR Pipeline的主程序正常构建运行；纸巾阈值与OCR模式切换正常 |
| 运行与性能记录 | `docs/development/OCRGangYin重构执行记录.md` | 内存、P50/P95、慢盘、停止/重启 | 用户按执行记录步骤填写真实数值 | 待用户验证 |
