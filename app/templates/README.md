# templates 目录

本目录是外部模板文件夹的唯一代码边界，只包含两个生产代码文件：

- `template_store.h`：集中定义模板参数、可编辑模板、运行模板、摘要和错误。
- `template_store.cpp`：读取 `template_settings.json` 和固定资源，执行运行校验，并用同一套目录事务完成新建、更新和确认覆盖。

一个外部文件夹就是一个模板。这里不保存“当前选择”，不扫描全盘，不维护模板 UUID 或模板索引，也不提供删除外部模板的接口。当前选择由 `AppSettings::detectionSchemes` 保存。

调用关系：

```text
模板选择 UI ──readSummary──> TemplateStore
模板编辑服务 ──loadEditable/save──> TemplateStore
检测启动服务 ──loadPrepared──> TemplateStore ──> PreparedTemplate
```

纸巾检测不经过本目录，其阈值直接保存在 `app_settings.json`。
