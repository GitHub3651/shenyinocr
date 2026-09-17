# OCRGangYin 运行故障自动停止方案

## 1. 文档状态

- 编写日期：2026-09-13。
- 当前状态：代码实施完成，待用户统一验证。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `3115fa6`。
- 权威范围：运行故障的首因记录、自动停止、正式采集意外停止、未完成产品收口、相机 Fault 关闭相机、PLC Fault 断开 PLC 和一次性故障警告。
- 关联范围：`OCRGangYin图像上方常驻工具栏与设备状态重构方案.md` 负责工具栏、设备状态和硬触发位置。
- 实施关系：本文与关联工具栏方案组成同一最终实施批次；逻辑上先收口 Fault 接口和停止链，再一次性落地最终工具栏，不保留中间兼容结构。
- 实施门禁：生产代码和现行说明已完成实施；Agent 只执行静态门禁，构建、运行和真实设备验证由用户统一完成。

## 2. 实施依据

- `InspectionRuntime::enterFault()` 是运行故障首因的唯一接收入口，负责停止正式结果投递并请求检测 Worker 停止。
- `InspectionApplicationService::stop(InspectionFaultReason)` 统一负责停止相机采集、等待检测 Worker、按原因关闭相机或断开 PLC、恢复普通停止后的相机预览、完成运行收口、清理活动模板路径并发布运行快照。
- `CaptureWorker::stop()` 同步中断等待并完成 `join()`；调用返回时采集线程已经退出。
- `InspectionRuntime::finishStop()` 是运行数据和统计的唯一停止收口边界。
- 首因被接受时必须清空待执行的延迟 NG 请求，避免本轮遗留请求在后续运行中触发 PLC 动作。
- `ResultService::process()` 是唯一正式结果事务；产品只有在 CSV、统计、存图步骤、PLC 和呈现均走完当前合同后才记为已完成。

## 3. 最终行为

任一现有 Fault 源，或正式采集意外停止，均通过 `InspectionRuntime::enterFault()` 执行以下唯一流程：

1. Runtime 保存首因快照，清空待执行的延迟 NG 请求，停止接收新的正式结果、阻止尚未收口的产品提交为已完成，并请求检测 Worker 停止。正式采集意外停止以 `CameraDisconnected` 进入此入口；模板预览不进入运行 Fault。
2. MainWindow 现有 `faultSnapshotChanged` 排队连接收到快照后，立即把快照中的原因传给 `InspectionApplicationService::stop()`。
3. ApplicationService 停止相机采集并等待检测 Worker 完全退出。
4. 原因为 `CameraDisconnected` 时，ApplicationService 调用现有相机关闭操作，不尝试重新打开相机或恢复预览；其他 Fault 在停止前相机仍打开时，沿用普通停止的预览恢复。
5. 原因为 `PlcDisconnected` 时，ApplicationService 在停止链内部直接调用 `m_runtime->disconnectPlc()`；不调用只允许空闲态人工操作的 `InspectionApplicationService::disconnectPlc()`。相机和 PLC Fault 即使在停止调用到达时 Runtime 已完成收口，也执行对应硬件关闭或断开。
6. `InspectionRuntime::finishStop()` 记录本轮尚未完成正式结果收口的产品为未确认，清理运行数据和 Fault 快照并回到 `Idle`。
7. ApplicationService 清理活动模板路径并发布最终运行快照。
8. MainWindow 按相机恢复结果刷新界面，在停止及对应硬件处置全部完成后显示一次故障警告。

“已完成”专指已经完成正式结果收口的产品：已按配置处理 CSV，形成并提交正常统计，执行存图步骤，完成当前 PLC 合同，并成功投递结果呈现。存图继续沿用现有异步合同，收口边界是无需存图或应保存图像已被 `ImageSaveService` 接收；运行中拒绝接收应保存图像时以 `RuntimeInvariantViolation` 停止，后台写盘失败仍走现有存图失败提示，不增加等待回执或回滚层。PLC 未启用时该步骤无需输出；延迟 NG 模式以本件延迟请求登记完成为当前 PLC 合同完成，不等待后续产品到达时的实际输出。

故障前已经完成收口的正式结果、统计、存图任务和 PLC 动作保持原值。尚未完成收口的当前产品只记入未确认数量，不提交正常统计，不重复存图或 PLC 输出。

