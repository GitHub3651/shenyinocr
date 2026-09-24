# OCRGangYin 应用、运行时与界面边界精简方案

## 1. 文档状态与最终决策

- 状态：架构设计和代码实施规则已由用户确认；四个代码阶段已完成并分别提交，尚待用户在 Qt Creator 统一构建、运行和现场验证。
- 更新日期：2026-09-19。
- 本文只定义目标架构、明确决策、实施顺序和验收门禁，不代表生产代码已经修改、构建或现场验证。
- 适用范围：`app/application`、`app/contracts`、`app/runtime`、`app/detection`、`app/ui` 中与检测运行、结果呈现和模板预览有关的边界。
- 保护范围：不改变已验证的模板终局、五种检测模式行为、PLC 时序、存图合同、设备安全和故障自动停止合同。

本版本不再保留 A/B 可选项，固定采用以下设计。除本文列出的代码实施外，不得额外引入替代路径或过渡架构。

1. `InspectionPresentation` 放在 `app/contracts/inspection_presentation.h`，作为跨模块纯数据合同。
2. `ProductKey` 不放入 `InspectionPresentation`；它只留在 Runtime、ResultService 和检测完成数据内部。
3. `InspectionRuntime` 继承 `QObject`，发布纯数据 Qt signal。`MainWindow` 负责连接信号并调用 Page；Runtime 不保存 MainWindow、Page、控件 Lambda 或 `PresentationSink`。
4. 产品结果固定为：`ResultService → Runtime → MainWindow → InspectionPage`。Application 不发布、不转发产品结果、告警或 UI 回调。
5. 页面内部的 `InspectionPageViewBindings`、`MachineSettingsPageViewBindings`、`TemplateEditorViewBindings` 保留。
6. 删除所有跨层结果控件回调：`InspectionViewBindingsDto`、`InspectionPresentationViewBindings`、`InspectionPage::resultViewBindings()`、`bindView()`、`ResultServiceCallbacks`、`InspectionUiCallbacks`。
7. `InspectionPage` 只保留一个生产结果入口：`present(const InspectionPresentation &)`；由它一次性更新同一产品的图片、文字和统计。
8. 保留容量为一的数据 mailbox；载荷为 `InspectionPresentation`；新结果替换尚未显示的旧结果；生产线程完全不等待 UI。
9. ResultService 先完成去重、统计、存图任务提交和 PLC 请求，再投递 UI 数据；UI 速度不能决定生产事务是否完成。
10. 正常 stop 禁止新认领，等待已认领产品完成并显示最后一份结果后取消 mailbox；Fault 立即取消未显示结果；新 Run 不接收旧 Run 数据。
11. `InspectionPresentationRenderer` 保留为纯图像/数据转换器，删除其 ViewBindings、控件回调和所有 `showXxx()` 方法。
12. `InspectionRuntime::resultService()` 在本轮重构的末尾删除；先迁移真实调用，不长期保留，也不为隐藏它无原则增加 Runtime 转发接口。
13. 删除 Application 的 `clearResultView()`、`clearTransientView()`；Runtime 在生命周期内清理后台呈现状态，Page 只清理自己明确的结果区域或临时提示。
14. 存图失败只记录现有日志，不进入运行状态标签，也不发布 UI signal；ROI 警告使用简单数据 signal，Runtime Fault 复用既有故障数据。不得增加 EventBus、告警管理器或复杂告警 Snapshot。
15. 模板预览继续走独立预览链，不伪装成生产结果。
16. `CameraSession` 已位于 `app/runtime`；本次不移动文件、不重构内部实现、不改变正式采集流程。
17. 本次不引入 `DetectionRunPolicy`，也不预先承诺未来引入；现有策略字段继续保留在 `DetectionResult`。

## 2. 设计目标与非目标

### 2.1 目标

本方案不追求形式上的严格单向依赖，而追求每个问题都有唯一主要负责人：

| 问题 | 主要负责人 |
|---|---|
| 控件在哪里、如何显示 | UI Page |
| 用户点击开始/停止要组织哪些步骤 | Application |
| 生产运行是否允许继续、何时停止、如何恢复 | Runtime |
| 一个产品是否完成、如何统计、何时发 PLC/存图 | ResultService |
| 一帧如何定位和判定 | Detection Pipeline |
| 相机 SDK 如何采集原始帧 | CameraSession |
| PLC 如何连接、写入和复位 | InspectionPlcController |

调用链允许短路径。例如结果数据可以从 Runtime 直接被 MainWindow 连接；Application 只在它确实承担用户用例、配置组合或错误转换时参与，不做无意义的信号转发。

### 2.2 非目标

本方案不新增 EventBus、MessageBus、通用 Command 分发框架、Manager、Facade、Coordinator、PageManager、每按钮类、每模式 Presenter、第二套状态机或第二套 Presentation；不修改算法、模型、模板 Schema、QSS、翻译、DLL、图片和部署资源；不在未测量前假设固定的 UI 更新耗时；不把 UI 显示丢帧等同于生产结果丢失。

## 3. 当前真实实现基线

本节描述源码现状，不代表目标已完成。

### 3.1 当前结果回调链

`InspectionPage::resultViewBindings()` 创建捕获页面 `this` 的 Lambda，例如：

```cpp
bindings.showImage = [this](const QImage &image) {
    m_view.imageLabel_inspection->setAutoFitPixmap(
        QPixmap::fromImage(image));
};
```

当前链路是：

```text
InspectionPage
 → InspectionViewBindingsDto
 → InspectionApplicationService::bindView()
 → InspectionPresentationViewBindings
 → ResultService / InspectionPresentationRenderer
 → UiCompletionMailbox::Work
 → UI 线程执行 Lambda
 → QLabel/ImageLabel
```

相关位置包括：`app/application/inspection_ui_contract.h`、`inspection_application_service.*`、`app/runtime/inspection_presentation_renderer.*`、`result_service.*`、`result_presentation_mailbox.*`、`app/ui/pages/inspection_page.*` 和 `app/ui/main_window.cpp`。

### 3.2 当前 Runtime 与 ResultService 关系

`InspectionRuntime` 持有 `ResultService`，同时公开 `resultService()`。Application 多处调用 `m_runtime->resultService().xxx()`，因此 Application 依赖了 Runtime 的内部对象，而非业务能力。

### 3.3 当前 mailbox 行为

当前 mailbox 载荷是 `std::function<void()>`；`submit()` 在邮箱有待处理或正在消费时等待；`processOne()` 取出并执行函数；`cancel()` 清空函数。当前 `ResultService::process()` 在 UI 工作提交后才请求 PLC，因此 UI 阻塞可能延迟 PLC 请求，必须在实施时调整。

### 3.4 当前 DetectionResult 混合字段

`DetectionResult` 同时包含算法判定、识别文字、Overlay、耗时，以及显示、存图、ROI 警告策略。第一阶段不拆字段，先停止继续增加 UI 回调字段；后续单独收口运行策略。

### 3.5 当前模板预览链

模板预览通过 `templatePreviewFrameReady` 独立到达 `TemplateEditorPage`。它没有 ProductKey、统计、PLC 或生产存图，不进入生产结果链。

## 4. 终局架构总览

