# OCRGangYin 重构执行记录

## 仓库与阶段状态

- 基线HEAD：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`
- 基线分支：`codex/repo-layout`
- 当前工作分支：`codex/ocrgangyin-refactor`（从基线HEAD新建）
- 当前阶段：Stage 1 配方与算法拆分（按用户风险接受条件进入）
- 当前切片：新建字库配方字符裁切后同步（用户门禁通过，待创建独立提交）
- 阶段结论：**Stage 1进行中**。模板配方事务发布边界已形成提交`e2e8b4c`。本次新建的单Profile字库模板已接到字符裁切完整成功点；Agent静态检查、20项配方存储测试和主程序实际发布门禁均通过。历史目录、多Profile选择和运行检测仍不切换。
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

## Stage 1已完成多Profile最高分选择切片

- 主要功能ID：`DET-003`、`DET-006`；`DET-001`的单Profile定位算法和`TPL-009`的多目录加载/部分成功策略只登记关联边界，本切片不修改其状态。
- 旧调用链：`MyThread`和`CameraThread`各自并行运行已加载Profile的`TrackingPoseMatcher::matchPrepared`→各自重复跳过无效Pose→映射二维码相对多边形→写入Profile索引→使用严格`score > bestScore`选中最佳项→发出原`DetectionPose`。
- 当前规则：无效候选不参与；分数更高才替换；分数相同保留加载顺序中先出现的Profile；选中后保留其名称、原Profile索引以及随anchor/angle映射的二维码多边形。
- 本切片目标：新增`detection/common/profile_pose_selector.*`并让软/硬触发线程共同调用；用内存Pose锁定无效跳过、最高分、同分优先级和二维码多边形映射。
- 保持边界：不改Profile加载/顺序、`TrackingPoseMatcher`参数和并行执行、软触发间隔、硬触发逐次收尾、字库/二维码Pipeline、Overlay、统计、存图、PLC、相机或DLL。

## Stage 1已完成RecipeStore切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-008`；旧入口尚未接入，因此本切片通过后只能恢复为`已基线`，不能冒充完整模板工作流已验证。
- 旧调用链：模板保存由`Widget::on_pushButton_5_clicked`先清空同名目录，再依次写原图、定位图、YAML和`app_settings.appset`；单模板选择由`on_pushButton_4_clicked`→`loadSettingsFromDir`读取旧私有配置和资源。覆盖中途失败可能留下不完整目录。
- 本切片目标：新增`recipes/recipe_store.*`，按不可变UUID定位配方目录；写入同级唯一临时目录，复制Schema引用的非空资源文件，重新加载并校验JSON/ID/资源及可注入资源验证器，通过后才把旧目录改名备份并提交新目录。
- 失败与回退：非法Schema、缺少/空/符号链接资源、资源校验失败、旧目录备份失败或新目录提交失败均返回失败；提交前失败不接触旧目录，提交失败时尝试恢复备份；加载失败不修改调用方传入的配方对象。
- 保持边界：不修改旧UI、模板绘制/坐标换算、旧INI、图片/YAML生成、Profile缓存、检测、线程、相机、PLC、统计或存图；本切片只建立后续入口迁移需要的存储基础。

## Stage 1已完成Profile配方数据合同切片

- 主要状态功能ID：`SET-003`、`TPL-006`、`TPL-008`；关联但保持基线的功能ID：`TPL-009..015`，因为本切片只建立其数据字段，不切换对应界面入口。
- 旧数据事实：`TemplatePrivateSettings`保存目标文本、默认70的图像阈值、`cv::Rect2d`定位框、`hasValidBoxes`、字符框/源图尺寸和二维码格式、padding、60ms预算及fallback；`Widget::WordTemplateProfile`再叠加Profile名称、定位图、YAML多边形和字符图片缓存。
- 本切片目标：新增`RecipeProfile`、`RecipeCharacterBox`和`BarcodeRecipeParameters`；非纸巾配方通过`parameters.profiles`往返Profile顺序、名称、目标文本、阈值、定位框、字符框/源图尺寸、二维码参数和对顶层资源表的逻辑键引用。
- 校验边界：纸巾配方禁止混入Profile；非纸巾至少一个Profile；拒绝无效定位框、越界阈值、非法字符框/源图尺寸、非法二维码参数和悬空资源键引用；JSON失败继续保持调用方对象不变。
- 保持边界：不修改`Widget`、`AppSettingsManager`、旧目录/INI、ROI/YAML/图片生成、Profile选择与编辑、检测、相机、PLC、统计或存图；不要求兼容尚未投入使用的旧版新Schema数据。

## Stage 1已完成旧模板Profile映射切片

- 受影响功能ID：`SET-003`、`TPL-009`、`TPL-011..015`；七项开始前均为`已基线`。`TPL-008`的非字库单模板入口不经过本加载链，保持基线。
- 旧调用链：字库/二维码选择一个或多个目录→`loadWordTemplateProfileFromDir`→读取`app_settings.appset`、YAML、定位图和字符图→构造`WordTemplateProfile`→保留有效项并维持部分成功策略。
- 本切片目标：新增`recipes/template_profile_mapper.*`，无损转换`TemplatePrivateSettings`与`RecipeProfile`的目标文本、阈值、定位框、字符框/源图尺寸、二维码选项和资源键；旧Profile加载成功后额外缓存规范化RecipeProfile，旧目标字符、阈值和字符切割写入成功后同步刷新该缓存。
- 保持边界：不切换旧INI或目录格式，不调用RecipeStore保存，不改变旧资源校验、自动修复、部分成功、提示、Profile顺序、检测缓存、UI、相机、PLC、统计或存图。

## Stage 1已完成字库家族Profile资产清单切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-009`、`TPL-015`；四项开始前均为`已基线`。
- 旧资源事实：Profile目录根部固定使用`tracking_template.bmp`、`calibrate_config.yaml`和可选`template_raw.png`；字符模板为根部`png/jpg/jpeg/bmp/tiff`，排除`template_raw.png`、`tracking_template.bmp`、`template_ring.bmp`，精确文件名及`_/-/(n)`变体均参与旧加载。
- 本切片目标：新增`recipes/template_profile_assets.*`，按最终Profile索引生成不冲突的顶层资产键、`assets/profiles/<index>/...`目标相对路径、源绝对路径和Profile逻辑键；集合加载/恢复、保存重载及字符切割成功后刷新内存清单。
- 保持边界：不改变旧文件名、目录、字符枚举顺序、加载/部分成功判定、UI提示、RecipeStore正式写入、检测、相机、PLC、统计或存图；资源不存在时不抢先改变旧入口结果。

