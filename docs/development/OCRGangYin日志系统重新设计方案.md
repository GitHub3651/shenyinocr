# OCRGangYin 日志系统重新设计方案

## 1. 文档状态与最终决策

- 状态：日志底座、第一轮调用点改造和第二轮业务日志精简均已实施；待用户Release统一验证。
- 更新日期：2026-09-20。
- 权威范围：应用日志目录、会话文件、编码、格式、级别、类别、直接写盘、分卷、保留、业务日志布点和 Windows 崩溃记录边界。
- 当前进度：阶段0～5均已实施。用户首轮Release实测已确认新日志可写、文件与`stderr`双输出和UTF-8中文正常；第二轮已按第15节删除跨层重复和高频无效字段，补齐模板最终结果与设置实际值，并通过静态门禁，等待用户统一执行Release构建和代表操作验证。
- 替代关系：代码已完全替代旧 `ApplicationLogger` 的兼容、按日日志和同文件崩溃追加行为；旧日志不属于新系统管理范围。
- 实施门禁：第二轮在分支`codex/ocrgangyin-refactor`、HEAD `cc8549c`上实施；开始前已核对工作区和暂存区，实施期间未暂存、提交、推送、合并或改写历史，既有暂存与未暂存资产均保留。

本方案固定采用以下终局，不保留备选实现：

1. 不读取、识别、迁移、重命名、续写或删除旧日志。
2. 新日志只写入 `<exe>/logs`；旧 `<exe>/log` 永远不访问。
3. 每次进程启动创建独立会话文件，固定使用 UTF-8 无 BOM。
4. 使用 Qt 消息处理器和六个 `QLoggingCategory`，业务代码只使用 INFO/WARN/ERROR/FATAL，不设计 DEBUG 日志级别，不引入第三方日志库。
5. 日志处理器直接调用 `QFile::write()`；不增加自研内存缓冲、定时刷新、日志线程、队列或无锁结构。
6. INFO 不逐条 `flush()`；WARN/ERROR/FATAL 写入后立即 `flush()`；正常退出统一刷新并关闭。
7. 单卷上限 32 MiB；在启动和换卷前清理正常日志，保留 60 天并为新卷预留容量，使总容量不超过 1024 MiB 加一条日志的允许误差。
8. 正常日志和 Windows 崩溃记录使用独立文件，不共享正常日志文件和互斥锁。
9. 不迁移旧日志文本；每个有业务意义的操作记录一条完整摘要，纯算法中间步骤不记录。
10. 所有实际产生的日志使用同一份格式化文本同时写入会话文件和 `stderr`，便于开发时在Qt Creator实时查看；文件输出失败后仍继续输出 `stderr`。
11. 日志实施、构建和验证只面向当前已经验证的Release配置，不要求修复或使用Debug构建链。
12. 日志级别只由 `qCInfo/qCWarning/qCCritical`和Qt原生致命消息类型决定；消息正文不得用 `[INFO]`、`[WARN]`、`[ERROR]`或`[FATAL]`模拟级别。
13. 同一次同步业务操作只在能够给出完整结果的一个自然边界记录；返回完整错误的下层函数和显示同一结果的UI层不得重复记录。
14. 不增加通用日志去重器、限流器、采样器、事件对象或配置系统；通过删除错误布点、复用已有统计和选择唯一记录边界解决冗余。

## 2. 目标与非目标

### 2.1 目标

1. 用最少状态完成线程安全、可检索、可控空间的应用日志。
2. 记录时间、级别、类别、线程和必要业务标识。
3. 移除当前逐条 `flush()` 的主要性能浪费。
4. 控制新日志的单卷大小、保存天数和总容量。
5. 让启动、Runtime、设备、模板、设置、检测、存图、必要用户操作和退出留下足够完整的操作轨迹。
6. Release 保留所有有业务意义的 INFO 摘要；每个正常帧只记录最终结果和完整耗时，模板、最终分数、识别内容仅在有值时追加，NG或异常才追加原因；Run计数`accepted/completed/cancelled`和现有界面累计统计`total/ng`只在`run.stopped`输出，纯算法中间值和细粒度尝试不记录。
7. 每条实际产生的日志同时出现在会话文件和 `stderr`，文件用于事后排查，`stderr`用于开发时实时观察。
8. 保持五种检测模式、算法、模板、设置、相机、PLC、统计、存图和 UI 行为不变。

### 2.2 非目标

本方案不实现旧日志兼容、DEBUG日志级别、Debug构建支持、远程上传、数据库或 JSON 日志、链路追踪、EventBus、日志配置页面、每类一个 Logger、异步日志线程、自研缓冲协议、minidump 或崩溃恢复；不因为日志重构修改算法、模板 Schema、设备协议、业务状态机、QSS、图片、DLL或用户流程。

## 3. 当前实现基线

当前入口：

- `app/system_support/logging/application_logger.h/.cpp`
- `app/startup/application_startup.cpp`
- `app/system_support/crash/windows_crash_handler.h/.cpp`
- `app/system_support/crash/windows_crash_stack.h/.cpp`

当前源码事实：

1. 日志写入 `<exe>/log/app_log_yyyy-MM-dd.txt`。
2. 代码会扫描旧文件、识别 BOM、改名 `*_legacy_*` 并按三个月清理。
3. 所有 Qt 日志在全局互斥锁内写入并逐条 `flush()`。
4. 日志没有真实级别，Release 源码上下文通常为空。
5. 崩溃信息追加到正常日志使用的同一 `QFile`。
6. 当前生产源码有 31 个 Qt 日志调用点；PaddleOCR 另有 9 处 `std::cout`。
7. 部分 `qDebug()` 使用 `[ERROR]` 等字符串模拟严重程度。

