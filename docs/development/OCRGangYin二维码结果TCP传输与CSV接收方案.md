# OCRGangYin 二维码结果 TCP 传输与 CSV 接收方案

## 1. 文档状态、授权与范围

- 状态：设计已确认，待生产代码实施授权。
- 更新时间：2026-08-31。
- 适用仓库：`D:/BaiduNetdiskDownload/ocr20260407/ocrgangyin`。
- 目标：将“二维码+三期”模式的最终产品结果通过 TCP 传送到另一台电脑，由接收端可靠写入 JSONL，并由操作员按需同步为 Excel 可打开的 CSV。
- 本文是该功能后续实施、测试和验收的单一规范合同。

本方案只覆盖：

- 检测主程序中的结果提交、当前进程 outbox、TCP 客户端、Runtime Fault、设置和界面；
- 位于 `tools/result_receiver` 的独立 Qt 接收程序；
- 双端协议、JSONL 事实记录、接收端 ID 幂等和手动 CSV 同步；
- 与上述行为直接相关的测试、部署和验收。

本方案不改变其他检测模式的结果处理，不修改二维码+三期的算法判定，不引入数据库、SQLite、消息中间件、认证体系、第二套结果出口或自动历史删除。

## 2. 可靠性语义与非目标

### 2.1 可靠性合同

结果传输在当前进程运行期间采用：

~~~text
发送端至少一次投递
    +
接收端按 id 幂等
    +
JSONL 中同一 id 只记录一次
    +
CSV 同步后同一 id 只物化一次
~~~

发送端当前进程内发生 TCP 断网、发送失败、ACK 丢失或未确认记录重发时，记录继续保留在当前进程 outbox。接收端已经写入 JSONL 但 ACK 丢失时，同一 ID 的再次发送不会产生第二条业务记录。

JSONL 是接收端唯一事实记录。CSV 是操作员根据全部 JSONL 主动生成的可见物化结果，不参与接收成功判定。

### 2.2 outbox 可靠性边界

outbox 只服务创建它的当前进程：

- 当前进程运行和重连期间保留未确认记录；
- 正常关闭时，操作员确认退出即明确放弃仍未发送完成的记录；
- 异常退出留下的文件不在下一次启动恢复或补发；
- 下一次启动先清理上一进程残留，再创建新的当前进程 outbox。

因此，本方案不承诺跨进程继续投递，也不承诺 TCP 层面绝对只发送一次。

### 2.3 持久化级别

发送端 outbox 快照使用 `QSaveFile`、临时文件、普通 `flush` 和原子替换；接收端 JSONL 在追加后执行普通 `flush` 并检查写入结果。

本方案不要求每件产品调用 Windows `FlushFileBuffers` 等强制写入物理磁盘的机制，不保证 Windows 突然崩溃、电脑突然断电或磁盘缓存尚未落盘时最后一笔操作绝对不丢失。

## 3. 当前代码事实与 A2 边界

### 3.1 最终结果唯一出口

当前产品完成后，`ResultService::process()` 是产品级最终结果出口，负责 ProductKey 认领以及统计、PLC、存图、界面呈现和日志。

结果传输必须接入这个最终出口，不能接入二维码解码器、Pipeline 中间步骤或 UI 信号。发送内容必须以二维码+三期的整体最终判定为准。

### 3.2 二维码结果字段

二维码+三期 Pipeline 在二维码读取成功后继续执行三期日期检测，并将日期检测结果合并到最终 `DetectionResult`。

- `verdict`：二维码和三期的整体 OK/NG；
- `recognizedText`：二维码原始内容；
- `presentationText`：三期识别结果和界面显示文本；
- `diagnostic`：普通 NG 原因或系统故障诊断。

在 `BarcodeWordDetectionPipeline` 完成日期结果合并后，最终 `recognizedText` 必须仍为二维码原文。

### 3.3 A2 普通 NG 与系统 Fault

现有 A2 边界继续有效：

- 二维码不可读、日期错误、定位失败、ROI 无效等属于普通 NG；
- 解码器不可用、执行期异常、内部状态错误等属于系统 Fault。

普通 NG 可以形成正式产品结果，发送 `overallOk=false` 和空 `qrContent`。在产品正式提交前发生的系统 Fault 不形成发送记录；产品正式提交后发生的后续副作用 Fault 不撤销已经提交的当前产品。

### 3.4 总体边界

~~~text
ShengYin.exe                                      ResultReceiver.exe
检测主程序 / TCP 客户端                          接收端 / TCP 服务端

