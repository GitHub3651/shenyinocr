# OCRGangYin 运行状态标签统一呈现修复方案

## 1. 文档状态

- 编写日期：2026-09-19。
- 当前状态：代码已实施，静态门禁已通过，待用户统一验证。
- 当前进度：七种运行状态的文字和颜色已统一输出，标签唯一写入入口、透明背景、旧零散写入和保存失败 UI 通知链清理均已完成。
- 权威范围：`label_runtimeStatus` 的文字、文字颜色、透明背景、写入入口，以及仅为该标签服务的保存失败通知链。
- 适用关系：本方案替代其他现行方案中关于 `label_runtimeStatus` 文案和状态样式的规定，并替代仅为旧检测状态文案保留 `InspectionAcquisitionDto` 的规定；其他方案中的相机、预览、模板制作、正式检测、存图和日志行为保持不变。
- 实施门禁：只修改本方案列出的生产文件和计划索引；Agent 只执行轻量静态检查，不执行 qmake、构建、主程序或设备验证。完整构建和人工交互由用户统一完成。

## 2. 最终目标

`label_runtimeStatus` 只显示当前运行状态，不显示操作结果、图像呈现进度或保存失败信息。

运行状态文字和文字颜色由同一份 `OperationUiSnapshot` 同时给出，并由 `InspectionPage::applyOperationState()` 一次性应用。其他业务函数不再直接修改该标签的文字、动态属性或样式。

标签背景在所有状态下保持透明，边框保持统一，只有文字颜色随运行状态变化。

不新增状态机、控制器、事件总线、兼容接口、临时回退或第二套业务状态。

## 3. 唯一状态来源

继续使用现有 `OperationUiState`：

```cpp
enum class OperationUiState
{
    CameraClosed = 0,
    CameraReady,
    CameraPreviewing,
    Detecting,
    Stopping,
    TemplatePreviewing,
    TemplateFrozen
};
```

不新增运行状态枚举。`MainWindow::operationUiState()` 继续按照真实运行快照、普通预览状态和模板捕获状态计算当前状态。

`OperationUiContext` 增加当前已应用的硬触发开关值：

```cpp
bool hardwareTriggerEnabled = false;
```

该值只用于在 `Detecting` 状态选择“硬触发模式运行中”或“软触发模式运行中”，不保存第二份检测状态。`MainWindow::updateOperationUiState()` 从当前已应用设置填入该值。

## 4. 完整显示合同

### 4.1 允许显示的全部文字

`OperationUiSnapshot` 同时提供：

```cpp
QString statusText;
QString statusUiState;
```

每个 `OperationUiState` 必须完整设置这两个字段：

| 当前状态 | `label_runtimeStatus` 文字 | `uiState` | 文字颜色 |
|---|---|---|---|
| `CameraClosed` | 相机已关闭 | `idle` | `#000000` |
| `CameraReady` | 相机已打开 | `ready` | `#17834C` |
| `CameraPreviewing` | 实时预览中 | `running` | `#1769AA` |
| `Detecting` 且硬触发关闭 | 软触发模式运行中 | `running` | `#1769AA` |
| `Detecting` 且硬触发开启 | 硬触发模式运行中 | `running` | `#1769AA` |
| `Stopping` | 正在停止识别 | `stopping` | `#C98218` |
| `TemplatePreviewing` | 模板制作中：实时取景 | `warning` | `#C98218` |
| `TemplateFrozen` | 模板制作中：请完成框选并保存 | `warning` | `#C98218` |

以上是该标签最终允许显示的完整文案集合。相机打开、关闭、预览停止、检测停止和退出模板制作后，不显示一次性操作结果，直接显示落定后的当前状态。

### 4.2 初始状态

`inspection_info_page.ui` 中的初始值与 `CameraClosed` 保持一致：

```text
text = 相机已关闭
uiState = idle
wordWrap = true
```

