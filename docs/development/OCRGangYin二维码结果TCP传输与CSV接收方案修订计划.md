# OCRGangYin 二维码结果 TCP 传输与 CSV 接收方案修订计划

## 1. 文档状态与范围

- 状态：当前修订范围和行为决策已确认，待执行目标方案修订。
- 更新时间：2026-08-31。
- 目标文档：docs/development/OCRGangYin二维码结果TCP传输与CSV接收方案.md。
- 本计划只指导目标方案文档的修订，不修改目标方案、生产代码、工程配置、测试代码或计划索引。
- 本计划不执行 qmake、编译、运行、单元测试、压力测试或现场验证。

既有 D1-D12 和补充决定的有效业务方向保留，并统一收敛到下述单一合同。除本文明确写出的规则外，不自行增加数据库、SQLite、消息中间件、认证体系、新协议、新 ACK 类型、自动网络重试、定时心跳或自动历史删除。

## 2. 当前最终合同

### 2.1 结果提交

一件产品的正式提交顺序为：

~~~text
最终 DetectionResult
    -> claimResult()，保留未确认 reconciliation 身份
    -> 当前进程 outbox 完整快照持久化成功
    -> 产品正式提交
    -> 允许网络线程发送
    -> 统计、正常产品 PLC、存图、界面呈现
~~~

outbox 持久化失败时，当前产品不计正常统计、不执行正常产品 PLC、不做正常存图或呈现，并进入 ResultExportUnavailable Fault。安全停止、故障复位等 Fault 流程所需的设备动作不受影响。outbox 成功后的副作用 Fault 不撤销该产品。

### 2.2 outbox 只服务当前进程

outbox 是当前进程的运行时队列，不承担跨进程恢复：

~~~text
当前进程启动
    -> 清理上一进程残留 outbox，不解析、不恢复
    -> 创建当前进程的新 outbox

当前进程运行
    -> enqueue 和 ACK 清理使用完整快照、串行持久化、QSaveFile
    -> TCP 断线重连时继续处理当前进程 outbox

正常关闭
    -> 停止接受新帧
    -> 已接受帧完成并写入 outbox
    -> 检查 outbox
    -> 为空：删除 outbox 并退出
    -> 非空：提示未发送数量
         -> 取消：不退出，继续运行
         -> 确认：删除 outbox，关闭程序

异常退出
    -> 不恢复上一进程 outbox
    -> 下一次启动直接清理残留并创建新 outbox
~~~

确认关闭后不等待 ACK、不等待接收端、不保留备份。删除失败时仍允许退出；下一次启动再次尝试清理，清理成功前不得把残留文件作为当前进程 outbox 使用。

当前进程 outbox 不设置条数或大小上限，不自动丢弃运行中的记录；积压较大时 FIFO 发送可能较慢。

### 2.3 当前进程内连接和历史处置

当前进程首次连接、断线重连或手动重新连接时先检查当前 outbox：

- outbox 为空时进入正常 Connected；Fault 未确认前不自动勾选传输；
- outbox 非空时等待操作员选择“同步历史记录”或“放弃历史记录”；
- 选择前不启用传输、不以启用状态开始检测、不执行或排队 PING；
- 同步按 FIFO，逐条 ACK 后清理队首；
- 放弃通过原子空快照清空且不留备份；
- 同步失败保留剩余记录并断开，空闲不产生新的 Fault；
- 全部同步或明确放弃成功后，才按 Fault 状态进入正常 Connected。

该流程只处理当前进程已经产生的 outbox，不处理上一进程残留文件。

### 2.4 Stop、shutdown 和故障

Stop 后不接受新帧，已接受帧继续完成；Stopping 中 outbox 持久化失败仍进入 ResultExportUnavailable Fault。正常 Stop 不主动关闭 TCP，当前 outbox 继续发送。

主动 Stop 完成后才发生的纯网络断开、ACK 超时或 socket 错误，只更新连接状态和日志，不新建 Runtime Fault。shutdown 开始后禁止新连接、检查、enqueue 和 Fault 回调，关闭 socket，等待网络线程退出，不等待远端 ACK。

### 2.5 接收端实时路径

JSONL 是唯一事实日志。实时收到产品后：

~~~text
解析和校验 JSON
    -> 按 UTC Z 转换本地日期
    -> 懒加载该日期 ID 索引
    -> 新 ID：JSONL 追加并 flush
             -> 登记 ID
             -> 返回 ACK
    -> 重复 ID：不追加 JSONL、不生成第二条业务记录
             -> 直接返回相同 ID ACK
~~~

CSV 不属于 ACK 前置条件。JSONL 写入或 flush 失败不返回 ACK；重复 ID 不等待 CSV，即使 CSV 未同步、被锁定或内容不完整也直接 ACK。未知字段忽略，必需字段、类型、OK/NG 与 qrContent 组合以及 UTC Z 按既有协议合同校验。坏 JSON 关闭当前 socket，第二客户端拒绝。

### 2.6 CSV 手动同步