二维码+三期 Pipeline
    -> ResultService::process()
    -> 当前进程 outbox 持久化
    -> ResultExportClient ----------------------> QTcpServer
                                                  -> 按 id 去重
                                                  -> 每日 JSONL
                                                  -> ACK
                                                  -> UI 手动同步全部日期 CSV
~~~

网络发送和 ACK 等待不在检测线程中执行；当前产品的 outbox 持久化属于产品正式提交边界，必须在统计、正常产品 PLC、存图和界面呈现之前完成。

## 4. 结果提交、outbox 生命周期与 Runtime 状态

### 4.1 产品正式提交

本轮“二维码+三期”启用结果传输时，一件产品按以下顺序提交：

~~~text
最终 DetectionResult
    -> claimResult()，保留未确认 reconciliation 身份
    -> 生成 ResultExportRecord
    -> 持久化当前进程 outbox 完整快照
    -> 持久化成功
    -> 产品正式提交
    -> 允许网络线程发送
    -> 统计
    -> 正常产品 PLC
    -> 存图
    -> 界面呈现
~~~

`claimResult()` 只完成唯一认领，不能直接把该产品从未确认 reconciliation 中移除。只有 outbox 持久化成功后，该产品才跨过正式提交边界。

outbox 持久化失败时：

- 当前产品保持未确认 reconciliation 身份；
- 不计入正常产品统计；
- 不执行当前产品正常 OK/NG 对应的 PLC 动作；
- 不执行当前产品正常存图和界面呈现；
- 立即进入 `ResultExportUnavailable` Fault。

故障停机、安全处理和故障复位所需的 PLC 动作不受上述限制。outbox 持久化已经成功时，之后的 PLC、存图或其他副作用故障不撤销当前产品，其 outbox 记录继续代表有效产品结果。

本轮未启用结果传输，或检测模式不是“二维码+三期”时，不创建 outbox 记录，继续使用现有产品处理流程。

### 4.2 当前进程 outbox 生命周期

~~~text
程序启动
    -> 检查固定 outbox 路径
    -> 清理上一进程残留文件，不解析、不恢复
    -> 创建当前进程的新 outbox

当前进程运行
    -> enqueue 和 ACK 清理均持久化完整队列快照
    -> 当前进程断线重连继续处理当前队列

正常关闭请求
    -> 停止接受新帧
    -> 已接受帧完成并按本轮配置提交
    -> 检查当前 outbox
       -> 为空：删除 outbox，进入 shutdown
       -> 非空：显示未发送数量
          -> 取消：不退出
          -> 确认：删除 outbox，进入 shutdown

异常退出
    -> 可能留下 outbox
    -> 下一次启动直接清理，不补发
~~~

操作员确认在 outbox 非空时退出，表示明确放弃这些未确认记录。程序不等待 ACK、不等待接收端恢复，也不为被放弃记录创建备份。

关闭时删除失败仍然允许退出。下一次启动重新尝试清理；清理成功前不得将残留文件作为当前进程队列使用，结果传输功能保持不可用。

### 4.3 Stop 与 shutdown

操作员 Stop 后：

- 不再接受新帧；
- Stop 前已经接受的帧继续完成；
- 这些帧按本轮固定的 `enabled` 配置决定是否进入当前进程 outbox；
- Stopping 中 outbox 持久化失败仍进入 `ResultExportUnavailable` Fault；
- 正常 Stop 不关闭 TCP，已有 outbox 继续按 FIFO 发送。

主动 Stop 后才发生的断开、ACK 超时、socket 错误或发送失败，只更新连接状态、保留未确认记录并记录日志，不再创建新的 Runtime Fault。

关闭确认完成并进入 shutdown 后：

- 不再接受连接、连接检查或 enqueue 请求；
- 关闭 socket；
- 通知网络 worker 停止；
- 等待网络线程正常退出；
- shutdown 中的网络错误只记录日志和完成资源清理，不进入 Runtime Fault。

### 4.4 ResultExportUnavailable Fault

本轮已经启用传输并处于生产阶段时，下列情况进入 `ResultExportUnavailable` Fault：

- 当前产品 outbox 持久化失败；
- TCP 断开；
- socket 发送失败；
- ACK 超时、损坏或 ID 不匹配；
- ACK 后 outbox 队首清理持久化失败。

Runtime 进入 Fault 后停止接受新帧，并沿用现有 Fault reconciliation。已经正式提交的产品不撤销。

故障恢复必须同时满足：

~~~text
TCP 已恢复
    +
