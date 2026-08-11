# OCRGangYin 重构执行记录

## 仓库与阶段状态

- 基线HEAD：`1c8d564fe42ce5717b7606513cc2b426a367ff4b`
- 基线分支：`codex/repo-layout`
- 当前工作分支：`codex/ocrgangyin-refactor`（从基线HEAD新建）
- 当前阶段：Stage 1 配方与算法拆分（按用户风险接受条件进入）
- 当前切片：基础产品配方数据、JSON校验与只读运行快照（已通过）
- 阶段结论：**Stage 1继续**。当前切片已完成Agent静态检查、用户Qt Creator Release主工程Rebuild/Run、`product_recipe_test`和旧纸巾基线测试复验。用户明确延期的人工样本、设备和性能证据保持“未验证”，不伪造为通过，但不阻塞下一Stage 1切片。
- 构建纪律：Agent未运行、未间接调用、也未通过GUI触发任何qmake、编译、链接、测试目标或主程序。

## Stage 0已完成范围

- 目标：给所有当前可达功能分配唯一ID，记录入口、调用链、输入、正常/失败结果、副作用和验证步骤；建立`tests/tests.pro`及首个离线检测测试。
- 受影响功能ID：`SYS-001..010`、`UI-001..009`、`SET-001..013`、`TPL-001..016`、`DET-001..008`、`CAM-001..006`、`RUN-001..006`、`PLC-001..007`、`RES-001..005`、`SAVE-001..005`、`MC-001..003`、`TOOL-001..002`。
- 明确不在范围内：不修改正式程序行为；不迁移生产职责；不调整算法、阈值、统计、PLC地址/值/顺序、相机时序；不补做多相机；不进入Stage 1；不构建/运行。

## Stage 1当前切片范围

- 用户风险接受：五模式固定样本、PLC、硬触发、存图和性能证据延期补充；继续开发不等于这些项目已验证。
- 受影响功能ID：`SET-003`、`SET-010`、`TPL-006`、`TPL-008`、`TPL-009`。
- 旧代码事实：产品私有设置由`AppSettingsManager`保存到`app_settings.appset`；`Widget::WordTemplateProfile`持有目录、定位模板、ROI、字符模板和私有设置；纸巾阈值当前同时存在整机6.0和检测器内部5.2来源。
- 本切片目标：新增基础`ProductRecipe`、固定五种模式ID、Schema 1 JSON解析/序列化、字段/资源相对路径校验和`shared_ptr<const ProductRecipe>`只读运行快照；暂不接管旧UI、模板目录或检测入口。
- 保持边界：不改变旧配置读写、模板制作、检测判定、线程、相机、PLC、统计或存图行为；代码切片只由用户在Qt Creator构建和运行测试。

## 功能状态变化

| 功能范围 | 修改前状态 | 修改后状态 | 本次为何涉及 | 验证证据 |
|---|---|---|---|---|
| SYS/UI/SET/TPL | 无对照表 | 已基线 | 启动、界面、设置和完整模板工作流属于Stage 0强制范围 | 源码/UI静态核对；实际操作待用户 |
| DET/CAM/RUN/PLC/RES/SAVE | 无对照表 | 已基线 | 五模式、软硬触发、设备副作用、统计和存图必须先锁定 | 源码/线程/回调静态核对；纸巾离线测试已通过；实际样本与设备待用户 |
| SET-010、DET-005 | 已基线 | 已基线 | 新增纸巾离线基线测试工程；修复测试运行库架构后锁定内部默认值、显式阈值、空图和纯黑图失败语义，不改变生产实现 | 主程序运行通过；Qt Creator Release连续两次`6 passed, 0 failed`、退出码0；真实纸巾样本仍待Stage 0回归 |
| SET-003、SET-010、TPL-006、TPL-008、TPL-009 | 已基线 | 已基线 | 新增基础配方Schema、五模式ID、资产路径校验和运行快照；尚未接管旧功能入口，不冒充为完整功能新路径验证 | Agent静态检查、主工程Rebuild/Run、`product_recipe_test`及旧纸巾测试均已通过；旧入口保持基线状态 |
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