这些事实只用于确定替换范围，不形成兼容要求。

## 4. 目标结构

```text
业务模块 qCInfo/qCWarning/qCCritical
              │
              ▼
        Qt 全局消息处理器
              │
              ├─ 格式化单行 UTF-8 文本
              ├─ QMutex 保护 QFile 和分卷状态
              ├─ 达到 32 MiB 时打开下一卷
              ├─ QFile::write() → <exe>/logs/ShengYin_会话_进程_分卷.log
              ├─ 同一行 fwrite() → stderr / Qt Creator应用程序输出
              └─ WARN/ERROR/FATAL 才刷新日志文件

Windows 未处理异常
              │
              ▼
    独立 WindowsCrashHandler → crash_会话_进程.txt
```

第一版不增加后台写线程、定时器、用户态缓冲或消息队列。每个正常帧只写一条最终摘要，帧接收成功和内部处理步骤不单独写日志；正常停止产生的取消帧不逐帧记录，只在`run.stopped`汇总，非正常帧拒绝或失败才记录WARN/ERROR。纯算法内部步骤不写日志；直接写入操作系统文件缓存已经足够。只有实际测量证明日志影响检测线程，才另立性能优化计划。

## 5. 文件和保留合同

### 5.1 根目录

生产日志目录固定为：

```cpp
QDir(QCoreApplication::applicationDirPath()).filePath("logs")
```

即 `<exe>/logs`。不得回退到旧 `<exe>/log`、用户 AppData、当前工作目录或临时目录。部署目录必须允许当前 Windows 用户创建和写入 `logs`。

### 5.2 会话文件

正常日志文件名：

```text
ShengYin_yyyyMMdd_HHmmss_zzz_pid_part.log
```

示例：

```text
ShengYin_20260825_101530_127_6124_001.log
ShengYin_20260825_101530_127_6124_002.log
```

规则：

- 时间是本次日志会话启动时间，包含毫秒。
- `pid`是当前进程 ID，`part`从 `001`开始。
- 文件使用 UTF-8 无 BOM 和 `\n`换行。
- 使用 `QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly`创建，不续写历史会话。
- 创建失败直接让 `start()`返回失败，不增加名称碰撞恢复分支。

### 5.3 旧日志完全忽略

新系统不得枚举或访问旧 `<exe>/log`。生产代码不再出现：

```text
app_log.txt
app_log_
legacy_
kUtf8Bom
```

旧日志由用户在软件之外自行保留或删除。

### 5.4 正常日志保留

只管理严格匹配以下名称的新文件：

```text
^ShengYin_\d{8}_\d{6}_\d{3}_\d+_\d{3}\.log$
```

固定常量只在 `application_logger.cpp`定义一次：

| 项目 | 值 |
|---|---:|
| 单卷上限 | 32 MiB |
| 最长保留 | 60 天 |
| 总容量上限 | 1024 MiB |

同一个清理函数只在两处执行：`start()`创建首卷之前，以及换卷时关闭当前卷之后、创建下一卷之前。它不使用定时器或后台线程。

1. 删除超过 60 天的匹配文件。
2. 重新计算剩余匹配文件总容量。
3. 按最后修改时间从旧到新删除，直到剩余文件不超过 992 MiB，为即将创建的 32 MiB 新卷预留空间。
4. 名称不匹配的文件永远不删除。
5. 删除失败通过 `stderr`输出一次汇总警告，不重试、不阻止启动。

清理时当前卷已经关闭，因此不需要“排除活动文件”分支。新卷最多增长到 32 MiB 加一条日志的长度，正常日志总量因此最多超过 1024 MiB 一条日志；第一版接受该明确误差。

### 5.5 崩溃文件保留

崩溃文件名：

```text
crash_yyyyMMdd_HHmmss_zzz_pid.txt
```

只删除严格匹配名称且超过 60 天的崩溃文件。崩溃文件不计入正常日志的 1024 MiB；第一版不增加崩溃文件容量算法。

## 6. 日志格式、类别和级别

### 6.1 单行格式

```text
timestamp level category tid=<tid> message
```

示例：

```text
2026-08-25 10:15:30.127 INFO app.runtime tid=7f30 event=run.started mode=word templates=1 trigger=software saveMode=save_none
2026-08-25 10:15:31.028 INFO app.detection tid=3a10 event=frame.completed mode=word result=OK ms=38.42 template=ABC score=0.944 text=GY20260825
2026-08-25 10:15:32.006 INFO app.runtime tid=7f30 event=run.stopped accepted=12 completed=12 cancelled=0 total=12 ng=1
2026-08-25 10:15:31.516 WARN app.device tid=3a10 event=camera.frame_timeout nativeCode=80000007
2026-08-25 10:15:32.031 ERROR app.device tid=7f30 event=plc.result_write_failed code=12
```

规则：