当前进程 outbox 已全部同步成功或由操作员明确放弃并清空
~~~

条件满足后才允许确认 `ResultExportUnavailable` 已恢复。在 Fault 尚未确认前，不自动勾选“启用二维码结果传输”。

程序启动时无法清理上一进程残留 outbox，或无法创建当前进程 outbox，属于结果传输组件不可用：Runtime 保持 Idle，禁止连接和启用传输，并提示人工处理。其他检测模式以及未启用结果传输的“二维码+三期”仍可运行。

## 5. 业务数据合同

### 5.1 创建记录的条件

只有同时满足以下条件才创建发送记录：

1. completion 合法；
2. `ResultService::claimResult()` 成功；
3. 当前运行模式为 `DetectionMode::BarcodeWord`，即“二维码+三期”；
4. 最终结果为正常 `AlgorithmVerdict::Ok` 或 `AlgorithmVerdict::Ng`；
5. 本轮 `ResultExportRunConfiguration.enabled` 为 `true`。

以下情况不创建发送记录：

- Detection Worker Fault 或其他未形成最终产品结果的系统 Fault；
- 不是“二维码+三期”模式；
- 本轮未启用二维码结果传输；
- completion 无效或无法认领唯一 ProductKey。

“启用二维码结果传输”只控制新检测结果是否进入当前进程 outbox，不控制已有 outbox 的同步、放弃或关闭清理。

### 5.2 结果映射

发送端只在 `ResultService` 的最终出口执行一次映射：

~~~text
overallOk = (result.verdict == AlgorithmVerdict::Ok)

overallOk=true:
    qrContent = recognizedText 原文

overallOk=false:
    qrContent = ""
~~~

接收端不重新执行二维码或三期判定。CSV 同步时，OK 记录写入 `qrContent`，NG 记录写入固定文本 `noQR`。

### 5.3 ID 与时间

每条记录使用已有 ProductKey 生成唯一 ID：

~~~text
id = runId + ":" + sequence
~~~

同一产品在当前进程内重发时 ID 不变。接收端只按 ID 判断重复，不比较重复消息的其他字段。

`time` 是 `ResultService` 最终处理时生成的 UTC ISO-8601 时间，必须使用 `Z` 后缀。接收端将它转换为接收电脑本地日期 `YYYYMMDD`，以此选择 JSONL 和 CSV 文件。当前进程断网跨日补发时，记录仍写入消息所属日期。

## 6. TCP 协议与时限

### 6.1 产品数据帧

协议使用 UTF-8 编码、LF 分隔的 JSON 行。每条 JSON 后追加一个 LF。

OK 示例：

~~~json
{"id":"run-uuid:125","time":"2026-08-30T10:22:18.123Z","overallOk":true,"qrContent":"DM202608270001"}
~~~

NG 示例：

~~~json
{"id":"run-uuid:126","time":"2026-08-30T10:22:18.423Z","overallOk":false,"qrContent":""}
~~~

| 字段 | 类型 | 规则 |
|---|---|---|
| `id` | 字符串 | 必须存在；作为 ACK 匹配键和接收端幂等键 |
| `time` | 字符串 | 必须存在；只接受带 `Z` 的 UTC ISO-8601 时间 |
| `overallOk` | 布尔值 | 必须存在；表示二维码+三期整体结果 |
| `qrContent` | 字符串 | 必须存在；OK 时非空，NG 时为空 |

未知字段忽略。以下记录拒绝且不返回 ACK：

- 缺少必需字段或字段类型不符；
- `time` 不是 UTC `Z`；
- `overallOk=true` 但 `qrContent` 为空；
- `overallOk=false` 但 `qrContent` 非空。

无法解析为 JSON 的数据行立即关闭当前 socket，不退出接收端程序。

### 6.2 ACK

接收端对产品返回：

~~~json
{"ack":"run-uuid:125"}
~~~

新 ID 只有在完整产品 JSON 已成功追加并 `flush` 到对应日期 JSONL、且 ID 已登记后才返回 ACK。重复 ID 不再次写 JSONL、不生成第二条业务记录、不等待 CSV，直接返回相同 ID 的 ACK。

发送端一次只发送并等待一条产品记录，只接受 `ack` 与当前队首 ID 完全一致的响应。ACK JSON 损坏、ID 不匹配、超时、socket 发送失败或断开均视为当前传输失败。

### 6.3 PING/PONG

手动连接检查使用同一 LF 分隔 JSON 协议：

~~~json
{"ping":true}
{"pong":true}
~~~

