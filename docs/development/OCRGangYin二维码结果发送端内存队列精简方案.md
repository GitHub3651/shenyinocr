# OCRGangYin 二维码结果发送端内存队列精简方案

## 1. 状态、基线与范围

- 状态：生产代码已实施，Agent 静态门禁通过，待用户统一验证。
- 方案日期：2026-09-01；生产代码实施日期：2026-09-02。
- 代码基线：`04aea48354012f48845cd4bb751c0fba31b6f342`，提交主题为 `feat: 增加二维码结果远程传输与接收端`。
- 权威范围：二维码+三期结果发送端的产品提交、待确认队列、重连、Fault 恢复、退出流程和相关 UI 状态。
- 目标：彻底删除发送端磁盘 outbox，改用当前进程内存 FIFO；保留产品 ID ACK、接收端幂等、JSONL、CSV 和异步 TCP。
- 当前进度：12 个计划内生产文件已完成内存 FIFO 精简、ACK 信号收口、产品提交顺序调整和旧发送链删除；原方案替代说明、计划索引和执行记录已同步。Agent 未执行 qmake、编译、运行或现场验证。
- 验证门禁：等待用户统一执行 Qt Creator Release 构建和第 7.2 节人工验证；验证反馈前不把本方案标记为完成。

本方案是 `OCRGangYin二维码结果TCP传输与CSV接收方案.md` 的发送端队列专项替代方案。冲突时只覆盖发送端磁盘 outbox、产品提交边界、重连处置、关闭清理和相关状态；原方案中的协议、接收端、ACK、JSONL、CSV、部署和结果字段继续有效。

实施采用原位直接替换。旧磁盘 outbox 与新内存 FIFO 不得并存，不保留旧接口空壳、转发包装、别名、兼容层、迁移代码、特性开关或跨层胶水。与本方案无关的 UI、模板、相机、PLC、算法、存图、日志和其他模式保持不变。

## 2. 最终行为与可靠性边界

现场网线实验表明，正常生产时 ACK 往返稳定，待确认记录通常为 `0`，短时只会出现少量记录。旧 outbox 在下一次启动时直接删除，不提供跨进程恢复；继续维护 `QSaveFile` 完整快照、双 mutex、启动清理和人工处置不能获得相匹配的可靠性。

最终链路为：

~~~text
产品正式提交
    -> 追加当前进程内存 FIFO
    -> 网络线程异步发送队首
    -> 接收端写入 JSONL
    -> 返回匹配产品 ID ACK
    -> 发送端删除队首
~~~

当前进程内采用：

~~~text
发送端至少一次投递
    +
内存 FIFO 保留未确认记录
    +
接收端按产品 ID 幂等
    +
JSONL 中同一 ID 只记录一次
~~~

核心约束：

- 同一时刻只允许一条产品记录等待 ACK；
- TCP 或 ACK 失败时保留队首和后续记录，当前连接内不自动重试；
- 操作员手动重连并通过首次 PING/PONG 后自动重发队首；
- 程序退出后内存 FIFO 不恢复；
- 不设置磁盘队列、离线数据库或通用消息框架。

明确不保证：

- 进程崩溃、Windows 重启或断电后的未确认记录恢复；
- 操作员确认退出后的未确认记录补发；
- TCP 层绝对只发送一次；
- 接收电脑突然断电时最后一条普通 `flush` 已进入物理介质；
- 未启用结果传输时仍保存待发送记录。

程序启动后 FIFO 必然为空，不扫描、不创建、不删除、不迁移旧的 `<AppDataLocation>/result_export/outbox.jsonl`。历史文件如需删除，由部署人员在生产代码之外处理。

保持不变：

- 产品格式 `id/time/overallOk/qrContent` 和 ACK 格式 `{"ack":"<id>"}`；
- 新 ID 在 JSONL 追加和普通 `flush` 成功后 ACK；
- 重复 ID 不重复追加，直接返回相同 ACK；
- JSONL 失败时不 ACK，CSV 不参与 ACK；
- PING/PONG、接收端单客户端、日期、残行修复、去重和手动 CSV。

实施完成后，“outbox”只用于历史文档；生产代码和当前文档统一使用“待确认队列”或“内存 FIFO”。

## 3. 产品提交与 FIFO 合同

### 3.1 唯一提交边界

二维码+三期启用结果传输时：

