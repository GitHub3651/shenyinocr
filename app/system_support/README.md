# system_support：设置、日志、授权、崩溃诊断与部署支持

## 一句话理解

`system_support` 放的是不属于某一种检测算法、但整台软件运行所必需的基础设施：整机设置持久化、全局日志、授权文件、Windows 崩溃栈和 Release 部署脚本。

## 当前结构

```text
system_support/
├─ machine_settings_policy.h
├─ machine_settings_policy.cpp
├─ settings/
│  ├─ machine_settings.h
│  ├─ machine_settings.cpp
│  ├─ machine_settings_store.h
│  └─ machine_settings_store.cpp
├─ logging/
│  ├─ application_logger.h
│  └─ application_logger.cpp
├─ license/
│  ├─ license_codec.h
│  └─ license_codec.cpp
├─ crash/
│  ├─ windows_crash_handler.h
│  ├─ windows_crash_handler.cpp
│  ├─ windows_crash_stack.h
│  └─ windows_crash_stack.cpp
├─ deployment/
│  └─ deploy_runtime.ps1
└─ README.md
```

这些子目录彼此不是一条业务调用链，而是五类独立的系统能力；共同点是它们都由 Startup 或 Application 使用，不应侵入 Detection。

## settings：整机设置

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `settings/machine_settings.h` | 整机设置 Schema：模式 ID、存图、曝光/增益、旋转/通道、硬触发延时、PLC 连接/工艺/地址、模式配方记忆和 Splitter 状态。 | 只放随整机/产线变化的数据。 |
| `settings/machine_settings.cpp` | 默认值、相等比较、枚举 ID 列表；检测模式 ID 来自唯一 Descriptor。 | 不在 UI 复制默认值；`cameraDelay` 当前只表示硬触发延时 ms。 |
| `settings/machine_settings_store.h` | 定义加载状态、错误和 Store 接口，以及应用数据/recipes/editor-workspaces 路径。 | 设置 JSON 唯一读写入口。 |
| `settings/machine_settings_store.cpp` | 校验 Schema 1、类型/范围/枚举/交叉约束，Base64 保存 Splitter，事务写 `app_settings.json`，支持默认和清空。 | 不提供旧 INI 双读；`clear` 不删除已发布配方。 |
| `machine_settings_policy.h/.cpp` | 根据相机/PLC 可用状态生成“恢复默认”时可应用的整机设置。 | 只调整硬件相关默认，不持久化、不操作控件。 |

### 设置调用链

```text
MachineSettingsPage
→ SettingsApplicationService 草稿
→ MachineSettingsStore 校验并事务保存
→ 已应用 MachineSettings
→ 下一次开始检测时复制进冻结运行上下文
```

整机设置与 ProductRecipe 不可混用。目标文字、ROI、模板和产品阈值属于配方，不应写入这里。

## logging：全局日志

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `logging/application_logger.h` | 声明日志安装、崩溃信息追加、日志目录和关闭。 | 只由 Startup 安装/关闭一次。 |
| `logging/application_logger.cpp` | 安装 `qInstallMessageHandler`，写时间/级别/文件行，创建日志目录并清理三个月前日志；Fatal 保持终止语义。 | 回调可由多线程进入，必须避免递归 Qt 日志和生命周期竞态。 |

日志系统只记录事实，不应吞掉 Fault、替代错误返回或自动恢复生产。

## license：授权文件格式

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `license/license_codec.h` | 定义授权文件错误、读取结果和统一 Codec。 | 主程序与 LicenseTool 必须共享同一格式和日期语义。 |
| `license/license_codec.cpp` | 固定 XOR/SHA256 保护、Base64、`expires` 解析和文件读写。 | 格式变化会使已有 license 失效，属于部署决策。 |

Startup 的 `RuntimeGuard` 负责“何时检查、失败如何提示”，Codec 只负责“文件如何编码/解码”。

## crash：Windows 崩溃诊断

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `crash/windows_crash_handler.h/.cpp` | 安装未处理异常过滤器，捕获异常并把诊断追加到应用日志，然后保持进程终止。 | 不能吞掉异常继续生产。 |
| `crash/windows_crash_stack.h/.cpp` | 使用 DbgHelp/Windows API 解析异常地址、模块、调用栈和系统版本，形成诊断文本。 | 崩溃路径尽量少做复杂分配或业务调用。 |

崩溃处理的目标是留下证据，不是让已经失去可信状态的程序继续运行。

## deployment：Release 部署

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `deployment/deploy_runtime.ps1` | Release 链接后的部署：验证路径、复制非 Qt 资产、保留当前 exe、调用当前 Kit 的 `windeployqt`，检查 DLL、配置、V3 模型和 Qt 插件。 | 只由 qmake Release 后处理调用；新增运行依赖要同步 `requiredFiles`。 |

部署脚本不决定业务功能，只确保“已链接成功的程序在 dist 中具备运行依赖”。

## 允许放什么

- 进程级、平台级和持久化基础设施。
- 与整机设置、授权、日志、崩溃和部署直接相关的稳定代码。
- 明确的错误结果、路径规则和事务写入。

## 禁止放什么

- 五种检测算法、产品配方和模板资源。
- 相机采集、PLC 结果时序和 Runtime 状态机。
- QWidget、弹窗和页面状态。
- 模糊的 `utils` 杂物或业务对象的全局单例。

## 新增整机设置字段

必须同步：

1. `MachineSettings` 字段和唯一默认值。
2. 相等比较与枚举 ID（如适用）。
3. Store JSON 写出、读入和校验。
4. SettingsApplicationService 草稿/应用路径。
5. MachineSettingsPage 控件、dirty 名和验证。
6. 真正运行消费者。
7. 数据 Schema、首次启动、保存重启、损坏拒绝、恢复默认/清空回归。

## 平台与供应链注意事项

- Crash 子目录是 Windows 专用实现。
- License 格式和密钥变更必须同时考虑现有部署与 LicenseTool。
- Deploy 脚本必须跟当前 Qt Kit、V3 OCR 模型和第三方 DLL 一致。
- 第三方版本和指纹记录在 `third_party/DEPENDENCIES.md`，不能只更新脚本不更新清单。

新增、删除或移动本目录代码/脚本时，请同步更新本 README、qmake 工程清单、第三方依赖清单和开发者维护指南。