初始值只保证界面创建到首次状态同步之间显示正确；固定换行保证模板冻结等长文案完整显示。运行后的文字和颜色仍由统一状态策略覆盖。

## 5. 单一呈现入口

`InspectionPage::applyOperationState()` 是 `label_runtimeStatus` 的唯一运行时写入入口。它在同一次调用中无条件完成：

```cpp
label_runtimeStatus->setText(operationUi.statusText);
setStyleProperty(
    *label_runtimeStatus,
    "uiState",
    operationUi.statusUiState);
```

不再使用“`statusText` 非空才更新”的逻辑。每次状态同步都同时覆盖文字和颜色属性，不能保留上一个状态的任一部分。

删除 `InspectionPage` 中根据 `OperationUiState` 再次计算颜色的 `runtimeUiState()`。文字和颜色类别均由 `OperationUiPolicy::create()` 的同一个状态分支产生，页面只负责应用结果。

## 6. 删除零散写入

### 6.1 `MainWindow`

删除 `main_window_inspection.cpp` 中所有对 `label_runtimeStatus` 的直接访问，包括以下流程中的写入：

- 相机恢复后写入“相机已打开”；
- 停止检测后写入“已停止”；
- 关闭相机后写入“相机已关闭”；
- 启动检测后写入“硬触发模式运行中”或“软触发模式运行中”；
- 退出模板制作后写入“已退出模板制作，相机已打开”或“已退出模板制作，相机已关闭”；
- 打开相机后写入“相机已打开”。

这些流程只完成自身业务操作并调用现有 `updateOperationUiState()`，由当前真实状态生成最终文字和颜色。

### 6.2 检测图像呈现

删除 `InspectionPage::present()` 对运行状态标签写入“正在显示相机采集图像...”的逻辑。

检测图像呈现只更新画布、判定、识别文字、模板名称、统计和耗时，不修改运行状态。已经排队的图像呈现请求不能覆盖停止后或其他状态下的运行状态文字。

### 6.3 特殊保留逻辑

删除 `keepDetectionWarning`。页面不再通过读取标签当前属性决定是否保留旧文字或旧颜色，也不允许临时提示占用运行状态标签。

## 7. 保存失败边界

保存失败不显示在 `label_runtimeStatus`，也不新增其他界面提示位置。现有存图执行和失败日志保持不变。

删除仅用于把保存失败转发到该标签的完整通知链：

```text
ImageSaveService::taskFailed
→ ResultService 转发连接
→ InspectionRuntime::publishImageSaveFailure()
→ InspectionRuntime::imageSaveFailed
→ MainWindow 连接
→ InspectionPage::reportImageSaveFailure()
```

同步删除：

- `InspectionPage::reportImageSaveFailure()`；
- `m_imageSaveFailedCount`；
- `m_imageSaveWarningScheduled`；
- 为延迟显示警告引入的 `QTimer`；
- 只服务于该定时器的 `InspectionPage` 根窗口引用和构造参数；
- 只服务于失败信号的 `ImageSaveService` 元对象和父对象参数；
- 对运行状态标签的 `setWordWrap(true)` 动态修改。

`ImageSaveService` 现有 `event=image_save.failed` 日志，以及 `ResultService` 对提交失败、缺少标注图和部分存图的现有日志继续保留。删除通知链不改变图像保存任务、失败计数条件或日志内容。

## 8. 透明背景与固定边框

`app_theme.qss` 中基础样式固定为透明背景和统一边框：

```css
QLabel#label_runtimeStatus {
    background-color: transparent;
    color: #000000;
    border: 1px solid #B8C4CE;
    padding: 10px;
    font-size: 26px;
    font-weight: 600;
}
```

各状态选择器只设置文字颜色：

```css
QLabel#label_runtimeStatus[uiState="idle"] {
    color: #000000;
}

QLabel#label_runtimeStatus[uiState="ready"] {
    color: #17834C;
}

QLabel#label_runtimeStatus[uiState="running"] {
    color: #1769AA;
}

QLabel#label_runtimeStatus[uiState="stopping"],
QLabel#label_runtimeStatus[uiState="warning"] {
    color: #C98218;
}
```

