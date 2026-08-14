# OCRGangYin 重构执行记录

## 仓库与阶段状态

- 基线HEAD：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`
- 基线分支：`codex/repo-layout`
- 当前工作分支：`codex/ocrgangyin-refactor`（从基线HEAD新建）
- 当前阶段：Stage 2 设备接口与运行协调（按用户风险接受条件进入）
- 当前切片：五模式`DetectionCompletion`临时结果帧交接与OCR同帧存图（实现中）
- 阶段结论：**Stage 1结构关口已通过，Stage 2进行中**。二维码、Paddle OCR、Snap7 PLC和海康单相机设备边界已分别提交为`ef3af6d`、`c78ebb8`、`9542aaa`和`e6dc0d2`；当前开始建立五模式统一的短生命周期检测完成对象，使判定结果和本次检测只读原帧一起交接，并修复深度OCR存图另取相机新帧的计划内差异。
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

## 未解决事项

| 问题 | 风险 | 是否阻塞当前门禁 | 下一步/需要谁确认 |
|---|---|---|---|
| 五模式实际固定样本和模板尚未登记 | 不能建立可重复算法/结果基线 | 不阻塞当前开发；阻塞最终验收 | 用户明确接受风险并延期，后续可补样本路径 |
| PLC、硬触发、停止/重启无本轮现场证据 | 不能确认外部副作用基线 | 不阻塞当前Stage 1；阻塞硬件替换与最终验收 | 用户明确延期；Stage 1保持旧硬件主链不变 |
| 单帧内存、P50/P95、慢盘存图尚无数值 | 不能量化Stage 2/3是否退化 | 不阻塞当前Stage 1；阻塞性能验收 | 用户明确延期；后续有条件时补测 |

## 结论

- 当前切片：Stage 2运行协调器已通过Qt Creator集中门禁，准备创建独立提交。
- 当前阶段：Stage 1结构关口已通过，Stage 2进行中；人工样本与现场证据按用户明确决定延期，不声称最终产品验收已满足。
- 功能状态计数：待盘点0 / 已基线60 / 迁移中0 / 已验证28 / 已延期2 / 已确认删除0。
- 下一允许动作：提交运行协调器切片并进入Stage 2剩余启动预检/参数门禁拆分。