PING/PONG 不进入 outbox，不写 JSONL 或 CSV，不生成产品 ID，也不参与 ACK 和去重。产品 ACK pending 或正在同步当前进程 outbox 时，不执行也不排队 PING。

系统不发送定时心跳。静默断网可能在 socket 断开事件、下一次产品发送、ACK 等待或操作员手动检查时才被发现。

### 6.4 固定时限与重发

- 初次 TCP 连接总超时为 3 秒；
- 手动 PING/PONG 检查期限为 3 秒；
- 产品 ACK 总期限为 1.5 秒；
- ACK 计时在完整 JSON 行成功交给 `QTcpSocket::write()` 后立即开始；
- 1.5 秒覆盖 Qt/socket 发送缓冲、网络传输、接收端校验、日期索引加载、JSONL 写入与 `flush`、ACK 返回；
- 当前连接内不自动重试。

传输失败时立即关闭当前 socket，当前记录继续保留在 outbox。当前进程以后重新连接时，按第 7.4 节处置同一 ID 的未确认记录。

### 6.5 服务端连接规则

ResultReceiver 使用 `QTcpServer`，同一时刻只保留一个发送端连接。已有客户端连接时，新客户端连接被拒绝，原连接保持不变。当前客户端断开后，服务端继续监听。

## 7. 发送端 ResultExportClient 与当前进程 outbox

### 7.1 所有权与线程生命周期

`InspectionRuntime` 在应用启动时创建并持有唯一 `ResultExportClient`，应用退出时销毁。它是 outbox、传输连接状态和网络错误的事实来源，不等同于网络线程。

网络 `QThread` 和 worker 在第一次连接请求时惰性创建；停止检测或切换模式不销毁，一直保持到应用退出。如果整个应用生命周期从未使用结果传输，网络线程可以始终不存在。

网络 worker 负责 `QTcpSocket`、发送、ACK、PING/PONG 和连接时限。检测线程不等待网络，但必须同步得到当前产品 outbox 持久化是否成功。

### 7.2 请求式接口与状态传播

对外连接接口采用异步请求式语义：

~~~cpp
void requestConnect(const QString &ip, quint16 port);
void requestConnectionCheck();
void requestDisconnect();
~~~

调用返回只表示请求已提交，不表示连接或检查已经成功，UI 线程不得同步等待 3 秒。

连接状态只沿以下路径传播：

~~~text
ResultExportClient
    -> InspectionApplicationService
    -> RuntimeSnapshot
    -> UI
~~~

Runtime 管理 Idle、Starting、Running、Stopping、Fault 及 `ResultExportUnavailable`，不复制维护第二份 TCP 连接状态。UI 不直接订阅或操作 network worker。

### 7.3 outbox 物理结构与串行持久化

当前进程 outbox 使用固定路径：

~~~text
<AppDataLocation>/result_export/outbox.jsonl
~~~

文件内容是当前未确认记录的完整 JSONL 队列。enqueue 和 ACK 后删除队首都按以下方式处理：

~~~text
基于当前内存队列准备新的完整快照
    -> QSaveFile 写临时文件
    -> flush 并检查写入
    -> commit 原子替换成功
    -> 再更新内存队列
~~~

所有 outbox 修改经过同一个持久化序列。enqueue 与 ACK 清理不得并发修改文件，也不得先改内存再尝试落盘。持有 outbox/persistence mutex 时，不调用 Runtime、UI 或其他可能锁重入的外部回调。

ACK 清理持久化失败时，当前记录继续留在内存队首并停止发送后续记录。运行期间发生该错误进入 `ResultExportUnavailable` Fault；空闲处置期间发生则保留剩余队列、断开连接，不创建新的 Runtime Fault。接收端通过 ID 幂等处理以后可能出现的重复发送。

当前进程 outbox 不设置条数或大小上限，不自动丢弃运行中的记录。积压较大时接受 FIFO 发送和完整快照保存较慢。

发送端不对 outbox 自动截尾、修复或恢复。启动阶段只按第 4.2 节清理上一进程文件，不解析其内容。

### 7.4 当前进程重连处置

TCP 连接或重连成功后，分别判断 TCP 状态和当前进程 outbox 处置状态。

outbox 为空时：

- 进入可正常传输的 Connected；
- Runtime 不在未确认 Fault 时，自动勾选“启用二维码结果传输”；
- Runtime 仍处于未确认的 `ResultExportUnavailable` Fault 时，不自动勾选，先完成故障确认。

outbox 非空时：