1. 时间使用本地时间，格式固定为 `yyyy-MM-dd HH:mm:ss.zzz`，保留毫秒，不附带时区。
2. 级别固定为 `INFO/WARN/ERROR/FATAL`，不输出 DEBUG。
3. 类别来自 `QMessageLogContext::category`。
4. PID只出现在文件名和 `app.start`事件，不在每行重复。
5. 业务字段使用简单的 `key=value`。
6. 只把消息中的回车和换行规范化为可见的 `\n`，不设计引号、空格和反斜杠转义协议。
7. Release不定义 `QT_MESSAGELOGCONTEXT`，不记录源码文件和行号。
8. 不再使用 `[DEBUG]`、`[INFO]`、`[WARN]`、`[ERROR]`、`[FATAL]`字符串模拟级别。

### 6.2 固定类别

第一版只定义六个类别：

| 类别 | 范围 |
|---|---|
| `app.startup` | 应用启动、环境检查、引擎装配、日志启动和退出 |
| `app.runtime` | Run生命周期、队列、Worker、Fault和结果事务 |
| `app.device` | 相机、PLC、OCR、条码和供应商适配边界 |
| `app.detection` | 检测输入、最终判定摘要、ROI异常和处理失败 |
| `app.template` | 模板预览、选择、加载、发布和资源准备 |
| `app.ui` | 页面状态恢复、显示失败和纯界面异常 |

不得为每个类、按钮、模式或错误码增加类别。

### 6.3 级别语义

| 级别 | 使用条件 | 文件刷新 |
|---|---|---|
| `INFO` | 有业务意义的正常操作及其结果，包括单帧最终摘要、连接、模板、设置和存图 | 不逐条刷新 |
| `WARN` | 可恢复异常、帧丢弃、队列满、参数回退 | 立即刷新 |
| `ERROR` | 当前操作失败、设备中断、PLC写入失败、模型或文件失败 | 立即刷新 |
| `FATAL` | 进程无法安全继续 | 立即刷新并终止 |

业务代码使用 `qCInfo/qCWarning/qCCritical`，致命路径保持Qt原有FATAL语义；生产代码不再使用 `qDebug/qCDebug`。消息处理器忽略意外收到的 `QtDebugMsg`。日志不代替用户提示、返回值、Runtime Fault或设备安全处理。

级别映射固定为：

| Qt调用或消息类型 | 文件级别 | 语义 |
|---|---|---|
| `qCInfo(category)` | `INFO` | 正常操作及正常结果 |
| `qCWarning(category)` | `WARN` | 可恢复异常，程序仍可继续 |
| `qCCritical(category)` | `ERROR` | 当前操作失败，但不会因此自动终止进程 |
| `QtFatalMsg` | `FATAL` | 记录后保持Qt原有终止行为 |

正确形式示例：

```cpp
qCInfo(logStartup) << "event=app.ready";
qCWarning(logDevice) << "event=camera.frame_timeout code=80000007";
qCCritical(logDevice) << "event=plc.write_failed code=12";
```

消息正文只写 `event`和业务字段，不重复写级别前缀。不得写成：

```cpp
qCInfo(logDevice) << "[ERROR] PLC write failed";
qCCritical(logDevice) << "[INFO] camera opened";
```

`qCCritical()`只表示ERROR，不会终止程序；不得为了提高日志严重程度改用 `qFatal()`。本方案不新增业务 `qFatal()`调用。

## 7. 写盘、并发和失败处理

### 7.1 正常写入

消息处理器固定执行：

1. 在互斥锁外取得时间、级别、类别和线程ID，生成完整单行 UTF-8。
2. 进入互斥锁。
3. 如果文件输出仍可用且当前文件已经达到 32 MiB，关闭当前卷，执行一次保留清理，再创建下一卷。
4. 调用一次 `QFile::write(line)`。
5. WARN/ERROR/FATAL 写入后调用 `flush()`；INFO不调用。
6. 退出互斥锁。
7. 使用一次 `std::fwrite()`把带换行的同一个 `line`写入 `stderr`，不检查返回值、不增加单独缓冲、刷新或重试状态。
8. `QtFatalMsg`完成文件和 `stderr`输出后调用 `std::abort()`，保持当前致命日志终止行为。

分卷只在写一条新日志前检查当前文件大小。因此文件最多可能超过32 MiB一条日志的长度；第一版接受该误差，不实现拆分缓冲或精确装箱。

文件和 `stderr`使用同一份已格式化UTF-8文本，不分别格式化。`stderr`写入放在文件互斥锁之外，避免Qt Creator输出速度影响其他线程写日志；它只承担开发时实时观察，不参与分卷和容量计算。

### 7.2 写入失败

- 首次创建目录或会话文件失败：`start()`返回错误，不安装自定义Handler；启动层显示一次警告并继续使用Qt默认输出。
- 运行期写入或分卷失败：在互斥锁内关闭文件并把 `fileSinkFailed`设为真；后续只输出 `stderr`。
- 失败后Handler继续安装到正常退出，不在业务线程中调用 `qInstallMessageHandler(nullptr)`。
- 失败原因只通过 `stderr`输出一次；不递归调用Qt日志、不重试、不切换目录。

### 7.3 正常退出

所有业务对象和工作线程结束后：

1. 记录 `app.stop`。
2. 恢复Qt默认Handler，阻止新的消息进入自定义处理器。
3. 在互斥锁内 `flush()`并关闭文件。
4. 清空当前文件和会话状态。

## 8. 业务日志布点

日志丰富度由业务调用点保证，不给 Logger 增加事件对象、包装层、去重器或聚合器。每个有业务意义的操作在自然边界记录一次，能够回答“谁在何时做了什么、使用了哪些关键参数、结果是什么、失败原因是什么”。不为普通函数进入/退出、UI 重绘、鼠标移动或算法内层循环写日志。

