# system_support：系统支撑

本目录提供设置持久化、日志、授权和崩溃诊断，不实现检测业务。

## 文件树

```text
system_support/
├─ settings/
│  ├─ app_settings.h/.cpp               AppSettings 与五模式 DetectionSchemes
│  └─ app_settings_store.h/.cpp         Schema 8 严格 JSON + QSaveFile
├─ license/license_codec.h/.cpp
├─ logging/application_logger.h/.cpp
└─ crash/
   ├─ windows_crash_handler.h/.cpp
   └─ windows_crash_stack.h/.cpp
```

## 设置边界

```text
UI 草稿
 → SettingsApplicationService
 → AppSettingsStore
 → %APPDATA%/ShengYin/settings/app_settings.json
```

`AppSettingsStore` 无长期内存副本、无逐字段 setter，也不读取模板文件夹。整机设置、检测方案和纸巾阈值共用一个完整 JSON 原子提交入口。

二维码+三期的本机 CSV 开关与绝对输出目录保存在 `barcodeCsv.enabled/outputDirectory`；左侧页面抽屉的 `QSplitter` 状态保存在 `ui.leftDrawerSplitterStateBase64`。目录准备和结果写入不属于 Store。

## 维护规则

- Schema 变化必须同步数据 Schema 文档和旧版本处理策略。
- 当前 Schema 损坏时拒绝覆盖；旧 Schema 只有用户确认后整体重置。
- 模板私有参数不写进全局设置；全局设置只记录模板绝对路径。
- 不为 JSON 每个分区建立独立 Store、Repository 或 Service。
