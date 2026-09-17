# OCRGangYin 二维码结果本机 CSV 直写方案

## 1. 状态与范围

- 编写日期：2026-09-03。
- 当前状态：代码实施完成，待用户统一验证。
- 权威范围：主程序 `app/` 内二维码+三期最终结果的本机每日 CSV 直写，以及对应设置、运行配置、界面和旧网络链清理。
- `tools/result_receiver/` 不在本方案范围内，继续按接收端说明独立维护。

## 2. 最终行为

- 设置使用严格 Schema 6，仅保留 `barcodeCsv.enabled` 和 `barcodeCsv.outputDirectory`。
- 二维码+三期启用本机记录后，启动时准备输出目录；整体 OK 的正式结果直接追加到当日 `qr_results_YYYYMMDD.csv`。
- 只在整体 OK 时追加二维码原文；整体 NG 不创建、不打开且不追加 CSV 文件。新文件或空文件写 UTF-8 BOM、无表头；字段按 CSV 规则转义并逐条 `flush()`。
- 其他四种模式不生成本机二维码 CSV。
- 主程序不再依赖远程连接、ACK、待发送队列或网络线程。

## 3. 结果边界

- CSV 写入成功后，继续现有正式结果提交、统计、存图、PLC 和界面发布顺序。
- CSV 打开、写入或 `flush` 失败进入 `BarcodeCsvUnavailable` Runtime Fault；当前产品不产生普通产品结果，统一停止链记录未完成产品并回到 `Idle`。
- 已成功追加的 CSV 行不回滚、不补偿；后续正式提交失败仍按现有 Runtime Fault 收口。
- 故障前已经形成的正式结果保持原值，未完成产品只记为未确认。
- 运行故障通过统一的一次性警告说明首因和本次运行数量。

## 4. 设置与界面

- 开关和输出目录直接绑定同一 `AppSettings`；保存失败恢复正式值并提示。
- 未选择目录时不能启用本机记录。
- 运行期间按既有权限规则禁用修改。
- 清空软件数据后，CSV 开关关闭、目录为空。

## 5. 文件边界

计划修改主程序的 AppSettings、ResultService、InspectionRuntime、InspectionApplicationService、检测设置页、MainWindow、翻译和工程清单；删除主程序远程结果传输类型、网络线程、ACK、队列和退出链。算法、模板、相机、PLC、存图、统计、UI 布局和 `tools/result_receiver/` 保持不变。

## 6. 静态门禁

- `app/` 中不存在远程结果传输类型、网络线程、ACK、待发送队列、IP/端口设置或远程状态 UI。
- 结果输出只保留 `barcodeCsv.enabled/outputDirectory` 与每日 CSV 追加。
- CSV 写入失败只通过通用 Runtime Fault 自动停止收口。
- 工程清单无缺失、重复或残留旧网络文件。
- 翻译、UTF-8、文件末尾换行和 `git diff --check` 通过。

## 7. 用户统一验证

1. 在 Qt Creator 使用 Release 执行 Run qmake、Clean、Rebuild。
2. 验证 Schema 重置、开关和目录保存、目录准备、整体 OK 追加二维码内容、整体 NG 不产生任何 CSV 写入、转义、BOM、续写和跨日文件。
3. 制造真实 CSV 写入失败，确认一次性故障警告、自动停止和未完成产品收口。
4. 回归关闭流程、五种检测模式、统计、存图和 PLC。
