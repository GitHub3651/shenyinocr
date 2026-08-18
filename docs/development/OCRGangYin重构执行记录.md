# OCRGangYin 重构执行记录

## 仓库与阶段状态

- 基线HEAD：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`
- 基线分支：`codex/repo-layout`
- 当前工作分支：`codex/ocrgangyin-refactor`（从基线HEAD新建）
- 当前阶段：新架构完全替换——阶段 0 治理文档已完成，等待阶段 1 第一轮。
- 当前切片：执行规则最终收口；纯组合根、阶段自动提交和阶段1 Schema冻结。
- 阶段结论：2026-08-15 固定四轮及其 Qt Creator 门禁是有效历史里程碑，但不是终局架构完成。2026-08-16 用户批准继续阶段 0～7 完全替换；当前正式状态为已基线0、迁移中0、已验证87、已延期0、已确认删除3（`MC-001..003`）。阶段 1 尚未开始。
- 构建纪律：Agent未运行、未间接调用、也未通过GUI触发任何qmake、编译、链接、测试目标或主程序。

> 2026-08-16 起，本文较早位置出现的“多相机保留/延期”“唯一未结论产品兜底 NG”“固定四轮完成即终局”等内容只记录当时事实，均由文末“新架构完全替换阶段”及正式新方案覆盖。

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

## Stage 1当前已发布模板配方可编辑会话切片

- 受影响功能ID：`SET-003`、`TPL-009..014`；七项由`已基线`进入`迁移中`，收到测试和主程序门禁结果前不恢复状态。
- 入口审计事实：`RecipeSelection`已经能解析UUID配方并装配旧形Profile缓存，但旧目标字符、阈值、字符裁切和二维码启动预检仍直接从`profile.dirPath`读写旧根目录；现在把“选择模板”按钮直接切到配方UUID目录会导致编辑写入错误位置。
- 本切片目标：新增`recipes/template_recipe_edit_session.*`，从不可变`RecipeSelection`建立独立可编辑工作副本；允许在保持Profile身份和资产绑定不变的前提下更新Profile参数，并使用同一UUID和配方内部资产事务回存，成功后刷新为新的不可变选择。
- 失败语义：无会话、Profile索引错误、Profile名称/资产绑定变化、Schema无效或RecipeStore保存失败均不改写已发布选择；失败时磁盘继续保留上一份完整配方，可编辑会话保留尚未发布的合法修改供重试。
- 保持边界：不连接“选择模板”、目标字符或阈值按钮；不改变旧目录选择、多Profile部分成功、模式记忆、字符裁切、运行检测、线程、相机、PLC、统计或存图。

## Stage 1当前新建配方目标字符重新发布接入切片

- 受影响功能ID：`SET-003`、`TPL-006`、`TPL-011`；三项由`已基线`进入`迁移中`，收到配方测试和主程序实际同UUID重新发布门禁结果前不恢复状态。
- 原入口调用链：本次新建单Profile字库模板完成字符裁切→`publishWordTemplateRecipeDraft`事务发布并取得`RecipeSelection`→建立`TemplateRecipeEditSession`；随后用户修改目标字符并点击“确认字符”→原字符图片完整性预检→原模板私有INI保存→旧Profile/字符缓存刷新→编辑会话更新同一Profile→`RecipeStore`使用原UUID事务重新发布。
- 正常结果：旧模板目录和当前内存缓存继续按原逻辑更新；已发布配方保持原UUID、Profile名称和资产绑定，只更新目标字符等本次Profile参数；日志新增`[RECIPE_PUBLISH] republished word recipe:`并输出与首次发布相同的UUID和目录。
- 失败语义：字符图片不完整或旧INI保存失败时保持原入口失败行为且不调用配方发布；编辑会话校验或RecipeStore保存失败时提示“目标字符已保存到当前模板，但产品配方重新发布失败”，上一份正式UUID配方仍完整保留。
- 保持边界：只覆盖当前进程中刚刚新建、字符裁切并成功发布的单Profile配方；选择旧目录、新配方UUID列表、多Profile/批量目标、阈值、字符资产增删、模式记忆、运行快照、检测、线程、相机、PLC、统计和存图均不切换。

## Stage 1当前新建配方图像阈值重新发布接入切片

- 受影响功能ID：`SET-003`、`TPL-013`；两项由`已基线`进入`迁移中`，收到配方测试和主程序实际同UUID重新发布门禁结果前不恢复状态。
- 原入口调用链：停止检测→当前字库家族Profile有效→`on_pushButton_3_clicked`校验0..100整数→保存旧模板私有INI→更新`profile.settings/recipeProfile`→发送原`ssim`阈值→刷新dirty状态→显示成功提示。
- 本切片目标：只在上述原成功点追加当前编辑会话的Profile更新和同UUID事务重新发布；成功日志继续使用`[RECIPE_PUBLISH] republished word recipe:`，并在原成功提示后说明配方已使用原编号重新发布。
- 失败语义：原参数或INI保存失败时不调用配方发布；编辑会话校验或RecipeStore保存失败时，旧模板INI、当前缓存和原`ssim`发送保持已成功结果，上一份正式UUID配方保持完整，并追加明确的重新发布失败提示。
- 保持边界：只覆盖当前进程刚新建、完成字符裁切并成功发布的单Profile配方；不接批量阈值、历史配方选择、目标字符批量、模式记忆、运行快照、检测判定、线程、相机、PLC、统计或存图。

## Stage 1当前编辑会话整批Profile原子更新切片

- 受影响功能ID：`SET-003`、`TPL-012`；两项由`已基线`进入`迁移中`，收到配方测试门禁前不恢复状态。
- 原行为：旧“批量确认字符”按Profile逐目录校验、写INI和刷新缓存，允许部分成功并汇总失败；新编辑会话已有单Profile更新与整配方事务发布，但若Widget逐项调用，后项失败会在会话中留下前项修改。
- 当前切片：`TemplateRecipeEditSession::updateProfiles`接收完整有序Profile候选，先检查数量、逐索引名称/资产绑定和整份Recipe Schema；全部成功才一次替换会话工作副本。
- 失败边界：未激活、数量不符、任一Profile身份/资产绑定不符或Schema无效时，会话中所有Profile保持调用前状态；尚未调用`publish`，不会写配方目录。
- 测试边界：新增两Profile原子更新测试，覆盖顺序保持、两项参数同时更新、第二项身份失败不留下第一项修改，以及成功发布后UUID保持。
- 保持边界：Widget、UI/qmake、旧INI、旧批量提示、历史配方选择、检测、设备和runtime均不修改。

## Stage 1当前批量目标字符重新发布接入切片

- 受影响功能ID：`SET-003`、`TPL-012`；两项由`已基线`进入`迁移中`，收到配方测试和主程序批量按钮实际同UUID重新发布门禁前不恢复状态。
- 原入口调用链：停止检测→字库家族已加载Profile→`on_batchTextsure_btn_clicked`解析目标字符→逐Profile检查目录和字符资源→逐目录保存旧私有INI→刷新Profile/字符缓存→按成功数显示全部失败、部分成功或全部成功提示。
- 当前切片：只在全部旧Profile保存成功后，把全部规范`RecipeProfile`按当前顺序交给已验证的`updateProfiles`，然后一次事务重新发布；单Profile“确认字符”和图像阈值仍通过同一发布日志边界。
- 失败语义：任一旧Profile失败时继续显示原部分成功汇总并且不调用配方发布；整批会话校验或RecipeStore失败时旧目录与缓存已经按原逻辑保存，正式UUID配方仍保持上一版本，原全部成功提示后追加重新发布失败原因。
- 保持边界：不修改字符资源加载、逐目录INI保存、成功计数、失败汇总、Profile顺序、批量阈值、历史配方选择、运行快照、检测、线程、相机、PLC、统计或存图。

## Stage 1当前批量图像阈值重新发布接入切片

- 受影响功能ID：`SET-003`、`TPL-014`；两项由`已基线`进入`迁移中`，收到配方测试和主程序批量阈值实际同UUID重新发布门禁前不恢复状态。
- 原入口调用链：停止检测→字库家族已加载Profile→`on_batchImageThresholdButton_clicked`校验0..100整数→逐Profile检查目录并保存旧私有INI→刷新规范Profile→当前Profile成功时发送`ssim`→刷新dirty→按成功数显示全部失败、部分成功或全部成功提示。
- 当前切片：只在全部旧Profile保存成功后，复用已验证的`publishWordTemplateRecipeEdits`把全部规范Profile按当前顺序一次事务重新发布。
- 失败语义：任一旧Profile失败时继续显示原全部失败/部分成功汇总并且不调用配方发布；整批会话校验或RecipeStore失败时旧目录、缓存、当前`ssim`和dirty已按原逻辑更新，正式UUID配方仍保持上一版本，原全部成功提示后追加重新发布失败原因。
- 保持边界：不修改0..100校验、逐目录INI保存、当前Profile `ssim`条件、dirty时机、成功计数、失败汇总、Profile顺序、目标字符、历史配方选择、运行快照、检测、线程、相机、PLC、统计或存图。

## Stage 1当前字库Profile只读资源访问桥切片

- 受影响功能ID：`SET-003`、`TPL-009..012`；五项实现期间由`已基线`进入`迁移中`，用户完成旧模板显示、单项/批量目标字符回归后恢复`已基线`；新资源角色分支要等历史配方选择接通后再升级验证状态。
- 入口审计事实：新`RecipeSelection`已解析每个Profile的`rawImage`和`character/<fileName>`绝对路径，但当前编辑器显示和目标字符确认仍只按`profile.dirPath`拼旧根目录文件。若直接把历史UUID配方接到选择按钮，原图显示和字符重载会访问错误位置。
- 当前切片：为`WordTemplateProfile`增加统一的只读资源访问边界；旧Profile没有资源角色表时完全回退原目录，新配方Profile则从已校验的资源角色读取原图和字符模板。单项与批量目标字符入口只替换读取来源，不改变解析、缓存替换或发布时机。
- 失败语义：空目标、缺少角色、资源失效或字符图片解码失败仍在修改设置前返回，不改写当前字符缓存；旧目录错误文本和旧资源读取顺序保持原状。
- 保持边界：不连接历史配方选择按钮，不修改旧INI写入、字符切割写入、阈值、Profile选择顺序、模式记忆、运行快照、检测、线程、相机、PLC、统计或存图。

## Stage 1当前字库Profile参数持久化分流切片

- 受影响功能ID：`SET-003`、`TPL-011..014`；五项实现期间由`已基线`进入`迁移中`，用户完成主工程旧模板单项/批量目标字符及阈值回归后恢复`已基线`；新配方不写旧INI要等历史选择入口接通后验证。
- 入口审计事实：单项/批量目标字符和图像阈值四个入口都直接调用`AppSettingsManager::saveTemplatePrivateSettings(profile.dirPath, ...)`。未来由`RecipeSelection`装配的Profile把`dirPath`指向UUID配方根目录，直接沿用会在`recipe.json`旁写入旧私有INI，且多Profile会互相覆盖。
- 当前切片：新增统一Profile设置保存边界；旧Profile的资源角色表为空时继续按原路径写私有INI，新配方Profile则要求有效编辑会话，并在独立会话副本中验证候选Profile身份、资产绑定和Schema；真正持久化仍由原成功点的同UUID `RecipeStore`事务发布完成。
- 失败语义：Profile索引、编辑会话、身份/资产绑定或Schema无效时在更新Widget缓存前失败；验证使用会话副本，不污染正式编辑会话，也不接触配方目录。旧INI写入失败路径和提示保持。
- 保持边界：不连接历史配方选择按钮，不修改目标解析、字符资源读取、0..100阈值、逐Profile部分成功、发布时机、字符切割、模式记忆、运行快照、检测、线程、相机、PLC、统计或存图。

## Stage 1当前二维码启动预检资源访问桥切片

- 受影响功能ID：`TPL-009`、`DET-006`、`RUN-001`；三项实现期间进入`迁移中`，用户确认二维码+三期旧模板可正常启动和停止后恢复原状态；新资源角色路径待历史配方入口接通后验证。
- 入口审计事实：历史配方Profile的编辑显示、字符加载和参数保存虽已有兼容边界，但`on_plcbtn_clicked`的二维码+三期启动预检仍按`profile.dirPath`拼接`tracking_template.bmp`和`calibrate_config.yaml`。直接接选择入口会导致UUID配方选择成功后在启动前被旧路径拒绝。
- 当前切片：启动预检复用统一Profile资源访问边界；旧Profile资源角色表为空时仍生成原目录路径，新配方Profile读取已解析的`trackingTemplate`和`calibration`绝对路径。
- 失败语义：文件缺失、解码失败、二维码/日期点数不足的原错误文本、聚合顺序和不启动结果保持；资源角色路径已由`RecipeSelection`校验，本切片不新增回退或自动修复。
- 保持边界：不连接历史配方选择按钮，不修改二维码DLL加载、Profile顺序、字符缓存检查、运行快照克隆、检测判定、ROI、线程、相机、PLC、统计或存图。

## Stage 1当前已发布配方单选入口切片

- 受影响功能ID：`SET-003`、`TPL-009..014`、`DET-003`、`DET-006`、`RUN-001`；实现期间保持`迁移中`，等待用户Qt Creator主程序门禁。`TPL-015`旧字符切割入口和`TPL-016`旧路径记忆继续保持基线，但新配方对应能力未在本切片冒充完成。
- 入口审计事实：RecipeStore目录、按模式选择、Profile加载计划、资源角色解析和编辑会话均已有测试与主工程旧入口门禁，但Widget没有可达入口，导致已发布配方只能在新建当次继续编辑，程序重启或切回后无法从配方库重新打开。
- 当前切片：在原Profile编辑行增加独立`RecipeSelectionDialog`。对话框只列当前字库家族模式的有效配方；用户单选一个配方后，先完整建立不可变选择、全部Profile候选和编辑会话，全部成功才原子替换当前缓存。一个配方内部的多Profile顺序和资源身份保持。
- 失败与安全语义：列表、模式、资源装配或会话建立失败均不修改当前模板；模式切换暂不把UUID配方根目录写入旧路径记忆；已发布配方字符资源增删暂由原入口明确拒绝，避免把文件误写到配方根目录。
- 保持边界：旧“选择模板”多目录入口不变；本切片一次只选择一个配方，不持久化UUID选择，不迁移字符裁切写入，不修改检测算法、线程、相机、PLC、统计或存图。

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
| TPL-006、TPL-015 | 已基线 | 已基线 | 将已验证的草稿身份、源目录门禁和成功发布后状态更新从Widget抽到`TemplateRecipeDraftSession`；Widget只保留Profile收集和提示 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期21项和主程序重复新建单字符模板发布均无问题 |
| SET-003、TPL-009..014 | 已基线 | 已基线 | 已发布配方建立独立编辑会话，保持UUID与内部资产绑定并支持参数修改后的事务回存；当前不接管UI | Agent静态检查通过；2026-08-13用户确认`recipe_store_test`预期23项及主程序旧入口回归无问题 |
| SET-003、TPL-006、TPL-011 | 已基线 | 已基线 | 将本次新建配方的成功发布选择绑定到当前编辑会话，并从原“确认字符”入口触发同UUID重新发布；完整历史配方工作流尚未切换，不冒充整个功能已迁移 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 23项、主程序首次/再次发布UUID一致及旧模板选择均无问题 |
| SET-003、TPL-013 | 已基线 | 已基线 | 在同一新建单Profile编辑会话中，从原图像阈值“设置”成功点触发同UUID重新发布；历史与批量入口尚未切换，因此不冒充完整模板工作流已验证 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 23项、主程序阈值同UUID重新发布、目标字符保持及旧模板回归均无问题 |
| SET-003、TPL-012 | 已基线 | 已基线 | 为后续批量参数接入新增保持Profile顺序、身份和资产绑定的整批原子更新边界；Widget和旧批量入口尚未切换，因此不冒充完整批量编辑已迁移 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 24项无问题 |
| SET-003、TPL-012 | 已基线 | 已基线 | 原“批量确认字符”全部成功后，把完整有序Profile候选原子更新到编辑会话并只执行一次同UUID事务重新发布；历史配方选择尚未切换，因此不冒充整个批量编辑已迁移 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 24项、主程序单次同UUID发布、阈值保持及旧模板回归均无问题 |
| SET-003、TPL-014 | 已基线 | 已基线 | 原“批量设置阈值”全部成功后，把完整有序Profile候选原子更新到编辑会话并只执行一次同UUID事务重新发布；历史配方选择尚未切换，因此不冒充整个批量编辑已迁移 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 24项、主程序单次同UUID发布、目标保持及旧模板回归均无问题 |
| SET-003、TPL-009..012 | 已基线 | 已基线 | 统一旧目录与新配方资源角色的只读原图/字符模板访问；历史配方选择尚未接通，因此不冒充新资源角色工作流已验证 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 24项、主工程构建运行、旧模板原图/Profile切换及单项/批量目标字符均无问题；新角色分支待选择入口接通后回归 |
| SET-003、TPL-011..014 | 已基线 | 已基线 | 四个参数入口统一分流旧INI与新配方事务写入；历史配方选择尚未接通，因此不冒充新配方无旧INI路径已验证 | Agent静态检查通过；2026-08-13用户确认`recipe_store_test` 24项、主工程及旧模板单项/批量目标字符、单项/批量阈值和重新选择后的保存值均无问题；新配方分支待选择入口接通后回归 |
| TPL-009、DET-006、RUN-001 | 原状态 | 原状态 | 二维码启动预检统一使用Profile资源角色并回退旧目录；历史配方选择尚未接通，不冒充新路径已验证 | Agent静态检查通过；2026-08-13用户确认二维码+三期旧模板可正常启动和停止；算法、DLL、运行快照和硬件链未改 |
| SET-003、TPL-009..014、DET-003、DET-006、RUN-001 | 原状态 | 迁移中 | 在原Profile编辑区新增按当前模式单选已发布配方入口，并把不可变选择、完整Profile候选及编辑会话一次原子安装；旧入口保留 | Agent静态检查通过，等待用户Qt Creator Run qmake/Rebuild/Run与实际配方选择、参数重新发布及启动停止门禁 |
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
5. 基线存图采用逐结果`QtConcurrent::run`，无容量限制；Stage 2现按用户确认改为容量32、双写入线程、满时等待且不因容量漏图的有界队列。
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
| Stage 1已发布配方可编辑会话切片边界 | 白名单、声明/定义、recipes依赖、身份/资产门禁、失败不改写、测试和工程清单检查 | 只新增纯recipes会话及两项测试；不改Widget、旧入口、设置、检测或硬件；主/测试工程均纳入新文件 | 7个工作区文件；新会话声明/定义各6项完整；主/测试工程各2个清单项；Widget引用0处；UI/检测/设备/OpenCV禁止依赖0处；输出和会话刷新各只有成功路径1处；21项业务测试声明/定义一致，预期QtTest汇总23项；90个ID唯一且状态74/7/7/2；`git diff --check`通过 | 通过 |
| Stage 1新建配方目标字符重新发布接入边界 | Widget白名单、首次发布绑定、原确认字符入口、会话重置、同UUID日志、工程清单与状态检查 | 只修改Widget及追踪文档；不修改recipes实现、qmake清单、测试、历史选择、多Profile批量、阈值或运行链 | 4个白名单文件；编辑会话成员、发布后绑定、辅助函数声明/定义/调用和重新发布日志各1处；草稿/编辑会话重置各6处且与Profile替换路径对齐；工程、测试、recipes、detection、devices和runtime改动0处；90个正式功能ID唯一且状态78/3/7/2；`git diff --check`通过 | 通过 |
| Stage 1新建配方目标字符重新发布测试 | 用户在Qt Creator Release运行`recipe_store_test` | 编辑会话及原Store回归共23项通过 | 2026-08-13用户反馈“没问题”，确认`23 passed, 0 failed`门禁通过 | 通过（用户证据） |
| Stage 1新建配方目标字符重新发布主程序门禁 | 用户Rebuild/Run，新建双字符单Profile配方后把目标改为已有资产子集并确认，再选择旧模板 | 首次`published`与随后`republished`使用同一UUID；无重新发布失败；旧模板选择正常 | 2026-08-13用户反馈“没问题”，确认实际同UUID重新发布及旧模板回归通过 | 通过（用户证据） |
| Stage 1新建配方图像阈值重新发布接入边界 | Widget白名单、原成功顺序、编辑会话调用、失败提示、工程/测试/批量/运行链隔离、状态与Git检查 | 只修改`on_pushButton_3_clicked`单Profile分支及追踪文档；不改头文件、UI、qmake、测试、recipes实现、批量阈值、检测、设备或runtime | 3个白名单文件；Widget全部3个零上下文hunk均属于该槽；旧INI保存→缓存→`ssim`→dirty→配方发布→提示的顺序索引严格递增；编辑会话入口调用由1增至2，阈值失败提示1处，统一重新发布日志仍1处；90个功能ID唯一且状态79/2/7/2；`git diff --check`通过 | 通过 |
| Stage 1新建配方图像阈值重新发布测试 | 用户在Qt Creator Release运行`recipe_store_test` | 编辑会话及原Store回归共23项通过 | 2026-08-13用户反馈“没问题”，确认`23 passed, 0 failed`门禁通过 | 通过（用户证据） |
| Stage 1新建配方图像阈值重新发布主程序门禁 | 用户Rebuild/Run，新建单字符配方后把阈值改为65并设置，再选择旧模板 | 原阈值成功提示保持；首次`published`与随后`republished`使用同一UUID；目标字符不变且旧模板选择正常 | 2026-08-13用户反馈“没问题”，确认实际同UUID重新发布、目标字符保持及旧模板回归通过 | 通过（用户证据） |
| Stage 1编辑会话整批Profile原子更新边界 | recipes层依赖、接口声明/定义、整批候选赋值点、测试声明/定义、Widget隔离、状态与Git检查 | 只修改纯recipes会话、`recipe_store_test`和追踪文档；整批身份或Schema失败时会话不改写；不接Widget和旧批量入口 | 5个白名单文件；接口声明/定义各1处；整批候选只在全量检查和Schema验证后1处赋值；recipes禁止依赖0处；Widget/主工程清单改动0处；业务测试声明/定义各22项，预期Qt Test汇总24项；90个功能ID唯一且状态79/2/7/2；`git diff --check`通过 | 通过 |
| Stage 1编辑会话整批Profile原子更新测试 | 用户在Qt Creator Release运行`recipe_store_test` | 原Store、单项编辑和新增整批原子更新共24项通过 | 2026-08-13用户反馈“没问题”，确认`24 passed, 0 failed`门禁通过 | 通过（用户证据） |
| Stage 1批量目标字符重新发布接入边界 | Widget白名单、辅助函数声明/定义、单/批入口复用、旧失败返回顺序、统一日志、工程/测试/批量阈值隔离及状态检查 | 只修改Widget及追踪文档；原全部失败/部分成功在发布前返回；批量全部成功时一次更新和发布 | 4个白名单文件；复数辅助函数声明/定义各1处，Widget整批会话调用1处，统一重新发布日志1处；批量顺序为全部失败→部分失败→dirty→发布→原提示；UI/qmake、tests、recipes、detection、devices、runtime及批量阈值改动0处；24项配方测试合同未改；90个功能ID唯一且状态79/2/7/2；`git diff --check`通过 | 通过 |
| Stage 1批量目标字符重新发布测试 | 用户在Qt Creator Release运行`recipe_store_test` | 编辑会话及原Store回归共24项通过 | 2026-08-13用户反馈“没问题”，确认`24 passed, 0 failed`门禁通过 | 通过（用户证据） |
| Stage 1批量目标字符重新发布主程序门禁 | 用户Rebuild/Run，新建双字符单Profile配方后从批量按钮改为已有资产子集，再选择旧模板 | 原批量成功提示保持；只新增一条同UUID`republished`日志；阈值不变且旧模板选择正常 | 2026-08-13用户反馈“没问题”，确认实际单次同UUID重新发布及回归通过 | 通过（用户证据） |
| Stage 1批量图像阈值重新发布接入边界 | Widget白名单、旧`ssim`/dirty/失败返回顺序、统一批量发布辅助函数、日志、工程/测试/其他模块隔离及状态检查 | 只修改批量阈值槽和追踪文档；原全部失败/部分成功在发布前返回；批量全部成功时一次更新和发布 | 3个白名单文件；Widget唯一代码hunk位于批量阈值槽；顺序为`ssim`→dirty→全部失败→部分失败→发布→原提示；复数辅助函数调用共3处，统一重新发布日志1处，阈值批量失败提示1处；头文件、UI/qmake、tests、recipes、detection、devices和runtime改动0处；24项配方测试合同未改；90个功能ID唯一且状态79/2/7/2；`git diff --check`通过 | 通过 |
| Stage 1批量图像阈值重新发布测试 | 用户在Qt Creator Release运行`recipe_store_test` | 编辑会话及原Store回归共24项通过 | 2026-08-13用户反馈“没问题”，确认`24 passed, 0 failed`门禁通过 | 通过（用户证据） |
| Stage 1批量图像阈值重新发布主程序门禁 | 用户Rebuild/Run，新建单字符单Profile配方后从批量按钮把阈值改为66，再选择旧模板 | 原批量成功提示保持；只新增一条同UUID`republished`日志；目标字符不变、阈值显示66且旧模板选择正常 | 2026-08-13用户反馈“没问题”，确认实际单次同UUID重新发布及回归通过 | 通过（用户证据） |
| Agent项目执行纪律 | Agent不执行构建、测试或运行 | 不产生Agent运行证据 | 未执行，符合强制规则 | 通过 |

## 用户Qt Creator门禁

### A. 当前PLC设备适配：运行测试

1. Qt Creator打开`tests/tests.pro`，因新增子工程先执行Run qmake。
2. 构建并运行`plc_device_adapter_test`，预期Qt Test汇总`9 passed, 0 failed`。该目标使用假Snap7函数，不连接真实PLC。

### B. 当前PLC设备适配：主程序

1. Qt Creator打开`app/AutoOCRproject.pro`，因新增源文件先Run qmake，再用Qt 5.14.2、MSVC 2017 64-bit、Release执行Rebuild并Run。
2. 有PLC时：确认启动1秒自动连接、手动断开/重连、连续/间歇模式设置、PLC工艺参数设置和拍照距离单项设置的成功/失败提示与之前一致。
3. 没有PLC时：确认自动连接失败后主窗仍可用；点“打开相机”时PLC失败仍不阻止继续打开相机；勾选PLC触发后未连接时仍拒绝启动检测。
4. 若有可观察PLC，分别产生一次OK和一次NG，确认DB1.DBB1033仍为OK写0，NG写49后约100ms写0；延迟剔除位置不变。没有可观察PLC时，只反馈上述可执行项，不冒充硬件时序证据。

### C. 本切片不要求重复运行的目标

`product_recipe_test`、`recipe_store_test`、所有`detection_tests`及`barcode_decoder_adapter_test`的源码和工程均未修改，不作为本切片必选门禁。

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
- [x] 字符裁切完成后发布切片已创建独立提交`38263e7`。
- [x] 开始新建配方草稿会话抽取切片时工作区干净，HEAD为`38263e7`。
- [x] 新建配方草稿会话抽取切片Agent静态检查通过；10个白名单文件、新会话声明/定义完整、主/测试工程清单4项、Widget直接publisher调用0处/会话发布调用1处、旧三份草稿状态引用0处、`recipe_store_test` 19项业务测试、recipes层禁止依赖0处、90个主功能ID唯一、状态79/2/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 草稿会话`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`21 passed, 0 failed`无问题。
- [x] 主程序Run qmake/Rebuild/Run及重复实际发布门禁：2026-08-13用户确认无问题。
- [x] 新建配方草稿会话抽取切片已创建独立提交`c81e3b8`。
- [x] 开始已发布模板配方可编辑会话切片时工作区干净，HEAD为`c81e3b8`。
- [x] 审计确认新配方缓存解析已具备，但旧目标字符、阈值、字符裁切和二维码启动预检仍依赖旧模板根目录；本切片不提前切换选择按钮或运行链。
- [x] 已发布模板配方可编辑会话切片Agent静态检查；7个白名单文件、会话声明/定义各6项、主/测试工程清单4项、Widget引用0处、recipes禁止依赖0处、21项业务测试、90个功能ID唯一、状态74/7/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 可编辑会话`recipe_store_test`门禁：2026-08-13用户确认预期Qt Test汇总`23 passed, 0 failed`无问题。
- [x] 可编辑会话主程序Run qmake/Rebuild/Run及旧模板选择、Profile显示和编辑入口回归门禁：2026-08-13用户确认无问题。
- [x] 已发布模板配方可编辑会话切片已创建独立提交`e875619`。
- [x] 开始新建单Profile目标字符同UUID重新发布切片时工作区干净，HEAD为`e875619`。
- [x] 审计限定为当前进程中刚新建、完成字符裁切并成功发布的单Profile配方；历史模板、多Profile批量、阈值、模式记忆和运行链不接入。
- [x] 新建配方目标字符重新发布切片Agent静态检查通过；4个白名单文件、编辑会话成员/发布后绑定/辅助函数声明定义调用/日志各1处、草稿与编辑会话重置各6处、工程及测试等越界改动0处、90个功能ID唯一、状态78/3/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 新建配方目标字符重新发布`recipe_store_test`回归门禁：2026-08-13用户确认`23 passed, 0 failed`无问题。
- [x] 主程序Rebuild/Run及实际同UUID重新发布门禁：2026-08-13用户确认首次`published`与随后`republished`使用相同UUID、无重新发布失败且旧模板选择正常。
- [x] 新建单Profile目标字符同UUID重新发布切片已创建独立提交`f267755`。
- [x] 开始新建单Profile图像阈值同UUID重新发布切片时工作区干净，HEAD为`f267755`。
- [x] 阈值入口审计限定为`on_pushButton_3_clicked`的单Profile成功路径；批量阈值、历史选择和运行链不接入。
- [x] 新建配方图像阈值重新发布切片Agent静态检查通过；3个白名单文件、Widget变更全部位于单Profile阈值槽、旧成功顺序保持、编辑发布调用2处、阈值失败提示1处、受保护工程/测试/批量/运行文件0改动、90个功能ID唯一、状态79/2/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 新建配方图像阈值重新发布`recipe_store_test`回归门禁：2026-08-13用户确认`23 passed, 0 failed`无问题。
- [x] 主程序Rebuild/Run及阈值同UUID重新发布门禁：2026-08-13用户确认原提示、重新发布日志、UUID、目标字符保持及旧模板回归均无问题。
- [x] 新建单Profile图像阈值同UUID重新发布切片已创建独立提交`62cbc00`。
- [x] 开始编辑会话整批Profile原子更新切片时工作区干净，HEAD为`62cbc00`。
- [x] 当前切片只扩展纯recipes编辑会话及配方测试，不修改Widget、UI/qmake、旧批量入口、检测、设备或runtime。
- [x] 编辑会话整批Profile原子更新切片Agent静态检查通过；5个白名单文件、接口声明/定义各1处、整批候选原子赋值1处、recipes禁止依赖0处、Widget/主工程清单改动0处、22项业务测试声明/定义一致、预期Qt Test汇总24项、90个功能ID唯一、状态79/2/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 编辑会话整批Profile原子更新`recipe_store_test`门禁：2026-08-13用户确认`24 passed, 0 failed`无问题。
- [x] 编辑会话整批Profile原子更新切片已创建独立提交`250c3bd`。
- [x] 开始批量目标字符同UUID重新发布接入切片时工作区干净，HEAD为`250c3bd`。
- [x] 当前接入只覆盖原“批量确认字符”的全部成功点；原全部失败/部分成功提示、批量阈值、历史配方选择和运行链不接入。
- [x] 批量目标字符重新发布切片Agent静态检查通过；4个白名单文件、复数辅助函数声明/定义各1处、Widget整批会话调用1处、统一重新发布日志1处，原全部失败→部分失败→dirty→发布→原提示顺序保持，受保护工程/测试/recipes/检测/设备/runtime/批量阈值改动0处，24项配方测试合同未改，90个功能ID唯一、状态79/2/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 批量目标字符重新发布`recipe_store_test`回归门禁：2026-08-13用户确认`24 passed, 0 failed`无问题。
- [x] 主程序Rebuild/Run及批量目标字符同UUID重新发布门禁：2026-08-13用户确认原提示、单条重新发布日志、UUID、阈值保持及旧模板回归均无问题。
- [x] 批量目标字符同UUID重新发布切片已创建独立提交`089b398`。
- [x] 开始批量图像阈值同UUID重新发布接入切片时工作区干净，HEAD为`089b398`。
- [x] 当前接入只覆盖原“批量设置阈值”的全部成功点；原全部失败/部分成功提示、`ssim`、dirty、历史配方选择和运行链保持。
- [x] 批量图像阈值重新发布切片Agent静态检查通过；3个白名单文件、Widget唯一代码hunk位于批量阈值槽，原`ssim`→dirty→全部失败→部分失败→发布→原提示顺序保持，复数辅助函数调用共3处、统一重新发布日志1处、批量阈值失败提示1处，受保护头文件/UI/qmake/tests/recipes/检测/设备/runtime改动0处，24项配方测试合同未改，90个功能ID唯一、状态79/2/7/2及`git diff --check`均通过；未执行构建、链接、测试或主程序。
- [x] 批量图像阈值重新发布`recipe_store_test`回归门禁：2026-08-13用户确认`24 passed, 0 failed`无问题。
- [x] 主程序Rebuild/Run及批量图像阈值同UUID重新发布门禁：2026-08-13用户确认原提示、单条重新发布日志、UUID、目标字符保持、阈值66及旧模板回归均无问题。
- [x] 批量图像阈值同UUID重新发布切片已创建独立提交`8de6ed6`。
- [x] 字库Profile只读资源访问桥已通过用户Qt Creator门禁并创建独立提交`60d6087`。
- [x] 字库Profile参数持久化分流已通过用户Qt Creator门禁并创建独立提交`c9394f2`。
- [x] 二维码启动预检资源访问桥已通过用户Qt Creator门禁并创建独立提交`6e1f498`。
- [x] 开始已发布配方单选入口切片时工作区干净，HEAD为`6e1f498`。
- [x] 当前切片新增独立`app/ui/dialogs/RecipeSelectionDialog`，Widget只负责运行状态门禁、调用recipes选择/装配/编辑会话边界并原子安装结果；旧“选择模板”入口保持。
- [x] 新入口安全边界：失败不改当前缓存；新配方不写旧INI；模式切换暂不覆盖旧路径记忆；已发布配方字符资产增删明确留待后续切片，避免误写配方根目录。
- [x] 已发布配方单选入口切片Agent静态检查通过；7个白名单文件、新对话框源/头工程清单各1处、选择槽声明/定义/连接各1处、UTF-8 BOM兼容MSVC2017、原子安装点总数保持3处、90个功能ID唯一、状态74/10/4/2及`git diff --check`均通过；等待用户Qt Creator构建和主程序回归。Agent未执行构建、链接、测试或主程序。
- [x] 首次Qt Creator构建门禁失败：2026-08-13用户截图报告`widget.cpp:6395`的MSVC C2065，新增按钮访问了已退出作用域的`commonPushButtonStyle`；同图7处`std::make_pair`均为本切片前已存在且参数/队列类型一致的代码模型诊断，本切片未修改这些调用。
- [x] 构建修复仅把原按钮样式常量提升到`setupWordTemplateEditorCombo()`函数作用域，使原按钮和新增按钮共享同一常量；调用、UI行为及其余检测代码不变。静态检查通过，等待用户重新Rebuild。
- [x] 第二次Qt Creator门禁已能构建运行并显示新增按钮，但2026-08-13用户截图确认按钮“已发布配方”发生UTF-8按GBK解释的乱码，UI可观察行为门禁未通过；此前`make_pair`代码模型诊断未阻塞实际构建。
- [x] 编码修复只把本切片新增的按钮、提示和独立选择对话框中文从`QStringLiteral`改为项目现有控件已经验证的`QString::fromUtf8`路径；不修改全局编译编码、旧界面文字、配方数据或行为。等待用户Rebuild后复验按钮及对话框中文。
- [x] 第三次主程序门禁截图显示旧模式记忆把此前误存的`recipes/<UUID>`根目录当成旧Profile目录恢复，因新配方按设计没有`app_settings.appset`而弹出“模板无法加载”；“模/板”之间是QMessageBox在长UUID/路径下的自动字符折行，不是消息源码中的回车。
- [x] 残留路径修复只在旧字库模板恢复入口识别RecipeStore直属目录：从旧路径记忆中静默移除并保存清理结果，不读取旧INI、不删除配方目录，也不提前实现UUID配方自动恢复；真实旧模板路径和错误提示保持。
- [x] 第四次主程序门禁截图显示独立配方选择对话框标题、说明、Profile数量和按钮全部乱码；证明该MSVC2017构建把新文件窄字符串生成为本地代码页字节，上一轮`QString::fromUtf8`仍不适用于此工程配置。
- [x] 最终编码修复把本切片全部新增中文改为ASCII源码中的Unicode转义并由`QStringLiteral`生成UTF-16；不再依赖源文件编码、窄字符串执行代码页或全局编译选项，配方名称等运行数据保持原值。等待用户Rebuild后复验完整对话框。
- [x] 第五次主程序门禁已确认选择对话框和参数提示中文正常，但已发布配方`750eca4e-d071-4399-8db6-79608d77a6df`在阈值同UUID重新发布时报告旧目录备份改名失败；只读检查确认目标为普通可写目录、无符号链接、无残留`.tmp/.bak`，原正式配方未被替换。
- [x] 第一次修复曾把失败归因于短暂占用并增加盲目重试；2026-08-13用户复验仍稳定失败，证明该判断错误，本轮已撤销重试及对应临时测试，不把推测保留为正式实现。
- [x] 持续失败诊断在主程序仍运行时完成：`ShengYin.exe`工作目录位于构建目录，未直接持有目标配方文件句柄；Windows对UUID目录及全部子目录/资源的`DELETE`访问检查全部成功，排除当前进程工作目录和目录ACL权限，但当时尚未完成外部进程句柄定位。
- [x] 诊断版曾改用Windows `MoveFileExW`取得原生错误码；它没有解决问题，只把失败确认为错误5。用户指出程序应提示关闭占用窗口，而不应继续修改事务实现；该意见正确，原生API改动随后全部撤销。
- [x] 第六次主程序门禁返回原生错误码5；定向系统句柄扫描随后找到直接占用者：`explorer.exe` PID 9292持有目标配方`assets/profiles/0`的两个目录句柄，对应资源管理器窗口HWND 2166054正停在该目录。另一个资源管理器窗口停在不同配方`b136.../assets`，与本次失败无关。由此修正上一轮“QDir调用本身失败”的判断：Windows不允许在子目录被资源管理器浏览时重命名整个UUID目录，`QDir::rename`与`MoveFileExW`均会失败；当前门禁先关闭或导航离开该窗口后用原按钮重试，不再继续替换事务API。
- [x] 最终修复恢复RecipeStore原有`QDir::rename`备份/提交/回滚路径，仅在旧目录备份失败信息中增加“关闭正在浏览该配方目录或子目录的文件资源管理器窗口及其他占用程序，然后重试”的中文操作提示；测试在既有提交失败场景内增加备份失败提示与原配方保留断言。Agent静态检查确认原生移动/盲目重试引用均为0、提示及断言各1处、22项业务测试（QtTest预期汇总24项）不变且`git diff --check`通过。
- [x] 操作员警告可见性调整：同UUID重新发布失败时，已保存参数的部分成功说明保持普通文字；“配方重新发布失败”、原始错误和“关闭资源管理器后重试”整段改为加粗红色并使用警告图标。单个/批量目标字符及单个/批量图像阈值4个入口统一调用同一UI呈现函数；不改RecipeStore事务、缓存、运行参数或检测主链。Agent静态检查确认函数定义及警告样式各1处、4个失败入口均接入，且`git diff --check`通过；等待用户Qt Creator主程序复验。
- [x] 2026-08-13用户确认已发布配方入口主程序门禁通过：二维码+三期模式可选择已发布`566`配方，`[RECIPE_SELECT]`出现，Profile/目标字符/阈值显示正确；修改阈值后`[RECIPE_PUBLISH] republished`出现且UUID保持，无重新发布或同步失败；启动/停止识别及旧“选择模板”入口均正常。用户同时确认目录占用的加粗红色警告显示符合预期。
- [x] 2026-08-13用户确认`recipe_store_test`重新构建运行无问题，预期汇总`24 passed, 0 failed`门禁通过；目录占用提示断言和原RecipeStore回归获得用户证据。
- [x] 已发布配方单选入口及目录占用红色操作提示已创建独立本地提交`19dc8ac`。
- [x] 开始按识别模式记住上次已发布配方切片时工作区干净，HEAD为`19dc8ac`。
- [x] 当前切片只为字库匹配和二维码+三期分别保存一个已发布配方UUID，切换模式和重启时优先原子恢复；旧模板路径继续作为回退，原检测算法、硬件、运行收尾和旧选择入口保持。
- [x] 当前切片自动恢复失败时清除失效UUID并回退旧路径；旧模板手动选择会清除当前模式的UUID记忆，避免下次启动又覆盖用户刚选的旧模板。
- [x] `product_recipe_test`新增隔离测试数据目录下的按模式UUID设置往返测试，同时验证未知模式被丢弃、UUID去空白及旧路径仍共存。
- [x] 按模式已发布配方记忆切片Agent静态检查通过：8个白名单文件，设置键/Widget共享激活函数/测试声明定义各唯一1处，8项业务测试对应Qt Test预期汇总10项，90个功能ID唯一，状态77/4/7/2，检测/设备/runtime改动0处且`git diff --check`通过；未执行构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 按模式已发布配方记忆Qt Creator门禁：2026-08-13用户确认`product_recipe_test`预期10项、主工程Run qmake/Rebuild/Run、`566`模式切换与重启`[RECIPE_RESTORE]`、Profile/目标字符/阈值及手动旧模板保持均无问题。
- [x] 按模式已发布配方记忆切片已创建独立提交`feb3fe5`。
- [x] 开始已发布配方字符模板资产编辑切片时工作区干净，HEAD为`feb3fe5`；影响`SET-003、TPL-011、TPL-012、TPL-015`。
- [x] 当前切片复用原字符裁切对话框，但已发布配方只在`TemplateCharacterAssetWorkspace`临时工作区生成文件；recipes编辑会话负责原子替换当前Profile的资产命名空间并用同UUID事务发布，其他Profile及旧目录入口保持。
- [x] 已发布配方字符资产编辑切片Agent静态检查通过：13个白名单文件（含2个新增recipes源文件），workspace/assembler替换/session替换/Widget接入/测试声明定义均各唯一1处，主工程及测试工程清单各包含新源和头1次；23项业务测试对应Qt Test预期汇总25项，90个功能ID唯一，状态77/4/7/2，检测/设备/runtime改动0处且`git diff --check`通过。未执行构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 已发布配方字符资产编辑Qt Creator门禁：2026-08-13用户确认`recipe_store_test`预期25项、主工程Run qmake/Rebuild/Run、`566`字符裁切、原UUID重新发布、重选/重启持久化及旧目录裁切入口均无问题。
- [x] 已发布配方字符资产编辑切片已创建独立提交`9aad739`。
- [x] 开始多Profile配方制作/发布闭环切片时工作区干净，HEAD为`9aad739`；影响`TPL-006、TPL-009、TPL-010、TPL-012、TPL-014、TPL-016`。
- [x] 当前切片把旧“选择模板”加载的有序多目录Profile组一次发布为一个多Profile产品配方，成功后复用已验证的选择/装配/编辑会话入口；失败保持原旧模板组和设置记忆。
- [x] 多Profile配方制作/发布闭环Agent静态检查通过：5个白名单文件；发布命令声明/定义/连接及专项测试声明/定义各1处；24项业务测试对应Qt Test预期汇总26项；90个功能ID唯一，状态75/6/7/2；`.pro/.pri`、检测、设备和runtime改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 多Profile配方制作/发布闭环Qt Creator门禁：2026-08-13用户确认`recipe_store_test` 26项、主工程构建运行、Profile顺序/参数、模板组发布、批量同UUID、模式/重启恢复、启停及旧选择入口全部无问题。
- [x] 多Profile配方制作/发布闭环切片已创建独立提交`1e7fe47`。
- [x] 开始字库家族统一只读运行Profile快照切片时工作区干净，HEAD为`1e7fe47`；影响`DET-003、DET-006、RUN-001、TPL-009、TPL-010`。
- [x] 当前切片使普通字库和二维码+三期在启动时均深拷贝完整Profile资源与参数，检测期间只读运行副本，停止或线程结束后统一释放；不改Profile评分、检测判定或硬件时序。
- [x] 字库家族统一只读运行Profile快照Agent静态检查通过：4个白名单文件；快照创建/清理声明定义各1处，旧二维码专用快照符号0处，统一运行快照字段检测读取1处、成功安装2处、释放5处；90个功能ID唯一，状态79/5/4/2；测试、`.pro/.pri`、detection、devices、runtime和采集线程源文件改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator主工程门禁。
- [x] 字库家族统一只读运行Profile快照Qt Creator门禁：2026-08-13用户确认字库匹配、二维码+三期和旧多目录入口的启动/停止/再次启动均无问题，统一快照日志、Profile及参数保持正常。
- [x] 字库家族统一只读运行Profile快照切片已创建独立提交`47c7c40`。
- [x] 开始钢印/深度OCR单模板产品配方底座切片时工作区干净，HEAD为`47c7c40`；影响`SET-003、TPL-006、TPL-007、TPL-008`。
- [x] 当前切片建立四种模板模式的统一配方模式/必需资产角色合同；钢印在定位模板和标定YAML之外必须包含`stampRing`，深度OCR沿用定位模板和标定YAML。资产清单、草稿、发布、同UUID重发、选择装配和编辑会话共享该合同；纸巾仍被模板会话拒绝。
- [x] 钢印/深度OCR单模板产品配方底座Agent静态检查通过：11个白名单文件；`isTemplateRecipeMode`及必需资产角色声明/定义各1处，`stampRing`资产清单生成1处，发布组装和选择装配均使用统一合同，草稿不再残留字库家族限制；`recipe_store_test` 25个声明/25个定义对应预期27项，`product_recipe_test` 8个声明/8个定义对应预期10项；90个功能ID唯一，状态77/4/7/2；Widget、检测、设备、runtime、采集线程和`.pro/.pri`改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator配方测试门禁。
- [x] 钢印/深度OCR单模板产品配方底座首次Qt Creator门禁：2026-08-13用户反馈`product_recipe_test`为`10 passed, 0 failed`；`recipe_store_test`新增业务合同通过，但既有失败诊断断言因提示固定短语由`required runtime assets`变为单数而出现`26 passed, 1 failed`。已恢复原固定短语并保留新增的具体缺失角色，等待仅复跑`recipe_store_test`。
- [x] 钢印/深度OCR单模板产品配方底座最终Qt Creator门禁：2026-08-13用户确认修复后无问题，即`product_recipe_test`为`10 passed, 0 failed`、`recipe_store_test`为`27 passed, 0 failed`。
- [x] 钢印/深度OCR单模板产品配方底座已创建独立提交`ccb6b5a`。
- [x] 开始钢印/深度OCR单模板Widget闭环切片时工作区干净，HEAD为`ccb6b5a`；影响`SET-001、SET-003、TPL-006..008、TPL-011、TPL-013、TPL-016、DET-002、DET-004、RUN-001`。
- [x] 当前切片一次接通两种单模板模式的旧目录发布、已发布配方选择、同UUID目标/阈值重发和按模式恢复；旧“选择模板”入口、检测算法、相机、PLC和启动时序保持。
- [x] 钢印已发布配方选择在内存候选中先解码定位图、读取日期/钢印区域、初始化钢印环引擎并按原“仅精确文件名”规则加载字符图；深度OCR先解码定位图和日期区域。任一步失败不安装半成品。
- [x] 两种单模板配方的目标字符和图像阈值均先在编辑会话副本中校验并同UUID事务重发，成功后才更新字符缓存或`ssim`运行阈值；失败沿用加粗红色操作警告。模式切换/重启优先恢复UUID，失效时清理记忆并回退旧路径。
- [x] 钢印/深度OCR单模板Widget闭环Agent静态检查通过：4个白名单文件；6个新增/泛化命令声明定义各唯一1处；选择严格先验证后安装，参数严格先事务后运行缓存；旧`loadSettingsFromDir`选择和旧重叠引擎初始化入口仍在；90个功能ID唯一，状态73/11/4/2；tests、`.pro/.pri`、detection、devices、runtime和采集线程源文件改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator大门禁。
- [x] 钢印/深度OCR单模板Widget闭环首次主程序门禁未通过：2026-08-13用户发现钢印编辑区被错误简化为普通单模板布局，并缺少原钢印“字符匹配+重叠检测”所需的“分割字符模板”入口；因此前述静态检查不作为切片完成证据，切片保持未提交和`迁移中`。
- [x] 纠偏切片扩展影响`UI-001、TPL-015`：钢印单Profile编辑区恢复与字库家族一致的“当前编辑模板”选择显示，旧目录新建/编辑和已发布配方均复用原字符裁切对话框；已发布配方在临时工作区保留`stampRing`并按钢印原精确字符名规则校验后同UUID事务重发。检测Pipeline、重叠算法、相机、PLC和运行时序不改。
- [x] 钢印字符模板制作/编辑纠偏Agent静态检查通过：5个白名单文件；新钢印裁切命令声明/分发/定义各1处，已发布配方临时工作区显式保留`stampRing`，旧目录与已发布配方均按原`includeVariants=false`规则校验；钢印编辑区、单Profile下拉与字符裁切显隐条件各唯一1处；90个正式功能ID状态71/13/4/2；tests、`.pro/.pri`、detection、devices、runtime和采集线程源文件改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator纠偏门禁；通过前不提交。
- [x] 钢印/深度OCR单模板Widget闭环主体Qt Creator门禁：2026-08-13用户确认主工程Run qmake/Rebuild/Run、`recipe_store_test` 27项、`product_recipe_test` 10项、两模式旧模板、首次发布、重新选择、同UUID参数重发、分别启停、模式恢复隔离及旧入口覆盖均无问题。主体相关功能恢复为已基线，DET-002/DET-004恢复已验证；钢印字符裁切专项未包含在本次反馈中，`UI-001、TPL-015`继续保持迁移中且当前切片仍不提交。
- [x] 钢印字符裁切专项Qt Creator门禁：2026-08-13用户澄清前述“全部正常”已包含钢印旧模板和已发布配方的“分割字符模板”实际保存、同UUID重发及启停；`UI-001、TPL-015`恢复已基线，当前切片门禁完整通过。
- [x] 钢印/深度OCR单模板Widget闭环已创建独立提交`17987fc`，提交后工作区干净。
- [x] 开始Stage 1统一模板配方工作流事务切片；影响`SET-003、TPL-006、TPL-011..015`，七项由已基线进入迁移中；既有配方选择装配链`TPL-008`不在本切片修改范围并保持已基线。
- [x] 旧调用链事实：Widget同时直接持有字库草稿会话、字库编辑会话和单模板编辑会话，并在首次发布、单项/批量参数重发和字符资产替换时自行复制候选、调用RecipeStore并决定何时覆盖会话；UI与配方事务状态所有权仍然混合。
- [x] 新增recipes层`TemplateRecipeWorkflow`，把新建草稿发布后建立编辑会话、单项/批量Profile重发、字符资产替换重发统一为“候选副本校验→RecipeStore发布→成功后提交会话”；Widget继续持有当前会话，仅提交控件候选、显示原提示并安装运行资源。
- [x] 配方测试把参数与字符资产两条真实事务路径接入工作流，覆盖首次发布失败、参数校验失败、参数落盘失败、字符资产校验失败和字符资产落盘失败均不污染当前会话、输出选择或正式配方；成功重发保持原UUID和其他Profile资产。
- [x] 统一工作流事务切片Agent静态检查通过：9个白名单文件（含2个新增recipes源文件）；4个工作流命令声明/定义各1处，Widget直接调用草稿/编辑会话更新与发布为0处，主工程和测试工程各登记新源/头1次；26项业务测试对应Qt Test预期汇总28项；90个功能ID唯一，状态75/7/6/2；detection、devices、runtime和采集线程改动0处，`git diff --check`通过。Agent未执行构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 统一工作流事务Qt Creator集中门禁：2026-08-13用户确认配方测试、主工程及可执行的字库/二维码/钢印发布与重发路径均无问题；深度OCR当前没有可用旧模板，未单独执行“先发布再改目标/阈值”，该项不记为实际通过证据且不阻塞当前内部事务收口。
- [x] 统一模板配方工作流事务已创建独立提交`89934ee`，提交后工作区干净。
- [x] Stage 1结构关口审计：五种检测均已有独立Pipeline和离线测试；产品配方Schema、事务存储、发布/选择/编辑会话及四模板模式Widget入口已经接通；字库多Profile、钢印字符模板和运行快照均经过主程序门禁。固定图像、PLC、硬触发、存图和性能证据继续按用户风险接受结论延期，因此只声明结构关口通过，不声明现场验收完成。
- [x] 开始Stage 2二维码解码设备适配切片；影响`TPL-004、DET-006、RUN-001、TOOL-002`，四项进入迁移中。旧调用链由Widget直接持有`HMODULE`和两个C ABI函数指针，并同时承担DLL定位/版本日志、单次ABI结果映射、7路OpenCV预处理、60ms预算、首选策略缓存与错误文本。
- [x] 新增`IBarcodeDecoder`与`BarcodeDecoderAdapter`，把DLL生命周期、ABI调用和7路通用解码策略整体迁出Widget；Widget通过接口所有权只提交灰度ROI、选项和Profile首选策略，并继续负责模板坐标换算、Profile缓存、检测结果显示和原失败提示。
- [x] 新增`runtime_tests/barcode_decoder_adapter_test`，通过可注入假C ABI覆盖接口就绪、成功文本/格式/角点映射、坏ROI在ABI调用前拒绝、ABI参数错误终止、关闭fallback只做快速尝试、缓存首选策略优先六项业务行为；Qt Test预期汇总`8 passed, 0 failed`。测试归入主计划限定的第三个`runtime_tests`子工程，不额外建立第四类测试工程。
- [x] 二维码设备适配切片Agent静态检查通过：12个白名单文件（含5个新增文件）；Widget中DLL装载、C ABI函数指针和旧解码函数引用为0，设备适配器保留7个策略分支，主工程源/头与tests运行子工程均唯一登记；Widget物理删除旧实现629行，当前文件约11457行；90个功能ID唯一，状态79/4/5/2；recipes、detection、相机、PLC及采集线程改动0处，`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 二维码设备适配首次Qt Creator构建未通过：MSVC2017按代码页936解析新适配器的无BOM UTF-8中文窄字符串，在原80行报`C4819/C2001`并连锁报`C2143`；源码括号结构正常，不是解码算法错误。
- [x] 编译错误修复：两条既有中文DLL错误提示改用仅含ASCII源码字符的`QString::fromWCharArray(L"\\u....")`构造，显示文字和占位符保持不变，避免依赖MSVC源文件代码页；等待用户重新构建。
- [x] 二维码设备适配Qt Creator集中门禁：2026-08-13用户确认`barcode_decoder_adapter_test`预期8项、`barcode_word_detection_pipeline_test`预期6项、主工程Run qmake/Rebuild/Run、旧或已发布配方、实际框选即时读码、DLL版本日志、二维码与日期启停以及跨模式模板/参数恢复全部无问题；`TPL-004、RUN-001、TOOL-002`恢复已基线，`DET-006`恢复已验证。
- [x] 二维码解码设备接口与适配器已创建独立提交`ef3af6d`，提交后工作区干净。
- [x] 开始Stage 2 Paddle OCR设备适配切片；影响`SYS-006、DET-004`，两项进入迁移中。旧调用链由Widget直接包含Paddle配置/模型头、构造并持有四个具体对象，检测回调内固定先`DBDetector::Run`再`CRNNRecognizer::Run`，分类器按`use_angle_cls`可空；ROI、清洗、目标比较和全部结果副作用在Pipeline/Widget其余代码中。
- [x] 新增`IOcrEngine/PaddleOcrEngine`，适配器保持配置路径解析、所有构造参数、可选分类器、原生调用顺序与输出行顺序；Paddle具体头和对象已从Widget清零，适配器以明确所有权在窗口销毁时释放模型。
- [x] `OcrDetectionPipeline`改为直接依赖`IOcrEngine`，Widget只提交裁切图与目标文字；现有四项业务测试改用内存`FakeOcrEngine`，继续锁定字节清洗、行顺序、非空精确匹配和空/不同文字NG，Qt Test预期仍为`6 passed, 0 failed`。
- [x] Paddle OCR设备适配切片初次Agent静态检查通过：13个白名单文件（含3个新增`devices/ocr`文件）；Widget具体Paddle类型/构造/调用引用0处，适配器检测与识别调用各唯一1处、可选分类器门禁与传入各唯一1处，Pipeline接口声明/调用和Widget注入均唯一；主工程新增源1/头2，OCR测试Fake与接口头已登记；recipes、barcode、相机、PLC、runtime及采集线程改动0处。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] Paddle OCR Pipeline Qt Creator测试门禁：2026-08-13用户确认`ocr_detection_pipeline_test`汇总`6 passed, 0 failed`；该测试使用Fake设备，不编译具体Paddle适配器，因此不能替代主工程构建门禁。
- [x] Paddle OCR适配器首次主工程构建未通过：`paddle_ocr_engine.cpp`直接包含无include guard的`ocr_cls.h`，随后`ocr_rec.h`再次包含同一头文件，MSVC报`C2011 PaddleOCR::Classifier类型重定义`并连锁报`C2280/C2027`。
- [x] 编译错误修复：删除适配器对`ocr_cls.h`的重复直接包含，继续由`ocr_rec.h`唯一引入完整`Classifier`定义；不修改Paddle第三方源码、模型对象、构造参数、可选分类器或检测/识别调用顺序，等待用户重新构建主工程。
- [x] Paddle OCR适配器主工程构建启动复验：2026-08-13用户提供运行中深度模型界面，证明重复包含修复后主工程已能构建并启动；直接启动时既有产品模板预检正确拒绝缺少tracking/date资源，但实际制作新模板时发现冻结画面没有启用绘图。
- [x] 深度OCR模板入口根因：`freezeTemplatePreview`只对硬编码模式索引0和字库家族开启`ImageLabel`模板绘图，`showTemplateGuideForCurrentMode/handleTemplateGuideEvent`也只接受0/1/4，遗漏深度OCR索引2，导致界面提示完成框选但鼠标事件按非模板路径处理。
- [x] 深度OCR模板入口修复：统一按`isSingleTemplateRecipeMode(modeId) || isWordFamilyMode(modeId)`决定绘图资格，深度OCR恢复“定位矩形→喷码多边形→右键闭合→保存”的既有通用流程，并显示“深度模型模板制作”逐步引导；纸巾仍不启用模板绘图，Paddle、ROI换算、模板落盘和检测判定未改。影响`TPL-002、TPL-003、TPL-005、DET-004`，等待主程序实际制作、保存、启停和退出门禁。
- [x] Paddle OCR与深度OCR模板入口Qt Creator集中门禁：2026-08-13用户确认`ocr_detection_pipeline_test`汇总`6 passed, 0 failed`；主工程修复重复头文件后正常构建启动；深度OCR模板实际完成定位/日期框选并产生`[RECIPE_SELECT]`与`[RECIPE_PUBLISH]`，随后检测成功启动、Paddle逐帧调用并正常停止。当前实际样本`Final String`为空，只记录为本次样本结果，不冒充准确率或固定样本对照证据；用户确认其余日志均无功能问题。
- [x] Paddle OCR设备适配与深度OCR模板入口修复已创建独立提交`c78ebb8`，提交后工作区干净。
- [x] 开始Stage 2 Snap7 PLC设备适配切片；影响`SYS-008、SET-005、CAM-001、RUN-001、PLC-001..006`十项，由原状态进入迁移中。`PLC-007`延迟剔除队列本身不修改，继续调用同一NG输出入口。
- [x] 旧调用链审计：生产态`TS7Client`调用全部位于Widget，包含启动1秒延迟连接、打开相机附带连接、手动连接/断开、界面使能查询、启动预检、四项工艺参数、拍照距离单项下发、OK写0和NG写49→100ms→0；检测Pipeline、采集线程和延迟剔除队列没有直接Snap7调用。
- [x] 新增`IPlcDevice`、结构化`PlcOperationResult`与`Snap7PlcDevice`；仅适配器包含`snap7.h`、`TS7Client`、`S7AreaDB`和`S7WL*`，Widget只保留设备接口所有权并沿用原返回码0/非0分支、原提示和原调用顺序。
- [x] 新增`runtime_tests/plc_device_adapter_test`，用可注入假Snap7函数覆盖连接端点与返回码、断开、连接状态、DB区Byte/Word/DWord映射和缺失后端安全失败七项业务行为；Qt Test预期汇总`9 passed, 0 failed`，不连接真实PLC。
- [x] PLC设备适配Agent静态检查通过：Widget中`TS7Client/S7AreaDB/S7WL*/client->`引用0处；适配器独占原生类型和常量；主工程新源/头清单各唯一；运行测试子目标已登记；活跃DB写入调用8处，连接3处，断开3处；工艺参数仍按980 Word→920 DWord→982 Word→924 DWord顺序，0、49和100ms字面量各保留唯一1处；90个功能ID状态为68/10/10/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] PLC设备适配Qt Creator集中门禁：2026-08-13用户确认`plc_device_adapter_test`预期`9 passed, 0 failed`及主工程Run qmake/Rebuild/Run和可执行PLC失败/启动保护入口均无问题；本次未提供真实PLC读回或49→100ms→0现场记录，因此相关功能恢复`已基线`而不冒充现场验证。
- [x] Snap7 PLC设备接口与适配器已创建独立提交`9542aaa`，提交后工作区干净。
- [x] 开始Stage 2单相机设备适配切片；影响`SYS-009、SET-005..007、TPL-001..002、CAM-001..005、RUN-001..003、SAVE-004`十五项，由原状态进入迁移中。`CAM-006`图像旋转/通道、所有检测Pipeline、PLC时序和多相机源码均不修改。
- [x] 旧调用链审计：Widget直接持有`MV_CC_DEVICE_INFO_LIST/CMvCamera*`并负责枚举、打开、关闭、曝光/增益、触发切换、回调注册和停止后重开；`MyThread`直接执行软件触发、读取帧序号/图像和模板预览；`CameraThread`直接管理非阻塞帧、硬触发帧就绪与停止唤醒；OCR原图保存还会直接另取相机帧。
- [x] 已新增`ICameraDevice/HikvisionCameraDevice`和可注入Fake后端；适配器的原生文件独占`MV_CC_DEVICE_INFO_LIST/CMvCamera/MVCC_FLOATVALUE`，继续委托原`CMvCamera`的枚举、首台打开、回调、条件变量、帧序号、取图和停止唤醒实现。Widget、`MyThread`和`CameraThread`只持有共享相机接口，迟退线程不再面对已删除的裸相机指针；线程停止策略和等待上限不变。
- [x] 相机设备适配Agent静态检查通过：三个生产调用方中`CMvCamera/MV_CC_DEVICE_INFO_LIST/MVCC_FLOATVALUE/MV_OK/MVS头`引用0处；原生SDK引用只存在于`hikvision_camera_device_native.cpp`和保留的旧`cmvcamera.*`实现；主工程登记新源2/头2，运行测试子目标登记唯一；适配器接口20项均有实现；Fake测试覆盖8项业务行为，Qt Test预期汇总`10 passed, 0 failed`；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 相机Fake测试首次Qt Creator编译反馈：MSVC在`missingBackendFailsSafely`中把`HikvisionCameraDevice device(HikvisionCameraFunctions());`按C++最令人困惑的解析识别为函数声明，后续成员调用报`C2228`；已改为先声明空函数表再显式构造测试对象，不修改生产适配器或相机行为，等待用户复编。
- [x] 单相机设备适配Qt Creator集中门禁：2026-08-13用户确认`camera_device_adapter_test`及主工程均无问题；开关相机、曝光/增益、模板实时预览/冻结/退出、软触发启停重开、可用现场路径、OCR原图保存及退出均按本次集中清单回归通过。固定样本、硬触发时序和性能量化证据继续按既定延期项管理，不冒充最终验收。
- [x] 单相机设备接口与海康适配器已创建独立提交`e6dc0d2`，提交后工作区干净。
- [x] 开始Stage 2检测完成结果帧与有界存图切片；影响`DET-002..006、RUN-001、SAVE-001..005`十一项，由原状态进入迁移中。检测算法、判定边界、统计增量、PLC写入/延迟剔除、UI结果文案和相机触发时序不修改。
- [x] 旧结果/存图调用链审计：五种模式均在Widget完成最终UI、统计、PLC和存图；钢印/字库/二维码/纸巾保存当前检测参数`cv::Mat`，唯独深度OCR的`saveImage2Async`在结果后从相机`readBuffer/latestImage`另取一帧；三处`QtConcurrent::run`按文件无界提交，标注图和原图会形成两个独立后台任务。
- [x] 在现有`TrackingTypes.h`增加`ProductKey、FrameData、AlgorithmVerdict、DetectionStatus、DetectionResult、DetectionCompletion`，每次正式启动生成运行UUID，每个正式结果递增序号并深拷贝本次检测原帧；五模式所有OK/NG正式结果路径均建立短生命周期完成对象，Widget不再为OCR存图读取相机。
- [x] `runtime/image_save_service.*`初版采用单工作线程、写入中与待写合计容量8个产品任务，同一产品的标注/原图属于一个任务；当时队列满拒绝当前第9个新任务并累计告警，随后因主程序门禁发现实际漏存而按用户决定被下述满时等待策略替代。
- [x] `runtime_tests/detection_completion_test`初版覆盖产品键、独立只读帧、结果/Overlay合同、短期共享、任务校验/FIFO、容量8拒新、失败累计和停止拒收共11项业务行为；随后容量测试按用户确认改为满时等待且不丢任务，Qt Test预期汇总仍为`13 passed, 0 failed`。测试部署脚本仅为该目标新增可选QtGui运行库参数，不改变既有测试目标。
- [x] DetectionCompletion与有界存图Agent静态检查通过：90个功能ID唯一且状态为72/11/5/2；Widget内`QtConcurrent/QFuture`、相机`readBuffer/latestImage`和旧分文件保存函数引用均为0；五模式7条正式结果路径均建立完成对象；存图服务容量判断唯一、两个产品任务提交入口、主工程源/头和运行测试目标均唯一登记；11项业务测试对应Qt Test预期13项；PowerShell部署脚本语法0错误；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 有界存图首次主程序门禁反馈：运行状态实际报告队列满后累计跳过24个新存图任务、写入失败0个，说明检测产出暂时快于PNG落盘而非磁盘写入失败；新中文警告受MSVC2017源代码页影响显示乱码。已将Widget警告及存图服务错误详情统一改为Unicode宽字符转义，并明确提示“检测和判定仍在继续”；容量8、FIFO、拒绝最新任务及检测/判定/PLC行为均未修改，等待重新构建显示复验。
- [x] 用户确认存图属于必要生产结果，工人需要查看每个被检测且命中保存策略的图片；正常条件下不允许因队列满漏图，并接受存图积压时降低检测速度。策略据此收敛为容量32、两个后台写入线程、满时提交者持有当前图片等待空位；移除容量满丢弃状态、计数和UI警告，只保留真实目录/权限/磁盘写入失败的红色累计报警。不引入临时文件、断电恢复或持久任务数据库，极少数实际磁盘故障允许失败并明确报警。
- [x] 存图测试合同同步改为Fake慢盘填满小容量队列后验证下一任务确实等待、腾位后全部任务写入；单工作线程测试继续锁定产品任务及任务内文件顺序，生产实例明确使用两个工作线程。业务测试数量仍为11项，Qt Test预期汇总仍为`13 passed, 0 failed`。
- [x] 满时等待策略Agent静态复验通过：90个功能ID唯一且状态72/11/5/2；生产实例容量32、工作线程2均唯一配置；服务中`QueueFull/taskDropped/droppedTaskCount`引用0处，空间条件等待和任务完成唤醒各唯一1处；Widget中`QtConcurrent/QFuture`及相机取帧API调用均为0；主工程源/头和运行测试目标均唯一登记；11项业务测试对应Qt Test预期13项；PowerShell部署脚本语法0错误；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] DetectionCompletion与无丢弃存图Qt Creator集中门禁：2026-08-13用户确认更新后的`detection_completion_test`及主工程均无问题；容量满策略已从实际漏存的拒新方案改为等待空位，中文失败提示正常，当前检测与应存图片未再出现队列容量导致的缺失。
- [x] 开始Stage 2运行会话身份切片；影响`RUN-001、DET-002..006`六项，由已验证进入迁移中。只把Widget中的运行UUID、产品递增序号、只读帧与DetectionResult组装迁入`runtime/DetectionSession`；当前Overlay采集、五模式算法、UI结果、统计、存图、PLC、延迟剔除和线程启停顺序均保持。
- [x] 新增`runtime/detection_session.*`：每次正式启动生成UUID并清零产品序号，完成一件产品时原子形成同运行ID递增`ProductKey`、独立只读帧和`DetectionResult`副本；保留未显式启动时自动建立会话的旧防御行为。Widget删除运行ID和序号字段，只保留当前模式/Overlay采集薄桥并委托会话完成组装。
- [x] `detection_completion_test`新增两项业务测试，覆盖新运行重置序号、同运行连续递增、未启动自动建会话、指定帧号/相机号/时间戳、原图独立所有权和结果/Overlay副本；业务测试由11项增至13项，Qt Test预期汇总由13项增至`15 passed, 0 failed`。
- [x] DetectionSession切片Agent静态检查通过：90个功能ID唯一且状态72/6/10/2；Widget中旧`m_detectionRunId/m_detectionProductSequence/QUuid`引用0处，六条完成对象调用继续经唯一Widget薄桥进入唯一`DetectionSession::complete`；主工程和测试工程的新源/头各唯一登记；13项业务测试对应Qt Test预期15项；本切片未修改配方、Pipeline、设备、PLC、存图服务或采集线程；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] DetectionSession Qt Creator集中门禁：2026-08-13用户确认`detection_completion_test`预期`15 passed, 0 failed`及主工程启停、再次启动回归均无问题；每次运行会话身份、判定、计数、存图和PLC保持正常，`RUN-001、DET-002..006`恢复已验证。
- [x] DetectionSession切片已提交为`45e7340`（`refactor(runtime): 提取检测运行会话`），提交后工作区干净。
- [x] 开始Stage 2统一结果处理切片；影响`DET-002..008、PLC-005..007、RES-001..003、SAVE-001`十四项，由原状态进入迁移中。五种算法判定、识别文本/Overlay、存图路径与格式、PLC地址/值/100ms复位Timer及线程触发顺序均不修改。
- [x] 新增`runtime/result_handler.*`：统一持有总数、NG数、合格率公式、四种存图选择矩阵和延迟NG目标计数队列；输入只接受`DetectionCompletion`，返回存图类别与OK/NG PLC请求。Widget删除`totalImages/ngImages/currentImagesSnapshot/removalQueue`，五模式及两条定位失败收尾统一委托运行层，再由薄桥调用原存图函数、`rightremove/wrongremove`和UI刷新。
- [x] 保留现有非对称清零语义：总数清零同时清总数和NG但不立即重算合格率；NG清零只清NG且不立即重算合格率；剔除队列复位仍只清未发出的延迟NG请求。保存模式0/1/2/3继续分别是不保存/仅NG/仅OK/全部；算法或系统失败到达正式收尾时继续按NG处理。
- [x] `detection_completion_test`新增六项ResultHandler业务测试，覆盖无效完成对象无副作用、OK/立即NG计数与PLC请求、延迟NG目标边界、四种存图矩阵及统计/队列分开复位；业务测试由13项增至19项，Qt Test预期汇总由15项增至`21 passed, 0 failed`。
- [x] ResultHandler切片Agent静态检查通过：90个功能ID唯一且状态64/14/10/2；Widget旧`totalImages/ngImages/currentImagesSnapshot/removalQueue`引用0处；六条完成对象生产入口形成十个OK/NG记录分支，六个正式收尾入口统一检查延迟NG；直接`rightremove/wrongremove`调用仅剩运行层请求薄桥3处；主工程和运行测试的新源/头各唯一登记；19项业务测试对应Qt Test预期21项；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] ResultHandler首次Qt Creator编译门禁未通过：MSVC2017在`class ResultHandler`声明起点产生首个C2447，后续99项均为类型未形成导致的级联错误。为规避Qt/Windows SDK或旧头文件环境中的通用标识符与头文件保护宏冲突，实际类型改为项目专用`DetectionResultHandler`，保护宏改为`OCRGANGYIN_RUNTIME_RESULT_HANDLER_H`；统计、保存、PLC请求和延迟队列逻辑未修改，等待重新构建。
- [x] ResultHandler第二次Qt Creator编译门禁仍在同一类声明起点产生C2447，证明仅改类名不足，首次“类名冲突”不能作为根因结论。现将新头文件收敛为全ASCII和`#pragma once`，并把所有公开数据类型、方法名及枚举项改为`Detection*`专用标识符，移除通用`None/Ok/Ng`和旧保护宏以排除MSVC2017实际包含环境中的编码/预处理干扰；旧标识符残留0、头文件非ASCII字节0，业务行为未改，等待重新构建。
- [x] ResultHandler运行测试编译及执行门禁：2026-08-13用户确认MSVC2017 Release汇总`21 passed, 0 failed`、退出码0；全ASCII专用类型修复有效，ResultHandler离线行为门禁通过。
- [x] 主程序集中门禁发现模板匹配ROI越界会在每个连续检测帧调用模态`QMessageBox::warning`，关闭一个后下一帧立即再弹，导致停止按钮无法正常操作；这是既有逐帧错误交互在连续采集下暴露，不是ResultHandler计数错误。现改为每次越界阶段仅写一次`[DETECTION_ROI]`并在`statusLabel`显示加粗红色操作提示，不再创建模态窗口；有效ROI时恢复运行状态，启动、停止和关相机时清理锁存。影响`UI-002、UI-005、RUN-002`，等待主程序复验。
- [x] 用户进一步明确ROI业务规则：外扩20像素只是期望边距，靠近原图边缘不足20像素时必须以相机原图边界为准继续检测，不应视为错误。新增头文件`detection/common/detection_roi_geometry.h`，先把旋转后的多边形点限制到`0..width-1/height-1`，再外扩并与原图求交；模板匹配、字库及二维码日期ROI共用该规则。此前单次红色越界提示只保留给真正无有效面积的异常，不再用于正常碰边场景。
- [x] `detection_completion_test`新增两项ROI几何业务测试，覆盖左上边缘外扩不足20像素时裁到0，以及多边形整体越过右边界时吸附到最邻近图像边缘并形成有效ROI；业务测试由19项增至21项，Qt Test预期汇总由`21 passed, 0 failed`增至`23 passed, 0 failed`。
- [x] ROI边界策略Agent静态检查通过：90个功能ID唯一且状态61/17/10/2；公共几何头在主工程/运行测试各唯一登记，Widget旧局部外扩函数0处、公共边界函数4处调用；ROI越界模态窗口0处；21项业务测试对应Qt Test预期23项；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] ResultHandler与ROI边界Qt Creator集中门禁：2026-08-14用户确认`detection_completion_test`预期`23 passed, 0 failed`、主工程靠边ROI按原图边界裁剪、连续检测无模态弹窗且可正常停止；模板匹配和二维码模式OK/NG、总数/NG/合格率、四种存图选择、PLC请求、延迟位置2、剔除复位以及两个统计清零入口均无问题。本切片17个迁移中功能转为已验证。
- [x] ResultHandler与ROI边界切片已创建独立提交`6050116`，开始Stage 2运行协调器大切片时工作区干净。
- [x] 新增全ASCII `runtime/inspection_runtime_controller.*`，统一持有`DetectionSession`和`DetectionResultHandler`，建立`Idle→Starting→Running→Stopping→Idle`及显式Fault确认状态；Widget删除两个直接成员，只保留运行按钮、线程和UI桥接。
- [x] 五模式六个正式结果入口统一先调用协调器`record`，只有被当前运行UUID和递增产品序号接受的完成对象才继续按返回动作存图和请求PLC；同运行重复/倒序完成对象及其他运行完成对象不再重复计数、存图或产生逻辑PLC请求。图像路径、格式、有界队列、PLC写值、100ms Timer及延迟剔除公式未修改。
- [x] 运行协调器新增6项业务测试，覆盖启停状态、重复启动、Fault确认、会话+结果所有权、重复/外来完成拒绝、新运行序号复位但统计保留、统计与延迟队列独立清理；`detection_completion_test`业务测试由21项增至27项，Qt Test预期汇总由`23 passed, 0 failed`增至`29 passed, 0 failed`。
- [x] 运行协调器Agent静态检查通过：90个正式功能ID唯一且状态60/15/13/2；新头文件非ASCII字节0；主/测试工程各唯一登记controller源码和头文件；测试声明/定义各27项；Widget直接持有旧Session/ResultHandler为0处、六个正式结果入口全部经协调器、预存图策略旁路0处；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 运行协调器Qt Creator集中门禁：2026-08-14用户确认`detection_completion_test`预期`29 passed, 0 failed`、主工程Run qmake/Rebuild/Run、模板匹配软触发OK/NG及停止/再次启动、二维码+三期硬触发启停/再次启动、判定/统计/存图和`[RUNTIME_CONTROLLER] starting`日志均无问题；15个迁移中功能转为已验证。
- [x] 运行协调器切片已提交为`328aca5`（`refactor(runtime): 统一检测运行协调器`），提交后工作区干净。
- [x] 开始Stage 2启动预检与参数门禁切片；影响`RUN-001、SET-004`，由原状态进入迁移中。相机/PLC实际写入、软硬触发分支、线程创建与运行Profile快照顺序均不修改。
- [x] 新增全ASCII `runtime/inspection_start_preflight.*`：访问门禁按原顺序统一处理模板制作、运行忙碌、相机未开、未应用参数确认和PLC未连接；资源门禁统一处理纸巾免模板、单模板三项资源、字库Profile完整性及二维码解码器/Profile资源聚合。Widget继续探测实际文件和设备、展示原中文提示，并在用户放弃未应用修改后按恢复出的PLC触发状态重新求值。
- [x] `detection_completion_test`新增8项启动预检业务测试，覆盖空闲接受、访问拒绝优先级、脏参数先于PLC、纸巾免模板、单模板缺失项顺序、字库空/未完成Profile、二维码聚合错误及两类有效Profile；业务测试由27项增至35项，Qt Test预期汇总由`29 passed, 0 failed`增至`37 passed, 0 failed`。
- [x] 启动预检切片Agent静态检查通过：新头/源和新增测试均为全ASCII源码，避免MSVC2017代码页936再次触发中文窄字符串错误；主工程与运行测试工程各唯一登记新模块；测试声明/定义各35项；Widget旧`barcodeStartErrors/productTemplateErrors`局部决策为0处，访问与资源门禁均唯一委托预检；实际相机/PLC应用、软硬触发线程分支、算法Pipeline、存图和结果协调器文件均未修改；90个受治理功能状态59/2/27/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 启动预检与参数门禁Qt Creator集中门禁：2026-08-14用户确认`detection_completion_test`预期`37 passed, 0 failed`、主工程Run qmake/Rebuild/Run、未开相机和模板制作保护、未应用参数取消/继续、模板匹配软触发及二维码硬触发启停均无问题；`RUN-001、SET-004`转为已验证。
- [x] 启动预检与参数门禁切片已提交为`5026314`（`refactor(runtime): 统一启动预检与参数门禁`），提交后工作区干净。
- [x] 开始Stage 2采集运行配置与线程装配切片；影响`RUN-001、CAM-003、CAM-004、CAM-006、SET-008、SET-009、SET-010`七项，由原状态进入迁移中。`MyThread/CameraThread`主循环、相机TriggerSource/LineDebouncer/TriggerDelay、Profile深拷贝、算法分发、PLC和结果链均不修改。
- [x] 新增全ASCII `runtime/inspection_run_configuration.*`：一次生成Software/Hardware采集类型、WholeFrame/SingleTemplate/WordProfiles跟踪类型和二维码硬触发标志；把原0..100图像阈值、大于0纸巾阈值及旋转/通道0..3映射从Widget抽成纯配置解析，非法值仍由Widget显示原中文提示。
- [x] 新增`runtime/inspection_worker_configurator.*`：软硬采集线程共享完全相同的整图、单模板和多Profile装配分支；硬线程额外接收原二维码模式标志。资源内容、调用先后和线程具体实现不变，Widget删除两套重复装配判断。
- [x] `detection_completion_test`新增7项运行配置测试，覆盖软触发单模板、硬触发二维码、纸巾整图、普通字库Profile、合法运行参数、越界UI索引旧默认回退以及两种阈值失败；业务测试由35项增至42项，Qt Test预期汇总由`37 passed, 0 failed`增至`44 passed, 0 failed`。
- [x] 采集运行配置与线程装配Agent静态检查通过：四个新文件及新增测试均为全ASCII源码；主工程对配置/装配源头各唯一登记，运行测试只登记纯配置源头；测试声明/定义各42项；Widget对`createPlan/parseSettings/configureSoftwareWorker/configureHardwareWorker`调用分别唯一，直接`setBypassTracking/setBarcodeWordHardTriggerMode/setPresetBoxes`调用均为0；原`MyThread/CameraThread`、相机/PLC设备、检测Pipeline、结果和存图文件改动0处；90个受治理功能状态54/7/27/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 采集运行配置与线程装配Qt Creator集中门禁：2026-08-14用户确认`detection_completion_test`预期`44 passed, 0 failed`、主工程Run qmake/Rebuild/Run、模板匹配软触发、纸巾整图、二维码硬触发、代表旋转/通道以及判定/统计/存图/PLC均无问题；七个迁移中功能转为已验证。
- [x] 采集运行配置与线程装配切片已提交为`02aac8d`（`refactor(runtime): 统一采集运行配置与线程装配`），提交后工作区干净。
- [x] Stage 2结构收口审计通过：二维码、OCR、PLC和单相机均已有窄接口及Fake/假ABI或适配器测试；Widget只持有`IBarcodeDecoder/IOcrEngine/IPlcDevice/ICameraDevice`接口，检测和新runtime代码无厂商SDK类型；`DetectionCompletion`只短期交接本次原帧，存图服务保持容量32、2个写线程、队满等待不丢任务；正式结果经唯一运行协调器收尾，PLC 0/49、约100ms复位及延迟剔除未改。固定实际样本、真实PLC精确读回/脉冲量化和P50/P95仍按用户决定延期，因此只声明Stage 2结构门禁完成，不声明最终产品验收完成。
- [x] 正式进入Stage 3首个“影子结果对照基础”切片；对照范围关联`DET-001、RES-001、RES-004`，但当前只新增无副作用比较器，不接正式入口，不改变这些功能ID的既有状态，也不产生第二次计数、存图或PLC请求。
- [x] 新增全ASCII `runtime/detection_shadow_comparator.*`：逐项比较模式、判定、完成状态、识别文本及有序Overlay角色/点集/分数；坐标和分数容差显式配置，耗时始终排除在业务一致性外，诊断文本可按需启用。比较器只返回差异字段，不持有图像、不访问设备、不发布结果。
- [x] `detection_completion_test`新增6项影子对照测试，覆盖完全一致、忽略耗时/默认忽略诊断、业务结果差异、几何容差、有序Overlay差异和可选诊断比较；业务测试由42项增至48项，Qt Test预期汇总由`44 passed, 0 failed`增至`50 passed, 0 failed`。
- [x] 影子结果对照基础Agent静态检查通过：两个新源码非ASCII字节0；主工程和运行测试工程对新头/源各唯一登记；新增测试声明/定义各6项，比较入口定义唯一；未修改Widget、两采集线程、检测Pipeline、设备、结果协调器、存图或PLC文件，正式结果出口和外部副作用均为0处变化；功能状态保持54/0/34/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 影子结果对照基础Qt Creator门禁：2026-08-14用户确认`detection_completion_test`预期`50 passed, 0 failed`及主工程Run qmake/Rebuild/Run均无问题；比较器仍未接正式结果入口，不产生生产副作用。
- [x] 影子结果对照基础切片已提交为`e333c24`（`test(runtime): 建立检测影子结果对照基础`），提交后工作区干净。
- [x] 开始Stage 3有界帧队列与串行检测工作线程基础切片；关联`RUN-002、RUN-003、RUN-004、CAM-003、CAM-004`，但本切片不接正式采集入口，不改变功能状态。旧软/硬线程仍分别在单个QThread循环执行取图、旋转/通道、定位并以Qt排队信号交给Widget；停止仍走原协调器、相机唤醒、限时等待和重开相机流程。
- [x] 新增全ASCII `runtime/frame_queue.*`：容量必须显式给定且最小为1，严格验证`FrameData/ProductKey/原图`，保持FIFO；满队列时提交者等待空位，取消时一次释放全部待处理帧并唤醒生产者/消费者，支持完成协作停止后重新打开。
- [x] 新增全ASCII `runtime/detection_worker.*`：单一`std::thread`串行消费`FrameQueue`，检测成功只构造一次`DetectionCompletion`并调用一个完成回调；停止时拒绝新帧、清空未开始帧、等待当前检测返回并抑制停止后的迟到完成回调；不使用`terminate()`，支持`requestStop/wait`后重新启动。异常只进入独立失败回调，不生成产品判定、统计、存图或PLC请求。
- [x] `detection_completion_test`新增8项并发基础测试，覆盖无效帧与最小容量、FIFO、满队列等待不丢帧、取消释放帧和阻塞提交者、严格串行与结果顺序、运行外拒收、取消抑制未完成结果以及停止等待后的重启；业务测试由48项增至56项，Qt Test预期汇总由`50 passed, 0 failed`增至`58 passed, 0 failed`。
- [x] 有界帧队列与串行检测工作线程基础Agent静态检查通过：四个新源码非ASCII字节0；主工程和运行测试工程对四个新头/源各唯一登记；运行测试声明/定义各56项，其中新增并发测试各8项；新模块`terminate()`引用0，Widget、软硬采集线程、检测Pipeline、设备及正式结果链差异0；功能状态保持54/0/34/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 有界帧队列与串行检测工作线程基础Qt Creator门禁：2026-08-14用户确认`detection_completion_test`预期`58 passed, 0 failed`及主工程Run qmake/Rebuild/Run均无问题；正式软硬采集链仍未切换。
- [x] 有界帧队列与串行检测工作线程基础已提交为`dde929f`（`refactor(runtime): 建立有界帧队列与串行检测工作线程`），提交后工作区干净。
- [x] 开始Stage 3正式接入前的产品帧受理契约切片；关联`RUN-001..004、DET-002..006`，但本切片不接Widget和正式采集入口，不改变功能状态。审计确认旧链在算法结束时才同时生成`ProductKey/FrameData/DetectionCompletion`，而新`FrameQueue`要求入队前已有合法产品身份，因此先固定受理与完成边界，避免接入时产品号重复、检测帧与存图帧错配。
- [x] `DetectionSession`新增检测前`acceptFrame`：空图不占序号，合法帧按当前运行UUID递增分配产品号并立即深拷贝为只读`FrameData`；受理数量和完成数量分开统计。新增同帧`complete(frame, result)`只接受当前运行中由本会话实际受理的同一只读对象，拒绝伪造同号帧、外来运行帧、重复及倒序完成；旧`complete(image, result)`保留并委托新契约，现有Widget调用行为不变。
- [x] `InspectionRuntimeController`新增受理薄桥：新`acceptFrame`仅在`Starting/Running`接收产品，`Stopping`拒绝新受理但允许停止前受理帧按序完成，`Idle/Fault`继续拒绝正式完成对象。旧`complete(image, result)`暂时保留原Stopping收尾兼容，待正式入口切换后再收口；现有五模式算法、两采集线程、队列、结果统计、存图和PLC路径均未修改。
- [x] `detection_completion_test`新增3项产品帧契约测试，覆盖检测前深拷贝和元数据、受理/完成计数分离、空图/伪造/外来/重复拒绝，以及停止后拒新但排空已受理帧；业务测试由56项增至59项，Qt Test预期汇总由`58 passed, 0 failed`增至`61 passed, 0 failed`。
- [x] 产品帧受理契约Agent静态检查通过：五个改动源码非ASCII字节0；运行测试声明/定义各59项；新`acceptFrame`在Widget及软硬采集线程引用0处，确认正式生产链尚未切换；主工程和测试工程无需新增源文件登记；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 产品帧受理契约Qt Creator门禁：2026-08-14用户确认`detection_completion_test`预期`61 passed, 0 failed`及主工程软硬触发代表路径均无问题；检测前唯一产品身份、同帧完成和旧结果入口兼容通过本次门禁，正式软硬采集链仍未切换。
- [x] 产品帧受理契约已提交为`64f7635`（`refactor(runtime): 固定检测产品帧受理契约`），提交后工作区干净。
- [x] 开始Stage 3纸巾软触发正式运行链接入；受影响功能ID为`DET-005、CAM-003、RUN-001..004`。源码追踪确认纸巾软触发原先在`MyThread`内直接运行Pipeline，而其余软触发模式仍把定位结果排队交给Widget，因此本切片只迁移可独立闭合的纸巾链，不混入其他算法和硬触发。
- [x] `MyThread`纸巾分支改为按原间隔发出整帧；使用DirectConnection在采集线程中先由`InspectionRuntimeController::acceptFrame`分配本次运行唯一`ProductKey`并深拷贝只读原帧，再提交容量1的`DetectionWorker`。队列满时阻塞采集线程而非UI，不丢正式输入，也不建立无界Qt事件积压；其他软触发定位信号仍保持QueuedConnection和原Widget分发。
- [x] 纸巾Pipeline新增通用`DetectionResult`映射；工作线程串行执行原`TissueRollDetector`，完成后携带同一`FrameData/ProductKey`回UI。UI继续复用原粗糙度文字、圆框、统计、存图选择和PLC请求收尾；硬触发仍由`CameraThread`原纸巾Pipeline及旧结果信号进入同一兼容收尾，不改算法、阈值或触发时序。
- [x] 运行协调器公共状态加互斥保护，以允许采集线程受理帧、UI线程启停和检测线程完成结果安全并发；停止和析构先关闭受理并取消队列以释放被反压的采集线程，再等待采集和检测工作线程协作退出，不使用`terminate()`，再次启动创建全新队列和Worker。
- [x] `detection_completion_test`新增1项正式受理→检测Worker→同帧完成→结果记录集成测试，业务测试由59项增至60项，Qt Test预期汇总由`61 passed, 0 failed`增至`62 passed, 0 failed`；纸巾基线测试新增通用结果映射测试，业务测试由4项增至5项，预期汇总由`6 passed, 0 failed`增至`7 passed, 0 failed`。
- [x] 纸巾软触发正式链接入Agent静态检查通过：`MyThread`内纸巾Pipeline、纸巾参数副本和旧结果信号引用均为0，新整帧信号声明/发出/连接完整；硬触发`CameraThread::signal_sendTissueResult`仍保留原发出与两处生命周期接线；运行测试声明/定义各60项，纸巾测试声明/定义各5项；主工程既有Worker/Pipeline源登记唯一，纸巾测试工程新增`TrackingTypes.h`登记；新增基础设施源码非ASCII字节0，功能矩阵状态53/6/29/2，`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 纸巾软触发正式链接入Qt Creator门禁：2026-08-14用户确认集中验证无问题；主程序日志显示容量1 Worker正常启动，连续处理66帧，停止时`processed=66、cancelled=0`并完成停止入口；OK/NG、粗糙度、存图及模板匹配软触发回归正常。停止后继续出现的存图日志来自容量32存图队列排空，不是检测Worker残留。
- [x] 纸巾软触发正式链接入已提交为`78dd660`（`refactor(runtime): 接入纸巾软触发检测工作线程`），提交后工作区干净。
- [x] 开始Stage 3定位检测任务合同与深度OCR软触发正式链接入；受影响功能ID为`DET-004、CAM-003、RUN-001..004`。本切片不修改深度OCR清洗/精确比较、Paddle调用、相机变换与定位、PLC值/脉冲、统计和存图规则；硬触发以及钢印、字库、二维码生产路径保持原实现。
- [x] `DetectionWorkItem`统一携带检测前已受理的只读`FrameData`和可选`DetectionPose`；`FrameQueue/DetectionWorker`新增任务重载并保留原纯帧API，纸巾既有调用无需修改。容量、FIFO、满时等待、取消、停止抑制和重启合同不变。
- [x] 原Widget私有日期ROI旋转/裁切实现迁到`detection/common/detection_roi_geometry.h`，继续使用同一旋转方向、`BORDER_REPLICATE`、多边形映射、原图边界裁剪和通道转换。钢印、字库及深度OCR的兼容槽统一调用该无状态边界，旧私有实现删除。
- [x] 深度OCR软触发启动时快照当前目标字符并创建容量1 Worker；`MyThread`定位完成后在采集线程受理同一原帧和Pose，队列满时反压而不丢正式输入。检测线程内完成ROI和`OcrDetectionPipeline/IOcrEngine`调用，再把同一`DetectionCompletion`排队交给UI；UI只执行原文本、OK/NG、统计、存图和PLC收尾。无效ROI仍沿用原“不产生正式判定”语义。
- [x] 统一软触发接线改为单一Direct入口按活动Worker模式分流：纸巾只接整帧，深度OCR只接定位任务；无活动Worker时显式排队回原Widget分发，避免同时Direct/Queued造成重复检测。停止先关闭受理和模式路由再取消队列；硬触发信号接线未修改。
- [x] `detection_completion_test`新增3项，覆盖任务保留Pose、Worker接收定位任务和靠边日期ROI裁切，业务测试由60项增至63项，Qt Test预期汇总由`62 passed, 0 failed`增至`65 passed, 0 failed`。`ocr_detection_pipeline_test`新增通用结果/Overlay映射和“定位任务→ROI→OCR→通用结果”测试，业务测试由4项增至6项，预期汇总由`6 passed, 0 failed`增至`8 passed, 0 failed`。
- [x] 深度OCR软触发正式链接入Agent静态检查通过：运行测试声明/定义各63项，OCR测试声明/定义各6项；旧Widget私有ROI实现0处、公共实现1处；统一软触发接线定义1处并在两次`MyThread`生命周期各接回1次；定位Pipeline声明/定义各1处；硬触发检测信号两处生命周期接线仍保留；基础设施8个源码非ASCII字节0；功能矩阵状态53/6/29/2，`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 深度OCR软触发正式链接入Qt Creator门禁：2026-08-14用户确认相关测试和主程序其余行为均无问题；真实模型可制作/选择模板、启动检测、停止和再次启动，纸巾及其他模式回归可运行。`DET-004、CAM-003、RUN-001..004`六项转为已验证。
- [x] 深度OCR软触发正式链接入已提交为`da8db2d`（`refactor(runtime): 接入深度OCR软触发检测工作线程`），提交后工作区干净；纸巾结果画面刷新慢于统计按用户决定登记延期，不混入该提交。
- [x] 开始Stage 3共享字符模板匹配与钢印/字库软触发正式链接入；影响`DET-002、DET-003、CAM-003、RUN-001..004`七项并进入迁移中。二维码软触发及全部硬触发路径不改，算法目标计数、阈值、匹配候选顺序、IoU 0.3排除、钢印重叠AND判定、统计、存图和PLC时序保持。
- [x] 新增`detection/common/CharacterTemplateMatcher`，把旧`TemplateMatch::prepareDigitTemplates/run3`的灰度与0.5缩放缓存、按目标索引最高分、阈值、已选框IoU排除、平均框尺寸和结果排序原样抽为无UI纯组件。旧`TemplateMatch`公开入口已转调新组件，旧函数体仅在本切片门禁前以不可编译分支保留，待门禁通过后再删除。
- [x] `StampDetectionPipeline`新增定位任务入口：Worker内完成边界裁剪、字符匹配和原钢印重叠引擎调用，输出通用判定、诊断及tracking/date/character/stamp Overlay；UI只负责安装显示、唯一结果记录、存图和PLC请求。无效日期ROI保持不形成产品NG并使用原一次性边界提示。
- [x] `WordDetectionPipeline`新增定位任务入口：按启动时只读Profile快照执行字符匹配，保留目标单元解析、识别数量判定、缺失字符诊断、定位失败正式NG和Profile名称显示；完成结果携带同一原帧及Overlay回UI。多Profile定位/最高分选择仍由原`MyThread/ProfilePoseSelector`执行。
- [x] 钢印和普通字库软触发均使用容量1 `DetectionWorker`，队列满时反压采集线程；停止关闭受理、取消队列并协作等待。二维码+三期尚未启用该Worker，继续走原排队分发，避免本切片修改解码首选策略状态和硬触发行为。
- [x] `stamp_detection_pipeline_test`新增2项定位任务合同测试，预期由`6 passed`增至`8 passed`；`word_detection_pipeline_test`新增2项，预期由`6 passed`增至`8 passed`。两者覆盖真实共享字符匹配、通用结果/Overlay、钢印ROI取消和字库定位失败NG。
- [x] 钢印/字库软触发正式链接入Agent静态检查通过：两个Pipeline测试声明/定义各6项、预期汇总各8项；共享Matcher主工程源/头清单各唯一1处、两个测试工程登记完整；钢印/字库Worker声明定义和启动分发各唯一1处；硬触发检测/纸巾信号两套生命周期接线仍各2处，二维码Worker启动0处；新增检测与测试基础文件非ASCII字节0；90个正式功能ID唯一，状态53/7/28/2，`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 钢印/字库软触发首次Qt Creator构建门禁失败并完成修正：用户报告`widget.h`四处新工作输出类型无法解析，继而产生82项级联错误；确认`.cpp`已包含完整Pipeline定义，仅在`widget.h`补充`StampDetectionWorkOutput`和`WordDetectionWorkOutput`前置声明，不改检测、线程、统计、存图或PLC行为。当前仍等待用户重新构建门禁。
- [x] 字库软触发首次运行门禁失败并完成修正：用户连续启停后报告界面停止重绘、统计停留而后台定位/检测日志继续；日志确认Worker仍正常处理19帧并协作停止。追踪发现软触发启动入口每次都会重复连接同一`MyThread::signal_boxesSelected`排队信号，多次启停后重复UI事件持续累积；连接现增加`Qt::UniqueConnection`约束，每个线程实例只保留一次定位事件入口。字符匹配、Profile选择、判定、统计口径、存图和PLC行为不变，等待用户关闭旧进程后重新构建运行门禁。
- [x] 字库界面仍可出现“日志/检测继续而图像和文字停留”后完成二次根因追踪：容量1只覆盖检测输入队列，Worker取出任务后队列即留出空位，完成对象仍可无界调用`QMetaObject::invokeMethod(...QueuedConnection)`进入主线程；`[WORD_DETECT]`日志位于完整UI收尾之后，故日志帧号超过画面统计可直接证明Qt重绘事件被完成任务淹没，而非算法停止。
- [x] 用户明确批准公共修复约束：UI图像、Overlay、识别文字、模板名、OK/NG、统计和耗时必须属于同一张产品图并整体刷新；软触发采集速度应受检测处理能力反压，检测越快则进图越快，上限为相机真实帧率；正常生产不可通过丢正式图片或结果来换取UI流畅。
- [x] 新增`runtime/result_presentation_mailbox.*`：容量固定为1，Worker必须等待上一个UI任务完整执行后才能交付下一个；每个任务闭包仍调用原模式收尾，因此同一`ProductKey`的图像、文字、统计、存图决策和PLC请求不拆分。停止先`cancel()`唤醒被反压的Worker，再走原协作停止，不引入强杀线程。
- [x] 软/硬采集线程的实时图和定位Pose现在在进入Qt事件队列前检查生产结果绑定状态；检测期不再排队无用预览帧或用新Pose移动上一张产品的Overlay。已迁移软触发模式的显示耗时改为算法耗时与从产品帧受理到UI呈现的端到端耗时两者较大值，不再因整数截断长期虚假显示`1ms`。
- [x] `MyThread`软触发每轮先记录帧序号，再触发并等待帧序号增加后取得新帧，不再由非阻塞读取立即重复使用旧帧。删除循环末额外固定`msleep(100)`；现有`cameraDelay`大于0时仍作为最小检测间隔，0现明确表示不增加软件间隔，此时有界检测队列和UI邮箱使采集节拍随实际处理能力自然反压。
- [x] `detection_completion_test`新增2项UI完成邮箱合同测试，覆盖整产品任务串行、第二提交满时等待、取消释放提交者与重开；业务测试由63项增至65项，Qt Test预期汇总由`65 passed, 0 failed`增至`67 passed, 0 failed`。
- [x] 公共UI完成邮箱/软触发新帧节拍Agent静态检查通过：邮箱源/头在主工程与运行测试工程各登记1次；测试声明/定义均65项，新邮箱测试声明/定义各2项；四种Worker完成回调均改经公共邮箱，原直接Queued完成投递0处；`MyThread` 固定100ms休眠0处、非正0回退300ms逻辑0处；新基础文件非ASCII字节0；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 用户批准同步修订主计划的检测线程方案；主运行链现明确增加容量1结果呈现邮箱和UI Presenter，区分“预览可覆盖”与“正式产品不可丢”，固定软触发新帧/0与非0间隔/全链反压/图文原子刷新/协作停止合同，并明确当前`MyThread`与`Widget`完成闭包仅为Stage 3迁移桥，不改变最终拆分主窗口的目标。
- [x] 公共UI完成邮箱/钢印与字库Worker集中Qt Creator门禁：2026-08-15用户确认`detection_completion_test` 67项、钢印和字库Pipeline各8项、主工程Run qmake/Rebuild/Run、字库连续检测图文/判定/统计/耗时同步、软触发间隔0、纸巾代表模式、全部保存、停止重启及无连续`[SOFTWARE_ACQUISITION] frame timeout`均无问题；当前切片门禁通过，可以提交并进入二维码+三期软触发迁移。
- [x] 公共UI完成邮箱、共享字符模板匹配及钢印/字库软触发正式链已提交为`0001281`（`refactor(runtime): 统一软触发检测与结果呈现链`），提交后工作区干净。
- [x] 开始Stage 3二维码+三期软触发正式运行链接入；影响`DET-006、DET-007、UI-004、CAM-003、RUN-001..004、TOOL-002`。软触发迁入现有容量1 Worker和UI完成邮箱；硬触发继续使用原Widget兼容链，读码ABI、7路策略、60ms预算、PLC值/脉冲、结果计数和存图策略不改。
- [x] `BarcodeWordDetectionPipeline`新增完整定位任务入口：同一`DetectionWorkItem`内完成组合定向ROI、`IBarcodeDecoder`调用、角点映射、连续失败策略状态、不可读短路和可读后的字符检测；输出一个包含二维码、日期、判定、Overlay和诊断的完成结果。组合ROI内已准备的日期图直接交给`WordDetectionPipeline`，避免同一产品再次旋转整张原图。
- [x] 二维码软触发启动时建立Worker专用的有序运行Profile和已准备字符模板副本，Worker内按最高分Pose索引选择Profile并持有本次运行解码首选策略；完成对象经公共UI邮箱一次更新同一产品的图像、二维码文本、日期状态、框、OK/NG、统计、耗时、存图和PLC请求。启动前发布模式标志，避免第一帧早于运行状态可见。
- [x] `barcode_word_detection_pipeline_test`新增4项定位任务测试，覆盖不可读短路、可读+日期OK、可读+日期NG和非法二维码多边形在调用解码器前拒绝；业务测试由4项增至8项，Qt Test预期汇总由`6 passed, 0 failed`增至`10 passed, 0 failed`。
- [x] 二维码软触发Worker Agent静态检查通过：二维码Pipeline测试声明/定义各8项，预期汇总10项；主工程相关Pipeline、公共ROI和字库Pipeline源/头各唯一登记1次，测试工程四个源文件各唯一登记1次；二维码Worker声明/定义/启动分发和模式4路由各唯一；Widget旧`runBarcodeWordDetection/finalizeBarcodeWordNg`、`CameraThread`、硬触发源、PLC写值入口在本切片diff中均为0处改动；相关检测/测试源码非ASCII字节0；90个正式功能状态为51/8/29/2，`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator门禁。
- [x] 二维码Pipeline首次Qt Creator门禁为`9 passed, 1 failed`：新增日期OK样本把字符放在日期ROI内的奇数像素偏移，原字符算法分别以0.5缩放整块ROI和单字符模板后产生人工采样相位差，阈值80下未命中。修正仅把测试字符移到相对日期ROI原点的偶数偏移，继续使用原阈值、原Matcher和原生产判定；生产源码、二维码短路及硬触发链均未因本失败调整，等待复验。
- [x] 二维码软触发主程序首次UI门禁发现顶部判定文字乱码：日志中的`final=OK/NG`、识别内容、统计、Overlay、存图及协作停止均证明判定结果已经产生，根因是新完成分支用`QString(const char *)`接收含中文转义的窄字符串，当前MSVC代码页字节被Qt按UTF-8解释。仅把“正确/错误”两处富文本改为`QStringLiteral`的UTF-16编译期字符串，不改判定、颜色、统计、存图或PLC行为；等待主工程复验。
- [x] 二维码软触发集中Qt Creator门禁通过：2026-08-15用户确认`barcode_word_detection_pipeline_test`为`10 passed, 0 failed`，主工程重新构建后实际读码、日期OK/NG、不可读短路、图像/Overlay/文字/判定/统计/耗时整体刷新、中文“正确/错误”显示、停止重启以及硬触发兼容回归均无问题；当前切片可以独立提交。
- [x] 二维码+三期软触发正式运行链已提交为`0128a14`（`refactor(runtime): 接入二维码软触发检测工作线程`），提交后工作区干净；五模式软触发均已完成容量1检测队列和UI完成邮箱门禁。
- [x] 开始Stage 3硬触发兼容桥切片；影响`CAM-004、RUN-001..004、UI-004、DET-002..007`。原`TriggerSource=0`、`LineDebouncerTime=5000`、回调注册/启动顺序、`CameraThread`取帧/旋转/通道/定位、二维码专用新帧条件、算法与PLC `0/49/约100ms复位`全部保持；只把正式整帧或同帧/Pose交给已验证的容量1 Worker和UI完成邮箱。硬触发队列满/Fault仍属Stage 4，不在本切片定义。
- [x] `CameraThread`新增显式外部Worker交接开关：普通定位模式继续产生原同帧/Pose，纸巾硬触发在开关启用后只交付正式整帧，不再在线程内执行纸巾算法；原纸巾内置Pipeline和结果信号作为门禁前兼容实现保留。
- [x] `Widget`增加硬触发Direct入口并统一五模式Worker启动分派；运行Profile/二维码模式快照在启动采集线程前冻结，Worker启动失败会回滚运行会话且不启动`CameraThread`。正常停止、硬触发线程异常结束和启动早退均先取消入口/UI邮箱，再等待Worker退出。
- [x] 当前切片静态审计通过：硬触发正式`signal_sendForDetection`已无旧Widget排队分发接线；新增声明/定义/调用完整，五模式索引映射保持0钢印、1字库、2深度OCR、3纸巾、4二维码；`git diff --check`通过。Agent未运行qmake、编译、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 硬触发五模式集中门禁通过：2026-08-15用户确认`detection_completion_test`、主工程构建运行、五模式硬触发代表判定/图文/统计/存图/PLC、停止再次启动及软触发回归均无问题；`DET-002..007、CAM-004`恢复已验证，`UI-004、RUN-001..004`因Stage 3后续职责拆分继续保持迁移中。
- [x] 硬触发五模式统一Worker切片已提交为`b7cca6e`（`refactor(runtime): 统一硬触发检测工作线程`），提交后工作区干净。
- [x] 开始零引用清理切片；影响`DET-002..007、RUN-003、UI-004`。候选范围仅限旧`dispatchDetectionByMode`及其独占OCR/钢印/字库/二维码算法路径、旧定位失败收尾，以及`CameraThread`门禁前纸巾内置Pipeline/结果信号；当前Worker完成处理、最终UI/统计/存图/PLC收尾和模板制作二维码验证不在删除范围。
- [x] 删除`Widget::dispatchDetectionByMode`及其独占的旧OCR、钢印、字库和二维码检测函数、旧字库/二维码定位失败收尾与旧纸巾结果槽；保留五个Worker完成处理、模式最终结果显示、唯一统计/存图/PLC收尾以及二维码模板框选即时校验。`widget.cpp`本切片净删约978行，不再同时维护两份产品算法。
- [x] 删除`CameraThread`门禁前内置`TissueDetectionPipeline`、纸巾参数副本、外部Worker切换开关和旧纸巾结果信号；硬触发纸巾现在与其他模式一样只交付正式整帧给统一Worker。Widget纸巾参数只保留为Worker启动快照，并把旧`applyTissueRecipeParametersToThreads`更名为`updateTissueRecipeParameters`。
- [x] 零引用清理Agent静态检查通过：10个旧Widget分发/算法/收尾符号及4个CameraThread纸巾兼容符号在`app/tests`源码、头文件、UI和工程文件中均为0处；五模式Worker启动分发、软硬采集Direct入口及最终完成处理声明/定义完整；二维码模板验证仍使用定向ROI和解码适配器；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 旧检测副本零引用清理Qt Creator门禁通过：2026-08-15用户确认`detection_completion_test`、主工程Run qmake/Rebuild/Run、五模式软硬触发代表检测、图像/Overlay/文字/统计/存图以及停止重启均无问题；可以提交本切片并继续Stage 3 UI结果呈现职责拆分。
- [x] 旧检测副本零引用清理已提交为`f1ade04`（`refactor(runtime): 删除旧检测分发副本`），提交后工作区干净。
- [x] 开始Stage 3结果图与Overlay Presenter切片；影响`UI-003、UI-004、RUN-004、SAVE-002`。只迁移五模式结果几何缓存、原图标注渲染、纸巾圆框和预览Pose跟随；判定、识别文字、模板名、统计、存图目录/命名、PLC以及模板制作绘图均保持原入口和语义。
- [x] 新增无控件依赖的`ui/presenters/DetectionResultPresenter`：统一持有当前结果Pose、字符/钢印多边形、钢印重叠颜色及纸巾内外圆状态，并从1/3/4通道原图生成标注`QImage`。原蓝色定位框、黄色二维码框、绿色日期/字符框、黄色分数、钢印正常黄色/重叠红色及纸巾黄/蓝圆规则保持。
- [x] Widget删除文件级结果绘制全局变量、OpenCV标注函数和旧Overlay安装桥；五模式完成收尾只把通用结果/Pose交给Presenter，`slot_displayAndDetect`缩为渲染结果交付`ImageLabel::setAutoFitPixmap`的薄桥。非生产预览Pose仍通过Presenter更新；生产期流帧/Pose抑制和同产品完整UI闭包保持。
- [x] 纸巾带框存图改为从同一Presenter状态和当前产品只读原帧生成，避免依赖控件缩放后的Pixmap；其他模式标注图、原图组合、目录/文件名、容量32无丢弃存图队列和失败提示均未修改。
- [x] `detection_completion_test`新增3项Presenter合同测试，覆盖五类Overlay颜色、四通道输入、Pose平移/失效清理及纸巾圆框/状态清理；业务测试由65项增至68项，Qt Test预期汇总由`67 passed, 0 failed`增至`70 passed, 0 failed`。
- [x] 结果图与Overlay Presenter Agent静态检查通过：主工程和运行测试工程对新源/头各唯一登记；测试声明/定义各68项；旧绘制全局状态、旧Overlay安装/转换/纸巾绘制及兼容完成构造符号在`app/tests`中均为0处；90个功能ID唯一，状态50/7/31/2；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 结果图与Overlay Presenter集中Qt Creator门禁通过：2026-08-15用户确认`detection_completion_test`预期`70 passed, 0 failed`、主工程Run qmake/Rebuild/Run、五模式结果图/Overlay/文字/统计/耗时、窗口缩放、纸巾带框保存以及停止重启均无问题；`UI-003、SAVE-002`转为已验证，`UI-004、RUN-004`继续进入后续完整呈现快照拆分。
- [x] 结果图与Overlay Presenter已提交为`b111649`（`refactor(ui): 抽离检测结果图呈现器`），提交后工作区干净。
- [x] 开始Stage 3完整结果呈现快照切片；影响`UI-004、UI-005、RUN-004、RES-001..005、SAVE-002`。五模式现有最终收尾继续执行唯一结果记录、存图选择和PLC请求，但图片、Overlay、识别文字、模板名、OK/NG、统计和耗时改为组装同一`ProductKey`只读快照并由Presenter一次写入；使用组合存图的钢印、字库、二维码、纸巾四模式标注图统一从同产品原帧和Presenter Overlay生成，不再反向读取界面Pixmap。
- [x] 新增`DetectionResultViewSnapshot/DetectionResultViewBindings`合同：快照携带`ProductKey`、原尺寸结果QImage、统一判定状态、识别内容、可选模板名、完整统计和耗时文字；Presenter由判定状态固定生成结果文字，并在一次UI线程调用中按固定顺序应用全部字段，图文不再分散到五个模式收尾中直接写控件。
- [x] 钢印、字库、二维码、深度OCR、纸巾五个收尾均先完成协调器唯一记录，再从当前`DetectionCompletion`原帧和Overlay生成一张结果图并组装快照，最后执行原PLC请求。原`x/j/judge`分支在实际代码中`x`始终为1且`judge`无true写入，已按等价行为删除，每个有效完成仍且只收尾一次。
- [x] 组合存图的四模式现直接把同一产品已呈现QImage交给`ImageSaveService`，删除从`image_undetected->pixmap()`反读缩放图的回退；标注图/原图组合、文件夹、格式、命名、任务顺序和满队列反压不变。深度OCR保持原有原帧存图入口。
- [x] `detection_completion_test`新增2项Presenter快照合同测试，覆盖全字段固定顺序应用、`ProductKey`保留、非检测预览只更新图片、停止短期清理、清理后保留UI绑定以支持再次启动，以及总数/NG两个既有不对称清零范围；业务测试由68项增至70项，Qt Test预期汇总由`70 passed, 0 failed`增至`72 passed, 0 failed`。
- [x] 完整结果呈现快照Agent静态检查通过：五个完成收尾各且仅有1处快照呈现；旧`refreshResultStatistics`、`m_allowTissueDetectionFrameDisplay`、`x/j/judge`及存图反读界面Pixmap均0处；测试声明/定义各70项，预期汇总72项；90个正式功能ID唯一，状态48/12/28/2；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 2026-08-15首次主程序门禁发现判定结果和检测耗时出现乱码，部分判定还把`font>`残片作为正文显示；结果图、Overlay、统计数值和算法判定本身正常。源码追踪确认本切片把旧窄字符串改成了含原始中文的`QStringLiteral`，在当前MSVC2017工程未统一启用`/utf-8`的条件下产生代码页误解码；同时`KeepCurrent`样式分支会让普通模式继承纸巾字号/颜色，属于本切片UI呈现回归。
- [x] 判定结果已改为`Qt::PlainText`纯文本，删除全部HTML `<font>`拼接；根据用户明确要求，样式合同进一步从普通/纸巾四种状态收敛为统一`Correct/Error`两种状态，Presenter固定生成无感叹号的“正确/错误”，五模式共用同一字号、正确色和错误色。耗时、纸巾粗糙度及同类模板错误提示继续使用代码页无关Unicode转义；钢印细分失败原因仍保留在算法结果和日志中，算法、计时数值、线程、统计、存图和PLC行为均未修改。
- [x] 现有Presenter全字段测试补充中文精确值和同一绑定从`Error`切换到`Correct`的统一样式验证；测试声明/定义仍各70项，Qt Test预期仍为`72 passed, 0 failed`。静态复查确认旧普通/纸巾四样式枚举、`KeepCurrent`、`<font>`、`</font>`和相关文件中含原始非ASCII的`QStringLiteral`均为0处，五模式快照调用仍为5处，无`.pro/.pri`变更，`git diff --check`通过；等待Qt Creator复验。
- [x] 完整结果呈现快照集中门禁通过：2026-08-15用户确认`detection_completion_test`预期72项及主程序复验均无问题；五模式统一显示纯文本“正确/错误”，普通与纸巾共用相同字号和颜色，中文、耗时、图文绑定、统计、模板名、组合存图及模式切换均通过。`UI-004、UI-005、RUN-004、RES-001..005、SAVE-002`转为已验证；`RUN-001..003`继续留给后续Widget运行装配拆分。
- [x] 完整结果呈现快照已提交为`88b4f1c`（`refactor(ui): 统一完整检测结果呈现`），提交后工作区干净。
- [x] 开始Stage 3“检测Worker与UI邮箱运行所有权收口”切片；影响`RUN-001..003`并继续保持迁移中。五模式算法闭包、模板/Profile/解码资源冻结、软硬采集线程、相机停止重开顺序、PLC值/脉冲、判定、统计和存图规则均不修改。
- [x] `InspectionRuntimeController`现统一持有活动`DetectionWorker`、模式索引和容量1 `UiCompletionMailbox`，提供Starting态启动、整帧/定位任务提交、UI完成任务串行提交/执行、协作取消、等待及重启接口；`requestStop`离开状态互斥区后先取消邮箱和Worker，避免阻塞生产者无法退出。
- [x] 阻塞的检测提交先在独立互斥区复制Worker的`shared_ptr`再调用容量1 FIFO，停止/清理无需持锁等待队列空间，既避免Worker悬空，也避免提交、UI反压与停止形成互斥锁死。析构沿同一协作停止/等待路径回收线程，不使用`terminate()`。
- [x] Widget删除Worker、活动模式、UI邮箱四项可变成员及两组停止/等待包装；五种模式仅组装现有检测闭包并交给控制器，软硬采集信号按控制器活动模式提交，原启动日志、错误提示和采集线程停止/相机恢复顺序保持。
- [x] `detection_completion_test`新增2项控制器合同测试，覆盖Starting态启动门禁、模式与容量、正式帧提交、协作停止/等待/清理，以及容量1 UI邮箱顺序、满时反压和停止取消释放；业务测试由70项增至72项，Qt Test预期汇总由`72 passed, 0 failed`增至`74 passed, 0 failed`。
- [x] 当前切片Agent静态审计通过：测试声明/定义各72项；五模式控制器启动调用恰为5处；Widget旧Worker/邮箱/活动模式/停止包装符号为0处；控制器Worker/邮箱所有权成员唯一；无`.pro/.pri`变更；90个正式功能ID状态仍为48项已基线、3项迁移中、37项已验证、2项已延期；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 检测Worker与UI邮箱运行所有权集中门禁通过：2026-08-15用户确认`detection_completion_test`预期`74 passed, 0 failed`、主工程构建运行、模板匹配软触发和二维码+三期硬触发的判定/统计/图文/存图、停止重启及跨模式切换均无问题；当前切片可以独立提交，`RUN-001..003`继续进入模式运行资源装配拆分。
- [x] 检测Worker与UI邮箱运行所有权切片已提交为`b95091e`（`refactor(runtime): 收口检测工作线程所有权`），提交后工作区干净。
- [x] 开始Stage 3“五模式Worker与算法资源装配迁出Widget”大切片；影响`RUN-001..003、DET-002..006`并进入迁移中。新增独立`DetectionModeWorkerFactory`，统一装配纸巾、深度OCR、钢印、字库和二维码+三期的容量1 Worker及模式私有运行状态；Widget仅冻结运行参数/资源、安装Worker并把类型化完成结果投递到既有UI邮箱。
- [x] 保持边界：不修改五种Pipeline算法、目标字符/阈值/Profile选择、二维码7路策略和60ms预算、相机软硬触发/取帧、PLC值与100ms脉冲、统计、存图、图文原子呈现或停止重启顺序；钢印重叠检测仍使用启动时复制的现有引擎。
- [x] 五种Pipeline构造、逐帧调用、类型化结果暂存和完成回调已全部迁入`runtime/detection_mode_worker_factory.*`；字库/二维码运行Profile由值语义只读副本持有，二维码首选策略和连续失败计数只在工厂私有副本中按原规则逐帧更新。
- [x] Widget删除五种`Software*DetectionState`和五段算法Worker闭包；五个启动函数保留原预检、参数/Profile冻结和钢印重叠引擎复制，通过统一安装函数交给`InspectionRuntimeController`启动。硬触发与软触发继续共用同一个活动Worker，原模式索引`0..4`映射不变。
- [x] `detection_completion_test`新增5项工厂合同测试，覆盖纸巾整帧与耗时结果、OCR Pose交接、钢印类型化输出、字库Profile索引门禁，以及二维码连续失败后首选策略跨帧更新；业务测试由72项增至77项，Qt Test预期汇总由`74 passed, 0 failed`增至`79 passed, 0 failed`。
- [x] 当前切片Agent静态审计通过：测试声明/定义各77项；Widget工厂调用5处、五种Pipeline构造0处、旧五种临时状态0处；工厂实现5个模式入口；主工程和运行测试`.pro`均各登记新工厂一次且依赖路径存在；正式功能状态为48项已基线、8项迁移中、32项已验证、2项已延期；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 首次Qt Creator主工程构建发现`widget.cpp`中的旧OCR最终呈现兼容函数仍按值构造`OcrDetectionResult`，而迁移Worker时一并移除了提供完整类型的OCR Pipeline头，产生C2079/C2027及后续参数级联错误。已只恢复`detection/ocr/ocr_detection_pipeline.h`直接include；OCR Pipeline构造和逐帧调用仍全部位于运行时工厂，Widget内`new OcrDetectionPipeline`保持0处，测试数量和`.pro`清单不变，等待重新构建。
- [x] 五模式运行时工厂Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`预期`79 passed, 0 failed`、主工程重新构建运行、模板匹配/字库/深度OCR/纸巾软触发及二维码硬触发的结果、图文、耗时、统计、存图、停止重启均无问题；`DET-002..006`恢复已验证，`RUN-001..003`继续进入运行配置装配拆分。
- [x] 五模式运行时工厂切片已提交为`f6a05f2`（`refactor(runtime): 收口五模式检测装配`），提交后工作区干净。
- [x] 开始Stage 3“统一运行Profile快照与五模式启动分发”大切片；影响`RUN-001..003、DET-002..006`并进入迁移中。目标是一次生成同序的定位/检测Profile快照，删除Widget运行Profile副本和五个重复模式启动函数；算法、采集、PLC、结果与存图保持不变。
- [x] 新增`InspectionProfileSnapshotBuilder`：按原Profile顺序一次生成定位线程的`WordTrackingProfile`和检测Worker的`DetectionModeWorkerProfile`，定位模板及字符模板在启动时拥有独立图像数据；Profile名称回退、整数阈值/界面阈值回退、二维码参数和运行内首选策略规则保持原样。
- [x] Widget删除`m_runningWordTemplateProfiles`、`m_wordTemplateRunActive`及对应清理函数；软硬触发均把同一局部不可变快照分别交给定位线程和模式Worker，Worker创建后持有自己的只读副本，不再把编辑态Profile作为运行状态长期保存在主窗口。
- [x] 五个`startSoftware*DetectionWorker`合并为一个模式请求入口；`DetectionModeWorkerDispatcher`统一执行模式索引、资源门禁、Worker类型、启动失败文本和日志名称映射，既有Factory仍唯一构造五种Pipeline Worker。钢印重叠回调、OCR/条码设备接口及五类UI完成回调保持原实现。
- [x] `detection_completion_test`新增5项合同测试，覆盖运行快照顺序与图像所有权、旧小数阈值回退和二维码策略、非法/缺资源请求拒绝、纸巾分发创建及条码DLL加载门禁；业务测试由77项增至82项，Qt Test预期汇总由`79 passed, 0 failed`增至`84 passed, 0 failed`。
- [x] 当前切片Agent静态审计通过：测试声明/定义各82项；Widget直接Factory创建0处、统一Dispatcher调用1处、旧五启动函数/运行Profile成员/清理函数0处；快照Builder调用1处；主工程与运行测试`.pro`各登记新快照实现1次；90个正式功能ID状态为48项已基线、8项迁移中、32项已验证、2项已延期；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 统一运行Profile快照与五模式启动分发Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`预期`84 passed, 0 failed`、主工程Run qmake/Rebuild/Run及五种模式启停、字库/二维码Profile参数与软硬触发、图文判定、耗时、统计和存图均无问题；`DET-002..006`恢复已验证，`RUN-001..003`继续进入软硬触发启动事务拆分。
- [x] 统一运行Profile快照与五模式启动分发已提交为`f679f11`（`refactor(runtime): 统一检测启动快照与分发`），提交后工作区干净。
- [x] 开始Stage 3“软硬触发启动事务与相机时序迁出Widget”大切片；影响`CAM-003..004、RUN-001..003`并进入迁移中。保持软触发TriggerSource=7/曝光/增益顺序，以及硬触发stop/200ms/TriggerMode/Line0/曝光/增益/TriggerDelay/回调/start/LineDebouncerTime=5000/100ms顺序不变；同时把Starting、Worker安装、Running提交和失败回滚收口为运行事务。
- [x] 新增`InspectionCameraStartTransition`，软触发精确保留TriggerSource=7、曝光、增益三步；硬触发精确保留停止采集、200ms、TriggerMode=1、Line0、曝光、增益、TriggerDelay=0、注册回调、开始采集、LineDebouncerTime=5000、100ms顺序。相机SDK返回值继续按旧路径处理，硬触发异常仍转换为原“相机初始化失败”提示。
- [x] 新增RAII `InspectionRuntimeStartTransaction`，统一执行Idle→Starting、容量1检测Worker安装、Starting→Running提交；Worker创建/启动失败、硬触发采集线程立即退出或未提交即离开作用域时，统一取消UI邮箱、协作停止并等待Worker后恢复Idle。
- [x] Widget删除`beginInspectionStart`、`markInspectionRunning`、`finishInspectionStop`三个单行状态包装器；正式检测启动不再直接调用`InspectionRuntimeController::beginStart/markRunning/startDetectionWorker`，只保留界面值、模式资源、Qt采集线程信号和原提示映射。
- [x] `detection_completion_test`新增6项合同测试，覆盖硬触发11步精确调用顺序、软触发3步精确调用顺序、曝光失败短路、事务提交、带Worker显式回滚及析构自动回滚；业务测试由82项增至88项，Qt Test预期汇总由`84 passed, 0 failed`增至`90 passed, 0 failed`。
- [x] 当前切片Agent静态审计通过：测试声明/定义各88项；正式启动按钮内直接`beginStart/markRunning/startDetectionWorker`为0处，直接相机启动SDK调用为0处，旧三个状态包装器为0处；四个新增运行层文件在主工程和运行测试`.pro`中均各登记1次且依赖路径存在；90个正式功能ID状态为48项已基线、5项迁移中、35项已验证、2项已延期；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 软硬触发启动事务与相机时序Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`为`90 passed, 0 failed`，主程序软触发和硬触发均能启动、停止并再次启动；`CAM-003..004、RUN-001..003`恢复已验证。
- [x] 软硬触发启动事务与相机时序切片已提交为`0eea9ae`（`refactor(runtime): 收口检测启动事务与相机时序`），提交后工作区干净。
- [x] 开始Stage 3“正式停止、相机恢复与采集线程生命周期迁出Widget”大切片；影响`CAM-003..005、RUN-002..003`并进入迁移中。目标是保持软/硬线程requestStop、stop、3000ms等待、stopTracking、检测Worker等待，以及close/100ms/open0/软件触发/曝光/TriggerDelay/回调/start恢复顺序不变，由runtime统一编排并返回类型化结果。
- [x] 新增`InspectionAcquisitionStopCoordinator`：先同时请求软硬采集停止，再按旧顺序执行软件stop/3000ms等待/stopTracking与硬件断连/再次requestStop/3000ms等待/stopTracking/deleteLater；任一线程超时均返回类型化失败并保持运行状态为Stopping，不提前恢复相机或提交Idle。
- [x] 新增`InspectionCameraRecoveryTransition`：在采集线程全部退出后按原close→100ms→open0→TriggerMode=1→TriggerSource=7→保存曝光→TriggerDelay=0→回调→start顺序恢复软件触发相机；曝光失败仍关闭相机、恢复曝光控件并显示原警告，打开失败和初始化异常继续保持旧失败语义。
- [x] 新增独立`InspectionRuntimeStopTransaction`，正式停止、软件采集线程自然结束和硬件采集线程自然结束不再直接组合`requestStop/waitForDetectionWorkerStop/finishStop`；停止超时时事务不提交，正常结束才进入Idle。Widget保留Qt对象绑定、最后结果保留及既有提示，不修改算法、PLC、统计、存图和关闭窗口流程。
- [x] `detection_completion_test`新增7项合同测试，覆盖软硬采集精确停止顺序、空闲软件线程跳过等待、线程超时不执行停止后动作、相机恢复精确调用顺序、打开失败、曝光失败关闭相机及停止事务Stopping→Idle提交；业务测试由88项增至95项，Qt Test预期汇总由`90 passed, 0 failed`增至`97 passed, 0 failed`。
- [x] 当前切片Agent静态审计通过：测试声明/定义各95项；正式停止按钮及软硬线程自然结束路径直接组合控制器停止调用为0处；六个新增runtime源码均为ASCII，主工程和运行测试工程对每个新源/头各登记1次；90个正式功能ID状态为47项已基线、5项迁移中、36项已验证、2项已延期；`git diff --check`通过。Agent未运行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 正式停止、相机恢复与采集线程生命周期Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`为`97 passed, 0 failed`，主程序软硬触发停止、相机恢复、最终结果保留和再次启动均无问题；`CAM-003..005、RUN-002..003`转为已验证。

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
| `38263e7` | Stage 1字符裁切完成后发布 | TPL-006、TPL-015 | 新建单Profile字库配方在字符裁切、设置重读和目标模板重载全部成功后事务发布 | Agent静态检查、`recipe_store_test` 20项及主程序实际发布门禁均通过 |
| `c81e3b8` | Stage 1新建配方草稿会话 | TPL-006、TPL-015 | 把稳定UUID、源目录门禁和发布后头信息从Widget抽入recipes层会话 | Agent静态检查、`recipe_store_test` 21项及主程序重复发布门禁均通过 |
| `e875619` | Stage 1已发布配方可编辑会话 | SET-003、TPL-009..014 | 从不可变选择建立保持UUID、Profile身份和内部资产绑定的编辑工作副本，支持参数更新后事务回存 | Agent静态检查、`recipe_store_test` 23项及主程序旧入口回归均通过；当时尚未连接Widget |
| `f267755` | Stage 1新建配方目标字符重新发布 | SET-003、TPL-006、TPL-011 | 首次发布选择绑定编辑会话，原“确认字符”成功后使用相同UUID事务重新发布 | Agent静态检查、`recipe_store_test` 23项、主程序同UUID重新发布及旧模板选择均通过 |
| `62cbc00` | Stage 1新建配方图像阈值重新发布 | SET-003、TPL-013 | 原单Profile图像阈值设置成功后更新编辑会话，并使用相同UUID事务重新发布 | Agent静态检查、`recipe_store_test` 23项、主程序同UUID重新发布、目标保持及旧模板选择均通过 |
| `250c3bd` | Stage 1编辑会话整批Profile原子更新 | SET-003、TPL-012 | 完整有序Profile候选通过数量、逐索引身份/资产及Schema校验后一次替换会话工作副本 | Agent静态检查及`recipe_store_test` 24项均通过；当时尚未连接Widget批量按钮 |
| `089b398` | Stage 1批量目标字符重新发布 | SET-003、TPL-012 | 原批量目标字符全部成功后，完整有序Profile原子更新并使用相同UUID只重新发布一次 | Agent静态检查、`recipe_store_test` 24项、主程序单次同UUID发布、阈值保持及旧模板选择均通过 |
| `8de6ed6` | Stage 1批量图像阈值重新发布 | SET-003、TPL-014 | 原批量阈值全部成功后，完整有序Profile原子更新并使用相同UUID只重新发布一次 | Agent静态检查、`recipe_store_test` 24项、主程序单次同UUID发布、目标保持及旧模板选择均通过 |
| `60d6087` | Stage 1字库Profile只读资源桥 | SET-003、TPL-009..012 | 统一原图、字符模板及目标字符验证的资源角色读取，并兼容旧目录 | Agent静态检查、`recipe_store_test` 24项、主工程旧Profile显示和参数入口均通过 |
| `c9394f2` | Stage 1字库Profile参数持久化分流 | SET-003、TPL-011..014 | 旧Profile继续写INI，新配方参数只经编辑会话和RecipeStore事务回存 | Agent静态检查、`recipe_store_test` 24项、旧Profile四个参数入口和重新选择均通过 |
| `6e1f498` | Stage 1二维码启动预检资源桥 | TPL-009、DET-006、RUN-001 | 二维码启动预检统一读取Profile资源角色并兼容旧目录 | Agent静态检查、二维码+三期旧模板启动停止门禁通过；算法和硬件主链未改 |
| `19dc8ac` | Stage 1已发布配方单选入口 | SET-003、TPL-009..014、DET-003、DET-006 | 字库家族按当前模式单选已发布配方并原子装配编辑会话，参数同UUID重新发布，目录占用给出加粗红色操作提示 | `recipe_store_test` 24项及主程序选择、参数、重发布、启停、旧入口和占用提示均由用户确认通过 |
| `feb3fe5` | Stage 1按模式恢复已发布配方 | SET-001、SET-002、TPL-009、TPL-016 | 字库家族分别持久化单个配方UUID，模式切换和重启优先原子恢复，失效回退旧路径 | `product_recipe_test` 10项及主程序切换、重启、参数恢复和旧模板覆盖规则均由用户确认通过 |
| `9aad739` | Stage 1已发布配方字符资产编辑 | SET-003、TPL-011、TPL-012、TPL-015 | 临时工作区裁切、当前Profile资产命名空间原子替换和同UUID重新发布 | `recipe_store_test` 25项及主程序裁切、重选/重启和旧入口均由用户确认通过 |
| `1e7fe47` | Stage 1多Profile产品配方发布 | TPL-006、TPL-009、TPL-010、TPL-012、TPL-014、TPL-016 | 将有序旧Profile组一次发布为一个产品配方，并接入完整编辑会话 | `recipe_store_test` 26项及主程序发布、批量编辑、恢复、启停和旧入口均由用户确认通过 |
| `47c7c40` | Stage 1字库运行Profile快照 | DET-003、DET-006、RUN-001、TPL-009、TPL-010 | 两个字库家族启动时深拷贝统一只读运行Profile，停止和线程结束统一释放 | 两模式及旧多目录入口启停/再次启动由用户确认通过；算法和硬件时序不变 |
| `ccb6b5a` | Stage 1四模板模式配方合同 | SET-003、TPL-006、TPL-007、TPL-008 | 统一模板模式判定和必需资产角色，钢印显式要求`stampRing`，草稿/发布/重发/选择共享合同 | `recipe_store_test` 27项和`product_recipe_test` 10项由用户确认通过 |
| `17987fc` | Stage 1钢印与OCR产品配方闭环 | SET-001、SET-003、UI-001、TPL-006..008、TPL-011、TPL-013、TPL-015、TPL-016、DET-002、DET-004、RUN-001 | 两种单模板模式接通发布、选择、同UUID参数/字符资产重发、模式恢复；钢印保留字符裁切和重叠检测双能力 | 用户确认主工程、两配方测试、钢印字符模板实际保存/同UUID重发、两模式启停及旧入口均正常 |
| `89934ee` | Stage 1统一模板配方事务工作流 | SET-003、TPL-006、TPL-011..015 | 草稿发布、参数重发和字符资产重发统一为候选校验、正式发布、成功后提交会话的单一事务边界 | 用户确认配方测试、主工程及字库/二维码/钢印可执行路径正常；深度OCR无旧模板项未单独执行 |
| `ef3af6d` | Stage 2二维码解码设备适配 | TPL-004、DET-006、RUN-001、TOOL-002 | DLL生命周期、C ABI调用和7路通用解码策略迁入`devices/barcode/`，Widget改用`IBarcodeDecoder` | 用户确认适配器8项、二维码Pipeline 6项、真实DLL/框选读码、启停和跨模式恢复均正常 |
| `c78ebb8` | Stage 2 Paddle OCR设备适配 | SYS-006、DET-004、TPL-002、TPL-003、TPL-005 | Paddle配置、模型对象和原生调用收口到`devices/ocr/`，Pipeline依赖`IOcrEngine`，并恢复深度OCR模板绘图入口 | 用户确认OCR测试6项、主工程模型初始化、模板制作/发布、逐帧OCR调用和启停均正常 |
| `9542aaa` | Stage 2 Snap7 PLC设备适配 | SYS-008、SET-005、CAM-001、RUN-001、PLC-001..006 | `TS7Client`、DB区和数据宽度常量收口到`devices/plc/`，Widget改用`IPlcDevice` | 用户确认适配器9项及主工程可执行门禁无问题；真实PLC读回和脉冲证据仍延期 |
| `e6dc0d2` | Stage 2海康单相机设备适配 | SYS-009、SET-005..007、TPL-001..002、CAM-001..005、RUN-001..003、SAVE-004 | 海康SDK枚举、首台打开、参数、回调、帧读取与停止唤醒收口到`devices/camera/`，Widget和两采集线程改用共享`ICameraDevice` | 用户确认适配器测试及主工程开关相机、参数、预览、启停、存图和退出均无问题；现场量化证据仍延期 |
| `fabc7b0` | Stage 2检测结果帧与无丢弃存图 | DET-002..006、RUN-001、SAVE-001..005 | `DetectionCompletion`统一携带本次检测只读原帧；五模式存图统一为容量32、双写线程、满时等待的产品任务队列 | 用户确认更新后的运行测试及主工程无问题；正常条件下不再因队列容量漏图，实际磁盘失败仍报警 |
| `45e7340` | Stage 2检测运行会话 | RUN-001、DET-002..006 | 运行UUID、产品递增序号及完成对象组装迁入`DetectionSession`，Widget删除对应可变状态 | 用户确认运行测试及主工程多模式启停、再次启动、判定、计数、存图和PLC均无问题 |
| `6050116` | Stage 2统一结果处理与ROI边界 | UI-002、UI-005、DET-002..008、RUN-002、PLC-005..007、RES-001..003、SAVE-001 | 五模式统计、存图选择、PLC结果请求和延迟剔除收口到`DetectionResultHandler`；日期ROI期望外扩20像素并裁到原图边界 | 用户确认运行测试23项、靠边ROI连续检测/停止、OK/NG、统计、存图、PLC、延迟剔除和清零入口均正常 |
| `328aca5` | Stage 2检测运行协调器 | DET-002..006、RUN-001..003、PLC-005..007、RES-001..003、SAVE-001 | 统一启停状态、运行会话、结果统计、存图决策和PLC请求，并拒绝重复/外来完成对象 | 用户确认运行测试29项、模板匹配软触发、二维码硬触发、启停/再次启动、判定/统计/存图均正常 |
| `5026314` | Stage 2启动预检与参数门禁 | RUN-001、SET-004 | 统一启动拒绝优先级、未应用参数确认及四类模式资源完整性判定，Widget保留原提示和硬件执行 | 用户确认运行测试37项及主工程相机/模板保护、参数取消/继续、软硬触发启停均正常 |
| `02aac8d` | Stage 2采集运行配置与线程装配 | RUN-001、CAM-003、CAM-004、CAM-006、SET-008..010 | 一次生成软硬采集/跟踪计划并统一装配两类原采集线程，抽出阈值、旋转和通道运行参数解析 | 用户确认运行测试44项及模板、纸巾、二维码代表模式、旋转/通道、判定/统计/存图/PLC均正常 |
| `e333c24` | Stage 3影子结果对照基础 | DET-001、RES-001、RES-004（对照范围） | 无副作用比较模式、判定、状态、文字和有序Overlay，显式坐标/分数容差并排除耗时 | 用户确认运行测试50项及主工程构建启动正常；尚未接正式入口 |
| `dde929f` | Stage 3有界帧队列与串行检测线程基础 | RUN-002..004、CAM-003..004 | 容量固定、满时等待、可取消重开的FIFO帧队列，以及单线程串行检测、协作停止和迟到结果抑制 | 用户确认运行测试58项及主工程构建启动正常；尚未接正式采集入口 |
| `64f7635` | Stage 3检测前产品帧受理合同 | RUN-001..004、DET-002..006 | 检测前分配唯一产品身份并深拷贝只读帧，完成时校验同一受理对象、顺序、重复及运行归属 | 用户确认运行测试61项及主工程软硬触发代表路径均无问题；正式采集入口当时尚未切换 |
| `78dd660` | Stage 3纸巾软触发正式运行链 | DET-005、CAM-003、RUN-001..004 | 纸巾软触发整帧经有界队列进入串行检测Worker，同一产品帧回UI完成原结果副作用 | 用户确认连续处理66帧、停止取消0帧，OK/NG、存图、停止及模板匹配软触发回归均正常 |
| `da8db2d` | Stage 3深度OCR软触发正式运行链 | DET-004、CAM-003、RUN-001..004 | 定位任务携带同一原帧和Pose进入容量1串行Worker，完成ROI、OCR和通用结果后回UI执行原副作用 | 用户确认相关测试、真实模型模板制作、软触发检测、停止重启及其他模式回归均无问题 |
| `0001281` | Stage 3统一软触发检测与结果呈现链 | DET-002、DET-003、CAM-003、RUN-001..005、UI-004 | 共享字符匹配、钢印/字库容量1 Worker、容量1 UI完成邮箱、新帧采集和0间隔语义 | 用户确认运行测试67项、钢印/字库Pipeline各8项，以及图文原子刷新、纸巾同步、全部保存、停止重启均正常 |
| `0128a14` | Stage 3二维码软触发正式运行链 | DET-006、DET-007、CAM-003、RUN-001..004、UI-004 | 二维码+三期同帧/Pose进入容量1 Worker，定向ROI、读码优先/失败短路、日期字符判定和完整UI结果包 | 用户确认Pipeline 10项、实际软触发读码/日期、图文统计同步、中文判定、停止重启及硬触发兼容均正常 |
| `b7cca6e` | Stage 3硬触发统一检测工作线程 | CAM-004、RUN-001..004、UI-004、DET-002..007 | 五模式硬触发正式帧/同帧Pose接入容量1 Worker和UI邮箱，保留原Line触发、定位、二维码取帧及PLC时序 | 用户确认五模式硬触发、停止重启和软触发回归均无问题 |
| `f1ade04` | Stage 3旧检测副本零引用清理 | DET-002..007、RUN-003、UI-004 | 删除Widget旧模式分发和CameraThread内置纸巾检测副本，生产检测只保留统一Worker路径 | 用户确认五模式软硬触发、结果、存图和停止重启均无问题 |
| `b111649` | Stage 3结果图与Overlay Presenter | UI-003、UI-004、RUN-004、SAVE-002 | 抽离五模式结果几何与标注图渲染，纸巾圆框和组合存图使用同产品原帧 | 用户确认运行测试70项及五模式图像、Overlay、缩放和存图均无问题 |
| `88b4f1c` | Stage 3完整检测结果呈现 | UI-004、UI-005、RUN-004、RES-001..005、SAVE-002 | 同一ProductKey快照一次更新图像、框、文字、统一“正确/错误”、统计、模板名和耗时 | 用户确认运行测试72项及五模式统一样式、图文绑定、组合存图和模式切换均无问题 |
| `b95091e` | Stage 3检测Worker运行所有权 | RUN-001..003 | 活动Worker、模式索引和容量1 UI完成邮箱收口到`InspectionRuntimeController`，Widget只保留模式装配桥 | 用户确认运行测试74项及软硬触发代表模式、图文统计存图、停止重启和跨模式切换均无问题 |
| `f6a05f2` | Stage 3五模式检测Worker装配 | RUN-001..003、DET-002..006 | 五种Pipeline构造、逐帧调用、私有状态和类型化完成结果迁入`DetectionModeWorkerFactory` | 用户确认运行测试79项及五模式软硬触发、结果、图文、耗时、统计、存图和停止重启均无问题 |
| `f679f11` | Stage 3统一运行Profile快照与启动分发 | RUN-001..003、DET-002..006 | 同序定位/检测Profile深拷贝快照、五模式统一分发，删除Widget五个重复启动函数和运行Profile副本 | 用户确认运行测试84项、主工程五模式软硬触发、图文判定、耗时、统计、存图和停止重启均无问题 |
| `e45bb0c` | Stage 3正式停止与相机恢复 | CAM-003..005、RUN-002..003 | 停止请求、采集线程等待、Worker退出及相机恢复顺序收口到运行事务 | 用户确认运行测试、主工程软硬触发停止、再次启动和相机恢复均无问题 |
| `c9c5dd7` | Stage 3统一检测完成协调 | DET-002..006、UI-004..005、RUN-004、PLC-005..007、RES-001..005、SAVE-001..003 | 五模式统计、存图任务、同产品UI快照和PLC请求按固定顺序由统一控制器执行 | 用户确认运行测试、主工程代表模式、图文统计、存图、PLC与停止重启均无问题 |
| `9adc05d` | Stage 3最终UI/设置/模板状态收口 | UI-001、SET-003..005、TPL-001、TPL-009..011、TPL-013、TPL-016、CAM-001..002、RUN-001..002 | 操作UI策略、未应用参数状态、运行Profile数据类型和按模式模板记忆迁出Widget | 用户确认运行测试112项、配方测试31项及主程序最终集中门禁均无问题 |

