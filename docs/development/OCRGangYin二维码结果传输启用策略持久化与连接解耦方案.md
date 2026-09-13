# OCRGangYin 二维码结果输出设置说明

## 状态

二维码+三期结果输出设置采用 `barcodeCsv.enabled/outputDirectory`，具体 Schema、界面和运行行为以 `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 为准。

## 当前合同

- 输出开关和目录由 AppSettings 直接保存。
- 只有二维码+三期模式读取该设置。
- 启动识别时准备输出目录；每个正式结果写入当日 CSV。
- 设置页面只呈现本机 CSV 输出所需控件。