## Stage 1已完成多Profile ProductRecipe候选组装切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-009`；三项开始前均为`已基线`。
- 旧数据交接事实：字库家族的`m_wordTemplateProfiles`已按最终加载顺序保留`RecipeProfile`与`TemplateProfileAssetManifest`；`RecipeStore::saveRecipe`接收完整`ProductRecipe`和与资产键对齐的源路径表，两者之间尚缺独立组装边界。
- 本切片目标：新增`recipes/template_recipe_assembler.*`，保留调用方提供的UUID、显示名和Word/BarcodeWord模式，按原顺序拷贝Profile参数，以资产清单刷新每Profile的资源引用，合并顶层资产目标和源路径，最后通过`validateProductRecipe`。
- 失败语义：空Profile集、非字库家族模式、清单三表不对齐、必需定位/YAML资产缺失、悬空引用、键或目标路径冲突及最终Schema无效均拒绝，调用方输出对象保持不变。
- 保持边界：不自行生成或改写产品身份，不执行RecipeStore写盘，不接管旧模板选择/保存，不修改Widget、旧INI、检测、相机、PLC、统计或存图。

## Stage 1已完成RecipeStore配方目录查询切片

- 受影响功能ID：`TPL-008`、`TPL-009`、`TPL-016`；三项在实现期间进入`迁移中`，测试和主程序门禁通过后因旧入口尚未切换而恢复`已基线`。
- 旧查询事实：单模板入口直接选择一个旧目录；字库家族允许选择多个旧目录并保留有效项；全局设置按模式记忆旧目录路径。此前新RecipeStore只能按已知UUID加载，不能给后续选择入口提供目录清单。
- 本切片目标：新增`RecipeStore::listRecipes`，只枚举`recipesRoot/<canonical-lowercase-uuid>`，复用完整配方加载/资源校验，输出UUID、显示名、模式和Profile数量，并按显示名（忽略大小写）及UUID稳定排序。
- 失败与隔离：非空但不存在的根目录视为空仓库且不创建目录；根路径为空、不是可读目录或为符号链接时失败并保持调用方输出不变；损坏的规范UUID目录进入`invalidRecipes`并继续列出其他有效项；临时目录和普通非UUID目录忽略。
- 保持边界：不连接Widget，不改变旧单/多目录选择、部分成功、路径记忆、模式恢复、保存、检测、线程、相机、PLC、统计或存图；本切片也不修改任何qmake工程清单。

## Stage 1已完成已选配方解析切片

- 受影响功能ID：`TPL-008`、`TPL-009`、`TPL-016`；三项在实现期间进入`迁移中`，测试和主程序门禁通过后因旧入口尚未切换而恢复`已基线`。
- 旧入口约束：旧选择完成后，运行和编辑直接依赖模板根部的`tracking_template.bmp`、`calibrate_config.yaml`、字符图片及私有INI；新RecipeStore把资源放在UUID目录的`assets/`下，因此仅把目录列表接到按钮会破坏编辑和运行预检。
- 本切片目标：新增`recipes/recipe_selection.*`，按UUID调用RecipeStore完整加载，要求配方模式与当前模式精确一致，创建不可变配方快照，并按JSON Profile顺序把每个资源角色解析为配方目录内的绝对路径。
- 失败语义：配方加载、模式、资源引用、文件或必需`trackingTemplate`/`calibration`角色任一无效即失败；调用方原`RecipeSelection`保持不变。纸巾配方没有Profile，仍可形成无资源选择快照。
- 保持边界：不连接Widget，不读取或写入全局设置，不改变旧目录选择、部分成功、模式恢复、编辑、模板保存、检测、线程、相机、PLC、统计或存图。

## Stage 1当前字库家族Profile加载计划切片

- 受影响功能ID：`SET-003`、`TPL-009..015`；八项实现期间由`已基线`进入`迁移中`，用户门禁通过后因新兼容装配尚未接管UI而恢复`已基线`。
- 旧加载事实：`loadWordTemplateProfileFromDir`从旧目录读取私有INI、`calibrate_config.yaml`、`tracking_template.bmp`和根目录字符图片；目标字符按精确文件名优先，再加载`_/-/(n)`变体并记录其目标索引。缺必需定位/YAML会整项失败，字符不完整则Profile仍加载但字符缓存清空并提示待设置。
- 本切片目标：新增`recipes/template_profile_load_plan.*`，把`ResolvedRecipeProfile`整理为定位、YAML、可选原图和按目标稳定排序的字符加载项；Widget新增未接管UI的`loadWordTemplateProfileFromRecipeSelection`，复用现有`CalibrationData`、`cv::imdecode`、`templatePrivateSettingsFromRecipeProfile`和`TemplateMatch::prepareDigitTemplates`建立同形旧缓存。
- 失败语义：必需路径、YAML、定位图或二维码四点无效时失败且不改写调用方Profile；目标为空、缺字符或字符解码失败时沿用旧待设置语义，不保留部分字符缓存。加载计划自身也只在完整成功后改写输出。
- 保持边界：兼容装配当前没有调用方；不切换选择按钮、模式路径记忆、编辑器显示/写入、RecipeStore保存、运行Profile安装、检测、线程、相机、PLC、统计或存图。

## Stage 1当前字库配方批量加载与原子装配切片

- 受影响功能ID：`SET-003`、`TPL-009..015`；八项实现期间由`已基线`进入`迁移中`，用户门禁通过后因新批量装配尚未接管UI而恢复`已基线`。
- 旧入口事实：字库家族“选择模板”逐个旧目录加载，坏目录可跳过，只要至少一个成功就整体替换`m_wordTemplateProfiles`；模式恢复也按保存路径逐项恢复。旧目标字符解析函数被选择、字符切割、单项/批量确认和新单Profile装配共同调用。
- 本切片目标：把旧目标字符解析原样提取到`recipes/template_profile_load_plan.*`，为整个`RecipeSelection`生成保持Profile顺序的原子`TemplateRecipeLoadPlan`；Widget新增未接管UI的批量兼容函数，先在局部容器装配全部旧缓存，再一次性交付。
- 失败语义：新配方任一Profile的必需资源、YAML、定位图或二维码四点硬失败时不改写调用方批量输出；字符目标为空、缺图或图片解码失败仍作为逐Profile待设置消息，不保留该Profile的部分字符缓存。
- 保持边界：不接“选择模板”按钮，不改变旧多目录部分成功、模式路径恢复、编辑器写盘、RecipeStore保存、检测、线程、相机、PLC、统计或存图。