```text
MainWindow（UI 装配与跨页面连接）
 ├─ InspectionPage / TemplateEditorPage / MachineSettingsPage
 ├─ 连接 InspectionRuntime 的公开 Qt signals
 └─ 调用 InspectionApplicationService 的用户用例

InspectionApplicationService（启动、停止、相机、PLC、模板预览编排）
 ├─ 使用 SettingsApplicationService / TemplateStore
 ├─ 协调 InspectionRuntime
 └─ 协调现有 CameraSession；本次不改变其控制边界

InspectionRuntime（QObject，生产 Run 生命周期）
 ├─ DetectionWorker / DetectionRegistry / Pipelines
 ├─ InspectionPlcController
 ├─ ResultService
 └─ PresentationMailbox

CameraSession（现有正式采集与模板预览流程）
 └─ 将正式采集帧交给 InspectionRuntime
```

生产结果数据流：

```text
CameraSession
 → InspectionRuntime
 → DetectionWorker
 → Detection Pipeline
 → ResultService
 → InspectionRuntime（接收完整结果）
 → PresentationMailbox（只保存最新待显示快照）
 → InspectionRuntime::presentationReady
 → MainWindow（Qt 连接）
 → InspectionPage::present()
 → QWidget
```

用户命令流：

```text
UI
 → Application：开始/停止/相机/PLC/模板用例
 → Runtime：正式运行命令和运行配置
 → Camera / Detection / PLC
```

结果数据只走这条 Runtime 直连链。Application 不是结果中转站；禁止 UI 访问 Runtime 内部组件、ResultService 或后台保存控件函数。

## 5. 模块职责与所有权

### 5.1 UI

UI 拥有 QWidget、页面内部控件绑定、用户输入、按钮状态、UI 文案、QSS 和 `InspectionPresentation` 到控件的最终转换。UI 不拥有 Runtime 状态机、DetectionWorker、FrameQueue、ProductKey、PLC、存图服务或模板事务。

### 5.2 Application

Application 拥有启动前设置/模板/设备检查、多服务组合的用户用例、DTO 到运行配置的转换和错误转换；现有运行快照通知可继续由 Application 维护。产品结果、ROI/存图告警和故障通知由 Runtime 直接发布给 UI；Application 不发布或转发这些数据，也不保存 UI 回调、QWidget、QLabel、ImageLabel、控件 Lambda，不持有或访问 ResultService。

### 5.3 Runtime

Runtime 是一次生产 Run 的唯一所有者，拥有 `Idle/Starting/Running/Stopping` 公开状态；内部保留 Fault 首因并在统一停止链中收口。它还拥有 RunId/ProductKey、Detection/队列/ResultService 的运行顺序、设备安全规则、旧 Run 隔离和 PresentationMailbox 生命周期。它继承 `QObject`，只发布纯数据 signal；不包含 UI 头文件，不认识 Page、QLabel、QMessageBox 或 QSS。本次不改变 Application 协调 CameraSession 的现有启动、停止和相机预览恢复流程。

### 5.4 ResultService

ResultService 是 Runtime 内部的单产品事务组件，负责结果校验、ProductKey 去重、统计、延迟 NG、异步存图、PLC 输出/复位、生成 `InspectionPresentation`，并向 Runtime 发出必要的简单告警数据。它不修改 UI、不保存 UI 回调。

### 5.5 Detection、Camera、PLC

Detection 只处理图像、模板和算法；CameraSession 只处理相机 SDK 和原始帧；PLC Controller 只处理连接、写入、脉冲和复位。CameraSession 的现有采集控制流程保持不变；Runtime 负责自身生产状态、检测、ResultService 和 PLC 结果事务。

## 6. 依赖规则

允许：

```text
UI → Application 用例接口
UI → Runtime 公开快照/结果数据接口（由 MainWindow 装配）
Application → Settings / TemplateStore / Runtime
Runtime → Camera / Detection / PLC / ResultService
ResultService → InspectionPresentation（纯数据）
```

禁止：

- Runtime/ResultService include 页面或控件头文件；
- Application 保存页面控件 Lambda；
- ResultService 保存 `InspectionViewBindingsDto` 或 `InspectionPresentationViewBindings`；
- UI 直接创建/停止 DetectionWorker、操作 FrameQueue 或写 PLC；
- Detection Pipeline 直接改控件、发 PLC 或写结果文件；
- 模板预览伪造 ProductKey、生产统计或 PLC 结果；
- 为一次转发新增 Facade、Manager 或 Coordinator。

## 7. ViewBindings 的最终处理

### 7.1 保留页面内部绑定

保留：

```text
InspectionPageViewBindings
MachineSettingsPageViewBindings
TemplateEditorViewBindings
```

它们只保存控件地址，调用范围是 `MainWindow → Page`，属于 UI 内部装配，不是本次跨层问题。

### 7.2 删除跨层结果绑定

删除：

```text
InspectionViewBindingsDto
InspectionPresentationViewBindings
InspectionPage::resultViewBindings()
InspectionApplicationService::bindView()
InspectionApplicationService::clearUiBindings()
ResultService::bindView()
InspectionPresentationRenderer::bindView()
InspectionPresentationRenderer::hasViewBindings()
```

删除原因是它们保存“稍后如何修改控件的函数”，把页面生命周期和 UI 线程语义带进了后台。

## 8. `InspectionPresentation` 数据合同

### 8.1 固定文件位置

固定使用：

```text
app/contracts/inspection_presentation.h
```

不保留 `app/runtime/inspection_presentation.h` 的第二份同名类型；迁移后所有跨模块使用者 include contracts 文件。本轮不为 include 方便保留第二份合同。

### 8.2 固定结构

```cpp
enum class DetectionVerdictViewStyle
{
    Correct,
    Error
};

struct DetectionResultStatistics
{
    int totalCount = 0;
    int ngCount = 0;

    double passRatePercent() const;
};

struct InspectionPresentation
{
    QImage image;
    DetectionVerdictViewStyle verdictStyle =
        DetectionVerdictViewStyle::Error;
    QString recognitionText;
    bool updatesTemplateName = false;
    QString templateName;
    DetectionResultStatistics statistics;
    QString elapsedText;
};
```

### 8.3 明确不放入的字段

不放 ProductKey、QWidget 指针、`std::function`、PLC 动作、存图路径和策略、OpenCV 临时矩阵、模型句柄或线程对象。ProductKey 仍由 Runtime/ResultService 在投递前校验，UI 不显示也不重做去重。

### 8.4 `InspectionPage::present()`

这是生产结果唯一显示入口，固定顺序更新图片、判定图像、识别文字、模板名、统计/合格率和耗时。`verdictStyle` 是判定呈现的唯一数据，页面用它选择 OK/NG SVG，Presentation 不再传递判定文字。它不做去重、PLC、存图、Runtime 状态裁决或模板文件操作；控件缺失只做一次 UI 级保护。统计按钮清零使用 UI 内部函数 `InspectionPage::setStatistics(const DetectionResultStatistics &statistics)`，不经过 Renderer 或 Application 的控件转发。

## 9. Runtime 数据 signal 与 MainWindow 装配

`InspectionRuntime` 改为继承 `QObject`，增加 `Q_OBJECT`，但不使用 Qt parent 管理它。它继续由启动层的 `std::shared_ptr` 唯一拥有，构造函数不增加 `parent` 参数，使用 `QObject(nullptr)` 初始化。`ResultService` 继续由 Runtime 的 `std::unique_ptr` 唯一拥有，也不设置 Qt parent，避免智能指针和 QObject parent 重复销毁。

Runtime 对外只发布以下纯数据 signal：