`InspectionRuntimeState::Fault` 仅在 Runtime 内部用于首因记录和停止收口。`RuntimeSnapshot` 对外发布 `Idle`、`Starting`、`Running`、`Stopping`；Fault 收口期间公开为 `Stopping`，完成后公开为 `Idle`。故障快照由 `faultSnapshotChanged` 直接交给 MainWindow 的警告入口。

程序保持运行。故障收口后，界面处于空闲态；相机和 PLC 的实际连接状态来自最终 `RuntimeSnapshot`。相机 Fault 后相机为关闭，PLC Fault 后 PLC 为未连接，操作员处理故障并重新连接对应硬件后可开始新的识别；其他 Fault 不主动关闭硬件。

故障处理不需要人工确认、恢复确认或故障解锁。不存在故障锁定态；停止及对应硬件处置完成后自动回到空闲态，故障警告只显示一次。

## 4. Runtime 收口

`InspectionRuntime::complete()` 只把产品从 `Accepted` 推进到 `AlgorithmCompleted` 并维护算法完成顺序，不再累加运行完成数。运行内计数改为 `m_finalizedProductCount`，只由 `finalizeResultClaim()` 在产品从 `Claimed` 完成正式结果收口并移出 `m_products` 时累加一次。

`ResultService::process()` 继续在现有函数内完成结果收口，不增加事务类或回滚框架：先计算本件候选统计和延迟 NG 请求，按配置完成 CSV、存图任务提交和当前 PLC 动作，再用候选统计投递呈现；运行中存图任务被拒绝或呈现投递失败时进入 `RuntimeInvariantViolation`。只有各步骤完成且 Runtime 未进入 Fault 时，才提交正常统计和本件延迟 NG 请求，并在末尾调用一次 `finalizeResultClaim()`。CSV、存图提交、PLC 或呈现失败进入 Fault 后不得提交统计或已完成数；故障后已认领但未最终提交的产品保留在 `m_products`，由 `finishStop()` 计为未确认。

`InspectionRuntime::finishStop(bool writeRunSummary = true)` 同时处理正常停止和 Fault 收口：

- 调用前沿用 `waitForStop()`，确保检测 Worker 已退出。
- 在锁内读取停止前状态、首因快照、已接收数、正式结果已完成数和 `m_products.size()`，清空产品、已接收帧、运行上下文和 Fault 快照，把状态设为 `Idle`。
- 在锁外仅当停止前存在 Fault 时，使用剩余产品数量写入本次 Fault 停止日志；未确认数量只属于本次运行停止摘要。
- 正常停止继续记录原有停止摘要；Fault 停止使用首因、接收数、正式结果已完成数和未确认数写入同一运行日志。

`finishStop()` 是唯一停止收口点；完成后不再调用独立的故障产品对账、故障确认或恢复确认函数。

`InspectionFaultSnapshot` 只包含 `reason`、`acceptedProductCount` 和 `finalizedProductCount`。`enterFault()` 的最终签名为 `bool enterFault(InspectionFaultReason reason, const QString &diagnostic)`，直接比较 `reason` 判断首因是否已经记录；诊断文字只在故障发生时写入日志。

`InspectionFaultReason` 的最终枚举为 `None`、`CameraDisconnected`、`PlcDisconnected`、`HardTriggerQueueOverflow`、`RuntimeInvariantViolation` 和 `BarcodeCsvUnavailable`。`None` 只表示正常停止，其余每个原因均对应当前生产代码中的真实故障入口。

`enterFault()` 在首因被接受后、Runtime 锁外直接调用 `ResultService::clearPendingDelayedNgRequests()`，再以本次锁内复制的 `enteredFault` 发出故障快照并请求 Worker 停止。Fault 后到达的图像由 Worker 活动检查和 `acceptFrame()` 状态检查直接拒绝。

## 5. 停止接口

停止链只传递调用者和界面实际使用的信息：

```cpp
CameraRecoveryResultDto stop(InspectionFaultReason reason);
```

正常停止显式传入 `InspectionFaultReason::None`；自动故障停止直接传入信号快照中的原因。该参数只用于本次调用内选择相机关闭、PLC 断开或普通停止后的预览恢复，不保存为第二份状态。

`CameraSession::stopInspection()` 返回 `void`，内部同步中断等待并完成 `join()`。ApplicationService 的停止返回值只使用 `CameraRecoveryResultDto`。

删除 `StopInspectionCommand`、`StopInspectionResult`、`StopInspectionIssue` 及其中的 Fault 恢复字段；停止合同不再携带确认、恢复或锁定信息。

