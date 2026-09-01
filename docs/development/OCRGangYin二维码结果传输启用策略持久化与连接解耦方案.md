# OCRGangYin 二维码结果传输启用策略持久化与连接解耦方案

## 1. 状态、基线与权威范围

- 状态：生产代码实施完成，Agent 静态门禁通过，待用户统一验证。
- 计划日期：2026-09-02。
- 代码基线：`01675d78e35f01e7df01435adba20c16ec2a345a`，提交主题为 `refactor: 精简二维码结果发送端内存队列`。
- 权威范围：`resultExportEnable` 的 AppSettings 持久化、UI 启用政策、TCP 连接解耦、二维码+三期开始识别门禁和相关旧状态删除。
- 当前进度：15 个计划内生产文件和 1 个子系统说明文件已完成原位修改；AppSettings Schema 4、启用政策持久化、TCP 状态解耦、开始门禁和运行配置扁平化均已落地，旧状态与旧转发零引用。
- 验证门禁：Agent 已完成范围、旧符号、严格 Schema 和文件质量静态检查；由用户在 Qt Creator 执行 Release 构建和第 9.2 节人工验证。

本方案只替代 `OCRGangYin二维码结果TCP传输与CSV接收方案.md` 和 `OCRGangYin二维码结果发送端内存队列精简方案.md` 中“TCP 连接状态自动决定启用复选框”的政策。内存 FIFO、产品 ID ACK、接收端幂等、JSONL/CSV、传输 Fault、退出提示和网络线程生命周期继续以现行实现为准。

实施必须原位完成。不得保留旧瞬时启用状态、连接驱动自动勾选、旧字段转发、单字段运行配置包装、兼容读取、迁移分支、双 Schema、适配器或新的设置服务。

## 2. 最终政策

`resultExportEnabled` 表示操作员是否希望二维码+三期结果发送到接收端；TCP 状态表示当前网络是否具备发送条件。两者是独立事实，互不修改。

| 场景 | `resultExportEnabled` | TCP 要求 | 结果 |
|---|---:|---|---|
| 程序首次使用 Schema 4 默认设置 | `false` | 无 | 允许二维码+三期识别，不发送结果 |
| 操作员勾选启用 | 保存为 `true` | 不要求当时已连接 | 复选框保持勾选，重启后恢复 |
| 操作员取消启用 | 保存为 `false` | 与连接状态无关 | 后续运行不产生发送记录 |
| TCP 连接或重连成功 | 不改变 | 状态变为 `Connected` | 不自动勾选启用 |
| TCP 主动断开或异常断开 | 不改变 | 状态变为 `Disconnected` | 不自动取消启用 |
| 已启用且开始二维码+三期识别 | `true` | 必须为 `Connected` | 未连接时弹出阻断提示并拒绝启动 |
| 未启用且开始二维码+三期识别 | `false` | 不要求连接 | 正常启动，不产生发送记录 |
| 其他四种检测模式 | 任意 | 不参与本模式启动 | 保持现有行为，不产生二维码结果发送记录 |

“弹出警告”固定表示提示后拒绝本次启动，不提供“仍然继续”“本轮临时禁用发送”或“先积压后补发”选项。

连接和启用允许以下稳定组合：

~~~text
未启用 + 未连接
未启用 + 已连接
已启用 + 未连接
已启用 + 已连接
~~~

只有最后一种组合允许以启用结果传输的配置开始二维码+三期识别。

## 3. 唯一数据所有权与 Schema 4

### 3.1 AppSettings 唯一事实

`AppSettings` 增加一个字段：

~~~cpp
bool resultExportEnabled;
~~~

默认值固定为 `false`。JSON 固定写入现有 `resultExport` 分区：

~~~json
"resultExport": {
  "enabled": false,
  "receiverIp": "192.168.10.20",
  "receiverPort": 35680
}
~~~

`SettingsApplicationService::current()` 是启用政策的唯一内存正式值，`AppSettingsStore` 是唯一磁盘入口。不得在 MainWindow、RuntimeSnapshot、InspectionRuntime 或新的对象中维护第二份正式启用状态。

### 3.2 严格 Schema 更新