## 已确认的关键现状

1. 纸巾整机默认在`GlobalSettings`中是6.0；`.ui`与检测器进程初始默认是5.2；主窗加载全局设置后会把整机值应用给后续检测器。Stage 1才按计划统一唯一6.0来源。
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
| 功能ID与状态 | 解析矩阵正式功能行（不含`DIFF-*`已知差异项）并检查ID/状态 | 90个唯一ID；切片收口后为88已基线、0迁移中、2已延期 | 90/90唯一；88/0/2，与统计一致 | 通过 |
| 禁止强杀线程 | 全仓搜索`QThread::terminate`/`.terminate()` | 不存在运行时强杀 | 0处命中 | 通过 |
| Stage 1配方工程清单 | 静态核对主工程及`tests/tests.pro`的源文件、子工程和运行库部署参数 | 所有路径存在；旧检测子工程保留；新配方测试4项 | PowerShell脚本解析0错误；必需路径0缺失；5模式ID和7个JSON关键字0缺失；旧生产实现文件0改动 | 通过 |
| Stage 1配方测试构建与编码修复 | 用户在Qt Creator/MSVC 2017 Release构建并运行`product_recipe_test` | 4项业务测试通过 | 首次因无BOM UTF-8中文字面量被代码页936解析而报`C4819/C2001/C1057`；改用C++11 Unicode转义后，2026-08-12 00:58用户复验为`6 passed, 0 failed, 0 skipped`，1ms，退出码0 | 通过（用户证据） |
| 当前纸巾基线测试复验 | 用户在同一Qt Creator Release tests工程运行`tissue_roll_detector_baseline_test` | 部署脚本改为可选OpenCV参数后原测试仍通过 | 2026-08-12用户反馈当前纸巾基线测试通过；2026-08-09已有两次`6 passed, 0 failed`完整证据 | 通过（用户证据） |
| 固定样本清单 | 解析TSV必填字段并按模式分组 | 15行、五模式各3类 | 15行；每模式OK/NG/FAILURE各一项；0行缺关键字段 | 通过 |
| 新文件格式 | 搜索新增文档、测试和清单的行尾空白 | 0处 | 0处 | 通过 |
| 主程序构建/运行 | 用户在Qt Creator Release执行Run qmake、Rebuild、Run | 含`recipes/product_recipe.*`的当前主程序可构建并启动 | 2026-08-12用户反馈当前主工程门禁通过 | 通过（用户证据） |
| 测试构建/运行复验 | 用户在Qt Creator执行 | 4项业务测试通过 | 01:50旧产物仍崩溃；完成x64 OpenCV部署后，01:59和02:00连续两次显示4项业务测试及QtTest自动初始化/清理共`6 passed, 0 failed, 0 skipped`，耗时3ms，退出码0 | 通过（用户证据，2026-08-09） |
| 运行库部署复核 | 检查EXE目录、Makefile链接后命令和SHA-256 | 判断链接后部署是否真实生效 | EXE目录已有三个运行库；OpenCV目标与`third_party`源DLL的SHA-256同为`B92A...B7C1B`；说明复制确实生效，但源DLL本身错误 | 通过；排除部署步骤未执行 |
| 中间运行环境诊断 | `dumpbin /dependents`检查EXE及OpenCV二级依赖；核对Qt Creator运行配置、EXE目录和系统VC运行库 | 找到`main()`前仍可能缺失的运行环境 | EXE依赖`Qt5Test.dll/Qt5Core.dll`；原EXE目录无这两个DLL；`tests.pro.user`为自定义运行配置；系统已有OpenCV需要的VC运行库 | 已通过完整DLL部署排除Qt PATH缺口 |
| `0xc000007b`架构诊断 | 读取Release目录及仓库同名DLL的PE Machine；核对OpenCV import library和发布DLL导出 | 找到无效映像的确定来源 | EXE/Qt DLL=`x64(0x8664)`；误部署OpenCV=`x86(0x014C)`；链接用OpenCV `.lib`为x64；`dist/ShengYin` OpenCV为x64且覆盖EXE导入的全部28个符号 | 通过；根因确定 |
| 架构安全的运行库部署修复 | Release改从`dist/ShengYin`复制OpenCV；脚本读取目标EXE和每个DLL的PE Machine，再复制并校验SHA-256 | x64 EXE只接受x64运行库；不匹配在构建链接后步骤明确失败 | Agent完成脚本语法、路径和PE静态检查，未执行脚本/构建/测试；用户Qt Creator Release复验通过；仓库暂无可用x64 Debug OpenCV DLL，门禁固定使用Release | 通过 |
| Agent项目执行纪律 | Agent不执行构建、测试或运行 | 不产生Agent运行证据 | 未执行，符合强制规则 | 通过 |

