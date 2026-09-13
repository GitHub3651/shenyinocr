# OCRGangYin 二维码结果接收端说明

## 状态与范围

主程序二维码+三期结果由 `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 定义并直接写入本机每日 CSV。本文件只说明独立 `tools/result_receiver/` 的维护范围。

## 接收端职责

- 接收二维码+三期结果的 TCP JSON 消息。
- 按产品 ID 保证 JSONL 业务记录幂等。
- 为每个接收日期生成无表头 CSV。
- 保持既有 PING/PONG、ACK 和手动全日期 CSV 同步协议。

## 边界

- `tools/result_receiver/` 独立运行，主程序不依赖其连接状态。
- 主程序不包含远程连接设置、网络工作线程或待发送队列。
- 接收端维护只在用户明确指定 `tools/result_receiver/` 时开展。

## 验证

接收端变更应验证 JSON 协议、重复 ID、JSONL 记录、日 CSV 和手动同步；主程序变更按本机 CSV 方案验证。