- [x] Stage 3正式停止、相机恢复与采集线程生命周期切片Qt Creator门禁由用户确认无问题，并已提交为`e45bb0c`（`refactor(runtime): 收口正式停止与相机恢复`），提交后工作区干净。
- [x] 用户明确将Stage 3剩余开发压缩为最多两轮：本轮整体迁移检测完成后的统计、存图、PLC与UI呈现桥；最后一轮集中完成模板/设置/UI剩余职责、零引用清理和Stage 3收口，不再拆成更多开发轮次。
- [x] 开始Stage 3统一检测完成控制器切片；受影响功能ID为`DET-002..006、UI-004..005、RUN-004、PLC-005..007、RES-001..005、SAVE-001..003`，19项由已验证转为迁移中。固定原顺序为：处理到期延迟NG→准备同产品结果图/文字→唯一记录统计和存图决策→提交存图任务→一次呈现完整快照→发出当前产品PLC请求；五模式算法、相机与触发、PLC值/100ms脉冲、存图组合/目录/命名和结果文案均不修改。
- [x] 新增全ASCII `ui/controllers/detection_completion_controller.*`：统一执行到期延迟NG、运行层唯一记录、存图任务构造/提交、统计写入同产品快照、Presenter一次呈现和当前产品PLC请求；PLC设备写入仍由Widget两个硬件薄桥完成，值49/0和100ms复位不变。控制器显式保留四模式“标注+原图”与OCR“仅原图”的目录、时间戳和PNG/JPG差异，并保留钢印/字库/二维码对`NotEvaluated`不存图的旧分支语义。
- [x] 五个模式收尾全部改为只准备本模式Overlay、识别文字、模板名和耗时，再提交一个`DetectionCompletionProcessRequest`；Widget删除`processDueDelayedNgRequest/applyPlcResultRequest/presentDetectionResult`以及五个旧存图辅助入口和本地路径/格式组装函数。Widget不再直接调用`record/consumeDueDelayedNgRequest/ImageSaveService::submit/DetectionResultPresenter::present`，本轮净减少190行和8个成员函数定义。
- [x] `detection_completion_test`新增7项完成控制器合同测试，覆盖无效请求零副作用、到期NG在当前记录前发出、当前PLC在呈现后发出、标注后原图顺序、仅标注/仅原图组合、OCR原目录/JPG命名、缺失标注图告警以及重复完成不重复呈现/PLC；业务测试由95项增至102项，Qt Test预期汇总由`97 passed, 0 failed`增至`104 passed, 0 failed`。
- [x] 统一检测完成控制器Agent静态检查通过：19个受影响功能ID唯一且均为迁移中，正式功能状态47/19/22/2；新头/源在主工程和运行测试工程各唯一登记1次且非ASCII字节0；运行测试声明/定义各102项；五模式控制器调用5处；Widget直接`record/consumeDueDelayedNgRequest/ImageSaveService::submit/DetectionResultPresenter::present`及8个旧桥函数引用均为0；Widget相对上一提交净减190行、成员函数定义净减8；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] 统一检测完成控制器Qt Creator集中门禁通过：用户确认本轮测试、主工程构建运行及代表模式回归均无问题；五模式判定、同产品图文统计、存图组合、PLC请求顺序、停止重启保持正常。19个迁移中功能恢复为已验证，允许创建本地提交并进入Stage 3最后一轮收口。
- [x] 统一检测完成控制器已提交为`c9c5dd7`（`refactor(ui): 统一检测完成协调`），提交后工作区干净；按用户明确限制，当前进入Stage 3最后一轮开发，不再新增后续Stage 3开发轮次。
- [x] 开始Stage 3最终UI/设置/模板状态收口切片；影响`UI-001、SET-003..005、TPL-001、TPL-009..011、TPL-013、TPL-016、CAM-001..002、RUN-001..002`。只迁移Widget内的操作按钮状态策略、未应用参数dirty状态、运行Profile数据类型和按模式模板路径/配方UUID记忆；检测算法、设备调用、配方事务、模板制作步骤、触发与停止时序均不修改。
- [x] 新增`ui/controllers/OperationUiPolicy`：以CameraClosed、CameraReady、Detecting、Stopping、TemplatePreviewing、TemplateFrozen六种状态生成完整按钮可用性、原按钮文字和模板制作状态文字；Widget只负责把快照应用到现有控件并保留原注意动画。
- [x] 新增`ui/controllers/SettingsEditState`：统一登记全局设置、去重同名脏项、合并模板目标字符/阈值未应用状态，并分别支持全局与模板范围清理；启动前原确认框、继续时恢复已应用值、标签星号及提示顺序保持。
- [x] 新增`recipes/TemplateModeMemory`和`TemplateRuntimeProfile`：五模式ID映射、旧模板路径及已发布配方UUID记忆迁出Widget，旧未知索引继续回退字库模式；字库家族运行Profile字段原样形成独立数据合同，Widget不再嵌套定义该资源结构。
- [x] `detection_completion_test`新增8项操作UI与dirty状态合同测试，业务测试由102项增至110项，Qt Test预期汇总由`104 passed, 0 failed`增至`112 passed, 0 failed`；`recipe_store_test`新增3项模式映射与两类记忆隔离测试，业务测试由26项增至29项，Qt Test预期汇总为`31 passed, 0 failed`。
- [x] Stage 3最终UI/设置/模板状态切片Agent静态门禁通过：主工程7个新头/源各唯一登记1次，两个测试工程登记完整；7个新文件非ASCII字节均为0；运行测试声明/定义各110项、配方测试声明/定义各29项；Widget旧两类模式Map、两项模板dirty布尔、嵌套OperationState/WordTemplateProfile和Binding内dirty均为0处，Widget头/源合计净减139行；90个正式功能状态为36/14/38/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户最后一次Qt Creator集中门禁。
- [x] Stage 3最终UI/设置/模板状态Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`为`112 passed, 0 failed`、`recipe_store_test`为`31 passed, 0 failed`，主工程Run qmake/Rebuild/Run，以及相机操作按钮、模板制作按钮/状态、未应用参数提示、字库/钢印/二维码跨模式模板路径与已发布配方记忆、代表模式启停重启均无问题。14个迁移中功能恢复为已验证。
- [x] Stage 3约定开发范围完成：五模式软硬触发正式帧经容量1有界队列串行检测，结果以同一`ProductKey`完整快照呈现；正常存图容量32满时反压不丢任务；Worker、结果协调、Presenter、设备适配、配方事务及本轮UI状态已迁出主窗口对应业务实现。按用户两轮上限不再追加Stage 3开发切片，等待创建本地提交与结项记录。
- [x] Stage 3最终开发切片已提交为`9adc05d`（`refactor(ui): 收口模板与设置状态`）；提交包含14项功能的最终门禁证据和主计划执行状态。Stage 3结构开发正式收口，不再追加Stage 3开发轮次。

## Stage 3结项摘要

- 生产检测链：五模式软硬触发均通过容量1有界FIFO进入串行Worker；检测帧、判定、Overlay、文字、统计和耗时使用同一`ProductKey`完成对象与呈现快照。
- 正常副作用：统计、容量32无丢弃存图任务、延迟NG和PLC结果请求均只有一条正式协调链；Stage 1至3未改变PLC值、约100ms脉冲和异常归类语义。
- 停止与恢复：采集入口先停止受理，邮箱/Worker协作取消并等待，随后按原顺序恢复相机；用户已验证停止和再次启动。
- UI与配方：设备适配、配方事务、模式Worker、结果Presenter、完成控制器、操作UI策略、未应用参数状态和模式模板记忆均已有独立模块；Widget保留界面组合、Qt槽和必要薄桥。
- 门禁结论：Stage 3约定结构开发和功能回归通过。固定实际样本、真实PLC量化、P50/P95、最大队列深度及30分钟/4小时现场运行仍按用户决定延期，故生产现场最终验收未关闭。
- 后续边界：Stage 4才允许实现PLC断线、硬触发FIFO溢出、无法保证单件唯一结果时的Fault策略及最终异常清理；当前未开始。

## 未解决事项

| 问题 | 风险 | 是否阻塞当前门禁 | 下一步/需要谁确认 |
|---|---|---|---|
| 五模式实际固定样本和模板尚未登记 | 不能建立可重复算法/结果基线 | 不阻塞当前开发；阻塞最终验收 | 用户明确接受风险并延期，后续可补样本路径 |
| PLC精确读回、硬触发脉冲量化尚无本轮现场数值 | 不能量化外部副作用基线；代表硬触发启停已人工通过 | 不阻塞当前Stage 3结构开发；阻塞最终硬件验收 | 用户明确延期；Stage 0-3保持旧PLC值、脉冲和硬触发实现 |
| 单帧内存、P50/P95、慢盘存图尚无数值 | 不能量化Stage 2/3是否退化 | 不阻塞当前并发基础切片；阻塞Stage 3最终性能验收 | 用户明确延期；生产入口接入后再补10分钟内存与节拍证据 |

## 当前结论（2026-08-15复核更正）

- 已提交的Fault合同切片`0d2811a`及其`130 passed, 0 failed`门禁证据继续有效；错误的是把该切片通过解释成整项重构完成。
- 当前阶段重新打开：按最终架构约束继续迁移设备组装、PLC业务命令、相机与采集线程生命周期、剩余主窗口业务，并逐项关闭功能表中的`已基线`状态。
- 当前功能状态计数：待盘点0 / 已基线28 / 迁移中14 / 已验证46 / 已延期2 / 已确认删除0。
- 当前切片完成前不提交；Agent只做静态检查，交由用户在Qt Creator执行构建和回归门禁。

## Stage 4第一轮：Fault状态合同与异常统计

- [x] 用户明确授权开始Stage 4第一轮；影响功能限定为`DET-008、RUN-001..004、RES-001..003`，八项由已验证转为迁移中。第一轮不接真实PLC、硬触发溢出信号、异常产品PLC收尾或UI人工恢复。
- [x] 新增全ASCII `runtime/inspection_fault_state.*`，定义PLC断线、硬触发FIFO溢出、产品身份无法保证和运行不变量破坏四类原因；故障快照固定首个原因、诊断、运行ID、受理/完成产品数、发生时间和Fault后拒收帧数，重复故障不能覆盖首因。
- [x] `InspectionRuntimeController`只允许`Starting/Running/Stopping`进入Fault；进入时关闭活动Worker和容量1 UI邮箱，Fault状态禁止重新启动、拒绝新的正式帧且不创建`ProductKey`，正常停止事务的`finishStop`不能把Fault覆盖为Idle；人工确认会等待Worker退出后回到Idle并清除当前故障快照。
- [x] `DetectionResultHandler`新增独立`DetectionAbnormalStatistics`，分别统计系统故障、取消产品、未确认产品和Fault后拒收帧；现有产品总数、NG和合格率公式保持原结构，普通统计清零与异常统计清零互不影响。
- [x] `detection_completion_test`新增7项合同测试，覆盖首因快照、空闲态拒绝进入Fault、Fault后新帧不分配产品身份、Worker取消与停止提交不覆盖Fault、异常统计不进入产品合格率、Fault中禁止完成副作用、人工确认只清活动故障；业务测试由110项增至117项，Qt Test预期汇总为`119 passed, 0 failed`。
- [x] 本轮Agent静态门禁通过：改动文件全部限于运行时Fault合同、运行测试、两处工程清单和三份治理文档；新头/源在主工程与运行测试工程各唯一登记1次且非ASCII字节0；运行测试声明/定义各117项，Qt Test预期`119 passed, 0 failed`；真实生产代码中无`enterFault`调用，确认本轮未接PLC、硬触发或UI故障源；八项受影响功能唯一且均为迁移中，状态计数36/8/44/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] Stage 4第一轮Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test` 119项、主工程Run qmake/Rebuild/Run、模板匹配软触发、二维码+三期硬触发、正常判定/统计/存图/PLC、停止重启以及总数/NG清零均无问题；八项功能恢复为已验证，允许创建本地提交并进入第二轮。
- [x] Stage 4第一轮已创建本地提交`6044e6a`（`refactor(runtime): 建立Fault状态合同`），提交后工作区干净。

