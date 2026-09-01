# OCRGangYin 二维码结果远程连接状态 UI 优化方案

## 1. 状态与范围

- 状态：代码实施完成，待用户统一验证。
- 日期：2026-09-01。
- 对象：主程序“二维码结果传输”区域中的 `resultExportStatus`。
- 目标：清晰显示远程接收端连接状态，并在首次连接及后续手动检查成功后显示当前往返延迟。
- 范围：状态文字、文字颜色、PING/PONG 延迟测量及其 UI 状态传播。

本方案不修改产品协议、ACK、outbox、故障、检测、设置或接收端行为。

## 2. 方案结论

Qt 5 和 Qt 6 均没有直接读取 TCP 延迟的 `QTcpSocket::latency()` 接口。本项目继续使用 Qt 5，通过原生 `QElapsedTimer` 测量现有 PING/PONG 的应用层往返耗时，不需要升级 Qt 或调用系统 `ping` 命令。

继续使用现有 `QLabel resultExportStatus`，通过 `uiState` 动态属性和 `app_theme.qss` 改变状态文字颜色：

```text
[连接] [断开连接]   [● 已连接 · 12.34 ms]
```

状态区域保持透明、无边框，只显示状态点和文字。不新增自定义 Widget、图标资源、动画、活动定时器或状态机。

## 3. 延迟测量合同

首次连接固定执行：

```text
操作员点击“连接”按钮
    ↓
建立 TCP 连接（最多 3 秒）
    ↓ TCP 连接成功
自动发送 {"ping":true}\n
    ↓ 启动 QElapsedTimer，等待 PONG（最多 3 秒）
收到 {"pong":true}\n
    ↓
记录往返耗时
    ↓
进入 Connected 并显示“● 已连接 · N.NN ms”
```

TCP 已连接但首次自动 PING/PONG 尚未完成时，状态为 `Checking`，显示 `● 正在检查…`。首次检查完成前不进入 `Connected`，也不开始待处置 outbox 提示或结果发送。

已进入 `Connected` 后，操作员再次点击“连接”按钮仍执行当前手动 PING/PONG 检查，并用最新测量值更新延迟。

测量规则：

- 计时从完整 PING 成功交给 `QTcpSocket::write()` 后开始；
- 收到对应 PONG 时读取 `QElapsedTimer::nsecsElapsed()` 并换算为毫秒；
- 延迟在状态 UI 中固定显示两位小数；
- 该数值包含发送端事件循环、TCP 传输、接收端处理和返回过程，是应用层往返延迟；
- 产品 ACK 耗时包含 JSONL 写入和 `flush`，不作为连接延迟；
- 开始新连接时清除历史延迟；
- 主动断开、TCP 异常断开、PING/PONG 检查失败或检查超时时清除延迟；
- 发送端 outbox 或其他本地持久化错误本身不清除延迟；只要 TCP 未断开且最近一次测量仍有效，就保留该值；
- PONG 超时继续使用当前 3 秒规则，并按现有逻辑断开连接。

TCP 建连和首次 PING/PONG 使用各自现有的 3 秒时限，因此一次首次连接校验在最坏情况下可能持续接近 6 秒；不把两个阶段合并成新的总超时。

PING/PONG JSON 不增加时间戳或序号。当前网络 worker 同一时刻只允许一个待确认请求，因此不需要修改协议来匹配本次测量。

本方案只在每次 TCP 连接成功后自动检查一次，不增加周期性自动心跳。产品传输期间继续由 ACK 检查通信，空闲连接只在操作员手动检查时更新延迟。

## 4. 状态显示

保持当前状态来源和判断顺序。首次 TCP 建连成功后先进入 `Checking`，只有收到 PONG 并取得有效延迟后才进入 `Connected`。

延迟只在最终显示状态为 `Connected` 时呈现。即使内存中仍保留最近一次有效测量值，`Connecting`、`Checking`、`Syncing`、待处置、错误和 `Disconnected` 状态也不得在文字中附加毫秒数。

| 条件 | 显示文字 | `uiState` | 文字颜色 |
|---|---|---|---|
| 本地结果队列未就绪 | `● 远程传输：本地队列不可用` | `error` | 红色 |
| 存在待处置记录 | `● 远程传输：N 条历史结果待处理` | `attention` | 橙色 |
| 正在连接 | `● 正在连接…` | `working` | 蓝色 |
| 正在检查连接 | `● 正在检查…` | `working` | 蓝色 |
| 正在同步历史结果 | `● 正在同步历史结果…` | `working` | 蓝色 |
| 已连接 | `● 已连接 · N.NN ms` | `online` | 绿色 |
| 已断开 | `● 已断开` | `offline` | 灰色 |

待处理数量继续使用现有 `snapshot.resultExportPendingCount`。延迟使用新增的 `snapshot.resultExportRoundTripMs`，以 `-1.0` 表示当前没有有效测量值。

状态不能只依赖颜色；普通连接状态使用简短文字，所属的“二维码结果传输”区域提供远程传输语境。

## 5. 实施范围