~~~text
最终 DetectionResult
    -> claimResult()
    -> 构造 ResultExportRecord
    -> finalizeResultClaim()
    -> 产品正式提交
    -> enqueue(record)
    -> 到期延迟 NG PLC
    -> 统计
    -> 界面图像渲染
    -> 提交异步存图
    -> 非 Fault 时执行正常产品 PLC
    -> 界面呈现
~~~

本次只调整 `finalizeResultClaim()` 和 `enqueue(record)` 的相对顺序。`enqueue(record)` 之后继续严格保持现有 `ResultService::process()` 的副作用顺序，不调整到期延迟 NG、统计、渲染、异步存图、正常产品 PLC 或界面发布的先后关系。

`finalizeResultClaim()` 是唯一产品提交边界：

- finalize 失败时不入队，继续按现有 `RuntimeInvariantViolation` 处理；
- finalize 成功后不再为内存入队建立提交、回滚或 reconciliation 语义；
- 内存入队不返回业务错误，不映射任何 Runtime Fault；
- 内存分配失败属于进程级失败，不设计业务回退。

`ResultExportRecord` 由 `ResultService` 直接生成：

~~~json
{
  "id": "<runId>:<sequence>",
  "time": "<UTC ISO-8601 with ms and Z>",
  "overallOk": true,
  "qrContent": "<二维码原文>"
}
~~~

整体 NG 时使用 `overallOk=false` 和空 `qrContent`。记录不是外部输入，删除 `ResultExportRecord::isValid()` 及其错误分支。其他模式、未启用传输、系统 Fault 和重复 completion 不创建记录。

### 3.2 最终接口和字段

`InspectionRuntime` 继续持有唯一 `ResultExportClient`。客户端继续惰性创建唯一网络线程和 `ResultExportNetworkWorker`，不新增队列类或适配器。

最终公开接口：

~~~cpp
int pendingCount() const;
void enqueue(const ResultExportRecord &record);

void requestConnect(const QString &ip, quint16 port);
void requestConnectionCheck();
void requestDisconnect();

ResultExportConnectionState connectionState() const;
double roundTripMs() const;
~~~

保留信号：

~~~text
connectionStateChanged
transportFailure
~~~

最终字段：

~~~text
m_mutex
m_pendingRecords
m_state
m_roundTripMs
m_sendInFlight
m_networkThread
m_worker
~~~

不保留 `hasPending()`、公开 `shutdown()` 或 `m_shutdown`。

### 3.3 入队、发送和 ACK

`enqueue()` 只在 `m_mutex` 保护下执行 `m_pendingRecords.push_back(record)`，释放锁后调用 `sendHeadIfPossible()`。不校验记录，不检查 shutdown，不处理目录、文件、快照或错误字符串。

`sendHeadIfPossible()` 仅在以下条件全部满足时发送：

- worker 已创建；
- 状态为 `Connected`；
- `m_sendInFlight=false`；
- FIFO 非空。

发送时复制队首并设置 `m_sendInFlight=true`，通过 `Qt::QueuedConnection` 交给 worker。

`ResultExportNetworkWorker` 是 ACK 格式、响应类型和产品 ID 匹配的唯一校验层。worker 只有在 ACK ID 与当前 `m_waitingAckId` 一致时才清除等待状态并发出无参数的 `ackReceived()`；ACK 不匹配、坏响应和超时均由 worker 关闭连接并报告失败。

`ResultExportClient::handleWorkerAck()` 不接收产品 ID，不再次比较 FIFO 队首 ID，也不保留 `m_sendInFlight`、队列为空或 ID 不一致时静默返回的业务分支。旧清空、放弃和关闭丢弃接口删除后，在途 ACK 对应的队首不会被业务代码替换；客户端收到 `ackReceived()` 后只在 `m_mutex` 保护下删除队首并清除 `m_sendInFlight`，释放锁后调用 `sendHeadIfPossible()` 发送下一条。不再存在 ACK 后磁盘快照清理失败。

FIFO 不设置上限或淘汰：

- 正常队列接近空；
- 网络失败会进入 `ResultExportUnavailable` 并停止继续接受新产品；
- Fault 前已接受的少量帧仍可完成并入队；
- 不为假设中的大规模离线积压增加策略。

待确认数量只保留 `pendingCount()`。关闭提示使用准确数量，Fault 恢复判断 `pendingCount() == 0`。

## 4. 连接、Fault、退出与 UI

### 4.1 连接和重连

连接状态只保留：

~~~text
Disconnected
Connecting
Checking
Connected
~~~