所有状态选择器中的 `background-color` 和 `border-color` 全部删除。状态变化不改变背景和边框。

## 9. 文件级修改范围

| 文件 | 修改内容 |
|---|---|
| `app/ui/main_window/operation_ui_policy.h` | 增加 `hardwareTriggerEnabled` 上下文和 `statusUiState` 输出 |
| `app/ui/main_window/operation_ui_policy.cpp` | 为七种状态一次生成完整文字和颜色类别 |
| `app/ui/main_window/main_window_inspection.cpp` | 填入硬触发上下文；删除全部运行状态标签直接写入 |
| `app/ui/main_window/inspection/inspection_page.h` | 删除保存失败呈现接口、计数、定时状态和根窗口依赖 |
| `app/ui/main_window/inspection/inspection_page.cpp` | 统一应用文字与颜色；删除图像呈现写入、颜色重复映射和保存失败警告 |
| `app/ui/main_window/main_window.cpp` | 删除保存失败到页面的连接；同步精简 `InspectionPage` 构造参数 |
| `app/ui/main_window/inspection/inspection_info_page.ui` | 将初始文字设为“相机已关闭”，保留 `idle` 初始属性并固定启用换行 |
| `app/resource/qss/app_theme.qss` | 背景固定透明、边框固定，各状态只改变文字颜色 |
| `app/runtime/image_save_service.h` | 删除无消费者的 `taskFailed` 信号和随之失去用途的 `QObject` 接口 |
| `app/runtime/image_save_service.cpp` | 删除对应信号发送和父对象初始化，保留现有失败日志 |
| `app/runtime/result_service.cpp` | 删除保存失败 UI 通知转发和缺少标注图时的 UI 发布调用 |
| `app/runtime/inspection_runtime.h` | 删除 `imageSaveFailed` 信号和 `publishImageSaveFailure()` |
| `app/runtime/inspection_runtime.cpp` | 删除 `publishImageSaveFailure()` 实现 |
| `app/application/inspection_application_service.h` | 删除只服务于旧标签写入的 `InspectionAcquisitionDto` 和结果字段 |
| `app/application/inspection_application_service.cpp` | 删除采集类型转换和结果赋值 |
| `docs/development/OCRGangYin应用运行时界面边界精简方案.md` | 同步 Runtime 对 UI 的最终信号边界和存图失败日志职责 |
| `docs/development/OCRGangYin范围控制与净结果整改方案.md` | 同步检测状态文字的当前数据来源，清理采集类型 DTO 合同 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案和任务路由 |

不新增生产源文件、资源、设置字段、Schema 字段或界面组件。

## 10. 实施阶段

### 阶段一：完整状态输出

1. 为 `OperationUiContext` 增加当前已应用的硬触发值。
2. 为 `OperationUiSnapshot` 增加 `statusUiState`。
3. 在 `OperationUiPolicy::create()` 中为每个枚举分支同时设置 `statusText` 和 `statusUiState`。
4. 将检测模式文字从 `startInspection()` 的直接写入迁入策略。

### 阶段二：唯一标签写入

1. 让 `InspectionPage::applyOperationState()` 无条件同时应用文字和颜色属性。
2. 删除 `runtimeUiState()`、空文字跳过和 `keepDetectionWarning`。
3. 删除主窗口、图像呈现和模板退出流程中的直接标签写入。
4. 同步 `.ui` 初始文字。

### 阶段三：保存失败通知清理

1. 删除页面保存失败提示和主窗口连接。
2. 删除 Runtime 与 ResultService 的纯 UI 转发。
3. 删除 `ImageSaveService` 无消费者的失败信号及其元对象接口。
4. 删除只服务于旧检测状态文字的采集类型结果数据。
5. 保持现有存图行为和日志不变。