| 范围 | INFO | WARN/ERROR |
|---|---|---|
| 启动 | `app.start`、版本、构建类型、PID、日志文件；`app.ready`；`app.stop` | 环境、设置、引擎或日志初始化失败 |
| Runtime | `run.started`、`run.stopped`、模式、触发/存图方式、Run计数`accepted/completed/cancelled`和现有累计统计`total/ng` | 唯一启动失败摘要、队列异常和进入Fault；正常停止不逐帧记录取消或旧结果 |
| 相机 | 打开、关闭和用户可见的参数应用结果 | 超时、断连、采集停止、参数应用失败、帧拒绝或丢弃 |
| PLC | 连接、断开、触发模式应用、正常读写结果 | 连接、读取、写入或复位失败 |
| OCR/条码 | 引擎/DLL加载成功和版本；最终识别结果、最终置信度和采用策略随单帧最终摘要记录 | 模型/DLL加载失败、调用错误、预算超时 |
| 模板 | 只记录预览开始/停止、恢复、保存、选择更新、应用和清除的最终结果；不记录模糊的`requested`事件；最终选中模板和最高分随单帧摘要记录 | 资源缺失、解析失败、准备失败、保存或应用失败 |
| 设置 | 加载结果和每次成功提交的`key/value`摘要；图像保存路径最终确定后只记录一次 | 损坏、保存失败、恢复失败 |
| 检测 | 每个正常帧只记录一条最终摘要，固定包含模式、判定和完整耗时；模板、最终分数、识别结果有值才追加，OK不重复成功诊断 | ROI非法、资源缺失、非正常帧拒绝或处理失败；正常停止取消不逐帧记录，错误摘要不显示RunId、sequence、帧号或内部会话ID |
| 存图 | 每次保存完成记录用途、文件路径和结果 | 队列满、目录/编码/写盘失败、停止时丢弃统计 |
| UI | 只记录会改变业务状态的用户操作及结果 | 页面恢复、数据显示或操作提交失败 |

以下纯内部细节不记录：

- 单次条码策略尝试。
- OCR或条码的候选结果、分段内容和详细置信度。
- 模板候选、分数和中间几何值。
- 算法各阶段耗时和其他中间值。

如果某个中间信息确实是定位失败所必需的，只把最终采用值或失败原因合并进单帧 INFO/WARN/ERROR摘要，不恢复DEBUG级别，也不逐步输出算法过程。

不为了日志新增故障锁存、节流或聚合状态；日志直接跟随已有业务状态和操作边界。

字符匹配的`Templates or target image is empty`和`No non-overlapping match found`都属于算法内部结果，不在逐帧算法函数中单独记录。前者需要的最终失败原因由检测结果向外传递并只进入单帧最终摘要，后者仅影响最终判定；两者都不得在算法调用点刷屏。

## 9. 供应商输出和崩溃记录

### 9.1 PaddleOCR和供应商输出

1. 删除PaddleOCR逐字符、逐框的 `std::cout`。
2. OCR和条码最终结果统一写入对应的单帧 INFO 摘要，不再单独重复输出完整结果。
3. 配置、模型和条码DLL加载结果以及调用失败由 `app.device`记录。
4. 不全局重定向 `stdout/stderr`，不捕获供应商DLL内部输出。

### 9.2 崩溃记录

1. `ApplicationLogger`删除 `appendCrashInformation()`。
2. `WindowsCrashHandler`取得新日志目录，继续复用现有 `WindowsCrashStack`生成异常文本。
3. 异常文本写入独立 `crash_*.txt`，不访问正常日志文件、互斥锁或状态。
4. 保持当前“尽力写入后终止”的行为，不增加恢复、吞异常、minidump或符号化框架。
5. 本方案不宣称当前Qt字符串和文件操作具备严格异常上下文安全性；若需要更高等级崩溃诊断，另立计划。

## 10. 目标接口和文件范围

### 10.1 最小接口

```cpp
class ApplicationLogger
{
public:
    static bool start(QString *errorMessage = nullptr);
    static void stop();
};
```

`start()`内部固定使用 `<exe>/logs`。不公开 `flush()`和活动文件状态，不提供旧日志迁移、查询、上传、业务事件对象或崩溃追加接口。

### 10.2 计划内生产文件

- `app/system_support/logging/application_logger.h/.cpp`
- `app/system_support/logging/log_categories.h/.cpp`（新增）
- `app/system_support/crash/windows_crash_handler.h/.cpp`
- `app/system_support/crash/windows_crash_stack.h/.cpp`
- `app/startup/application_startup.cpp`
- `app/AutoOCRproject.pro`
- 当前实际包含Qt日志或 `std::cout`的生产 `.cpp`，仅修改日志调用所在语句。
- `app/contracts/detection_mode.h/.cpp`和`app/detection/detection_registry.h/.cpp`，只删除精简成功日志后不再使用的`workerLogName`元数据。

日志调用替换不得改变所在函数的控制流、返回值、线程、队列、算法、设备调用、模板数据、设置数据、UI文案或文件保存结果。

### 10.3 文档和验证文件

- `docs/development/OCRGangYin日志系统重新设计方案.md`
- `docs/development/OCRGangYin计划索引.md`
- `docs/development/OCRGangYin现有功能对照表.md`
- `docs/development/OCRGangYin重构执行记录.md`

