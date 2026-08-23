# OCRGangYin 整体架构复杂度与精简审查

## 1. 文档说明

- 文档类型：当前代码架构审查报告。
- 审查日期：2026-08-21。
- 审查范围：`app/AutoOCRproject.pro`、`app/runtime`、`app/application`、`app/contracts`，并反向检查其对 `ui`、`startup`、`templates`、`detection`、`devices` 和 `engines` 的影响。
- 本文性质：只记录当前代码事实、问题判断和推荐精简顺序，不代表已经实施这些修改。
- 行为边界：不建议删除已经验证的 Runtime 状态机、产品身份、故障锁定、设备异常保护、有界队列、异步存图和模板安全保存机制。

## 2. 总体结论

当前真正存在明显过度设计的是 `application` 和 `runtime` 的交界；`contracts` 是一个没有必要独立存在的抽象层；`AutoOCRproject.pro` 主要是显式构建清单很长，不是架构复杂度的主要来源。

当前软件并非所有地方都设计过度。初次重构主要留下了大量 DTO、回调、转发接口、微型合同文件和模板式注释。生产运行效率总体尚可，但开发效率、理解成本和编译效率不理想。

## 3. 整体量化结果

| 范围 | 代码文件 | 代码行 | 评价 |
|---|---:|---:|---|
| `app/runtime` | 21 | 4,561 | 必要复杂度较多，但结果/UI 链过度 |
| `app/application` | 15 | 2,944 | 当前最大的过度设计区域 |
| `app/contracts` | 3 | 195 | 文件不多，但独立抽象层没有必要 |
| `app/AutoOCRproject.pro` | 1 | 239 | 构建清单长，不等于架构复杂 |

`runtime/application/contracts` 三个代码目录合计约 7,700 行，占整个非 vendor 代码的约三分之一。由于它们又位于全部业务流程的中心，因此给开发者造成的复杂感远高于三分之一。

## 4. 当前最绕的真实调用链

当前检测结果显示大致经过：

```text
InspectionPage
  创建 9 个控件更新回调
       ↓
InspectionViewBindingsDto
       ↓
InspectionApplicationService::bindView()
  手动复制和转换 9 个回调
       ↓
InspectionPresentationViewBindings
       ↓
ResultService
       ↓
InspectionPresentationRenderer
       ↓
再次调用 9 个 UI 回调
       ↓
InspectionPage 更新控件
```

对应代码：

- UI 回调合同：`app/application/inspection_ui_contract.h:22`。
- Runtime 又定义一套几乎相同的回调：`app/runtime/inspection_presentation_renderer.h:21`。
- Application 手动转换：`app/application/inspection_application_service.cpp:1137`。

这就是当前结构难以理解的主要原因：数据不是直接从检测结果流向界面，而是为了分层绕了一整圈。

更合理的流程应当是：

```text
DetectionWorker
    ↓
ResultService：统计、PLC、存图、形成一次完整结果
    ↓
一个 InspectionPresentation
    ↓
InspectionPage::present()
    ↓
统一更新所有 UI
```

目标是不再传递 9 个独立控件回调，也不再定义两套几乎相同的 `ViewBindings`。

## 5. `AutoOCRproject.pro` 审查

### 5.1 主要不是过度设计

`app/AutoOCRproject.pro:31` 开始的构建清单当前列出了：

- 67 个 `.cpp`。
- 79 个 `.h`。
- 总计 146 个代码文件。
- 其中 27 个是 vendor 适配或 Paddle/Snap7 相关文件。

检查结果：

- 所有 146 个文件都存在。
- 没有漏列代码文件。
- 没有已经删除却仍然残留的文件。
- 没有重复列出的代码文件。

因此 `.pro` 文件很长，主要是因为 qmake 使用显式文件清单，它不是架构本身的主要问题。

### 5.2 已确认的冗余

OpenCV 被配置了两套头文件路径：

- `app/AutoOCRproject.pro:203`：`third_party/opencv/include`。
- `app/AutoOCRproject.pro:224`：`third_party/opencv/x64/vc15/include`。

已经比对第二个目录中的 397 个文件：

- 全部都存在于第一个目录。
- 397 个文件内容完全一致。

因此第二套 OpenCV `INCLUDEPATH/DEPENDPATH` 可以删除。

另外，当前存在：

```qmake
INCLUDEPATH += $$THIRD_PARTY/Libraries/win64
DEPENDPATH += $$THIRD_PARTY/Libraries/win64
```

该目录只有 `snap7.lib`、`snap7.dll`，没有头文件，因此不应加入 `INCLUDEPATH`。

### 5.3 不建议的做法

