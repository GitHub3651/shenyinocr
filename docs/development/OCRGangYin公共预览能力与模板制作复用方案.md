# OCRGangYin 公共预览能力与模板制作复用方案

## 1. 文档状态

- 编写日期：2026-09-18。
- 当前状态：代码实施完成，静态门禁已通过，待用户统一验证。
- 权威范围：新增主工具栏预览按钮，并将普通预览与模板制作统一接入同一套相机预览能力、帧传递和画面呈现边界。
- 适用关系：普通预览和模板制作的预览能力、帧传递、画面呈现和 UI 状态均按本方案执行。
- 实施门禁：只执行本方案批准的代码、UI、资源和说明修改；Agent 只执行静态检查，不执行 qmake、构建、主程序、相机或 PLC 验证。完整构建与人工验证由用户在 Qt Creator 及现场环境完成。

## 2. 最终目标

在 `frame_mainToolbar` 中新增唯一的预览按钮。按钮只负责开始和停止普通实时预览，不进入模板制作，不冻结画面，不创建模板框线，不保存模板，不执行正式检测。

模板制作和普通预览共同调用应用服务的公共预览接口：

```text
普通预览按钮 ───────┐
                    ├─ InspectionApplicationService::startPreview()
模板制作入口 ───────┘                         │
                                             ├─ CameraSession::startPreview()
                                             ├─ previewFrameReady
                                             └─ MainWindow::presentPreviewFrame()
```

模板制作在公共预览停止后继续执行自己的冻结、框选、校验和保存流程；普通预览在公共预览停止后回到相机就绪状态。

## 3. 最终操作合同

### 3.1 普通预览按钮

控件对象名固定为：

```text
toolButton_previewAction
```

按钮位于 `frame_mainToolbar`，固定使用工具栏现有的 `76×62px` 尺寸、`27×27px` 图标和 `Qt::ToolButtonTextUnderIcon` 样式。

| 操作状态 | 文字 | 可用 | 点击结果 |
|---|---|---:|---|
| `CameraClosed` | 预览画面 | 否 | 无动作 |
| `CameraReady` | 预览画面 | 是 | 启动普通预览 |
| `CameraPreviewing` | 停止预览 | 是 | 停止普通预览 |
| `Detecting` | 预览画面 | 否 | 无动作 |
| `Stopping` | 预览画面 | 否 | 无动作 |
| `TemplatePreviewing` | 预览画面 | 否 | 无动作 |
| `TemplateFrozen` | 预览画面 | 否 | 无动作 |

普通预览使用图像设置页当前的旋转和颜色通道设置。每一张成功预处理的帧都通过唯一的跨线程 Qt 排队信号提交给主窗口，主窗口使用现有画面呈现边界显示。停止预览后保留最后一帧已显示画面。

只有 `startPreview()` 返回成功后，普通预览才进入 `CameraPreviewing` 状态并更新操作 UI；启动失败保持当前状态并显示失败提示。只有 `stopPreview()` 返回成功后，普通预览才清除 `CameraPreviewing` 状态并回到相机就绪状态；停止失败保持当前状态并显示失败提示。收到预览失败或意外停止信号时，由普通预览调用方完成状态收口和操作 UI 更新。

普通预览期间，相机操作、正式识别、模板管理、相机参数、图像参数、PLC 操作和统计清零均按统一 UI 策略禁用；只有“停止预览”按钮继续可用。普通预览不改变模板草稿、模板目录、检测统计、PLC 状态、结果状态、存图或 CSV。

### 3.2 模板制作

模板制作继续使用现有按钮和模板页状态：

```text
制作模板
→ 公共预览启动
→ 模板页进入 Previewing
→ 再次点击后停止公共预览
→ 读取 CameraSession 当前帧
→ 模板页进入 Frozen
→ 模式化框选、二维码校验、模板保存
```

模板制作独有的 `Idle/Previewing/Frozen`、模板向导、框选几何、二维码校验、保存和退出行为保持不变。公共预览能力不持有这些模板状态，也不调用模板页的框选或保存方法。

## 4. 公共预览接口

### 4.1 `CameraSession`

保留相机预览采集、预处理、当前帧和停止行为；回调名称统一为公共预览语义：

```cpp
struct CameraSessionCallbacks
{
    std::function<void(const cv::Mat &)> previewFrameReady;
    std::function<void(const QString &)> previewFailed;
    std::function<void()> previewStopped;
    std::function<void(InspectionFaultReason, const QString &)> enterFault;
};
```