## Stage 4第二轮：真实故障源、持续报警与人工恢复

- [x] 开始Stage 4第二轮；影响`UI-002、UI-005、DET-008、RUN-001..004、PLC-005..007`十项，由已验证转为迁移中。普通算法NG、普通存图失败、五种Pipeline、正常PLC值0/49、约100ms复位和软触发背压规则均不修改。
- [x] `FrameQueue`和`DetectionWorker`新增非阻塞`trySubmit`结构化结果；硬触发入口使用该接口，在容量1 FIFO已满时立即报告`QueueFull`并进入`HardTriggerQueueOverflow` Fault。软触发继续使用原阻塞`submit`，正常条件下仍按检测完成速度反压取图，不丢已受理帧。
- [x] PLC真实故障入口接入Widget硬件薄桥：运行中500ms连接状态检查、OK写0、NG写49和100ms后复位写0任一断连/写失败均进入`PlcDisconnected` Fault；正常DB1.DBB1033值和Timer时序未改，设置页普通写入失败仍保持原参数提示。
- [x] 新增`InspectionFaultPresenter`和`OperationUiState::Fault`：Fault时冻结正式结果画面、持续显示红色“系统故障/检测已暂停”，禁用启动、关闭相机、模板和设置等普通动作，只保留“确认故障并恢复”；操作提示明确“输送线状态未知”，要求使用输送线自身控制停机并隔离产品，且明确解除软件锁定不代表输送线已停止。
- [x] 人工恢复复用现有协作停止、Worker等待和相机恢复顺序；现场确认对话框默认取消，确认后才调用`acknowledgeFault`回到Idle。采集线程在确认前仍可能产帧，但新帧只增加Fault后丢弃计数，不创建`ProductKey`、不进入检测FIFO、不更新产品统计或请求PLC结果。
- [x] `detection_completion_test`新增4项合同测试，覆盖Fault UI锁定、输送线未知提示、非阻塞FIFO满且不覆盖队首、DetectionWorker硬触发溢出结构化结果；业务测试由117项增至121项，Qt Test预期汇总为`123 passed, 0 failed`。
- [x] 第二轮Agent静态门禁通过：121项测试声明/定义一一对应，Qt Test预期123项；新Presenter源/头均为全ASCII并在主工程、运行测试工程各唯一登记；真实Fault入口7处仅覆盖PLC断连/0或49写失败/复位失败、500ms连接检查及两类硬触发提交，普通存图失败入口Fault调用0处；软触发继续走阻塞提交，硬触发两类入口唯一走非阻塞提交；值0、49和`timer->start(100)`各保留唯一1处；十项影响功能唯一且均为迁移中；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] Stage 4第二轮自动测试门禁：2026-08-15用户确认`detection_completion_test`为`123 passed, 0 failed`。当前无法连接PLC，故真实断线、OK写0失败、NG写49失败、100ms复位0失败、持续红色告警和人工恢复均记录为延期，不能冒充现场通过；用户明确授权在保留该风险的前提下进入第三轮。
- [x] Stage 4第二轮已提交为`40fbf32`（`refactor(runtime): 接入生产Fault故障源`）；提交保留真实PLC异常链延期说明，未将自动合同测试记作现场通过。

