# OCRGangYin 二维码结果本机 CSV 直写主程序替换方案

## 1. 状态、基线与权威范围

- 状态：代码实施完成，待用户统一验证。
- 方案日期：2026-09-03。
- 代码基线：`4645617e54387a9dc3262e268abe6b68d82cc843`，提交主题为 `refactor: 拆分主窗口 UI 并落地右侧折叠导航`。
- 工作区保护：实施前已确认生产 `app/` 和暂存区无差异；既有计划索引修改及本方案文档均已保留，未覆盖、回退、暂存或提交用户资产。
- 权威范围：主程序 `app/` 内“二维码+三期”最终结果从远程 TCP 传输改为本机每日 CSV 直接写入，以及由此产生的设置、UI、运行配置、Fault、关闭流程、工程依赖和旧代码删除。
- 当前进度：Schema 6、本机 CSV 设置 UI、启动目录准备、逐产品直接追加、写入失败 Fault、旧网络链删除、工程与文档同步均已实施；第 14.1 节静态验收通过。
- 验证门禁：Agent 未运行 qmake、编译、主程序或测试目标；Qt Creator Release 构建和第 14.2 节人工/现场验证由用户统一完成。

本计划只处理主程序。以下目录本轮明确不读取其行为作为新实现依赖、不修改、不删除、不重命名，也不更新其工程或部署文件：

~~~text
tools/result_receiver/
~~~

该目录即使暂时保留，也不得继续被主程序连接、调用、配置或作为本机 CSV 写入链的一部分。以后是否删除或改造，由独立任务决定。

## 2. 替代关系与实施原则

本计划在主程序范围内替代以下方案的目标行为：

- `OCRGangYin二维码结果TCP传输与CSV接收方案.md` 中主程序发送端、TCP、ACK、连接设置、发送 Fault 和关闭协调；
- `OCRGangYin二维码结果发送端内存队列精简方案.md` 中内存 FIFO、在途记录、ACK 删除和退出未发送提示；
- `OCRGangYin二维码结果传输启用策略持久化与连接解耦方案.md` 中远程传输启用政策、TCP 启动门禁和 `resultExport` 命名；
- `OCRGangYin二维码结果远程连接状态UI优化方案.md` 中连接状态、PING/PONG 延迟和远程状态 UI。

上述历史方案正文保留用于追溯，不再约束本计划批准范围内的主程序终局。`tools/result_receiver/` 相关内容不在本计划替代范围内。

实施必须遵守以下原则：

1. 直接替换，不保留远程传输和本机 CSV 两套路径。
2. 不保留旧类空壳、固定返回值、别名、转发方法、兼容字段、旧 Schema 读取、迁移分支或特性开关。
3. 不新增 Writer、Store、Repository、Manager、Adapter、队列类或后台线程；CSV 是 `ResultService` 唯一使用的一次性本地副作用，原位实现为私有函数。
4. 不引入 JSONL、SQLite、数据库、磁盘 outbox、内存待写队列、自动补写、备用目录、批量同步或历史修复工具。
5. 不对项目自身生成的二维码结果重复校验，不重新判断二维码或三期算法结果。
6. 不修改二维码+三期算法、其他四种模式、相机、模板、图像保存、正常产品 PLC 规则和统计口径；只调整启用本机 CSV 时的正式提交边界。

## 3. 当前代码事实

### 3.1 最终结果入口

当前 `ResultService::process()` 已经是产品最终结果唯一出口，依次处理：

~~~text
completion 合法性
    -> claimResult()
    -> 远程发送记录构造
    -> finalizeResultClaim()
    -> ResultExportClient::enqueue()
    -> 延迟 NG、统计、图像渲染、异步存图、正常 PLC、界面呈现
~~~

本机 CSV 必须在该出口写入，不能接入二维码解码器、Pipeline 中间步骤、UI 信号或日志回调。

### 3.2 当前二维码数据

二维码+三期 Pipeline 已把二维码原文保存在 `DetectionResult::qrContent`，整体结果保存在 `DetectionResult::verdict`：

- `AlgorithmVerdict::Ok`：二维码和三期整体通过；
- `AlgorithmVerdict::Ng`：二维码不可读、三期不通过或其他正常算法 NG；
- Worker、运行时或设备异常：系统 Fault，不形成普通产品 CSV 行。