`startPreview()`、`stopPreview()` 是相机层唯一的预览入口。预览帧继续在相机线程中完成预处理并更新 `m_currentImage`，不让相机线程直接操作 Qt 控件。

预览失败和意外停止只发出公共预览回调；正式检测仍走现有 Fault 回调。`previewStopped` 表示采集意外停止，由当前预览调用方完成状态收口；用户主动停止由 `stopPreview()` 的返回结果同步收口。预览停止不新增缓存队列、确认机制、流控状态或第二套采集线程。

### 4.2 `InspectionApplicationService`

公共应用接口固定为：

```cpp
OperationResult startPreview(
    int rotationCode,
    int colorChannelCode);
OperationResult stopPreview();
bool hasCurrentCameraImage() const;
cv::Mat currentCameraImageClone() const;
```

公共信号固定为：

```cpp
void previewFrameReady(cv::Mat image);
void previewFailed(QString reason);
void previewStopped();
```

`startPreview()` 统一负责：

- 检查正式运行状态和相机状态；
- 应用当前相机曝光；
- 应用旋转和颜色通道预处理设置；
- 启动 `CameraSession::startPreview()`。

`stopPreview()` 统一停止相机预览。应用层不新增普通预览专用包装、模板预览专用包装、重载、别名或转发函数。调用方按照返回结果更新所属预览状态：启动成功后进入预览状态，启动失败保持原状态；停止成功后退出预览状态，停止失败保持当前状态。应用关闭优先于普通预览状态，关闭窗口前先停止公共预览并完成收口。

错误码、日志事件和用户提示统一使用普通预览语义；不保留模板专用预览接口或兼容名称。

### 4.3 主窗口画面呈现

`MainWindow` 只保留一个预览帧接收连接和一个画面呈现入口：

```cpp
void presentPreviewFrame(const cv::Mat &image);
```

该入口继续调用：

```text
InspectionApplicationService::renderPreviewFrame()
→ InspectionPage::presentPreviewImage()
→ InspectionImageCanvas
```

普通预览激活或模板页处于 `Previewing` 时显示排队帧；普通预览停止或模板冻结后，丢弃已经排队但尚未呈现的预览帧，防止覆盖冻结画面或停止后的静态画面。

冻结帧和已加载模板原图继续通过模板页现有的一次性显示请求呈现，不重新进入实时预览帧链路。

## 5. UI 状态与权限

### 5.1 `OperationUiState`

新增唯一状态：

```cpp
CameraPreviewing
```

`MainWindow::operationUiState()` 的状态顺序固定为：

```text
正式检测 → 停止中 → 普通预览 → 模板预览 → 模板冻结 → 相机就绪 → 相机关闭
```

普通预览使用 `MainWindow` 的一个页面级状态成员记录当前预览归属；模板页继续使用自己的三态模板制作状态。两者互斥，不建立第二个状态机或通用状态管理器。

### 5.2 `OperationUiSnapshot`

新增：

```cpp
Access previewAction;
QString previewActionText;
```

`OperationUiPolicy::create()` 统一计算普通预览按钮权限、文字和禁用原因。`CameraPreviewing` 的业务禁用原因统一为“请先停止实时预览。”；模板制作状态的普通预览按钮禁用原因统一为“请先退出模板制作。”。

普通预览状态的 `statusText` 为“实时预览中”，检测信息页沿用正式 QSS 的运行中视觉状态。

### 5.3 页面绑定

`InspectionPage::applyOperationState()` 负责：

- 设置预览按钮文字；
- 在普通预览状态显示“停止预览”图标；
- 应用 `previewAction` 权限和禁用原因。

模板页只负责模板按钮、模板向导和模板编辑控件，不访问普通预览按钮，也不维护普通预览状态。

## 6. 模板制作与普通预览的调用边界

### 6.1 普通预览

```text
MainWindow::handlePreviewAction()
→ MainWindow::startPreviewOnly()/stopPreviewOnly()
→ InspectionApplicationService::startPreview()/stopPreview()
→ CameraSession
```

普通预览成功后仅更新普通预览状态和操作 UI；不进入模板绘图流程、模板向导、模板保存或模板草稿流程。

普通预览启动前模板页必须处于非预览状态；模板预览启动前普通预览必须处于非激活状态。两种预览互斥，普通预览只由 `MainWindow` 收口，模板预览只由 `TemplateEditorPage` 收口。

### 6.2 模板制作

```text
TemplateEditorPage::handleTemplateCaptureButton()
→ TemplateEditorPage::startTemplateCapturePreview()
→ InspectionApplicationService::startPreview()
```