ApplicationService 在调用 `CameraSession::stopInspection()` 前读取 `m_cameraSession->isOpen()`。同步停止并等待检测 Worker 后按本次原因直接分支：`CameraDisconnected` 调用 `CameraSession::close()` 并返回 `recoveryAttempted=false`、`cameraOpen=false`；其他原因仅在相机先前仍打开时调用现有 `restorePreviewReady()`。`PlcDisconnected` 在同一停止链中直接调用 `m_runtime->disconnectPlc()`。Runtime 已为 `Idle` 时不重复停止或恢复预览，但相机和 PLC Fault 仍完成对应硬件关闭或断开，随后发布最终快照并返回当前相机状态。

`MainWindow::finishInspectionStopUi()` 改为接收 `const CameraRecoveryResultDto &`，只处理仍然存在的曝光恢复结果、模板绘制清理、统计刷新和停止后的界面状态。最终运行状态继续由 `runtimeSnapshotChanged` 发布；内部 Fault 对外映射为 `ApplicationRuntimeState::Stopping`，`RuntimeSnapshot::isInspectionBusy()` 只判断启动、运行和停止。`MainWindow::closeEvent()` 删除停止前状态分支和第一次 `shutdown()` 调用，只保留一次无条件关闭流程。

## 6. MainWindow 故障入口

MainWindow 构造函数中的 `faultSnapshotChanged` 排队连接是唯一界面入口：

1. 接收信号携带的 `InspectionFaultSnapshot`。
2. 调用 `m_inspectionApplicationService->stop(snapshot.reason)`，将相机恢复结果交给 `finishInspectionStopUi()`。
3. 在一个 `switch` 中将 Fault 原因转换为操作员可读文字和处理提示；相机、PLC Fault 分别提示重新连接相机或 PLC，其他 Fault 不提示重连硬件。
4. 调用一次 `QMessageBox::critical()`。

`enterFault()` 只为本次运行的首个 Fault 发出信号，因此每次运行只会触发一次这一入口。

`CameraSession::handleCaptureStopped()` 在正式采集意外停止时直接走 `enterFault(CameraDisconnected, ...)` 回调，不再发出布尔型 `captureStopped(bool)`。模板预览意外停止调用 `CameraSessionCallbacks::templatePreviewStopped`；`InspectionApplicationService` 发出无布尔参数的 `templatePreviewStopped()`，由 `TemplateEditorPage` 更新模板预览状态。

警告原因和处理提示只在上述 lambda 的 `switch` 中转换一次。排队连接携带完整的 `InspectionFaultSnapshot`，Qt 元类型只声明并注册该快照；不再保留 `InspectionRuntime::faultSnapshot()` 查询入口或 MainWindow 的第二条故障呈现路径。

`MainWindow` 是故障自动停止和警告的唯一界面所有者。删除 `InspectionPage::presentFault()`、`InspectionPage::confirmFaultRecovery()`、`InspectionPage::restoreNormalFaultStyle()`、`InspectionFaultPresenter` 及 `m_faultAlarmPresented`；检测页不再负责故障确认、恢复或系统故障弹窗。

## 7. 警告文案

标题固定为：

```text
系统故障－识别已停止
```

正文固定为：

```text
系统发生故障，识别已自动停止。

故障原因：{故障原因}
本次运行已接收 {数量} 件，已完成 {数量} 件。

请检查输送线状态和故障期间的产品。
{按故障原因生成的处理提示}
```

原因映射覆盖当前有效 `InspectionFaultReason`：相机异常、PLC 异常、硬触发队列溢出、运行状态异常和二维码 CSV 写入异常。相机异常的处理提示说明相机已关闭并要求检查、重新连接相机；PLC 异常说明 PLC 已断开并要求检查、重新连接 PLC；其余原因只要求处理对应问题后重新开始识别，不要求重连硬件。

## 8. 文件范围

