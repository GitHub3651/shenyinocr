# OCRGangYin 正常停止已认领结果完整收口方案

## 1. 状态与范围

- 状态：代码实施完成，静态检查通过，待用户统一验证。
- 实施基线：分支 `codex/ocrgangyin-refactor`，HEAD `b72c367`。
- 当前进度：五个生产文件完成原子认领、正常停止最后结果收口和旧路径删除；生产代码旧符号零引用，差异检查通过。
- 权威范围：正常停止时正式结果的认领边界、完整结果收口和最后一次界面呈现。
- 替代关系：本文仅替代 `OCRGangYin应用运行时界面边界精简方案.md` 中“正常停止清空待显示结果”和 `Stopping` 仍可认领结果的旧行为；Fault 取消未完成结果、CSV/存图/PLC 协议和统计口径保持不变。
- 实施门禁：Agent 只修改计划内文件并执行轻量静态检查；构建、运行、相机、PLC 和现场验证由用户完成。

## 2. 最终行为

`InspectionRuntime::complete()` 是算法结果进入正式结果事务的唯一原子认领点：

- `Starting` 或 `Running` 下，合法产品直接从 `Accepted` 进入 `Claimed`。
- `Stopping` 或 `Fault` 下拒绝新结果，不执行 CSV、存图、PLC、统计和界面呈现。
- 已经进入 `Claimed` 的产品不受随后到达的正常停止影响，继续完成 CSV、存图任务提交、PLC、Presentation、统计和 `finalizeResultClaim()`。
- 正常停止等待 DetectionWorker 退出后，同步呈现 mailbox 中最后一份结果，再取消 mailbox 并回到 `Idle`。
- Fault 继续立即取消 mailbox；真实步骤失败时不提交正常统计或完成数。

## 3. 最小代码范围

- `app/runtime/inspection_runtime.h/.cpp`
  - 删除 `ProductProgress::AlgorithmCompleted` 和 `claimResult()`。
  - `complete()` 在现有 Runtime 锁内直接完成认领。
  - 正常停止不提前取消 mailbox；Worker 退出后由 `finishStop()` 呈现最后结果并取消 mailbox。
  - `requestDetectionWorkerStop()` 只负责 Worker，不再管理界面结果。
- `app/runtime/detection_worker.cpp`
  - 算法返回后不再因停止请求丢弃结果，统一交给 `InspectionRuntime::complete()` 决定是否认领。
- `app/runtime/result_service.cpp`
  - 删除已经失去用途的二次认领调用，保留现有正式结果事务顺序。
- `app/ui/main_window/main_window.cpp`
  - `presentationReady` 使用默认 `Qt::AutoConnection`；Runtime 的 drain 固定在 UI 线程，停止收口时可同步更新页面。

不新增锁、状态、线程、事务类、回滚层、兼容接口或转发包装。

## 4. 静态门禁

- `claimResult` 和 `AlgorithmCompleted` 生产代码零引用。
- `beginStop()`、`requestDetectionWorkerStop()` 不取消 mailbox。
- DetectionWorker 取帧前仍响应停止；算法已返回的结果不在 Worker 内二次丢弃。
- `finishStop()` 只在正常 `Stopping` 状态同步 drain 最后一份 Presentation，随后统一取消 mailbox。
- `presentationReady` 不再额外排一次 Qt queued 连接。
- `ResultService` 的 CSV、存图、PLC、Presentation、统计和完成计数顺序不变。
- `git diff --check` 通过；不执行构建或程序运行。