不建议使用 `files(**/*.cpp)` 通配符自动收集代码文件。虽然 `.pro` 会变短，但废弃文件和临时文件也可能被静默加入工程，反而更难检查。

也不建议仅仅为了让 `.pro` 变短，再拆出五六个 `.pri` 文件。这只会把复杂度搬到其他文件，不会减少代码。

推荐的最简单处理：

1. 保留一个 `.pro`。
2. 删除重复路径。
3. 按 `application/runtime/detection/ui/vendor` 重新排列构建项。
4. 增加少量分区注释。

## 6. `application` 审查

### 6.1 `InspectionApplicationService` 已成为大总管

`app/application/inspection_application_service.cpp` 有 1,321 行，同时管理：

- 相机打开和关闭。
- 相机曝光和增益。
- 模板实时取景。
- PLC 连接和参数。
- 检测启动和停止。
- 模板运行加载。
- Runtime 快照。
- 故障翻译。
- 统计清零。
- UI 结果绑定。

应用服务本来应该负责组织一次用户用例，但现在同时承担了设备门面、Runtime 门面和 UI 结果桥接。

不应再把这些功能拆成更多 `Manager` 或 `Service`，否则会继续过度设计。更合适的方向是删除无意义的转发和重复合同，让该服务只保留真正的用户操作。

### 6.2 相机结果定义了两套相同数据

Runtime 在 `app/runtime/camera_session.h:22` 定义：

- `InspectionCameraOpenIssue`。
- `InspectionCameraOpenResult`。
- `InspectionCameraParameterResult`。
- `InspectionCameraRecoveryResult`。

Application 又在 `app/application/camera_application_contract.h:17` 定义一套 DTO，然后在以下位置逐字段复制：

- `cameraParameterDto()`：`app/application/inspection_application_service.cpp:194`。
- `cameraOpenDto()`：`app/application/inspection_application_service.cpp:229`。
- `cameraRecoveryDto()`：`app/application/inspection_application_service.cpp:262`。

这是典型的为了分层而分层。当前程序只在同一进程内运行，不是网络服务，没有必要保留一比一 DTO。应当只保留一套中立的相机结果结构。

### 6.3 多个微型文件只服务一个调用者

以下文件基本只有一个直接消费者：

- `camera_application_contract.h`。
- `runtime_snapshot.h`。
- `template_editor_contract.h`。
- `inspection_start_preflight.h/.cpp`。

其中 `InspectionStartPreflight` 只有两个静态函数，内容只是几组状态 `if`，见 `app/application/inspection_start_preflight.cpp:18`。

这些检查需要保留，但不需要专门一个类和两个文件。作为 `InspectionApplicationService.cpp` 内部辅助函数更容易理解。

### 6.4 打开相机隐式连接 PLC

`InspectionApplicationService::openCamera()` 位于 `app/application/inspection_application_service.cpp:680`，内部第 700 行主动连接 PLC。

但主窗口启动一秒后又会自动连接 PLC，见 `app/ui/main_window.cpp:432`。

当前实际行为是：

```text
程序启动 → 尝试连接 PLC
点击打开相机 → 又尝试连接 PLC
```

“打开相机”不应该暗中附带“连接 PLC”。这是旧逻辑遗留的职责耦合。推荐改为：

- 打开相机只处理相机。
- 连接 PLC 只处理 PLC。
- 是否启动时自动连接 PLC 只能有一个明确入口。

### 6.5 重复代码

保存自动修正曝光值的同一段 Lambda 在应用服务中重复了四次：

- `inspection_application_service.cpp:457`。
- `inspection_application_service.cpp:611`。
- `inspection_application_service.cpp:707`。
- `inspection_application_service.cpp:1095`。

这段代码可以直接收为一个私有辅助函数，不需要新建类。

### 6.6 推荐保留的 Application 结构

```text
application/
├─ application_result.h
├─ inspection_application_service.h/.cpp
├─ settings_application_service.h/.cpp
├─ template_application_service.h/.cpp
└─ template_geometry_service.h/.cpp
```

其余微型合同优先合并。预计可以从 15 个代码文件精简至约 9～10 个。

## 7. `runtime` 审查

### 7.1 必须保留的部分

以下结构不是过度设计：

- `CaptureWorker` 相机采集线程。
- `DetectionWorker` 检测线程。
- 有界队列和反压。
- `InspectionRuntime` 状态机。
- 产品 `ProductKey` 和一次结果去重。
- Fault 锁定和人工确认恢复。
- `InspectionPlcController`。
- 异步 `ImageSaveService`。
- 软触发和硬触发的不同队列策略。

工业视觉软件需要处理硬件断连、线程停止、旧结果晚到和 PLC 写入失败。这些防线不能因为 UI 按钮已经禁用就删除。

