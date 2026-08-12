# OCRGangYin 测试工程

本目录是架构重构的离线测试入口。Agent只维护源码和工程清单，不在命令行、脚本或GUI自动化中构建、链接或运行测试。

## Qt Creator验证步骤

1. 在Qt Creator中打开 `tests/tests.pro`。
2. 选择与主程序一致的Qt 5.14.2、MSVC 2017 64-bit Kit，并使用Release配置。
3. 首次打开或工程清单变化后执行 **Run qmake**。
4. Build `product_recipe_test`和`tissue_roll_detector_baseline_test`。
5. 分别运行两个测试目标，确认各自4个测试函数全部通过。

`recipe_tests`验证Stage 1基础产品配方的五种模式ID、Schema 1 JSON往返、
非法字段/越界资源路径拒绝和不可变运行快照；它不读写旧模板目录，也不改变主程序入口。

`detection_tests`验证Stage 1纸巾切片：配方唯一默认阈值为6.0，
显式`TissueRecipeParameters`能传入纸巾Pipeline，空图与纯黑图仍按原有诊断拒绝。
测试不会调用相机、PLC、存图或主界面。

测试目标链接完成后会把当前Kit的`Qt5Core.dll`、`Qt5Test.dll`
（Debug为带`d`后缀版本）复制到EXE目录；纸巾检测目标还会复制主程序
Release发布包使用的x64 `opencv_world341.dll`，
并读取PE头确认每个DLL与目标EXE架构一致，再逐个校验源文件和目标文件的SHA-256。
这样即使Qt Creator把SUBDIRS工程配置为
自定义可执行程序、未把Qt `bin`加入运行时PATH，测试也能独立启动。若部署失败，
构建应直接失败，不能留下一个链接成功但在`main()`前因缺少运行库而崩溃的测试目标。

仓库当前`third_party/opencv/x64/vc15/bin`下的两个OpenCV DLL实际是x86，
不能供64位测试目标使用；Debug测试会由架构校验明确阻止。当前门禁统一使用Release。

测试结果需由用户反馈并写入 `docs/development/OCRGangYin重构执行记录.md`。
新代码切片在收到对应结果前保持“迁移中”；基础配方切片已于2026-08-12通过用户Qt Creator Release门禁。

## 固定样本

`baseline/sample_manifest.tsv` 是五种模式的样本和可观察结果登记表。样本图可能包含生产信息，不要求直接提交仓库；可以填写受控绝对路径或脱敏后放在仓库外的固定目录。每种模式至少需要OK、产品NG和失败输入各一份。
