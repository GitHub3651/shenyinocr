# OCRGangYin 模板预览帧链路精简方案

## 1. 范围

- 状态：代码实施完成，待用户统一验证。
- 只精简点击“制作模板”后进入的实时预览帧链路。
- 当前按钮、`Idle/Previewing/Frozen` 状态、框选、保存、退出和正式识别行为全部保持不变。

## 2. 当前问题和目标

修改前，每张预览帧都要经过：

```text
CameraSession
→ previewFrameReady(sessionId, image)
→ InspectionApplicationService
→ templatePreviewFrameReady(sessionId, image)
→ TemplateEditorPage::handlePreviewFrame()
→ acknowledgeTemplatePreviewFrame()
→ m_lastPreviewFrame
→ previewFramePresentationRequested
→ MainWindow
→ InspectionPage
```

为此还维护了 `m_previewFramePending`、两层 `m_previewSessionId`、acknowledge 和多次图像复制。

实施后，每个由 `CaptureWorker` 成功取得的预览帧直接投递到主窗口：

```text
CaptureWorker
→ CameraSession：预处理并更新 m_currentImage
→ InspectionApplicationService::templatePreviewFrameReady(image)
→ MainWindow（Qt::QueuedConnection）
→ 现有主画布显示
```

`CaptureWorker` 每成功取得一帧，界面就接收一次显示通知。

相机线程不能直接操作 Qt 控件，所以仍需保留一次跨线程 Qt 信号。这是线程边界，不是新的预览架构。

## 3. 保持不变的操作

```text
第一次点击“制作模板”
→ 启动预览

预览中再次点击
→ 停止预览
→ 冻结最新图像
→ 开始框选

冻结后点击“重新取景”
→ 按现有确认规则清空框线
→ 重新预览
```

按钮动态文字、启用与禁用、提示动画和“退出模板制作”均不调整。

## 4. 最小代码修改

### 4.1 `app/runtime/camera_session.h/.cpp`

- `startPreview()` 删除 `sessionId` 参数。
- 完成相机状态校验和软件触发模式切换后，在启动 `CaptureWorker` 前于现有互斥锁内清空 `m_currentImage`，防止首帧到达前使用上一次图像。
- `handleFrame()` 的预览分支继续预处理；在现有互斥锁内直接令 `m_currentImage = image`，利用 `cv::Mat` 引用计数保存当前帧，随后对每个成功帧调用一次 `previewFrameReady(image)`。
- `previewFrameReady` 精简为现有回调容器中的 `std::function<void(const cv::Mat &)>`，不携带会话编号。
- `previewFailed` 精简为 `std::function<void(const QString &)>`，只传递错误原因。
- 删除 `acknowledgePreviewFrame()`、`m_previewSessionId` 和 `m_previewFramePending`。
- 删除只有这一处调用的 `replaceCurrentImage()` 声明和定义，避免保留单行包装以及对同一预处理帧再次深拷贝。
- `callbacksSnapshot()` 移入预览分支，正式识别帧不再无意义地复制回调集合。
- `CaptureWorker`、`m_currentImage`、互斥锁和正式识别分支不变。

### 4.2 `app/application/inspection_application_service.h/.cpp`

- `startTemplatePreview()` 删除 `sessionId` 参数。
- 将相机预览帧直接转成 `templatePreviewFrameReady(cv::Mat image)` 信号。
- 不再显式执行 `image.clone()`；Qt 队列按值保存 `cv::Mat` 的引用计数副本，图像在界面处理前保持有效。
- 删除 `acknowledgeTemplatePreviewFrame()`。
- `templatePreviewFailed` 只传递错误原因。
- 保留 `hasCurrentCameraImage()` 和 `currentCameraImageClone()`。
- 删除不再需要的 `replaceCurrentCameraImage()` 转发；`m_currentImage` 由 `CameraSession` 自己更新。
- 正式识别、Fault 和采集停止接口不变。

### 4.3 `app/ui/main_window/template/template_editor_page.h/.cpp`

- 删除预览帧信号连接和 `handlePreviewFrame()`；模板页不再逐帧中转图像。
- 删除 `m_previewSessionId` 和 `m_lastPreviewFrame`。
- `startTemplatePreview()` 使用精简后的应用服务接口，状态和提示逻辑不变。
- `freezeTemplatePreview()` 先用现有 `hasCurrentCameraImage()` 确认已有图像，再停止采集；停止后只调用一次 `currentCameraImageClone()`，用最终当前图像显示并开始框选。
- 不再把 `m_lastPreviewFrame` 写回 `m_currentImage`。
- `handlePreviewFailure()` 删除会话编号参数和对应比较，只保留当前是否为 `Previewing` 的现有状态判断、日志、复位和提示。
- 采集意外停止回调删除会话编号递增和 `m_lastPreviewFrame` 清理，只保留回到 `Idle` 和刷新 UI。
- 保留 `previewFramePresentationRequested`，只用于冻结帧和已加载模板原图的一次性显示，不作为实时预览的逐帧中转。

### 4.4 `app/ui/main_window/main_window.cpp`