| 文件 | 最终职责 |
|---|---|
| `app/runtime/inspection_runtime.h/.cpp` | Runtime 内部 Fault 收口、正式结果完成计数、三字段故障快照、延迟 NG 请求清理和本次停止摘要 |
| `app/runtime/camera_session.h/.cpp` | 同步停止采集、正式采集意外停止进入相机异常 Fault、相机 Fault 直接关闭、其他停止的相机预览恢复和模板预览停止回调；删除布尔型 `captureStopped` |
| `app/runtime/result_service.h/.cpp`、`app/contracts/inspection_presentation.h` | 在现有 `process()` 内完成正式结果事务、延迟 NG 请求清理、普通检测统计和结果呈现 |
| `app/application/inspection_application_service.h/.cpp` | 统一 `stop(reason)`、相机恢复返回值、公开停止状态映射、相机关闭、Runtime PLC 断开和 `templatePreviewStopped()`；删除旧停止类型、`completeUnexpectedAcquisitionStop()` 和布尔型采集停止信号 |
| `app/application/runtime_snapshot.h` | 对外发布空闲、启动、运行和停止状态以及设备连接状态 |
| `app/ui/main_window/main_window.h/.cpp` | 在现有排队连接内自动停止并显示一次警告；删除 `m_faultAlarmPresented`、故障呈现辅助函数和旧 Fault 显示连接 |
| `app/ui/main_window/main_window_inspection.cpp` | 最终 `stopInspection()` 入口、停止后的检测界面刷新和单次关闭流程 |
| `app/ui/main_window/main_window_settings.cpp` | 删除旧停止槽及其 Fault 确认/恢复职责；不再承担停止流程 |
| `app/ui/main_window/inspection/inspection_page.h/.cpp` | 检测结果、统计、预览图、普通运行状态和存图失败提示；删除 Fault 显示、确认和恢复函数 |
| `app/ui/main_window/inspection/inspection_fault_presenter.h/.cpp` | 删除，不再登记到工程 |
| `app/ui/main_window/operation_ui_policy.h/.cpp` | 相机、运行、停止和模板状态的权限规则 |
| `app/ui/main_window/template/template_editor_page.cpp` | 连接无布尔参数的 `templatePreviewStopped()`，并在停止时取消模板绘制 |
| `app/resource/qss/app_theme.qss` | 停止状态和普通判定的视觉规则 |
| `app/AutoOCRproject.pro` | 与最终源文件一致的工程清单 |
| `app/application/README.md`、`app/runtime/README.md`、`app/ui/README.md` | 当前停止链和模块职责 |
| `docs/development/OCRGangYin计划索引.md` | 当前计划状态和路由 |
| `docs/development/OCRGangYin开发者代码结构与维护指南.md` | 当前停止链和界面职责 |
| `docs/development/OCRGangYin现有功能对照表.md` | 当前故障行为与验证状态 |

本文不修改 `.ui` 布局、工具栏对象、硬触发位置、AppSettings Schema、模板 Schema、Detection、Devices、PLC 协议、算法或资源文件；只调整现有正式结果流程的提交时点和运行内完成计数，不改变 CSV 格式、统计口径、存图策略或 PLC 协议。关联工具栏方案在同一实施批次负责最终 `.ui` 和工具栏改造。

每个受影响文件在完成最终职责后，同步清理失去用途的 include、前置声明、字段、函数、局部辅助代码、源文件和工程条目。文档只保留当前最终结构和行为。

## 9. 联合实施顺序

1. 在同一代码批次中先把算法完成与正式结果完成计数分开，将 `finalizeResultClaim()` 移到现有 `ResultService::process()` 的结果收口末尾。
2. 收口 `finishStop()`、最小 Fault 快照和延迟 NG 清理，再将正式采集意外停止接入 `CameraDisconnected` Fault；不保留旧停止合同。
3. 将模板预览改为独立的无参数停止通知，并删除布尔型采集停止回调链。
4. 把相机停止收敛为同步 `void`；应用服务按原因选择相机关闭、`m_runtime->disconnectPlc()` 或普通预览恢复，并直接返回 `CameraRecoveryResultDto`。
5. 直接改写 MainWindow 现有 `faultSnapshotChanged` 排队 lambda，完成自动停止、停止刷新、原因转换和一次性警告；同时删除检测页确认链、Presenter、旧停止结果类型和重复关闭调用。
6. 与工具栏方案一次性修改 `.ui`、统一按钮、硬触发位置、操作权限、QSS、工程清单和模块说明。
7. 执行联合静态门禁，不建立新旧停止链或新旧工具栏并存的过渡阶段。

## 10. 最终运行结构门禁

