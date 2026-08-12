# OCRGangYin 重构执行记录

## 仓库与阶段状态

- 基线HEAD：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`
- 基线分支：`codex/repo-layout`
- 当前工作分支：`codex/ocrgangyin-refactor`（从基线HEAD新建）
- 当前阶段：Stage 1 配方与算法拆分（按用户风险接受条件进入）
- 当前切片：多Profile最高分选择公共职责（已验证，待提交）
- 阶段结论：**Stage 1进行中**。基础配方、纸巾、深度OCR、模板匹配、字库匹配和二维码+三期切片已分别提交为`8d9c400`、`e590703`、`c8e3113`、`5113ea5`、`a7404c1`和`f2658f2`。多Profile选择切片已通过用户Qt Creator测试和主工程门禁，正在形成独立提交。
- 构建纪律：Agent未运行、未间接调用、也未通过GUI触发任何qmake、编译、链接、测试目标或主程序。

## Stage 0已完成范围

- 目标：给所有当前可达功能分配唯一ID，记录入口、调用链、输入、正常/失败结果、副作用和验证步骤；建立`tests/tests.pro`及首个离线检测测试。
- 受影响功能ID：`SYS-001..010`、`UI-001..009`、`SET-001..013`、`TPL-001..016`、`DET-001..008`、`CAM-001..006`、`RUN-001..006`、`PLC-001..007`、`RES-001..005`、`SAVE-001..005`、`MC-001..003`、`TOOL-001..002`。
- 明确不在范围内：不修改正式程序行为；不迁移生产职责；不调整算法、阈值、统计、PLC地址/值/顺序、相机时序；不补做多相机；不进入Stage 1；不构建/运行。

## Stage 1已完成基础切片

- 用户风险接受：五模式固定样本、PLC、硬触发、存图和性能证据延期补充；继续开发不等于这些项目已验证。
- 受影响功能ID：`SET-003`、`SET-010`、`TPL-006`、`TPL-008`、`TPL-009`。
- 旧代码事实：产品私有设置由`AppSettingsManager`保存到`app_settings.appset`；`Widget::WordTemplateProfile`持有目录、定位模板、ROI、字符模板和私有设置；纸巾阈值当前同时存在整机6.0和检测器内部5.2来源。
- 本切片目标：新增基础`ProductRecipe`、固定五种模式ID、Schema 1 JSON解析/序列化、字段/资源相对路径校验和`shared_ptr<const ProductRecipe>`只读运行快照；暂不接管旧UI、模板目录或检测入口。
- 保持边界：不改变旧配置读写、模板制作、检测判定、线程、相机、PLC、统计或存图行为；代码切片只由用户在Qt Creator构建和运行测试。

## Stage 1已完成纸巾切片

- 受影响功能ID：`SET-010`、`DET-005`、`RUN-001`。
- 旧调用链：纸巾UI阈值→`TissueRollDetector::setDefaultRoughnessThreshold`进程级原子值→`MyThread/CameraThread`无参构造检测器→`processImage`→现有结果槽、统计、存图和PLC。
- 已知差异：正常主窗加载整机设置后实际应用值是6.0或用户保存值，但绕过主窗直接构造检测器时仍有5.2的第二默认。
- 本切片目标：删除检测器进程级默认；由UI在启动前将`TissueRecipeParameters`显式复制到软/硬触发线程；线程为本次运行构造只读纸巾Pipeline。
- 保持边界：不改粗糙度计算、`score >= threshold`判NG边界、空图/无圆诊断、Overlay、统计、存图、PLC、相机和触发时序。

## Stage 1已完成深度OCR切片

- 受影响功能ID：`DET-004`。
- 旧调用链：模式2→`dispatchDetectionByMode`→`slot_readAndDetect`→`prepareOrientedDateRoi`→`DBDetector::Run`→`CRNNRecognizer::Run`→Widget按字节清洗/换行拼接→非空且与目标文本精确相等为OK→现有UI、统计、存图和PLC收尾。
- 当前清洗规则：逐字节保留ASCII字母数字、所有高位字节及`- . :`；清洗后空行丢弃，其余按Paddle返回顺序用`\n`拼接。
- 本切片目标：将Paddle识别调用编排、原文本清洗和精确判定提取为`detection/ocr/ocr_detection_pipeline.*`；Widget暂时以窄回调提供已有Paddle对象，Stage 2再迁入`IOcrEngine`适配器。
- 保持边界：不改Paddle模型/参数、ROI旋转裁剪、识别顺序、精确比较、空结果NG、识别文本显示、统计、存图、PLC和延迟剔除。

## Stage 1已完成模板匹配切片

- 受影响功能ID：`DET-002`；界面正式名称为“模板匹配”，内部稳定ID为`stamp_detection`，业务内容是钢印+字符检测；`DET-001`仅提供已有定位位姿和日期ROI，本切片不修改其参数、算法或状态。
- 旧调用链：模式0→`dispatchDetectionByMode`→`slot_readAndDetect3`→`prepareOrientedDateRoi`→目标文本字符计数→`TemplateMatch::run3`→`OverlapDetector::processImage`→字符数量相等且钢印无重叠才OK→原UI、统计、存图、PLC和延迟剔除收尾。
- 当前失败路径：无效图直接返回；日期ROI越界弹警告并返回；字符数量不等为NG；缺少`calibrate_config.yaml`或重叠检测返回失败为NG；两项均失败显示组合错误。
- 本切片目标：将目标字符计数、两个旧算法的窄回调编排、钢印多边形结果和两条件AND判定提取到`detection/stamp/stamp_detection_pipeline.*`。
- 保持边界：不改定位、ROI padding=20、字符匹配阈值/框、拉环/重叠算法、错误文案、Overlay、统计、存图、PLC、延迟剔除、相机或线程时序。

## Stage 1已完成字库匹配切片

- 主要功能ID：`DET-003`；`DET-006`在读码成功后复用相同字符子流程，本切片只登记间接影响，不迁移二维码优先、失败短路或组合判定。
- 旧调用链：软/硬采集线程并行运行各Profile的`TrackingPoseMatcher`并选最高score→`DetectionPose::wordTemplateProfileIndex`→`Widget::dispatchDetectionByMode`选Profile→`runWordTemplateDetection`→日期ROI→目标字符解析/阈值应用→`TemplateMatch::run3`→检测数等于目标数OK→原Overlay、统计、存图、PLC和延迟剔除收尾。
- 当前失败路径：无定位由`finalizeWordTrackingNg`收尾；无效Profile索引/无目标模板直接返回；无效图或日期ROI直接返回；检测数量少于或多于目标均NG。
- 本切片目标：将目标字符解析、原`TemplateMatch`窄回调和数量判定提取到`detection/word/word_detection_pipeline.*`；保留匹配框供旧入口映射和诊断。
- 保持边界：不改多Profile并行选择、最高score规则、日期ROI padding=20、模板阈值、字符匹配算法、匹配框、诊断文本、Overlay、统计、存图、PLC、相机或触发时序。

## Stage 1已完成二维码+三期切片

- 主要功能ID：`DET-006`；`DET-003`提供已经验证的字符检测子流程，`DET-007`定位失败收尾和`DET-008`当前失败计数语义只登记边界，本切片不修改其状态。
- 旧调用链：模式4→软/硬线程多Profile最高score定位→`dispatchDetectionByMode`→`runBarcodeWordDetection`→二维码/日期组合旋转ROI→`decodeBarcodeRoi`原图快路径及60ms内fallback→读码失败立即`finalizeBarcodeWordNg`→读码成功才调用`runWordTemplateDetection`→原UI、统计、存图、PLC和延迟剔除收尾。
- 当前失败路径：无定位、无效Profile/模板、二维码4点或日期多边形缺失、组合ROI越界、DLL不可用、不可读、解码超时/内部错误均不执行日期字符检测；读码成功但日期ROI无效也形成一次NG；日期字符少/多沿用字库NG。
- 当前状态与缓存：成功策略和option flags缓存到本次运行Profile；连续3次读码失败清除首选策略；解码角点映射回原图；硬触发定位失败仍逐触发形成一次NG。
- 本切片目标：将“读码结果优先→不可读时短路→可读时调用日期字符窄回调→组合最终判定”提取到`detection/barcode_word/barcode_word_detection_pipeline.*`。
- 保持边界：不改多Profile选择、软硬触发差异、ROI padding=8%/日期20像素、DLL加载与ABI、DataMatrix/QR掩码、60ms预算、fallback顺序、策略缓存、角点映射、文本、Overlay、统计、存图、PLC或相机时序。

## Stage 1当前多Profile最高分选择切片

- 主要功能ID：`DET-003`、`DET-006`；`DET-001`的单Profile定位算法和`TPL-009`的多目录加载/部分成功策略只登记关联边界，本切片不修改其状态。
- 旧调用链：`MyThread`和`CameraThread`各自并行运行已加载Profile的`TrackingPoseMatcher::matchPrepared`→各自重复跳过无效Pose→映射二维码相对多边形→写入Profile索引→使用严格`score > bestScore`选中最佳项→发出原`DetectionPose`。
- 当前规则：无效候选不参与；分数更高才替换；分数相同保留加载顺序中先出现的Profile；选中后保留其名称、原Profile索引以及随anchor/angle映射的二维码多边形。
- 本切片目标：新增`detection/common/profile_pose_selector.*`并让软/硬触发线程共同调用；用内存Pose锁定无效跳过、最高分、同分优先级和二维码多边形映射。
- 保持边界：不改Profile加载/顺序、`TrackingPoseMatcher`参数和并行执行、软触发间隔、硬触发逐次收尾、字库/二维码Pipeline、Overlay、统计、存图、PLC、相机或DLL。

## 功能状态变化

| 功能范围 | 修改前状态 | 修改后状态 | 本次为何涉及 | 验证证据 |
|---|---|---|---|---|
| SYS/UI/SET/TPL | 无对照表 | 已基线 | 启动、界面、设置和完整模板工作流属于Stage 0强制范围 | 源码/UI静态核对；实际操作待用户 |
| DET/CAM/RUN/PLC/RES/SAVE | 无对照表 | 已基线 | 五模式、软硬触发、设备副作用、统计和存图必须先锁定 | 源码/线程/回调静态核对；纸巾离线测试已通过；实际样本与设备待用户 |
| SET-010、DET-005 | 已基线 | 已基线 | 新增纸巾离线基线测试工程；修复测试运行库架构后锁定内部默认值、显式阈值、空图和纯黑图失败语义，不改变生产实现 | 主程序运行通过；Qt Creator Release连续两次`6 passed, 0 failed`、退出码0；真实纸巾样本仍待Stage 0回归 |
| SET-003、SET-010、TPL-006、TPL-008、TPL-009 | 已基线 | 已基线 | 新增基础配方Schema、五模式ID、资产路径校验和运行快照；尚未接管旧功能入口，不冒充为完整功能新路径验证 | Agent静态检查、主工程Rebuild/Run、`product_recipe_test`及旧纸巾测试均已通过；旧入口保持基线状态 |
| SET-010、DET-005、RUN-001 | 已基线 | 已验证 | 纸巾阈值从进程级默认改为启动前显式参数副本，旧检测器由新Pipeline组装 | Agent静态检查通过；2026-08-12 13:49用户确认主程序退出码0、阈值显示正常，纸巾测试`6 passed, 0 failed` |
| DET-004 | 已基线 | 已验证 | 深度OCR识别编排、按字节文本清洗与精确判定从Widget提取到新Pipeline | Agent静态检查通过；2026-08-12 14:24 OCR测试及纸巾子工程回归均`6 passed, 0 failed`；用户随后确认主工程Run qmake/Rebuild/Run及OCR模式切换通过 |
| DET-002 | 已基线 | 已验证 | 模板匹配目标字符计数、旧字符匹配/重叠调用和两条件AND判定从Widget提取到新Pipeline | Agent静态检查通过；2026-08-12 14:58测试`6 passed, 0 failed`、退出码0；用户截图确认主程序运行及“模板匹配”入口 |
| DET-003 | 已基线 | 已验证 | 字库目标字符解析、原字符匹配回调和数量判定从Widget提取到新Pipeline | Agent静态检查通过；2026-08-12 15:43 Pipeline测试`6 passed, 0 failed`、退出码0；用户随后确认主程序及“字库匹配”模式门禁通过 |
| DET-006 | 已基线 | 已验证 | 二维码优先、失败短路、读码成功后日期检测和组合判定从Widget提取到新Pipeline | Agent静态检查通过；2026-08-12 16:48 Pipeline测试`6 passed, 0 failed`、退出码0；用户随后确认主程序Run qmake/Rebuild/Run及模式切换无问题 |
| DET-003、DET-006 | 已验证 | 已验证 | 软/硬触发线程重复的多Profile最高分选择、同分顺序和二维码多边形映射提取到公共Selector | Agent静态检查通过；2026-08-12 17:19测试`6 passed, 0 failed`、退出码0；用户确认主工程正常 |
| MC-001 | 无对照表 | 已基线 | 当前多相机窗口入口仍可达 | 源码静态核对；实际入口待用户 |
| MC-002..003 | 无对照表 | 已延期 | 计划3.6明确本轮不扩建/迁移多相机；当前窗口控制按钮未接底层Controller | 源码/UI零接线核对；计划依据 |
| TOOL-001..002 | 无对照表 | 已基线 | 独立授权工程和条码DLL工程仍是当前可进入/部署能力 | 工程/源码静态核对；构建待用户 |

## 已检查的现有实现

| 功能范围 | 入口/触发 | 旧文件、函数和调用链 | 正常结果、失败路径和副作用 |
|---|---|---|---|
| SYS | 进程启动、24h定时器、退出、Release链接后部署 | `app/main.cpp`、`RuntimeGuard.*`、`ccrashstack.*`、`AutoOCRproject.pro`、`deploy_runtime.ps1`、`dist/ShengYin` | 授权/单实例/日志/崩溃/OCR模型/设置/PLC延迟连接/部署/退出均已反向登记 |
| UI/SET | `widget.ui`全部顶栏与设置控件、事件过滤器、显式/自动槽 | `widget.ui`、`widget.h/.cpp`、`appsettingsmanager.*`、`imagelabel.*` | 记录状态使能、提示、dirty值、默认/保存/回退、布局和图像展示 |
| TPL | 制作、冻结、绘图、保存、选择、多Profile、切字、模式切换 | `Widget`模板函数、`ImageLabel`、`CharacterTemplateCropDialog`、`AppSettingsManager`、`TrackingPoseMatcher`、`OverlapDetector` | 覆盖正常保存、非法资源、部分Profile成功、覆盖删除和即时读码失败清理 |
| DET | 五模式线程结果 | `MyThread`/`CameraThread`→`TrackingPoseMatcher`/`TissueRollDetector`→`Widget::dispatchDetectionByMode`→五种结果槽 | 记录当前OK/NG、失败短路、识别文本、Overlay、统计、存图和PLC |
| CAM/RUN | 打开/关闭、软硬触发、开始/停止/重建线程 | `cmvcamera.*`、`mythread.*`、`CameraThread.*`、`Widget`运行控制 | 记录首台相机、回调frameSeq、协作停止、结果帧抑制和无显式有界帧队列 |
| PLC | 连接/断开、参数确认、正式结果、剔除复位 | `Widget::applyPlc*`、`rightremove/wrongremove`、Snap7 | 记录DB1032/1033/920/924/980/982、49→100ms→0和延迟剔除队列 |
| RES/SAVE | 正式收尾、统计清零、存图策略 | 五模式结果槽、`saveImage2Async/saveResultImages/saveWordResultImages` | 记录当前合格率公式、清零不对称、OCR另取帧和无界QtConcurrent任务 |
| MC/TOOLS | 多相机顶栏入口；两个独立工具工程 | `multicamerawidget.*`、`MultiCamera*`；`tools/license_tool`、`tools/barcode_decoder` | 多相机只有返回按钮接线；底层Controller无当前UI入口；工具格式/ABI已登记 |

另已核对无独立可观察入口的候选：`Zhuizong`两个方法全仓无调用，`timer1`只构造未启动，`on_eliminatebutton_clicked`没有同名UI控件或显式连接，`lineBoxIndex`在UI中隐藏且无源码读写。它们均保留原状，不在Stage 0擅自删除或制造虚假功能ID。

完整逐功能链路、参数、失败和验证方法见 `OCRGangYin现有功能对照表.md`。

## 本次变更

| 旧职责 | 新位置 | 处理方式 | 行为保持/计划内优化 | 仍保留的旧入口 |
|---|---|---|---|---|
| 当前全部可达生产功能 | `docs/development/OCRGangYin现有功能对照表.md` | 仅建立基线映射 | 正式程序零行为变化 | 全部旧入口原样保留 |
| 五模式固定样本需求 | `tests/baseline/sample_manifest.tsv` | 建立15项OK/NG/FAILURE证据清单 | 不预填虚假结果，等待用户真实证据 | 原界面五模式入口 |
| 纸巾检测失败语义 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | 新增离线测试源码和`tests/tests.pro` | 锁定空图/纯黑图NG和当前阈值来源差异，不改生产代码 | `TissueRollDetector`旧调用链原样保留 |
| 产品配方身份、模式、参数和资产映射 | `app/recipes/product_recipe.*` | 新增Schema 1数据对象、五模式稳定ID、JSON往返与输入校验 | 新配方纸巾阈值唯一默认为6.0；本切片不替换旧INI或检测入口 | `AppSettingsManager`、`Widget::WordTemplateProfile`及原模板目录逻辑 |
| 运行期配方可变性 | `ProductRecipeSnapshot` | 将经校验的可编辑配方拷贝为`shared_ptr<const ProductRecipe>` | 为后续任务启动时固定快照；尚未接入检测线程 | 旧线程参数传递不变 |
| 基础配方回归 | `tests/recipe_tests/product_recipe_test.cpp` | 新增4项业务测试并纳入`tests/tests.pro` | 验证数据合同，不读写用户模板 | 原纸巾基线测试子工程保留 |
| 纸巾运行组装 | `app/detection/tissue/tissue_detection_pipeline.*` | Pipeline仅接收显式`TissueRecipeParameters`，内部组装原`TissueRollDetector` | 删除进程级可变默认；粗糙度计算和判定不变 | `TissueRollDetector::processImage`算法保留 |
| 软/硬触发纸巾参数 | `MyThread` / `CameraThread` | 主界面在`start()`前复制参数，线程`run()`构造本次运行的`const TissueDetectionPipeline` | 纸巾切片形成运行内只读参数；不改相机/触发顺序 | 原结果信号、统计、存图和PLC链保留 |
| 纸巾离线回归 | `tests/detection_tests/tissue_roll_detector_baseline_test.cpp` | 验证配方6.0唯一默认、显式阈值传递和原失败诊断 | 保留4项业务测试；用户已验证 | 测试目标名保持不变 |
| 深度OCR识别编排与判定 | `app/detection/ocr/ocr_detection_pipeline.*` | Pipeline调用窄识别回调，按旧规则清洗/拼接并生成精确判定 | 原Paddle调用和返回顺序不变；检测模块不依赖Paddle类 | `Widget::slot_readAndDetect`仍负责ROI和结果收尾 |
| 深度OCR离线回归 | `tests/detection_tests/ocr_detection_pipeline_test/` | 新增4项内存假识别测试，覆盖字节清洗/顺序、精确OK、空NG和不等NG | 不加载Paddle、不调用UI/相机/PLC/存图 | 原生产OCR入口保留 |
| 检测测试子目标组装 | `tests/detection_tests/detection_tests.pro` | `subdirs`集合保留纸巾/OCR/钢印/字库目标并新增二维码+三期目标 | 五个目标独立运行；需要OpenCV的目标继续执行x64运行库校验 | `tests/tests.pro`顶层入口不变 |
| 钢印目标字符计数、算法编排和组合判定 | `app/detection/stamp/stamp_detection_pipeline.*` | Pipeline通过窄回调调用原字符匹配和重叠检测，返回两项子判定与钢印多边形 | 原正则/回退计数、字符数量相等且无重叠才OK、缺配置NG均保持 | `Widget::slot_readAndDetect3`继续负责ROI、Overlay和结果收尾 |
| 钢印Pipeline离线回归 | `tests/detection_tests/stamp_detection_pipeline_test/` | 新增4项内存假回调测试 | 覆盖变体/回退计数、两条件OK、字符NG和重叠/缺配置NG；不运行视觉算法或外部副作用 | 原生产钢印入口保留 |
| 字库目标字符解析、算法编排和数量判定 | `app/detection/word/word_detection_pipeline.*` | Pipeline解析原目标单元，通过窄回调调用原字符匹配，并返回目标数、检测数和判定 | 原正则、无可解析单元时按字符串长度回退、数量相等OK均保持 | `Widget::runWordTemplateDetection`继续负责阈值、ROI、诊断、Overlay和结果收尾 |
| 字库Pipeline离线回归 | `tests/detection_tests/word_detection_pipeline_test/` | 新增4项内存假回调测试 | 覆盖变体/回退解析、数量相等OK、少/多均NG；不运行视觉算法或外部副作用 | 原生产字库入口保留；二维码模式仅复用成功读码后的字符子流程 |
| 二维码优先、日期子流程和组合判定 | `app/detection/barcode_word/barcode_word_detection_pipeline.*` | Pipeline接收读码是否可读和日期检测窄回调，明确是否执行日期、是否产出结果及最终判定 | 不可读绝不调用日期；可读但无日期阶段不输出OK；日期OK/NG决定组合结果 | `Widget::runBarcodeWordDetection`继续负责ROI、DLL解码、缓存、角点、文本和结果收尾 |
| 二维码+三期Pipeline离线回归 | `tests/detection_tests/barcode_word_detection_pipeline_test/` | 新增4项内存假回调测试 | 覆盖不可读短路、日期阶段不可用、可读+日期OK及可读+日期NG；不加载DLL或运行外部副作用 | 原生产二维码+三期入口保留 |
| 多Profile最高分选择 | `app/detection/common/profile_pose_selector.*` | 从`MyThread`和`CameraThread`提取共同Selector并接回原循环 | 保持无效跳过、严格`>`、同分先到优先、Profile索引和二维码多边形映射 | 两线程仍负责并行定位、触发节流和结果信号 |
| Profile选择离线回归 | `tests/detection_tests/profile_pose_selector_test/` | 新增4项内存Pose测试 | 不运行定位算法、相机、PLC、存图或主界面 | 原字库和二维码模式入口保留 |

## 已确认的关键现状

1. 切片修改前，纸巾整机默认是6.0，`.ui`与检测器进程初始默认是5.2。当前代码已将唯一默认收敛到`TissueRecipeParameters`的6.0，已保存的整机值仍优先且会在启动前显式复制到线程。
2. 模板图像阈值私有默认和构造初始化为70，但`.ui`静态文本是80；以构造后和模板配置的实际值作为运行基线。
3. 当前到达正式收尾的无定位、读码失败、无纸卷等失败通常进入总数和NG，影响合格率并可能产生PLC动作；Stage 1至3不提前改变。
4. OCR原图存储会异步向相机另取一帧，不保证与检测帧相同；Stage 2计划用`DetectionCompletion`修正。
5. 当前存图采用逐结果`QtConcurrent::run`，无容量限制；Stage 2计划改为容量8的有界队列。
6. 多相机窗口目前只有“返回单相机”接线；其余可见按钮没有接入`MultiCameraController`。升级计划明确本轮保持原状。
7. “打开相机”会先尝试连接PLC；PLC失败后仍继续打开首台枚举相机。这是当前设备时序，Stage 1至3不得顺带改变。

## Agent静态验证证据

| 验证项 | 命令/步骤 | 预期 | 实际结果 | 状态 |
|---|---|---|---|---|
| 工作区起点 | `git status --short --branch`、`git rev-parse HEAD`、`git log` | 基线干净且HEAD可记录 | 基线分支`codex/repo-layout`干净；HEAD=`1c8d564...`；已新建专用分支 | 通过 |
| 源码/UI候选盘点 | Skill清单脚本输出到系统临时目录；再人工读取入口和调用链 | 不修改仓库，覆盖所有候选 | 盘点68个app源/工程/UI文件约32377行；已人工核对矩阵，不把脚本输出当功能证据 | 通过 |
| 可见控件反向核对 | 解析`widget.ui`和`multicamerawidget.ui`并与槽/显式连接对照 | 所有按钮/输入/自定义控件有去留 | 已覆盖；确认多相机除返回外未接线 | 通过 |
| 功能ID与状态 | 解析矩阵正式功能行（不含`DIFF-*`已知差异项）并检查ID/状态 | 90个唯一ID；Selector切片验证后为81已基线、0迁移中、7已验证、2已延期 | 90/90唯一；81/0/7/2，与统计一致 | 通过 |
| 禁止强杀线程 | 全仓搜索`QThread::terminate`/`.terminate()` | 不存在运行时强杀 | 0处命中 | 通过 |
| Stage 1配方工程清单 | 静态核对主工程及`tests/tests.pro`的源文件、子工程和运行库部署参数 | 所有路径存在；旧检测子工程保留；新配方测试4项 | PowerShell脚本解析0错误；必需路径0缺失；5模式ID和7个JSON关键字0缺失；旧生产实现文件0改动 | 通过 |
| Stage 1纸巾切片边界 | 搜索旧默认API/5.2字面量、核对两线程设置和`start()`顺序、解析UI/工程清单 | 无进程级默认；参数在线程启动前固定；新文件都纳入主/测试工程 | 旧API和5.2字面量0命中；软/硬触发均在`start()`前调用运行参数应用；UI XML和工程路径静态检查通过 | 通过 |
| Stage 1深度OCR切片边界 | 搜索旧Widget清洗帮助函数、Paddle调用、Pipeline依赖、测试方法和qmake清单 | 旧帮助函数零引用；Paddle检测/识别调用保留；Pipeline无UI/Paddle/PLC/存图依赖；4项测试和两个检测子目标路径完整 | 旧帮助函数0命中；Paddle调用2处保留于窄回调；Pipeline边界禁止符号0命中；必需路径0缺失；OCR业务测试4项；部署脚本解析0错误 | 通过 |
| Stage 1深度OCR Pipeline测试 | 用户在Qt Creator Release运行`ocr_detection_pipeline_test` | 字节清洗/顺序、精确OK、空NG、不等NG共4项业务测试通过 | 2026-08-12 14:24:19汇总`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| 纸巾测试子工程结构回归 | 用户在Qt Creator Release运行`tissue_roll_detector_baseline_test` | 嵌套`subdirs`后原4项业务测试仍通过 | 2026-08-12 14:24:46汇总`6 passed, 0 failed, 0 skipped`，3ms，退出码0 | 通过（用户证据） |
| Stage 1深度OCR主程序门禁 | 用户在Qt Creator Release对`app/AutoOCRproject.pro`执行Run qmake、Rebuild、Run并切换深度OCR模式 | 主程序正常构建启动；模式切换和目标文本保持正常 | 2026-08-12用户确认“主程序也通过了” | 通过（用户证据） |
| Stage 1模板匹配切片边界 | 白名单核对、工程路径/子目标解析、旧调用和旧Widget判定搜索、Pipeline禁止依赖、正则字面量、测试方法、部署脚本与Git差异检查 | 只改当前10个文件；原算法调用各保留1处；Pipeline不依赖UI/算法类/PLC/存图；4项业务测试；状态与文档一致 | 10个文件且无额外差异；90个ID唯一；旧字符匹配/重叠调用各1处；旧Widget组合判定0处；Pipeline边界违规0处；测试4项；部署脚本和`git diff --check`通过 | 通过 |
| Stage 1模板匹配Pipeline测试 | 用户在Qt Creator Release运行`stamp_detection_pipeline_test` | 目标计数、两条件OK、字符NG、重叠/缺配置NG共4项业务测试通过 | 2026-08-12 14:58:41汇总`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| Stage 1模板匹配主程序入口 | 用户在Qt Creator运行主程序并检查识别模式下拉框索引0 | 主程序正常运行；现有界面入口保持“模板匹配”，不新增或重命名为“钢印模式” | 2026-08-12用户截图确认主程序运行，五个入口完整；`.ui`静态核对“模板匹配”为索引0，代码映射到`stamp_detection`和`slot_readAndDetect3` | 通过（用户证据+静态映射） |
| Stage 1字库匹配切片边界 | 白名单核对、工程路径/子目标解析、旧调用与Widget判定搜索、Pipeline禁止依赖、正则与测试方法检查、Git差异检查 | 只改当前10个文件；两个原字符匹配分支各保留1处；Pipeline不依赖UI/算法类/PLC/存图；4项业务测试；状态与文档一致 | 10个文件且无额外差异；90个ID唯一；原匹配调用由窄回调承接；多Profile选择及二维码优先链未改；Pipeline边界违规0处；测试4项；`git diff --check`通过 | 通过 |
| Stage 1字库匹配Pipeline测试 | 用户在Qt Creator Release运行`word_detection_pipeline_test` | 目标解析/回退、数量相等OK、少NG、多NG共4项业务测试通过 | 2026-08-12 15:43:04汇总`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| Stage 1字库匹配主程序门禁 | 用户在Qt Creator Release对主工程Run qmake、Rebuild、Run并切到“字库匹配” | 新Pipeline可编译链接；主程序正常启动；现有模式入口和目标文本正常 | 2026-08-12用户确认“没问题”；按用户风险接受条件不要求生产样本 | 通过（用户证据） |
| Stage 1二维码+三期切片边界 | 白名单核对、工程路径/子目标解析、原解码与日期子流程搜索、Pipeline禁止依赖、测试方法、受保护线程/解码/config差异和Git检查 | 只改当前10个文件；原DLL解码和日期子流程各保留1处；Pipeline不依赖UI/OpenCV/DLL/算法类/PLC/存图；4项业务测试；状态与文档一致 | 10个文件且无额外差异；90个ID唯一；用户门禁前为81/1/6/2，门禁后为81/0/7/2；多Profile、线程、ROI、DLL与配置文件0改动；Pipeline边界违规0处；测试4项；部署脚本和`git diff --check`通过；测试工程补充`QMAKE_PROJECT_DEPTH = 0`以规避jom误判相对依赖路径 | 通过 |
| Stage 1二维码+三期Pipeline测试 | 用户在Qt Creator Release运行`barcode_word_detection_pipeline_test` | 不可读短路、日期阶段不可用、可读+日期OK、可读+日期NG共4项业务测试通过 | 首次构建由jom报告相对依赖不存在，改用绝对项目深度后重新生成；2026-08-12 16:48:00汇总`6 passed, 0 failed, 0 skipped`，0ms，退出码0 | 通过（用户证据） |
| Stage 1二维码+三期主程序门禁 | 用户在Qt Creator Release对主工程Run qmake、Rebuild、Run并切到“二维码+三期” | 新Pipeline可编译链接；主程序正常启动；现有模式入口和目标文本正常 | 2026-08-12用户确认“没问题”；按用户风险接受条件不要求生产样本 | 通过（用户证据） |
| Stage 1多Profile选择切片边界 | 白名单、两线程旧重复选择逻辑、Selector依赖、测试方法、工程路径、功能状态和Git差异检查 | 只修改本切片文件；两线程都调用同一Selector；旧重复比较归零；4项业务测试；不改Profile加载、定位器、Widget结果链和硬件逻辑 | 11个文件且无额外差异；两线程各1处Selector调用、旧比较0处；Selector禁止依赖0处；测试4项；必需路径0缺失；受保护的Widget/定位器/相机/配置文件0改动；`git diff --check`通过 | 通过 |
| Stage 1 Profile选择测试 | 用户在Qt Creator Release运行`profile_pose_selector_test` | 无效跳过、最高分、同分先到优先、二维码多边形位姿映射共4项业务测试通过 | 2026-08-12 17:19:45汇总`6 passed, 0 failed, 0 skipped`，0ms，退出码0 | 通过（用户证据） |
| Stage 1多Profile选择主程序门禁 | 用户在Qt Creator Release对主工程Run qmake、Rebuild、Run并切换“字库匹配”与“二维码+三期” | 新Selector可编译链接；主程序正常启动；两个模式入口和目标文本正常 | 2026-08-12用户确认“主工程也正常”；按用户风险接受条件不要求生产样本 | 通过（用户证据） |
| Stage 1配方测试构建与编码修复 | 用户在Qt Creator/MSVC 2017 Release构建并运行`product_recipe_test` | 4项业务测试通过 | 首次因无BOM UTF-8中文字面量被代码页936解析而报`C4819/C2001/C1057`；改用C++11 Unicode转义后，2026-08-12 00:58用户复验为`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| 当前纸巾Pipeline测试 | 用户在Qt Creator Release tests工程Run qmake、构建并运行`tissue_roll_detector_baseline_test` | 配方6.0默认、显式阈值、空图和纯黑图4项业务测试通过 | 2026-08-12用户确认新纸巾Pipeline测试汇总`6 passed, 0 failed` | 通过（用户证据） |
| 固定样本清单 | 解析TSV必填字段并按模式分组 | 15行、五模式各3类 | 15行；每模式OK/NG/FAILURE各一项；0行缺关键字段 | 通过 |
| 新文件格式 | 搜索新增文档、测试和清单的行尾空白 | 0处 | 0处 | 通过 |
| 纸巾切片主程序构建/运行 | 用户在Qt Creator Release执行Run qmake、Rebuild、Run | 含纸巾Pipeline的当前主程序可构建、启动并加载配方阈值 | 2026-08-12 13:49主程序正常初始化、纸巾模式阈值显示正常，退出码0；`on_eliminatebutton_clicked()`无匹配为已登记旧槽告警，未阻止启动 | 通过（用户证据） |
| 测试构建/运行复验 | 用户在Qt Creator执行 | 4项业务测试通过 | 01:50旧产物仍崩溃；完成x64 OpenCV部署后，01:59和02:00连续两次显示4项业务测试及QtTest自动初始化/清理共`6 passed, 0 failed, 0 skipped`，耗时3ms，退出码0 | 通过（用户证据，2026-08-09） |
| 运行库部署复核 | 检查EXE目录、Makefile链接后命令和SHA-256 | 判断链接后部署是否真实生效 | EXE目录已有三个运行库；OpenCV目标与`third_party`源DLL的SHA-256同为`B92A...B7C1B`；说明复制确实生效，但源DLL本身错误 | 通过；排除部署步骤未执行 |
| 中间运行环境诊断 | `dumpbin /dependents`检查EXE及OpenCV二级依赖；核对Qt Creator运行配置、EXE目录和系统VC运行库 | 找到`main()`前仍可能缺失的运行环境 | EXE依赖`Qt5Test.dll/Qt5Core.dll`；原EXE目录无这两个DLL；`tests.pro.user`为自定义运行配置；系统已有OpenCV需要的VC运行库 | 已通过完整DLL部署排除Qt PATH缺口 |
| `0xc000007b`架构诊断 | 读取Release目录及仓库同名DLL的PE Machine；核对OpenCV import library和发布DLL导出 | 找到无效映像的确定来源 | EXE/Qt DLL=`x64(0x8664)`；误部署OpenCV=`x86(0x014C)`；链接用OpenCV `.lib`为x64；`dist/ShengYin` OpenCV为x64且覆盖EXE导入的全部28个符号 | 通过；根因确定 |
| 架构安全的运行库部署修复 | Release改从`dist/ShengYin`复制OpenCV；脚本读取目标EXE和每个DLL的PE Machine，再复制并校验SHA-256 | x64 EXE只接受x64运行库；不匹配在构建链接后步骤明确失败 | Agent完成脚本语法、路径和PE静态检查，未执行脚本/构建/测试；用户Qt Creator Release复验通过；仓库暂无可用x64 Debug OpenCV DLL，门禁固定使用Release | 通过 |
| Agent项目执行纪律 | Agent不执行构建、测试或运行 | 不产生Agent运行证据 | 未执行，符合强制规则 | 通过 |

## 用户Qt Creator门禁

### A. 当前多Profile选择切片：主程序构建与启动

1. Qt Creator打开 `app/AutoOCRproject.pro`。
2. 选择Qt 5.14.2、MSVC 2017 64-bit Kit和Release配置。
3. 当前切片新增`detection/common/profile_pose_selector.*`并修改主工程清单，必须先执行Run qmake。
4. Rebuild并Run，确认Release部署脚本完成、授权有效、主窗正常打开。
5. 分别切到界面“字库匹配”和“二维码+三期”模式，确认主界面仍能正常切换且目标文本保留；本门禁按用户决定不要求补做生产样本。
6. 反馈Qt Creator完整构建结论和启动结论；失败时提供首个错误及相关上下文，不要跳过。

### B. Stage 1 Profile选择测试

1. Qt Creator另开 `tests/tests.pro`。
2. 使用与主程序相同Kit，执行Run qmake。
3. Build并运行`profile_pose_selector_test`。
4. 预期4个业务测试全部通过：`invalidCandidatesAreIgnored`、`highestScoreCandidateIsSelected`、`equalScoreRetainsFirstCandidate`、`selectedBarcodePolygonUsesPoseTransform`。
5. 加上QtTest自动的初始化/清理，汇总应为`6 passed, 0 failed`。

### C. 本切片不要求重复运行的目标

`product_recipe_test`、`tissue_roll_detector_baseline_test`、`ocr_detection_pipeline_test`、`stamp_detection_pipeline_test`、`word_detection_pipeline_test`和`barcode_word_detection_pipeline_test`的源码/子工程均未修改，不作为本切片必选门禁。

### D. 已延期：五模式原入口固定样本

按`tests/baseline/sample_manifest.tsv`为每模式提供OK、产品NG、FAILURE各一份，共15项。每项从当前主界面入口运行并填写：图/采集来源、模板/Profile、结果、识别文本、Overlay、总数/NG/合格率增量、实际保存文件、PLC写序列和耗时。

### E. 已延期：关键非算法回归

- 有效/无效授权、第二实例、设置保存/损坏回退、五模式模板历史恢复。
- 制作模板的预览→冻结→框选→保存→重载；二维码不可读时只保留定位框；字符切割撤销/重名/保存。
- 相机打开/关闭、软触发和硬触发、开始/停止/再次启动；停止后无残留线程且最后结果保留。
- PLC连接/断开、模式和工艺参数读回、OK写0、NG写49后约100ms写0、延迟剔除与复位。
- 四类存图策略×三类图像组合的代表场景；记录正常盘和慢盘的写入耗时/内存。

## 已延期的Stage 0运行证据

| 数据 | 获取方法 | 当前值 | 门禁要求 |
|---|---|---|---|
| 五模式OK/NG/FAILURE结果 | 填`sample_manifest.tsv` | 待用户 | 15项完整且可重复 |
| 识别文本与Overlay | 原入口截图/日志/清单 | 待用户 | 每模式至少三类输入 |
| 统计口径 | 逐项记录总数、NG、合格率 | 待用户 | 与RES-001..003一致 |
| PLC写序列 | PLC模拟器/抓包/现场读回 | 待用户 | 地址、值、顺序、时间明确 |
| 单帧内存 | 记录实际`cols*rows*elemSize()`及分辨率 | 待用户 | 至少彩色主路径一项 |
| 算法耗时 | Release固定样本各30次 | 待用户 | 每模式P50/P95 |
| 正常/慢盘存图 | 连续保存并记录任务、RSS、文件耗时 | 待用户 | 正常盘与受控慢盘各一组 |
| 停止/重启 | 软/硬触发循环10次 | 待用户 | 无重复结果、无残留线程 |

## 门禁检查

- [x] 所有当前功能在修改前已分配功能ID并达到`已基线`，或按计划标为`已延期`。
- [x] 当前切片的入口、调用链、正常结果、失败路径和副作用已写入功能矩阵。
- [x] Agent轻量静态检查全部通过；未执行构建、链接、测试目标或程序。
- [x] 用户已在Qt Creator确认主程序工程可运行：2026-08-09用户反馈通过。
- [x] 用户已在Qt Creator确认`tissue_roll_detector_baseline_test`构建和4项业务测试通过：2026-08-09连续两次`6 passed, 0 failed`、退出码0。
- [x] 用户已在Qt Creator Release确认`product_recipe_test`编码修复后的4项业务测试通过：2026-08-12 00:58，合计`6 passed, 0 failed`、退出码0。
- [x] 用户已确认当前主工程Run qmake/Rebuild/Run以及`tissue_roll_detector_baseline_test`复验均通过：2026-08-12。
- [ ] 已从原入口完成15项固定样本回归：待用户填写清单。
- [ ] UI、提示、设置、文件、统计、存图、日志和硬件副作用已有实际运行证据：待用户验证。
- [x] 用户已明确接受上述两项延期风险，并要求以源码静态基线继续Stage 1；延期不等于验证通过。
- [x] Stage 1基础配方切片的主工程、配方测试和旧纸巾测试门禁全部通过；旧功能入口未迁移，对应功能ID恢复`已基线`。
- [x] 纸巾切片已从旧代码中提取Pipeline并接入软/硬触发线程；旧算法函数、结果信号与收尾链保留。
- [x] 纸巾切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 纸巾切片用户Qt Creator主工程和`tissue_roll_detector_baseline_test`门禁通过：2026-08-12 13:49主程序退出码0，阈值显示正常，测试`6 passed, 0 failed`。
- [x] 深度OCR切片已将识别编排、文本清洗和判定提取到Pipeline；旧Paddle调用及UI/统计/存图/PLC收尾保留。
- [x] 深度OCR切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 深度OCR切片`ocr_detection_pipeline_test`门禁：2026-08-12 14:24:19，`6 passed, 0 failed`，退出码0。
- [x] 纸巾测试子工程结构回归：2026-08-12 14:24:46，`6 passed, 0 failed`，退出码0。
- [x] 深度OCR切片用户Qt Creator主工程Run qmake/Rebuild/Run及OCR模式切换门禁：2026-08-12用户确认通过。
- [x] 模板匹配切片已将目标字符计数、旧算法窄回调编排和两条件AND判定提取到Pipeline；原定位/算法/结果收尾保留。
- [x] 模板匹配切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 模板匹配切片`stamp_detection_pipeline_test`门禁：2026-08-12 14:58:41，`6 passed, 0 failed`，退出码0。
- [x] 模板匹配切片主程序入口门禁：2026-08-12用户截图确认主程序运行且界面“模板匹配”入口存在；“钢印模式”是此前不准确的验证用语，不改变现有UI名称。
- [x] 字库匹配切片已将目标字符解析、原算法窄回调和数量判定提取到Pipeline；多Profile选择、二维码优先链和结果收尾保留。
- [x] 字库匹配切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 字库匹配切片`word_detection_pipeline_test`门禁：2026-08-12 15:43:04，`6 passed, 0 failed`，退出码0。
- [x] 字库匹配切片主程序Run qmake/Rebuild/Run及“字库匹配”模式切换门禁：2026-08-12用户确认“没问题”。
- [x] 二维码+三期切片已将读码优先、不可读短路、日期窄回调和组合判定提取到Pipeline；多Profile、ROI、DLL解码/缓存和结果收尾保留。
- [x] 二维码+三期切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 二维码+三期切片`barcode_word_detection_pipeline_test`门禁：2026-08-12 16:48:00，`6 passed, 0 failed`，退出码0。
- [x] 二维码+三期切片主程序Run qmake/Rebuild/Run及模式切换门禁：2026-08-12用户确认“没问题”。
- [x] 二维码+三期切片已创建独立提交`f2658f2`。
- [x] 开始多Profile选择切片时工作区干净，HEAD为`f2658f2`。
- [x] 多Profile选择切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] `profile_pose_selector_test`门禁：2026-08-12 17:19:45，`6 passed, 0 failed`，退出码0。
- [x] 多Profile选择主程序Run qmake/Rebuild/Run及两个模式切换门禁：2026-08-12用户确认主工程正常。

## 本地提交记录

| 提交 | 阶段/切片 | 功能ID | 内容 | 验证 |
|---|---|---|---|---|
| `030aaa0` | Stage 0功能盘点 | 全部90个ID | 功能矩阵、执行记录、固定样本清单 | Agent静态核对通过；纯文档提交 |
| `b783026` | Stage 0首个离线测试 | SET-010、DET-005 | tests工程、纸巾基线测试和架构安全的测试运行库部署 | Qt Creator Release连续两次测试通过 |
| `8d9c400` | Stage 1基础配方 | SET-003、SET-010、TPL-006、TPL-008、TPL-009 | ProductRecipe、Schema 1 JSON校验、只读运行快照和配方测试 | 主工程、`product_recipe_test`和纸巾基线测试均通过 |
| `e590703` | Stage 1纸巾配方与检测 | SET-010、DET-005、RUN-001 | 唯一6.0默认、显式参数副本、纸巾Pipeline和离线测试 | Agent静态检查、主程序启动/阈值显示和纸巾测试`6 passed, 0 failed`均通过 |
| `c8e3113` | Stage 1深度OCR模式 | DET-004 | OCR识别窄回调、字节清洗、换行拼接、精确判定和离线测试 | Agent静态检查、OCR/纸巾测试及主程序OCR模式门禁均通过 |
| `5113ea5` | Stage 1模板匹配模式 | DET-002 | 钢印+字符检测窄回调、目标计数、两条件AND判定和离线测试 | Agent静态检查、钢印Pipeline测试及主程序“模板匹配”入口门禁均通过 |
| `a7404c1` | Stage 1字库匹配模式 | DET-003 | 目标字符解析、原字符匹配窄回调、数量判定和离线测试 | Agent静态检查、字库Pipeline测试及主程序“字库匹配”入口门禁均通过 |
| `f2658f2` | Stage 1二维码+三期模式 | DET-006 | 读码优先、失败短路、日期子流程组合判定和离线测试 | Agent静态检查、二维码Pipeline测试及主程序模式门禁均通过 |

## 未解决事项

| 问题 | 风险 | 是否阻塞当前门禁 | 下一步/需要谁确认 |
|---|---|---|---|
| 五模式实际固定样本和模板尚未登记 | 不能建立可重复算法/结果基线 | 不阻塞当前开发；阻塞最终验收 | 用户明确接受风险并延期，后续可补样本路径 |
| PLC、硬触发、停止/重启无本轮现场证据 | 不能确认外部副作用基线 | 不阻塞当前Stage 1；阻塞硬件替换与最终验收 | 用户明确延期；Stage 1保持旧硬件主链不变 |
| 单帧内存、P50/P95、慢盘存图尚无数值 | 不能量化Stage 2/3是否退化 | 不阻塞当前Stage 1；阻塞性能验收 | 用户明确延期；后续有条件时补测 |

## 结论

- 当前切片：Stage 1多Profile最高分选择公共职责；代码、测试工程、Agent静态检查和用户Qt Creator门禁均已完成。
- 当前阶段：Stage 1进行中；人工样本与现场证据按用户明确决定延期，不声称最终产品验收已满足。
- 功能状态计数：待盘点0 / 已基线81 / 迁移中0 / 已验证7 / 已延期2 / 已确认删除0。
- 下一允许动作：形成多Profile选择独立提交，然后继续Stage 1配方加载与安全保存的下一个最小切片。