本计划不改变这些字段的生成、含义或生命周期。

### 3.3 当前远程发送残留

主程序当前仍包含：

- `ResultExportClient`、`ResultExportNetworkWorker`；
- `ResultExportRecord`、`ResultExportConnectionState`；
- 当前进程内存 FIFO、单条在途 ACK；
- 接收电脑 IP、端口、连接、断开、PING/PONG 和往返延迟；
- `ResultExportUnavailable` Fault 及 TCP 恢复条件；
- 退出时未发送数量提示；
- qmake 的 Qt Network 依赖。

以上内容在本计划实施后必须从 `app/` 端到端删除。

## 4. 最终用户行为

### 4.1 启用政策

本机 CSV 保留一个明确的持久化开关：

| 场景 | 行为 |
|---|---|
| 非“二维码+三期”模式 | 不显示本机 CSV 设置，不写 CSV |
| “二维码+三期”且未启用 | 正常检测，不要求输出目录，不写 CSV |
| “二维码+三期”且已启用、目录可准备 | 正常开始，本轮每个正式结果直接写本机 CSV |
| 启用时尚未选择目录 | 拒绝保存启用状态，复选框恢复未启用，并提示先选择目录 |
| 已启用但开始时目录无法创建 | 拒绝本次开始，不进入 Running |
| Running 中 CSV 打开、写入或 `flush` 失败 | 立即进入 `BarcodeCsvUnavailable` 系统 Fault |

开关和目录都只保存在 `AppSettings`。MainWindow、RuntimeSnapshot 和 InspectionRuntime 不维护第二份正式状态。

### 4.2 设置界面

原“二维码结果传输”组原位替换为：

~~~text
二维码结果本机记录

[ ] 启用本机 CSV 记录
输出目录  [只读绝对路径________________] [选择目录]
~~~

固定要求：

- 输出目录输入框只读，只能通过现有 `QFileDialog::getExistingDirectory()` 选择，避免引入未应用文本状态；
- 选择目录后立即通过现有 `SettingsApplicationService::saveConfiguration()` 保存；保存失败时 UI 保持正式设置值；
- 切换启用复选框后立即保存；保存失败时用 `QSignalBlocker` 恢复正式值；
- 未选择目录时勾选启用，保存被拒绝并恢复未勾选；
- Starting、Running、Stopping 或 Fault 时，复选框和选择目录按钮不可修改；
- 删除 IP、端口、连接、断开、连接状态、延迟和所有对应布局；
- 不新增“测试写入”“打开目录”“立即导出”“同步历史”等按钮。

### 4.3 恢复默认设置

主程序执行现有“恢复默认设置”时，本机 CSV 两个字段恢复为：

~~~text
barcodeCsvEnabled = false
barcodeCsvOutputDirectory = ""
~~~

不保留旧的接收端地址或端口默认值。

## 5. AppSettings 与严格 Schema 6

### 5.1 最终字段

`AppSettings` 删除：

~~~cpp
bool resultExportEnabled;
QString resultExportReceiverIp;
int resultExportReceiverPort;
~~~

直接增加：

~~~cpp
bool barcodeCsvEnabled;
QString barcodeCsvOutputDirectory;
~~~

不增加嵌套 C++ 设置类型，不保留 `resultExportEnabled` 别名或访问器。

### 5.2 JSON 合同

`AppSettings::CurrentSchemaVersion` 从 5 升级为 6。根对象删除 `resultExport`，增加：

~~~json
"barcodeCsv": {
  "enabled": false,
  "outputDirectory": ""
}
~~~

严格规则：

- `barcodeCsv.enabled` 必须是布尔值；
- `barcodeCsv.outputDirectory` 必须是字符串；
- 空目录只允许与 `enabled=false` 组合；
- 非空目录必须是绝对路径；
- `enabled=true` 时目录必须非空；
- 只做设置值约束，不在 AppSettings Store 中创建目录或测试磁盘写入；
- Schema 5 按现有 `SETTINGS_RESET_REQUIRED` 流程整体重置为 Schema 6 默认值；
- 不迁移 Schema 5 的启用值、IP 或端口，不读取 `resultExport`，不兼容旧键，不双写两个分区。

