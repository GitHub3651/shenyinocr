# OCRGangYin 许可证到期时间显示授权实施方案

## 1. 文档状态

- 方案日期：2026-09-25。
- 当前状态：阶段 A～D 代码和文档实施完成，静态检查通过，待用户统一验证。
- 权威范围：LicenseTool 签发许可证有效期显示授权、24 位激活码传输、`license.ini` 持久化、主程序启动传递和软件设置页只读呈现。
- 关联基线：激活申请码、设备绑定、有效期、授权模式、默认模式、激活流程和运行期间到期检查继续服从《OCRGangYin 离线激活申请码与激活码实施方案》；许可证有效期显示授权及 `version=5` 许可证格式以本方案为准。
- 当前进度：两位十进制授权字段、七行 `version=5` 许可证、LicenseTool 复选框与只读展示、主程序启动传递和软件设置页只读呈现均已实施；主程序和 LicenseTool 的 Qt Creator 构建及人工交互验证待用户执行。

## 2. 最终目标

LicenseTool 在授权配置区提供以下复选框：

```text
在软件设置中显示许可证有效期
```

复选框状态随激活码传给主程序，并保存在 `license.ini` 中。软件设置页按照许可证授权显示：

- 具体日期许可证：`许可证有效期：yyyy-MM-dd`；
- 长期有效许可证：`许可证有效期：长期有效`；
- 未授权显示：隐藏整行许可证有效期信息。

该值属于许可证授权数据。软件设置页只负责呈现，不提供修改入口；清空软件设置不会改变该值。

## 3. 最终协议

### 3.1 激活申请码

激活申请码保持现有协议：

- 固定为 16 位纯数字；
- 继续使用现有类型前缀、设备摘要、数字变换和 Luhn 校验；
- S1/S2 设备来源、设备摘要算法和设备匹配规则保持不变。

### 3.2 激活码

激活码保持 24 位纯数字，21 位明文主体固定为：

| 字段 | 长度 | 编码 |
|---|---:|---|
| 设备绑定方式 | 1 位 | `1=S1`、`2=S2` |
| 设备摘要 | 12 位 | 来自激活申请码 |
| 有效期 | 5 位 | `00000` 表示长期有效；具体日期沿用现有日期编码 |
| 授权字段 | 2 位 | 两位十进制数 `01`～`63` |
| 默认模式 | 1 位 | `1`～`5` |

外层格式保持：

```text
42 + 21位加密主体 + 1位Luhn校验位
```

### 3.3 两位十进制授权字段

授权字段在激活码中占两个十进制数字位置，字段值内部按二进制位表示各项授权：

| 二进制位 | 数值 | 含义 |
|---:|---:|---|
| 0 | 1 | `stamp` |
| 1 | 2 | `word` |
| 2 | 4 | `ocr` |
| 3 | 8 | `tissue` |
| 4 | 16 | `barcodeWord` |
| 5 | 32 | 在软件设置中显示许可证有效期 |

LicenseTool 生成授权字段：

```cpp
const int authorizationFlags =
        featureMask | (activation.showExpiry ? 32 : 0);
```

主程序和 LicenseTool 解析授权字段：

```cpp
activation->showExpiry = (authorizationFlags & 32) != 0;
const int featureMask = authorizationFlags & 31;
```

授权字段必须满足：

- 数值范围为 `01`～`63`；
- 低五位至少包含一个授权模式；
- 默认模式编号为 `1`～`5`；
- 默认模式包含在低五位模式位图中。

主程序与 LicenseTool 使用相同的字段位置、位定义和模式顺序，并在各自现有协议文件中直接实现。

## 4. `license.ini` 最终格式

许可证固定为七行 UTF-8 文本：

```text
version=5
Encrypt("expires")=Encrypt("yyyy-MM-dd" 或 "permanent")
Encrypt("features")=Encrypt("一个或多个 modeId")
Encrypt("defaultMode")=Encrypt("features 中的一个 modeId")
Encrypt("deviceBinding")=Encrypt("S1" 或 "S2")
Encrypt("deviceCode")=Encrypt("64位大写十六进制设备码")
Encrypt("showExpiry")=Encrypt("true" 或 "false")
```

固定规则：

