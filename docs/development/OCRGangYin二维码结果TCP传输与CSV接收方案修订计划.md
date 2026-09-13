# OCRGangYin 二维码结果输出范围说明

## 状态

二维码+三期主程序结果输出以 `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 为准。独立接收端的协议与 JSONL/CSV 行为由 `OCRGangYin二维码结果TCP传输与CSV接收方案.md` 说明。

## 当前边界

- 主程序直接将二维码+三期最终结果追加到本机每日 CSV。
- `tools/result_receiver/` 保持独立维护和部署。
- 主程序与接收端的变更分别验证，不共享运行状态或设置。