接收端实时运行只写 JSONL、登记 ID 和返回 ACK。CSV 可以暂时过期、dirty 或被 Excel 锁定，不影响接收、ACK、生产或接收端继续运行。

接收端 UI 提供“同步 JSONL 到 CSV”，一次处理所有已有日期：

~~~text
操作员点击同步
    -> 获取已有日期列表
    -> 逐日期读取完整 JSONL
    -> 生成无表头、每行一个值的临时 CSV
    -> 原子替换对应 CSV
    -> 显示完成或失败日期
~~~

CSV 使用 UTF-8 BOM；OK 写入 qrContent，NG 写入 noQR，并按标准 CSV 规则转义。同步失败不修改 JSONL、不撤销 ACK；操作员之后可以再次同步。启动不扫描全部历史 JSONL，只有操作员主动同步时才处理所有已有日期。

### 2.7 日期索引和损坏

只缓存当前访问日期的 ID 索引，切换日期时释放旧索引；不设置自动保留或删除期限。接收端 JSONL 最后一条残缺尾行可先备份原文件，再截断到最后完整 LF；中间损坏或无法恢复时只禁用对应日期，其他日期继续服务。该残行规则不用于发送端 outbox。

### 2.8 设置、界面和协议时限

- ResultExportRunConfiguration 只保留本轮 enabled；
- InspectionRuntime 持有唯一 ResultExportClient；
- UI 状态只经 ResultExportClient -> InspectionApplicationService -> RuntimeSnapshot -> UI；
- 网络线程第一次连接时惰性创建；
- AppSettings Schema 不匹配时保持 ResetRequired，不做自动迁移；
- 接收端首次输出目录为空，必须人工选择绝对路径并由 QSettings 保存，路径失效时重新选择；
- TCP 连接超时 3 秒；
- ACK 总期限 1.5 秒，从完整 JSON 被 QTcpSocket::write() 接受后立即开始；
- 当前连接不自动重试，失败后当前进程重新连接时按 outbox 规则处理；
- 无定时心跳；产品 ACK pending 或同步当前进程 outbox 时不执行、不排队 PING。

## 3. 目标方案章节重排

目标方案重排为以下结构，时序、失败矩阵、部署和验收只引用第 2 节合同：

1. 文档状态、授权和范围；
2. 可靠性语义与非目标；
3. 当前代码事实与 A2 边界；
4. 结果提交、outbox 生命周期和 Runtime 状态；
5. 业务数据合同；
6. TCP 协议与时限；
7. 发送端 ResultExportClient、当前进程 outbox、关闭提示和重连处置；
8. 接收端 Server、Store、JSONL、重复 ID 和手动 CSV 同步；
9. 设置、UI 和部署参数；
10. 单一失败处理矩阵；
11. 实施文件范围；
12. 测试、验收和性能门禁；
13. 实施顺序与状态维护。

## 4. 必须清理的冲突表述

目标方案中必须删除或改写以下规则：

| 冲突表述 | 当前合同 |
|---|---|
| 应用重启恢复上一进程 outbox | 启动清理残留，创建新 outbox，不跨进程补发 |
| outbox 永久作为历史队列 | 只属于当前进程，正常关闭确认后删除 |
| 异常残留 outbox 进入 D6 | 启动直接清理，不解析、不恢复、不进入 D6 |
| 关闭时等待全部 ACK | 检查并提示，确认后删除并退出 |
| 用户取消关闭仍退出 | 取消关闭，程序继续运行 |
| 删除失败阻止退出 | 允许退出，下一次启动再次清理，清理前不使用旧文件 |
| 统计、PLC、存图后再写 outbox | claim -> outbox 成功 -> 正式提交 -> 其他副作用 |
| CSV 成功后才能 ACK | JSONL 成功即可 ACK |
| 重复 ID 必须等待 CSV | 重复 ID 直接返回相同 ID ACK |
| 每条产品自动全量重建 CSV | 实时只写 JSONL，CSV 由 UI 手动同步所有日期 |
| 接收端启动扫描全部历史 JSONL | 启动不扫描，主动同步时才处理所有已有日期 |
| ACK 超时后当前连接自动重试 | 保持 R0，当前进程重新连接后再处理 outbox |
| Schema 2 自动迁移 | Schema 不匹配触发 ResetRequired |
| ResultReceiver 默认输出目录 | 首次为空，操作员选择绝对路径 |

## 5. 失败矩阵和关键时序

目标方案只保留一张失败矩阵，至少覆盖：

- 当前进程启动清理残留成功或失败；
- 当前进程 outbox 为空、非空未选择、同步成功、同步失败、明确放弃；
- 当前进程断线、ACK 超时、ACK 清理失败、outbox 写入失败；
- 正常 Stop、Stopping 中已接受帧、主动 Stop 后迟到网络错误、shutdown；
- 关闭时 outbox 为空、取消、确认删除、删除失败；
- 新 ID、重复 ID、ACK 丢失、当前进程重连；
- JSONL 写失败、CSV 手动同步失败、CSV 被锁；
- 异常退出后下一次启动不恢复旧 outbox；
- 单日期 JSONL 损坏和其他日期继续服务；
- 未启用传输时二维码+三期继续检测。