- 第一行是明文 `version=5`；
- 后续六行的字段名称和值继续使用现有加密算法；
- 字段顺序与上述格式一致；
- `showExpiry` 只使用 `true` 或 `false`；
- 主程序激活成功后使用解析出的授权数据和本机完整设备码生成许可证；
- 主程序和 LicenseTool 按同一七行格式读取许可证。

生产代码只保留这一套许可证格式和解析路径。

## 5. 数据模型与传递路径

在现有数据结构中直接增加：

```cpp
bool showExpiry = false;
```

涉及：

- 主程序 `ActivationCodeData`；
- 主程序 `LicenseData`；
- 主程序 `RuntimeGuardResult`；
- LicenseTool `ActivationCodeData`；
- LicenseTool `LicenseFileData`。

完整传递路径为：

```text
LicenseTool 复选框
  -> 24 位激活码授权字段
  -> LicenseCodec::parseActivationCode()
  -> RuntimeGuard::activate()
  -> version=5 license.ini
  -> RuntimeGuard::check()
  -> RuntimeGuardResult
  -> ApplicationStartup
  -> MainWindow
  -> 软件设置页
```

`showExpiry` 不写入 `AppSettings`。软件设置页不读取许可证文件，也不保存许可证数据。

## 6. LicenseTool 界面与行为

### 6.1 授权配置

在现有授权配置区域增加一个默认勾选的 `QCheckBox`：

```text
在软件设置中显示许可证有效期
```

生成激活码时，直接把复选框状态赋给 `ActivationCodeData::showExpiry`，并编码进两位十进制授权字段。

### 6.2 授权内容读取

读取激活码和 `license.ini` 时，在现有结果中增加：

```text
显示许可证有效期：是
```

或：

```text
显示许可证有效期：否
```

许可证详情中的格式版本显示为 `5`。其他显示内容和失败提示保持不变。

## 7. 主程序行为

### 7.1 激活与启动

`RuntimeGuard::activate()` 负责：

1. 从激活码取得 `showExpiry`；
2. 与有效期、授权模式、默认模式和设备信息一起构造 `LicenseData`；
3. 写入七行 `version=5` 许可证；
4. 把有效许可证数据放入 `RuntimeGuardResult`。

`RuntimeGuard::check()` 负责：

1. 读取七行 `version=5` 许可证；
2. 执行现有设备、有效期、授权模式和默认模式校验；
3. 把 `showExpiry`、`permanent`、`expiresDate`、授权模式和默认模式放入 `RuntimeGuardResult`。

许可证有效期显示授权只控制界面呈现，不参与许可证有效性判断，也不改变启动时和运行期间的到期处理。

### 7.2 主窗口

`ApplicationStartup` 创建 `MainWindow` 时直接传入：

```cpp
license.showExpiry
license.permanent
license.expiresDate
```

`MainWindow` 在软件设置页完成 `setupUi()` 后，直接设置许可证有效期文字和可见性。`MachineSettingsPage` 保持现有设置管理职责。

## 8. 软件设置页

在软件设置页根布局中增加独立的有效期水平布局，放在 `groupBox_softwareData` 外部。独立布局包含两个 `QLabel`：

- `label_licenseExpiry`：显示“许可证有效期：”；
- `label_licenseExpiryValue`：显示具体日期或“长期有效”。

两个标签的文字均加粗。

呈现规则：

| `showExpiry` | 有效期类型 | 页面结果 |
|---|---|---|
| `false` | 任意 | 隐藏独立布局中的两个标签，页面不保留空白行 |
| `true` | 长期有效 | 显示“长期有效” |
| `true` | 具体日期 | 显示 `yyyy-MM-dd` |

两个标签使用现有统一 QSS。页面不增加许可证编辑控件、保存操作或定时刷新。

## 9. 文件范围

### 9.1 LicenseTool

| 文件 | 修改内容 |
|---|---|
| `tools/license_tool/widget.ui` | 在授权配置区增加显示有效期复选框 |
| `tools/license_tool/widget.cpp` | 生成时读取复选框；查看激活码和许可证时显示授权值及版本 5 |
| `tools/license_tool/activation_protocol.h` | `ActivationCodeData` 和 `LicenseFileData` 增加 `showExpiry` |
| `tools/license_tool/activation_protocol.cpp` | 编解码授权字段；读取七行 `version=5` 许可证 |

### 9.2 主程序