升级后旧设置整体重置是“不保留旧代码兼容性”的直接结果。实施和部署验证时必须重新确认相机、PLC、模板、存图路径及本机 CSV 目录等正式参数。

## 6. 本轮运行配置

`ResultServiceRunConfiguration` 删除：

~~~cpp
bool resultExportEnabled;
~~~

增加两个直接字段：

~~~cpp
bool barcodeCsvEnabled = false;
QString barcodeCsvOutputDirectory;
~~~

`InspectionApplicationService` 从同一份 `AppSettings` 生成本轮配置：

~~~text
barcodeCsvEnabled
    = settings.barcodeCsvEnabled
      && mode == DetectionMode::BarcodeWord

barcodeCsvOutputDirectory
    = barcodeCsvEnabled
      ? settings.barcodeCsvOutputDirectory
      : 空字符串
~~~

开始成功后两个字段在本轮保持冻结。运行期间磁盘设置文件发生变化，不改变当前运行。

不增加单字段或双字段包装结构，不向 StartInspectionCommand、RuntimeSnapshot 或 InspectionRuntime 复制配置。

## 7. 开始识别门禁

`InspectionApplicationService::start()` 删除 TCP Connected 检查，改为唯一的本机目录准备：

~~~text
模式不是 BarcodeWord
    -> 不处理本机 CSV

模式是 BarcodeWord 且 barcodeCsvEnabled=false
    -> 正常继续

模式是 BarcodeWord 且 barcodeCsvEnabled=true
    -> 读取已保存的绝对输出目录
    -> QDir().mkpath(outputDirectory)
       -> 成功：继续现有启动流程
       -> 失败：返回结构化开始失败，拒绝启动
~~~

固定开始失败代码与提示：

~~~text
BARCODE_CSV_DIRECTORY_UNAVAILABLE
二维码 CSV 输出目录不可用，请重新选择可写入的本机目录。
~~~

这里不创建测试文件、不写临时探针、不打开当天 CSV。目录实际可写性由第一条真实结果写入确认；若外部权限、文件占用或磁盘状态导致失败，统一进入第 9 节运行时 Fault。

## 8. CSV 文件合同

### 8.1 文件路径与日期

输出文件固定为：

~~~text
<barcodeCsvOutputDirectory>/qr_results_YYYYMMDD.csv
~~~

- 日期取该产品在 `ResultService::process()` 中准备写入时的本机日期；
- 同一天持续追加同一文件；
- 跨日本机日期后，下一件产品自然写入新日期文件；
- 程序重启后继续追加已有当日文件，不覆盖既有行；
- 不创建日期子目录，不增加运行 ID 文件名或时间戳文件名。

### 8.2 内容映射

保持现有 CSV 可见合同，只改变写入电脑：

~~~text
result.verdict == AlgorithmVerdict::Ok
    -> 写 result.qrContent 原文

result.verdict == AlgorithmVerdict::Ng
    -> 写固定文本 noQR
~~~

三期 NG 属于整体 NG，即使 Pipeline 已经读到二维码，也继续写 `noQR`。本计划不改为“只要二维码成功就写原文”。

系统 Fault、无效 completion、无法 `claimResult()` 的重复或过期结果不写 CSV。

### 8.3 物理格式

- UTF-8；
- 新文件或现有空文件先写 UTF-8 BOM；
- 无表头；
- 每件正式产品一行；
- 行内容只含二维码原文或 `noQR`，不增加 ID、时间、OK/NG 状态、模板名或诊断列；
- 保持现有单字段 CSV 转义：包含逗号、双引号、回车或换行时整体加双引号，内部双引号写成两个双引号；
- 每行以换行结束；
- 每件产品执行一次普通 `flush()`；
- 不调用 Windows `FlushFileBuffers`，不承诺突然断电时操作系统缓存中的最后一行已经进入物理介质。

## 9. `ResultService` 直接写入与失败边界

### 9.1 最小实现

不新增生产文件。只在 `result_service.h/.cpp` 中增加私有函数，例如：

~~~cpp
bool appendBarcodeCsvResult(
    const DetectionResult &result,
    const QString &outputDirectory,
    QString *filePath,
    QString *errorMessage) const;

static QString csvEscape(QString value);
~~~

函数只做：