关键时序必须为：

~~~text
当前进程启动
  -> 清理上一进程残留 outbox
  -> 创建新的当前进程 outbox
  -> 不恢复旧记录
~~~

~~~text
当前进程关闭
  -> 停止新帧
  -> 已接受帧完成
  -> 检查当前 outbox
  -> 为空：删除并退出
  -> 非空：提示
       -> 取消：继续运行
       -> 确认：删除并退出
~~~

~~~text
接收产品 JSON
  -> JSONL 校验、写入并 flush
  -> 新 ID 登记后 ACK
  -> 重复 ID 直接 ACK
  -> CSV 不参与 ACK
~~~

~~~text
操作员点击“同步 JSONL 到 CSV”
  -> 获取所有已有日期
  -> 逐日期从完整 JSONL 生成临时 CSV
  -> 原子替换
  -> 报告成功/失败日期
~~~

## 6. 实施文件范围

本次不创建生产文件。未来代码实施范围包括：

- 主程序 ResultService、InspectionRuntime、ApplicationService、RuntimeSnapshot、ResultExportClient、AppSettings 和参数设定 UI；
- 当前进程 outbox 的串行快照持久化、关闭确认、启动残留清理和当前进程 D6 重连；
- ResultReceiver Window、Server、Store 以及“同步 JSONL 到 CSV”UI 操作；
- Store 负责 JSONL、按日期 ID 索引、重复 ID 判断、日期损坏和手动 CSV 全量重建；
- Server 负责 TCP、LF 拆帧、PING/PONG 和 ACK。

不新增后台 CSV 自动同步、跨进程 outbox 恢复、数据库、消息中间件或第二套结果出口。具体类名、文件名和 qmake 拆分在代码实施授权后按现有项目风格确定。

## 7. 测试与验收范围

目标方案验收必须覆盖：

1. 当前进程 outbox enqueue/ACK 清理使用完整快照、串行持久化和 QSaveFile；
2. 当前进程断线重连按 D6 同步或放弃；
3. 正常关闭 outbox 非空时提示数量，取消不退出，确认删除后退出；
4. 异常退出残留 outbox 下一次启动直接清理，不恢复、不进入 D6；
5. 删除失败仍允许退出，下一次启动再次清理，清理前不使用旧文件；
6. outbox 提交失败的正式提交边界、reconciliation 和 Fault；
7. JSONL 成功即可 ACK，CSV 手动同步失败不影响接收和生产；
8. 重复 ID 不写 JSONL、不等待 CSV、直接同 ID ACK；
9. 手动同步覆盖所有已有日期，生成无表头 BOM UTF-8 CSV；
10. 启动不扫描全部历史 JSONL，ID 索引按当前访问日期懒加载；
11. 最后一条残行备份后截断，中间损坏只禁用对应日期；
12. 连接 3 秒、ACK 1.5 秒、write() 起算和 R0；
13. Stop、shutdown、未启用传输、Fault 和关闭提示门禁；
14. 约 96,000 条历史 JSONL 冷加载时，新 ID 和重复 ID 从 write() 接受到 ACK 稳定满足 1.5 秒并记录耗时。

性能门禁不满足时，不得归类为普通网络故障，不得私自增加重试或修改超时，必须重新评估索引实现或 ACK 合同并重新取得用户确认。本次不执行上述测试，不声称已通过。

## 8. 实施细节边界

以下内容可在代码实施时按现有项目风格确定：

- 当前进程 outbox 文件名和启动残留清理调用点；
- 关闭确认对话框的控件对象名和最终文案；
- 当前进程状态枚举名称；
- 手动 CSV 同步按钮的控件、日期列表和进度显示；
- 同步期间 UI 是否暂时锁定连接操作；
- JSONL 残行备份后缀；
- qmake 新增文件拆分；
- 日志字段和级别。

这些实现细节不得改变当前进程专属 outbox、关闭确认删除、异常残留清理、当前进程 D6、JSONL ACK、重复 ID 直接 ACK、手动全日期 CSV 同步、3 秒连接超时、1.5 秒 ACK、R0 和性能验收门禁。

## 9. 静态门禁与交付边界

目标方案获授权修改后，只执行只读静态检查：

- 确认全文只有一套 outbox 生命周期、关闭提示、D6、JSONL ACK 和手动 CSV 同步定义；
- 确认不再出现跨进程 outbox 恢复、启动历史同步、CSV 成功后 ACK、重复 ID 等待 CSV 或自动 CSV 同步；
- 确认 D1-D12 的有效行为、重复 ID ACK、当前日期懒加载和 96,000 条冷加载性能门禁一致；
- 检查标题层级、代码块、表格、UTF-8、行尾和 git diff --check。

本计划只修改本计划文件本身。目标方案、app/、tools/、工程配置、测试代码和计划索引不在本次修改范围内。下一步需用户确认本计划后，再单独授权目标方案修订；本计划不授权生产代码实施。