| 文件 | 修改内容 |
|---|---|
| `app/system_support/license/license_codec.h` | `ActivationCodeData` 和 `LicenseData` 增加 `showExpiry` |
| `app/system_support/license/license_codec.cpp` | 解析授权字段；读写七行 `version=5` 许可证 |
| `app/startup/runtime_guard.h` | `RuntimeGuardResult` 增加 `showExpiry` |
| `app/startup/runtime_guard.cpp` | 激活、保存和有效结果传递 `showExpiry` |
| `app/startup/application_startup.cpp` | 创建主窗口时传入显示授权和有效期 |
| `app/ui/main_window/main_window.h` | 构造函数增加显示授权和有效期参数 |
| `app/ui/main_window/main_window.cpp` | 初始化许可证有效期文字和可见性 |
| `app/ui/main_window/settings/software_settings_page.ui` | 在软件数据分组外增加独立的许可证有效期布局，并加粗标题和值标签 |

### 9.3 维护文档

实施完成时同步更新：

- `docs/development/OCRGangYin离线激活申请码与激活码实施方案.md`；
- `docs/development/OCRGangYin开发者代码结构与维护指南.md`；
- `docs/development/OCRGangYin现有功能对照表.md`；
- `docs/development/OCRGangYin重构执行记录.md`；
- `docs/development/OCRGangYin计划索引.md`。

维护文档统一描述本方案定义的 24 位激活码、七行 `version=5` 许可证和软件设置页呈现规则。

## 10. 实施顺序

### 阶段 A：协议与许可证

1. 在主程序和 LicenseTool 的现有数据结构中增加 `showExpiry`。
2. 在两端现有协议文件中同步实现授权字段编解码。
3. 将许可证读写统一为七行 `version=5`。
4. 清理与最终协议无关的许可证格式代码和未使用符号。

### 阶段 B：LicenseTool

1. 增加复选框并接入激活码生成。
2. 在激活码和许可证读取结果中显示授权值。

### 阶段 C：主程序界面

1. 通过 `RuntimeGuardResult` 和现有启动链传递显示授权与有效期。
2. 在软件设置页的软件数据分组外增加独立的只读有效期布局，并加粗标题和值。
3. 由 `MainWindow` 直接设置文字和可见性。

### 阶段 D：收口

1. 同步维护文档中的协议、文件格式和界面行为。
2. 执行符号搜索、UI XML 检查和 `git diff --check`。
3. 用户在 Qt Creator 中完成主程序与 LicenseTool 构建和人工验证。

## 11. 验收清单

### 11.1 LicenseTool

- 复选框默认勾选。
- 勾选和取消勾选时生成的激活码均为 24 位纯数字并通过 Luhn 校验。
- 读取激活码能正确显示授权值。
- 读取 `version=5` 许可证能正确显示授权值。
- 授权模式和默认模式能从授权字段低五位正确解析。

### 11.2 主程序

- 激活时正确解析 `showExpiry`。
- 激活成功后生成固定七行 `version=5` 许可证。
- 重新启动后恢复相同的显示授权、有效期、授权模式和默认模式。
- 设备绑定、到期判断和运行期间到期检查保持有效。

### 11.3 软件设置页

- `showExpiry=false` 时许可证有效期整行隐藏且不保留空白。
- `showExpiry=true` 且为具体日期时显示正确日期。
- `showExpiry=true` 且为长期有效时显示“长期有效”。
- 有效期标题和值均以加粗文字显示。
- 页面不提供修改许可证显示授权的入口。
- 清空软件设置后显示结果不变。

### 11.4 代码与结构

- 主程序与 LicenseTool 的授权字段位置、位定义和模式顺序一致。
- 生产代码只有一套许可证版本、字段顺序和解析路径。
- 本功能直接复用现有数据结构、启动链和软件设置页，不新增协议包装类、适配器、共享库、全局状态或设置项。
- 与最终协议无关的许可证代码和未使用符号已清理。
- `.ui` 文件可按 XML 解析。
- 声明、定义和构造调用参数一致。
- `git diff --check` 通过。

## 12. 最终状态

```text
16位激活申请码
  -> LicenseTool 选择有效期、授权模式、默认模式和显示授权
  -> 24位激活码
  -> 主程序验证并生成七行 version=5 license.ini
  -> 启动时读取许可证
  -> 软件设置页按许可证授权显示或隐藏有效期
```

项目使用这一套许可证协议和直接数据链。显示授权只控制软件设置页的有效期呈现，许可证有效期判定始终生效。