### 7.2 Runtime 与 ResultService 相互依赖

当前关系：

- Runtime 拥有 ResultService：`app/runtime/inspection_runtime.h:190`。
- ResultService 又保存 Runtime 引用：`app/runtime/result_service.h:179`。
- Runtime 公开 `resultService()`：`app/runtime/inspection_runtime.h:152`。
- Application 再直接访问 `runtime->resultService()` 十余次。

这不是内存泄漏意义上的循环，但属于职责循环：

```text
Runtime 拥有 ResultService
ResultService 又通过 Runtime 完成产品、写 PLC、进入 Fault、提交 UI
Application 又绕过 Runtime 直接操作 ResultService
```

推荐调整为：

- `ResultService` 保持 Runtime 的私有内部组件。
- 删除公开的 `resultService()`。
- UI 呈现不再由 Runtime 直接绑定控件回调。
- ResultService 只向外提交一次完整结果快照。

### 7.3 ResultService 职责过多

`app/runtime/result_service.cpp:623` 附近的结果流程同时处理：

- 最终结果去重。
- 统计。
- 延迟 NG 队列。
- PLC 脉冲。
- 图像保存。
- 图像标注绘制。
- UI 回调。
- ROI 提示。
- 模板名称显示。
- 耗时字符串格式化。

“结果统一收口”这个概念正确，但 UI 控件显示和字符串样式不应位于 Runtime。

不建议继续拆出 `StatisticsManager`、`PlcManager`、`PresentationManager` 等新类。最简单的修复是先把 UI 呈现移出去，ResultService 会自然缩小。

### 7.4 `UiCompletionMailbox` 行为应保留，独立文件不一定保留

容量为一的结果邮箱可以防止 Qt UI 队列无限堆积，这是正确设计。

但它只服务 ResultService/Runtime，可以改为 ResultService 的私有实现，不一定需要独立的 `.h/.cpp` 两个文件。

### 7.5 异常被静默吞掉

`app/runtime/result_presentation_mailbox.cpp:73` 捕获所有异常后只返回 `false`，调用方只记录 `cancelled or empty`。

这会把真正的 UI 呈现异常误认为普通取消。线程边界捕获异常是必要防御，但至少应该：

- 记录明确日志。
- 生产运行中进入 Runtime Fault。
- 不静默忽略。

## 8. `contracts` 审查

### 8.1 建议最终删除该目录

`contracts` 只有 3 个代码文件、195 行，但目录名称过于抽象，而且装了两个无关概念：

```text
contracts/
├─ detection_mode.h/.cpp
└─ barcode_parameter_defaults.h
```

它没有形成真正的合同层。

### 8.2 检测模式存在三种身份

当前同时存在：

- `DetectionMode` 枚举。
- `modeId`，例如 `stamp`。
- `uiId`，例如 `stamp_detection`。

定义位于 `app/contracts/detection_mode.h:29`。

实际 JSON 已经保存稳定 `modeId`，因此 `uiId` 没有必要。推荐：

- C++ 内部使用 `DetectionMode`。
- JSON 和 ComboBox `itemData` 使用稳定 `modeId`。
- 中文名称保持独立，允许随意修改。
- 删除 `uiId` 和相关转换函数。
- 不改变现有 JSON Schema。

`isWordFamilyMode()` 当前没有任何真实调用，可以直接删除，见 `app/contracts/detection_mode.cpp:112`。

### 8.3 二维码同一组参数定义了三遍

当前有：

- `TemplateBarcodeParameters`：`app/templates/template_store.h:26`。
- `TemplateBarcodeValidationOptions`：`app/application/template_editor_contract.h:10`。
- `BarcodeDecodeOptions`：`app/engines/barcode/barcode_types.h:45`。

三套结构都是相同的四个字段，属于过度设计。

建议只保留：

- 模板持久化结构中的二维码参数。
- 运行解码使用的 `BarcodeDecodeOptions`。

编辑校验直接转换一次或直接使用现有结构，不再增加第三套。

最终可以：

- 将 `detection_mode.h/.cpp` 移到 `app/detection/`。
- 将二维码默认值并入 `barcode_types.h`。
- 删除整个 `app/contracts` 目录。

## 9. 其他目录检查结果

### 9.1 TemplateStore 存在明显重复读盘

`TemplateStore::readSummary()` 位于 `app/templates/template_store.cpp:809`，当前流程是：

```text
readSettings()
→ loadEditable()
→ loadPrepared()
   → 再次 loadEditable()
   → 再次读取图片和字符资源
```

新增模板文件夹时，选择对话框先调用一次 `readSummary()`，随后 `addPath()` 又调用一次。

因此一次添加模板可能重复读取、解析和解码同一套模板资源多次。这是实质性的低效编程。

