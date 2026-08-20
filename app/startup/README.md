# startup：进程初始化和对象装配

本目录只负责进程级准备和创建唯一对象图。

## 文件

| 文件 | 职责 |
|---|---|
| `main.cpp` | 唯一程序入口。 |
| `application_startup.h/.cpp` | 应用名、翻译、日志、设置加载、旧 Schema 重置提示、设备/引擎/Store/Service/页面装配和事件循环。 |
| `runtime_guard.h/.cpp` | Windows 运行库和必要 DLL 前置检查。 |
| `single_instance_guard.h/.cpp` | 单实例保护。 |

## 对象图

```text
AppSettingsStore ─→ SettingsApplicationService
TemplateStore ────┬→ TemplateApplicationService
                  └→ InspectionApplicationService
Camera + PLC + DetectionRegistry → InspectionRuntime
上述 Service → MainWindow + 三个页面对象
```

旧版设置只在这里触发一次明确重置确认；损坏的当前 Schema 不自动覆盖。

## 维护规则

- 这里只做创建、注入、生命周期和启动错误展示，不放业务判断。
- `TemplateStore` 只创建一个共享实例。
- 不在启动层扫描模板目录或迁移旧数据。
- 新依赖必须先确定所有者，再在这里装配，不能在页面中临时 new 底层服务。