1. 根据一次取得的本机时间生成当日文件名；
2. 判断目标文件是否为空，以决定是否写 BOM；
3. 根据最终 verdict 选择二维码原文或 `noQR`；
4. 完成 CSV 转义；
5. 使用 `QFile` 的追加文本模式写入 BOM（如需）、本行和换行；
6. 检查写入字节数并执行一次 `flush()`；
7. 返回成功或真实文件错误。

不扫描旧记录，不读取或解析已有 CSV，不建立 ID 索引，不检测重复内容，不截断损坏尾行，不重写整个文件，不使用 `QSaveFile`，不加互斥量。唯一调用者是单个 DetectionWorker 的最终结果串行消费链，已有 `claimResult()` 负责产品级去重。

### 9.2 正式提交顺序

二维码+三期且本轮启用 CSV 时，`ResultService::process()` 改为：

~~~text
completion 合法
    -> claimResult(productKey)
    -> 复制本轮 barcodeCsvEnabled 和 outputDirectory
    -> appendBarcodeCsvResult()
       -> 成功：继续
       -> 失败：进入 BarcodeCsvUnavailable Fault 并立即返回
    -> finalizeResultClaim(productKey)
    -> 延迟 NG
    -> 统计
    -> 图像渲染
    -> 异步存图
    -> 非 Fault 时正常产品 PLC
    -> 界面呈现
~~~

未启用本机 CSV 或不是二维码+三期模式时，直接执行现有 `finalizeResultClaim()` 及后续流程，不打开任何 CSV。

CSV 成功写入是启用本机记录时的产品正式提交前置条件。这样写入失败的产品仍保留在 Runtime 的未确认集合中，不会冒充已完整提交的正常产品。

CSV 追加成功后不删除该行，也不执行文件回滚或补偿。如果随后 `finalizeResultClaim()` 因现有运行时不变量失败，保留已经写入的 CSV 行，并按现有 `RuntimeInvariantViolation` 流程进入 Fault；不得为此删除末行、重写整个 CSV 或引入事务机制。

### 9.3 运行中写入失败

以下任一真实失败统一处理：

- 当日 CSV 无法打开；
- BOM 或业务行写入字节数不完整；
- `flush()` 返回失败。

处理固定为：

1. 使用 `qCCritical(logRuntime)` 记录一条 ERROR 级运行日志，包含事件名、目标文件路径和 `QFile::errorString()`；不得调用 `qFatal()`，不得主动退出程序；
2. 调用现有 `InspectionRuntime::enterFault()`，原因使用 `InspectionFaultReason::BarcodeCsvUnavailable`；
3. 立即退出当前 `process()`；
4. 不执行当前产品的 `finalizeResultClaim()`；
5. 不增加正常总数或 NG 数；
6. 不提交当前产品图像保存；
7. 不执行当前产品正常 OK/NG PLC；
8. 不发布当前产品正常结果界面；
9. 复用现有 Fault 停止 DetectionWorker、取消结果 mailbox 和现场安全处理；
10. 操作员确认故障时复用现有 reconciliation，把未完成产品计入 Unconfirmed 并要求隔离。

固定故障原因文案：

~~~text
二维码 CSV 写入不可用
~~~

诊断信息直接显示真实失败路径和文件错误，不新增错误码映射表。

不把 CSV 写入失败改写为普通 NG，不自动切换到其他目录，不跳过该行继续生产，不在内存中保留待补写记录，不定时重试，也不在故障确认时自动补写失败产品。

### 9.4 故障恢复

删除原 `ResultExportUnavailable` 的“TCP 已连接且待确认队列为空”专用恢复门禁。`BarcodeCsvUnavailable` 使用现有通用 Fault 恢复：

~~~text
操作员处理目录、权限、磁盘空间或文件占用
    -> 停止并等待 DetectionWorker
    -> reconcileFaultProducts()
    -> 未完成产品记为 Unconfirmed 并由现场隔离
    -> acknowledgeFault()
    -> 回到 Idle
    -> 再次开始时重新执行目录准备
~~~

不新增 CSV 专用恢复状态、恢复按钮、测试接口或第二套 reconciliation。

## 10. 必须删除的主程序旧代码

### 10.1 删除文件

从 `app/` 和主工程清单删除：