最简单的方案：

- `readSummary()` 复用第一次得到的 `EditableTemplate`。
- `loadPrepared()` 使用一个仅在 `TemplateStore.cpp` 内部存在的“从 EditableTemplate 准备”函数。
- `addPath()` 复用已经读取的 Summary。
- 不增加新类。

### 9.2 预览图像存在连续深拷贝

当前连续出现：

- ApplicationService `image.clone()`：`app/application/inspection_application_service.cpp:296`。
- TemplateEditorPage 再次 clone：`app/ui/pages/template_editor_page.cpp:408`。
- MainWindow 再次 clone：`app/ui/main_window.cpp:381`。

高分辨率相机图像连续复制三次会浪费内存带宽。

相机 SDK 缓冲区边界保留一次深拷贝是必要的；后续只读传递可以使用 `cv::Mat` 的引用计数浅拷贝。

### 9.3 DetectionResult 携带过多 UI/存图策略

`DetectionResult` 位于 `app/detection/common/detection_pose.h:64`，其中包含：

- `modeId`。
- `clearImageLabelRects`。
- `showRoiWarningOnCancelled`。
- `saveRawOnly`。
- `saveNotEvaluatedAsNg`。
- `elapsedDecimals`。

这些大部分是当前运行模式的固定策略，却被复制到每一件产品的检测结果中，再由 `app/detection/detection_registry.cpp:78` 的 `applyDescriptorPolicy()` 每次写入。

推荐在启动检测时形成一次模式运行策略，由 ResultService 使用。DetectionResult 只返回算法结果、文字、模板、图形和耗时。

### 9.4 编译依赖过重

`detection_pose.h` 有 253 行，直接被 17 个文件包含，而且使用重量级 `opencv2/opencv.hpp`，实际主要只需要 core 数据类型。

`camera_session.h` 又包含整个 `inspection_runtime.h`，Runtime 头文件继续包含 ResultService、Renderer、TemplateStore 和 DetectionRegistry。

这不会明显拖慢现场检测，但会拖慢编译和维护。可以通过以下方式改善：

- 使用 `opencv2/core.hpp`。
- 删除未使用 include。
- 增加前置声明。
- 避免公共头文件包含 ResultService/Renderer。

不需要引入 PIMPL 或新框架。

### 9.5 自动生成式注释严重干扰阅读

整个 `app` 中约有 1,032 行如下模板式注释：

```cpp
// 函数说明：xxx 函数实现名称所表示的处理步骤。
// 组件说明：xxx 组件封装对应业务职责和生命周期边界。
```

仅 `runtime/application/contracts` 就有 497 行，占这三个目录约 6.5%。

这些注释基本只是在重复函数名，是初次重构留下的明显噪声。删除后不会影响功能，却会显著提高代码可读性。

## 10. 最终评价

| 项目 | 评价 |
|---|---|
| 运行时性能 | 中上，队列、线程、反压和异步存图设计合理 |
| 工业安全性 | 较好，Fault、产品身份和队列上限应该保留 |
| 代码简洁度 | 较差，DTO、回调和微型文件过多 |
| 架构可理解性 | 较差，Application 与 Runtime 边界绕行 |
| 防御性编程 | Runtime 多数合理；UI 空指针、重复加载、静默 catch 有过度 |
| 初次重构遗留 | 明显存在，主要是合同层、Presentation 链和模板式注释 |

## 11. 推荐精简顺序

1. 删除无效注释、死函数、死参数、重复 include 和 `.pro` 重复路径。
2. 合并 Application 微型合同和 Preflight 文件。
3. 删除 `uiId`，内部统一使用 `DetectionMode` 和稳定 `modeId`。
4. 合并二维码重复参数，删除 `contracts` 目录。
5. 将 9 回调结果链改为一个 `InspectionPresentation`。
6. 让 ResultService 成为 Runtime 私有组件，删除公开 `resultService()`。
7. 修复 TemplateStore 重复加载和预览图像重复 clone。
8. 最后再判断是否合并 FrameQueue、UiCompletionMailbox 等单消费者文件。

## 12. 明确不建议删除的结构

不建议删除以下结构：

- Runtime 状态机。
- 产品身份和一次结果约束。
- Fault 锁定与人工确认恢复。
- 相机和 PLC 设备异常保护。
- 有界检测队列和 UI 反压。
- 异步图像保存。
- 模板临时目录、备份和回滚保存。

这些是真正的工业软件防线，不属于过度设计。

## 13. 审查状态

本文记录的是当前代码审查结果。创建本文时没有修改任何生产代码，也没有宣称上述精简方案已经实施或通过构建、相机、PLC、模板和真实样本验证。