- `AppSettings::CurrentSchemaVersion` 从 `3` 直接升级为 `4`。
- `resultExport.enabled` 是 Schema 4 必填布尔字段。
- `settingsToJson()` 固定写出该字段；`settingsFromJson()` 严格读取布尔值。
- `hasOnlyKeys(resultExport, ...)` 只允许 `enabled/receiverIp/receiverPort`。
- Schema 3 按现有 `SETTINGS_RESET_REQUIRED` 流程整体重置为 Schema 4 默认设置，启用默认恢复为 `false`。
- 不实现 Schema 3 到 Schema 4 迁移，不把字段设计为可选，不为缺失字段提供默认回退，不双读旧键，不保留版本分支。

设置文件写入失败属于真实外部失败边界。用户切换复选框但保存失败时，必须恢复为 `SettingsApplicationService::current().resultExportEnabled` 并显示现有设置保存失败提示；这不是防御性分支，而是保证 UI 不冒充已保存状态的必要提交边界。

## 4. 单一运行链

### 4.1 启动与 UI

~~~text
程序启动
    -> 从 AppSettings 恢复 resultExportEnable
    -> TCP 状态只更新连接状态文字和连接按钮

用户切换 resultExportEnable
    -> 复制 SettingsApplicationService::current()
    -> 修改 candidate.resultExportEnabled
    -> 复用 saveConfiguration(candidate)
       -> 成功：正式值和复选框一致
       -> 失败：复选框恢复正式值并提示
~~~

程序化恢复复选框时只使用现有 `QSignalBlocker`，避免把启动恢复再次当成用户修改。不得增加 `m_initializing`、设置同步信号、专用 setter 服务或额外状态机。

`updateResultExportUi()` 只负责：

- 二维码+三期区域显隐；
- TCP 连接状态和延迟显示；
- 运行、停止或 Fault 期间锁定可修改控件；
- IP、端口和连接按钮的现有连接状态政策。

`resultExportEnable` 是否勾选不得由 `Connected/Disconnected/Connecting/Checking` 改写。空闲且非 Fault 时，复选框可在任何 TCP 状态下修改。

### 4.2 开始识别

`InspectionApplicationService::start()` 已持有 `SettingsApplicationService::current()`，因此直接读取 `settings.resultExportEnabled`：

~~~text
模式不是 BarcodeWord
    -> 忽略结果传输启用政策

模式是 BarcodeWord 且 resultExportEnabled=false
    -> 正常继续启动

模式是 BarcodeWord 且 resultExportEnabled=true
    -> connectionState == Connected：继续启动
    -> 其他状态：返回 RESULT_EXPORT_NOT_CONNECTED，拒绝启动
~~~

未连接提示固定为：

~~~text
已启用二维码结果传输，但结果接收端未连接，请先连接后再开始识别。
~~~

继续复用现有 `StartInspectionResult` 和 `presentStartFailure()` 呈现这次拒绝。不得在 MainWindow 再做一次 TCP 预检查，不新增第二个弹窗分支，不新增“是否继续”对话框，也不新增只为该提示服务的 DTO、信号或应用服务方法。

本轮运行配置直接从同一份 AppSettings 冻结：

~~~text
configuration.resultExportEnabled
    = settings.resultExportEnabled
      && 当前模式为 BarcodeWord
~~~

`ResultServiceRunConfiguration` 直接保存 `bool resultExportEnabled`，不再通过只有一个字段的 `ResultExportRunConfiguration` 包装。开始成功后，本轮 `ResultServiceRunConfiguration::resultExportEnabled` 保持不可变；运行期间修改磁盘文件不改变当前运行。

## 5. 必须删除的旧代码

实施完成后，生产代码中以下旧状态和转发必须零引用：

~~~text
m_resultExportUserEnabled
m_resultExportAutoEnableApplied
StartInspectionCommand::resultExportEnabled
RuntimeSnapshot::resultExportEnabled
InspectionRuntime::resultExportEnabled()
command.resultExportEnabled
snapshot.resultExportEnabled
ResultExportRunConfiguration
ResultServiceRunConfiguration::resultExport
configuration.resultExport.enabled
m_runConfiguration.resultExport.enabled
~~~

同时删除：

- `updateResultExportUi()` 中断线自动置 `false`、连接后自动置 `true` 和首次自动启用分支；
- 连接状态参与 `resultExportEnable` 勾选值和可修改条件的逻辑；
- `on_resultExportDisconnect_clicked()` 中取消启用状态的赋值；
- `on_resultExportEnable_toggled()` 中只写 MainWindow 成员的瞬时实现；
- ApplicationService 向 Runtime 传递临时启用布尔值的调用；
- RuntimeSnapshot 对本轮启用状态的无消费者字段和发布赋值；
- InspectionRuntime 只为该快照字段存在的公开查询包装；
- `ResultExportRunConfiguration` 单字段结构、`ResultServiceRunConfiguration::resultExport` 嵌套字段及对应嵌套读写；
- `result_service.h` 中仅因上述包装产生的 `result_export_client.h` 头文件依赖。