- Runtime 运行状态为 `Idle`、`Starting`、`Running`、`Stopping`、`Fault`；`Fault` 只在 Runtime 内部参与首因记录和停止收口。
- Application 和 UI 运行状态只包含空闲、启动、运行和停止；界面通过 `Stopping` 和最终 `Idle` 呈现自动停止过程。
- `InspectionFaultReason` 只包含 `None`、`CameraDisconnected`、`PlcDisconnected`、`HardTriggerQueueOverflow`、`RuntimeInvariantViolation` 和 `BarcodeCsvUnavailable`；每个非 `None` 原因均有真实生产入口。
- `InspectionFaultSnapshot` 只包含 `reason`、`acceptedProductCount` 和 `finalizedProductCount`，并作为 `faultSnapshotChanged` 的唯一参数。
- `m_finalizedProductCount` 只在 `finalizeResultClaim()` 成功提交正式结果时增加；`InspectionRuntime::complete()` 不再累加产品完成数。
- `ResultService::process()` 只在正式结果链走完且 Runtime 未进入 Fault 时提交候选统计、登记本件延迟 NG 请求并调用 `finalizeResultClaim()`；故障产品仍留在 `m_products` 等待停止摘要收口。
- `InspectionApplicationService::stop(InspectionFaultReason)` 直接返回 `CameraRecoveryResultDto`；`CameraSession::stopInspection()` 返回 `void`。
- 正式采集意外停止进入 `CameraDisconnected` Fault；模板预览意外停止只发出无参数的 `templatePreviewStopped()`。
- MainWindow 只在现有 `faultSnapshotChanged` 排队 lambda 中执行自动停止、停止后刷新、原因转换和警告。
- `ApplicationRuntimeState`、`OperationUiState` 和 UI 不存在 `Fault` 对外状态；`InspectionRuntimeState::Fault` 仅用于 Runtime 内部首因记录和停止收口。
- 以下旧接口、状态、统计、回调和样式项全部删除并保持零引用：

  ```text
  ApplicationRuntimeState::Fault
  OperationUiState::Fault
  StopInspectionCommand
  StopInspectionResult
  StopInspectionIssue
  StopInspectionCommand::acknowledgeFault
  CameraCaptureStopResult
  CameraCaptureStopResult::allStopped()
  CameraCaptureStopResult::shouldRestoreCamera()
  reconcileFaultProducts()
  acknowledgeFault()
  confirmFaultRecovery()
  presentFault()
  restoreNormalFaultStyle()
  presentInspectionFault()
  restoreNormalFaultUi()
  faultReasonText()
  m_faultAlarmPresented
  InspectionFaultPresenter
  CameraSessionCallbacks::captureStopped
  captureStopped(bool)
  completeUnexpectedAcquisitionStop()
  InspectionRuntime::faultSnapshot()
  Q_DECLARE_METATYPE(InspectionFaultReason)
  qRegisterMetaType<InspectionFaultReason>()
  InspectionRuntime::m_completedProductCount
  beforeShutdown
  QLabel#label_runtimeStatus[uiState="fault"]
  QLabel#label_verdictResult[verdict="fault"]
  ```

- 未确认数量只在 `finishStop()` 的本次 Fault 停止摘要中由剩余 `m_products.size()` 计算。延迟 NG 请求只由 `enterFault()` 清理一次。
- 不存在人工故障确认、恢复确认、故障锁定或第二个故障弹窗入口；`InspectionFaultPresenter` 及检测页 Fault 显示/恢复函数零引用。
- `InspectionPage` 只负责检测结果、统计、预览图、普通运行状态和存图失败提示。
- `OperationUiPolicy` 只处理相机、识别、停止和模板操作状态；QSS 只处理普通运行状态和产品判定样式。
- `InspectionRunContext::runId` 继续作为产品身份的一部分；运行快照只发布界面实际消费的数据。
- `AutoOCRproject.pro` 只登记实际存在且被生产工程使用的文件。
- 受影响文件不存在失去用途的 include、前置声明、字段、函数、局部辅助代码或工程条目。

## 11. 验证

### 11.1 Agent 静态门禁

