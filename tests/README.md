# OCRGangYin 测试工程

本目录是架构重构的离线测试入口。Agent只维护源码和工程清单，不在命令行、脚本或GUI自动化中构建、链接或运行测试。

## Qt Creator验证步骤

1. 在Qt Creator中打开 `tests/tests.pro`。
2. 选择与主程序一致的Qt 5.14.2、MSVC 2017 64-bit Kit，并使用Release配置。
3. 首次打开或工程清单变化后执行 **Run qmake**。
4. Build `product_recipe_test`、`tissue_roll_detector_baseline_test`、`ocr_detection_pipeline_test`、`stamp_detection_pipeline_test`、`word_detection_pipeline_test`、`barcode_word_detection_pipeline_test`和`profile_pose_selector_test`。
5. 分别运行七个测试目标，确认各自4个业务测试函数全部通过。

`recipe_tests`验证Stage 1基础产品配方的五种模式ID、Schema 1 JSON往返、
非法字段/越界资源路径拒绝和不可变运行快照；它不读写旧模板目录，也不改变主程序入口。

`detection_tests`是五种检测Pipeline的子工程集合。纸巾目标验证配方唯一默认阈值6.0、
显式参数传入和原失败诊断；深度OCR目标通过内存假识别回调验证原按字节清洗、
换行拼接、非空精确匹配OK、空或不等NG；钢印目标验证原目标字符计数、
字符数量与无重叠两条件AND判定，以及缺少重叠配置时保持NG。
字库目标验证原目标单元解析、无可解析单元时按字符串长度回退、检测数量相等OK及少/多均NG。
二维码+三期目标验证二维码不可读时不调用日期检测、可读但日期阶段不可用时不输出最终OK、可读且日期OK/NG时的组合判定。
Profile选择目标验证无效候选跳过、严格最高分选择、同分保留先出现Profile，以及二维码相对多边形随命中位姿旋转平移。
这些测试不会加载Paddle模型，也不会调用相机、PLC、存图或主界面。

测试目标链接完成后会把当前Kit的`Qt5Core.dll`、`Qt5Test.dll`
（Debug为带`d`后缀版本）复制到EXE目录；需要OpenCV的五个检测目标还会复制主程序
Release发布包使用的x64 `opencv_world341.dll`，
并读取PE头确认每个DLL与目标EXE架构一致，再逐个校验源文件和目标文件的SHA-256。
这样即使Qt Creator把SUBDIRS工程配置为
自定义可执行程序、未把Qt `bin`加入运行时PATH，测试也能独立启动。若部署失败，
构建应直接失败，不能留下一个链接成功但在`main()`前因缺少运行库而崩溃的测试目标。

仓库当前`third_party/opencv/x64/vc15/bin`下的两个OpenCV DLL实际是x86，
不能供64位测试目标使用；Debug测试会由架构校验明确阻止。当前门禁统一使用Release。

测试结果需由用户反馈并写入 `docs/development/OCRGangYin重构执行记录.md`。
新代码切片在收到对应结果前保持“迁移中”；基础配方和纸巾Pipeline切片已于2026-08-12通过用户Qt Creator Release门禁。

## 固定样本

`baseline/sample_manifest.tsv` 是五种模式的样本和可观察结果登记表。样本图可能包含生产信息，不要求直接提交仓库；可以填写受控绝对路径或脱敏后放在仓库外的固定目录。每种模式至少需要OK、产品NG和失败输入各一份。
