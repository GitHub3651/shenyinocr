# OCRGangYin 二维码结果输出界面说明

## 状态

二维码+三期主程序界面只提供本机 CSV 输出开关和目录选择，具体行为以 `OCRGangYin二维码结果本机CSV直写主程序替换方案.md` 为准。

## 当前界面边界

- 输出开关直接绑定 `barcodeCsv.enabled`。
- 输出目录直接绑定 `barcodeCsv.outputDirectory`。
- 二维码+三期以外的模式隐藏该组设置。
- 运行期间按既有权限规则禁用设置修改。