## Stage 4第三轮：Fault前产品唯一收口与最终清理

- [x] 开始Stage 4第三轮；影响`UI-002、UI-005、DET-008、RUN-001..004、PLC-005..007、RES-001`十一项。五模式算法结论、普通产品OK/NG统计、正常PLC值0/49、约100ms脉冲、相机与存图路径均不修改。
- [x] 新增全ASCII `runtime/inspection_product_reconciler.*`：每次运行只保存Fault前已受理产品的`ProductKey`及Accepted/AlgorithmCompleted状态，不保存图像；正常正式记录或Fault收口后删除对应身份，重复收口和跨运行身份被拒绝。
- [x] 收口策略固定为：仅当恰有一个尚无算法结论的产品且PLC可写时，允许请求一次49→约100ms→0兜底NG；已有算法结论、多件身份无法唯一确认或PLC不可写时只记未确认，不覆盖算法OK/NG，也不把系统异常计入产品质量总数、NG或合格率。
- [x] 正常当前结果和延迟NG的PLC请求均携带准确`ProductKey`；延迟队列保存原始NG身份，PLC写失败由运行控制器按产品去重登记未确认。Fault恢复先停止采集和Worker，再检查未收口账本；未收口或49后复位0失败时继续保持Fault，完成收口后才允许解除锁定。
- [x] 人工恢复路径复用当前PLC连接参数；唯一可兜底产品只有在实际49和0均成功后才记为已请求。49失败记未确认；49成功而0失败记未确认并保持Fault直至复位成功；恢复摘要分别显示兜底请求数和未确认数。真实PLC操作仍需现场补验。
- [x] 删除零引用的`InspectionRuntimeController::markFault`兼容别名，以及可绕过产品身份账本的控制器直接`recordCancelledProduct/recordUnconfirmedProduct`入口；现有内部异常统计能力保留，由产品身份收口结果驱动。
- [x] `detection_completion_test`新增7项业务合同，覆盖正常记录清账、单件兜底门禁、已有算法结论保护、多件零盲发、PLC失败去重未确认、Fault收口后图像引用释放和新运行空账本；业务测试由121项增至128项，Qt Test预期汇总为`130 passed, 0 failed`。
- [x] Stage 4第三轮Agent静态门禁通过：运行测试声明/定义各128项；新收口器在主工程和运行测试工程各唯一登记1次且非ASCII字节0；旧`markFault`和控制器直接异常计数入口引用均为0；十一项受影响功能唯一且均为迁移中，状态计数36/11/41/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序，等待用户Qt Creator集中门禁。
- [x] Stage 4第三轮Qt Creator集中门禁通过：2026-08-15用户确认`detection_completion_test`为`130 passed, 0 failed`，主程序在当前无PLC条件下的构建启动、模式切换、模板/配方选择和相机基本操作均无问题；十一项迁移中功能恢复为已验证，允许创建本地提交并完成Stage 4结构结项。
- [x] 真实PLC断线、OK写0失败、NG写49失败、100ms复位0失败、持续红色告警、产品收口摘要和人工恢复因当前没有可连接PLC继续延期；本轮自动合同和无PLC主程序门禁不替代现场验证，也不宣称输送线已停止或机械剔除已确认。
- [x] Stage 4第三轮已提交为`0d2811a`（`refactor(runtime): 收口Fault前产品`）；Stage 4约定结构开发结束，后续不再安排计划内开发轮次。