### `app/ui/main_window.ui`

- 二维码结果传输容器统一命名为 `groupBox_resultExport`；
- 保留现有 `QLabel resultExportStatus`；
- 接收电脑 IP 和 TCP 端口控件放入独立表单布局，与连接操作横向布局平级；
- 高度与连接按钮一致；
- 使用横向扩展尺寸策略；
- 文字水平、垂直居中。

### `app/ui/main_window.cpp`

在现有 `updateResultExportUi()` 中：

- 保持当前状态判断顺序；
- 设置对应的显示文字和 `uiState`；
- 只有最终状态为 `Connected` 且延迟有效时才追加两位小数格式的 `· N.NN ms`；
- 其他所有状态忽略已保存的延迟值，不在状态文字中显示毫秒数；
- 只在属性变化时刷新控件样式。

不改变按钮可用性和当前“已连接时再次点击执行手动检查”的行为。首次 TCP 建连成功后由 Client 自动发起一次检查。

### `app/resource/qss/app_theme.qss`

`groupBox_resultExport` 与 `groupBox_currentTemplateSettings` 使用同一卡片样式。

`resultExportStatus` 保持透明背景和无边框，只设置字体及各状态的文字颜色：

```text
offline   灰色
working   蓝色
online    绿色
attention 橙色
error     红色
```

颜色沿用当前主题已有色板，不设置状态底色，不在 C++ 中使用控件级 `setStyleSheet()`。

### `app/runtime/result_export_network_worker.h/.cpp`

- 增加一个 `QElapsedTimer` 成员；
- PING 成功写入后启动计时；
- 收到 PONG 时换算为 `double` 毫秒；
- 将 `pongReceived()` 扩展为携带延迟值的信号；
- 清理 pending、断开或失败时使当前计时失效。

`QElapsedTimer` 只读取经过时间，不产生定时事件，不增加线程或周期任务。

### `app/runtime/result_export_client.h/.cpp`

- 保存最近一次有效检查延迟，默认值为 `-1.0`；
- TCP 建连成功后先进入 `Checking` 并自动请求一次 PING/PONG；
- 接收 worker 返回的延迟后先更新数值，再恢复 `Connected`；
- 首次自动检查成功后再执行现有待处置 outbox 判断和后续发送；
- 开始新连接、主动断开、TCP 异常断开、PING/PONG 检查失败或超时时清除延迟；
- 本地 outbox 或其他持久化错误不单独清除仍然有效的延迟；
- 提供只读延迟查询，不增加第二套连接状态。

### `app/application/runtime_snapshot.h`

增加：

```cpp
double resultExportRoundTripMs = -1.0;
```

`-1.0` 表示尚未测量或当前测量已失效。

### `app/application/inspection_application_service.cpp`

构造 `RuntimeSnapshot` 时，从 `ResultExportClient` 读取当前有效延迟。状态仍沿现有 Client → ApplicationService → RuntimeSnapshot → UI 路径传播，UI 不直接订阅网络 worker。

## 6. 保持不变

- `ResultExportConnectionState` 枚举；
- TCP 建连、断开和 3 秒连接超时；
- PING/PONG JSON 格式和 3 秒检查超时；
- 产品 JSON、ACK 和 1.5 秒 ACK 超时；
- outbox、历史记录处置和故障恢复；
- ResultReceiver 的 PING/PONG 响应；
- IP、端口和启用复选框规则；
- 检测、PLC、统计、存图和算法逻辑；
- Qt 5、qmake、C++11 和现有线程模型。

不增加 ICMP Ping、系统命令、TCP KeepAlive 参数、周期性自动心跳、自动重连或网络测速功能。

## 7. 验收标准

1. 七种显示条件均使用正确文字和颜色；
2. 所有状态下均无边框和底色；
3. 首次点击连接时依次显示 `● 正在连接…`、`● 正在检查…` 和两位小数格式的 `● 已连接 · N.NN ms`；
4. 首次自动检查成功前不进入 `Connected`，不开始 outbox 处置或发送；
5. 首次自动检查失败或超时时断开，不显示无延迟值的 `● 已连接`；
6. 已连接后手动检查期间只显示 `● 正在检查…`，不同时显示旧延迟；
7. 连续执行手动检查时延迟值按最新结果更新；
8. `Connecting`、`Checking`、`Syncing`、待处置、错误和 `Disconnected` 状态均不显示毫秒数；
9. 开始新连接、主动断开和异常断开后不显示旧延迟；
10. 检查超时继续在 3 秒后断开，不把 `3000 ms` 当作有效延迟显示；
11. TCP 建连和首次检查分别保持 3 秒时限；
12. 产品发送和 ACK 过程不覆盖 PING/PONG 延迟；
13. 切换检测模式后返回时，只在当前状态为 `Connected` 时显示有效延迟；
14. 本地 outbox 或其他持久化错误未导致 TCP 断开时，不清除有效往返延迟；
15. PING/PONG JSON、ACK、outbox、故障和按钮行为保持既有合同；
16. Qt 5 + qmake + C++11 下能够正常编译。