```cpp
signals:
    void presentationReady(InspectionPresentation presentation);
    void roiWarningChanged(bool active);
    void faultSnapshotChanged(InspectionFaultSnapshot snapshot);
```

`presentationReady` 表示“一个产品的完整结果已可显示”。它只传递数据，不传控件指针、Lambda、Page、MainWindow 或 ResultService。

`MainWindow` 在启动层取得 Runtime 的非拥有指针并负责装配连接；Runtime 不连接、持有或调用 MainWindow。ResultService 通过 Runtime 的内部方法把完整 Presentation 和必要告警数据交给 Runtime，Runtime 再对外发布 signal。结果连接使用默认 `Qt::AutoConnection`：

```cpp
connect(
    m_runtime,
    &InspectionRuntime::presentationReady,
    this,
    [this](const InspectionPresentation &presentation) {
        m_inspectionPage->present(presentation);
    });
```

MainWindow 接收 `roiWarningChanged(active)` 后调用 `InspectionPage` 的显示方法。存图失败只保留现有日志，不进入运行状态标签或新增其他 UI 通知。MainWindow 直接连接 `faultSnapshotChanged(InspectionFaultSnapshot)`，立即调用现有停止链；停止完成后显示一次固定警告，不创建第二份故障 DTO。

不使用 `PresentationSink`、`setPresentationSink()`、`clearPresentationSink()` 或任何 Runtime 保存的 `std::function` 数据出口。唯一允许使用连接 Lambda 的位置是 MainWindow 的 UI 装配代码；该 Lambda 由 Qt 连接和 MainWindow 生命周期管理，不被后台层保存。

mailbox 的 UI 唤醒固定使用 Runtime 的私有槽 `drainPresentationMailbox()`。ResultService 调用 Runtime 内部的 `publishPresentation()` 后，Runtime 只在 `wakePosted` 从 `false` 变为 `true` 时使用字符串槽调用排入一个 `Qt::QueuedConnection`；不保存 Lambda。`drainPresentationMailbox()` 在 UI 线程取出一个快照并发出 `presentationReady(snapshot)`，若取出后仍有待显示数据，再排下一次唤醒。`InspectionPresentation` 和 `InspectionFaultSnapshot` 需声明/注册为 Qt 元类型（`Q_DECLARE_METATYPE` 与启动时 `qRegisterMetaType`）。

Runtime 的内部数据出口固定为：

```cpp
bool publishPresentation(const InspectionPresentation &presentation);
void publishRoiWarning(bool active);
```

这些函数只由 `ResultService` 调用，不加入 Runtime 的公开业务 API；ResultService 使用 `friend class ResultService` 访问它们。

Runtime 的构造和 MainWindow 装配固定为：

```cpp
// inspection_runtime.h
explicit InspectionRuntime(
    const RunIdFactory &runIdFactory,
    const std::shared_ptr<InspectionPlcController> &plcController,
    const std::shared_ptr<DetectionRegistry> &detectionRegistry);
~InspectionRuntime() override;

// main_window.h
MainWindow(
    const std::shared_ptr<InspectionApplicationService> &inspectionService,
    InspectionRuntime *runtime,
    const std::shared_ptr<SettingsApplicationService> &settingsService,
    const std::shared_ptr<TemplateApplicationService> &templateService,
    QWidget *parent = nullptr);
```

MainWindow 只保存 `InspectionRuntime *m_runtime`，不负责释放它。启动层继续保存 `std::shared_ptr<InspectionRuntime> runtime`，并以 `runtime.get()` 传入 MainWindow。`InspectionFaultSnapshot` 继续定义在 `app/runtime/inspection_runtime.h`，本轮不新建告警合同文件；UI 通过 Runtime 的已有故障快照接收数据。Runtime 的公开 signal 连接在 Page 创建完成后进行；ResultService 不连接 UI。

## 10. PresentationMailbox 最终设计

### 10.1 定位

mailbox 是线程交接和反压工具，不是业务层。保留它是为了防止大图在 Qt 事件队列中无限积压，而不是假设每张图固定耗时 100 ms。

### 10.2 固定语义

- 容量：1 个待显示 `InspectionPresentation`；
- `submit()` 不等待 UI，不阻塞统计、PLC 或存图；
- 若已有待显示数据，用新数据替换旧数据；
- UI 已经取出的当前快照不再替换；它不占用“待显示”槽位，新的结果可以写入唯一的待显示槽位；
- 正常停止禁止新认领，但允许已认领产品提交最后一份结果，Worker 退出后由 UI 线程取出该结果再取消 mailbox；Fault 立即禁止提交并清空待显示数据；
- 新 Run 重新打开 mailbox，旧 Run 不再提交。

例：UI 正显示产品 10，邮箱待显示产品 11，产品 12 到达时丢弃尚未消费的 11、保留 12。产品 11 的生产事务仍然已经完成。

固定接口：

```cpp
bool submit(
    const InspectionPresentation &presentation);
bool processOne(InspectionPresentation *presentation);
void cancel();
bool reopen();
```

`processOne()` 只在 UI 线程调用：它在短暂持锁期间把待显示快照移动到局部变量并清空待显示槽位，随后释放锁。mailbox 不再使用 `condition_variable`、`std::function<void()>` 或 `m_processing` 等等待执行状态；只保留互斥锁、取消状态和一个待显示快照。Runtime 的 `wakePosted` 负责 queued 唤醒去重。当前快照显示期间到来的新结果可以占用唯一待显示槽位；当前显示结束后，若邮箱又有新数据，再安排下一次唤醒。

## 11. ResultService 事务顺序

固定顺序：

1. 校验 `DetectionCompletion`；
2. 校验 ProductKey 属于当前 Run；
3. Runtime 在接受算法结果时原子认领并去重；
4. 更新统计和延迟 NG；
5. 提交异步存图；
6. 提交或执行到现有业务合同规定阶段的 PLC OK/NG 请求；
7. 生成 `InspectionPresentation`；
8. ResultService 将 Presentation 交给 Runtime，Runtime 提交 mailbox；
9. Runtime 安排一次 UI 线程消费 mailbox；消费到快照后发出 `presentationReady` 数据 signal。

mailbox 被替换或 UI 尚未消费时，不回滚统计、PLC、存图和去重。UI 展示失败只记录诊断。

保留 ProductKey 去重、统计、延迟 NG、PLC、ImageSaveService、图片/Overlay 组合和结构化告警；删除所有 ViewBindings 和控件回调调用。

## 12. Renderer 最终职责

`InspectionPresentationRenderer` 保留为小型纯转换组件，负责 `cv::Mat → QImage`、Overlay 绘制，以及把现有 `DetectionResult` 和统计数据组装到 `InspectionPresentation`。不新增运行策略对象。删除 `m_viewBindings`、`bindView()`、`hasViewBindings()` 和所有 `showXxx()` 调用。

只服务模板预览的 `presentFrame()` 应移到预览链；生产实时帧若仍需要显示，必须明确命名为帧预览，不与完整产品结果混用。

## 13. Application API 收口

保留 `start`、`stop`、相机/PLC 用例、模板预览、统计重置、延迟 NG 清理和快照查询。模板预览仍由 Application 作为用户用例入口，但其内部只能调用 Runtime 的 `presentPreviewFrame()`，不得访问 ResultService。删除 `bindView`、`setUiCallbacks` 和 `clearUiBindings`。

删除 `clearResultView()` 和 `clearTransientView()`，不以新名字继续保留 Application 的跨层显示 API。