## Stage 1当前已选配方可回存工作副本切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-009..015`；九项在实现期间由`已基线`进入`迁移中`，收到测试和主程序门禁结果前不恢复状态。
- 输入事实：`RecipeSelection`持有已校验的不可变`ProductRecipeSnapshot`、JSON Profile顺序和按角色解析到当前配方目录的绝对资源路径；`RecipeStore::saveRecipe`需要一个可编辑`ProductRecipe`和完整的资产键到源文件映射。
- 本切片目标：扩展`template_recipe_assembler`，把已选快照复制为独立可编辑工作副本，并逐Profile核对名称、资源角色、资产键和配方内部确切文件，形成可直接交给RecipeStore的`TemplateRecipeAssembly`；测试在系统临时目录修改显示名、回存、重载并核对Profile顺序和资源内容。
- 失败语义：空快照、非字库家族、Profile数量/映射不符、资源缺失/符号链接/空文件、路径不指向该资产在当前配方中的确切文件或存在未解析资产时失败；调用方原工作副本不改写。
- 保持边界：不连接Widget，不改变旧选择、编辑、字符切割、阈值设置或保存按钮；不读写用户模板/配方目录，不改变检测、线程、相机、PLC、统计、存图或运行快照。

## Stage 1当前多配方选择批次切片

- 受影响功能ID：`TPL-009`、`TPL-016`；两项由`已基线`进入`迁移中`，收到测试和主程序门禁结果前不恢复状态。
- 旧入口事实：字库家族文件夹对话框允许勾选多个目录，按选择顺序逐项加载；坏目录被跳过，只要至少一个模板有效就整体替换当前Profile集合，全部无效时保留旧集合。模式恢复同样逐项跳过无效路径。
- 本切片目标：在`recipes/recipe_selection.*`新增有序批次选择；对UUID大小写去重，对每个唯一ID复用精确模式和资源校验，成功项与拒绝原因分开返回，为后续按钮接入多个新配方建立纯数据合同。
- 失败语义：部分无效仍成功并保留有效选择顺序；全部无效、空输入或空输出指针时失败且调用方原`RecipeSelectionBatch`不改写；成功时错误字符串为空，拒绝详情在批次内显式保存。
- 保持边界：不连接Widget，不写设置或用户配方目录，不改变旧多目录对话框、Profile缓存、模式恢复、检测、线程、相机、PLC、统计或存图。