- 显示“发现 N 条当前进程待发送记录”；
- 要求操作员选择“同步历史记录”或“放弃历史记录”；
- 选择完成前不启用传输，不允许以启用状态开始检测；
- 不执行或排队 PING，“连接”按钮可禁用。

选择“同步历史记录”时按 FIFO 逐条发送，每条收到匹配 ACK 后按第 7.3 节原子删除队首。全部 ACK 后 outbox 为空，处置完成。

选择“放弃历史记录”时，以空队列快照原子清空 outbox，不保留备份。清空成功后处置完成。

同步或清空失败时保留剩余 outbox、断开连接并回到 Disconnected。Idle 下不创建新的 Runtime Fault；已有 `ResultExportUnavailable` Fault 则继续保持。

本节只处理当前进程已经产生的记录。上一进程残留文件在程序启动阶段直接清理，不进入此处置界面。

### 7.5 关闭协调

主程序收到关闭请求后，由关闭流程按第 4.2 节先停止新帧并等待已接受帧完成，再读取当前 outbox 数量。

outbox 非空时，确认框必须明确显示未发送数量，并说明继续退出会清空这些记录：

- 取消：终止本次关闭，程序保持打开；
- 确认：删除当前 outbox 后继续退出，不等待网络恢复或 ACK。

删除失败时记录错误但不阻止退出。真正进入 shutdown 后，`ResultExportClient` 拒绝新请求，关闭 socket 并等待网络线程结束。

## 8. 接收端 Server、Store 与手动 CSV 同步

### 8.1 工程与职责

ResultReceiver 是同仓库下的独立 Qt Widgets 程序：

~~~text
tools/result_receiver/
~~~

它单独构建为 `ResultReceiver.exe`，不链接主程序的相机、PLC、OCR、二维码 DLL 或 OpenCV，只使用所需的 Qt Core、Network 和 Widgets。

职责边界：

- Window：监听参数、输出目录、连接状态和“同步 JSONL 到 CSV”操作；
- Server：TCP 监听、单客户端、LF 拆帧、协议校验、PING/PONG 和 ACK；
- Store：JSONL、日期 ID 索引、重复 ID 判断、单日期损坏处理和手动 CSV 生成。

不设置自动 CSV 物化 worker、后台同步状态机、manifest 或 checkpoint。

### 8.2 JSONL 事实记录

按消息 `time` 转换后的接收端本地日期生成：

~~~text
qr_results_YYYYMMDD.jsonl
qr_results_YYYYMMDD.csv
~~~

JSONL 保存完整产品记录，是接收端唯一事实来源。新 ID 的实时主路径为：

~~~text
校验产品 JSON
    -> 计算消息所属本地日期
    -> 加载或使用该日期 ID 索引
    -> 追加完整 JSON 行和 LF
    -> flush 并确认成功
    -> 登记 ID
    -> 返回 ACK
~~~

JSONL 写入或 `flush` 失败时不登记 ID、不返回 ACK。CSV 是否存在、是否完整或是否被 Excel 锁定，都不改变 JSONL 和 ACK 规则。

### 8.3 ID 索引与日期损坏

接收端启动时不扫描全部历史 JSONL。收到产品消息时，只按该消息日期懒加载对应 JSONL 的 ID 索引；内存只缓存当前处理日期，切换日期时释放旧索引。

加载某日期 JSONL 时：

- 最后一条是无完整 LF 的残缺尾行，可以先备份原文件，再截断到最后一个完整 LF；
- 中间记录损坏、文件无法读取或无法按上述规则恢复时，只禁用该日期；
- 其他日期继续接收和同步；
- 发往禁用日期的新记录无法可靠写入，因此不返回 ACK。

历史 JSONL 和 CSV 不设置自动保留或删除期限。

### 8.4 重复 ID

如果当前日期索引中已经存在相同 ID：

~~~text
不再次写 JSONL
    -> 不生成第二条业务记录
    -> 不检查或等待 CSV
    -> 直接返回相同 ID ACK
~~~

该规则用于处理 ACK 丢失和当前进程重连重发。接收端只按 ID 幂等，不对重复消息增加字段冲突处理。

### 8.5 手动同步 JSONL 到 CSV

接收端 UI 提供“同步 JSONL 到 CSV”。操作员触发一次同步时处理输出目录下所有已有日期：

~~~text
获取全部已有日期 JSONL
    -> 逐日期读取完整 JSONL
    -> 按记录顺序生成临时 CSV
    -> 原子替换对应日期 CSV
    -> 汇总显示成功日期和失败日期
~~~