- Runtime 在正常 stop 时显示最后一份已认领结果后取消 mailbox，在 Fault 和新 Run 生命周期中取消 mailbox、隔离旧 Run 并清理内部呈现状态；
- `InspectionPage` 只清理自己明确拥有的结果区域或临时提示控件，不清空整个页面；
- 参数、按钮、统计数据、其他页面内容和已经显示的最后一张结果保留；
- Application 不转发“清空界面”操作。

Application 不再调用 `m_runtime->resultService().xxx()`。真实调用固定迁移如下：

```cpp
DetectionResultStatistics statistics() const;
bool requiresPlcForRun() const;
void presentPreviewFrame(const cv::Mat &image,
                         bool tissueMode,
                         bool productionRunning);
void resetStatistics();
void resetNgCount();
void clearPendingDelayedNgRequests();
```

其中 `requiresPlcForRun()` 是开始/相机用例需要的运行查询，`presentPreviewFrame()` 是独立模板预览用例；二者不是 ResultService 的逐方法转发。`setCallbacks()`、`bindView()`、`clearUiBindings()`、`clearResultView()`、`clearTransientView()`、`presentTotalAndNgCounts()` 和 `presentNgCount()` 直接删除，不新增对应的 Runtime 包装。统计清零后由 MainWindow 读取 Runtime 的 `statistics()`，调用 `InspectionPage::setStatistics()`。

## 14. 模板预览与生产分离

模板预览固定使用：

```text
CameraSession
 → InspectionApplicationService::templatePreviewFrameReady
 → TemplateEditorPage
```

预览不进入 `InspectionPresentation`、生产 mailbox、统计、PLC、存图或 ProductKey。生产结果固定使用 `ResultService → Runtime → PresentationMailbox → Runtime::presentationReady → MainWindow → InspectionPage::present()`。

## 15. CameraSession 与 DetectionRunPolicy 的范围

### 15.1 CameraSession

`CameraSession` 已经在 `app/runtime`。本次不移动文件、不重构内部实现，也不改变由 Application 协调相机准备、启动、停止、曝光恢复和故障自动停止的现有正式采集流程。模板预览始终独立。CameraSession 不拥有统计、PLC 事务或页面。

### 15.2 DetectionRunPolicy

本次不引入 `DetectionRunPolicy`，也不预先承诺后续一定引入。`saveRawOnly`、`elapsedDecimals`、`clearImageLabelRects`、`showRoiWarningOnCancelled` 等现有字段继续留在 `DetectionResult`。只有这些运行策略明显继续膨胀，或算法结果需要跨运行场景复用时，才另行立项评估拆分。

## 16. 新增检测模式的最小变更路径

普通模式只修改：`DetectionMode/Descriptor`、`DetectionRegistry`、新 Pipeline、模板字段、必要 AppSettings 和算法样本测试。

不修改：`InspectionPage::present()`、Runtime 状态机、ResultService 通用去重/统计/存图/PLC 事务和 PresentationMailbox。

只有真实独特需求才扩大范围：独特显示扩展稳定 Presentation 字段；独特触发扩展 Runtime；独特 PLC 扩展 ResultService/PLC 合同；独特模板编辑只改 TemplateEditorPage。不得新增模式 Presenter、模式状态机或跨层控件回调。

## 17. 防止过度设计和重复防御

| 规则 | 主要负责人 |
|---|---|
| 输入格式和按钮确认 | UI/Application |
| 能否开始/停止 | Runtime |
| ProductKey 归属和重复 | Runtime/ResultService |
| FrameQueue/mailbox 容量 | Runtime/Mailbox |
| PLC 连接、写入、复位 | PLC/ResultService |
| 模板临时保存和回滚 | TemplateStore |
| 控件可用性和显示 | UI |

不要在三层重复同一状态判断。只保留会影响生产安全、线程安全、生命周期或事务一致性的检查；普通显示保护集中在 `InspectionPage::present()`。新类型必须拥有明确状态或不变量；只做转发、改名、包装的类不新增。

## 18. 分阶段实施

### 阶段 0：基线

记录分支、HEAD、工作区、旧符号引用和五种模式/软硬触发/PLC/存图/ROI/Fault/预览行为。不改生产代码。

### 阶段 1：结果合同和页面入口

新增 `app/contracts/inspection_presentation.h`，更新 include，增加 `InspectionPage::present()`，保持所有显示内容一致。不改 Detection、PLC、Camera 正式采集、模板 Schema、存图路径。

### 阶段 2：删除跨层控件回调

删除 `InspectionViewBindingsDto`、`InspectionPresentationViewBindings`、`ResultServiceCallbacks`、`InspectionUiCallbacks` 和 Application/Runtime/ResultService 的 `bindView`；将 Runtime 改为 `QObject`，由 MainWindow 连接 Runtime 的 `presentationReady`、存图失败、ROI 状态和故障 signal；缺少标注图复用存图失败通知。页面内部 ViewBindings 保留。ResultService 通过 Runtime 内部方法报告数据，不保存任何 UI 函数。

### 阶段 3：mailbox 数据化和非阻塞化

将 `Work` 改为 `InspectionPresentation`；待显示数据只保留最新一份；UI 唤醒只排一个 queued 事件；正常停止显示最后一份已认领结果后清空 mailbox，Fault 立即清空；统计、PLC、存图先于 mailbox 提交。

### 阶段 4：隐藏 ResultService

删除 Runtime 的 `resultService()`；按第 13 节固定映射迁移 Application 的真实调用；ResultService 只由 Runtime 创建、配置和销毁。除 `requiresPlcForRun()`、`presentPreviewFrame()` 等真实业务能力外，不新增逐方法转发。

## 19. 逐文件修改清单

### `app/contracts`

- 新增 `inspection_presentation.h`，只放纯数据合同。

### `app/application`

- `inspection_application_service.*`：保留用户用例，删除 `bindView`、`clearUiBindings`、`clearResultView`、`clearTransientView` 和结果/告警转发。
- `inspection_ui_contract.h`：本轮删除整个文件；跨层结果绑定、UI 回调和重复的 `ApplicationFaultSnapshot` 均不再保留。`InspectionFaultSnapshot` 继续使用 `app/runtime/inspection_runtime.h` 中的唯一类型。页面内部 `InspectionPageViewBindings` 继续留在 `inspection_page.h`。
- `runtime_snapshot.h`：保留运行状态和统计快照。

### `app/runtime`

- `inspection_runtime.*`：改为 `QObject` 并加入 `Q_OBJECT`；保持启动层 `std::shared_ptr` 为唯一所有权，QObject parent 固定为空；保留 `InspectionFaultSnapshot` 作为 Runtime 的唯一故障数据类型；增加 `publishPresentation()` 等仅供 ResultService 使用的内部方法、`drainPresentationMailbox()` 私有槽和 `wakePosted` 唤醒标志；发布结果/告警数据 signal；删除公开 `resultService()`，只保留上面列出的真实业务接口。
- `result_service.*`：保留单产品事务，删除 ViewBindings 和 `ResultServiceCallbacks`，生成 Presentation 并向 Runtime 报告必要告警数据。
- `inspection_presentation_renderer.*`：删除回调成员，保留纯图像/数据转换。
- `result_presentation_mailbox.*`：保留现有文件和 `UiCompletionMailbox` 类名；删除 `std::function<void()>`、条件变量等待和 `m_processing`，载荷改为 Presentation，容量一、非阻塞、最新待显示结果替换。
- `camera_session.*`：本次不修改正式采集边界或内部实现。