~~~text
app/runtime/result_export_client.h
app/runtime/result_export_client.cpp
app/runtime/result_export_network_worker.h
app/runtime/result_export_network_worker.cpp
~~~

### 10.2 删除类型、字段和接口

主程序中以下内容必须零引用：

~~~text
ResultExportClient
ResultExportNetworkWorker
ResultExportRecord
ResultExportConnectionState
ResultExportUnavailable
resultExportEnabled
resultExportReceiverIp
resultExportReceiverPort
resultExportClient
pendingResultExportCount
requestResultExportConnect
requestResultExportConnectionCheck
requestResultExportDisconnect
resultExportConnectionState
resultExportRoundTripMs
result_export_client
result_export_network_worker
result_export_unavailable
result_export.disconnected
~~~

同时删除：

- `InspectionRuntime` 对 ResultExportClient 的 include、所有权、构造、析构顺序依赖、访问器和 `transportFailure` 信号连接；
- `RuntimeSnapshot` 对传输头文件的依赖，以及连接状态和往返延迟字段；
- ApplicationService 的连接请求、连接状态快照发布、TCP 开始门禁、TCP Fault 恢复门禁；
- MainWindow 的连接、断开、PING、延迟和网络状态 UI 逻辑；
- 退出时“尚有二维码结果未发送完成”的数量查询和确认框；
- AppSettings 的 `resultExport` JSON 分区、IP 合法性检查和 Qt Network 地址类型依赖；
- QSS 中只服务 `resultExportStatus` 的状态属性样式；
- 翻译源中的远程连接、TCP、ACK、未发送和结果接收端文案；
- 主工程 `QT += network` 以及四个旧网络文件条目。

旧代码不得以空类、空函数、固定状态、注释代码、宏开关、别名或弃用声明残留。

### 10.3 允许保留的非业务文本

`app/devices/plc/vendor/snap7.*` 等第三方 PLC 源码中的 TCP 名称与本计划无关，不得因为字符串相同而删除或修改。静态门禁必须限定在二维码结果传输业务符号和主工程 Qt Network 依赖。

## 11. 逐文件修改计划

| 文件 | 计划修改 | 最终边界 |
|---|---|---|
| `app/runtime/result_service.h` | 将运行配置改为 `barcodeCsvEnabled/barcodeCsvOutputDirectory`；删除 `resultExportEnabled()`；增加两个私有 CSV 函数声明 | 不新增 CSV 类、队列或线程 |
| `app/runtime/result_service.cpp` | 原位实现 CSV 转义和追加；在 `finalizeResultClaim()` 前写入；失败进入 `BarcodeCsvUnavailable` | 保持成功后的延迟 NG、统计、渲染、存图、PLC、呈现顺序 |
| `app/runtime/inspection_runtime.h/.cpp` | 删除 ResultExportClient 依赖、所有权、访问器和网络失败连接；Fault 枚举改为 `BarcodeCsvUnavailable` 并更新稳定名称 | 保留通用 Fault、reconciliation 和设备生命周期 |
| `app/runtime/result_export_client.h/.cpp` | 删除文件 | 不保留本地 CSV 适配壳 |
| `app/runtime/result_export_network_worker.h/.cpp` | 删除文件 | 不保留 TCP、PING/PONG 或 ACK |
| `app/application/runtime_snapshot.h` | 删除网络 include、连接状态和延迟字段 | 不增加 CSV 状态快照 |
| `app/application/inspection_application_service.h/.cpp` | 删除四个传输控制/查询接口、连接状态快照、TCP 开始与恢复门禁；生成 CSV 运行配置；开始时创建输出目录 | 不增加 CSV Service 或跨层 setter |
| `app/system_support/settings/app_settings.h/.cpp` | Schema 升至 6；旧三字段替换为本机 CSV 两字段；更新默认值和相等比较 | 不保留旧字段或迁移数据 |
| `app/system_support/settings/app_settings_store.cpp` | 严格读写 `barcodeCsv`；删除 `resultExport` 和 QHostAddress/QAbstractSocket 校验；增加最小路径组合校验 | Store 不创建目录、不写 CSV |
| `app/system_support/settings/app_settings_store.h` | 仅在现有 Schema 或职责说明确有旧内容时同步更新 | 不新增接口 |
| `app/system_support/machine_settings_policy.cpp` | 恢复默认值改为本机 CSV 两字段 | 不保留 IP/端口默认值 |
| `app/ui/main_window/settings/detection_settings_page.ui` | 原位替换远程传输组为复选框、只读目录和选择按钮 | 不增加测试、同步、历史或状态控件 |
| `app/ui/main_window/main_window.h/.cpp` | 删除远程槽和状态方法；增加本机 CSV 启用、目录选择和最小 UI 状态逻辑 | 不维护第二份设置状态 |
| `app/ui/main_window/main_window_settings.cpp` | 恢复默认和设置回填改为本机 CSV 字段 | 保持现有设置提交边界 |
| `app/ui/main_window/main_window_inspection.cpp` | 删除退出时待发送记录提示 | 保持其他关闭顺序 |
| `app/ui/main_window/inspection/inspection_fault_presenter.cpp` | 将远程传输 Fault 呈现替换为本机 CSV 写入 Fault | 复用通用现场安全和确认文案 |
| `app/resource/qss/app_theme.qss` | 删除只服务网络连接状态标签的样式；按现有输入框和按钮样式呈现新控件 | 不新增独立视觉体系 |
| `app/resource/Translate_CN.ts`、`app/resource/Translate_EN.ts` | 删除远程传输字符串并加入最终本机 CSV 字符串 | 不保留废弃文案 |
| `app/AutoOCRproject.pro` | 删除 Qt Network 和四个旧文件条目 | 不新增生产文件条目 |
| `app/system_support/README.md` | 将设置 Schema 和结果输出说明更新为本机 CSV | 只描述最终状态 |