## 历史 Stage 4结项摘要（2026-08-15，Fault兜底结论已被取代）

- Fault策略：PLC断线、结果输出失败和硬触发FIFO满进入持续Fault，停止新正式受理；软件只声明视觉检测暂停和输送线状态未知。
- 当时的产品一致性合同：Fault前产品由`ProductKey`账本唯一收口；已有算法结论不被覆盖，唯一未结论产品才可请求一次兜底NG，其余记未确认且不进入产品合格率。该兜底合同已被2026-08-16用户决策取代，新策略一律不猜测补发NG。
- 生命周期：Fault后新帧不创建产品身份；队列、Worker和UI邮箱协作取消；收口账本不持有图像，恢复后新运行使用空账本。
- 门禁证据：三轮运行测试由119项增至123项再增至130项，用户均确认通过；第三轮无PLC主程序门禁无问题。
- 未关闭证据：真实PLC断线、0/49写失败、100ms复位失败、现场人工恢复、固定实际样本、P50/P95和连续运行仍待后续现场验收；这些事项不再扩展为重构开发轮次。

> 复核更正：本节只代表三轮Fault合同切片完成，不再作为Stage 4或整项重构结项依据。最终架构完成阶段已重新打开。

## 历史“最终架构完成”阶段：启动组装与设备运行边界（2026-08-15）

- [x] 复核最终架构完成条件：`Widget`仍直接构造Snap7/海康实现、持有PLC设备接口、编码DB字节并管理软硬采集线程；功能表仍有36项`已基线`。据此撤销`8eadcae`文档中的过早结项结论，不回退此前已经验证的代码切片。
- [x] 本切片受影响功能限定为`SYS-001、SYS-006、SYS-008..009、SET-005、CAM-001、PLC-001..007、TOOL-002`，全部先转为`迁移中`；检测算法、触发参数、PLC地址、0/49值、约100ms脉冲、提示和保存时机不改。
- [x] 新增`runtime/inspection_plc_controller.*`：独占`IPlcDevice`，统一连接/断开、DB1.DBB1032触发模式、DB1.DBW980剔除时间、DB1.DBD920剔除距离、DB1.DBW982拍照时间、DB1.DBD924拍照距离及DB1.DBB1033结果值的类型化命令和大端编码。
- [x] `InspectionRuntimeController`成为PLC命令唯一运行入口；`Widget`不再包含`IPlcDevice`成员、不再调用`writeDbArea`或编码PLC字节，现有UI槽只读取界面参数并调用类型化运行命令。
- [x] 具体`HikvisionCameraDevice`、`Snap7PlcDevice`和`BarcodeDecoderAdapter`改由`main.cpp`启动组装后注入；`PaddleOcrEngine`由`main.cpp`提供具体工厂并在Widget原初始化位置创建，以保持模型日志/初始化顺序；`widget.cpp`不再包含这些具体实现头。
- [x] 将`IBarcodeDecoder`从具体适配器头拆到`devices/barcode/barcode_decoder.h`；Pipeline、Worker工厂和Widget只依赖接口，具体DLL适配器仅由启动入口和适配器自身测试包含。
- [x] `main`不再以`exit(result)`绕过栈对象析构，改为从事件循环正常返回；关闭主窗后Widget、运行控制器和启动组合按C++生命周期释放，原`closeEvent`协作停止路径保留。
- [x] `detection_completion_test`增加6项Fake PLC合同，覆盖无设备拒绝、运行控制器连接边界、触发模式0/1、四项工艺参数写入顺序与大端编码、首个失败即停止、结果和拍照距离固定地址。
- [x] Agent静态门禁通过：运行测试声明/定义各134项，Qt Test预期`136 passed, 0 failed`；三个新头/源均为全ASCII并在相应qmake工程唯一登记；Widget内具体海康/Snap7/Paddle/二维码适配器、`IPlcDevice`、`writeDbArea`和PLC编码引用均为0；具体实现只在`main`启动组合；14项受影响功能唯一且均为`迁移中`，状态计数28/14/46/2；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 2026-08-15用户确认Qt Creator集中门禁通过：`detection_completion_test`为`136 passed, 0 failed`，`barcode_decoder_adapter_test`为`8 passed, 0 failed`，`barcode_word_detection_pipeline_test`为`10 passed, 0 failed`；主工程完成Run qmake、Rebuild、Run，无PLC连接失败不影响主窗口，相机可打开，任一软触发模式可启动/检测/停止，二维码+三期仍可读码，深度模型可启动检测，关闭主窗口后进程正常退出。
- [x] 14项受影响功能由`迁移中`恢复为`已验证`，正式功能状态更新为已基线28、迁移中0、已验证60、已延期2；真实PLC断线、写入失败、复位和人工恢复仍按原记录延期，当前无PLC门禁不冒充现场验证。
- [x] 指定会话`01a00464-5a6e-7e60-8446-b3fa66c75e45`中的存图调整已完成审计并独立提交为`346bae6`（`feat(runtime): 统一检测图片保存为JPEG 92`）；五模式仅统一生产图片格式和编码质量，不改变保存范围、目录、命名、原图/标注图组合或失败报警。
- [x] 启动组装与设备运行边界切片已提交为`4402fe1`（`refactor(runtime): 收口启动组装与PLC设备边界`）；提交后正式功能状态为已基线28、迁移中0、已验证60、已延期2。

## 历史“最终架构完成”阶段：固定四轮收口计划（2026-08-15）

- [x] 2026-08-15用户确认剩余架构开发固定为四轮；此前的`4402fe1`不计入四轮。编译修复、运行修复、接入后零引用清理和同轮复测均属于当前轮，不以任何名称增加第五轮。
- [x] 28项剩余基线已完整路由：第1轮`DET-001、RUN-006`；第2轮`SYS-007、UI-007..009、SET-001..002、SET-006..007、SET-011..013`；第3轮`UI-006、TPL-004、TPL-006..008、TPL-012、TPL-014..015`；第4轮`SYS-002..005、SYS-010、MC-001、TOOL-001`。实际触及的已验证功能仍必须加入对应轮次门禁。
- [x] 第1轮：运行线程、采集所有权和检测结果收尾。代码、Agent静态门禁和用户Qt Creator集中门禁均已通过，28项功能恢复`已验证`，已提交为`16bafa4`。
- [x] 第2轮：相机操作与整机设置业务。代码、旧实现清理、Agent静态门禁和用户Qt Creator集中门禁均已通过；22项实际受影响功能恢复`已验证`，允许独立提交并进入第3轮。
- [x] 第3轮：模板制作与编辑流程。模板ROI/引导、二维码验证、钢印环、字符分割、多Profile和单个/批量编辑已迁入`TemplateEditorController`，Widget净减3922非空行；自动测试和主程序集中门禁均通过。
- [x] 第4轮：启动系统、独立工具、零引用清理和最终验收。收口启动保护与system_support，保留验证多相机入口和LicenseTool，清理全部已替代旧路径；最终Widget为3437非空行，功能表已基线0、迁移中0。
- [x] 四轮共同门禁固定：每轮先列受影响功能ID并追踪原调用链；同轮完成新路径接入和旧路径清理；Agent只做静态检查；用户Qt Creator集中验证通过后回填功能状态并独立提交；上一轮未通过不得开始下一轮。

## 固定四轮第1轮：运行线程、采集所有权和检测结果收尾

- [x] 从真实入口复核影响范围。最低关闭项为`DET-001、RUN-006`；实际所有权和回调迁移同时影响`CAM-001..006、RUN-001..005、RES-001..005、SAVE-001..005、UI-001..005`，共28项，均已在功能对照表临时标为`迁移中`。
- [x] 新增`InspectionAcquisitionController`，独占单相机接口、`MyThread`、`CameraThread`和共享采集图像缓冲，统一软/硬采集Worker创建、配置、信号接线、模板预览、协作停止、重建、相机恢复与退出清理。软触发继续阻塞提交，硬触发继续使用容量1 FIFO非阻塞提交并在满时进入既有Fault合同。
- [x] 新增`InspectionResultCoordinator`，独占`DetectionResultPresenter`、容量32且2线程的`ImageSaveService`和`DetectionCompletionController`，统一五模式Worker创建、完成去重、同一`ProductKey`呈现快照、统计、存图决策和PLC请求顺序；五种算法、阈值和模式索引未修改。
- [x] 新增`InspectionRuntimeUiCoordinator`，接管运行期按钮使能/样式、Fault显示与恢复确认、ROI警告、存图失败合并提示和Presenter的界面绑定；`Widget`保留Qt槽、控件取值与命令转发。
- [x] 删除`Widget`中的相机/软硬线程/存图服务/完成控制器/Presenter所有权、五模式完成与收尾实现、Worker信号接线和线程重建编排；保留旧`jiancestring`到`TemplateMatch`的唯一兼容接线。以提交`4402fe1`的`app/widget.cpp`为基准，统一按非空物理行统计为11273行，当前9748行，净移除1525行，达到本轮不少于约1500行的硬门槛。
- [x] `detection_completion_test`新增3项协调器合同：纸巾完整呈现与副作用、重复完成不重复计数/存图/PLC、协调器可创建并启动纸巾Worker；业务测试声明/定义各137项，Qt Test预期汇总`139 passed, 0 failed`。
- [x] 首次Qt Creator测试得到`137 passed, 2 failed`：两个结果协调器用例在未启动DetectionWorker时直接模拟完成回调，容量1 UI完成邮箱仍处于取消态，因而统计保持0。生产启动路径始终先启动Worker并重开邮箱，不受此测试前置条件遗漏影响；本轮仅为两个测试补齐`Starting→启动空闲Worker/重开邮箱→Running`的真实顺序，未修改生产代码，等待同一门禁复测。
- [x] 主工程首次复编译报告`widget.cpp:1237 C2447`；静态对照确认清理相邻旧函数时只删掉了自由函数`QImage2cvMat(QImage)`标题而遗留函数体。先恢复标题定位边界，再经全工程引用检查确认该自由函数仅有定义、调用为0，最终完整删除函数而非保留孤立函数体；未修改任何仍在使用的图像转换路径。
- [x] 首次主程序运行门禁暴露条件分支回归：软触发、未启用PLC时，一次成功检测仍会请求PLC OK/NG输出，且500ms运行健康检查也会把未连接PLC判为系统Fault。已在`InspectionResultRunConfiguration`保存本次运行的PLC启用快照，结果协调器仅在快照启用时转发PLC请求，运行健康检查复用同一快照；硬触发/PLC运行的0/49、约100ms复位、断线Fault和人工恢复合同不变。现有两个协调器测试分别固定为“软触发零PLC请求”和“启用PLC仅一次请求”，测试总数不增加。
- [x] 第二次Qt Creator测试仍为`137 passed, 2 failed`且耗时37956ms：两个协调器用例已补齐Worker启动，但测试入口仍为`QTEST_APPLESS_MAIN`，不存在`QCoreApplication`事件循环，协调器投递的Qt队列完成任务无法执行，两个`QTRY_COMPARE`分别超时后统计保持0。测试入口已改为`QTEST_GUILESS_MAIN`，只提供核心事件循环而不创建图形界面；生产代码、测试数量和139项预期均不变，等待复测。
- [x] Agent静态门禁通过：6个新增源/头均为ASCII，在主工程各唯一登记1次；结果协调器在运行测试工程唯一登记；Widget不再持有旧线程、相机、存图、完成控制器或Presenter成员；90项正式功能当前为已基线26、迁移中28、已验证34、已延期2；`Widget`净减1525非空行；`git diff --check`通过。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 2026-08-15用户确认第1轮Qt Creator集中门禁全部通过：`detection_completion_test`为`139 passed, 0 failed`；无PLC条件下主工程完成Run qmake/Rebuild/Run，二维码+三期软触发不会误入PLC Fault，字库和纸巾可启动/检测/停止，全部保存无跳图，停止后可再次启动并正常关闭进程。
- [x] 28项实际受影响功能恢复为`已验证`，正式状态为已基线26、迁移中0、已验证62、已延期2；真实PLC现场异常链继续沿用既有延期记录，不用无PLC验证冒充通过。第1轮允许创建独立本地提交并进入第2轮。
- [x] 第1轮已提交为`16bafa4`（`refactor(runtime): 收口采集与检测结果所有权`），提交后工作区干净。