CSV 合同：

- UTF-8 BOM；
- 无表头；
- 每条业务记录一行、每行一个值；
- `overallOk=true` 写 `qrContent`；
- `overallOk=false` 写 `noQR`；
- 值包含逗号、双引号或换行时使用标准 CSV 转义；
- 同步生成的完整 CSV 可以覆盖原 CSV，不支持依赖人工编辑内容。

CSV 被 Excel 锁定或某日期同步失败时，该日期原 CSV 保持原状并报告失败；JSONL、已有 ACK 和实时接收不受影响。操作员可以在文件恢复可写后再次执行全日期同步。

只有产品消息到达时的单日期索引加载和操作员主动同步会读取历史 JSONL；程序启动不执行全日期扫描。

## 9. 设置、UI 与部署

### 9.1 主程序设置与运行配置

`AppSettings` 增加结果接收端设置：

~~~cpp
struct ResultExportSettings
{
    QString receiverIp = QStringLiteral("192.168.10.20");
    quint16 receiverPort = 35680;
};
~~~

当前 AppSettings Schema 2 增加结果传输设置后使用 Schema 3。Schema 不匹配继续返回 `ResetRequired`，由现有启动流程要求操作员确认重置；不实现 Schema 2 到 Schema 3 的自动迁移。

`ResultExportRunConfiguration` 只保留本轮固定状态：

~~~cpp
struct ResultExportRunConfiguration
{
    bool enabled = false;
};
~~~

接收端 IP、端口属于 AppSettings 和连接请求；固定 outbox 路径属于 `ResultExportClient`，不重复复制到本轮运行配置。

### 9.2 主程序界面

“参数设定”页在“当前产品模板设置”之后增加同级“二维码结果传输”区域，包含：

- “启用二维码结果传输”复选框；
- 接收电脑 IP；
- TCP 端口；
- “连接”按钮；
- 连接状态文字；
- “断开连接”按钮。

该区域只在“二维码+三期”模式显示。切换到其他模式时隐藏但不主动断开 TCP；切回后显示当前状态。

界面规则：

- 未连接时启用复选框未勾选且不可用；
- 连接、检查、当前进程 outbox 处置和 Fault 状态按第 7 节刷新；
- 检测运行期间 IP、端口、连接、断开和启用控件不可修改；
- 已连接且无待处置 outbox时，操作员可以手动取消启用；
- 未启用结果传输时允许开始“二维码+三期”检测；
- 已勾选启用但 TCP 或 outbox 处置状态不满足时拒绝开始；
- 切换模式和停止检测不销毁 `ResultExportClient` 或网络线程。

### 9.3 接收端界面与设置

接收端界面包含：

- 监听 IP，默认 `0.0.0.0`；
- 监听端口，默认 `35680`；
- 输出目录和“浏览”按钮；
- “连接”按钮、连接状态文字和“断开连接”按钮；
- “同步 JSONL 到 CSV”按钮及同步结果。

接收端通过 `QSettings("OCRGangYin", "ResultReceiver")` 保存：

~~~text
network/listenIp
network/port
storage/outputDirectory
~~~

首次运行的输出目录为空，操作员必须选择绝对路径，不提供默认目录。以后恢复上次设置；路径失效时要求重新选择。程序启动后不自动监听，仍由操作员点击“连接”。

点击“连接”后校验当前输出目录并启动监听。等待发送端时显示“等待发送端连接”；已有客户端时显示“连接成功”。重复点击不重复监听，客户端自行断开后继续监听。点击“断开连接”时关闭当前客户端并停止监听。

### 9.4 部署约定

- 主程序通过 Qt Network 连接接收电脑实际 IPv4，不能把 `0.0.0.0` 作为连接目标；
- 现场防火墙放行 TCP 端口 `35680`；
- 发送电脑系统时间和接收电脑时区必须正确；
- ResultReceiver 与所需 Qt 运行库部署在接收电脑；
- 接收端不依赖 Excel 或 Python；
- CSV 可以在任意时间缺失、过期或被锁定，现场事实以 JSONL 为准。

## 10. 失败处理矩阵

下表只汇总前述合同，不改变第 4、6、7、8 节的规则。