### `app/ui`

- `inspection_page.*`：删除 `resultViewBindings()`，增加 `present()`。
- `main_window.*`：接收启动层传入的非拥有 `InspectionRuntime *`，直接连接 Runtime 的 `presentationReady`、存图失败、ROI 状态和故障 signal；保留页面内部 ViewBindings。
- `template_editor_page.*`：继续消费独立模板预览信号。

### 工程文件

- `AutoOCRproject.pro`（工程目录为 `app`）：在 `HEADERS` 加入 `contracts/inspection_presentation.h`；从 `HEADERS` 移除 `runtime/inspection_presentation.h` 和 `application/inspection_ui_contract.h`；不重命名 mailbox 文件；不修改模型、DLL、图片、翻译、QSS 和部署资源。

## 20. 风险处理

| 风险 | 固定处理 |
|---|---|
| UI 跨线程 | `present()` 只在 UI 线程调用，mailbox 用 queued 唤醒 |
| UI 跟不上检测 | mailbox 不阻塞生产事务，只替换待显示结果 |
| 统计/PLC 与 UI 不一致 | 统计、PLC、存图在 mailbox 提交前完成 |
| 旧 Run 结果 | 正常停止只允许已认领产品完成并显示最后结果，随后清空 mailbox；Fault 立即清空，新 Run 重新打开 |
| 重复结果 | Runtime/ResultService 以 ProductKey 认领一次 |
| PLC 故障 | 写入/复位失败进入 Runtime Fault |
| 页面销毁竞态 | 先停 Runtime、取消 mailbox、断开 Runtime → UI 的 Qt 连接，再销毁 Page |
| 图片复制开销 | 只保存当前结果，实际性能用测量数据判断 |

## 21. 验收门禁

### 21.1 静态门禁

- `InspectionViewBindingsDto`、`InspectionPresentationViewBindings`、`resultViewBindings()`、Application/ResultService `bindView()` 零引用；
- Application/Runtime/ResultService 不包含 `QLabel`、`QWidget`、`ImageLabel`、`Ui::MainWindow`；
- mailbox 不再保存 `std::function<void()>` 控件工作；
- Application 不存在 `m_runtime->resultService()`；
- `InspectionPage::present()` 是唯一生产结果入口；
- qmake 清单完整；
- `git diff --check` 通过。

### 21.2 用户构建与回归

由用户在 Qt Creator 执行 Run qmake、Rebuild，并验证：五种模式结果显示；软/硬触发、QueueFull、停止中帧；PLC OK/NG、延迟 NG、复位、断连；原图/标注图和异步存图失败；ROI 警告；Fault 自动停止、一次警告和新 Run；模板预览隔离；UI 变慢时检测、统计、PLC、存图继续而界面只显示最新待显示结果。

未执行的构建、真实相机、PLC、机械动作和现场异常恢复必须如实标记为待验证。

## 22. 新增模式验收

新增普通模式后，应证明没有无必要修改：

```text
InspectionPage::present
InspectionRuntime 状态机
ResultService 通用事务
PresentationMailbox
```

新增模式必须有自己的 Pipeline、模板字段和样本验证。真实独特显示、触发或 PLC 语义才允许扩大对应模块，并说明原因。

## 23. 完成标准

实施完成必须同时满足：

- UI 拥有控件和显示逻辑；
- Application 拥有用户用例，不保存控件回调；
- Runtime 拥有生产生命周期、设备编排、队列、Fault 和 ProductKey；
- ResultService 拥有单产品统计、存图和 PLC 事务；
- Detection 拥有算法；
- 结果以一个 `InspectionPresentation` 数据对象到达页面；
- mailbox 不保存控件 Lambda，不阻塞生产事务；
- 模板预览和生产结果独立；
- 普通新增模式不需要修改通用 UI 和生产事务；
- 用户构建和人工回归完成后，才可把状态从“草案”改为“已实施”。

## 24. 最终原则

```text
UI：拥有控件和显示
Application：拥有用户用例
Runtime：拥有生产生命周期和安全状态
ResultService：拥有单产品结果事务
Detection：拥有算法
Camera/PLC：拥有设备适配
```

精简对象是跨层控件回调和无意义转发，不是工业生产所需的状态机、队列、去重、PLC、存图和 Fault 防线。只要坚持“传数据、不传控件函数；保留必要状态、不新增空壳管理层”，就能同时获得清晰流程、局部扩展和较低维护成本。

## 25. 术语和阅读方式

### 25.1 命令、快照、结果和通知不是一回事

为了避免把所有跨层对象都叫“回调”或“消息”，本方案固定使用四种词：

| 名称 | 方向 | 含义 | 例子 |
|---|---|---|---|
| Command | UI → Application/Runtime | 操作者要求系统执行一次动作 | 启动、停止请求 |
| Snapshot | Application/Runtime → UI | 某一时刻的状态复制 | `RuntimeSnapshot`、`InspectionFaultSnapshot` |
| Presentation | ResultService → UI | 一个产品最终要显示的完整结果 | `InspectionPresentation` |
| Notification | Runtime → UI | 一次仍需界面呈现的告警事件 | `roiWarningChanged(active)` |

Command 不保存控件，Snapshot/Presentation/Notification 不保存函数。只有 UI 层的 Qt `connect()` 可以使用 Lambda 做装配；后台模块不能保存指向 Page 的 Lambda。

### 25.2 “信号”是什么

Qt signal 是对象发布数据事件的机制，不是业务对象，也不是新的管理层。例如：

```cpp
void presentationReady(InspectionPresentation presentation);
```

它只表示“有数据可消费”。真正的业务处理仍在 Runtime/ResultService，真正的控件操作仍在 `InspectionPage::present()`。不要把 signal 扩展成一个包含几十种事件的通用总线。

### 25.3 为什么本方案不用 sink

`sink` 是后台保存的函数对象，例如：

```cpp
std::function<void(const InspectionPresentation &)>
```

它可以传纯数据，但仍要求 Runtime 保存外部提供的函数，并在停止和销毁时手工清空。本方案明确不采用它：`InspectionRuntime` 继承 `QObject`，通过 Qt signal 发布数据；MainWindow 在 UI 装配处建立连接。这样 Runtime 不持有 Application、Page 或任何外部函数对象。

## 26. 目标对象的生命周期和所有权

### 26.1 创建顺序

应用启动时固定按以下顺序装配：

```text
1. 创建 SettingsApplicationService / TemplateStore
2. 创建 InspectionPlcController / DetectionRegistry
3. 创建 InspectionRuntime（内部创建 ResultService 和 mailbox）
4. 创建 CameraSession，并按当前流程交给 Application 协调使用
5. 创建 InspectionApplicationService
6. 创建 MainWindow，并传入 `runtime.get()` 作为非拥有指针
7. 创建三个 Page，Page 只接收自己的 ViewBindings
8. MainWindow 直接连接 Runtime 的结果、告警和故障 signal
9. 发布初始 RuntimeSnapshot，刷新按钮状态
```

Runtime 的 shared_ptr 在启动层先于 MainWindow 创建，MainWindow 在 Runtime 之前销毁，因此 MainWindow 的非拥有指针始终有效。不要把 Runtime 设置为 MainWindow 或 Application 的 Qt parent；不要在 Page 构造函数中创建 Runtime、CameraSession 或 DetectionWorker；不要在 Runtime 中反向查找 MainWindow。