## 用户Qt Creator门禁

### A. 主程序构建与启动

1. Qt Creator打开 `app/AutoOCRproject.pro`。
2. 选择Qt 5.14.2、MSVC 2017 64-bit Kit和Release配置。
3. 当前切片已向主工程清单加入`recipes/product_recipe.*`，先执行Run qmake。
4. Rebuild并Run，确认Release部署脚本完成、授权有效、主窗正常打开。
5. 反馈Qt Creator完整构建结论和启动结论；失败时提供首个错误及相关上下文，不要跳过。

### B. Stage 1基础配方测试

1. Qt Creator另开 `tests/tests.pro`。
2. 使用与主程序相同Kit，执行Run qmake。
3. Build并运行`product_recipe_test`。
4. 预期4个测试函数全部通过；反馈测试汇总。

### C. 纸巾基线测试复验

1. 同一`tests/tests.pro`工程中Build并运行`tissue_roll_detector_baseline_test`。
2. 预期原4个业务测试仍全部通过；这用于确认运行库部署脚本改为可选OpenCV参数后未回归。

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
- [x] 未删除、移动或改写旧正式程序实现；仅新增配方模型并将其加入主工程清单。
- [x] 起始工作区没有用户未提交修改；当前只包含本切片代码、测试、工程清单和记录修改。

## 本地提交记录

| 提交 | 阶段/切片 | 功能ID | 内容 | 验证 |
|---|---|---|---|---|
| `030aaa0` | Stage 0功能盘点 | 全部90个ID | 功能矩阵、执行记录、固定样本清单 | Agent静态核对通过；纯文档提交 |
| `b783026` | Stage 0首个离线测试 | SET-010、DET-005 | tests工程、纸巾基线测试和架构安全的测试运行库部署 | Qt Creator Release连续两次测试通过 |

## 未解决事项

| 问题 | 风险 | 是否阻塞当前门禁 | 下一步/需要谁确认 |
|---|---|---|---|
| 五模式实际固定样本和模板尚未登记 | 不能建立可重复算法/结果基线 | 不阻塞当前开发；阻塞最终验收 | 用户明确接受风险并延期，后续可补样本路径 |
| PLC、硬触发、停止/重启无本轮现场证据 | 不能确认外部副作用基线 | 不阻塞当前Stage 1；阻塞硬件替换与最终验收 | 用户明确延期；Stage 1保持旧硬件主链不变 |
| 单帧内存、P50/P95、慢盘存图尚无数值 | 不能量化Stage 2/3是否退化 | 不阻塞当前Stage 1；阻塞性能验收 | 用户明确延期；后续有条件时补测 |

## 结论

- 当前切片：Stage 1基础产品配方数据、JSON校验与只读运行快照已通过全部门禁；旧入口未切换。
- 当前阶段：Stage 1继续；人工样本与现场证据按用户明确决定延期，不声称最终产品验收已满足。
- 功能状态计数：待盘点0 / 已基线88 / 迁移中0 / 已验证0 / 已延期2 / 已确认删除0。
- 下一允许动作：创建当前独立切片提交，然后按阶段1顺序开始“纸巾配方与纸巾检测”最小切片。