~~~text
操作员连接
    -> TCP connected
    -> 自动 PING/PONG
    -> Connected
    -> 自动发送 FIFO 队首
~~~

主动断开时保留 FIFO。网络恢复后由操作员手动连接，不出现同步、放弃或历史处置界面。

继续保持产品 ACK pending 时不执行 PING、已连接空闲时允许手动 PING、连接 3 秒、PING/PONG 3 秒、ACK 1.5 秒。不增加周期性心跳、自动重连或 KeepAlive 调整。

### 4.2 ResultExportUnavailable 与恢复

启用传输且处于生产阶段时，以下网络错误继续进入 `ResultExportUnavailable`：

- TCP 异常断开或 socket 错误；
- 产品 JSON 未完整交给 `QTcpSocket::write()`；
- ACK 超时、ID 不匹配；
- 响应 JSON 损坏或类型未知。

失败时 worker 关闭 socket，客户端清除 `m_sendInFlight` 并保留全部 FIFO；已接受帧按现有 Stopping 合同完成。`ResultExportUnavailable` 不再表示任何本地文件问题。

Fault 恢复要求：

~~~text
手动重连
    -> PING/PONG 成功
    -> 自动重发
    -> 全部获得匹配 ACK
    -> Connected && pendingCount() == 0
    -> 允许确认 Fault
~~~

正常 Stop 不断开 TCP，FIFO 继续发送。主动 Stop 完成后的纯网络错误只更新连接状态和日志，不新建 Fault。

### 4.3 退出和唯一析构所有权

~~~text
关闭请求
    -> 停止并等待检测线程
    -> pendingResultExportCount()
       -> 0：继续退出
       -> N > 0：提示退出将丢失 N 条记录
          -> 取消：保持程序打开，FIFO 继续发送
          -> 确认：继续退出，FIFO 随进程销毁
~~~

关闭流程不调用清空、丢弃或结果传输 shutdown 接口，不等待 ACK，不处理文件。

`ResultExportClient` 析构函数是 socket、worker 和网络线程的唯一最终清理所有者：

~~~text
InspectionRuntime 停止并等待 DetectionWorker
    -> ResultService 销毁
    -> ResultExportClient 析构
       -> worker 和网络线程未创建：直接结束析构
       -> worker 和网络线程已创建：
          -> worker stop
          -> QThread quit / wait
~~~

客户端保持惰性创建网络资源，因此程序从未发起连接时，`m_worker` 和 `m_networkThread` 可以均为空。析构函数必须先判断两者是否已经创建；只有两者均已创建时，才通过现有阻塞调用执行 worker `stop()`，随后执行线程 `quit()` 和 `wait()`。这个存在性分支是惰性创建所必需的生命周期逻辑，不是兼容层、公开关闭入口或重复 shutdown 防护，不得据此保留 `m_shutdown`。

保持头文件中先声明 `m_resultExportClient`、后声明 `m_resultService`，从而按 C++ 逆序析构规则先销毁 `m_resultService`、再销毁 `m_resultExportClient`。删除 ApplicationService、Runtime 的 `shutdownResultExport()` 转发和客户端公开 `shutdown()`；worker 的 `stop()` 只供客户端析构使用。

### 4.4 UI

保留接收端 IP、端口、连接、断开、启用传输、四种连接状态和 PING/PONG 延迟。

删除：

- 本地队列不可用、历史待处理、正在同步状态；
- `Syncing`；
- 同步/放弃历史记录按钮和处置对话框；
- startup ready、处置状态和磁盘 outbox 数量对应的门禁。

启用传输并开始二维码+三期时只要求 `Connected`；Fault 恢复额外要求 `pendingCount() == 0`。

## 5. 逐文件修改

### 5.1 `app/runtime/result_export_client.h/.cpp`

- 删除磁盘目录、启动清理、快照持久化和关闭删除；
- 删除 `ResultExportRecord::isValid()`；
- 删除未被 queued signal、`QVariant` 或 `Q_ARG` 使用的 `Q_DECLARE_METATYPE(ResultExportRecord)` 和 `qRegisterMetaType<ResultExportRecord>()`；`ResultExportConnectionState` 的元类型声明和注册继续保留；
- 删除 `result_export_client.h` 中未使用的 `<memory>`；
- `m_records` 原位改名为 `m_pendingRecords`；
- 实现无返回值入队和 ACK 后直接删除队首；
- 将客户端 ACK 槽改为无参数，不再接收或二次校验产品 ID；
- PONG 后直接发送，断线保留 FIFO；
- 析构函数仅在惰性创建的 worker 和线程均已存在时执行 `stop()`、`quit()` 和 `wait()`；
- 删除第 6.1 节列出的旧接口、字段、信号、依赖和文案。