实施结果：修改 23 个现有主程序文件，删除 4 个旧网络文件，不新增生产代码文件。

## 12. 明确不做

- 不修改 `tools/result_receiver/` 下任何文件；
- 不删除或构建 ResultReceiver；
- 不通过 `127.0.0.1`、本机 IP 或 loopback 继续运行 TCP；
- 不保留远程传输开关作为本机 CSV 的兼容入口；
- 不迁移 Schema 5 设置；
- 不保留 IP、端口、连接状态、ACK、FIFO 或 pending count；
- 不写 JSONL 后再生成 CSV；
- 不把 CSV 整体读入内存后原子重写；
- 不做自动修复、尾行截断、重复内容扫描或历史去重；
- 不做跨进程恢复或失败记录补发；
- 不增加 CSV 表头、额外列、日期子目录或多种格式选项；
- 不改变三期 NG 写 `noQR` 的既有映射；
- 不改变其他模式、算法、模板、相机、PLC、统计、图像保存和主界面其他区域；
- 不顺手实施其他待授权 UI 或架构计划。

## 13. 实施顺序

1. 记录实施时 HEAD、分支、工作区和暂存区，确认并保护所有计划外既有用户修改。
2. 将 AppSettings 一次性升级到严格 Schema 6，完成新字段、默认值、校验、读写和恢复默认链。
3. 将检测设置页远程传输组原位替换为本机 CSV 开关、只读目录和选择按钮，接通现有设置保存失败回退。
4. 将 `ResultServiceRunConfiguration` 和 ApplicationService 启动链切换为本机 CSV 配置，并删除 TCP 开始门禁。
5. 在 `ResultService` 原位实现直接追加和 CSV 失败 Fault，固定写入与正式提交顺序。
6. 删除 Runtime、ApplicationService、RuntimeSnapshot、Fault 恢复和关闭流程中的网络状态与队列链。
7. 删除四个网络生产文件，从 qmake 移除文件及 Qt Network 依赖。
8. 清理 QSS、翻译、README、旧业务符号和旧文案，不触碰 `tools/result_receiver/`。
9. 执行第 14.1 节静态验收。
10. 同步计划索引、现有功能对照表和重构执行记录的最终实施状态。
11. 交付用户执行 Qt Creator Release 构建和第 14.2 节人工/现场验证。

## 14. 验收计划

### 14.1 Agent 静态验收

