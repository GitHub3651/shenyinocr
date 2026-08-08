# OCRGangYin 测试工程

本目录是架构重构的离线测试入口。Agent只维护源码和工程清单，不在命令行、脚本或GUI自动化中构建、链接或运行测试。

## Qt Creator验证步骤

1. 在Qt Creator中打开 `tests/tests.pro`。
2. 选择与主程序一致的Qt 5.14.2、MSVC 2017 64-bit Kit，并使用Release配置。
3. 首次打开或工程清单变化后执行 **Run qmake**。
4. Build `tissue_roll_detector_baseline_test`。
5. 运行该测试目标，确认4个测试函数全部通过。

测试目标链接完成后会把主程序Release发布包使用的x64
`opencv_world341.dll`，以及当前Kit的
`Qt5Core.dll`、`Qt5Test.dll`（Debug为带`d`后缀版本）复制到EXE目录，
并读取PE头确认每个DLL与目标EXE架构一致，再逐个校验源文件和目标文件的SHA-256。
这样即使Qt Creator把SUBDIRS工程配置为
自定义可执行程序、未把Qt `bin`加入运行时PATH，测试也能独立启动。若部署失败，
构建应直接失败，不能留下一个链接成功但在`main()`前因缺少运行库而崩溃的测试目标。

仓库当前`third_party/opencv/x64/vc15/bin`下的两个OpenCV DLL实际是x86，
不能供64位测试目标使用；Debug测试会由架构校验明确阻止。当前门禁统一使用Release。

测试结果需由用户反馈并写入 `docs/development/OCRGangYin重构执行记录.md`。在收到结果前，Stage 0门禁保持未通过。

## 固定样本

`baseline/sample_manifest.tsv` 是五种模式的样本和可观察结果登记表。样本图可能包含生产信息，不要求直接提交仓库；可以填写受控绝对路径或脱敏后放在仓库外的固定目录。每种模式至少需要OK、产品NG和失败输入各一份。