### 5.2 `app/runtime/result_export_network_worker.h/.cpp`

- 将匹配成功信号由 `ackReceived(QString id)` 改为无参数 `ackReceived()`；
- worker 在 ACK ID 与当前 `m_waitingAckId` 一致后清除等待状态并执行 `emit ackReceived()`；
- 不保留带 ID 的重载、转发信号或兼容连接；
- 除信号签名及对应 emit 外，不修改 TCP、PING/PONG、ACK 匹配、连接和定时器行为。

### 5.3 `app/runtime/result_service.cpp`

- 保留结果映射和 ID 生成；
- 先 `finalizeResultClaim()`，成功后再 `enqueue(record)`；
- 删除 `exportError`、本地队列写入失败和 enqueue 拒绝分支；
- `enqueue(record)` 之后的到期延迟 NG、统计、界面图像渲染、异步存图、正常产品 PLC 和界面呈现顺序保持不变。

### 5.4 Runtime 与 Application

`app/runtime/inspection_runtime.h/.cpp`：

- 删除 `resultExportReady()` 和 `shutdownResultExport()`；
- 保留客户端所有权、网络 Fault 接入和 Stop 后错误边界；
- 析构时先停止并等待 DetectionWorker。

`app/application/runtime_snapshot.h` 删除：

~~~text
resultExportStartupReady
resultExportDispositionPending
resultExportPendingCount
~~~

`app/application/inspection_application_service.h/.cpp` 保留：

~~~text
requestResultExportConnect
requestResultExportConnectionCheck
requestResultExportDisconnect
pendingResultExportCount
~~~

删除：

~~~text
synchronizeResultExportOutbox
abandonResultExportOutbox
clearResultExportOutbox
discardResultExportOutboxForShutdown
hasPendingResultExport
shutdownResultExport
resultExportOutboxDispositionRequired
~~~

同时删除旧 outbox 信号连接和仅为旧快照存在的 `pendingCountChanged` 连接。Fault 恢复直接读取客户端 `pendingCount()`。

开始二维码+三期且启用传输时，未连接提示统一为“请先连接结果接收端。”，不再提及待发送记录处置。`ResultExportUnavailable` 恢复未完成时提示统一为“请先恢复 TCP，并等待待确认记录发送完成。”，不再提供同步或放弃语义。

### 5.5 UI 与文档

`app/ui/main_window.cpp` 删除处置对话框、`Syncing` 和旧状态门禁，保留连接参数锁定和延迟显示。

`app/ui/main_window_inspection.cpp` 通过 `pendingResultExportCount()` 显示“当前有 N 条二维码结果尚未发送完成，确认退出将丢失这些记录”的内存丢失提示，删除 outbox 丢弃调用和 `event=result_export.shutdown_delete_failed` 日志；确认后正常退出，取消后 FIFO 继续发送。

实施完成后更新原 TCP/CSV 方案替代说明、计划索引和执行记录；历史正文保留。

## 6. 净删除门禁与受保护范围

### 6.1 必须删除

发送端相关生产代码中以下旧符号必须零引用：

~~~text
outbox.jsonl
queueDirectoryPath
m_persistenceMutex
persistSnapshot
m_outboxPath
m_startupError
m_startupReady
m_dispositionPending
m_receiverIp
m_receiverPort
m_shutdown
startupReady
startupError
outboxPath
resultExportReady
clearOutbox
discardOutboxForShutdown
requestSynchronizeOutbox
requestAbandonOutbox
outboxDispositionPending
pendingCountChanged
outboxDispositionRequired
localQueueUnavailable
outboxDispositionFinished
setDispositionPending
Syncing
hasPending
hasPendingResultExport
shutdownResultExport
Q_DECLARE_METATYPE(ResultExportRecord)
qRegisterMetaType<ResultExportRecord>
result_export.shutdown_delete_failed
~~~

同时删除：

- `ResultExportClient::shutdown()` 和 `ResultExportRecord::isValid()`；
- `result_export_client.h` 中未使用的 `<memory>`；
- 仅服务发送端 outbox 的 `QDir`、`QFile`、`QFileInfo`、`QSaveFile`、`QStandardPaths`；
- “发现当前进程待发送记录”“同步历史记录”“放弃历史记录”“历史结果待处理”“正在同步历史结果”“本地队列不可用”“outbox 清理失败”“确认退出将清空当前进程 outbox”“完成当前进程待发送记录处置”“同步完成或明确放弃当前进程待发送记录”“结果传输本地队列写入失败”等旧文案、日志和注释。