## 固定四轮第2轮：相机操作与整机设置业务

- [x] 从真实入口复核影响范围：最低范围为`SYS-007、UI-007..009、SET-001..002、SET-006..007、SET-011..013`；实际还触及`UI-002、SET-004..005、SET-008..010、CAM-001..002、SAVE-001..003`，共22项，均已临时转为`迁移中`。
- [x] 新增`ui/controllers/machine_settings_page_controller.*`，接管全局设置控件绑定、即时/需应用设置同步、dirty星号、运行中使能、数值校验、防滚轮误改、软件数据目录双击、右侧分隔条记忆、读取/安全保存/清空和按硬件状态恢复默认；模板私有设置仍留在模板轮次处理。
- [x] 新增`runtime/inspection_camera_operations.*`，通过`ICameraDevice`统一曝光/增益范围、写入验证、保存曝光越界调整及首台相机打开顺序；`InspectionAcquisitionController`只委托这些业务，并继续持有相机与采集Worker生命周期。
- [x] 新增纯逻辑`system_support/machine_settings_policy.*`，固定相机开/关、PLC连/断四种状态下哪些默认值可立即恢复、哪些已应用硬件值必须保留待用户操作；当前无PLC条件只做合同测试，PLC连接态人工门禁继续记录为现场待验。
- [x] 删除`Widget`中全局设置binding映射、硬件依赖样式、dirty判断、默认合并、设置文件直接读写及相机曝光/增益实现；`Widget`只保留Qt槽、用户提示、模板私有设置和控制器命令转发。按第1轮提交统一统计，非空物理行由9748降至8807，净移除941行。
- [x] `detection_completion_test`新增7项合同，覆盖曝光/增益整数边界、越界拒绝、曝光回读、开机顺序与越界保存、曝光失败关闭相机，以及断开/连接硬件时的默认恢复策略；业务测试由137项增至144项，Qt Test预期汇总`146 passed, 0 failed`。
- [x] 第2轮Qt Creator集中门禁：2026-08-15用户确认`detection_completion_test`为`146 passed, 0 failed`；主工程Run qmake/Rebuild/Run及相机参数、设置读取/保存/默认/清空、目录/滚轮/分隔条、存图策略、停止重启和正常退出均无问题。当前无PLC，PLC连接态人工门禁继续按现场待验记录保留。
- [x] 第2轮22项实际受影响功能恢复为`已验证`，正式状态为已基线15、迁移中0、已验证73、已延期2；允许创建独立本地提交并进入第3轮。

## 固定四轮第3轮：模板制作与编辑流程

- [x] 从真实入口复核本轮影响范围。最低关闭项为`UI-006、TPL-004、TPL-006..008、TPL-012、TPL-014..015`；模板预览/冻结、模式切换、模板私有dirty、ROI画布、Recipe发布选择和运行资源安装属于同一可观察工作流，实际门禁扩展为`UI-001..002、UI-006、SET-003..004、TPL-001..016`，共21项，已临时转为`迁移中`。
- [x] 新增`ui/controllers/template_editor_controller.*`并接入主窗口，接管模式化引导、二维码即时验证、字符资产工作区、多Profile、旧模板装配/已发布配方选择、发布与同UUID重发、按模式记忆、单个/批量目标字符和阈值编辑；旧选择目录对话框、模板保存画布交互和运行资源继续由`Widget`薄接入，不改用户入口。
- [x] 删除`Widget`中模板会话、按模式记忆、动态编辑控件、二维码模板校验和当前编辑状态所有权；旧字段在`Widget`源码/头文件中均为0处，控制器头/源声明定义各98个方法且主工程清单各唯一登记1次。按第2轮提交统一统计，`Widget`非空物理行由8807降至4885，净移除3922行，超过本轮约2500至3500行目标；五种算法、阈值语义、PLC和存图链未修改。
- [x] 第3轮Agent静态门禁通过：新控制器花括号665/665，`Widget`花括号693/693；新中文源码已按MSVC2017现有约定保存为UTF-8 BOM；21项影响功能唯一且均保持`迁移中`；`git diff --check`通过。现有测试声明数未修改，Qt Test预期仍为`detection_completion_test 146 passed`、`recipe_store_test 31 passed`、`product_recipe_test 10 passed`、`barcode_decoder_adapter_test 8 passed`、`barcode_word_detection_pipeline_test 10 passed`，均应为0 failed。Agent未执行qmake、构建、链接、测试或主程序。
- [x] 第3轮首次Qt Creator复编译修复：迁出后的控制器仍有6处使用`Widget`私有别名`OperationState`时遗漏`Widget::`限定，导致MSVC把`Detecting/Stopping`级联报告为未声明；现已全部改为`Widget::OperationState`。同时将控制器被警告的中文字符串改为Unicode转义，并为既有`imagelabel.h`补UTF-8 BOM、移除注释中的Emoji，消除C4819编码告警；业务分支和运行语义未改，等待同轮复编译。
- [x] 第3轮Qt Creator自动测试门禁通过：2026-08-15用户确认`detection_completion_test`为`146 passed, 0 failed`（修复后重复运行两次均通过），`recipe_store_test`为`31 passed, 0 failed`，`product_recipe_test`为`10 passed, 0 failed`，`barcode_decoder_adapter_test`为`8 passed, 0 failed`，`barcode_word_detection_pipeline_test`为`10 passed, 0 failed`。尚缺主程序模板制作/编辑、发布选择重发和跨模式恢复的人工门禁，21项功能继续保持`迁移中`。
- [x] 2026-08-15用户确认第3轮Qt Creator主程序集中门禁通过：字库、二维码+三期、钢印和深度模型的旧模板/已发布配方、字符分割、多Profile、单个/批量编辑、同UUID重发和跨模式恢复均无问题；纸巾无模板可启动，各代表模式启停及程序退出正常。
- [x] 第3轮21项实际受影响功能恢复为`已验证`，正式状态为已基线7、迁移中0、已验证81、已延期2；允许创建独立本地提交并进入固定四轮第4轮。
- [x] 第3轮已提交为`e5a73c6`（`refactor(ui): 收口模板制作与编辑流程`），提交后工作区干净。

## 固定四轮第4轮：启动系统、独立工具、零引用清理和最终验收

- [x] 从真实入口复核本轮影响范围：最低`SYS-002..005、SYS-010、MC-001、TOOL-001`，启动设备组装、OCR/二维码具体实现创建和正常退出还实际触及`SYS-001、SYS-006、SYS-008..009、TOOL-002`，共12项，已临时转为`迁移中`。
- [x] 最终零引用清理继续迁出`Widget`中的启动/停止命令以及旧模板选择/保存实现，附加影响`UI-001..002、UI-006、SET-003..004、TPL-001..016、CAM-003..005、RUN-001..003`共27项；与启动系统12项合计39项，全部等待同一轮最终门禁。
- [x] 收口授权、单实例、日志、崩溃记录、Release部署和启动对象组装：新增`startup/ApplicationStartup`、`RuntimeGuard`、`SingleInstanceGuard`及`system_support/license|logging|crash|deployment`；具体设备仍按原顺序组装，授权启动/24小时检查、共享内存键`ecust`、3个月日志保留、Windows异常记录和正常栈析构语义保留。旧根目录`main.cpp`、`RuntimeGuard.*`、`CCrashStack.h/ccrashstack.cpp`和`deploy_runtime.ps1`已删除，生产引用为0。
- [x] LicenseTool改为直接编译共享`LicenseCodec`，不再复制密钥、XOR/SHA256、INI解析和QSaveFile逻辑；新增`system_support_test`7项业务合同，Qt Test预期`9 passed, 0 failed`，覆盖写读、过期仍可解析、固定旧密文兼容、缺失/损坏/无效日期和同键第二实例拒绝。
- [x] 最终清理不只拆启动文件：新增`InspectionStartController`和`InspectionStopController`完整接管原启动/停止按钮命令；`TemplateEditorController`继续接管旧模板多目录选择、模板保存、坐标转换和钢印ROI标定。`Widget`四个入口均成为单行转发，非空物理行由第3轮4885降至3437，处于最终2500至4000行目标内。
- [x] 迁移等价性静态对照通过：启动、停止、旧模板选择、模板保存四段函数在去除类限定和新增空指针保护后，与`e5a73c6`原实现规范化文本完全一致；部署脚本新旧Git对象哈希同为`fa7d30e47c45820d50c2eaa69d4e588e061e017b`；多相机源码无改动。
- [x] 第4轮Agent静态门禁通过：正式功能状态为0/39/49/2；`Widget`、模板控制器、启动/停止控制器花括号分别498/498、781/781、61/61、24/24；新启动/system_support头源和两运行命令控制器在主工程各唯一登记1次，旧路径和旧授权/日志/崩溃符号生产引用0；`system_support_test`声明/定义各7项；`git diff --check`通过。Agent未执行qmake、构建、链接、测试目标、LicenseTool或主程序。
- [x] 第4轮首次Qt Creator复编译修复：`InspectionStartController`迁出后有13处访问`Widget`私有嵌套状态时遗漏`Widget::`类型限定，另有2个相机曝光回调错误捕获了成员名`m_host`而非控制器`this`，造成MSVC级联报告状态枚举未声明和Lambda捕获非法；现已统一修正，启动顺序、参数、失败提示及相机调用语义均未改变，等待同轮复编译。
- [x] 第4轮Qt Creator复编译门禁通过：用户确认修复后执行Run qmake与Rebuild均无问题；本条仅证明工程可完整构建，39项功能仍等待自动测试与最终人工回归后统一回填。
- [x] 2026-08-15用户确认第4轮最终Qt Creator集中验收通过：`tests/tests.pro`全部测试子项目均为0 failed；主工程Release完成启动保护、相机打开、五模式启动/停止、二维码读码、深度模型、无PLC主窗可用和正常关闭退出回归。第4轮39项功能统一回填为`已验证`，功能表达到已基线0、迁移中0、已验证88、已延期2；允许创建第4轮本地收口提交并声明本轮重构完成。
- [x] 第4轮最终静态收口通过：功能表99项中已验证88、已延期2、已基线/迁移中均为0；`widget.cpp`为3437非空行；旧根目录启动/授权/崩溃/部署文件均不存在且生产引用为0；全仓`QThread::terminate()`调用为0；新增启动、system_support和运行控制器文件均在工程清单中唯一登记；`git diff --check`通过。Agent未执行任何构建、测试或主程序。

## 新架构完全替换阶段：阶段 0 治理文档（2026-08-16）

### 决策与覆盖关系

- [x] 用户确认上一轮“固定四轮完成”只是历史计划完成，不是终局架构完成；当前代码仍是模块化外壳包住旧核心，必须继续执行彻底替换。
- [x] 用户确认删除全部多相机功能，包括 `MC-001` 窗口入口、`MC-002` 占位控件和 `MC-003` 无入口底层 API；不再保留或延期，也不建立后续多相机接入任务。
- [x] 用户确认简化 Fault：停止受理新正式产品、保留已有算法结论、未完成产品记 `Unconfirmed`，不再对唯一未结论产品猜测性补发 49。
- [x] 用户确认正常 PLC 合同不变：OK 写 0；NG 写 49，约 100 ms 后写 0。
- [x] 用户确认除多相机和 Fault 兜底策略外，其余 87 项用户可达功能、五种算法、阈值、模型、统计口径、界面外观、按钮入口、提示语义和操作流程全部保持。
- [x] 用户确认不兼容旧模板、旧设置和旧目录，不提供运行时兼容或离线转换工具。
- [x] 用户确认继续使用 Qt 5.14、qmake、MSVC2017、C++11；真实相机用于回归，PLC 仅做 Fake 合同验证，真实 PLC、机械剔除和现场恢复继续标为现场待验。

上述决策覆盖主计划和历史执行记录中的“多相机保留/延期”“唯一未结论产品可兜底 NG”以及“固定四轮达到终局架构”结论。历史代码变更、测试结果和用户门禁证据继续保留，不因治理结论更新而作废。

### 本阶段范围与结果

- [x] 新建 `docs/development/OCRGangYin新架构完全替换执行方案.md`，记录终局目录、依赖规则、核心接口、简化 Fault、阶段 0～7、功能 ID 映射、旧类型删除清单、轮末集中门禁和最终完成定义。
- [x] 更新 `docs/development/OCRGangYin工业视觉框架升级计划.md`，将新方案设为当前终局依据；旧五阶段和固定四轮改为历史里程碑；删除仍作为当前要求的多相机保留/延期和猜测性兜底 NG 结论。
- [x] 更新 `docs/development/OCRGangYin现有功能对照表.md`：`MC-001..003` 改为 `已确认删除`；正式状态更新为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3；新增 Fault 行为变化 `DIFF-010`。
- [x] Fault 计划影响范围记录为 `UI-002、UI-005、RUN-001..004、PLC-005..007、RES-001`；本阶段没有修改生产代码，这些功能继续保留 `已验证` 基线，进入阶段4时再按真实改动范围转为 `迁移中`。
- [x] 阶段 1～7 的最低功能 ID、删除对象和轮末集中门禁均已写入新方案；每阶段仍须开始前重新追踪真实调用链，最低范围不能替代实际影响范围。
- [x] 本阶段只修改四份治理文档，不修改 `app/`、测试、`.pro/.pri` 或资源文件，不需要用户执行 Qt Creator 构建。

### 仓库核对与文件保护

- [x] 阶段 0 开始时分支为 `codex/ocrgangyin-refactor`，HEAD 为 `8595cb2 refactor: complete stage 4 architecture closure`。
- [x] 2026-08-16 实际执行 `git status --short --untracked-files=all` 无输出，工作区干净；交接基准中提到的 `app.zip` 当前不存在。Agent 未创建、修改、移动或删除该文件；若后续重新出现，继续作为用户文件排除在修改和提交之外。
- [x] 阶段 0 最终静态检查：现行多相机/Fault结论一致；功能表90个正式功能ID中已验证87、已确认删除3，其他状态均为0；`git diff --check`通过，工作区差异只包含四份治理文档。

### 固定轮次与统一验证补充决策

- [x] 2026-08-16用户要求每个阶段固定一轮；阶段0已经完成，剩余阶段1～7固定为7轮。
- [x] 每轮由Agent一次完成本阶段新实现、唯一正式路径接入、全部计划内旧路径删除、测试源码、工程清单和静态检查，中途不要求用户构建中间版本。
- [x] 用户只在每轮末对最终差异集中执行一次Run qmake、Rebuild、相关测试和主程序人工回归；该门禁同时验证接入和删除结果。
- [x] 集中验证失败后的诊断、修复和复验继续属于原轮次，不新建额外阶段或轮次；门禁通过前不得进入下一轮。

### 组合根、自动提交与Schema冻结补充决策

- [x] 2026-08-16用户确认 `startup` 负责进程初始化和完整对象图组装，包括 vendor 设备、Store、Runtime、应用服务、页面和主窗口；它是具体设备实现唯一构造位置，但不得包含业务逻辑。
- [x] 2026-08-16用户授权每阶段门禁通过后由Agent自动创建一个本地提交，不再逐阶段询问提交授权；每阶段只提交本阶段精确差异，禁止自动推送、合并、变基或混入用户文件。
- [x] 阶段0是纯文档阶段，静态门禁已经通过；本次规则补充完成并复核后，自动创建阶段0本地提交。
- [x] 阶段1写生产代码前必须先新增并冻结 `docs/development/OCRGangYin新架构数据Schema.md`，逐字段确定 `MachineSettings`、五模式`ProductRecipe/Profile`、`PreparedRecipe`和资源清单；真实代码与已确认行为不能消除的冲突必须询问用户，不得猜默认值。

### 阶段0完成时的下一阶段

- [x] 阶段1“设置与配方成为唯一数据源”已于2026-08-16在阶段0提交`4b00646`之后开始。
- [x] 阶段0静态核对与提交完成前没有修改阶段1生产代码。

## 新架构完全替换阶段：阶段 1 设置与配方唯一数据源（2026-08-16～2026-08-17，已完成）

### 调用链影响与Schema冻结

- [x] 开始时复核分支`codex/ocrgangyin-refactor`、HEAD `4b006464b62d612a7c381f5fb7895ac768684cee`和干净工作区；`app.zip`不存在。阶段1期间持续检查该文件仍不存在，未创建、修改、移动、删除、暂存或提交用户文件。
- [x] 在生产代码修改前从当前源码、UI、旧INI键、五种模式消费者、功能表和计划反向冻结`docs/development/OCRGangYin新架构数据Schema.md`，覆盖AppData根、MachineSettings、五种类型化ProductRecipe、单/多Profile、ROI/字符框/二维码参数、图片/YAML资源、PreparedRecipe、编辑会话、事务保存、拒绝合同和错误码。
- [x] 真实调用链影响范围最终确认为50项：`SYS-001、SYS-007..008、UI-001、UI-008..009、SET-001..013、TPL-003..016、DET-002..006、CAM-001、CAM-003..004、CAM-006、RUN-001、RUN-005、PLC-001、PLC-003..004、SAVE-001..003`；功能表保持`迁移中50、已验证37、已确认删除3`，只有用户集中门禁通过后才恢复状态。

### 唯一设置路径

- [x] 新增`MachineSettings`和`MachineSettingsStore`。新设置只读写`<AppData>/settings/app_settings.json`，首启返回唯一默认对象；保存采用同目录临时文件、重读比较、备份改名、提交和失败回滚；损坏JSON、未知/缺失字段、非法范围、固定PLC地址变化和旧UI枚举ID均明确拒绝，不读取旧INI或旧目录。
- [x] 新JSON使用`stamp/word/ocr/tissue/barcodeWord`、`none/ngOnly/okOnly/all`等Schema稳定枚举值；旧`word_detection`、`save_all`等只留在Qt页面适配内存中，不能进入新JSON。相机曝光800、增益1、检测间隔300、JPEG质量92等默认只由`MachineSettings`构造函数提供。
- [x] `ApplicationStartup`成为本阶段唯一加载点：一次取得AppData根并加载设置快照，损坏即阻止启动；随后在组合根构造`MachineSettingsStore`、`RecipeStore`、vendor相机/PLC/OCR/读码器和主窗。主窗与设置页面共享同一个Store和启动快照，不再二次加载。
- [x] 机器设置页面的保存、恢复默认和清空全部委托新Store；清空只删除新设置JSON，不删除配方、生产图片、授权或日志。相机曝光/增益、PLC连接/地址/工艺参数、存图策略和UI布局都来自MachineSettings快照；纸巾阈值不再属于机器设置。

### 唯一配方与运行资产路径

- [x] 收口`ProductRecipe`为五种DetectionMode的严格Schema：Stamp/Ocr单Profile，Word/BarcodeWord多Profile，Tissue零Profile零资源；跟踪ROI、字符来源尺寸/字符框、目标文本、阈值和二维码参数类型化；资源只能使用`assets/`内相对路径，同一资源不得承担多个Profile角色。
- [x] 二维码formatMask=1、padding=8%、预算60ms、fallback=true的唯一代码默认移到`contracts/barcode_parameter_defaults.h`；产品配方和设备读码选项只引用该合同。图像阈值唯一默认70来自`RecipeProfile`，纸巾唯一默认6.0来自`TissueRecipeParameters`。
- [x] 新增不可变`PreparedRecipeSnapshot`，完整读取并解码所有已声明的tracking/raw/stampRing/字符图片，解析`calibrate_config.yaml`及日期/二维码/钢印多边形并执行已保存资产交叉约束；资源缺失、空文件、坏图、坏YAML、越界路径和已声明字符数据内部不完整均返回结构化错误。目标字符尚未确认、整组字符尚未切割或目标覆盖不足形成可继续编辑但不可启动的快照，由启动预检拒绝；运行时不再引用外部绝对资源路径。
- [x] `RecipeStore`只接受规范小写UUID目录。保存先写同级`<uuid>.tmp.<transaction-uuid>`，复制完整资源并从临时目录重载、比较、解码和准备，通过后备份正式目录并改名提交；提交失败恢复上一完整目录。非UUID旧模板目录不进入目录列表，显式请求返回`RECIPE_LEGACY_FORMAT_REJECTED`；UUID目录内只有旧INI时按缺少`recipe.json`拒绝。
- [x] 新增统一`RecipeEditorSession`，负责New/Editing状态、稳定recipeId、草稿、资源源路径、事务发布和UUID工作区；重置/析构只清理当前会话工作区，不修改正式配方。模板编辑入口继续保持原按钮、画布和提示流程，但创建、加载、编辑、字符资产回存、同UUID重发及纸巾阈值保存全部走新Session/Store。
- [x] 五模式启动预检只接受当前模式的PreparedRecipe；模式索引、相机/PLC/存图/变换/间隔来自已应用MachineSettings，跟踪图、ROI、YAML多边形、字符图、钢印环和纸巾阈值来自本次PreparedRecipe。启动链不再从UI控件、旧模板目录或检测器路径加载函数补资源。

### 旧路径删除与工程清单

- [x] 删除`AppSettingsManager`、`GlobalSettings`、`TemplatePrivateSettings`及其旧INI读写、旧设置兼容、模板父目录/按模式路径记忆、检测器按目录加载和旧模板私有设置调用；生产代码中三类旧符号与`settings.ini/app_settings.appset/QSettings`引用均为0（授权`license.ini`不属于本阶段旧设置）。
- [x] 删除旧配方并行链：`recipe_selection.*`、`template_profile_assets.*`、`template_character_asset_workspace.*`、`template_profile_load_plan.*`、`template_profile_mapper.*`、`template_recipe_assembler.*`、`template_recipe_draft_session.*`、`template_recipe_edit_session.*`、`template_recipe_workflow.*`、`template_recipe_publisher.*`及旧`recipes/template_runtime_profile.h`；新的UI缓存头位于`runtime/template_runtime_profile.h`且不含路径/持久化字段。
- [x] `app/recipes`不包含Detection实现或旧设置管理器；主工程和三个受影响测试工程已删除全部旧项并登记MachineSettings、PreparedRecipe、RecipeEditorSession、共享二维码默认合同及新runtime头。工程清单逐项解析结果为文件存在、大小写匹配、无重复登记。

### 测试源码与Agent静态门禁

- [x] 重写`product_recipe_test`为7项业务测试，覆盖MachineSettingsStore首启、保存重载、恢复默认、清空、损坏/旧INI/旧枚举拒绝、五模式JSON往返、单/多Profile、纸巾零模板和旧Recipe形状拒绝。
- [x] 重写`recipe_store_test`为7项业务测试，覆盖五模式整目录保存/加载/准备、多Profile、只读PreparedRecipe、RecipeEditorSession与工作区清理、提交失败回滚、资源缺失/损坏/非法路径、旧模板目录拒绝和五模式启动资源预检；OpenCV依赖及运行库清单已加入测试工程。
- [x] 更新`detection_completion_test`以使用MachineSettings和Prepared配方合同，删除旧运行设置字符串解析用例；声明/定义静态核对为140/140。Agent只修改和检查测试源码，未运行任何测试目标。
- [x] Agent静态门禁通过：主工程及三个测试工程清单解析通过；全仓本地quoted include和文件名大小写通过；MachineSettingsStore、RecipeStore、RecipeEditorSession、模板/设置控制器及PLC控制器声明/定义核对通过；Widget 27个自动连接槽声明/定义一致；产品/Store测试各7/7；旧工程项0引用；配方反向依赖0；运行启动参数只来自设置/Prepared快照；默认值唯一性搜索通过；功能表90行状态统计为50/37/3；`git diff --check`通过。
- [x] Agent未运行或间接触发qmake、nmake、jom、msbuild、cmake构建、Qt Creator构建、项目测试可执行文件、主程序或任何编译/链接/启动脚本。

### 当前门禁状态

- [x] 2026-08-17首次Qt Creator Rebuild在`machine_settings_store.cpp:48`因MSVC2017将无BOM UTF-8中文字符串按本机代码页解析而报C2001/C1907；已为本阶段4个严格UTF-8且含中文的编译单元统一补UTF-8 BOM，未改变提示文本或业务逻辑，并完成同类源码扫描。
- [x] 2026-08-17复验发现`product_recipe_test`因测试工程漏列`DetectionModes.cpp`而报LNK2019/LNK1120，并发现既有窄字符中文较多的`template_editor_controller.cpp`不适合补BOM、导致模板提示框乱码；已为`product_recipe_test`和同样引用MachineSettings的`detection_completion_test`补齐`DetectionModes.cpp/.h`，同时仅撤回模板控制器BOM。MachineSettingsStore、ApplicationStartup和产品配方测试中只使用`QStringLiteral`的中文编译单元继续保留BOM。
- [x] 2026-08-17测试复验中`recipe_store_test`为4通过/5失败，均由Windows下资产根前缀使用反斜杠、绝对资产路径使用正斜杠而将合法`assets/profiles/0/calibrate_config.yaml`误判为越界；已将PreparedRecipe的包含关系比较统一为清理后的正斜杠绝对路径，不放宽绝对路径、`..`或外部资源限制。同次`detection_completion_test`为141通过/1失败，失败夹具未给首个Profile设置被断言的`profile-a`名称；已补齐测试输入，不在生产构建器增加名称fallback。
- [x] 2026-08-17主程序复验显示撤回`template_editor_controller.cpp`的BOM后出现多处C2001，证明依赖自动编码识别无法同时保证编译与窄字符中文显示；已审计`app/`下186个C/C++源码和头文件均为严格UTF-8，并在主qmake工程的MSVC配置中显式加入`/utf-8`，同时固定源字符集和执行字符集。Qt 5.14、MSVC2017及C++11保持不变。
- [x] 2026-08-17人工回归发现“保存模板”被空目标字符阻断；用户确认模板制作不得受目标字符影响。复核阶段0基准代码后确认既有顺序是先保存基础模板，再询问是否立即切割字符模板，目标字符随后通过独立入口确认。现已删除基础保存的目标字符门禁和自动强制切割：基础几何/资源先事务发布，字符切割可立即执行或以后执行，目标字符可最后确认；`ProductRecipe`与`PreparedRecipe`允许这些明确的未完成编辑状态，同时仍解码并拒绝所有已声明的损坏资源，只有启动资源预检要求当前模式的目标字符及字符模板覆盖完整。Schema修订为1.1（JSON schemaVersion仍为1），产品配方、RecipeStore及启动预检测试源码同步覆盖“基础保存→可选切割→目标确认→允许启动”的边界。
- [x] 2026-08-17用户确认阶段1最终差异的统一Qt Creator门禁均无问题：已执行Run qmake、Rebuild、设置/配方相关测试和主程序人工回归，并覆盖新设置首启/保存/重启/恢复默认/清空、新配方创建/加载/编辑/损坏拒绝、五模式资源预检、事务失败保护、旧格式拒绝、相机曝光/增益、模板入口及基础模板保存不依赖目标字符的流程。
- [x] 阶段1实际受影响的50项保留功能全部由`迁移中`恢复为`已验证`；正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。当前无真实PLC，本轮PLC证据仍只限Fake合同，不冒充真实PLC、机械剔除或现场恢复验收。
- [x] 阶段1提交前最终静态门禁通过：90个正式功能ID唯一且状态为0/0/0/87/0/3；74个差异路径均属于阶段1且无构建产物或用户文件，`app.zip`不存在、暂存区为空；主工程新增/删除清单、三个测试工程、全仓本地include大小写、关键声明/定义、三组测试声明/定义（7/7、7/7、140/140）、严格UTF-8和花括号核对通过；生产代码中旧设置三类型、旧INI读写、recipes反向依赖、旧目标字符保存门禁及`PREPARE_TARGET_TEXT_MISSING`均为0；`git diff --check`通过。Agent未执行构建、测试或主程序。允许精确暂存并创建唯一阶段1本地提交；不推送，不进入阶段2。

## 新架构完全替换阶段：阶段 2 应用层与启停边界（2026-08-17，已完成）

### 开始基准与真实影响范围

- [x] 开始时复核分支`codex/ocrgangyin-refactor`、HEAD `b7bb3f4cb28a3203a22513e42cef824e00503f75 refactor(settings): 完成阶段1设置与配方唯一数据源`和干净工作区；`app.zip`不存在，阶段2未创建、修改、移动、删除、暂存或提交该用户文件。
- [x] 从启动按钮、停止按钮、相机开关、设置dirty、三类PLC入口、启动事务、采集线程结束回调、关闭事件、Qt信号槽和qmake清单重新追踪实际调用链；阶段2实际影响范围与最低范围一致，为12项：`RUN-001..003、UI-002、SET-004..005、CAM-001..002、PLC-001..004`。2026-08-17用户确认统一门禁通过后，状态恢复为`迁移中0、已验证87、已确认删除3`。

### 应用服务、命令与只读快照

- [x] 新增`InspectionApplicationService`、`SettingsApplicationService`、`ApplicationError/OperationResult`、`RuntimeSnapshot`和`InspectionRunContext`。`ApplicationStartup`只负责构造并注入Store、Runtime、应用服务、运行端口和主窗，不包含预检、运行决策、设备时序或UI用例规则。
- [x] `SettingsApplicationService`成为生产代码中`MachineSettingsStore`唯一写入者，统一持有current/draft并提供应用、丢弃、默认、清空和只读目录查询；设置页与模板工作区不再持有或写Store。
- [x] 启动访问门禁、检测模式解析、当前UUID配方重新加载/准备、五模式资源预检、Profile快照、运行ID与启动时间、MachineSettings和PreparedRecipe快照以及Start事务统一进入`InspectionApplicationService::start`。运行执行只使用本次`InspectionRunContext`快照，不在启动过程中回读可变UI草稿或模板目录。
- [x] `InspectionApplicationService::stop`统一处理正常/Fault停止请求、采集协作停止、检测Worker等待、相机恢复、故障产品收口和Runtime事务提交；UI只保留Fault确认框、中文提示、画布/结果显示清理。普通停止不会误执行Fault样式复位。
- [x] 相机打开/关闭、PLC连接/断开、触发模式、工艺参数和拍照距离均增加应用命令和结构化结果；保持“打开相机先尝试PLC、PLC失败不阻止相机”、固定首台相机、曝光调整事务保存、触发0/1和工艺参数固定地址/顺序等既有合同。
- [x] 检测运行状态只以`InspectionRuntimeController`为真源，UI通过`RuntimeSnapshot`信号/查询派生按钮状态；模板Preview/Frozen继续作为独立UI编辑状态，不再与第二份检测状态并存。

### 旧路径删除与工程清单

- [x] 删除`InspectionStartController`、`InspectionStopController`及对应两个`friend`，并从主工程清单移除；启动预检迁到`application/inspection_start_preflight.*`，现有两个使用该纯规则的测试工程同步更新唯一源文件路径。
- [x] 删除Widget中的`isCollecting`、`m_operationState`、`m_bOpenDevice`、`hasRunningInspectionThread`及采集控制器同名辅助API；删除停止路径中的`QCoreApplication::processEvents`兜底。相机打开状态和运行状态均由只读应用快照提供。
- [x] 主qmake工程登记全部应用层源码/头文件；`tests/runtime_tests`登记新的`application_service_test`子工程，测试工程显式使用MSVC `/utf-8`并复用既有Qt/OpenCV运行库部署脚本。旧Start/Stop和旧runtime预检工程项为0。

### 测试源码与当前门禁状态

- [x] 新增`application_service_test` 6项测试源码：设置草稿/应用/丢弃/默认/清空，五模式新配方启动-停止-重启，模板制作/相机/dirty/重复启动预检顺序，PLC触发未连接拒绝，应用层PLC连接/断开/触发/工艺参数/拍照距离Fake边界，以及PLC失败不阻止开相机和检测中拒绝关相机。
- [x] 既有`recipe_store_test`和`detection_completion_test`只把启动预检引用切换到application唯一位置，保留原业务断言；未引入旧实现副本或双路径。
- [x] 2026-08-17首次Qt Creator复编译在`inspection_run_configuration.h:29`报告`DetectionMode`未声明；根因是该枚举实际定义于`recipes/product_recipe.h`而非`DetectionModes.h`。未用补配方头制造runtime→recipes反向依赖，改为`InspectionRunConfiguration`只接受自身的`InspectionTrackingKind`和二维码运行标记，由`InspectionApplicationService`把稳定`DetectionMode`映射为运行计划；四处运行计划测试调用同步更新，五模式跟踪/软硬触发语义不变。
- [x] 2026-08-17首次运行`application_service_test`在进入任何测试前因缺少Qt `windows`平台插件退出；该测试不创建Widget、Pixmap、字体或GUI事件对象，问题来自误用`QTEST_MAIN`而构造`QApplication`。现改为与现有runtime/recipe测试一致的`QTEST_GUILESS_MAIN`，使用`QCoreApplication`运行，不通过额外复制`qwindows.dll`掩盖无GUI测试的错误依赖。
- [x] 2026-08-17主工程复编译在`widget.cpp:595`报告旧构造参数名`startupSettings`未声明；Widget已经改为接收设置应用服务，该处初始化页面改为读取`SettingsApplicationService::current()`的已加载只读快照。全仓同名搜索确认`startupSettings`只保留在`ApplicationStartup`组合根的加载局部变量中。
- [x] 阶段2最终Agent静态门禁通过：旧Start/Stop控制器、两个friend、三项重复状态、旧线程探测API、旧runtime预检路径和`processEvents`停止兜底在生产/测试中均为0引用；application无Widget/UI/对话框/vendor引用，runtime无application/UI反向依赖，recipes无application/detection/UI反向依赖；主工程新增/删除项、runtime测试子工程、本地include大小写、6/6应用服务测试声明定义和27/27 Widget自动槽均核对通过；正式状态统计为0/0/12/75/0/3，暂存区为空，`app.zip`不存在，`git diff --check`通过，差异只包含阶段2源码、测试、qmake清单、功能表和执行记录。
- [x] Agent只完成源码、测试源码、qmake清单和静态检查，未运行或间接触发qmake、nmake、jom、msbuild、cmake构建、Qt Creator构建、项目测试可执行文件、主程序或任何编译/链接/启动脚本。
- [x] 2026-08-17用户反馈阶段2Qt Creator统一门禁“都没问题”；本阶段12项功能全部恢复为`已验证`。真实PLC在线连接、现场读回和机械动作仍按功能表既有记录延期，未以Fake或无PLC结果冒充现场验收。
- [x] 阶段2提交前最终静态门禁通过：90个正式功能ID唯一且状态为`0/0/0/87/0/3`；37个差异路径全部命中阶段2白名单，暂存区为空且`app.zip`不存在；旧Start/Stop控制器、重复运行状态、旧线程探测、旧runtime预检和停止路径`processEvents`引用均为0；application、runtime、recipes依赖边界核对通过；主工程新增/删除项、runtime测试工程、应用服务测试声明/定义`6/6`和Widget自动槽声明/定义`27/27`一致；`git diff --check`通过。Agent未运行构建、测试可执行文件或主程序，允许精确暂存并创建唯一阶段2本地提交，不推送。