## Stage 1当前模板配方发布边界切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-009`；三项由`已基线`进入`迁移中`，收到测试和主程序门禁结果前不恢复状态。
- 旧入口事实：`on_pushButton_5_clicked`先直接重建旧产品目录并写原图、定位图、YAML和私有设置，随后重载单个字库Profile；成功提示后用户还可立即进入字符切割，字符图因此可能晚于首次保存生成。
- 本切片目标：新增`recipes/template_recipe_publisher.*`，接收配方头和规范Profile源，复用组装器形成完整RecipeStore入参，事务保存成功后再按同一模式加载不可变`RecipeSelection`，为后续旧入口切换提供单一发布调用。
- 失败语义：空输出、组装或RecipeStore失败均不改写调用方选择；组装阶段失败不接触Store，Store阶段继续沿用临时目录/备份回滚合同并保留上一正式配方。
- 保持边界：发布接口当前无Widget调用者；测试只写系统临时目录，不写实际AppData；不改变旧保存、字符切割、提示、选择、模式记忆、检测、线程、相机、PLC、统计或存图。

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
| SET-003、TPL-006、TPL-008 | 已基线 | 已基线 | 新增RecipeStore加载、资源复制、临时目录重载校验和可回滚整目录替换；旧入口本切片不改，基础能力验证后恢复基线状态 | Agent静态检查、`recipe_store_test`、重组后的`product_recipe_test`和主工程门禁均通过 |
| SET-003、TPL-006、TPL-008 | 已基线 | 已基线 | 为旧模板保存/加载入口补齐类型化Profile参数、字符框、二维码参数及资源引用JSON合同；旧入口本切片不改，基础能力验证后恢复基线状态 | Agent静态检查、两个配方测试和主工程门禁均通过；TPL-009..015保持旧入口基线状态 |
| SET-003、TPL-009、TPL-011..015 | 已基线 | 已基线 | 旧字库家族Profile加载及后续设置写入成功后同步生成规范化RecipeProfile缓存，并用双向映射测试锁定字段无损 | Agent静态检查、`product_recipe_test`和主工程门禁均通过；旧INI和成功/失败策略不变；TPL-008保持基线 |
| SET-003、TPL-006、TPL-009、TPL-015 | 已基线 | 已基线 | 按Profile顺序和旧目录实际资源生成可交给RecipeStore的规范化资产清单，并在资源集合改变后刷新 | Agent静态检查通过；2026-08-12用户确认扩展后的`product_recipe_test`与主工程Run qmake/Rebuild/Run及模式切换全部通过；旧加载/保存/切割行为不变 |
| SET-003、TPL-006、TPL-009 | 已基线 | 已基线 | 将多Profile参数和资产清单组装为完整`ProductRecipe`候选及与RecipeStore入参一致的源资产表 | Agent静态检查通过；2026-08-12用户确认扩展后的`product_recipe_test`与主工程Run qmake/Rebuild/Run及模式切换全部通过；旧选择/保存/检测行为不变 |
| TPL-008、TPL-009、TPL-016 | 已基线 | 已基线 | 为后续新配方选择与按模式恢复建立只读目录查询边界；本切片不接管旧入口 | Agent静态检查通过；2026-08-12用户确认扩展后的`recipe_store_test`和主程序门禁均无问题 |
| TPL-008、TPL-009、TPL-016 | 已基线 | 已基线 | 为后续选择入口建立精确模式、不可变快照、Profile顺序和资源角色解析边界；本切片不接管旧入口 | Agent静态检查通过；2026-08-12用户确认`recipe_store_test`为`9 passed, 0 failed`且主程序正常 |
| SET-003、TPL-009..015 | 已基线 | 已基线 | 将已解析的新配方Profile整理为旧运行/编辑缓存可消费的资源加载计划，并提供尚未接管UI的兼容装配；因尚未切换旧入口，不冒充完整工作流已验证 | Agent静态检查通过；2026-08-12用户确认`recipe_store_test`预期11项及主程序旧模板入口均无问题 |
| SET-003、TPL-009..015 | 已基线 | 已基线 | 统一旧目标字符解析，为整个新配方建立保持Profile顺序且失败不改写的批量计划和旧缓存候选；因尚未切换旧入口，不冒充完整工作流已验证 | Agent静态检查通过；2026-08-12用户确认`recipe_store_test`预期13项及主程序旧模板入口均无问题 |
| SET-003、TPL-006、TPL-009..015 | 已基线 | 已基线 | 从不可变已选配方生成独立可编辑工作副本和当前配方内部资源源表，为后续UI编辑后安全回存建立边界；旧入口不切换 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期15项及主程序门禁均无问题 |
| TPL-009、TPL-016 | 已基线 | 已基线 | 为新选择入口锁定有序多配方、重复ID、部分无效隔离及全部无效不改写合同；旧入口不切换 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期17项及主程序门禁均无问题 |
| SET-003、TPL-006、TPL-009 | 已基线 | 已基线 | 建立规范Profile组装、事务提交和同模式重新选择的一体化发布边界；旧保存入口不切换 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期19项及主程序旧入口门禁均无问题 |
| TPL-006、TPL-015 | 已基线 | 已基线 | 为本次新建的单Profile字库模板建立会话内稳定UUID草稿，只在字符裁切、设置重读和目标模板重载完整成功后事务发布；旧加载和运行入口不切换 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期20项通过，主程序日志与配方UUID目录证明二维码+三期单字符模板发布成功 |
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
| ProductRecipe持久化 | `app/recipes/recipe_store.*` | 新增按UUID加载和事务式整目录保存 | 临时目录写入并重载通过后才提交；失败保留旧正式目录；支持注入模式资源校验 | 旧`AppSettingsManager`和Widget模板入口保持不变 |
| RecipeStore离线回归 | `tests/recipe_tests/recipe_store_test/` | 新增4项临时目录测试；`recipe_tests`改为包含两个独立目标的subdirs工程 | 覆盖首次保存/加载、整目录覆盖、源资源缺失保旧、资源校验或目录提交失败保旧 | 原`product_recipe_test.cpp`不改，另建其嵌套工程清单 |
| 已选配方运行资源解析 | `app/recipes/recipe_selection.*` | 从已验证Store加载选中UUID，检查当前模式并按Profile顺序解析资源角色绝对路径 | 失败不改写输出；只生成只读快照和路径，不解码图像/YAML或执行检测 | 旧`on_pushButton_4_clicked`、编辑器和运行Profile保持不变 |
| 已选配方解析回归 | `tests/recipe_tests/recipe_store_test/recipe_store_test.cpp` | 新增两项临时目录测试 | 两Profile顺序/资源隔离、模式不匹配、缺必需资源和失败不改写 | 测试不读取用户配方目录，不加载OpenCV或主界面 |
| 字库Profile加载计划 | `app/recipes/template_profile_load_plan.*` | 将已解析角色路径转换为定位/YAML/原图及按目标索引排序的字符加载项 | 精确字符优先、`_/-/(n)`变体随后；任一目标缺资源时清空全部字符加载项并返回待设置提示 | 旧目录枚举和选择入口保持不变 |
| 新配方到旧字库缓存兼容装配 | `Widget::loadWordTemplateProfileFromRecipeSelection` | 复用原YAML/图片解码、Profile映射和字符预计算构造同形`WordTemplateProfile` | 必需资源失败不改写；字符不完整保留Profile但不保留部分字符缓存 | 私有函数当前无调用方，旧按钮/编辑/运行路径不变 |
| Profile加载计划回归 | `tests/recipe_tests/recipe_store_test/recipe_store_test.cpp` | 新增两项纯路径计划测试 | 锁定字符精确/变体顺序、目标索引、缺字符清空和失败不改写 | 不解码OpenCV、不读取用户配方目录 |
| Profile配方参数与JSON合同 | `app/recipes/product_recipe.*` | 新增Profile、字符框、二维码参数和顶层资源键引用；非纸巾参数写入`profiles`数组 | 拒绝无Profile、无效框/阈值/字符框/二维码参数和悬空资源引用；纸巾仍无模板Profile | 旧`TemplatePrivateSettings`、`WordTemplateProfile`和所有UI入口保持不变 |
| Profile合同离线回归 | `tests/recipe_tests/product_recipe_test.cpp`、`recipe_store_test.cpp` | 原4项业务测试内扩展完整Barcode Profile往返、非法Profile拒绝及Store事务回归 | 不读写用户模板目录，不加载OpenCV/Paddle/二维码DLL，不运行外部副作用 | 测试目标和工程清单不变 |

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
| 功能ID与状态 | 解析矩阵正式功能行（不含`DIFF-*`已知差异项）并检查ID/状态 | 90个唯一ID；Profile合同切片为78已基线、3迁移中、7已验证、2已延期 | 90/90唯一；78/3/7/2，与统计一致 | 通过 |
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
| Stage 1 RecipeStore切片边界 | 白名单、工程路径、Store依赖、事务步骤、测试方法、功能状态和Git差异检查 | 新Store只依赖Qt Core与ProductRecipe；临时写入/重载/备份/恢复路径存在；4项业务测试；旧Widget/AppSettingsManager及生产资源生成链0改动 | 10个文件且无额外差异；必需文件/工程引用0缺失；Store禁止依赖0处；临时重载、备份改名和提交失败恢复路径均存在；测试4项（含可控目录提交失败及旧目录恢复）；受保护生产文件0改动；`git diff --check`通过 | 通过 |
| Stage 1 RecipeStore测试 | 用户在Qt Creator Release运行`recipe_store_test` | 首次保存/加载、整目录覆盖、缺资源保旧、资源校验与目录提交失败保旧共4项业务测试通过 | 2026-08-12 18:14:46汇总`6 passed, 0 failed, 0 skipped`，105ms，退出码0 | 通过（用户证据） |
| ProductRecipe测试结构回归 | 用户在Qt Creator Release运行重组后的`product_recipe_test` | 原4项ProductRecipe业务测试仍通过 | 2026-08-12 18:15:33汇总`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| Stage 1 RecipeStore主程序门禁 | 用户在Qt Creator Release对主工程Run qmake、Rebuild、Run | 新Store可编译链接；主程序正常启动；旧模板入口未接入因而行为不变 | 2026-08-12用户确认“主窗口也正常启动” | 通过（用户证据） |
| Stage 1 Profile合同切片边界 | 白名单、依赖、Profile字段/JSON、资源引用、测试方法、功能状态和Git差异检查 | 只修改ProductRecipe、两个配方测试及文档；ProductRecipe保持Qt Core边界；Profile字段和非法值测试完整；旧生产入口0改动 | 7个文件且无额外差异；Recipe禁止依赖0处；14个Profile JSON关键字段0缺失；悬空资源引用校验存在；两个目标各4项业务测试；受保护生产文件0改动；90个ID唯一且78/3/7/2；`git diff --check`通过 | 通过 |
| Stage 1 Profile合同ProductRecipe测试 | 用户在Qt Creator Release运行`product_recipe_test` | 完整Barcode Profile往返、非法Profile拒绝和原纸巾/快照业务测试共4项通过 | 2026-08-12 18:54:57首次运行汇总`5 passed, 1 failed`、退出码1；默认`QSize()`为`(-1,-1)`，导致悬空资源测试被字符源图尺寸校验提前拒绝；显式初始化为`(0,0)`后，用户确认复验全部PASS | 通过（用户证据） |
| Stage 1 Profile合同RecipeStore回归 | 用户在Qt Creator Release运行`recipe_store_test` | 带Profile资源引用的保存/加载、覆盖及两类失败保旧共4项通过 | 2026-08-12 18:55:56首次运行汇总`2 passed, 4 failed`、退出码4；4项均因默认`QSize()`为`(-1,-1)`而在保存前校验失败；显式初始化为`(0,0)`后，用户确认复验全部PASS | 通过（用户证据） |
| Stage 1 Profile合同主程序门禁 | 用户在Qt Creator Release对主工程Rebuild、Run | 扩展后的ProductRecipe可编译链接；主窗口正常启动；旧模板入口行为不变 | 2026-08-12用户确认“主窗口启动正常” | 通过（用户证据） |
| Stage 1旧Profile映射切片边界 | 白名单、依赖、双向字段映射、旧加载/编辑接入点、测试和Git差异检查 | 只新增映射器、规范化内存缓存和映射测试；不改旧INI/资源校验/检测/硬件 | 10个工作区条目；主/测试工程各2个映射器条目；11类字段0缺失；1个加载入口及5个设置成功赋值全部同步、0个遗漏；5项ProductRecipe业务测试；受保护旧设置实现、字符切割对话框、检测线程、相机和PLC文件0改动；90个ID唯一且74/7/7/2；`git diff --check`通过 | 通过 |
| Stage 1旧Profile映射测试 | 用户在Qt Creator Release运行`product_recipe_test` | 新增映射业务测试通过，原4项业务测试继续通过 | 2026-08-12 20:29:09汇总`7 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| Stage 1旧Profile映射主程序门禁 | 用户Run qmake、Rebuild、Run并选择字库或二维码模式 | 主窗口正常启动；模式切换不受新缓存影响 | 2026-08-12用户确认主工程Run qmake、Rebuild、Run及模式切换没有问题 | 通过（用户证据） |
| Stage 1 Profile资产清单切片边界 | 白名单、旧资源枚举规则、Profile命名空间、刷新接入点、测试和Git差异检查 | 只新增内存资产清单及测试；不改变旧目录读写、检测或硬件 | 10个工作区条目；主/测试工程各2个清单文件条目；3张资产映射表；5类字符扩展名及3个系统模板排除项完整；3个Profile集合安装点和1个字符切割刷新点全部接入；6项ProductRecipe业务测试；受保护旧设置/切割对话框/RecipeStore/检测线程/相机/PLC文件0改动；90个ID唯一且77/4/7/2；`git diff --check`通过 | 通过 |
| Stage 1 Profile资产清单测试 | 用户在Qt Creator Release运行`product_recipe_test` | 新增资源清单业务测试通过，原5项业务测试继续通过 | 2026-08-12用户确认包含QtTest自动初始化/清理的预期`8 passed, 0 failed`全部通过 | 通过（用户证据） |
| Stage 1 Profile资产清单主程序门禁 | 用户Run qmake、Rebuild、Run并切换字库或二维码模式 | 主窗口正常启动；旧Profile加载与模式切换不变 | 2026-08-12用户确认Run qmake、Rebuild、Run及字库家族模式切换全部正常 | 通过（用户证据） |
| Stage 1多Profile配方组装切片边界 | 白名单、工程清单、组装依赖、身份/顺序/资产不变量、失败不改写、测试和Git差异检查 | 只新增纯Qt Core组装器及测试；不改Widget、RecipeStore、旧INI、检测或硬件 | 8个工作区条目；主/测试工程各2个组装器清单项；Word/BarcodeWord模式门禁、必需定位/YAML资产、三表对齐、键/目标冲突、最终Schema和成功后单点输出赋值均存在；7项ProductRecipe业务测试；现有生产源文0改动；90个ID唯一且78/3/7/2；`git diff --check`通过 | 通过 |
| Stage 1多Profile配方组装测试 | 用户在Qt Creator Release运行`product_recipe_test` | 两Profile顺序/参数/资产合并、不完整源映射、目标冲突及失败不改写通过，原6项业务测试继续通过 | 2026-08-12用户确认包含QtTest自动初始化/清理的预期`9 passed, 0 failed`全部通过 | 通过（用户证据） |
| Stage 1多Profile配方组装主程序门禁 | 用户Run qmake、Rebuild、Run并切换字库或二维码模式 | 新组装器可编译链接；主窗口正常启动；旧Profile加载与模式切换不变 | 2026-08-12用户确认Run qmake、Rebuild、Run及字库家族模式切换全部正常 | 通过（用户证据） |
| Stage 1配方目录查询切片边界 | 白名单、RecipeStore依赖、UUID过滤、完整校验、排序、失败不改写、测试、工程清单和功能状态检查 | 只修改RecipeStore、其现有测试和文档；不改qmake清单、Widget、旧设置、检测或硬件；5项业务测试 | 6个工作区文件；规范UUID目录复用完整加载校验；非UUID/临时目录忽略；有效项稳定排序；损坏项隔离；无效根路径失败不改写；90个ID唯一且78/3/7/2；`git diff --check`通过 | 通过 |
| Stage 1配方目录查询测试 | 用户在Qt Creator Release运行`recipe_store_test` | 原4项业务测试及新增目录查询/损坏隔离测试共5项通过；加QtTest初始化/清理为`7 passed, 0 failed` | 2026-08-12用户确认按预期无问题 | 通过（用户证据） |
| Stage 1配方目录查询主程序门禁 | 用户在Qt Creator Release Rebuild并Run主工程，切换模板匹配或字库匹配 | 扩展后的RecipeStore可编译链接；主窗口正常；旧选择、模式恢复和检测行为不变 | 2026-08-12用户确认无问题 | 通过（用户证据） |
| Stage 1已选配方解析切片边界 | 白名单、新模块依赖、模式门禁、Profile顺序、资源解析、失败不改写、测试、工程清单和功能状态检查 | 只新增纯Qt Core选择解析器及两项测试；不改Widget、旧设置、检测或硬件 | 8个工作区文件；主/测试工程各2个新清单项；选择器禁止依赖UI/OpenCV/设置/检测/硬件；两Profile顺序和角色路径解析、精确模式、必需资源及单点输出赋值存在；7项RecipeStore业务测试；90个ID唯一且78/3/7/2；`git diff --check`通过 | 通过 |
| Stage 1已选配方解析测试 | 用户在Qt Creator Release运行`recipe_store_test` | 原5项及新增两项选择解析测试共7项业务测试通过；加QtTest初始化/清理为`9 passed, 0 failed` | 2026-08-12用户确认全部通过 | 通过（用户证据） |
| Stage 1已选配方解析主程序门禁 | 用户Run qmake、Rebuild、Run并切换模板匹配或字库匹配 | 新解析器可编译链接；主窗口正常；旧选择/编辑/运行行为不变 | 2026-08-12用户确认全部正常 | 通过（用户证据） |
| Stage 1字库Profile加载计划切片边界 | 白名单、加载计划依赖、字符顺序/目标索引、失败不改写、兼容装配复用旧解码、测试、工程清单和功能状态检查 | 新加载计划仅依赖Qt Core和RecipeSelection；兼容函数无UI调用方；两项新测试；旧选择/编辑/检测/硬件不变 | 10个工作区文件；主/测试工程各2个新清单项；加载计划禁止依赖UI/OpenCV/设置/检测/硬件；输出只在两条成功路径原子赋值；兼容装配声明/定义/调用为1/1/0并复用原标定、图片解码、字段映射和字符预计算；RecipeStore业务测试9项；97个总ID唯一、其中90个功能状态为73/8/7/2；受保护运行文件0改动；`git diff --check`通过 | 通过 |
| Stage 1字库Profile加载计划测试 | 用户在Qt Creator Release运行`recipe_store_test` | 原7项及新增两项加载计划测试共9项业务测试通过；加QtTest初始化/清理为`11 passed, 0 failed` | 2026-08-12用户反馈“没问题”，确认按当前门禁执行结果正常 | 通过（用户证据） |
| Stage 1字库Profile加载计划主程序门禁 | 用户Run qmake、Rebuild、Run并切换字库或二维码模式、选择一次旧模板 | 新文件和兼容函数可编译链接；旧模板加载、编辑显示及模式切换不变 | 2026-08-12用户反馈“没问题”，确认主程序及旧模板入口正常 | 通过（用户证据） |
| Stage 1字库配方批量加载切片边界 | 白名单、统一目标解析、批量Profile顺序、失败不改写、Widget局部缓存交付、测试、工程清单和功能状态检查 | 批量计划仅依赖Qt Core和RecipeSelection；Widget批量函数无UI调用方；旧入口/检测/硬件不变 | 8个工作区文件；纯计划禁止依赖UI/OpenCV/设置/检测/硬件；目标解析声明/定义1/1，旧Widget解析引用0、旧入口改由新解析调用6处；批量Widget装配声明/定义/调用1/1/0，单Profile装配由批量函数调用1处；RecipeStore业务测试11项；90个功能状态为73/8/7/2；`git diff --check`通过 | 通过 |
| Stage 1字库配方批量加载测试 | 用户在Qt Creator Release运行`recipe_store_test` | 原9项及新增两项批量计划测试共11项业务测试通过；加QtTest初始化/清理为`13 passed, 0 failed` | 2026-08-12用户反馈“没问题”，确认当前门禁结果正常 | 通过（用户证据） |
| Stage 1字库配方批量加载主程序门禁 | 用户Rebuild、Run并切换字库或二维码模式、选择一次旧模板 | 统一解析和未接管UI的批量装配可编译链接；旧选择、编辑和模式恢复不变 | 2026-08-12用户反馈“没问题”，确认主程序及旧模板入口正常 | 通过（用户证据） |
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