| 场景 | 数据处理 | Runtime / 连接结果 |
|---|---|---|
| 启动时无残留 outbox | 创建新的当前进程空队列 | 结果传输可用 |
| 启动时存在上一进程残留 | 直接清理，不解析、不补发 | 清理成功后创建新队列 |
| 启动残留清理或新队列创建失败 | 不使用残留文件 | Runtime 保持 Idle，结果传输不可用 |
| 当前产品 outbox 持久化失败 | 产品未正式提交，保留 reconciliation 身份 | Stopping 或 Running 均进入 `ResultExportUnavailable` |
| 当前进程连接成功且 outbox 为空 | 无待处置记录 | 进入可正常传输的 Connected；未确认 Fault 前不自动勾选 |
| 当前进程连接成功且 outbox 非空 | 等待同步或放弃 | 选择前禁止启用检测和 PING |
| 当前进程 outbox 同步完成 | 全部匹配 ACK 后队列为空 | 可进入正常 Connected |
| 当前进程 outbox 明确放弃 | 原子清空，不留备份 | 可进入正常 Connected |
| 当前进程 outbox 同步或放弃失败 | 保留剩余记录 | 断开；Idle 不新建 Fault，已有 Fault 保持 |
| 运行中 TCP、发送或 ACK 失败 | 保留当前及后续未确认记录 | 关闭连接并进入 `ResultExportUnavailable` |
| ACK 后队首清理失败 | 队首不删除，停止后续发送 | 运行中进入 Fault；空闲处置时断开且不新建 Fault |
| 操作员 Stop 后发生纯网络错误 | 保留未确认记录 | 更新状态和日志，不新建 Fault |
| 正常关闭且 outbox 为空 | 删除队列 | 直接退出 |
| 正常关闭且 outbox 非空，操作员取消 | 不删除队列 | 取消退出 |
| 正常关闭且 outbox 非空，操作员确认 | 删除并明确放弃未确认记录 | 不等待 ACK，继续退出 |
| 关闭时 outbox 删除失败 | 文件可能残留 | 记录错误但仍退出，下次启动重新清理 |
| 异常退出 | 文件可能残留 | 下一次启动清理，不跨进程补发 |
| 新 ID 的 JSONL 写入或 flush 失败 | 不登记 ID、不写 CSV | 不返回 ACK |
| 重复 ID | 不重复写 JSONL，不等待 CSV | 直接返回相同 ID ACK |
| CSV 缺失、过期或被 Excel 锁定 | JSONL 保持事实完整 | 不影响 ACK、接收和生产 |
| 手动 CSV 同步某日期失败 | 不修改 JSONL，原 CSV 保持原状 | 报告失败日期，可再次同步 |
| 某日期 JSONL 尾行残缺 | 先备份，再截断至最后完整 LF | 恢复后该日期继续使用 |
| 某日期 JSONL 中间损坏 | 不自动丢弃中间记录 | 只禁用该日期，其他日期继续 |
| 未启用传输的二维码+三期 | 不创建 outbox 记录 | 正常检测 |

## 11. 未来实施文件范围

本次不创建或修改以下生产文件。取得代码实施授权后，预计范围为：

### 11.1 主程序

- `app/detection/detectionmode/barcode_word/barcode_word_detection_pipeline.cpp`：保持最终 `recognizedText` 为二维码原文；
- `app/runtime/result_service.h/.cpp`：结果映射、正式提交边界和 outbox 失败出口；
- `app/runtime/inspection_runtime.h/.cpp`：持有唯一 `ResultExportClient`，接入 `ResultExportUnavailable`；
- `app/application/inspection_application_service.h/.cpp` 和 `runtime_snapshot.h`：请求接口、开始门禁和单一 UI 状态传播；
- `app/system_support/settings/app_settings.h/.cpp`、`app_settings_store.cpp`：接收端参数和新 Schema；
- 主窗口、参数设定页及 UI policy：连接控件、当前进程 outbox 处置和关闭确认；
- 主程序 qmake 工程：Qt Network 及新增源文件；
- ResultExportClient 及其网络 worker：当前进程 outbox、TCP、ACK 和 shutdown。

### 11.2 接收端

- `tools/result_receiver` 下的独立 qmake 工程和入口；
- ResultReceiver Window：设置、监听状态和手动全日期 CSV 同步；
- ResultReceiver Server：TCP、LF 拆帧、单客户端、PING/PONG 和 ACK；
- ResultReceiver Store：JSONL、当前日期 ID 索引、重复 ID、日期损坏和 CSV 原子生成；
- 接收端 README 和部署说明。

不新增自动 CSV 同步、跨进程 outbox 恢复、数据库、消息中间件或第二套结果出口。具体新文件拆分、类内私有状态和控件对象名在代码实施时按项目现有风格确定。

## 12. 测试、验收与性能门禁

### 12.1 结果与提交边界