- 将 `InspectionApplicationService::templatePreviewFrameReady` 使用 `Qt::QueuedConnection` 直接连接到主窗口。
- 收到预览帧时只在模板页仍为 `CaptureState::Previewing` 的情况下调用现有 `presentTemplatePreviewFrame()`。
- 该状态检查会丢弃停止预览后已经排入 Qt 事件队列的旧帧，避免覆盖冻结画面。
- 冻结帧和已加载模板原图继续通过 `TemplateEditorPage::previewFramePresentationRequested` 一次性显示，现有连接保留。

### 4.5 不修改

- `CaptureWorker` 及预览触发频率；
- `MainWindow::presentTemplatePreviewFrame()` 及其现有 Application/Runtime 渲染边界；
- `InspectionPage::presentPreviewImage()`；
- `InspectionPresentationRenderer::renderRawFrame()`；
- 启动阶段已有的 `qRegisterMetaType<cv::Mat>("cv::Mat")`；
- `OperationUiPolicy`、`.ui` 和 QSS；
- 模板框选、保存和模板 Schema；
- 正式识别、检测算法、结果、Overlay、PLC 和存图链。

`InspectionApplicationService::renderPreviewFrame()` 仍包含空图像及生产运行状态判断，`InspectionRuntime::renderPreviewFrame()` 是当前既定的 Application 到 Runtime 边界；两者不是本次应删除的兼容层或胶水层。不得为缩短调用链而让 `MainWindow` 绕过应用服务直接依赖 Runtime 或 Renderer。

生产代码只修改以下 7 个现有文件，不新增文件：

```text
app/runtime/camera_session.h
app/runtime/camera_session.cpp
app/application/inspection_application_service.h
app/application/inspection_application_service.cpp
app/ui/main_window/template/template_editor_page.h
app/ui/main_window/template/template_editor_page.cpp
app/ui/main_window/main_window.cpp
```

## 5. 删除与保留门禁

已删除：

```text
acknowledgePreviewFrame
acknowledgeTemplatePreviewFrame
TemplateEditorPage::handlePreviewFrame
m_previewFramePending
m_previewSessionId
m_lastPreviewFrame
replaceCurrentCameraImage
replaceCurrentImage
```

以下旧签名也必须完全消失，不保留重载：

```text
CameraSession::startPreview(quint64, ...)
InspectionApplicationService::startTemplatePreview(quint64, ...)
CameraSessionCallbacks::previewFrameReady(quint64, const cv::Mat &)
CameraSessionCallbacks::previewFailed(quint64, const QString &)
InspectionApplicationService::templatePreviewFrameReady(quint64, cv::Mat)
InspectionApplicationService::templatePreviewFailed(quint64, QString)
```

继续保留并精简签名：

```text
CameraSessionCallbacks::previewFrameReady(const cv::Mat &)
InspectionApplicationService::templatePreviewFrameReady(cv::Mat)
```

继续保留现有边界：

```text
CameraSession::m_currentImage
InspectionApplicationService::currentCameraImageClone
MainWindow::presentTemplatePreviewFrame
InspectionApplicationService::renderPreviewFrame
InspectionRuntime::renderPreviewFrame
InspectionPresentationRenderer::renderRawFrame
InspectionPage::presentPreviewImage
```

`TemplateEditorPage::previewFramePresentationRequested` 仅承担冻结帧和已加载模板原图的一次性显示请求，不再出现在实时预览逐帧链路中。

不得保留旧接口重载、空实现、兼容转发或废弃成员。`MainWindow` 中新增的一个排队连接和 `Previewing` 状态判断是唯一必要的 UI 线程接收点：前者负责跨线程，后者防止停止采集前已排队的帧覆盖冻结画面；不再为它增加槽函数、转发信号或中间对象。

## 6. “显示所有帧”的边界

本方案不主动跳帧：`CaptureWorker` 每成功取得一帧，就向界面投递一帧。相机 SDK 内部尚未被 `CaptureWorker` 取出的硬件帧不属于本方案承诺范围。

预览链由 `CaptureWorker` 回调和 Qt 排队信号组成，应用侧不维护帧缓存队列、确认或流控状态。当界面处理速度低于采集速度时，待显示帧会在 Qt 事件队列中等待。

## 7. 验收

1. 第一次点击“制作模板”仍进入实时预览。
2. `CaptureWorker` 每成功取得一帧，主窗口收到一次预览显示通知。
3. 首帧未到时点击冻结，继续提示等待且不退出预览。
4. 冻结后停止采集，画面不被已排队的预览帧覆盖，并立即进入当前模式的框选流程。
5. 保存和二维码框选校验读取的图像与冻结图像一致。
6. “重新取景”和“退出模板制作”行为不变。
7. 预览失败或意外停止后页面恢复 `Idle`。
8. 正式识别的软触发、硬触发、检测结果和画布显示不受影响。

第 5 节删除项在 `app/` 范围内零引用；失效的声明、定义和注释已清理；7 个生产文件在 qmake 工程中各有且仅有一个条目；`git diff --check` 通过。Release 构建和真实相机验收按仓库限制由用户在 Qt Creator 中完成。