### 26.2 正常关闭顺序

窗口退出、应用 shutdown 和页面销毁必须遵守同一顺序：

```text
MainWindow 开始关闭
 → Application::shutdown()
 → Runtime 禁止新命令/新帧
 → Application 按既有流程停止 CameraSession
 → Runtime 停止 DetectionWorker 并等待线程退出
 → ResultService 完成允许完成的 PLC 复位/存图收尾
 → Runtime 显示最后一份已认领结果并 cancel mailbox
 → 断开 Runtime signal → UI 的 Qt 连接
 → 销毁 Page
 → 释放 MainWindow 的 Ui
```

如果某一步失败，不能跳过后续的生命周期清理；但也不能在 UI 析构阶段重新启动设备或弹出新的业务对话框。关闭期间只记录诊断并保持 Stopping 语义。

### 26.3 Page 生命周期规则

`InspectionPageViewBindings` 中的控件由 `MainWindow` 的 `ui` 对象拥有，Page 只借用地址，不负责 delete。Page 销毁前必须断开指向它的 Qt 连接，Runtime、ResultService 和 Application 不得继续持有 Page 的引用或 Lambda。

## 27. 四条具体业务流程

### 27.1 开始检测

```text
用户点击“开始检测”
 → MainWindow/InspectionPage 收集当前 UI 输入
 → Application::start(StartInspectionCommand)
 → Application 读取 AppSettings 和 TemplateStore
 → Application 检查模板、模式、相机和 PLC 前置条件
 → Runtime::beginStart(runContext)
 → Runtime 状态 Idle → Starting
 → Runtime 配置 ResultService、创建 DetectionWorker、打开 mailbox
 → Application 按现有流程调用 CameraSession::prepareInspection/startInspection()
 → Runtime::commitStart()
 → Runtime 状态 Starting → Running
 → Runtime 发布 RuntimeSnapshot
 → UI 更新按钮和状态文字
```

Application 负责“能否发起这个用例”的前置组合，Runtime 负责“在并发和设备条件下能否安全进入 Running”。两层不重复维护第二套状态机。

### 27.2 一个产品完成

```text
相机产生原始帧
 → Runtime 为当前 Run 分配 ProductKey
 → 帧进入有界 FrameQueue
 → DetectionWorker 取帧并调用当前模式 Pipeline
 → Pipeline 返回 DetectionCompletion
 → Runtime 接受算法结果并原子认领 ProductKey
 → 更新统计/延迟 NG
 → 提交存图
 → 提交/执行 PLC 请求并安排复位
 → Renderer 生成 InspectionPresentation
 → mailbox 保存/替换待显示快照
 → queued 唤醒 UI
 → Runtime::presentationReady
 → MainWindow
 → InspectionPage::present()
```

如果 UI 没有及时消费，只有最后的展示步骤受影响；统计、存图和 PLC 已经完成，不允许通过 UI 结果失败回滚生产事务。

### 27.3 停止检测

```text
用户点击“停止检测”
 → Application::stop()
 → Runtime 检查当前状态
 → Runtime 状态 Running → Stopping
 → 禁止新帧进入 ProductKey/FrameQueue
 → Application 按现有流程调用 CameraSession::stopInspection()
 → 请求 DetectionWorker 停止并等待；已返回的算法结果仍交给 Runtime 做原子认领判定
 → 已认领产品完成 CSV、存图、PLC、统计和结果呈现；记录其余未确认产品
 → 停止 PLC 新输出并完成安全复位
 → UI 线程显示 mailbox 中最后一份结果并 cancel mailbox
 → Runtime 状态 Stopping → Idle
 → Runtime 发布最终 RuntimeSnapshot
```

停止不等于“把所有计数清零”。统计清零是另一个明确用例，只在 Idle 执行。

### 27.4 Fault 自动停止

```text
相机断连/PLC 写入失败/QueueFull/运行不变量失败
 → Runtime::enterFault(reason, diagnostic)
 → 状态 Running → Fault（只允许第一次进入生效）
 → 停止接收新帧
 → 阻止新的 ResultService 事务
 → 取消 mailbox 待显示结果
 → 发布 FaultSnapshot
 → MainWindow 调用 Application::stop()
 → Runtime::finishStop() 统计未确认产品并清理运行数据
 → 状态 Fault → Idle
 → UI 显示一次故障警告
 → 新 start 创建新的 runId 和新的运行上下文
```

Fault 的收口由 Runtime 停止边界完成；旧 Run 的结果不能进入新的 Run。

## 28. API 合同的详细边界

### 28.1 UI 调用 Application 的接口

| 接口 | 输入 | 输出 | 失败如何处理 |
|---|---|---|---|
| `start()` | 模式变化、未应用设置等命令 | `StartInspectionResult` | UI 显示用户错误；Runtime 不进入 Running |
| `stop()` | 无 | `CameraRecoveryResultDto` | UI 显示停止中或相机恢复失败 |
| `openCamera()` | PLC/相机连接参数 | `OpenCameraResult` | UI 显示连接失败，不自行重试设备 |
| `connectPlc()` | 地址、rack、slot | `OperationResult` | Application 转换错误码和用户文本 |
| `resetStatistics()` | 无 | `OperationResult` | Runtime 忙时拒绝 |
| `startTemplatePreview()` | sessionId、旋转、通道 | `OperationResult` | TemplateEditorPage 显示预览失败 |

Application 接口应以业务动作命名，不使用 `forwardXxx()`、`sendToRuntimeXxx()` 这类实现导向名称。

### 28.2 Runtime 的公开接口

Runtime 只公开生产业务和安全能力：

```text
beginStart / commitStart / rollbackStart
beginStop / waitForStop / finishStop
enterFault
state / faultSnapshot / runId / isRunning
acceptFrame / submitDetectionFrame
statistics / requiresPlcForRun / resetStatistics / resetNgCount
clearPendingDelayedNgRequests
presentPreviewFrame
presentationReady / roiWarningChanged / faultSnapshotChanged
```

Runtime 不公开：

```text
ResultService &resultService()
InspectionPresentationRenderer &renderer()
UiCompletionMailbox &mailbox()
DetectionWorker *worker()
```

这些 signal 只发布数据，不把 UI 所有权交给 Runtime；ResultService、Renderer、Mailbox 和 DetectionWorker 的生命周期及调用时序由 Runtime 内部保证。

### 28.3 ResultService 的内部接口

ResultService 固定保留以下内部接口：

```text
configureRun
completionConsumer
process completion
statistics / abnormalStatistics
reset / shutdown
```

`process completion` 内部直接完成 Presentation 组装、存图和 PLC 顺序，然后调用 Runtime 的 `publishPresentation()`；不再使用 `ProcessRequest::beforePresent`、`ResultServiceCallbacks` 或任何控件函数。`preparePresentation` 和 `finalizePresentation` 不再作为 `std::function` 字段存在，改为 ResultService 的私有同步代码。以上接口只能被 Runtime 调用。Application 如果需要统计或清零，调用 Runtime 的业务接口；不要把 ResultService 的每个方法逐一复制到 Application。

## 29. Presentation 字段来源和一致性

每个字段必须只有一个主要来源：

