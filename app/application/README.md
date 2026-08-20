# application：应用用例与跨模块编排

## 一句话理解

`application` 是 UI 与底层运行系统之间的唯一业务入口。它回答“这个操作现在能不能做、需要哪些数据、按什么顺序调用底层、失败如何告诉 UI”，但不实现图像算法、设备 SDK、PLC 字节编码或文件事务细节。

## 它在整体架构中的位置

```text
ui
  ↓ 结构化命令、DTO、回调
application
  ├─→ runtime          正式运行、相机会话、PLC、统计、存图
  ├─→ recipes          配方草稿、发布、加载运行快照
  └─→ system_support   整机设置的草稿和持久化
```

上层 UI 不应越过本目录直接操作 Runtime、Camera、PLC 或 RecipeStore。本目录也不能把底层实现细节反向暴露给 UI。

## 当前文件树

```text
application/
├─ application_result.h
├─ camera_application_contract.h
├─ inspection_application_service.h
├─ inspection_application_service.cpp
├─ inspection_start_preflight.h
├─ inspection_start_preflight.cpp
├─ inspection_ui_contract.h
├─ runtime_snapshot.h
├─ settings_application_service.h
├─ settings_application_service.cpp
├─ template_application_service.h
├─ template_application_service.cpp
├─ template_editor_contract.h
├─ template_geometry_service.h
├─ template_geometry_service.cpp
└─ README.md
```

这 15 个代码文件可折叠为 4 组职责，而不是 15 个独立系统：

1. 检测应用服务：`inspection_application_service.*`、`inspection_start_preflight.*` 及相关合同。
2. 设置应用服务：`settings_application_service.*`。
3. 模板应用服务：`template_application_service.*`、`template_editor_contract.h`。
4. 模板坐标换算：`template_geometry_service.*`。

## 逐文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `application_result.h` | 通用应用层返回值，包含是否成功、稳定错误码、用户提示和诊断信息。 | Application 返回结构化结果，不能直接弹 `QMessageBox`。 |
| `camera_application_contract.h` | 相机打开、参数范围、软硬触发、停止恢复等面向 UI 的 DTO。 | 不能暴露海康句柄、SDK 结构体或采集线程对象。 |
| `inspection_ui_contract.h` | 检测结果视图绑定、UI 回调、判定样式和 Fault 快照。 | 只放跨 Application/UI 的轻量数据与回调。 |
| `runtime_snapshot.h` | UI 可观察的运行快照：运行状态、相机、PLC、采集和 Fault 摘要。 | 它是只读投影，不是第二套运行状态机。 |
| `inspection_start_preflight.h` | 声明开始检测前的输入、问题类型和检查结果。 | 所有启动门禁应成为明确问题，而不是散落布尔判断。 |
| `inspection_start_preflight.cpp` | 按稳定顺序检查忙碌、相机、设置、PLC、配方、目标字符、模板、Profile 和引擎条件。 | 检查顺序决定用户首先看到哪个错误，修改后要回归五模式失败路径。 |
| `inspection_application_service.h` | 正式检测、停止、相机、PLC、模板预览、统计、Fault 恢复等运行用例的公开边界。 | 所有改变设备或运行状态的公开命令都必须返回结构化结果。 |
| `inspection_application_service.cpp` | 编排启动事务、冻结运行快照、相机准备、软硬触发、停止/Fault、PLC/相机命令和 UI 发布。 | 只编排；不写检测算法，不直接拼 PLC 字节，不复制设备 SDK 实现。 |
| `settings_application_service.h` | 声明整机设置的当前值、草稿、应用、放弃、默认和清空用例。 | UI 不应直接使用 `MachineSettingsStore`。 |
| `settings_application_service.cpp` | 维护“已应用值”和“待编辑草稿”，调用 Store 做事务保存并转换错误。 | 正式运行只读取已应用值，不能把未应用草稿混入运行。 |
| `template_editor_contract.h` | 模板编辑跨层 DTO：配方目录项、损坏项、字库 Profile、条码校验参数等。 | 只放数据合同，不放对话框、Store 或检测算法。 |
| `template_application_service.h` | 声明新建、编辑、取消、发布、激活、资源暂存、即时条码校验和模式配方记忆等用例。 | UI 访问配方和编辑会话的唯一入口。 |
| `template_application_service.cpp` | 协调 `RecipeEditorSession`、`RecipeStore`、几何服务和条码引擎，维护当前编辑/激活状态。 | 即时模板校验不形成正式产品结果；正式算法仍在 Detection。 |
| `template_geometry_service.h` | 声明显示几何、Profile 几何以及 UI 坐标到原图坐标的转换。 | 所有模板坐标换算应经过这里。 |
| `template_geometry_service.cpp` | 实现等比缩放留边、偏移、裁边、矩形/多边形 ROI 映射。 | 取整和缩放规则会影响已保存 ROI，修改必须做边缘与宽高比回归。 |