### A. 当前字库配方批量加载切片：主程序构建与启动

1. Qt Creator打开 `app/AutoOCRproject.pro`。
2. 选择Qt 5.14.2、MSVC 2017 64-bit Kit和Release配置。
3. 本切片只修改已在工程清单中的源码，无需再次Run qmake；直接Rebuild并Run。
4. 确认Release部署脚本完成、授权有效、主窗正常打开。
5. 切换到“字库匹配”或“二维码+三期”，用现成旧模板选择一次；确认Profile下拉框、目标文本、阈值和原图显示仍正常。统一目标解析保持旧正则，新批量装配尚未连接UI，因此界面不应新增列表或改变选择方式。
6. 反馈完整构建结论和启动结论；失败时提供首个错误及相关上下文。

### B. Stage 1字库配方批量加载测试

1. Qt Creator另开 `tests/tests.pro`。
2. 测试工程清单没有变化，无需再次Run qmake；Rebuild并运行`recipe_store_test`。
3. 确认原9项及新增两项批量计划测试共11项业务测试通过。
4. 加上QtTest自动初始化/清理，汇总应为`13 passed, 0 failed`。

### C. 本切片不要求重复运行的目标

`product_recipe_test`和所有`detection_tests`源码/子工程均未修改，不作为本切片必选门禁。

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
- [x] 多Profile选择切片已创建独立提交`1d24601`。
- [x] 开始RecipeStore切片时工作区干净，HEAD为`1d24601`。
- [x] RecipeStore切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] `recipe_store_test`门禁：2026-08-12 18:14:46，`6 passed, 0 failed`，退出码0。
- [x] 重组后的`product_recipe_test`门禁：2026-08-12 18:15:33，`6 passed, 0 failed`，退出码0。
- [x] RecipeStore切片主程序Run qmake/Rebuild/Run门禁：2026-08-12用户确认主窗口正常启动。
- [x] RecipeStore基础事务存储切片已创建独立提交`c3ef246`。
- [x] 开始Profile配方合同切片时工作区干净，HEAD为`c3ef246`。
- [x] Profile配方合同切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] Profile合同`product_recipe_test`门禁：2026-08-12首次运行`5 passed, 1 failed`；修复默认`QSize(-1,-1)`问题后，用户确认复验全部PASS。
- [x] Profile合同`recipe_store_test`回归：2026-08-12首次运行`2 passed, 4 failed`；修复同一默认尺寸问题后，用户确认复验全部PASS。
- [x] Profile合同主程序Rebuild/Run门禁：2026-08-12用户确认主窗口启动正常。
- [x] Profile配方数据合同切片已创建独立提交`0174003`。
- [x] 开始旧Profile映射切片时工作区干净，HEAD为`0174003`。
- [x] 旧Profile映射切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 旧Profile映射`product_recipe_test`门禁：2026-08-12 20:29:09，`7 passed, 0 failed`，退出码0。
- [x] 旧Profile映射主程序与模式切换门禁：2026-08-12用户确认Run qmake、Rebuild、Run均没有问题。
- [x] 旧Profile映射切片已创建独立提交`0e3af42`。
- [x] 开始Profile资产清单切片时工作区干净，HEAD为`0e3af42`。
- [x] Profile资产清单切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] Profile资产清单`product_recipe_test`门禁：2026-08-12用户确认预期`8 passed, 0 failed`全部通过。
- [x] Profile资产清单主程序门禁：2026-08-12用户确认Run qmake、Rebuild、Run及模式切换全部正常。
- [x] Profile资产清单切片已创建独立提交`1d4b5ea`。
- [x] 开始多Profile配方组装切片时工作区干净，HEAD为`1d4b5ea`。
- [x] 多Profile配方组装切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 多Profile配方组装`product_recipe_test`门禁：2026-08-12用户确认预期`9 passed, 0 failed`全部通过。
- [x] 多Profile配方组装主程序门禁：2026-08-12用户确认Run qmake、Rebuild、Run及模式切换全部正常。
- [x] 多Profile配方组装切片已创建独立提交`8ffabff`。
- [x] 开始配方目录查询切片时工作区干净，HEAD为`8ffabff`。
- [x] 配方目录查询切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 配方目录查询`recipe_store_test`门禁：2026-08-12用户确认预期`7 passed, 0 failed`无问题。
- [x] 配方目录查询主程序Rebuild/Run及模式切换门禁：2026-08-12用户确认无问题。
- [x] 配方目录查询切片已创建独立提交`772b58c`。
- [x] 开始已选配方解析切片时工作区干净，HEAD为`772b58c`。
- [x] 已选配方解析切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 已选配方解析`recipe_store_test`门禁：2026-08-12用户确认预期`9 passed, 0 failed`。
- [x] 已选配方解析主程序Run qmake/Rebuild/Run及模式切换门禁：2026-08-12用户确认全部正常。
- [x] 已选配方解析切片已创建独立提交`333f2ef`。
- [x] 开始字库Profile加载计划切片时工作区干净，HEAD为`333f2ef`。
- [x] 字库Profile加载计划切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 字库Profile加载计划`recipe_store_test`门禁：2026-08-12用户确认预期`11 passed, 0 failed`无问题。
- [x] 字库Profile加载计划主程序Run qmake/Rebuild/Run及旧字库模板门禁：2026-08-12用户确认无问题。
- [x] 字库Profile加载计划切片已创建独立提交`8f9b048`。
- [x] 开始字库配方批量加载切片时工作区干净，HEAD为`8f9b048`。
- [x] 字库配方批量加载切片Agent静态检查通过；未执行构建、链接、测试或主程序。
- [x] 字库配方批量加载`recipe_store_test`门禁：2026-08-12用户确认预期`13 passed, 0 failed`无问题。
- [x] 字库配方批量加载主程序Rebuild/Run及旧字库模板门禁：2026-08-12用户确认无问题。
- [x] 字库配方批量加载切片已创建独立提交`088d481`。
- [x] 开始已选配方可回存工作副本切片时工作区干净，HEAD为`088d481`。
- [x] 已选配方可回存工作副本切片Agent静态检查通过；7个白名单文件、纯配方层禁止依赖0处、接口声明/定义各1处、13项业务测试、90个功能ID唯一且状态72/9/7/2、`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 已选配方可回存`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`15 passed, 0 failed`无问题。
- [x] 已选配方可回存主程序Run qmake/Rebuild/Run及旧字库模板门禁：2026-08-13用户确认无问题。
- [x] 已选配方可回存工作副本切片已创建独立提交`694eebd`。
- [x] 开始多配方选择批次切片时工作区干净，HEAD为`694eebd`。
- [x] 多配方选择批次切片Agent静态检查通过；6个白名单文件、recipes层禁止依赖0处、接口声明/定义各1处、15项业务测试、90个功能ID唯一、状态79/2/7/2、Widget调用0处及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 多配方选择批次`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`17 passed, 0 failed`无问题。
- [x] 多配方选择批次主程序Rebuild/Run及模式切换门禁：2026-08-13用户确认无问题。
- [x] 多配方选择批次切片已创建独立提交`3f5b44b`。
- [x] 开始模板配方发布边界切片时工作区干净，HEAD为`3f5b44b`。
- [x] 模板配方发布边界切片Agent静态检查通过；8个白名单文件、recipes层禁止依赖0处、接口声明/定义各1处、主/测试工程清单4项、17项业务测试、Widget调用0处、90个功能ID唯一、状态78/3/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 模板配方发布`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`19 passed, 0 failed`无问题。
- [x] 模板配方发布主程序Run qmake/Rebuild/Run及旧字库模板门禁：2026-08-13用户确认无问题。
- [x] 模板配方发布边界已创建独立提交`e2e8b4c`。
- [x] 开始新建字库配方字符裁切后同步切片时工作区干净，HEAD为`e2e8b4c`。
- [x] 审计确认`CharacterTemplateCropDialog::saveTemplates()`成功后才`accept()`；主窗口只在`Accepted`且保存数大于0后重读字符框、刷新资产并重载目标字符模板。
- [x] 新建字库配方字符裁切后同步切片Agent静态检查通过；6个白名单文件、90个主功能ID唯一、状态79/2/7/2、主工程发布头/调用各1处、草稿准备调用1处、旧加载草稿清理2处、`recipe_store_test` 18项业务测试、recipes层禁止依赖0处及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 字符裁切同步`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`20 passed, 0 failed`无问题。
- [x] 主程序Run qmake/Rebuild/Run及实际发布门禁：2026-08-13用户新建二维码+三期单Profile模板`566`，目标字符`1`及字符图片保存成功，日志出现`[RECIPE_PUBLISH]`并给出UUID目录；未报告同步失败。
- [x] 2026-08-13首次主程序日志复核：程序启动、旧模板恢复、相机及二维码模板验证正常；新模板`D:/muban/454`已在00:48:06保存`1.png`，但目标文本为`1707202618072027`且只有字符`1`资源，目标模板重载未完整成功，因此按门禁设计未触发发布，AppData下也未生成`recipes`目录；需要用完整字符集或单字符目标复验。

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
| `1d24601` | Stage 1多Profile最高分选择 | DET-003、DET-006 | 无效Pose跳过、严格最高分、同分先到优先、Profile索引和二维码多边形映射 | Agent静态检查、Profile选择测试及主工程门禁均通过 |
| `c3ef246` | Stage 1 RecipeStore基础事务存储 | SET-003、TPL-006、TPL-008 | UUID目录加载、资源复制、临时目录重载校验、备份替换和提交失败恢复 | Agent静态检查、两个配方测试及主程序门禁均通过；旧模板入口保持基线 |
| `0174003` | Stage 1 Profile配方数据合同 | SET-003、TPL-006、TPL-008 | 类型化Profile/字符框/二维码参数、Schema 1 JSON及资源键引用校验 | Agent静态检查、两个配方测试及主程序门禁均通过；旧模板入口保持基线 |
| `0e3af42` | Stage 1旧Profile数据映射 | SET-003、TPL-009、TPL-011..015 | 旧私有设置与RecipeProfile双向映射、加载及编辑成功后的规范化内存缓存 | Agent静态检查、`product_recipe_test`及主工程模式切换门禁均通过；旧INI保持事实来源 |
| `1d4b5ea` | Stage 1 Profile资产清单 | SET-003、TPL-006、TPL-009、TPL-015 | 按Profile顺序枚举定位/YAML/原图/字符资源，建立逻辑键、目标相对路径和源绝对路径清单 | Agent静态检查、`product_recipe_test`及主工程模式切换门禁均通过；旧目录读写不变 |
| `8ffabff` | Stage 1多Profile配方组装 | SET-003、TPL-006、TPL-009 | 将规范化Profile和资产清单组装为完整ProductRecipe候选及RecipeStore源资产表 | Agent静态检查、`product_recipe_test`及主工程模式切换门禁均通过；旧选择/保存/检测不变 |
| `772b58c` | Stage 1 RecipeStore配方目录查询 | TPL-008、TPL-009、TPL-016 | 规范UUID目录枚举、完整校验、稳定排序和损坏目录隔离 | Agent静态检查、`recipe_store_test`及主工程模式切换门禁均通过；旧入口不变 |
| `333f2ef` | Stage 1已选配方解析 | TPL-008、TPL-009、TPL-016 | 精确模式门禁、不可变快照、Profile顺序及资源角色绝对路径解析 | Agent静态检查、`recipe_store_test`及主工程模式切换门禁均通过；旧入口不变 |
| `8f9b048` | Stage 1字库Profile加载计划 | SET-003、TPL-009..015 | 统一单Profile资源角色、字符精确/变体顺序及旧缓存兼容装配 | Agent静态检查、`recipe_store_test`及主工程旧模板入口门禁均通过；兼容装配尚未接管UI |
| `088d481` | Stage 1字库配方批量加载 | SET-003、TPL-009..015 | 统一目标字符解析，为整个RecipeSelection建立保持Profile顺序的批量加载计划和原子旧缓存候选 | Agent静态检查、`recipe_store_test`及主工程旧模板入口门禁均通过；批量装配尚未接管UI |
| `694eebd` | Stage 1已选配方可回存工作副本 | SET-003、TPL-006、TPL-009..015 | 从不可变选择复制独立可编辑配方，并把资产键绑定到当前配方内部确切文件供RecipeStore事务回存 | Agent静态检查、`recipe_store_test` 15项及主工程旧模板入口门禁均通过；旧编辑和保存入口不变 |
| `3f5b44b` | Stage 1多配方选择批次 | TPL-009、TPL-016 | 有序UUID批次、大小写去重、部分无效隔离和全部无效不改写 | Agent静态检查、`recipe_store_test` 17项及主工程门禁均通过；旧选择和模式记忆入口不变 |
| `e2e8b4c` | Stage 1模板配方发布边界 | SET-003、TPL-006、TPL-009 | 将规范Profile组装、RecipeStore事务提交和同模式重新选择合成原子发布入口 | Agent静态检查、`recipe_store_test` 19项及主程序旧模板入口门禁均通过；尚未连接Widget |

## 未解决事项

| 问题 | 风险 | 是否阻塞当前门禁 | 下一步/需要谁确认 |
|---|---|---|---|
| 五模式实际固定样本和模板尚未登记 | 不能建立可重复算法/结果基线 | 不阻塞当前开发；阻塞最终验收 | 用户明确接受风险并延期，后续可补样本路径 |
| PLC、硬触发、停止/重启无本轮现场证据 | 不能确认外部副作用基线 | 不阻塞当前Stage 1；阻塞硬件替换与最终验收 | 用户明确延期；Stage 1保持旧硬件主链不变 |
| 单帧内存、P50/P95、慢盘存图尚无数值 | 不能量化Stage 2/3是否退化 | 不阻塞当前Stage 1；阻塞性能验收 | 用户明确延期；后续有条件时补测 |

## 结论

- 当前切片：Stage 1新建字库配方字符裁切后同步；实现、Agent静态检查、配方存储测试和主程序实际发布门禁均完成，待创建独立提交。
- 当前阶段：Stage 1进行中；人工样本与现场证据按用户明确决定延期，不声称最终产品验收已满足。
- 功能状态计数：待盘点0 / 已基线81 / 迁移中0 / 已验证7 / 已延期2 / 已确认删除0。
- 下一允许动作：创建当前切片独立提交，然后把Widget中的新建配方草稿身份与发布状态抽入recipes层会话对象；仍不接管历史目录、多Profile选择、模式记忆或运行检测。