| Presentation 字段 | 主要来源 | 生成时机 |
|---|---|---|
| `image` | 原始帧 + DetectionOverlay + Renderer | ResultService 处理完成结果时 |
| `verdictStyle` | `AlgorithmVerdict` | ResultService 组装结果时 |
| `recognitionText` | Pipeline 的 `recognizedText` | Pipeline 完成后 |
| `templateName` | 当前运行模板快照 | 仅 `updatesTemplateName` 时写入 |
| `statistics` | ResultService 事务统计 | 统计更新后 |
| `elapsedText` | `elapsedMs` + 当前运行显示规则 | 结果组装时 |

页面不得重新计算合格率、重新解释 `AlgorithmVerdict` 或根据模式 ID 猜测文字。`DetectionResultStatistics::passRatePercent()` 只做一次统一计算，避免各页面使用不同公式或小数位。

### 29.1 快照一致性

图片、判定、识别文本、模板名、统计和耗时必须从同一次 ResultService 事务组装。不能先投递图片，再单独投递统计；不能让 UI 通过多个回调拼出一个结果。

### 29.2 图像内存规则

`QImage` 是隐式共享值类型，提交后视为只读。ResultService 在提交后不修改它；从 OpenCV 转换时完成必要的深拷贝。UI 只将它转换成自己的 `QPixmap`，不把 `QPixmap` 或控件指针交回后台。

## 30. Mailbox 线程和反压细节

### 30.1 线程归属

| 操作 | 执行线程 | 说明 |
|---|---|---|
| `submit(presentation)` | Detection/ResultService 线程 | 只复制或移动数据，不触碰 QWidget |
| `Runtime::drainPresentationMailbox()` / `processOne()` | Runtime 所在线的 Qt UI 线程 | 取出最新数据并触发 Runtime 的结果 signal |
| `InspectionPage::present()` | Qt UI 线程 | 唯一允许修改 QLabel/ImageLabel 的生产结果入口 |
| `cancel()`/`reopen()` | Runtime 控制线程 | 停止和启动阶段调用 |

`InspectionRuntime` 在应用启动时创建于 UI 线程并保持该线程归属；不能把它 move 到 DetectionWorker 线程。Detection/ResultService 线程只提交数据到 mailbox，然后以 `Qt::QueuedConnection` 请求 Runtime 的 drain 方法；它们不能直接 emit 会触发 UI 的 signal，也不能直接调用 `InspectionPage`。

### 30.2 唤醒事件去重

邮箱需要一个很小的内部标志 `wakePosted`：

1. 第一次提交待显示数据时，若 `wakePosted=false`，设置为 true 并排一个 queued 唤醒；
2. 后续结果只替换邮箱中的数据，不重复排 queued 事件；
3. UI 唤醒后调用 `processOne()`，取出当前快照；
4. 若取出后邮箱仍有新快照，继续排一次唤醒；否则清除 `wakePosted`；
5. `cancel()` 同时清除数据和唤醒标志，并使已排队的旧唤醒成为空操作。

这样 Qt 事件队列中最多只有一个“请消费结果”的有效唤醒，不会因为检测频率提高而无限增长。

### 30.3 为什么允许替换待显示结果

生产事务和 UI 展示是不同速度的两条线。ResultService 必须处理每个产品，但 UI 主图通常只表达“当前最新生产状态”。在 mailbox 未消费阶段替换旧快照，可以避免检测线程等待，也避免无界内存。

如果未来需要逐件查看所有结果，应新增有界的历史记录/审计功能并明确容量和保存策略；不能把 UI mailbox 变成无界历史队列。

## 31. 当前代码到目标代码的映射

| 当前代码 | 目标代码/位置 | 处理方式 |
|---|---|---|
| `InspectionPage::resultViewBindings()` | `InspectionPage::present()` | 删除回调创建，集中更新控件 |
| `InspectionViewBindingsDto` | `InspectionPresentation` | 删除回调 DTO，使用纯数据 |
| `InspectionPresentationViewBindings` | 无 | 删除第二层回调包装 |
| `ResultService::bindView()` | `Runtime::presentationReady` | 删除绑定；ResultService 将完整 Presentation 交给 Runtime |
| `ResultServiceCallbacks` / `InspectionUiCallbacks` | Runtime 的简单数据 signal | 删除 UI 回调；ROI 状态以数据通知，存图失败写日志 |
| `Renderer::m_viewBindings` | `Renderer` 纯状态 | 删除 UI 回调成员 |
| `UiCompletionMailbox::Work` | `InspectionPresentation` | 载荷数据化、非阻塞、最新替换 |
| `m_runtime->resultService().statistics()` | `m_runtime->statistics()` | 提升为业务语义查询 |
| `m_runtime->resultService().resetStatistics()` | `m_runtime->resetStatistics()` | Runtime 内部调用 ResultService |
| `clearResultView()` | 无 | 删除；Runtime 在停止/新 Run 内部清理后台状态，Page 决定自身显示清理 |
| `clearTransientView()` | 无 | 删除；由 Page 清理临时提示，由 Runtime 清理 mailbox/内部状态 |

## 32. 分阶段的进入条件、保持项和退出条件

### 阶段 1：Presentation 数据化

进入条件：已记录当前结果显示和统计基线。

必须保持：五种模式的图片、判定图像、识别文字、模板名、统计、合格率和耗时。

禁止修改：Detection 算法、PLC 时序、存图规则、Camera 正式采集、模板 Schema、QSS。

退出条件：`InspectionPage::present()` 能用一份快照更新所有生产结果控件；旧回调链只允许作为迁移中的临时编译状态，阶段 2 结束前必须全部删除，不能保留为运行时路径。

### 阶段 2：删除控件回调

进入条件：阶段 1 已完成用户构建和结果显示回归。

必须保持：UI 页面内部 ViewBindings 和模板预览回调。

禁止修改：ResultService 统计/PLC/存图事务顺序。

退出条件：旧结果绑定符号零引用，Runtime/ResultService 不含 Qt 控件类型。

### 阶段 3：Mailbox 数据化

进入条件：阶段 2 已证明生产结果不再依赖控件函数。

必须保持：容量一、取消、停止和旧 Run 隔离语义。

新增行为：UI 慢时只替换未消费的待显示结果，生产事务不等待 UI。

退出条件：压力测试中 UI 变慢不会导致统计、PLC、存图停止；Qt 唤醒事件不无界累积。

### 阶段 4：隐藏 ResultService

进入条件：Application 已无任何控件结果绑定。

必须保持：Runtime 对 ResultService 的唯一所有权和已有事务行为。

退出条件：Application 中 `m_runtime->resultService()` 零引用，Runtime 只暴露业务语义接口。

## 33. 新增模式的具体示例

假设新增一个 `StampV2` 模式，复用现有图片、判定、识别文字、统计和耗时显示合同。

### 33.1 必改位置

```text
app/contracts/detection_mode.h
app/detection/detection_registry.*
app/detection/stamp_v2/*
app/templates/template_store.*（只有新增模板字段时）
app/system_support/settings/app_settings.*（只有新增运行设置时）
AutoOCRproject.pro（新增源文件时）
```

### 33.2 不改位置

```text
app/ui/pages/inspection_page.*
app/runtime/inspection_runtime.*
app/runtime/result_service.*
app/runtime/result_presentation_mailbox.*
```

### 33.3 只有特殊需求才扩大

如果 `StampV2` 需要新的覆盖图形，只扩展 `DetectionOverlay` 和 `InspectionPage::present()` 能理解的稳定数据；如果需要特殊 PLC 脉冲，只扩展 ResultService/PLC 合同；如果需要独特模板绘制步骤，只扩展 `ImageLabel`/`TemplateEditorPage`。不能把这些差异做成一套新的跨层回调。

