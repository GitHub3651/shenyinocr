# OCRGangYin 二维码结果输出说明

## 状态

二维码+三期主程序使用本机每日 CSV 直写，具体合同以 `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 为准。

## 当前结果链

```text
二维码+三期最终结果
    ↓
ResultService
    ↓
本机每日 CSV
```

输出目录由 `barcodeCsv.enabled/outputDirectory` 配置，单次正式结果直接完成一次 CSV 追加、统计、存图、PLC 和界面发布。

## 维护边界

主程序结果输出只维护本机 CSV 链；接收端协议与部署位于 `tools/result_receiver/` 的独立范围内。