必须保留 `ResultService::resultExportEnabled()`。它仍是 Runtime 处理运行中传输失败时判断本轮是否启用发送的实际查询，不属于旧 UI 状态或胶水层。

旧符号不得以固定返回值、弃用声明、别名、转发函数、注释代码或兼容字段残留。

## 6. 逐文件计划表

| 文件 | 修改 | 删除/保持边界 |
|---|---|---|
| `app/system_support/settings/app_settings.h` | Schema 升级为 4，增加 `resultExportEnabled` | 不增加嵌套设置类或第二个 Schema |
| `app/system_support/settings/app_settings.cpp` | 默认值设为 `false`，加入相等比较 | 不增加迁移默认或旧字段映射 |
| `app/system_support/settings/app_settings_store.h/.cpp` | 严格读写 `resultExport.enabled`，更新过时 Schema 注释 | 不接受缺失字段、数字或字符串替代布尔值 |
| `app/application/inspection_application_service.h` | 删除 `StartInspectionCommand::resultExportEnabled` | `unappliedChanges` 和其他启动合同保持 |
| `app/application/inspection_application_service.cpp` | 启动门禁和运行配置直接读取 AppSettings；更新未连接提示；删除快照赋值 | 不在 UI 重复预检，不改变 Fault 恢复合同 |
| `app/application/runtime_snapshot.h` | 删除无消费者的 `resultExportEnabled` | 保留连接状态和往返延迟 |
| `app/runtime/inspection_runtime.h/.cpp` | 删除只服务快照的 `resultExportEnabled()` 包装 | 保留 `ResultService::resultExportEnabled()` 和传输 Fault 接入 |
| `app/runtime/result_export_client.h` | 删除只有 `enabled` 字段的 `ResultExportRunConfiguration` | 不增加替代结构或别名 |
| `app/runtime/result_service.h` | 在 `ResultServiceRunConfiguration` 中直接保存 `bool resultExportEnabled`，删除嵌套字段和仅由其产生的头文件依赖 | 保留每轮运行配置冻结边界和 `ResultService::resultExportEnabled()` |
| `app/runtime/result_service.cpp` | 改为直接读写 `m_runConfiguration.resultExportEnabled`，按实际使用显式包含 `result_export_client.h` | 不改变结果提交、FIFO、ACK 或传输 Fault 行为 |
| `app/ui/main_window.h` | 删除两个 MainWindow 瞬时启用成员 | 不新增替代成员或缓存 |
| `app/ui/main_window.cpp` | 启动恢复复选框；切换时保存；断开不改启用；连接状态不改勾选 | 保存失败只恢复 AppSettings 正式值 |
| `app/ui/main_window_inspection.cpp` | 删除启动命令临时赋值 | 开始失败统一走现有结果呈现 |
| `app/system_support/README.md` | 将 AppSettings Store 的过时 Schema 2 说明更新为 Schema 4 | 不扩写架构或修改其他说明 |

预计修改 15 个现有生产文件和 1 个现有子系统说明文件，不新增生产代码文件，不修改 `.ui`、qmake、接收端或协议。

## 7. 明确禁止

- 不新增 ResultExportSettings Store、Policy 类、Presenter、Controller、Adapter 或 Session。
- 不保留或新建只包装 `resultExportEnabled` 的运行配置结构、别名或访问器。
- 不新增设置变更信号、启用状态同步信号、跨层 setter 或 Runtime 启用缓存。
- 不自动连接、自动重连、周期性 PING 或在启动按钮内发起连接并等待。
- 不允许警告后继续识别，不增加临时禁用、临时覆盖或本轮例外。
- 不把 TCP 连接状态、延迟、待确认数量写入 AppSettings。
- 不修改内存 FIFO、ACK、产品 ID、JSONL、CSV、接收端、Fault 恢复或退出合同。
- 不增加 Schema 3 兼容、迁移、旧键别名、缺失字段回退或离线转换工具。
- 不修改其他四种模式、模板、算法、相机、PLC、存图、统计、日志、样式和资源。
- 不新增测试工程、测试代码文件或 qmake 条目。