1. 二维码+三期整体 OK 时发送二维码原文；
2. 二维码或三期普通 NG 时发送 `overallOk=false`、空 `qrContent`；
3. 其他模式、系统 Fault、本轮未启用传输和重复 completion 不创建记录；
4. `claimResult()` 后 outbox 成功才正式提交；
5. outbox 失败时不执行正常统计、PLC、存图和呈现，并保留 reconciliation 身份；
6. outbox 成功后的副作用 Fault 不撤销已提交产品。

### 12.2 当前进程 outbox 与生命周期

1. enqueue 和 ACK 清理使用完整快照、串行持久化和 `QSaveFile`；
2. 当前进程断线重连可由操作员同步或放弃；
3. 正常关闭 outbox 非空时显示数量，取消不退出，确认删除后退出；
4. 删除失败仍允许退出，下一次启动重试清理；
5. 异常退出残留下一次启动直接清理，不恢复且不进入重连处置界面；
6. Stop 后不接受新帧，已接受帧继续完成，已有 outbox 继续发送；
7. 主动 Stop 后迟到网络错误和 shutdown 网络错误不新建 Fault。

### 12.3 协议与接收端

1. 合法 OK/NG JSON 写入消息所属本地日期 JSONL；
2. 未知字段被忽略，必需字段、类型、UTC `Z` 和 OK/NG 内容组合按合同校验；
3. 坏 JSON 关闭当前 socket，第二客户端被拒绝；
4. JSONL 成功即可 ACK，CSV 不参与 ACK；
5. ACK 丢失后的重复 ID 不增加 JSONL 记录并直接返回同 ID ACK；
6. 产品 ACK pending 和 outbox 同步期间不执行或排队 PING；
7. 连接 3 秒、手动检查 3 秒、ACK 1.5 秒及 `write()` 起算点符合第 6.4 节；
8. ACK 失败时当前连接不自动重试。

### 12.4 CSV 与日期

1. 接收端启动不扫描全部历史 JSONL；
2. ID 索引按当前消息日期懒加载，切换日期时只保留当前日期；
3. 手动同步一次覆盖所有已有日期；
4. CSV 为 UTF-8 BOM、无表头、每条业务记录一行；
5. OK 写二维码原文，NG 写 `noQR`，特殊字符按标准 CSV 转义；
6. CSV 被锁或同步失败不影响 JSONL、ACK 和生产；
7. JSONL 最后残行备份后截断，中间损坏只禁用对应日期。

### 12.5 UI、部署与回归

1. 主程序结果传输区域只在“二维码+三期”显示；
2. 未连接时不能启用，未启用时仍允许检测；
3. 当前进程 outbox 待处置和未确认 Fault 状态满足连接与启用门禁；
4. 接收端首次输出目录为空，必须选择绝对路径，重启后由 QSettings 恢复；
5. 两端连接、主动断开、客户端断开后继续监听和手动 PING/PONG 正常；
6. 五种检测模式现有行为不因结果传输功能回归。

### 12.6 性能门禁

使用约 96,000 条历史 JSONL 对单日期索引冷加载进行验证：

- 新 ID 从完整 JSON 被 `QTcpSocket::write()` 接受到 ACK 稳定满足 1.5 秒；
- 重复 ID 从完整 JSON 被 `QTcpSocket::write()` 接受到 ACK 稳定满足 1.5 秒；
- 分别记录两类请求的实际耗时。

性能门禁不满足时，不得把原因归类为普通网络故障，也不得直接增加重试或修改超时。必须重新评估索引实现或 ACK 合同并取得用户确认。

## 13. 实施顺序与状态

未来获得代码实施授权后，按以下顺序执行：

1. 固定 BarcodeWord 最终 `recognizedText` 和结果映射测试；
2. 实现当前进程 outbox、正式提交边界及 reconciliation；
3. 实现 `ResultExportClient`、异步连接接口、ACK 和连接状态传播；
4. 接入 Runtime Fault、Stop、关闭确认和启动残留清理；
5. 增加 AppSettings 新 Schema、主程序设置和 UI 门禁；
6. 实现独立 ResultReceiver、JSONL、当前日期 ID 索引和重复 ID ACK；
7. 增加接收端“同步 JSONL 到 CSV”和日期损坏处理；
8. 完成双端测试、约 96,000 条冷加载性能测试、五模式回归和现场验收。

在生产代码、自动测试和现场门禁完成前，本方案状态保持“设计已确认，待生产代码实施授权”，不得把计划行为标记为已经实现或验证。