## 新架构完全替换阶段：阶段 3 相机、采集与多相机删除（2026-08-17，已完成）

### 开始基准与真实影响范围

- [x] 开始时复核分支`codex/ocrgangyin-refactor`、HEAD `41014c60686d9476e414e9ec67f1d7ae11b03505 refactor(application): 完成阶段2应用层与启停边界`和干净工作区；`app.zip`不存在，本阶段未创建、修改、移动、删除、暂存或提交该用户文件。
- [x] 从启动组合根、相机开关/参数按钮、模板实时预览、五模式启动、软硬采集、旋转/通道、单/多Profile定位、检测提交、停止/恢复/退出、Qt信号槽、隐藏多相机按钮和qmake清单双向追踪；实际影响33项：`SYS-009、UI-002..003、SET-006..010、TPL-001..002、DET-001..007、CAM-001..006、RUN-001..006、PLC-001、MC-001..003`。
- [x] `MC-001..003`继续保持用户确认的`已确认删除`；其余30项逐行改为`迁移中`。当前正式功能状态为待盘点0、已基线0、迁移中30、已验证57、已延期0、已确认删除3；用户集中门禁通过前不恢复状态，不进入阶段4。

### 唯一相机端口、Session与采集线程

- [x] 将`ICameraDevice`收口为`enumerate/openFirst/applySettings/setTriggerMode/startGrabbing/triggerSoftware/waitNextFrame/interruptWait/stopGrabbing/close`，帧等待只返回`FrameReady/Timeout/Interrupted/DeviceError`；设备接口不再暴露SDK节点、共享图像、ReadBuffer、非阻塞开关或主线程取图细节。
- [x] 新`devices/camera/vendor/HikvisionCameraDevice`直接调用MVS SDK，vendor内部独占设备句柄、回调、条件变量、像素转换、帧序号和图像所有权；MVS类型/函数只存在vendor目录。保持既有Mono8/10/12处理和其他格式BGR8转换，SDK回调只产生独立`CameraFrame`，转换/分配失败形成DeviceError并结束本次采集，不捕获全部异常后继续检测。
- [x] 新`CameraSession`成为相机打开、关闭、曝光/增益、模板预览、正式运行准备、停止与恢复的唯一所有者；`ApplicationStartup`只在组合根构造vendor设备、Session、Runtime、应用服务和Widget。Widget与模板控制器只调用应用服务，不持有设备或生产采集线程。
- [x] 软触发、硬触发和预览共用一个`CaptureWorker`及一个`std::thread`。软件模式按“上一帧提交完成→最小cameraDelay→软触发→等待新帧”串行；硬触发只等待下一帧且不增加cameraDelay；预览只发布相机会话ID/图像并限制一个待处理UI帧，不创建`ProductKey`或进入检测、统计、存图、PLC链。
- [x] 停止固定执行停止标志、`interruptWait()`和`join()`；自然结束的旧线程在下一次启动前同样先join。不存在detach、`QThread::terminate()`、超时放弃对象/缓冲区或故意泄漏。正式采集意外结束按阶段3前既有语义收口运行并恢复相机，不提前引入阶段4的新Fault结论。
- [x] 保持相机正常时序：打开时软件TriggerSource=7、曝光、TriggerDelay=0、回调/start；硬触发时stop、约200ms、TriggerMode/Line0、曝光/增益/TriggerDelay=0、回调/start、LineDebouncerTime=5000、约100ms；停止后按旧合同关闭、约100ms、重开软件触发并恢复保存曝光。曝光越界继续调整并事务保存；任一步骤失败直接返回结构化错误。

### 公共预处理、定位与正式运行接入

- [x] 新`FramePreprocessor`统一四种旋转和彩色/红/绿/蓝通道处理；MachineSettings只在应用服务建立运行快照时映射一次。旧软/硬线程中的两套旋转、通道和间隔逻辑已删除。
- [x] 将原根目录`TrackingPoseMatcher`按原-45..45度/2度步长、0.2金字塔和0.3阈值迁到`detection/positioning`；新`InspectionPositioner`统一WholeFrame、SingleTemplate和WordProfiles，多个Profile共享一次灰度/缩略预处理并通过既有`ProfilePoseSelector`选择结果。运行资产不完整直接拒绝准备，不跳过坏Profile或切换其他算法。
- [x] `InspectionApplicationService::start/stop/open/close/shutdown`直接编排CameraSession；运行执行继续把同一PreparedRecipe和MachineSettings快照交给阶段4前现有Runtime/五Pipeline/结果链。五种算法实现、阈值、正常统计、存图和PLC `OK=0/NG=49→约100ms→0`合同未改；本阶段没有实施阶段4简化Fault或结果链重写。
- [x] UI跨线程只接收应用服务的`cv::Mat`副本、`DetectionPose`、预览会话ID和采集状态信号；`InspectionFaultReason`补充Qt元类型登记以保证既有硬触发队列溢出信号可排队传递。模板冻结后只把克隆图像写回Session当前图查询，不恢复设备穿透。

### 旧路径、多相机与工程清单删除

- [x] 删除`MyThread`、`CameraThread`、`InspectionAcquisitionController`、`InspectionWorkerConfigurator`、`InspectionCameraStartTransition`、`InspectionCameraRecoveryTransition`、`InspectionAcquisitionStopCoordinator`、`InspectionCameraOperations`及对应头/实现/qmake项。
- [x] 删除根目录`CMvCamera`、`Zhuizong`、旧`TrackingPoseMatcher`和旧`devices/camera/HikvisionCameraDevice`函数表/Native桥；TemplateMatch不再构造旧采集线程，Widget删除相机句柄、旧采集成员、重复相机启动/恢复/停止函数和无调用相机信号。
- [x] 删除`MultiCameraWidget`、`MultiCameraController`、`MultiCameraUnit`、`MultiCameraSyncManager`、`IMultiCameraProvider`、`MultiCameraTypes`、隐藏`MultiCameraMode`按钮、独立UI文件及全部主工程项；不保留占位入口、兼容桥或未来扩建API。
- [x] 主qmake工程登记vendor、CameraSession、CaptureWorker、FramePreprocessor和positioning文件；runtime测试子工程用`camera_session_test`替换旧camera adapter目标，应用服务和检测完成测试同步删除旧Transition/Operations源码清单。所有受影响工程项存在、大小写一致且无重复登记。

### 测试源码与Agent静态门禁

- [x] 新`camera_session_test` 7项Fake测试源码覆盖软件触发新帧、硬触发不发送软件命令且忽略cameraDelay、预览帧不进入产品受理、cameraDelay 0/非0、等待中interrupt+join及重启、旋转/通道、单Profile/多Profile定位。
- [x] `application_service_test`保持五模式启动/停止/重启并使用真实CameraSession+Fake ICameraDevice，新增模板预览共享Session、曝光/增益查询设置以及PLC Fake连接下的硬触发应用入口；仍使用`QTEST_GUILESS_MAIN`，不会依赖Windows平台插件。当前声明/定义为7/7。
- [x] `detection_completion_test`删除只验证已移除Transition/Operations/StopCoordinator的14项旧测试及其大Fake，保留Runtime、五Pipeline、结果、正常PLC和Fault历史合同测试；声明/定义为126/126。Agent只编写测试源码和工程清单，没有运行测试目标。
- [x] Agent静态核对：CameraSession/CaptureWorker/vendor/application/Widget定义均有声明；Widget自动连接槽26/26；三组受影响测试7/7、7/7、126/126；`widget.ui` XML有效；主工程和测试工程文件存在、大小写一致、无重复项；Runtime/Detection/Device无Widget、Ui、QMessageBox依赖；MVS符号只在vendor；旧相机方法、旧线程/控制器和全部MultiCamera代码/UI/qmake生产引用均为0。
- [x] 功能表90个正式ID状态为迁移中30、已验证57、已确认删除3；差异只包含阶段3相机/采集/预处理/定位接入、计划内旧路径删除、测试/qmake、功能表和本执行记录；暂存区为空，`app.zip`不存在；tracked及新增文件空白检查和`git diff --check`通过。
- [x] Agent未运行或间接触发qmake、nmake、jom、msbuild、cmake构建、Qt Creator构建、任何项目测试可执行文件、主程序或编译/链接/启动脚本。

### 当前门禁状态

- [x] 2026-08-17阶段3首次Qt Creator Rebuild在`widget.h:405/426/446/448`报C2143/C4430/C2238；原因是4处历史裸`vector`声明曾偶然依赖已删除旧头文件导入`std`命名空间。现已全部改为显式`std::vector`，没有恢复旧include或全局`using namespace std`，不改变检测框、颜色表、字符模板和字符区域的数据类型或业务行为；全仓同类裸`vector`生产引用为0，`git diff --check`通过，等待同轮Rebuild复验。
- [x] 2026-08-17用户确认阶段3当前最终差异的集中门禁验证完成；30项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。真实PLC、机械剔除和现场恢复仍不得由Fake或无PLC结果冒充验收。
- [x] 用户在门禁后追溯提交`47e7115e7e5992267e348b2327a32c3076e5ff0b`和`e5a73c6f3171955f616ace544dfc0b98816f818a`的“相机延时(ms)”行为，确认历史UI `cameraDelay`是线程检测/循环节流而非SDK `TriggerDelay`；后者固定为0且没有UI入口。用户提出软件触发未来可改为“上一帧检测结束后立即取下一帧”，随后明确回退本轮尝试、要求以后再处理。该问题只登记为功能表`DIFF-011`，阶段3生产代码保持用户已验证版本，不纳入阶段4。
- [x] 阶段3门禁已通过，允许执行最终静态检查、精确暂存并自动创建唯一阶段3本地提交；不推送，提交完成后才进入阶段4。
- [x] 阶段3最终静态检查、精确暂存和缓存差异核对通过，已创建本地提交`6071787ad85e36269d595db98221855048a5864b refactor(camera): 完成阶段3相机与采集替换`；未推送，提交后工作区干净。

## 新架构完全替换阶段：阶段 4 统一Runtime、Pipeline与结果事务（2026-08-17，已完成）

### 开始基准与真实影响范围

- [x] 用户确认阶段3集中门禁已经完成，并要求把“软件触发不受相机延时影响”的讨论只记录为`DIFF-011`、以后再处理；阶段4未改变`cameraDelay`/SDK `TriggerDelay`行为。开始时分支为`codex/ocrgangyin-refactor`，HEAD为`6071787ad85e36269d595db98221855048a5864b refactor(camera): 完成阶段3相机与采集替换`，工作区干净且`app.zip`不存在。
- [x] 从组合根、五模式启动资源装配、相机正式帧受理、Worker完成回调、结果去重、统计、存图、PLC正常输出/延迟输出、Fault首因/停止/人工恢复、UI呈现、Qt信号槽和主/测试qmake清单重新追踪；阶段4实际影响41项：`SYS-008、SYS-010、UI-001..005、SET-005、DET-001..008、CAM-001..002、RUN-001..006、PLC-001..007、RES-001..005、SAVE-001..005`。
- [x] 41项保留功能均已改为`迁移中`，其余46项保持`已验证`，`MC-001..003`保持`已确认删除`；本轮集中门禁通过前不恢复状态、不提交且不进入阶段5。

### 唯一运行时、Pipeline与不可变运行上下文

- [x] 新建`InspectionRuntime`并成为单次生产运行的唯一状态与所有权根，持有稳定`DetectionMode`、唯一检测Worker、容量1呈现邮箱、小型ProductKey账本、Fault首因、PLC控制器和唯一ResultService。Runtime构造时必须注入`PipelineRegistry`，缺失直接失败；正式帧不会自动补建缺失运行会话。
- [x] 将`InspectionRunContext`收口到runtime并改为构造期完全初始化的不可变对象；每次`beginStart`只创建一次runId、UTC开始时间、MachineSettings、PreparedRecipe和Profile快照，运行期间算法、设备配置和结果事务均观察同一份快照。
- [x] 新建`PipelineRegistry`，按PreparedRecipe中的稳定`DetectionMode`唯一装配纸巾、钢印、字库、深度OCR和二维码+三期五种既有Pipeline；Worker容量保持1，钢印重叠资产、字符模板、阈值、多Profile与二维码运行策略从PreparedRecipe/Profile快照构造，不从Widget或可变设置回读。
- [x] `InspectionApplicationService`只组织启动预检、快照建立、相机/PLC参数下发、Pipeline启动、提交、停止、Fault收口和相机恢复；`ApplicationStartup`只组装OCR/二维码实现、PipelineRegistry、InspectionRuntime、CameraSession、应用服务和Widget，没有加入业务规则。

### 唯一结果事务、正常PLC与存图

- [x] 新建`ResultService`作为五模式唯一正式结果事务。每个合法`ProductKey`只能claim一次；一次事务内依次形成完整`InspectionPresentation`、更新正常统计一次、按策略提交最多一个产品存图任务、向容量1邮箱发布一次完整呈现，并在未进入Fault时执行一次正常PLC输出。重复或外来完成对象不再产生统计、呈现、存图或PLC副作用。
- [x] 正常PLC合同保持：OK写`DB1.DBB1033=0`；NG写49并由单次Qt定时器约100ms后写0；延迟剔除队列保存原始ProductKey并按既有产品偏移到期。连接断开、49失败或复位0失败均进入Fault，不重试猜测性产品结果、不把当前或唯一未结论产品补发为NG。
- [x] `ImageSaveService`继续固定容量32、两个写线程；ResultService按MachineSettings运行快照保留不保存/NG/OK/全部、标注/原图组合、OCR同帧原图、分类目录和JPEG质量。队满继续阻塞提交者而不丢正式存图任务；实际写失败只报警，不修改已产生算法结论、统计或PLC合同。
- [x] 新增完整只读`InspectionPresentation`并将原图Overlay渲染收口为runtime内部纯`InspectionPresentationRenderer`；Runtime目录对`ui/`、`Ui::*`、Widget、MainWindow和QMessageBox零依赖。UI只安装显示绑定并整体应用同一产品的图、判定、文字、模板名、统计和耗时。

### 简化Fault与旧路径删除

- [x] Fault只记录第一个原因，立即停止新正式产品受理、取消Worker与容量1呈现等待并协作退出线程；Fault前已经形成的算法结论继续按原结论完成正常统计/呈现/存图，不被系统NG覆盖。尚未形成正式结果的ProductKey在人工恢复时统一记`Unconfirmed`，不进入正常总数、NG或合格率。
- [x] UI故障语义固定为“视觉检测已暂停”“输送线状态未知”；解除软件锁定前必须由操作员确认、采集与Worker实际退出且未完成产品完成Unconfirmed收口。下一次启动重新执行相机、PLC和五模式资源预检；不自动补建会话、不假定输送线已停。
- [x] 删除`InspectionRuntimeController`、`InspectionRuntimeStartTransaction`、`InspectionRuntimeStopTransaction`、`InspectionFaultState`、`InspectionProductReconciler`、`DetectionSession`、旧ResultHandler、`DetectionModeWorkerFactory`、`InspectionRuntimePort`、`InspectionResultCoordinator`和`DetectionCompletionController`及全部主/测试qmake项。Widget删除旧五模式Worker装配、结果协调器、PLC脉冲队列、存图服务和Fault兜底回调编排，只保留界面绑定与应用命令。

### 测试源码与当前门禁状态

- [x] 重写`detection_completion_test`为14项阶段4合同源码：显式不可变运行上下文、五模式稳定Registry选择、重复产品副作用一次、单一呈现快照、OK写0、NG 49→约100ms→0、已发49在其他Fault后仍完成一次0复位、PLC写失败保持判定且零兜底、未完成产品Unconfirmed、Fault首因与Fault后拒收、延迟NG原始身份、重复结果只存一次、存图服务容量32/双Worker、容量满反压且不丢任务。
- [x] 更新`application_service_test`及工程清单，继续覆盖设置应用边界、五模式启动/停止/重启、预检顺序、PLC命令、相机命令和硬触发共享Session；两个阶段4 runtime测试入口统一为`QTEST_GUILESS_MAIN`，只创建`QCoreApplication`事件循环，不创建GUI平台对象，避免再次依赖Qt `windows`平台插件。
- [x] 主工程和两个runtime测试工程已登记新Runtime、PipelineRegistry、ResultService、InspectionPresentation/Renderer/RunContext及五种Pipeline所需源头，删除旧Controller/Transaction/Fault/Reconciler/Session/Coordinator/Presenter清单；Agent只编写源码、测试源码和qmake清单，未运行或间接触发qmake、构建、测试可执行文件或主程序。
- [x] 阶段4最终Agent静态门禁通过：90个正式功能ID唯一，状态为迁移中41、已验证46、已确认删除3；旧Runtime/Transaction/Fault/Reconciler/Session/ResultHandler/WorkerFactory/ResultCoordinator/CompletionController及旧Presenter生产/测试/qmake引用为0，Fault兜底49代码为0；runtime对`ui/`、`Ui::*`、Widget、MainWindow和QMessageBox引用为0；主工程及两个受影响runtime测试工程共283个文件项全部存在、大小写一致且无重复；两组测试声明/定义为14/14和7/7；23个差异C/C++文件严格UTF-8、花括号及本地include检查通过；全部26个现存差异文件无尾随空白且有末尾换行，`git diff --check`通过。暂存区为空，`app.zip`不存在。Agent未运行qmake、构建、测试可执行文件或主程序。
- [x] 2026-08-17阶段4首次Qt Creator Rebuild在`result_service.h:143`报告`UiCompletionMailbox`未声明，并在`result_service.cpp`形成`postUiWork`重载、静态成员和参数转换等连锁错误；根因是`ResultService`头文件直接使用嵌套类型`UiCompletionMailbox::Work`却依赖间接包含。现已直接包含`runtime/result_presentation_mailbox.h`，声明与定义签名一致；该头文件已在主工程和两个runtime测试工程清单中登记，无需增加工程项。修复不改变Runtime、结果事务、Fault、PLC或UI行为，`git diff --check`通过，等待同轮Rebuild复验。
- [x] 2026-08-17阶段4再次链接`detection_completion_test`时报告`BarcodeWordDetectionMode`未解析；引用来自该测试目标已登记的`machine_settings.cpp`，唯一定义位于漏列的`DetectionModes.cpp`。现已在`detection_completion_test.pro`补齐`DetectionModes.cpp/.h`，与主工程、`application_service_test`及`product_recipe_test`的唯一实现清单一致；没有复制常量、增加第二定义或改变检测模式行为，等待同轮重新Run qmake、Rebuild复验。
- [x] 2026-08-17用户确认阶段4Qt Creator集中门禁“没问题”；41项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。真实PLC、机械剔除和现场恢复仍不得由Fake或无PLC结果冒充验收。
- [x] 阶段4提交前最终静态门禁通过：90个正式功能ID唯一且状态为`已验证87/已确认删除3`；53个差异路径全部属于阶段4Runtime/Pipeline/结果事务、计划内旧路径删除、测试/qmake、功能表和执行记录；旧Runtime/Transaction/Fault/Reconciler/Session/ResultHandler/WorkerFactory/结果协调器/完成控制器/旧Presenter、Fault兜底49及runtime对UI引用均为0；主工程及两个runtime测试工程共283个文件项存在、大小写一致、无重复，测试声明/定义为14/14和7/7；两次编译修复的直接include与`DetectionModes.cpp/.h`工程项唯一；23个现存差异C/C++文件严格UTF-8且本地include可解析；暂存区为空、`app.zip`不存在，`git diff --check`通过。允许精确暂存并创建唯一阶段4本地提交，不推送。
- [x] 阶段4已创建本地提交`d446b6155c04180acaeb1775e096b71c935fb162 refactor(runtime): 完成阶段4统一运行与结果事务`；未推送，提交后工作区干净且`app.zip`不存在。

## 新架构完全替换阶段：阶段 5 模板编辑器（2026-08-17，已完成）

### 开始基准与真实影响范围

- [x] 开始时分支为`codex/ocrgangyin-refactor`，HEAD为`d446b6155c04180acaeb1775e096b71c935fb162 refactor(runtime): 完成阶段4统一运行与结果事务`，工作区干净且`app.zip`不存在；阶段4用户集中门禁、功能状态恢复、最终静态检查和本地提交均已完成。
- [x] 从制作模板按钮、相机预览/冻结/重拍/退出、ImageLabel鼠标与绘图事件、二维码即时验证、显示/原图坐标换算、钢印环/定位/日期/二维码资源、字符裁切命名、单/多Profile、单项/批量编辑、配方发布/选择/同UUID重发、跨模式记忆、启动资源预检、相机关闭和程序退出双向追踪；阶段5实际影响39项：`SYS-007、SYS-009、UI-001..003、UI-006、SET-003..006、SET-008..010、TPL-001..016、DET-002..006、CAM-001..002、CAM-006、RUN-001..002`。
- [x] 39项已统一改为`迁移中`，其余48项保持`已验证`，`MC-001..003`保持`已确认删除`。本阶段不改变五种算法判定/阈值/模型、正常统计、存图或PLC `0/49→约100ms→0`合同；用户集中门禁通过前不提交且不进入阶段6。

### 模板应用边界、编辑会话与只读状态

- [x] 新增唯一`TemplateApplicationService`，由`ApplicationStartup`组合根构造并注入；服务独占`RecipeStore`、二维码解码端口、`RecipeEditorSession`、编辑工作区、模式UUID记忆、当前`PreparedRecipeSnapshot`和字库Profile运行资产。Widget与模板页面不再持有Store、解码器或可变编辑会话。
- [x] 模板UI只能读取`const ProductRecipe`草稿、`const PreparedRecipeSnapshot`、`const TemplateModeMemory`和`const std::vector<WordTemplateProfile>`；新建/加载、草稿替换、Profile替换、模式记忆、资源暂存、事务发布和失败回滚均通过应用服务命令。删除页面对Profile容器、模式映射和PreparedRecipe的可变引用出口，同一状态不再由Widget和模板控制器各持一份。
- [x] 新增纯`TemplateGeometryService`统一显示图到原图矩形换算，并生成定位ROI、日期多边形和二维码多边形；新增`RecipeAssetService`统一在编辑会话工作区编码原图、定位模板、钢印环、`calibrate_config.yaml`和字符PNG，正式目录仍只由`RecipeStore`整目录事务发布。页面不直接读写模板文件、旧目录或INI。

### 模板页面、字符对话框与预览生命周期

- [x] 新建`TemplateEditorPage`并通过显式`TemplateEditorViewBindings`绑定现有控件；页面只调用Inspection/Settings/Template应用服务并负责模板交互呈现、引导文字、鼠标绘图和结构化错误显示，不包含`Widget*`、`Ui::Widget*`、RecipeStore、相机设备、生产线程、检测实现、PLC时序或存图服务。
- [x] 模板Preview/Freeze/重拍/退出状态、预览会话ID和最后一帧归页面所有；预览仍经阶段3唯一`InspectionApplicationService`/`CameraSession`命令，迟到帧按会话ID丢弃，停止/关闭相机/关闭程序继续协作结束采集。Widget删除第二份模板采集状态和重复预览信号处理。
- [x] 新`CharacterTemplateEditorDialog`只返回字符框、名称和裁剪后的`QImage`，不持久化配方或资源；示意图只从qrc读取，不使用外部目录fallback。字符资源由应用服务暂存并随同一UUID配方事务重发；取消或发布失败时正式配方保持上一完整版本。
- [x] 保持四种有模板模式的基础模板保存不受目标字符阻断；字符切割、目标确认、当前/批量阈值修改仍是独立编辑命令。纸巾模式继续发布零模板资产配方；五模式正式启动仍由既有资源预检决定是否允许运行。

### 旧路径删除、测试源码与工程清单

- [x] 删除`TemplateEditorController`及其业务`friend`、`CharacterTemplateCropDialog`、Widget模板几何/字符资产/Overlap缓存、旧模板目录选择和旧控制器对Widget私有成员的穿透；生产、测试和qmake中旧类名、旧文件名及对应include均为0引用，不保留兼容桥或并行保存/重发路径。
- [x] Widget只保留当前阶段尚需的主窗事件薄桥，并进一步删除29个无人调用的模板转发API；模板操作直接进入`TemplateEditorPage`。主工程登记Template应用/几何/资产服务、新页面和新字符对话框，并移除旧控制器/旧对话框工程项。
- [x] 新增`template_application_service_test` 6项测试源码，覆盖四种模板模式发布Prepared资产、单/多Profile几何与二维码即时验证、字符资产同UUID重发、事务失败不破坏正式配方、跨模式切换与纸巾无模板配方，以及Profile/模式状态只能经应用命令替换；测试工程显式登记`TemplateModeMemory`实现并使用`QTEST_APPLESS_MAIN`，不会创建GUI平台插件依赖。

### 当前Agent静态门禁

- [x] 阶段5生产依赖检查通过：模板页面/字符对话框对`Ui::Widget`、Widget、RecipeStore、二维码解码实现、相机/采集线程、Runtime实现、检测算法、PLC和存图服务均为0；application对Widget/UI/对话框/vendor为0；recipes对UI、detection实现、devices和旧设置管理器为0；业务`friend`为0。
- [x] 主工程159个C/C++文件项和新测试工程20个文件项均存在、大小写一致且无重复；TemplateApplicationService 29项、Geometry 2项、Asset 2项和TemplateEditorPage 90项定义均有声明，新测试声明/定义为6/6，Widget现存自动槽为26/26。14个受影响C/C++文件均为严格UTF-8，本地include核对通过。
- [x] 90个正式功能ID唯一，当前状态为待盘点0、已基线0、迁移中39、已验证48、已延期0、已确认删除3；阶段5差异只包含模板应用/页面/资产/几何、计划内旧路径删除、Widget与startup接入、测试/qmake、功能表和本记录。暂存区为空，`app.zip`不存在，`git diff --check`通过。
- [x] Agent未运行或间接触发qmake、nmake、jom、msbuild、cmake构建、Qt Creator构建、任何项目测试可执行文件、主程序或会编译、链接、启动项目的脚本。等待用户对本轮最终差异执行一次Qt Creator集中门禁；通过前39项保持`迁移中`，不提交且不进入阶段6。
- [x] 2026-08-17阶段5首次Qt Creator Rebuild在`template_editor_page.cpp:1103`报告`QFrame`构造参数无法转换，并在1815/1819报告`QCheckBox`未定义；根因是新页面头文件只前置声明`QGroupBox/QCheckBox`，实现文件却分别需要`QGroupBox*→QWidget*`基类转换和调用`QCheckBox`成员。现已在实现文件直接包含`QGroupBox`和`QCheckBox`，不依赖`ui_widget.h`或其他间接包含；未改变模板交互、设置、算法、相机或PLC行为，等待同轮Rebuild复验。
- [x] 同次Rebuild继续在`widget.cpp:430/466`报告`QLineEdit*→QLabel*`和`QToolButton*→QPushButton*`不可转换；逐项解析`widget.ui`后确认`currentTemplateName`真实类型为`QLineEdit`、`VideoShoot`真实类型为`QToolButton`，其余39个模板View绑定类型均与UI XML一致。现将两个绑定改为真实类型并为`QToolButton`补直接include，不做C风格/强制类型转换；控件实例、对象名、信号槽和界面行为均未改变，等待同轮Rebuild复验。
- [x] 阶段5新`template_application_service_test`首次编译三份含中文的生产源码时报告C2001/C1057及后续语法连锁错误；主工程已经统一使用MSVC `/utf-8`，但新测试目标漏列同一选项。现按既有runtime测试工程做法在该测试`.pro`的`msvc`块统一加入`QMAKE_CXXFLAGS += /utf-8`，不逐文件混用BOM、不改中文提示或业务逻辑；本修复需要对该测试工程重新Run qmake后Rebuild。
- [x] `template_application_service_test`复验为7通过/1失败，失败项`characterAssetsRepublishSameUuid`把字符源图尺寸硬编码为`40×20`，与生产流程按日期ROI实际裁剪出的尺寸不一致，PreparedRecipe正确以`PREPARE_CHARACTER_ASSETS_FAILED`拒绝。现将测试夹具改为从已发布PreparedProfile的定位中心、日期多边形和原图边界计算真实裁剪矩形，并据此生成字符框与字符图；未放宽生产Schema/资源校验。同期`recipe_store_test`为9/9通过，其中损坏YAML触发的OpenCV错误日志属于预期拒绝路径。
- [x] 2026-08-17用户确认阶段5Qt Creator统一门禁完成；39项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。真实PLC、机械剔除和现场恢复仍不得由Fake或无PLC结果冒充验收。
- [x] 阶段5门禁通过后只执行最终静态检查、精确暂存和唯一阶段5本地提交；不推送，提交完成后才进入阶段6。
- [x] 阶段5提交前最终静态门禁通过：90个正式功能ID唯一且状态为`已验证87/已确认删除3`；23个差异路径全部属于阶段5模板应用/页面/资产/几何、计划内旧路径删除、startup/Widget薄接入、测试/qmake、功能表和执行记录；旧模板控制器、旧字符裁切对话框及对应生产/测试/qmake引用为0，模板UI边界和recipes反向依赖命中为0；主工程160个源码/头/UI文件项与新测试工程20个文件项全部存在且无重复，新测试声明/定义6/6，Widget自动连接槽26/26；差异文本严格UTF-8、无尾随空白且均有末尾换行，暂存区为空、`app.zip`不存在，`git diff --check`通过。Agent未运行qmake、构建、测试可执行文件或主程序。
- [x] 阶段5已创建本地提交`393f07974421e9332bf68abf3672e15066db41ed refactor(template): 完成阶段5模板编辑器替换`；未推送，提交后工作区干净且`app.zip`不存在。

## 新架构完全替换阶段：阶段 6 MainWindow与页面（2026-08-17，进行中）

### 开始基准与真实影响范围

- [x] 开始时分支为`codex/ocrgangyin-refactor`，HEAD为`393f07974421e9332bf68abf3672e15066db41ed refactor(template): 完成阶段5模板编辑器替换`，工作区干净且`app.zip`不存在；阶段5用户统一门禁、功能状态恢复、最终静态检查和本地提交均已完成。
- [x] 从`ApplicationStartup`对象图、`Widget`构造/析构/自动槽、运行结果与Fault呈现、相机/PLC命令、MachineSettings页面、TemplateEditor页面、ImageLabel绘图、五模式启动与根目录共享类型、Qt信号槽、UI XML、翻译和主/测试qmake清单双向追踪。旧`Widget`仍是全部保留功能的唯一窗口与页面入口；阶段6又必须移动五模式共用的DetectionMode、定位Pose、纸巾检测器和vendor具体源，因此本轮实际影响除已删除多相机外的全部87项保留功能。
- [x] 87项保留功能统一改为`迁移中`，`MC-001..003`保持`已确认删除`；本轮集中门禁通过前不恢复状态、不提交且不进入阶段7。

### MainWindow、页面和纯组合根替换

- [x] 新建唯一顶层`ui/MainWindow`并沿用原`widget.ui`视觉结构、对象名、中文提示语义、自动槽和操作入口；UI表单改为`ui/main_window.ui`，Qt翻译上下文同步改为`MainWindow`。`ApplicationStartup`显式构造vendor设备实现、Store、Runtime、三个应用服务、`InspectionPage`、`MachineSettingsPage`、`TemplateEditorPage`和`MainWindow`，页面由组合根注入，startup未加入算法、设备时序、结果判定或UI用例规则。
- [x] 新`InspectionPage`接管检测结果绑定、Fault呈现、图像与状态区域；新`MachineSettingsPage`接管机器设置控件映射、dirty状态、默认/清空/保存、滚轮保护与数值校验；阶段5`TemplateEditorPage`继续作为唯一模板交互页。`MainWindow`只持有页面、应用服务和纯UI状态，负责顶层槽连接、结构化结果/错误呈现及窗口生命周期，不持有相机设备、PLC设备、生产线程、Pipeline、算法、存图服务或RecipeStore。
- [x] 新增`camera_application_contract.h`、`inspection_ui_contract.h`和`template_editor_contract.h`，把相机操作结果、检测View绑定/Fault快照、模板目录/二维码验证/Profile编辑快照收口为应用层命令、查询和DTO。UI不再包含`runtime/`、`devices/`、`detection/`或`recipes/`实现；跟踪Pose和完整结果呈现继续由Application转给唯一Runtime/ResultService，不恢复Widget信号中转。
- [x] 将原单体窗口实现按职责拆为`main_window.cpp`、`main_window_inspection.cpp`和`main_window_settings.cpp`；核心`main_window.cpp`为509个非空行。所有自研、非生成、非纯算法`.cpp`均不超过1500个非空行，最大的页面实现为1015行；没有为旧Widget保留兼容外壳或并行入口。

### 根目录旧类型迁移和同轮删除