| 检查项 | 通过条件 |
|---|---|
| 计划范围 | `tools/result_receiver/` 无差异；无范围外生产文件修改 |
| 工作区保护 | 实施前记录的计划外既有用户修改保持，不被覆盖、回退、删除、暂存或提交 |
| Schema | 只有 Schema 6 和 `barcodeCsv.enabled/outputDirectory`；`app/` 中旧 `resultExport` 字段零引用 |
| 设置状态 | AppSettings 是启用和目录唯一正式值；MainWindow/Runtime 无重复缓存 |
| UI | 只有启用、只读目录和选择按钮；IP、端口、连接、断开、状态、延迟控件零引用 |
| 运行配置 | 只有 `barcodeCsvEnabled/barcodeCsvOutputDirectory` 两个直接字段；无包装结构 |
| 结果入口 | CSV 只从 `ResultService::process()` 写入；Pipeline 和 UI 无文件写入 |
| 提交顺序 | `claimResult()` 后写 CSV，写入成功后 `finalizeResultClaim()`，失败立即 Fault 并返回；已成功追加的 CSV 行不回滚或补偿 |
| CSV 格式 | 每日文件、UTF-8 BOM、无表头、单字段、OK 原文、NG `noQR`、标准转义和每行 flush |
| Fault | CSV 失败使用 `qCCritical(logRuntime)` 记录 ERROR，不调用 `qFatal()`；`BarcodeCsvUnavailable` 使用通用 Fault 与 reconciliation；没有专用恢复状态或重试队列 |
| 关闭 | 无 pending 数量、未发送提示或传输 shutdown |
| 网络删除 | 四个网络文件从磁盘和 qmake 删除；主程序无 Qt Network 业务依赖 |
| 旧符号 | 第 10.2 节符号与远程业务文案在 `app/` 中零引用 |
| 工程质量 | `.pro` 路径存在且无重复，UI XML 有效，严格 UTF-8、有末尾换行，`git diff --check` 通过 |

Agent 不运行 qmake、编译、测试程序或主程序。

### 14.2 用户 Qt Creator 与人工验证

1. 使用旧 Schema 5 设置启动新版本，确认按现有策略重置为 Schema 6 默认设置；重新配置相机、PLC、模板、存图和本机 CSV 参数。
2. 切换到其他四种模式，确认本机 CSV 组隐藏、检测行为不变且不生成 CSV。
3. 切换到二维码+三期，未选择目录时勾选启用，确认提示并恢复未启用。
4. 选择本机绝对目录，退出重启，确认目录恢复；启用状态保存和恢复正确。
5. 删除已保存目录后开始，确认程序能够重新创建目录；将父目录设为不可创建时，确认拒绝启动。
6. 本地记录未启用时检测代表性产品，确认不产生 CSV。
7. 启用后检测整体 OK，确认当日 CSV 创建、UTF-8 BOM、无表头且写入二维码原文。
8. 检测二维码 NG，确认新增一行 `noQR`。
9. 检测二维码成功但三期 NG，确认仍新增一行 `noQR`。
10. 使用含逗号或双引号的可控二维码样本，确认 Excel 打开后仍为一个单元格且内容正确。
11. 同日连续运行和程序重启，确认继续追加而不覆盖；跨本机日期后确认创建新的每日文件。
12. 运行中制造文件不可写、磁盘不可用或真实写入失败，确认进入“二维码 CSV 写入不可用”Fault；当前产品不计正常统计、不执行正常 OK/NG PLC、不呈现正常结果。
13. 执行现有故障确认，确认未完成产品记为 Unconfirmed，并按提示隔离；修复目录后重新开始并正常写入。
14. 回归软触发、硬触发、Stop、关闭、统计、图像保存、正常 PLC 和五模式切换。

## 15. 完成条件

只有同时满足以下条件，才能把本计划标记为完成：

1. 用户另行明确授权生产代码实施；
2. 第 11 节计划内主程序修改完成，`tools/result_receiver/` 保持无差异；
3. 主程序远程传输类型、设置、UI、网络线程、ACK、队列、Fault 恢复和退出链端到端删除，无兼容或胶水残留；
4. 本机 CSV 只有 `ResultService` 私有直接写入实现，没有新增生产抽象；
5. 第 14.1 节静态验收通过；
6. 用户完成 Qt Creator Release 构建及第 14.2 节人工和现场验证；
7. 计划索引、功能对照表和重构执行记录同步最终状态；
8. 最终提交只包含本计划及直接必要修改，不覆盖或提交实施前用户资产，不自动推送或改写历史。