本方案不新增独立日志测试工程、测试专用生产接口或故障注入框架。

## 11. 实施阶段

### 阶段0：基线

1. 记录分支、HEAD、工作区、暂存区和用户文件。
2. 核对其他有效计划与 `application_startup.cpp`、日志调用点和工程清单的交叉范围。
3. 统计当前Qt日志和 `std::cout`调用点。

### 阶段1：日志底座

1. 原位重写 `ApplicationLogger`，删除全部旧兼容代码。
2. 实现 `<exe>/logs`、会话文件、UTF-8无BOM、直接写入、简单分卷、60天和1024 MiB保留。
3. 新增六个日志类别。
4. 启动层在业务对象构造前安装日志，记录 `app.start/app.ready/app.stop`。
5. 实现启动失败和运行期文件输出禁用路径。

### 阶段2：调用点

1. 删除字符串伪级别和不一致标签；级别只由Qt日志调用函数决定，消息正文不再出现手写级别前缀。
2. 按第8节重新选择INFO、WARN和ERROR事件，保持现有FATAL终止语义。
3. 补充必要的 `event`、模式、判定、耗时、按需模板/分数/识别结果和错误码；日志正文不显示RunId、sequence和帧号。
4. 为单帧完成、设备操作、模板/设置提交和存图结果保留紧凑 INFO；帧接收成功和内部成功步骤不单独输出，纯算法内部细节直接删除，不设置隐藏级别。
5. 删除PaddleOCR `std::cout`，在适配器边界保留摘要。

### 阶段3：崩溃分离

1. 删除正常日志的崩溃追加接口。
2. 独立创建和清理 `crash_*.txt`。
3. 保持异常终止语义，不扩张崩溃框架。

### 阶段4：收口

1. 更新功能对照表、执行记录和计划索引。
2. 执行静态检查、Git检查和最小运行验证，不新增日志测试框架。
3. 由用户在Qt Creator完成构建、运行和人工验证。

### 阶段5：第二轮业务日志精简

1. 按第15节删除跨层重复、正常内部成功步骤、正常停止逐帧取消和无意义内部字段。
2. 将同步Run启动失败收口为一条完整最终摘要；异步设备根因和`run.fault`状态转换允许各保留一条，因为它们表示不同事实。
3. 将单帧累计统计移入`run.stopped`，补齐模板最终操作结果和设置实际值。
4. 不修改`ApplicationLogger`、六个类别、日志目录、文件格式、分卷、保留、崩溃文件和文件/`stderr`双输出合同。
5. 同步功能对照表、执行记录和计划索引，然后执行静态门禁并交给用户统一Release验证。

## 12. 验证门禁

### 12.1 最小验证

不新增独立日志测试工程。实施后只做与风险相称的检查：

1. 启动、执行一组代表性操作并正常退出，确认会话文件名、UTF-8中文、级别、类别和完整业务字段，并确认同一条代表日志同时出现在文件和Qt Creator应用程序输出中。
2. 从现有工作线程自然产生并发日志，确认每行完整、程序无死锁或明显卡顿，WARN/ERROR能够及时看到。
3. 核对32 MiB分卷和60天/1024 MiB清理代码路径；条件允许时使用测试文件做一次烟雾验证，不为此增加生产接口。
4. 保留一个旧 `<exe>/log`代表文件，确认启动和退出后内容及修改时间不变，新日志只出现在 `<exe>/logs`。
5. 不制造真实生产崩溃、磁盘故障或权限故障。

### 12.2 静态门禁

1. 生产代码中旧文件名、`legacy_`、`kUtf8Bom`和旧编码探测引用为0。
2. `ApplicationLogger::appendCrashInformation`、公开 `flush()`和生产 `std::cout`日志引用为0。
3. 生产 `qDebug/qCDebug`为0；日志消息中的 `[DEBUG]`、`[INFO]`、`[WARN]`、`[ERROR]`、`[FATAL]`字符串级别前缀为0。
4. 日志目录中不新增 `QTimer`、`QThread`、自研队列或用户态缓冲。
5. 六个类别在工程清单中唯一登记；日志模块不依赖UI、Detection实现、设备SDK或模板Store。
6. 调用点替换不改变控制流，`git diff --check`通过，用户无关修改保持不变。

### 12.3 Qt Creator和人工门禁

1. 只使用当前已验证的Release配置执行Run qmake、Rebuild并启动主程序；本方案不要求Debug构建。
2. 确认 `<exe>/logs`、会话文件、UTF-8中文和 `app.start/app.ready/app.stop`，并确认Qt Creator应用程序输出同步显示所有实际产生的日志。
3. 执行相机、五模式、模板、设置、PLC和存图的代表性成功/失败路径，确认正常帧只有一条最终摘要，包含模式、判定和耗时，按需包含模板、分数、识别结果或失败原因，且不显示累计统计、RunId、sequence、帧号和内部布尔状态；`run.stopped`集中包含最终统计。
4. 正常退出后确认日志完整，旧 `<exe>/log`不变，检测和界面无明显日志写入卡顿。

未执行真实相机、PLC、长时间运行和隔离崩溃测试时必须标记待验。

## 13. 完成标准