## 三条真实工作流

### 1. 开始正式检测

```text
MainWindow
→ InspectionApplicationService::start
→ InspectionStartPreflight
→ SettingsApplicationService 当前已应用设置
→ TemplateApplicationService / RecipeStore 准备只读配方
→ InspectionRuntime::beginStart
→ CameraSession::prepareInspection / startInspection
```

Application 负责把“一个 UI 点击”变成有顺序、可失败、可回滚的完整用例。

### 2. 保存整机设置

```text
MachineSettingsPage 收集控件值
→ SettingsApplicationService 更新草稿
→ MachineSettingsStore 校验并事务保存
→ 草稿成为已应用值
```

### 3. 制作并发布模板

```text
TemplateEditorPage
→ TemplateApplicationService
→ RecipeEditorSession 暂存草稿和资源
→ RecipeStore 事务发布
→ loadPreparedRecipe 重新加载验证
→ 激活只读 PreparedRecipeSnapshot
```

## 状态和所有权

- `InspectionRuntime` 是生产运行状态唯一真源；Application 只读取快照并执行用例门禁。
- `SettingsApplicationService` 拥有设置的“当前值/草稿”语义，但实际 JSON 读写由 Store 完成。
- `TemplateApplicationService` 协调编辑会话；正式配方目录唯一写入口仍是 `RecipeStore`。
- Application 服务由 `startup/ApplicationStartup` 构造，再注入 MainWindow 和页面。

## 允许放什么

- 用户用例入口和跨模块调用顺序。
- UI 与底层之间的稳定 DTO、错误码和只读快照。
- 启动前置条件、事务编排、失败映射和补偿动作。
- 不属于算法的坐标转换等应用级服务。

## 禁止放什么

- OpenCV/Paddle/字符匹配等正式检测算法。
- 海康、Snap7、Barcode DLL 等供应商 API。
- `QWidget`、对话框和控件使能代码。
- PLC 地址编码、结果脉冲时序和存图线程实现。
- 绕过 Store 的 JSON/配方目录直接读写。

## 常见维护入口

| 需求 | 首先修改 | 同步检查 |
|---|---|---|
| 新增一个运行命令 | `inspection_application_service.*` | Runtime/Session 是否已有底层不变量；UI 如何呈现结构化结果 |
| 新增开始检测条件 | `inspection_start_preflight.*` | 错误码、提示、五模式失败路径 |
| 新增整机设置用例 | `settings_application_service.*` | `MachineSettings`、Store、UI dirty 状态 |
| 新增模板编辑操作 | `template_application_service.*` | EditorSession、Store 事务、TemplateEditorPage |
| 修改 ROI 换算 | `template_geometry_service.*` | 已发布模板兼容性与边界样本 |

## 阅读顺序

想理解正式检测，依次阅读：

```text
inspection_application_service.h
→ inspection_start_preflight.cpp
→ inspection_application_service.cpp
→ ../runtime/inspection_runtime.h
```

想理解模板，依次阅读：

```text
template_editor_contract.h
→ template_application_service.h/.cpp
→ ../recipes/recipe_editor_session.*
→ ../recipes/recipe_store.*
```

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单和开发者维护指南。