## 8. 实施顺序

1. 将 AppSettings 和严格 JSON 合同一次性升级到 Schema 4，并更新 AppSettings Store 的过时 Schema 说明。
2. 删除 `ResultExportRunConfiguration`，将本轮启用值直接放入 `ResultServiceRunConfiguration`。
3. 将复选框改为 AppSettings 恢复和现有 `saveConfiguration()` 提交。
4. 将 ApplicationService 启动门禁和运行配置切换为直接读取 AppSettings。
5. 删除 MainWindow、StartInspectionCommand、RuntimeSnapshot 和 InspectionRuntime 的旧瞬时状态与转发。
6. 执行旧符号、范围、编码和差异静态门禁。
7. 实施完成后同步原 TCP/CSV 方案的替代说明、计划索引和重构执行记录。
8. 交付用户统一执行 Qt Creator Release 构建和人工验证。

## 9. 验收表

### 9.1 Agent 静态验收

| 检查项 | 通过条件 |
|---|---|
| AppSettings | 只有一个 `resultExportEnabled` 持久化正式字段，默认 `false`，Schema 为 4 |
| JSON | `resultExport.enabled` 必填且严格为布尔值，保存和重载一致 |
| 旧 Schema | 无迁移、可选字段、双读和回退代码 |
| Schema 说明 | AppSettings Store 的生产源码说明和子系统 README 均为 Schema 4，无 Schema 2 残留 |
| UI 状态 | 两个 MainWindow 瞬时成员及连接驱动自动勾选逻辑为零 |
| 启动命令 | `StartInspectionCommand::resultExportEnabled` 和赋值为零 |
| 快照与 Runtime | `RuntimeSnapshot::resultExportEnabled`、快照赋值和 Runtime 包装为零 |
| 开始门禁 | 唯一检查位于 ApplicationService，并直接读取 AppSettings |
| 警告呈现 | 未连接时返回现有结构化失败；MainWindow 无第二套 TCP 预检 |
| 运行配置 | `ResultExportRunConfiguration` 和 `.resultExport.enabled` 为零；本轮值由 `ResultServiceRunConfiguration::resultExportEnabled` 直接冻结 |
| 运行合同 | `ResultService::resultExportEnabled()`、内存 FIFO、ACK 和 Fault 路径保持 |
| 范围 | `.ui`、qmake、接收端、协议、算法、PLC、存图和资源无本计划差异 |
| 文件质量 | 严格 UTF-8、无尾随空白、有末尾换行，`git diff --check` 通过 |

Agent 不运行 qmake、编译、主程序、接收端、真实相机或 PLC。

### 9.2 用户统一验证

1. 使用现有 Schema 3 设置启动新版本，确认按严格策略重置为 Schema 4 默认设置，`resultExportEnable` 默认为未启用。
2. TCP 未连接时勾选启用，退出重启后仍保持启用；连接状态仍显示未连接。
3. 已启用且 TCP 未连接时开始二维码+三期识别，确认弹出提示并且识别没有启动。
4. 点击连接成功后，复选框保持原值，不自动勾选或取消。
5. 主动断开或制造异常断开后，复选框保持原值。
6. 已启用且 TCP 已连接时正常启动，代表性 OK/二维码 NG/三期 NG 仍按现行协议发送并最终清空 FIFO。
7. 未启用但 TCP 已连接时正常启动，不产生发送记录，连接本身保持。
8. 保存启用设置失败时，复选框恢复磁盘和正式内存中的上一值，不显示虚假成功状态。
9. 切换并启动其他四种模式，确认不受结果传输启用设置影响。
10. 回归 Stop、传输 Fault、手动重连、ACK 前断线重发、退出非空提示、JSONL 去重和 CSV。

## 10. 完成条件

只有同时满足以下条件，才能标记本方案完成：

1. 用户明确授权并完成第 6 节生产代码修改；
2. 第 5 节旧状态、旧转发和连接驱动政策全部零引用；
3. AppSettings Schema 4 成为唯一持久化合同，不存在兼容或迁移代码；
4. Agent 第 9.1 节静态门禁通过；
5. 用户第 9.2 节 Qt Creator Release 构建和人工验证通过；
6. 相关计划状态、索引和执行记录同步；
7. 最终提交不包含实施开始前的用户文件或其他计划资产，不自动推送或改写历史。