1. 旧日志兼容、迁移、改名和清理代码全部删除。
2. 新目录、会话文件、UTF-8、格式、文件与 `stderr`双输出、直接写入、简单分卷和保留合同完成。
3. 当前调用点已按INFO/WARN/ERROR/FATAL和六个类别收口；级别全部由Qt调用函数决定，生产 `qDebug/qCDebug`和字符串级别前缀为0；Release日志覆盖有业务意义的正常操作和单帧最终摘要，不记录普通函数调用或算法内层过程。
4. PaddleOCR `std::cout`已删除，OCR和条码最终结果已进入对应的单帧 INFO 摘要。
5. 正常日志和崩溃文件分离。
6. 静态门禁、最小运行验证和用户Qt Creator门禁均有实际通过证据。
7. 五模式、模板、设置、相机、PLC、统计、存图、UI和退出行为无回归。
8. 功能对照表、执行记录和计划索引同步到真实状态。
9. 第15节列出的禁止事件和重复组合为0，模板最终操作、设置实际值和`run.stopped`统计具备代表性运行证据。

## 14. 实施结果（2026-08-25）

### 14.1 已完成

1. 原位重写`ApplicationLogger`，接口收口为`start(QString *)/stop()`；新日志只写`<exe>/logs`，旧`<exe>/log`及`app_log`、BOM探测、`legacy_`改名和旧文件清理代码均已删除。
2. 新增六个唯一`QLoggingCategory`；所有生产日志统一使用`qCInfo/qCWarning/qCCritical`，消息处理器忽略`QtDebugMsg`，同一UTF-8文本直接写会话文件和`stderr`。
3. 完成32 MiB分卷、正常日志60天/1024 MiB保留、换卷预留992 MiB、严格文件名匹配和失败一次告警；没有新增日志线程、队列、定时器、用户态缓存、重试或备用目录。
4. 完成启动、设置、Run、相机、PLC、单帧最终摘要、存图和关键UI操作日志；根据用户首轮Release实测删除正常`frame.received`、Worker/采集等内部成功步骤和重复状态字段。单帧最终摘要固定包含模式、判定和完整耗时，模板、最终跟踪分数、识别文本按有值追加，NG或异常才追加原因；逐帧累计统计、RunId、sequence、帧号、`confidence=unavailable`及存图/PLC/呈现内部布尔状态不再显示，统计集中在`run.stopped`。
5. 删除字符匹配内层“无非重叠结果”、条码逐策略尝试/最终尝试统计、Paddle配置逐项和OCR逐字符/置信度输出；最终采用结果或失败原因只保留在设备边界及单帧摘要。精简Worker成功日志后同步删除唯一用于该日志的`workerLogName`元数据。
6. `WindowsCrashHandler`改为独立`crash_*.txt`，只清理严格匹配且超过60天的崩溃文件，不再访问正常日志文件、Logger互斥锁或`appendCrashInformation()`。
7. `AutoOCRproject.pro`已唯一登记`log_categories.h/.cpp`；功能对照表、执行记录和计划索引已同步。

### 14.2 静态证据

- 生产`qDebug/qCDebug`、`std::cout`、字符串伪级别、`app_log`、`legacy_`、`kUtf8Bom`和四个旧`ApplicationLogger`接口引用均为0。
- 六个类别声明6处、定义6处且名称唯一；全部分类日志文件显式包含`log_categories.h`，未发现其他类别。
- 日志目录未新增后台线程、消息队列、定时器或第三方日志依赖；正常日志只有一个`QMutex`保护`QFile`和分卷状态，崩溃文件完全独立。
- `git diff --check`通过；第二轮禁止事件和裸内部数字字段静态命中为0，既有暂存区保持原状。静态检查不等同于编译、运行、硬件或现场验证。

### 14.3 待用户统一验证

用户已完成一次Release构建和二维码+三期软触发运行，确认新会话文件、文件/`stderr`双输出和UTF-8日志文件中文正常。第15节第二轮方案现已全部实施并完成静态检查；尚需用户验证新的启动、相机、Run、模板、设置和存图输出。统一验证通过前，`SYS-004`和`SYS-005`保持“迁移中”。

## 15. 第二轮业务日志精简方案（已实施，待Release验证）

### 15.1 审查基线和边界

1. 静态审查覆盖生产代码15个文件中的122个`qCInfo/qCWarning/qCCritical`调用点。
2. 用户代表日志共88行，其中70行是`frame.completed`；高频帧结果本身属于业务证据，但每帧累计统计、跨层完成事件和正常取消不应重复输出。
3. 第二轮只调整日志调用语句和为日志准备的轻量文本转换；不得修改算法判定、Runtime状态机、相机/PLC调用顺序、模板和设置数据、存图结果或UI文案。
4. 不新增Logger包装层、事件类、配置项、通用去重/节流状态、后台线程、定时器或测试工程。
5. 日志底座、`<exe>/logs`、本地毫秒时间、`tid`、六个类别、文件与`stderr`双输出、32 MiB分卷、60天/1024 MiB保留和独立崩溃文件全部保持不变。

### 15.2 唯一记录边界

1. 同步调用能够通过返回值完整传递结果时，只由最外层业务操作记录一次；下层函数不再记录同一成功、拒绝或失败，UI弹窗和状态栏展示也不另记日志。
2. 异步相机断连、PLC写入失败、队列满等无法作为同步返回值传递的根因保留一条设备或检测日志；如果它导致Runtime进入Fault，再保留一条`run.fault`状态摘要。
3. `INFO`记录业务成功或正常结果；正常NG仍是INFO。用户正常停止引起的帧取消不是WARN，只进入最终统计。
4. 每个事件优先使用可读名称；已有稳定名称函数时直接复用，不为日志建立新的枚举映射框架。`status=0/issue=2/state=1/field=3/mode=0`等裸数字改为可读文字或随冗余事件一并删除。
5. `tid`、设备`nativeCode`、文件路径、最终模板、最终分数、识别文本和失败原因继续保留；内部`session`、成功事件中的`stopped=1`、固定为真的能力字段和无意义`adjusted=0`删除。

