# runtime：生产运行时

本目录管理一次正式检测的生命周期、线程、队列、结果、统计、PLC 输出和存图。它不解释模板 JSON，也不拥有设置编辑。

## 主要文件

| 文件 | 职责 |
|---|---|
| `inspection_runtime.h/.cpp` | 唯一运行实例；创建不可变 `InspectionRunContext`，启停工作线程和 Fault。 |
| `camera_session.h/.cpp` | 相机预览/正式采集会话，复用 `contracts/camera_operation_result.h` 并向 Runtime 提交帧。 |
| `capture_worker.h/.cpp` | 采集线程。 |
| `frame_queue.h/.cpp` | 有界帧队列。 |
| `detection_worker.h/.cpp` | 单检测工作线程和执行器。 |
| `result_service.h/.cpp` | 每产品结果收口、统计、PLC、存图调用。 |
| `inspection_plc_controller.h/.cpp` | PLC 运行写入和延迟剔除队列。 |
| `image_save_service.h/.cpp` | 原图/标注图保存。 |
| `result_presentation_mailbox.h/.cpp` | 工作线程到 UI 的有界结果邮箱。 |
| `contracts/inspection_presentation.h`、`inspection_presentation_renderer.*` | 跨层结果显示数据和运行时渲染。 |

## 生命周期

```text
Idle
 → beginStart(AppSettings + PreparedTemplate/纸巾阈值)
 → Starting
 → startDetection + startInspection
 → commitStart
 → Running
 → beginStop / Fault
 → Stopping
 → finishStop
 → Idle
```

运行上下文形成后不再读取磁盘或 UI。用户修改模板和设置只在停止并再次启动后生效。

## 维护规则

- 不隐式创建 Run；只有 `beginStart()` 能创建。
- 不将模板编辑状态放入 Runtime。
- 保持一次产品一次最终结果、队列容量、Fault 人工恢复、PLC 脉冲和存图合同。
- Runtime 可以依赖 Detection 和设备抽象，不能反向依赖 Application 或 UI。