模板页收到公共预览失败或意外停止信号后，只在自身处于 `Previewing` 时恢复模板三态和向导提示。普通预览失败或意外停止由 `MainWindow` 根据普通预览状态处理；两个调用方不互相修改对方状态。

模板冻结时先调用公共 `stopPreview()`，再读取 `currentCameraImageClone()`，然后进入 `Frozen` 和现有框选流程。公共预览接口不复制冻结逻辑。

`startTemplateCapturePreview()` 和 `stopTemplateCapturePreview()` 只负责模板页自身的预览状态、提示、日志和冻结流程，并直接调用公共 `startPreview()`/`stopPreview()`；它们不逐帧转发、缓存预览帧，也不访问普通预览按钮。模板页仅在公共停止成功后进入 `Frozen`，停止失败保持模板预览状态。

模板页仅在公共 `startPreview()` 成功后进入 `Previewing`；启动失败保持模板页原状态并显示失败提示。模板预览失败或意外停止时，由模板页收口自身状态和向导提示。

## 7. 文件级修改范围

### 7.1 生产代码

| 文件 | 修改内容 |
|---|---|
| `app/runtime/camera_session.h` | 将停止回调统一为 `previewStopped`；保留唯一相机预览接口和回调边界 |
| `app/runtime/camera_session.cpp` | 使用公共预览日志、错误和停止回调；保持预处理、当前帧和采集线程行为 |
| `app/application/inspection_application_service.h` | 将应用层预览接口和信号统一为 `startPreview/stopPreview/preview*` |
| `app/application/inspection_application_service.cpp` | 实现公共预览接口，统一错误码、提示、日志和回调转发 |
| `app/ui/main_window/main_window.ui` | 在 `frame_mainToolbar` 增加 `toolButton_previewAction`，保持工具栏固定布局和尺寸 |
| `app/ui/main_window/main_window.h` | 增加普通预览动作、状态和公共画面呈现声明 |
| `app/ui/main_window/main_window.cpp` | 连接唯一公共预览帧信号、失败/停止信号和预览按钮；删除模板语义连接 |
| `app/ui/main_window/main_window_inspection.cpp` | 实现普通预览启停、统一画面呈现、状态计算、关闭与停止收口 |
| `app/ui/main_window/operation_ui_policy.h` | 增加 `CameraPreviewing`、`previewAction` 和 `previewActionText` |
| `app/ui/main_window/operation_ui_policy.cpp` | 完成普通预览状态的权限、文字、禁用原因和状态栏文本 |
| `app/ui/main_window/inspection/inspection_page.cpp` | 绑定预览按钮文字、图标、权限和运行状态视觉 |
| `app/ui/main_window/template/template_editor_page.cpp` | 调用公共 `startPreview/stopPreview`，保留模板冻结、框选、保存和退出职责 |

不新增 PreviewManager、Controller、Facade、事件总线、状态机、缓存队列或中间信号对象。

### 7.2 资源与说明

| 文件 | 修改内容 |
|---|---|
| `app/resource/svg/preview.svg` | 新增唯一预览按钮图标，采用现有工具栏图标尺寸和视觉规范 |
| `app/resource/image.qrc` | 登记 `svg/preview.svg` |
| `app/resource/qss/app_theme.qss` | 为 `toolButton_previewAction` 增加现有工具栏同级状态样式 |
| `app/resource/README.md` | 记录预览图标由主工具栏预览按钮使用 |
| `app/ui/README.md` | 记录公共预览接口、普通预览按钮和模板制作的最终职责边界 |
| `docs/development/OCRGangYin计划索引.md` | 登记本方案并更新预览任务路由 |

不新增翻译上下文之外的文案系统，不新增设置项、Schema 字段或持久化状态。`.pro` 不新增生产源文件；新增 SVG 只登记到现有 QRC。

## 8. 必须清理的符号与零引用门禁

公共接口完成后，以下模板语义的预览符号必须在 `app/` 生产代码中零引用：

```text
startTemplatePreview
stopTemplatePreview
templatePreviewFrameReady
templatePreviewFailed
templatePreviewStopped
presentTemplatePreviewFrame
CameraSessionCallbacks::templatePreviewStopped
```

模板页的业务函数只保留模板流程语义，例如模板捕获、冻结、框选、校验和保存；不得通过同名转发函数继续包装公共预览。

以下公共符号必须各有唯一声明、定义和调用路径，不保留重载、空实现、别名或兼容转发：