上述同名规则只约束结果传输链，其他组件合法的 `shutdown()`、`isValid()`、`hasPending()` 不受影响。

旧方法不得以固定返回值、空操作、转发包装、别名或弃用声明残留。不得新增兼容、迁移、适配器、版本判断、特性开关、Schema 字段、队列抽象或生产文件。

### 6.2 必须保持

- `result_export_network_worker.h/.cpp` 的 TCP、PING/PONG、ACK、定时器和 `stop()`；
- `tools/result_receiver` 全部生产文件、工程和部署脚本；
- 协议、接收端单客户端、JSONL、重复 ID、日期、残行修复和 CSV；
- AppSettings 的接收端 IP、端口和启用规则；
- 二维码+三期算法、其他四种模式、PLC、存图、统计、模板、相机和日志。

不新增跨进程恢复、数据库、自动重连、周期性心跳、多条并发在途、ACK 窗口、队列上限、淘汰、压缩、批发送、优先级或第二套结果出口。

## 7. 验收与完成

### 7.1 静态验收

1. 第 6.1 节旧符号、依赖和文案全部清除；
2. `ResultExportRecord` 没有元类型声明或注册，`result_export_client.h` 没有未使用的 `<memory>`；`ResultExportConnectionState` 的元类型声明和注册仍然存在；
3. 客户端只有一个内存 FIFO 和一个 `m_sendInFlight`；
4. `enqueue()` 返回 `void`，没有校验、shutdown 或错误返回分支；
5. `finalizeResultClaim()` 在 `enqueue(record)` 之前；
6. `enqueue(record)` 之后的到期延迟 NG、统计、界面图像渲染、异步存图、正常产品 PLC 和界面呈现顺序与当前 `ResultService::process()` 一致；
7. worker 是 ACK 格式、类型和 ID 匹配的唯一校验层，匹配成功信号和客户端 ACK 槽均无产品 ID 参数，客户端没有二次 ID 比较或静默返回分支；
8. 客户端析构函数是结果传输资源的唯一最终清理入口，未创建网络资源时直接结束，已创建时执行 worker `stop()` 和线程 `quit()`、`wait()`；
9. ApplicationService 和 Runtime 没有结果传输 shutdown 转发，客户端没有公开 `shutdown()` 或 `m_shutdown`；
10. 开始和 Fault 恢复提示只表达连接与待确认记录发送状态，不含处置、同步、放弃或磁盘删除语义；关闭流程没有 `result_export.shutdown_delete_failed` 日志；
11. 关闭数量只通过 `pendingResultExportCount()` 获取；
12. 没有新增兼容、胶水、迁移、适配器或生产文件；
13. 网络 worker 的 TCP、PING/PONG、ACK、定时器和 `stop()` 语义，以及协议、接收端、设置、其他模式、PLC、存图、统计和算法无范围外差异；
14. 差异文件为严格 UTF-8、有末尾换行，`git diff --check` 通过。

Agent 只执行静态检查，不运行 qmake、编译、测试程序或主程序。

### 7.2 Qt Creator 与人工验收

- 正常链路：连接并确认 PING/PONG；检测代表性 OK、二维码 NG、三期 NG；JSONL、ID、去重、统计、PLC、存图和 UI 正确，队列最终为空。
- 断线重发：ACK 前断网；队首保留并进入 Fault；手动重连后自动重发；重复 ID 不重复落盘；队列清空后允许确认 Fault。
- Stop 与退出：Stop 不断开 TCP；非空退出显示准确数量；取消后继续发送；确认后直接退出且无文件错误；重启后队列为空。
- 回归：未启用传输和其他四种模式不变；接收端监听、JSONL、CSV 和 PING/PONG 延迟不变；PLC 与机械动作按既有门禁现场验证。

### 7.3 完成条件

1. 用户明确授权生产代码实施；
2. 第 5 节代码修改和第 7.1 节静态验收完成；
3. 用户完成 Qt Creator Release 构建及第 7.2 节人工验证；
4. 原 TCP/CSV 方案替代说明、计划索引和执行记录同步；
5. 最终提交只包含本方案及直接必要修改；
6. 未覆盖、暂存、提交无关用户文件，未自动推送或改写历史。