## 34. 停止、Fault 和结果显示的边界案例

### 34.1 结果已完成但尚未显示

结果事务完成后，Presentation 可能仍在 mailbox 中。正常停止等待 DetectionWorker 退出后，由 UI 线程显示 mailbox 中最后一份结果，再取消 mailbox；Fault 自动停止仍直接清空未显示快照。统计、PLC 和存图状态保留在 Runtime/ResultService 快照中。

### 34.2 结果在停止过程中返回

Runtime 接受算法结果和认领 ProductKey 使用同一状态锁：认领先于正常停止时，该产品必须完成 CSV、存图、PLC、统计、完成计数和最后一次界面呈现；正常停止先于认领时，迟到结果计入“未确认/丢弃”诊断，不得进入新 Run 的 Presentation。

### 34.3 自动停止后的旧 queued 唤醒

`cancel()` 后，已经排到 Qt 事件队列的唤醒函数仍可能执行一次，但它只能发现 mailbox 已取消并立即返回，不能访问页面，不显示旧快照，也不能重开 mailbox。

### 34.4 页面关闭时的顺序

关闭窗口必须先停止生产和取消 mailbox，再断开 Runtime → UI 的 Qt 连接，最后销毁 Page。不能依靠 Lambda 内部的 `if (page)` 判空代替生命周期顺序。

## 35. 结构化告警和故障通知

正常产品结果使用 `InspectionPresentation`；异常不再通过 `ResultServiceCallbacks` 或 `InspectionUiCallbacks` 保存 UI 函数。Runtime 对 MainWindow 固定发布以下简单数据 signal：

```cpp
// Runtime 对 MainWindow 发布。
void roiWarningChanged(bool active);
void faultSnapshotChanged(InspectionFaultSnapshot snapshot);
```

存图失败由现有日志记录，不发布 Runtime → UI 通知，也不占用运行状态标签。

通知流固定为：

```text
ResultService 发现或汇总异常数据
 → Runtime 判断是否影响生产并发布数据 signal
 → MainWindow 接收
 → InspectionPage 显示或清除提示
```

ROI 警告只改变 `roiWarningChanged(active)`；存图失败传递累计失败次数和最近错误；Runtime Fault 复用已有 FaultSnapshot/故障通知。`clearPreviousOverlay` 不是告警，删除 ResultService 到 TemplateEditorPage 的控件回调；模板绘制取消由 UI 自己的页面生命周期或开始/停止用例处理。PLC 写入失败仍必须由 Runtime 进入 Fault。UI 不根据文字内容猜测哪个告警需要停机，也不需要 EventBus、NotificationManager、WarningCenter 或每种告警一个复杂结构体。

## 36. 性能和容量验证方法

本方案不把任何假设耗时写成事实。实施 mailbox 阶段时记录四个时间：

```text
t_result：ResultService 生成 Presentation 的耗时
t_submit：提交 mailbox 的耗时
t_present：InspectionPage::present() 的耗时
t_interval：相邻产品结果产生的时间间隔
```

至少观察：平均值、最大值和高分位值（例如 P95/P99）。判断规则：

- 若 `t_present` 长期小于 `t_interval`，mailbox 通常不会替换结果；
- 若偶发 `t_present > t_interval`，容量一 mailbox 吸收短时抖动；
- 若长期 `t_present > t_interval`，只保留最新待显示结果，不能让生产事务等待 UI；
- 若业务要求逐件图像审阅，另行设计有界历史记录，不修改生产事务链。

测量只用于确认 UI 呈现策略，不用于删除 Runtime 状态机、FrameQueue、PLC 保护或 ProductKey 去重。

## 37. 实施时的代码审查问题

每次提交前按以下问题审查，而不是只看文件数量：

1. 新增代码是否把 QWidget、Page 或控件 Lambda带入 Application/Runtime/ResultService？
2. 新增结果字段是否属于完整 Presentation，而不是又增加一个单独回调？
3. 新增检测模式是否修改了通用 ResultService 或 UI，而实际需求只是 Pipeline 差异？
4. UI 卡顿时，统计、PLC、存图和 Fault 是否仍按原顺序完成？
5. 正常停止或故障自动停止后，旧 Run 的 Frame、Completion、Presentation 和 queued 唤醒是否都会被隔离？
6. 是否新增了没有状态、不变量或第二个调用者的包装类？
7. 是否在多个层重复同一状态检查，造成不同错误文本或不同裁决？
8. qmake 清单、include 方向和析构顺序是否与目标边界一致？

## 38. 最终验收矩阵

| 场景 | 应看到的结果 | 负责层 |
|---|---|---|
| 正常 OK 产品 | 统计+1、PLC OK、存图、UI 显示 OK | ResultService + UI |
| 正常 NG 产品 | NG 统计/延迟 NG/PLC NG/存图/UI 显示 NG | ResultService + UI |
| 重复 ProductKey | 不重复统计、不重复 PLC、不重复存图 | Runtime/ResultService |
| UI 变慢 | UI 只显示最新待显示快照，生产事务继续 | Mailbox + ResultService |
| PLC 断连 | Runtime 进入 Fault，停止新生产并自动收口，UI 显示一次警告 | Runtime + UI |
| 相机断连 | Runtime 进入规定故障或停止状态 | Runtime/Camera |
| 正常停止时已认领结果 | 完成 CSV、存图、PLC、统计、完成计数并显示最后结果 | Runtime/ResultService/Mailbox |
| 正常停止后未认领结果 | 不进入正式结果事务，不进入新 Run，不更新 UI | Runtime/ResultService/Mailbox |
| 模板预览 | 只更新 TemplateEditorPage，不改生产统计和 PLC | Application + TemplateEditorPage |
| 新增普通模式 | 主要改 Detection/模板/设置，通用 UI 不变 | Detection |

## 39. 方案实施前的明确前提

本文已经固定了架构选择，并已获得本轮代码实施批准。四个阶段的提交为：

```text
5da95b4  refactor: migrate inspection presentation contract
b169bb4  refactor: remove cross-layer UI callbacks
010b57b  refactor: make presentation mailbox nonblocking
8db119e  refactor: hide result service behind runtime
```

阶段提交前已执行差异检查、旧符号检索、qmake 清单路径检查和 Qt moc 解析。当前尚未完成 Qt Creator 的 Run qmake/Rebuild、程序运行、相机/PLC、Fault 自动停止和生产现场验证；这些验证由用户在全部阶段完成后统一执行。若构建或运行暴露源码事实与本文冲突，应暂停后续修订，核对真实调用链，不得用文档强行覆盖生产行为。

## 40. 结论

最终流程保持短而明确：

```text
UI 命令 → Application 用例 → Runtime 生产控制
Detection → ResultService 单产品事务 → Runtime
Runtime → PresentationMailbox → Runtime::presentationReady
Runtime::presentationReady → MainWindow → InspectionPage::present() → 控件
模板预览 → 独立预览链
```

Application 不再发布或转发产品结果、告警和 UI 回调；Runtime 不保存 sink、页面引用或控件 Lambda；ResultService 不直接改控件。普通新增检测模式只需进入 DetectionRegistry、Pipeline、模板和设置，通用生产事务和页面显示合同保持稳定。这是在简单、清晰、可扩展和不增加空壳架构之间的平衡点。