```text
CameraSession::startPreview
CameraSession::stopPreview
InspectionApplicationService::startPreview
InspectionApplicationService::stopPreview
InspectionApplicationService::previewFrameReady
InspectionApplicationService::previewFailed
InspectionApplicationService::previewStopped
MainWindow::presentPreviewFrame
```

帧链路门禁：

- `CaptureWorker` 每成功取得一帧只产生一次公共预览帧通知；
- 主窗口只存在一个排队连接和一个呈现入口；
- `TemplateEditorPage` 不再逐帧中转、缓存或确认预览帧；
- 不保留第二份当前帧、会话编号、确认状态、预览缓存或图像替换包装；
- 停止预览后排队旧帧不能覆盖冻结画面或停止后的静态画面；
- 正式检测帧不进入公共预览信号。

## 9. 保持范围

以下行为不因本方案改变：

- 相机打开、关闭和正式检测的设备合同；
- 软触发、硬触发、PLC、Fault、统计、CSV 和存图；
- 图像旋转、颜色通道、曝光和当前帧预处理；
- 模板三态、模板向导、四种模板绘图步骤、二维码即时校验和模板保存；
- 模板原图显示、冻结帧来源和 `InspectionImageCanvas` 绘图边界；
- `InspectionApplicationService::renderPreviewFrame()` 到 `InspectionPage::presentPreviewImage()` 的应用到 UI 边界；
- 现有工具栏高度、按钮尺寸、QSS 主题和左侧导航布局。

普通预览不进入正式运行状态，不产生产品结果，不修改模板或运行统计。

## 10. 静态验收门禁

1. `main_window.ui` 中 `toolButton_previewAction` 唯一存在且位于 `frame_mainToolbar`。
2. `preview.svg` 在文件系统和 `image.qrc` 中各有一个有效登记。
3. `OperationUiState::CameraPreviewing` 的所有 `switch` 分支完整，编译期无遗漏枚举分支。
4. 普通预览按钮的权限、文字、Tooltip、图标和禁用原因全部由统一 UI 策略驱动。
5. 公共预览接口、信号和回调的声明、定义、连接签名一致。
6. 第 8 节模板语义符号在 `app/` 中零引用；公共预览符号无重复声明、定义或转发。
7. `TemplateEditorPage` 只调用公共预览接口，不拥有逐帧信号连接、帧缓存、确认状态或普通预览按钮访问。
8. 主窗口只有一个 `previewFrameReady` 排队连接和一个 `presentPreviewFrame()` 实现。
9. `.ui`、`.qrc`、QSS、翻译和 `.pro` 文件存在性、路径、重复项、UTF-8、末尾换行和 `git diff --check` 通过。
10. 生产代码不新增 Manager、Controller、Facade、事件总线、状态机、缓存队列或兼容接口。

静态门禁通过只表示源码、工程文件和资源关系已核对，不表示构建、运行或设备验证通过。

## 11. 用户统一验证

1. Qt Creator 执行 Run qmake、Clean 和 Release Rebuild。
2. 相机关闭时“预览画面”按钮禁用；相机打开且空闲时可用。
3. 点击“预览画面”后主画布连续刷新，状态栏显示“实时预览中”，不出现模板向导和框线。
4. 点击“停止预览”后相机采集停止，最后一帧保持显示，界面回到相机就绪状态。
5. 普通预览期间正式识别、模板管理、相机参数、图像参数、PLC 操作和统计操作均不可执行。
6. 普通预览失败或相机意外停止后，按钮和状态恢复到可用的相机就绪状态，并显示明确提示。
7. 模板制作仍能完成实时预览、拍照冻结、各模式框选、二维码校验、保存和退出。
8. 模板冻结后，已排队实时帧不能覆盖冻结画面；保存和二维码校验读取冻结帧。
9. 普通预览和模板制作都使用当前旋转、颜色通道和曝光设置。
10. 五种检测模式、正式软硬触发、PLC、结果、统计、CSV、存图和退出流程回归无变化。

## 12. 完成条件

- 普通预览按钮已加入 `frame_mainToolbar`，开始和停止行为完整可用。
- 普通预览与模板制作共用唯一公共预览能力，模板页不再拥有预览底层接口或逐帧中转职责。
- 模板语义预览接口、信号、回调、画面入口和兼容转发全部清理，生产代码零引用。
- 公共预览信号、主画面呈现入口和 UI 状态均为唯一实现。
- 不新增设置、数据格式、线程、队列、状态机、胶水层或未授权功能。
- 静态门禁通过，并由用户完成 Qt Creator、相机和现场交互验证。
