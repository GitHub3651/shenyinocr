# application：应用用例层

本目录回答“用户点一次按钮后，跨相机、PLC、设置、模板和运行时的操作如何完成”。它编排流程，但不实现 QWidget、设备 SDK、图像算法或磁盘格式。

## 调用位置

```text
UI
 ↓ 结构化命令
application
 ├─ SettingsApplicationService → AppSettingsStore
 ├─ TemplateApplicationService → TemplateStore / 条码引擎
 └─ InspectionApplicationService
      ├─ CameraSession / InspectionRuntime
      ├─ SettingsApplicationService::current()
      └─ TemplateStore::loadPrepared()
```

正式检测不经过模板编辑服务：

```text
当前 AppSettings
 → 读取当前模式模板路径或纸巾阈值
 → TemplateStore 严格加载（纸巾跳过）
 → 创建不可变 PreparedTemplate 数组
 → InspectionRuntime
```

## 文件

| 文件 | 职责 |
|---|---|
| `application_result.h` | 应用操作的统一成功/失败结构。 |
| `runtime_snapshot.h` | UI 可读取的运行状态快照。 |
| `inspection_start_preflight.h/.cpp` | 只检查忙碌、相机、PLC、脏设置、运行模板数量和读码器状态；模板字段由 `TemplateStore` 校验。 |
| `inspection_application_service.h/.cpp` | 开关相机、启停检测、PLC 操作、模板运行加载、采集方式 DTO 和运行快照发布；相机结果复用 `contracts/camera_operation_result.h`。 |
| `settings_application_service.h/.cpp` | 唯一 `AppSettings current/draft`；整机草稿、模板路径和纸巾阈值都通过同一 Store 保存。 |
| `template_editor_contract.h` | 模板编辑页与应用服务之间的轻量数据。 |
| `template_geometry_service.h/.cpp` | 显示坐标到原图坐标、定位/二维码/日期区域转换。 |
| `template_application_service.h/.cpp` | 单模板编辑草稿、资源准备、路径检查、统一保存和逐模板批量更新。 |

## 维护规则

- 新增一个跨模块用户操作时，先判断是否应成为现有 Service 的一个方法，不新建无状态转发层。
- 模板选择属于设置；模板文件夹内容属于 `TemplateStore`；正式启动属于 `InspectionApplicationService`。
- UI 不直接调用 Runtime、设备或 Store。
- 应用层不弹对话框、不读取控件、不包含 vendor 头文件。
- 多模板坏项提示只显示模板名和操作员原因，完整路径与诊断先写入日志；有效模板至少一个时继续启动。
- 错误结构中的代码、路径和诊断只用于日志，应用服务返回给 UI 的 `userMessage` 只描述结果、影响和操作动作。
- 修改启动顺序后必须回归相机未开、脏设置、PLC 未连、单模板损坏、多模板部分/全部损坏五类失败路径。