### 15.3 启动和UI

| 当前事件 | 处理 | 终局 |
|---|---|---|
| `window.ready` + `app.ready window=maximized` | 合并 | 只保留`app.ready` |
| `statistics.reset context=startup` | 删除成功日志 | 启动统计重置失败才记录 |
| `ui.splitter_state_saved` | 删除成功日志 | 只保留`ui.splitter_state_save_failed`和启动恢复无效警告 |
| `services.assembled` | 保留 | 每次启动一条，确认实际设备/引擎组合 |
| `settings.loaded status=<数字>` | 改字段 | 使用可读加载状态、模式和触发方式 |
| `ocr.engine_loaded detector=1 recognizer=1 ...` | 删除固定字段 | 保留配置路径、classifier和GPU等实际可变项 |

### 15.4 相机和PLC

1. 删除`camera.open_request_plc_ready`、`camera.open_request_plc_failed`、`camera.open_request_completed`和设备失败后的`camera.open_request_failed`；分别由`plc.connected/plc.connect_failed`和`camera.opened/camera.*_failed`唯一表示结果。
2. 保留无对应下层结果的用户操作拒绝，并统一为`camera.open_rejected code=<语义码> reason=<原因>`和`camera.close_rejected ...`；不使用含糊的`partial_failure`或裸`issue`数字。
3. 删除`camera.close_request_completed`，成功只保留`camera.closed`；设备关闭失败只保留`camera.close_failed`。
4. `camera.opened`保留设备数量和最终曝光；曝光范围只在相机实际调整值或出现失败时追加，正常`adjusted=0`不输出。
5. 自动打开、停止恢复和模板取景前重复应用相同曝光时不记录成功日志。用户明确应用曝光/增益时记录一次最终值，应用失败保留`nativeCode`和原因。
6. 模板同步取景的成功、拒绝和启动/停止失败由外层`template.preview_*`唯一记录；删除下层`camera.preview_started/stopped`和同步`camera.preview_start_*`日志。取景已经启动后发生的异步`camera.preview_failed`属于设备根因，继续保留错误码和原因，但删除内部会话ID。
7. PLC连接、断开、运行参数写入、结果写入和失败是独立硬件操作，继续保留；UI不重复记录相同结果。

### 15.5 Run和单帧

1. 同步启动失败统一为：

```text
event=run.start_failed code=<语义码> reason=<用户可读原因> diagnostic=<按需底层原因>
```

2. 删除同步失败链中的`run.start_rejected`、`run.commit_rejected`、`run.start_rolled_back`、`detection.start_rejected`以及已经完整返回给最终摘要的相机准备/采集启动辅助日志。异步运行期设备根因不受此条影响。
3. 保留每个实际完成帧的一条`frame.completed`，字段固定为`mode/result/ms`，按需追加`template/score/text/reason`；删除逐帧`total/ng`。
4. `mode`统一使用与`run.started`相同的稳定名称，不再混用`word`和`word_detection`。
5. `run.stopped`固定输出Run计数`accepted/completed/cancelled`和现有界面累计统计`total/ng`。软件触发时`total/ng`不声明为本次Run独占统计；直接复用已有Runtime和ResultService数据，不增加新的运行级计数器或日志聚合器。
6. 用户正常停止造成的`frame.cancelled`和停止后的`frame.completion_ignored`不逐条记录；ROI非法、处理异常等非正常取消仍保留一条明确WARN/ERROR。
7. 删除`CharacterGlyphMatcher::match()`中的逐帧`character_match.invalid_input`；把必要原因随现有检测结果传递到最终帧摘要，不记录算法内部调用过程。
8. `frame.rejected`、`presentation.submit_failed/dispatch_failed`只在仍处于有效运行且确实影响业务时记录；正常停止/邮箱取消路径不输出。不得为此增加通用限流器。

### 15.6 存图

1. `image_save.completed`继续按实际文件记录路径、格式和字节数，方便直接定位图片。
2. 标注图为空且最终没有可提交项时，只输出一条`image_save.submit_failed reason=empty_annotated_image`；删除同一次操作的`item_skipped + no_items`组合。
3. 后台任务写盘失败只由`image_save.failed totalFailed reason`记录；删除`image_save.failure_presented`，UI显示同一错误不是新的业务事件。
4. 磁盘持续失败时复用现有`totalFailed`计数，只在本次进程首次失败时输出详细ERROR，后续失败只更新已有计数；不增加恢复检测、定时器、重试或通用抑制器。
5. 队列拒绝、空目录和服务不可用分别保留一条最终提交失败，不再由上层重复记录。

### 15.7 设置和模板

1. `settings.saved`保留`key`并增加实际`value`；布尔、模式和枚举使用可读文本，不记录裸索引。
2. `image.save_path`不在`QLineEdit::textChanged`产生逐字符成功日志。使用浏览按钮最终选择路径时只保留一条`image_save.directory_selected path=...`；底层保存失败仍记录一次。
3. 删除`templates.restore_requested`和`template.capture_action_requested`，因为只说明发起请求且后面已有或应有最终结果。
4. 在现有模板业务边界补齐且只补齐以下最终事件：`templates.restored mode count`、`template.saved name path`、`templates.selection_updated count`、`template.applied name/path`、`template.preview_started/stopped`；失败使用对应`*_failed`并带原因。
5. 不记录模板候选、每个候选分数、中间坐标、文件枚举过程或普通函数进入/退出；单帧只保留最终选中模板和最终分数。