- `InspectionApplicationService::stop(InspectionFaultReason)` 直接返回 `CameraRecoveryResultDto`；正常停止传入 `None`，故障停止传入首因。
- `InspectionFaultReason` 的非 `None` 枚举值均能从生产代码定位到对应的 `enterFault()` 调用链，警告原因映射与枚举逐项一致。
- `CameraSession::stopInspection()` 返回 `void` 并同步停止采集线程；相机 Fault 直接关闭相机且不尝试恢复预览，其他停止以停止前相机是否打开为预览恢复依据。
- `InspectionRuntime::complete()` 只表示算法完成；`finalizeResultClaim()` 在正式结果收口末尾累加 `m_finalizedProductCount`，警告中的完成数读取同一计数。
- `InspectionRuntime::finishStop()` 同时覆盖正常停止和 Fault；未完成正式结果收口的产品只记录一次，完成后状态为 `Idle`。
- Application 和 UI 的公开运行状态只有空闲、启动、运行和停止；Fault 收口期间公开为 `Stopping`。
- `faultSnapshotChanged` 的排队 lambda 直接执行自动停止和警告；每次运行只在停止及对应硬件处置完成后显示一次故障警告，故障快照只携带原因、接收数和正式结果完成数。
- 正式采集意外停止以 `CameraDisconnected` 进入自动停止并使用同一停止链。
- 模板预览意外停止只发出无布尔参数的 `templatePreviewStopped()`，正式采集不经过该信号。
- 未确认数量只写入本次 Fault 停止日志，首因被接受时清空待执行的延迟 NG 请求。
- `CameraDisconnected` Fault 调用相机关闭操作、不调用预览恢复，并提示重新连接相机；`PlcDisconnected` Fault 调用 PLC 断开操作并提示重新连接 PLC。最终快照反映对应硬件未连接状态。
- 非硬件 Fault 只停止识别，不主动关闭相机或断开 PLC，也不提示重新连接硬件。
- 故障前的正式结果保持原值；未完成产品不产生 NG、重复结果、重复存图或重复 PLC 输出。
- CSV 写入成功但后续 PLC 进入 Fault 时，本件不提交正常统计或完成数；完整走完 CSV、统计、存图步骤、PLC 和呈现的产品只增加一次完成数。
- `MainWindow::closeEvent()` 只调用一次关闭流程。
- `AutoOCRproject.pro` 的文件清单完整且无重复。
- `InspectionFaultPresenter` 的源文件和工程条目已删除，旧停止合同、旧回调和旧故障确认链全部零引用。
- 受影响文件中的 include、前置声明、字段、函数、局部辅助代码和工程条目均与最终职责一致。
- `.ui`、Schema、模板、算法、设备协议和工具栏布局没有计划外差异。
- UTF-8、文件末尾换行和 `git diff --check` 通过。

### 11.2 用户统一验证

1. Qt Creator 执行 Run qmake、Clean 和 Release Rebuild。
2. 分别制造相机错误、正式采集意外停止、PLC、硬触发队列、运行状态和二维码 CSV Fault，确认识别自动停止且程序保持运行。
3. 确认每次运行只在停止及对应硬件处置完成后出现一次“系统故障－识别已停止”，并包含首因、接收数和正式结果完成数。
4. 检查警告中的“已完成”只统计已经走完 CSV、统计、存图步骤、PLC 和呈现的产品；未完成产品只计入未确认数量。
5. 制造 `CameraDisconnected` Fault 或正式采集意外停止，确认相机被关闭、未尝试恢复预览，警告提示检查并重新连接相机。
6. 制造 `PlcDisconnected` Fault，确认 PLC 显示未连接，警告提示检查并重新连接 PLC；相机保持打开并恢复预览。
7. 分别制造硬触发队列、运行状态和二维码 CSV 非硬件 Fault，确认只停止识别，相机和 PLC 不被主动关闭或断开，警告不要求重连硬件。
8. 确认 Fault 收口后 Runtime 为 `Idle`，无需重启软件即可处理问题、重新连接对应硬件并开始识别。
9. 在产生待执行的延迟 NG 请求后制造 Fault，确认故障收口及重新开始识别后旧请求不会再触发 PLC 动作。
10. 验证正常停止仍执行相机预览恢复、统计刷新和界面清理，模板预览意外停止仍退出预览状态。
11. 回归软硬触发、剔除、CSV、存图、模板和五种检测模式。

## 12. 完成条件

- 任一现有 Fault 通过唯一入口调用停止链并回到 `Idle`。
- 故障前正式结果保持原值，完成数只统计已经走完 CSV、统计、存图步骤、PLC 和呈现的产品，未完成产品只记为未确认一次。
- 相机 Fault 关闭相机且不尝试恢复预览，PLC Fault 断开 PLC，非硬件 Fault 不主动处置硬件；程序始终保持运行。
- 正式采集意外停止作为相机异常进入同一 Fault 自动停止链并关闭相机。
- 停止接口只保留故障原因入参和相机恢复返回值；同步停止、按原因进行硬件处置或预览恢复以及延迟 NG 请求清理没有语义损失。
- 每次运行只有一个包含首因、数量和对应处理提示的故障警告，并在停止完成后显示。
- 停止链保持单一实现，生产代码和文档只包含最终运行结构。
- Agent 静态门禁通过，用户完成 Qt Creator、交互和真实设备验证。