### 阶段四：透明样式和静态检查

1. 将标签背景固定为透明，状态规则只保留文字颜色。
2. 检查所有旧文案和直接写入零残留。
3. 检查声明、定义、连接、构造签名和 include 一致。
4. 同步仍然有效的架构和范围文档，不保留已删除合同的现行描述。
5. 执行 UI XML、UTF-8、末尾换行和 `git diff --check`。

## 11. 静态验收门禁

1. `label_runtimeStatus` 的运行时代码访问只存在于 `InspectionPage::applyOperationState()`；`.ui` 初始值和 QSS 样式不计入运行时写入。
2. `OperationUiPolicy::create()` 的七个状态分支均同时设置非空 `statusText` 和 `statusUiState`。
3. 不存在根据空 `statusText` 跳过更新的代码。
4. 不存在 `runtimeUiState()`、`keepDetectionWarning` 或其他文字与颜色分开维护的逻辑。
5. `label_runtimeStatus` 的旧写入路径中不再出现以下文案，其他控件和业务提示中的同词不属于本门禁：

```text
已停止
正在显示相机采集图像...
已退出模板制作，相机已打开
已退出模板制作，相机已关闭
保存失败：未采集到标注图像。
保存图像失败：累计
```

`inspection_info_page.ui` 中该标签原有的“停止”同步替换为“相机已关闭”。

6. `reportImageSaveFailure`、`imageSaveFailed`、`publishImageSaveFailure`、`taskFailed`、`InspectionAcquisitionDto`、`acquisitionKind` 和 `acquisitionDto` 在生产代码中零引用。
7. QSS 中 `label_runtimeStatus` 只有基础规则设置 `background-color: transparent`；状态规则不存在背景色和边框色。
8. `.ui` 初始文字为“相机已关闭”，初始 `uiState` 为 `idle`，并固定启用换行。
9. 不新增状态枚举、状态管理器、兼容路径、回退逻辑或新的保存失败 UI。
10. `git diff --check`、UI XML、声明定义和连接签名检查通过。

静态门禁通过只表示代码与工程关系已核对，不表示构建、运行或设备验证通过。

## 12. 用户统一验证

1. Qt Creator 执行 Run qmake、Clean 和 Release Rebuild。
2. 启动后未打开相机时显示“相机已关闭”，文字为黑色，背景透明。
3. 打开相机后显示“相机已打开”，文字为绿色，背景透明。
4. 开始普通预览后显示“实时预览中”，文字为蓝色；停止后立即恢复“相机已打开”和绿色。
5. 软触发识别期间显示“软触发模式运行中”，硬触发识别期间显示“硬触发模式运行中”，两者均为蓝色。
6. 停止识别过程中显示“正在停止识别”和橙色；停止完成后根据相机实际状态显示“相机已打开”或“相机已关闭”。
7. 模板实时取景显示“模板制作中：实时取景”，冻结后显示“模板制作中：请完成框选并保存”，两者均为橙色。
8. 退出模板制作后直接显示相机实际状态，不显示退出操作结果。
9. 检测图像持续到达或停止前存在排队图像时，运行状态文字和颜色不被图像呈现覆盖。
10. 图像保存失败时，运行状态文字和颜色不变化；失败信息继续写入现有日志。
11. 所有状态下标签背景透明、边框一致，只有文字颜色变化；长文案能够完整换行显示。

## 13. 完成条件

- `label_runtimeStatus` 只显示第 4 节规定的当前运行状态文字。
- 文字和文字颜色由同一个状态策略分支生成，并由一个页面入口同时应用。
- 主窗口、图像呈现、模板流程和保存失败流程不再直接访问该标签。
- 保存失败标签提示及其纯 UI 转发链完整删除，现有存图和日志行为保持不变。
- 标签背景始终透明，状态切换只改变文字颜色。
- 静态门禁通过，并由用户完成 Qt Creator 与实际交互统一验证。