### 15.8 计划内文件

- `app/startup/application_startup.cpp`
- `app/ui/main_window.cpp`
- `app/ui/main_window.h`
- `app/ui/main_window_inspection.cpp`
- `app/ui/main_window_settings.cpp`
- `app/ui/pages/inspection_page.cpp`
- `app/ui/pages/machine_settings_page.cpp`
- `app/ui/pages/machine_settings_page.h`
- `app/ui/pages/template_editor_page.cpp`
- `app/ui/pages/template_editor_page.h`
- `app/ui/dialogs/template_selection_dialog.cpp`
- `app/runtime/camera_session.cpp`
- `app/runtime/inspection_runtime.cpp`
- `app/runtime/inspection_runtime.h`
- `app/runtime/result_service.cpp`
- `app/runtime/image_save_service.cpp`
- `app/detection/common/character_template_matcher.cpp`
- 实际承接模板最终结果的现有UI边界
- `app/engines/ocr/ocr_engine.cpp`
- 第10.3节列出的四个文档

除非实施时静态调用链证明缺少唯一最终日志，否则不扩张到其他生产文件；不得新增日志生产文件。

### 15.9 第二轮静态门禁

1. 以下成功/展示/内部事件生产引用为0：`window.ready`、启动`statistics.reset`、`ui.splitter_state_saved`、`template.capture_action_requested`、`templates.restore_requested`、`camera.close_request_completed`、`camera.open_request_plc_ready`、`camera.open_request_plc_failed`、`camera.open_request_completed`、同步`camera.preview_started/stopped`、`run.start_rolled_back`、`image_save.failure_presented`、逐帧`character_match.invalid_input`。
2. `frame.completed`消息不含`total/ng/RunId/sequence/frame/session`，`run.stopped`包含`accepted/completed/cancelled/total/ng`。
3. 模板最终成功/失败事件覆盖恢复、保存、选择更新、应用和预览；模板候选和算法内部日志为0。
4. 日志正文中的裸`status/issue/state/field/mode`数字和成功事件中的固定无意义布尔字段为0；设备原生错误码不受此限制。
5. `ApplicationLogger`、崩溃处理、类别定义、工程清单和保留算法无行为差异；不新增日志包装、去重、采样、定时器、线程或配置代码。
6. `git diff --check`通过；不覆盖、回退、暂存或提交用户现有修改。

### 15.10 Release人工验收

1. 启动到退出只出现一组`log.started/app.start/settings.loaded/services.assembled/app.ready/app.stop`及实际失败事件，不出现`window.ready`和启动统计重置成功日志。
2. 相机打开成功只出现PLC连接结果和`camera.opened`；PLC失败时不再出现UI层重复错误；关闭成功只出现`camera.closed`。
3. 连续检测代表帧每帧只有一条最终摘要且无累计统计，停止时只有一条完整`run.stopped`统计；正常取消不刷WARN。
4. 分别验证一次Run启动失败、模板恢复/保存/选择/取景、设置修改、目录选择、存图成功和存图失败，确认每个业务操作只有一条最终结果且字段可读。
5. 验证硬件触发时PLC结果写入仍独立记录，异步相机/PLC故障仍能同时看到根因和一次`run.fault`，不得因去重丢失关键故障链。
6. 确认文件和Qt Creator输出内容一致，旧日志目录不变，日志时间无`T`和时区后缀；五种检测模式、设备、模板、设置、统计、存图和UI行为无回归。

### 15.11 第二轮实施结果

1. 启动和相机同步操作已收口到唯一最终事件；`app.ready`不再带窗口内部状态，相机打开/关闭成功只由设备边界记录，模板同步取景由`template.preview_*`记录。
2. `frame.completed`已移除逐帧`total/ng`并统一稳定模式名；正常停止期间的取消和过期结果不逐帧告警，`run.stopped`集中输出`accepted/completed/cancelled/total/ng`。启动回滚不再伪装成一次正常Run停止。
3. 存图空标注图不再产生`item_skipped + no_items`组合；后台写盘错误只在本进程首次失败时记录详细ERROR，UI继续更新失败累计但不重复写日志。
4. 设置成功日志增加可读`value`，`image.save_path`不再逐字符输出成功日志；模板恢复、保存、选择更新、应用和取景均在现有业务边界记录最终成功或失败，未新增日志生产文件。
5. 第二轮实际扩张到`template_selection_dialog.cpp`，原因是静态调用链证明模板选择保存失败只在该对话框内可见；该处只补最终成功/失败日志，没有改变对话框保存、移除或关闭行为。
6. 第15.9节禁止事件均为0，`frame.completed`与`run.stopped`字段静态门禁通过，日志正文中的裸内部状态数字已清理；`ApplicationLogger`、类别、保留、崩溃处理和工程清单无第二轮行为修改，未新增包装器、配置、线程、定时器或通用去重状态。
7. `git diff --check`通过；未执行qmake、构建、测试程序、主程序、真实相机/PLC或故障注入，运行结论等待用户在Qt Creator使用Release统一验证。