- [x] `DetectionModes`迁为`contracts/detection_mode.*`，`TrackingTypes`中的定位Pose迁为`detection/positioning/detection_pose.h`，纸巾检测器迁为`detection/tissue/tissue_roll_detector.*`，重叠检测器迁为`detection/stamp/overlap_detector.*`，ImageLabel迁为`ui/widgets/image_label.*`；所有调用方和测试工程统一改用新路径与稳定类型。
- [x] Barcode公共值类型迁到`devices/barcode/barcode_types.h`，Barcode DLL API/适配器迁到`devices/barcode/vendor`；Paddle OCR实现及其内部源码迁到`devices/ocr/vendor/paddle`；Snap7设备实现与C++包装源码迁到`devices/plc/vendor`。供应商SDK类型和头文件只保留在对应vendor目录，startup是唯一构造具体设备实现的位置。
- [x] 同轮删除`widget.cpp/.h/.ui`、`TemplateMatch`、旧`InspectionRuntimeUiCoordinator`、旧`MachineSettingsPageController`、旧`imagelabel`、根目录DetectionModes/TrackingTypes/TissueRollDetector/Detector、根目录Barcode/Snap7和旧PaddleOCR路径；旧Widget信号、旧类名、旧include、生产/测试/qmake工程项均为0引用，不保留桥、别名或双路径。Git历史保留删除前版本；未触碰任何用户文件。

### 测试源码、工程清单与当前Agent门禁

- [x] 新增`tests/ui_tests/ui_architecture_test`及tests子目录工程，使用`QTEST_APPLESS_MAIN`静态覆盖MainWindow/ImageLabel目标类型、三个页面显式注入和页面不可复制合同；更新Application、Template、Recipe、五模式Pipeline、Camera/PLC/Barcode适配器等受影响测试源码及qmake路径。测试源码只验证新架构入口，不构造第二套运行或配方实现。
- [x] 全仓23个`.pro/.pri`静态解析结果为文件项缺失0、大小写错误0、重复0；阶段6新增/移动的52个生产源码、头、UI和资源文件全部登记在主工程。UI XML的根类/对象均为`MainWindow`，自定义ImageLabel头为`ui/widgets/image_label.h`；新增及拆分类型的声明/定义、Qt信号槽和自动连接槽已静态核对。
- [x] 阶段6静态依赖门禁通过：UI对Runtime/设备/Detection/Recipes实现直接include为0；runtime对Widget/MainWindow/`Ui::*`/QMessageBox为0；detection对UI/磁盘/PLC/相机SDK为0；recipes对Detection实现/UI/Runtime/设备/旧设置管理器为0；业务`friend`为0；vendor SDK引用未越出对应vendor实现和既有部署/链接清单。
- [x] 90个正式功能ID唯一，当前状态为`迁移中87/已确认删除3`；阶段6差异只包含MainWindow/页面/Application DTO边界、根目录与vendor迁移、计划内旧路径删除、测试/qmake、翻译、功能表和本记录。差异文本严格UTF-8，本地include、文件名大小写和源文件体积检查通过，`git diff --check`通过；暂存区为空且`app.zip`不存在。Agent未运行或间接触发qmake、构建、测试可执行文件或主程序。
- [x] 2026-08-17阶段6首次Qt Creator Rebuild在`template_editor_support.cpp:38`报告`DetectionMode`未声明并产生转换函数连锁错误；根因不是页面缺少旧Recipes include，而是首版`contracts/detection_mode.*`只迁入了旧Barcode模式字符串，没有把稳定`DetectionMode`枚举及JSON/UI ID转换从`product_recipe.*`一并收口。现已将枚举和四个转换函数迁入Application/UI可依赖的合同层，`ProductRecipe`反向包含该稳定合同并删除重复声明/定义；UI继续不包含Recipes实现。主工程及所有使用ProductRecipe/MachineSettings/ApplicationService的测试目标只链接这一份`detection_mode.cpp`，并为`recipe_store_test`补齐工程项；五种枚举值和持久化/UI字符串保持不变，等待同轮Run qmake、Rebuild复验。
- [x] 同轮Rebuild随后在`main_window_inspection.cpp`报告`Ui::MainWindow`未定义、`QSignalBlocker`无匹配构造及`cv::destroyAllWindows`不可见等连锁错误。拆分单元本身已直接包含`ui_main_window.h`，真实根因是手写`ui/main_window.h`与uic生成头同时使用`UI_MAIN_WINDOW_H`保护宏：手写头先定义宏后导致生成UI头被整份跳过。现将手写保护宏改为唯一`OCRGANGYIN_UI_MAIN_WINDOW_H`，并为两个直接调用`destroyAllWindows`的窗口实现文件补`opencv2/highgui.hpp`；未修改生成文件、UI对象、槽或运行行为，等待同轮Rebuild复验。
- [x] 同轮Rebuild继续在`main_window_settings.cpp:681/692`报告`InspectionPlcRunSettings`不完整；该类型属于Runtime层，Application头仅为内部签名前置声明，UI既不能构造不完整类型，也不得为此包含Runtime头。现新增应用层`PlcRunSettingsCommand`，UI和应用服务测试只构造该命令；Application实现文件逐字段转换为唯一Runtime `InspectionPlcRunSettings`后下发。字段类型、数值和PLC写入顺序不变，UI对Runtime直接include仍为0，等待同轮Rebuild复验。
- [x] 测试工程Rebuild在`application_service_test.cpp:120`报告`ICameraDevice`未定义并产生全部`override`连锁错误；阶段6移除Application头对CameraSession的间接包含后，Fake设备测试没有直接包含自身继承的端口。现为ApplicationService测试直接包含camera/plc端口，并同步为CameraSession、DetectionCompletion和OCR Pipeline中的Fake基类补齐各自端口头；相关头原已登记在测试qmake中，不增加实现或链接项，等待同轮Rebuild复验。
- [x] 同一测试继续在`application_service_test.cpp:456`报告`CameraSession`不完整；Application公开头刻意只前置声明该实现类型，但测试夹具直接构造Session，必须声明自己的实现依赖。现为该测试直接包含`runtime/camera_session.h`；工程项原已登记，Application/UI边界不回退，等待同轮Rebuild复验。
- [x] 2026-08-17用户确认阶段6Qt Creator统一门禁“都没问题”；87项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。真实PLC、机械剔除和现场恢复仍不得由Fake或无PLC结果冒充验收。
- [x] 阶段6门禁通过后只执行最终静态检查、精确暂存和唯一阶段6本地提交；不推送，提交完成后才进入阶段7。
- [x] 阶段6提交前最终静态门禁通过：90个正式功能ID唯一且状态为`已验证87/已确认删除3`；163个差异路径全部属于阶段6 MainWindow/页面/Application DTO边界、根目录与vendor迁移、计划内旧路径删除、受影响测试/qmake、翻译、功能表和执行记录；旧Widget/TemplateMatch/两个旧UI Controller及历史设置、多相机、相机线程类的精确生产引用为0，业务`friend`为0，UI/Runtime/Recipes依赖边界未回退；163个差异文件均已纳入范围核对，严格UTF-8、无尾随空白且有末尾换行，暂存区为空、`app.zip`不存在，`git diff --check`通过。Agent未运行qmake、构建、测试可执行文件或主程序。阶段7已知qmake重复部署、重复OpenCV项和续行问题留待阶段6提交后按既定阶段边界统一处理。
- [x] 阶段6已创建本地提交`0cd9d153ebf4c5b6c58a96a6e7f3e03aad4ce4cd refactor(ui): 完成阶段6主窗口与页面替换`；未推送，提交后工作区干净且`app.zip`不存在。

## 新架构完全替换阶段：阶段 7 qmake、第三方清单与最终清理（2026-08-17，进行中）

### 开始基准与真实影响范围

- [x] 开始时分支为`codex/ocrgangyin-refactor`，HEAD为`0cd9d153ebf4c5b6c58a96a6e7f3e03aad4ce4cd refactor(ui): 完成阶段6主窗口与页面替换`，工作区干净且`app.zip`不存在；阶段6用户统一门禁、功能状态恢复、最终静态检查和本地提交均已完成。
- [x] 阶段7修改主/测试qmake、链接与部署入口、第三方依赖清单，并要求全部自动化测试和87项保留功能最终验收；因此实际影响除已删除多相机外的全部87项保留功能。87项统一改为`迁移中`，`MC-001..003`保持`已确认删除`；最终统一门禁通过前不恢复状态、不提交。

### qmake、部署和第三方唯一组合

- [x] 主工程删除重复`RC_ICONS`、重复`target.path/INSTALLS`段、无效空`DISTFILES`、重复大小写C++11配置以及与MSVC无关的旧GCC Release参数；`TRANSLATIONS`末项不再错误续接后续赋值。清理未使用的AxContainer、SerialPort、Sql和Multimedia模块及对应窗口冗余include，主工程只声明真实使用的Qt Core/Gui/Widgets。
- [x] OpenCV删除断裂的组件库列表、两个重复tracking项、重复include/depend路径及未使用的img_hash/xfeatures2d项；主工程与现有测试一样按Release/Debug分别且只链接一次`opencv_world341/341d`。磁盘核对两份import lib均存在，现有Release部署继续从`dist/ShengYin`复制锁定运行库和模型，不改变OpenCV 3.4.1、算法或模型。
- [x] Snap7收口为唯一组合：主工程和PLC适配器测试各自编译同一份`app/devices/plc/vendor/snap7.cpp/.h`包装，链接`third_party/Libraries/win64/snap7.lib`，运行时部署匹配的`snap7.dll`；全仓不存在第二份Snap7实现。`third_party/DEPENDENCIES.md`已更新PaddleOCR vendor新路径、OpenCV链接事实及Snap7 wrapper/import lib/DLL路径、大小与SHA-256。
- [x] 全仓23个`.pro/.pri`共426个源码、头、UI、qrc和翻译文件项全部存在、大小写一致且单目标内无重复；主工程磁盘上的自研生产源码/头/UI/qrc/翻译均已登记，无漏列旧路径或虚构工程项。主工程仅有一个图标声明、一个三行平台安装段、一个Release部署入口、一个Snap7包装源和一组Release/Debug OpenCV world链接。

### 注释旧实现、无调用API与最终边界清理

- [x] 删除全仓零调用的40个旧公开/私有方法及对应状态：MainWindow假按钮/旧显示与清理桥、ImageLabel旧矩形/多边形兼容API、RecipeEditorSession未使用原配方/单资源入口、TemplateApplicationService未调用清空入口、Runtime/队列/存图/PLC/Mailbox未使用诊断getter、设置dirty入口和模板页面冗余包装getter；同步删除无入口`on_eliminatebutton_clicked`、注释`on_ReShoot`、隐藏`lineBoxIndex`及ImageLabel无人写入的旧绘图容器。所有真实按钮、模板顺序绘制、统计、剔除队列复位和可达设置入口保持不变。
- [x] 删除阶段6拆分遗留的注释代码、空QImage转换说明和兼容标记；非vendor生产源码已无`#if 0`、注释旧实现或“以后再删”桥。旧格式相关剩余`legacy`仅为`RecipeStore`明确拒绝旧目录的错误路径，许可证历史载荷测试继续属于保留功能，不构成运行兼容读取。
- [x] 为满足最终字面零引用门禁，将新检测类型`CharacterTemplateMatcher/TemplateMatchPreparedTemplates/CharacterTemplateMatchResult`分别更名为`CharacterGlyphMatcher/PreparedCharacterTemplates/CharacterMatchResult`并同步三个Pipeline、Runtime快照/Registry及三组测试源码；只清除与旧`TemplateMatch`类名的碰撞，不改变匹配算法、阈值或结果。
- [x] 将`InspectionTrackingKind`从Runtime头下沉至定位合同，删除Detection对`runtime/inspection_run_configuration.h`的反向include；Detection、Runtime、UI和Recipes最终依赖边界检查均为0命中，枚举值及软/硬触发运行计划不变。

### 测试源码与当前Agent静态门禁

- [x] 三组受Character匹配类型更名影响的Pipeline测试源码已同步唯一新类型；全仓16个测试目标的QtTest槽声明/定义静态核对无缺失，MainWindow 25个自动点击槽均对应`main_window.ui`真实控件。阶段7不复制测试或建立第二套实现，用户最终门禁需运行全部16个既有测试目标。
- [x] 计划规定的最终生产搜索已执行：`#include "widget.h"`、`MyThread|CameraThread|CMvCamera|TemplateMatch|Zhuizong`、`AppSettingsManager|TemplatePrivateSettings|GlobalSettings`、`MultiCamera|IMultiCameraProvider`、`QThread::terminate|.terminate(`及两个危险所有权模式在`app`均为0；`friend class`仅剩Paddle vendor内Clipper自身3处合法声明，业务`friend`为0。旧Widget/TemplateMatch/根目录类型/两个旧UI Controller精确生产引用为0；测试源码仍以JSON键字符串`TemplatePrivateSettings`构造旧格式拒绝样本，不是生产兼容读取路径。
- [x] 当前正式功能状态为`迁移中87/已确认删除3`，暂存区为空且`app.zip`不存在。Agent未运行或间接触发qmake、构建、任何测试可执行文件或主程序；真实PLC、机械剔除和现场恢复继续保持待验，不能由Fake结果代替。
- [x] 阶段7用户门禁前最终Agent静态复核通过：54个差异文件均属于qmake/部署/第三方清单、无调用API与注释旧实现删除、最终字面零引用和对应测试/文档。23个qmake工程共426个声明文件项，路径缺失、大小写错误和单目标重复均为0；主工程图标1处、Release部署1处、OpenCV Release/Debug各1处、Snap7包装源和链接各1处，tracking重复项为0。MainWindow为461非空行，非vendor生产`.cpp`超过1500非空行的文件为0；54个差异文本严格UTF-8、无尾随空白且均有末尾换行，本地include/声明定义/QtTest槽与MainWindow自动槽静态复核无新增缺口，依赖边界未回退；暂存和未跟踪文件均为0，`app.zip`不存在，`git diff --check`通过。Agent未执行任何构建或运行命令。
- [x] 阶段7首次Qt Creator Rebuild在三个MainWindow拆分实现中报告`QDebug`未定义，并在`main_window.cpp`报告`cv::destroyAllWindows`不可见；这是阶段7清理冗余间接include时漏掉的直接声明依赖。现为三个实际调用`qDebug()`的实现文件分别直接包含`QDebug`，并为实际调用`destroyAllWindows()`的`main_window.cpp`直接包含`opencv2/highgui.hpp`；另一个调用点`main_window_inspection.cpp`原已直接包含该OpenCV头。未修改日志文本、析构顺序、设置保存、相机、算法、PLC或UI行为，关联`SYS-001、SYS-009`，等待同轮Rebuild复验。
- [x] 2026-08-17用户确认阶段7Qt Creator最终统一门禁“都没问题”：主工程在补齐MainWindow直接include后完成复验，`application_service_test`构建和运行告警显示问题经核对不影响最终结果，全部自动测试与87项人工回归按本轮清单完成。87项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3；真实PLC、机械剔除和现场恢复仍保留待验，不能由Fake或无PLC结果冒充完整现场生产验收。
- [x] 阶段7门禁通过后只执行最终静态检查、精确暂存和唯一阶段7本地提交；不推送。本提交完成即表示新架构阶段0～7代码替换与软件侧门禁收口，不表示真实PLC现场生产验收完成。
- [x] 阶段7提交前最终Agent静态门禁通过：90个正式功能ID唯一且状态为`已验证87/已确认删除3`，其他状态均为0；54个差异文件全部属于阶段7 qmake/部署/第三方清单、无调用API和注释旧实现清理、字面零引用、直接include修复、对应测试及治理记录。全仓23个qmake工程共426个声明文件项均存在，大小写错误和单目标重复为0；主工程图标与Release部署各1处、OpenCV Release/Debug各1处、Snap7包装源/链接各1处，tracking重复项为0。计划规定的旧符号、线程强杀和危险所有权模式生产命中为0，业务`friend`为0，UI/Runtime/Detection/Recipes依赖边界未回退；MainWindow为463非空行，非vendor生产`.cpp`超过1500非空行的文件为0。54个差异文本严格UTF-8、无尾随空白且均有末尾换行，暂存前暂存区和未跟踪文件均为0，`app.zip`不存在，`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试可执行文件或主程序。

## 阶段0～7完成后的采集节拍语义修订（2026-08-18，等待用户门禁）

- [x] 用户独立批准解决`DIFF-011`：界面字段由“相机延时(ms)”明确为“硬触发延时(ms)”。软触发不得读取该值或增加固定等待，其连续节拍只由相机速度、容量1检测队列、容量1结果呈现邮箱和存图链反压决定；硬触发启动时把该ms值乘1000写入海康MVS `TriggerDelay`（µs）。
- [x] `CaptureWorker`已删除`minimumIntervalMs`参数、间隔状态、专用条件变量和`waitMinimumInterval()`；软触发在上一帧同步提交返回后立即发出下一次软件触发。预览同样不再携带无意义的间隔参数。
- [x] `CameraSessionCaptureConfiguration`只携带`hardwareTriggerDelayMicroseconds`；应用服务在机器设置快照边界完成ms到µs换算，`CameraSession`仅在HardwareLine0准备事务中要求设备写`TriggerDelay`。打开、恢复和软件触发路径仍明确把SDK `TriggerDelay`复位为0，不受界面值控制。
- [x] 硬触发`waitNextFrame`的1000ms仍只是一次条件变量等待切片：帧回调、设备错误或`interruptWait()`会立即提前唤醒；无外部沿时到期后重新等待，不产生产品失败、NG或Fault，也不限制后续帧。由于正常停止已有`interruptWait()`，该切片不是停止正确性的必要条件，只保留为采集线程周期性重新取得控制权的防御性心跳。
- [x] Schema 1已落盘键`inspection.minimumIntervalMs`暂不改名，避免本次行为修订制造第二份不兼容设置格式；其唯一当前语义已在Schema和功能表改为硬触发延时ms。正式功能`CAM-003、CAM-004、RUN-005`暂记`迁移中`，等待用户Qt Creator主程序与真实海康相机门禁后恢复为`已验证`。
- [x] 按用户此前要求，仓库`tests`已删除；本轮不创建测试代码，也不运行qmake、构建、测试程序或主程序，只执行正式代码静态检查，最终运行验证交由用户在Qt Creator完成。

## 阶段0～7完成后的外部引擎目录拆分（2026-08-18，等待用户门禁）

- [x] 用户明确要求将广义`devices`拆为物理设备与识别引擎：`app/devices`只保留`camera/plc`，新增`app/engines`承载`ocr/barcode`。
- [x] `IOcrEngine/PaddleOcrEngine`及Paddle vendor源码整体迁至`engines/ocr`；`IBarcodeDecoder/BarcodeDecoderAdapter`、公共值类型和DLL API整体迁至`engines/barcode`。所有include与主工程qmake清单同步改为新路径，接口名称、构造位置、模型、DLL ABI、算法和判定不变。
- [x] 当前架构文档、第三方依赖清单和正式功能路径同步区分`devices`与`engines`；历史执行记录中的旧路径保留为当时事实，不回写历史证据。
- [x] 受影响功能为`SYS-006、TPL-004、DET-004、DET-006、TOOL-002`，暂记`迁移中`；与既有采集节拍待验项合计`迁移中8/已验证79/已确认删除3`。等待用户在Qt Creator执行Run qmake、Rebuild，并复验OCR模型初始化、代表帧OCR、模板即时读码和二维码+三期模式。
- [x] 按用户此前要求，仓库`tests`已删除；本轮不创建或运行测试，不运行qmake、构建或主程序，只执行正式代码静态检查。

## 阶段0～7完成后的S1零调用代码清理（2026-08-18，已完成）

- [x] 用户明确批准只删除“现在不用且完全不影响功能”的代码或文件，并再次限定本轮只精简代码：图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL、部署资产和用户文件均不删除、不修改。原计划S0中的8项既有待验功能不因本轮清理改变状态，继续保持`迁移中8/已验证79/已确认删除3`。
- [x] 删除无任何生产入口的`runtime/detection_shadow_comparator.h/.cpp`及主qmake两条工程项。该比较器只接收两份外部`DetectionResult`做离线差异比较，从未接入startup、CameraSession、DetectionWorker、五种Pipeline、ResultService、统计、存图、UI或PLC链。
- [x] 删除全仓仅有声明和定义的5个普通查询方法：`InspectionApplicationService::queryCameraExposureRange()`、`InspectionRuntime::unresolvedFaultProductCount()`、`InspectionRuntime::faultUnconfirmedProductCount()`、`InspectionRuntime::runContext()`、`TemplateEditorPage::validatedBarcodeText()`。Qt自动槽、虚函数、SDK回调、许可证工具接口和vendor源码均已排除，没有按名称或目录猜测删除。
- [x] 连带删除只写不读的Runtime故障累计字段和模板页二维码文本字段，以及无人消费的`TemplateBarcodeValidationResult`返回DTO、结果参数、二维码角点回填和局部变量。实际故障未确认数量仍由`ResultService::recordUnconfirmedProducts()`登记；二维码仍执行真实解码并保留可读状态、失败原因和已验证ROI；曝光范围底层查询仍由实际曝光设置流程使用。
- [x] 本轮代码差异删除2个代码文件、清理6个零调用查询/返回接口及2组只写状态，`app/`的`.h/.cpp`由166个降为164个；未删除任何检测模式、Pipeline、Recipe、设备/引擎适配器、资源或第三方代码。仓库`tests`不存在，按用户要求未重建测试。
- [x] Agent静态门禁通过：已删文件和全部已删符号在`app`生产源码/qmake中为0引用；主qmake登记文件缺失为0；`git diff --check`通过；差异不含资源、`.pro.user`、`.ui.autosave`或`app.zip`。Agent未运行或间接触发qmake、构建、测试程序或主程序，等待用户在Qt Creator执行Run qmake、Rebuild和主程序回归后再收口与提交。
- [x] 2026-08-18用户按S1交付清单完成Run qmake、Rebuild及主程序回归后反馈“没问题”；旧文件删除后的工程可构建运行，二维码+三期模板即时验证和代表检测模式启停未发现回归，S1用户门禁通过。
- [x] S1没有对应独立用户功能ID，功能表不作状态变更；采集节拍语义修订和engines目录迁移涉及的`CAM-003、CAM-004、RUN-005、SYS-006、TPL-004、DET-004、DET-006、TOOL-002`继续保持`迁移中`，不得以本轮无功能精简门禁替代其真实相机/OCR专项验证。

## 架构精简S2～S7合并实施（2026-08-18，已通过用户统一门禁）

- [x] 用户明确要求不再逐阶段停下验证，一次性完成原计划S2～S7后再统一验收；本轮因此只合并交付节奏，不放宽行为等价、资源冻结、Qt 5.14/qmake/MSVC2017/C++11、真实PLC/机械/现场待验及Agent不运行构建程序等约束。
- [x] S2统一五模式完成合同和收尾入口：`DetectionResult`承载识别文本、模板名、诊断、Overlay、耗时及模式呈现策略；`ResultService`只保留一个完成回调，统计、存图和PLC仍在同一正式收尾边界且每产品只完成一次。OK继续写0，NG继续写49并约100ms后复位0。
- [x] S3建立唯一五模式描述表`DetectionModeDescriptor`：模式枚举、Recipe ID、UI ID、中文名称、定位类型、启动依赖、呈现和存图差异均集中登记。整机设置、模板模式记忆、启动预检和UI下拉框改为读取该表，删除固定下标、散落字符串和重复模式分支；装配Registry同时从Runtime迁至Detection。
- [x] S4将具体检测组装从Runtime迁至Detection：删除`runtime/pipeline_registry.*`，新增`detection/detection_registry.*`；定位、预处理、五种Pipeline和二维码策略由Detection内部组装，Runtime只管理生命周期、容量1队列、产品账本和完成转交，不再识别具体检测模式或算法类型。
- [x] S5将Profile快照从Runtime迁至Detection并收口构造：删除`runtime/inspection_profile_snapshot.*`，新增`detection/detection_profile_snapshot.*`；Application只提交已选配方和机器参数，不再逐字段拼装字符、二维码及定位内部结构。启动预检按唯一模式描述表检查目标字符、字符模板、条码引擎和多Profile条件。
- [x] S6完成UI依赖收口：`InspectionPage`和`MachineSettingsPage`改为显式ViewBindings，不再持有整个`Ui::MainWindow`；MainWindow仍是生成UI对象唯一所有者。五个检测模式由描述表动态填充，模板页的模式判断改用`DetectionMode`语义值。
- [x] S7将正式采集链收为`CaptureWorker → FrameQueue → Detection预处理/定位/Pipeline → ResultService`：CameraSession不再携带定位Profile、预处理正式帧或发布第二条跟踪结果链；模板预览仍保留独立预处理。硬触发容量1队列和QueueFull故障语义不变。
- [x] 按用户UTF-8要求，`app`正式C++源码中的`\\uXXXX`、`\\UXXXXXXXX`和`\\xNN`转义文本已机械等价转换为可直接阅读的中文/Unicode字符；截图中的`L"\\u2195  \\u62d6\\u52a8\\u8c03\\u6574"`现为`L"↕  拖动调整"`。不改变字符串内容、编码约定或Qt边界。
- [x] 本轮只精简和重组代码；图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL、部署资产和用户文件均未删除、未修改。`main_window.ui`只删除硬编码的五个下拉框条目，条目由唯一模式描述表在代码中恢复，不属于资源清理。
- [x] 87项保留功能统一标为`迁移中`，`MC-001..003`保持`已确认删除`；用户完成一次Run qmake、Rebuild、五模式/设置/模板/相机/存图/统计/PLC回归并确认通过前，不恢复为`已验证`，也不创建本轮提交。
- [x] 最终Agent静态门禁通过：`app`仍为164个`.h/.cpp`；主工程168个源码、头、UI、qrc和翻译登记项均存在且无重复；5个模式Descriptor唯一；旧Registry/Profile、五套Consumer/handle/finalize、Runtime具体模式分支、Runtime字符/二维码/模式Profile依赖、CameraSession定位和第二结果信号均为0；UI页面持有`Ui::MainWindow *`为0；人类可读Unicode转义为0；资源、`.pro.user`、`.ui.autosave`和`app.zip`差异均为0；`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试可执行文件或主程序，仓库`tests`继续不存在。
- [x] 2026-08-18用户确认“S2～S7已经验证成功”；本轮统一门禁通过，87项保留功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。
- [x] 本轮仅据用户确认关闭S2～S7代码与功能门禁；真实PLC在线读写、机械剔除和现场异常恢复仍保持单列待验，不以无PLC、Fake或一般主程序回归替代现场生产验收。

## S8 Runtime/Recipes认知精简（2026-08-18，已通过用户统一门禁）

- [x] 用户确认S2～S7验证成功并要求继续当前精简；S2～S7已封板为唯一提交`b22ff8d refactor: 完成检测架构精简S2至S7`，提交后工作区干净。
- [x] 开始基线为`app`自研`.h/.cpp`共164个，其中`runtime`24个、`recipes`12个；本轮目标为157、21、8，不删除资源，不运行qmake、构建、测试可执行文件或主程序。
- [x] 从设置保存/模式切换/模板新建编辑发布、启动检测、相机配置、软硬触发、DetectionWorker、ResultService和停止恢复重新追踪调用链；实际影响`SYS-007、SYS-010、UI-001..002、SET-003、SET-008..010、TPL-001..016、CAM-003..004、RUN-001..006`共32项，已从`已验证`改为`迁移中`，其余55项保持`已验证`，`MC-001..003`保持`已确认删除`。
- [x] 删除无独立策略的`InspectionRunConfiguration/InspectionRunPlan/InspectionAcquisitionKind`，由相机会话配置直接保存硬触发布尔值；软硬触发、延时、曝光、增益和统计清零语义保持。
- [x] 将只由`InspectionRuntime`使用的`InspectionRunContext`隐藏到实现文件，删除公开头文件；不可变运行快照字段和创建时机保持。
- [x] 删除只包装QMap的`TemplateModeMemory`，由`TemplateApplicationService`直接保存各模式已发布配方ID，UI模式ID继续读取唯一`DetectionModeDescriptor`。
- [x] 将只服务编辑会话的`RecipeAssetService`并入`RecipeEditorSession`，保持全部资源文件名、编码格式、资产键、相对路径和发布事务不变。
- [x] README已增加`app/`核心职责树和正式检测唯一阅读路径；当前`recipes`只剩ProductRecipe、PreparedRecipe、RecipeStore、RecipeEditorSession四组职责，Runtime主链保持CameraSession→CaptureWorker→FrameQueue→DetectionWorker→ResultService。
- [x] 新增面向维护者的`OCRGangYin开发者代码结构与维护指南.md`：按当前S8工作树逐项覆盖主工程登记文件和部署脚本，共163个工程/代码文件；说明五条核心调用链、每个文件职责和修改边界，并给出新增检测模式、线程/队列/内存、设置/配方、vendor、调试和交付指南。机器覆盖核对为163/163，无遗漏；该纯文档补充不改变本轮32项功能状态。
- [x] 根据用户要求将开发者指南的目录摘要扩展为完整文件树：逐项列出`app/`当前299个项目维护文件，代码、工程配置、UI、翻译、脚本、CSS、ICO、PNG和`.qm`均不省略；机器对照当前文件清单为299/299，遗漏0、误列0。被Git忽略的`AutoOCRproject.pro.user`明确标注为本机Qt Creator个人配置，不计入项目架构，也未修改任何资源文件。
- [x] 用户随后明确文件树和文件数只展示代码文件；开发者指南已改为仅列当前157个`.h/.cpp`（83个头文件、74个实现文件），工程配置、UI、资源清单、翻译、脚本、CSS、ICO、PNG和`.qm`不再出现在树中，也不计入代码文件数。机器对照当前代码清单为157/157，遗漏0、误列0；实际资源文件未删除、未修改。
- [x] Agent静态门禁通过：`app`自研`.h/.cpp`为157个，`runtime`21个、`recipes`8个；主qmake共161个源码/头/UI/qrc/翻译登记项，缺失0、重复0；7个已删文件及全部旧类型引用为0；人类可读Unicode转义0；资源差异0；32项迁移中、55项已验证、3项已确认删除；`tests`与`app.zip`均不存在，`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试可执行文件或主程序。
- [x] 2026-08-18用户确认S8统一门禁“验证通过”；32项受影响功能由`迁移中`恢复为`已验证`，正式功能状态为待盘点0、已基线0、迁移中0、已验证87、已延期0、已确认删除3。真实PLC在线读写、机械剔除和现场异常恢复继续单列待验，不以本轮一般验证替代现场生产验收。
- [x] S8提交前最终Agent静态门禁通过：90个正式功能ID唯一且状态为`已验证87/已确认删除3`，其他状态均为0；`app`代码文件157个、`runtime`21个、`recipes`8个；主qmake登记161个源码/头/UI/qrc/翻译项，缺失0、重复0，157个`.h/.cpp`与磁盘代码清单完全一致。7个已删包装文件和旧类型引用、人类可读Unicode转义、资源差异及用户文件差异均为0；开发者指南代码树157/157，`tests`与`app.zip`不存在，`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试程序或主程序；运行门禁结论来自用户确认。

## S9 UI状态与命令门禁统一（2026-08-18，已通过用户统一门禁）

- [x] 用户批准由Agent按风险自行判断UI单层、应用/数据双层和设备/Runtime三层保护；目标不是在每层复制相同`if`，而是统一UI显示、应用层唯一业务裁决、底层只保证线程/设备/事务安全。
- [x] 开始基线为分支`codex/ocrgangyin-refactor`、HEAD `66aedc7 refactor: 完成 Runtime 与 Recipes 认知精简`、工作区干净且`app.zip`不存在；本轮不修改图片、图标、QSS/CSS、翻译、`.qrc`、模型、DLL或部署资源，不运行qmake、构建、测试程序或主程序。
- [x] 从七种主操作状态、相机开关与参数、模板预览/冻结/保存、五模式启停、PLC连接与写入、设置/配方编辑、统计和剔除队列清理、Fault恢复及退出生命周期重新追踪；实际影响59项：`SYS-009..010、UI-001..006、SET-001..013、TPL-001..016、CAM-001..006、RUN-001..006、PLC-001..007、RES-001..003`。这些功能已由`已验证`改为`迁移中`，其余28项保持`已验证`，`MC-001..003`保持`已确认删除`。
- [x] `OperationUiPolicy`已成为唯一UI权限矩阵：一次接收七状态、相机打开和PLC连接上下文，一次输出主操作、模板、普通/相机/PLC设置、配方、统计和剔除队列的`Access{enabled, disabledReason}`；MainWindow只生成和分发一份快照，三个Page显式更新自己拥有的控件。
- [x] `InspectionPage`已删除整窗`findChildren<QAbstractButton *>`扫描和“先全禁再全开”；`MachineSettingsPage`已删除页面级全部编辑器开关，按None/Camera/PlcConnection/PlcRuntime应用同一快照；`TemplateEditorPage`按配方选择/编辑权限应用，禁用提示不再被模板操作说明覆盖。
- [x] UI槽已删除忙碌、相机打开/关闭、相机采集和PLC连接状态的重复业务前置判断，只保留输入格式、确认对话框和模板页面内部CaptureState路由。开关相机、相机参数、PLC、模板取景、统计和剔除队列均消费Application结构化结果，拒绝不再误报成功。
- [x] `InspectionApplicationService`删除`m_cameraOpen`状态副本，统一读取`CameraSession::isOpen/isCapturing`；公开相机、PLC、模板取景、统计和剔除队列命令在Application裁决允许状态，正式启动内部PLC下发使用私有设备助手，Runtime/CameraSession/Device继续负责线程、连接和SDK不变量。因UI不再查询采集状态，同时删除零调用的Application `isCapturing()`和三个MainWindow模板预览转发。
- [x] 开发者指南已升级为1.4，补充七状态权限矩阵、三类风险分层和新增控件维护方法；架构精简计划补充S9目标、边界和门禁。代码文件仍为157个，`runtime`21个、`recipes`8个，本轮不增删或修改资源。
- [x] 当前Agent静态门禁通过：20个差异文件严格UTF-8；主qmake 161个登记项唯一且缺失0；整窗按钮扫描、旧页面级开关、`m_cameraOpen`、UI提交模板忙碌状态和Application零调用采集查询均为0；UI业务控件`setEnabled`只剩统一Access应用点，对话框内部选择有效性除外；Application结构化设备/运行拒绝码均存在；人类可读Unicode转义和资源差异均为0；`app.zip`与`tests`不存在，`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试程序或主程序。
- [x] 2026-08-18用户确认S9统一门禁“没问题”：Qt Creator Run qmake、Rebuild及59项范围人工回归通过；59项受影响功能已由`迁移中`恢复为`已验证`，正式功能状态恢复为`已验证87/已确认删除3`。真实PLC在线读写、机械剔除和现场异常恢复继续单列待验，不以本轮一般验证替代现场生产验收。
- [x] S9提交前最终Agent静态门禁通过：20个差异文件与本轮白名单完全一致且均为严格UTF-8；90个正式功能ID唯一，状态为`已验证87/已确认删除3`，其他状态均为0；`app`代码文件157个、`runtime`21个、`recipes`8个；主qmake登记161项且重复0、缺失0；整窗按钮扫描、旧页面级开关、`m_cameraOpen`、UI模板忙碌副本、Application零调用采集查询、人类可读Unicode转义、资源差异均为0；`app.zip`与`tests`不存在，`git diff --check`通过。Agent未运行或间接触发qmake、构建、测试程序或主程序；运行门禁结论来自用户确认。
