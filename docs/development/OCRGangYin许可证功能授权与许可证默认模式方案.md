# OCRGangYin 许可证功能授权与许可证默认模式方案

## 1. 文档状态

- 方案日期：2026-09-20。
- 当前状态：历史实现参考；当前许可证目标状态与实施以 `OCRGangYin离线激活申请码与激活码实施方案.md` 为唯一依据。
- 参考范围：追溯当前 v3 许可证代码中的功能授权、有效期、长期有效状态、默认检测模式、设备绑定，以及 `LicenseTool` 的生成和读取。
- 不在范围内：在线授权服务器、账号系统、硬件加密狗、检测算法、模板 Schema、相机、PLC 和具体检测业务规则。

## 2. 目标

程序保留现有各检测模式的代码、配置、模板与资源，许可证只决定当前授权可用的模式集合和默认模式。

许可证需要同时表达：

1. 本许可证授权哪些检测模式；
2. 许可证是按日期到期还是长期有效；
3. 程序首次启动或保存模式不可用时使用哪个默认检测模式；
4. 许可证是否已经绑定当前设备。

许可证的逻辑字段结构为：

```text
version=3
expires=<yyyy-MM-dd 或 permanent>
features=<授权 modeId 列表>
defaultMode=<授权列表中的一个 modeId>
deviceBinding=UNACTIVATED
deviceCode=UNACTIVATED
```

上面是逻辑字段示例。实际许可证文件中只有 `version=3` 使用明文；`expires`、`features`、`defaultMode`、`deviceBinding`、`deviceCode` 的字段名和值分别独立加密。

## 3. 许可证文件格式

### 3.1 外层格式

主程序固定从 `QDir(QCoreApplication::applicationDirPath()).filePath("license.ini")` 读取许可证；`LicenseTool` 也按同一表达式，以工具自身的应用程序目录作为默认生成和读取位置。文件第一行固定为明文 `version=3`；除此之外不保留可读字段名称，不出现 `data`、`expires`、`features`、`defaultMode`、`deviceCode` 或 `UNACTIVATED` 等明文。

第一行之后直接写入五行加密后的字段名和字段值，文件形态如下：

```ini
version=3
加密后的 expires 字段名=加密后的字段值
加密后的 features 字段名=加密后的字段值
加密后的 defaultMode 字段名=加密后的字段值
加密后的 deviceBinding 字段名=加密后的字段值
加密后的 deviceCode 字段名=加密后的字段值
```

`LicenseTool` 按 `version`、`expires`、`features`、`defaultMode`、`deviceBinding`、`deviceCode` 的固定顺序生成文件。除第一行 `version=3` 外，字段名和字段值均生成为大写十六进制文本，并使用 `=` 和换行组成键值结构。

### 3.2 独立字段加密

不再把所有信息拼成一个 `data` 密文。`version=3` 作为明文格式入口，不参与加密；以下五个字段各自独立加密：

| 逻辑字段 | 加密内容 | 备注 |
|---|---|---|
| `expires` | `yyyy-MM-dd` 或 `permanent` | 指定到期日或长期有效；两者共用一个字段 |
| `features` | 逗号分隔的稳定 `modeId` | 授权功能集合 |
| `defaultMode` | 稳定 `modeId` | 必须属于 `features` |
| `deviceBinding` | `UNACTIVATED`、`S1` 或 `S2` | 设备指纹方案；未激活时为 `UNACTIVATED` |
| `deviceCode` | `UNACTIVATED` 或实际设备码 | 设备绑定状态 |

每一行都执行“字段名独立加密 + 字段值独立加密”，不能只加密值而保留明文键名，也不能把多项重新合并为一个总密文。

具体编码方式沿用当前许可证的轻量实现：

1. 使用现有固定密钥派生 SHA-256 密钥；
2. 对每个 UTF-8 字段名和字段值分别执行循环异或；
3. 对每个结果单独编码为大写十六进制；
4. 读取时使用 `QByteArray::fromHex()` 直接解码对应文本，再执行循环异或和 UTF-8 文本转换；
5. 按解密后的字段名建立逻辑字段表。

读取时先检查第一行是否为 `version=3`，缺少或值不一致时许可证无效，不继续解析后续内容。版本通过后逐行读取后续 `字段名=字段值`，按上述路径直接解密并建立逻辑字段表；任一必需字段无法取得或字段值不符合对应业务规则时许可证无效。未识别的额外字段不参与许可证含义。

### 3.3 `expires` 有效期规则

- `expires=yyyy-MM-dd`：当前日期大于该日期时许可证失效。
- `expires=permanent`：表示长期有效，不执行日期比较。长期有效是 `expires` 字段支持的一种有效日期状态，不新增第二个有效期字段。
- 缺少 `expires`、解密结果为未知值或日期格式错误时，许可证无效。
- 长期有效只表示不受日期限制，不表示拥有全部功能；仍必须通过 `features` 校验。

有效期检查与设备绑定检查分开处理：所有许可证启动时都必须完成一次完整的版本、字段、功能、默认模式和设备绑定校验。启动成功后不再读取或校验许可证；只有 `expires` 为具体日期时，保存启动阶段已确认的到期日期并启动固定间隔为 24 小时的定时器。定时器触发时只比较 `QDate::currentDate()` 是否大于该日期：未超过时不执行操作；超过时显示现有运行期提示“程序出错，即将退出，请联系供应商。”，用户确认后调用 `QCoreApplication::quit()`。定时器不重新读取许可证，不重新解密字段，也不检查功能授权或设备绑定。`expires=permanent` 时不创建有效期定时器，也不执行日期比较；长期许可证不因日期变化退出。

### 3.4 功能和默认模式规则

- `features` 只能包含现有稳定模式 ID：`stamp`、`word`、`ocr`、`tissue`、`barcodeWord`。
- `features` 至少包含一个有效模式。
- `defaultMode` 必须是有效模式 ID，且必须出现在 `features` 中。
- 许可证保存稳定 `modeId`，不保存界面显示名称，也不保存可能变化的 `uiId`。
- 读取后通过现有 `detectionModeFromId()` 转换为 `DetectionMode`，再通过 `detectionModeUiId()` 写入当前 `AppSettings` 使用的模式字段。

`detectionModeDescriptors()` 是许可证功能中模式定义和模式顺序的唯一来源，固定顺序为 `stamp`、`word`、`ocr`、`tissue`、`barcodeWord`。`features` 只表达授权成员集合，不提供另一套顺序。`LicenseTool` 的功能项和默认模式列表、`features` 的写入顺序以及主程序授权模式下拉框都按 `detectionModeDescriptors()` 遍历，再按授权集合过滤；`features` 写入时使用英文逗号连接且不添加空格。不得维护独立模式数组、手工顺序表或按许可证文本出现顺序排列界面选项。

### 3.5 设备绑定字段

- `deviceBinding=UNACTIVATED` 与 `deviceCode=UNACTIVATED` 表示许可证尚未绑定设备；这些文字只存在于解密后的逻辑值，不出现在文件中。
- 首次绑定只能按 `S1 -> S2` 选择设备绑定方案：优先使用有效的 SMBIOS `System UUID`；只有 S1 无效时才尝试主板序列号与 BIOS 序列号组合。
- 绑定成功后必须同时写入 `deviceBinding` 和 `deviceCode`；其他加密字段保持原逻辑值和授权含义不变。
- 已绑定许可证必须严格按照许可证中的 `deviceBinding` 重新计算当前设备码，并与 `deviceCode` 完全一致；不一致时禁止启动。
- 后续启动禁止从 S1 自动切换到 S2，也禁止从 S2 自动切换到 S1。
- 设备码不写入 `app_settings.json`，不写入模板，不写入日志，也不显示在主程序界面。

设备标识只通过一个 Windows WMI 读取实现取得：`Win32_ComputerSystemProduct.UUID`、`Win32_BaseBoard.SerialNumber` 和 `Win32_BIOS.SerialNumber`。不再增加其他硬件来源或多级回退。

每个原始标识都按同一规则规范化：去除首尾空白，转为大写，去除空格、连字符和 UUID 外层花括号；占位值也按相同规则规范化后再比较。规范化结果为空、全为 `0`、全为 `F`，或等于 `TOBEFILLEDBYOEM`、`DEFAULTSTRING`、`UNKNOWN`、`NONE`、`NOTSPECIFIED` 时视为无效。S1 使用规范化后的 System UUID，S2 使用规范化后的主板序列号和 BIOS 序列号；设备码为拼接输入的 SHA-256 大写十六进制结果。

## 4. 启动模式优先级

许可证必须在读取软件设置前完成解析和设备绑定。启动流程为：

```text
从应用程序目录读取 license.ini
  -> 校验明文 version=3
  -> 解密 expires、features、defaultMode、deviceBinding 和 deviceCode
  -> 按业务规则校验五个必需字段
  -> deviceBinding/deviceCode=UNACTIVATED 时按 S1 -> S2 读取本机标识并生成设备码
  -> 同时写入 deviceBinding 和 deviceCode
  -> 原子写回并复读校验设备绑定
  -> 读取 app_settings.json
  -> 首次启动或已保存模式未获授权时，在内存中改用许可证 defaultMode
  -> 计算本次启动的有效检测模式
  -> 组装设置服务、模板服务、检测服务和主窗口
  -> 仅当 expires 为具体日期时启动 24 小时有效期定时器，过期时显示现有提示并退出
```

许可证的版本、字段、功能、默认模式和设备绑定只在启动阶段校验。启动成功后，授权模式集合用于初始化本次运行的模式界面，不再执行许可证读取、字段解密、功能授权或设备绑定校验；具体日期许可证的定时器只比较启动阶段已确认的到期日期，并在过期时显示现有运行期提示后调用 `QCoreApplication::quit()`。

### 4.1 设备码绑定

- LicenseTool 生成的新许可证将 `deviceBinding` 和 `deviceCode` 的逻辑值都设为 `UNACTIVATED`，但两个值在文件中必须分别独立加密。
- 首次启动发现未激活时，先读取有效的 SMBIOS `System UUID`。S1 有效时使用 `deviceBinding=S1`，并计算 `SHA256(NormalizedSystemUUID)`。
- S1 无效时读取 Baseboard Serial 和 BIOS Serial。两者都有效时使用 `deviceBinding=S2`，并计算 `SHA256(NormalizedBaseboardSerial + "|" + NormalizedBiosSerial)`。
- S1 无效且 S2 信息不完整时，设备码生成失败，禁止激活和启动。
- `System UUID`、主板序列号和 BIOS 序列号必须按 3.5 的统一规则规范化，并排除无效值。
- 首次绑定成功后，同时更新 `deviceBinding` 和 `deviceCode` 两个独立加密字段；有效期、功能和默认模式字段不重新解释、不丢失。
- 写回使用现有 `QSaveFile` 原子替换；写入完成后重新读取许可证，并严格按照刚写入的 `deviceBinding` 复算设备码进行校验，确认前不得继续组装主窗口。
- 已绑定许可证只使用其记录的绑定方式：S1 只读取 System UUID；S2 只读取 Baseboard Serial + BIOS Serial。计算结果必须与许可证 `deviceCode` 完全一致。
- 设备码生成失败、许可证目录不可写、原子写回失败、复读失败或设备码不匹配，均禁止启动。
- 设备码只用于稳定区分绝大多数设备，不宣称数学意义上的绝对唯一；不使用产品盐、MachineGuid、MAC 地址或系统盘卷序列号。
- `deviceBinding` 和 `deviceCode` 必须同时处于未激活状态或同时处于已绑定状态；一方未激活而另一方已绑定时，许可证无效。

主程序对上述所有失败继续使用现有的通用初始化失败提示，不显示设备码、许可证字段或具体失败原因。

模式优先级如下：

| 场景 | 本次启动模式 | 是否删除或清空其他模式数据 |
|---|---|---|
| 首次启动 | 许可证 `defaultMode` | 否 |
| 已保存模式且该模式已授权 | 已保存模式 | 否 |
| 已保存模式未授权 | 许可证 `defaultMode` | 否 |
| 许可证默认模式未授权或不存在 | 阻止启动 | 否 |
| 许可证缺失、损坏或失效 | 阻止启动 | 否 |

当前 `AppSettings::defaults()` 中的字库匹配只作为结构层的通用默认值保留。生产启动时，首次启动或已保存模式未获授权时，在内存中的 `startupSettings.detectModeId` 使用许可证 `defaultMode` 对应的 UI ID；不修改 Schema，不删除其他模式数据，也不强制覆盖已保存且已授权的模式。

许可证默认模式仅用于首次启动、设置缺失和已保存模式未获授权的情况；已保存且获授权的模式不被覆盖。

## 5. 设置与模板数据保持规则

- 不修改现有 `AppSettings` Schema 结构。
- 不删除未授权模式的模板目录、模板路径和模式私有参数。
- 不因为许可证变化删除 `app_settings.json` 中其他模式的配置。
- 已保存模式未授权时，只把本次启动使用的 `detectModeId` 设为许可证默认模式；其他模式数据继续保留。
- 用户切换到已授权模式后，继续沿用现有设置保存链。
- 许可证升级后，原来保留的模式配置和模板可以重新使用。

## 6. 界面限制

`MachineSettingsPage` 的模式下拉框遍历 `detectionModeDescriptors()`，只加入许可证 `features` 中的模式，因此下拉框顺序只由现有模式描述符定义。未授权模式不显示为可选项，也不增加假的锁定入口；不得使用 `features` 文本顺序、独立列表或固定索引形成另一套顺序。

模式对应的参数、模板制作、模板选择、模板保存和字符分割入口继续跟随当前有效模式显示。

## 7. LicenseTool 改动

`LicenseTool` 增加以下生成项：

1. `expires`：在具体日期和长期有效之间二选一；具体日期直接写入 `yyyy-MM-dd`，长期有效写入 `permanent`；
2. 授权功能：五种检测模式的多选项；
3. 默认模式：从已勾选的授权功能中选择；
4. 设备绑定状态：只读显示“未激活”，不允许手工输入绑定方式或设备码。

授权功能项和默认模式下拉框均直接使用 `detectionModeDescriptors()` 的模式定义和顺序；默认模式下拉框再按当前勾选的授权集合过滤，不维护第二套模式列表或排序。

生成前必须校验：

- 至少选择一个功能；
- 默认模式已选择；
- 默认模式属于授权功能；
- 选择具体日期时日期必须有效；选择长期有效时写入 `expires=permanent`，不新增其他有效期字段。
- 设备绑定和设备码初始逻辑值均固定为 `UNACTIVATED`，并按独立字段分别加密写入。

查看许可证时，`LicenseTool` 解密并显示 `expires` 对应的具体日期或长期有效状态、授权功能、默认模式、设备绑定方式和设备绑定状态；这些信息只在工具界面显示，不回写为明文许可证字段。

## 8. 代码文件边界

预计修改范围：

- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\system_support\license\license_codec.h/.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\startup\runtime_guard.h/.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\startup\application_startup.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\AutoOCRproject.pro`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\ui\main_window\main_window.h/.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\ui\main_window\settings\machine_settings_page.h/.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\app\ui\main_window\template\template_editor_page.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\tools\license_tool\widget.h/.cpp`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\tools\license_tool\widget.ui`
- `D:\BaiduNetdiskDownload\ocr20260407\ocrgangyin\tools\license_tool\LicenseTool.pro`

本次实施仅完成本方案定义的许可证格式、启动校验、设备绑定、默认模式和授权模式界面限制，以及使这些改动能够编译运行的直接配套修改。优先在现有文件、类型、函数和调用链中原位实现；不得新增本方案未要求的模块、服务、抽象层、通用框架、配置字段、界面入口、兼容层或未来扩展预留。确有必要修改上述范围之外的文件时，必须先把具体文件及必要性补入本方案并取得确认。

`app_settings_store.h/.cpp` 不在修改范围内：许可证默认模式只覆盖本次启动使用的内存设置，不改变现有设置 Schema 和存储格式。`runtime_guard.*` 负责在启动阶段一次性读取、绑定并输出解析后的许可证信息；启动层保存具体日期许可证已经校验通过的到期日期，由 24 小时定时器直接比较当前日期，过期时显示现有运行期提示并调用 `QCoreApplication::quit()`，长期许可证不创建该定时器。

许可证信息只由启动层持有并按启动需要使用：启动层根据已校验的授权模式 ID 集合和默认模式确定本次启动模式，并把授权模式 ID 集合传给 `MachineSettingsPage`；不新增全局单例，不把许可证字段写入 `AppSettings`，不让设置页自行读取 `license.ini`。

### 8.1 执行流程

1. 重写 `LicenseCodec` 的数据结构和读写实现，以明文 `version=3` 作为格式入口，直接解密独立加密的 `expires`、`features`、`defaultMode`、`deviceBinding`、`deviceCode`；写入时使用固定逻辑顺序，删除旧 `data` 总密文、Base64 载荷和日期专用 API，继续由调用方传入具体文件路径。
2. 直接替换 `RuntimeGuard::check()` 的旧布尔接口，改为启动阶段唯一的许可证结果接口；固定读取主程序应用程序目录下的 `license.ini`，先校验明文版本，再在 `runtime_guard.cpp` 内部通过单一 WMI 读取实现完成标识规范化和 S1/S2 设备码计算，同时完成许可证字段解密、有效期和授权字段校验以及首次设备绑定或已绑定设备校验，并把已校验的授权信息和到期日期交给启动层。失败继续返回现有通用初始化失败结果；不保留旧布尔接口包装层或第二套解析逻辑。
3. 在 `ApplicationStartup` 中先完成许可证入口，再读取设置；根据已保存模式是否获授权，在内存中选择保存模式或许可证 `defaultMode`，随后把授权模式集合传给设置页。具体日期许可证创建 24 小时定时器并直接比较启动阶段已确认的到期日期，过期时显示现有运行期提示“程序出错，即将退出，请联系供应商。”，用户确认后调用 `QCoreApplication::quit()`；`permanent` 许可证不创建该定时器。
4. `MachineSettingsPage` 遍历 `detectionModeDescriptors()` 并按启动层传入的授权集合过滤下拉框；`TemplateEditorPage` 使用下拉框实际数据，不保留按固定全量描述符索引回退，也不新增其他模式顺序来源。
5. 更新 `LicenseTool` 的单一 `expires` 控件、功能多选、默认模式选择和只读未激活状态；模式项和顺序直接取自 `detectionModeDescriptors()`，在 `LicenseTool.pro` 中复用现有 `contracts/detection_mode.cpp`，不复制模式定义；工具默认使用自身应用程序目录下的 `license.ini`，生成文件第一行写入明文 `version=3`，其余五个字段独立加密并按固定顺序写入，生成后立即用新格式读回显示。
6. 删除旧日期许可证读取、旧总密文写入、旧字段结构和所有仅服务于旧格式的声明、调用与工程引用；不删除其他检测模式代码、模板、模型或配置。

实施完成后只保留一套许可证数据结构、一套 `LicenseCodec` 读写接口、一套启动校验结果和一套位于 `RuntimeGuard` 内部的设备码计算入口。新代码直接使用新接口；不得通过旧函数转发、新旧结构互转、字段别名、默认值补全或双格式分支维持原调用点。

不修改检测模式稳定 ID，不修改模板 Schema，不新增在线授权服务，不复制客户专用代码分支。

## 9. 旧许可证处理

只有日期、没有功能和默认模式的旧许可证属于旧格式。新版本不把缺失字段解释为全功能，也不把默认模式自动解释为任意模式。

旧日期许可证以及缺少任一新字段的许可证都不能用于新版本启动，不能因为缺少字段而放行或补全默认值。旧许可证可作为外部备份保留，但主程序和 `LicenseTool` 不保留旧格式解析、旧 `data` 总密文、Base64 载荷或兼容回退；使用 `LicenseTool` 生成的新许可证必须符合 v3 字段结构。

## 10. 验收清单

### 10.1 许可证解析

- 文件第一行缺少明文 `version=3` 或值不等于 `3` 时启动被阻止，不继续解析后续密文。
- `expires` 为具体日期时，在到期日前有效，到期后失效。
- `expires=permanent` 时不比较日期，跨日期仍有效。
- 具体日期许可证启动 24 小时有效期定时器；长期许可证不启动该定时器。
- 长期许可证仍在启动时校验设备绑定，但不因日期变化退出。
- 24 小时定时器只比较启动阶段已确认的到期日期，不重新读取许可证或校验其他字段；发现过期时显示“程序出错，即将退出，请联系供应商。”，用户确认后调用 `QCoreApplication::quit()`。
- 任一必需字段缺失、无法解密或解密结果不符合对应业务规则时启动被阻止。
- 除第一行 `version=3` 外，许可证文件中不出现字段名称、日期、模式名称、功能 ID、`permanent` 或 `UNACTIVATED` 明文。
- 主程序固定读取自身应用程序目录下的 `license.ini`，`LicenseTool` 默认读写工具自身应用程序目录下的 `license.ini`。

### 10.2 默认模式

- 无 `app_settings.json` 时，首次启动使用许可证 `defaultMode`。
- 已保存且已授权的用户模式继续恢复，不被许可证默认模式强制覆盖。
- 已保存模式未授权时切换到许可证默认模式，其他配置和模板不丢失。
- 许可证默认模式不属于授权功能时拒绝启动。

### 10.3 功能授权

- 许可证只显示并允许启动其 `features` 中的模式。
- 启动时发现已保存模式未授权，则本次使用许可证 `defaultMode`。
- 包含全部稳定模式 ID 的许可证可以使用全部现有模式。
- 许可证升级后，原有其他模式模板和配置仍可使用。
- `LicenseTool` 默认模式下拉框和主程序检测模式下拉框都按 `detectionModeDescriptors()` 的固定顺序显示授权模式，`features` 的文本顺序不形成另一套界面顺序。

### 10.4 LicenseTool

- 可以为同一个 `expires` 字段生成具体日期或长期有效值。
- 可以选择多个授权功能。
- 默认模式只能从已授权功能中选择。
- 设备绑定和设备码字段生成时显示“未激活”且不可手工编辑。
- 生成文件按明文 `version=3`、`expires`、`features`、`defaultMode`、`deviceBinding`、`deviceCode` 的固定逻辑顺序写入；除 `version=3` 外不出现字段名称和值的明文。
- `features` 按 `detectionModeDescriptors()` 顺序使用英文逗号连接且不含空格。
- 读取功能能正确显示解密后的授权信息和设备绑定状态。

### 10.5 设备绑定

- 未激活许可证在目标设备首次启动时自动写入当前设备码。
- 首次绑定成功后重启，设备码匹配并正常启动。
- 将已激活许可证复制到另一台设备，启动被阻止。
- S1 有效时始终使用 S1；S1 无效时首次绑定使用 S2，后续不会自动切换绑定方式。
- S2 缺少主板序列号或 BIOS 序列号时，设备码生成失败并禁止启动。
- 激活时许可证目录不可写，启动被阻止且只显示通用初始化失败提示。
- System UUID、主板序列号或 BIOS 序列号按 3.5 规则无效时，按对应设备标识无效处理。
- 设备码生成所需标识缺失时，启动被阻止且不显示具体原因。
- 设备码不写入 `app_settings.json`、模板、日志或主程序界面。

## 11. 明确不做的事情

- 不删除钢印、字库、OCR、纸巾检测代码。
- 不删除未授权模式模板、配置、模型或 DLL。
- 不保留旧 `data` 总密文、Base64 载荷、旧日期解析函数或旧许可证兼容回退。
- 不保留旧 `LicenseReadResult`、`LicenseFileError` 字段语义、旧 `writeFile(QDate, ...)` 签名或任何同名兼容重载；旧接口直接删除，调用方一次性迁移到新接口。
- 不增加新旧许可证结构之间的转换器、适配器、代理、包装函数、字段别名、兼容默认值或双格式解析分支。
- 不为保持旧调用点而增加胶水层；`RuntimeGuard`、`LicenseTool`、启动层和设置页直接使用同一套新许可证结果和授权模式集合。
- 不复制一套新的模式 ID 映射表或模式顺序表；稳定 `modeId` 到 `DetectionMode` 的转换和所有授权模式顺序只使用现有 `detectionModeDescriptors()` 及其规范入口，不新增兼容映射、UI ID 回退或其他顺序来源。
- 只删除本次替换后仅服务于旧许可证格式的声明、实现、调用和工程引用，并以本方案列出的旧许可证符号零引用作为静态门禁；不清理其他无关代码。
- 不把缺少功能字段的许可证默认放行。
- `license.ini` 只允许第一行 `version=3` 使用明文；其他字段名称和值均不使用明文。
- 不把有效期、功能、默认模式和设备码重新合并为一段总密文；每个字段名和字段值都独立加密。
- 不把设备码写入设置文件、模板、日志或主界面。
- 不使用产品盐、MachineGuid、MAC 地址或系统盘卷序列号生成设备码。
- 不在已绑定许可证的后续启动中自动切换 S1/S2 绑定方式。
- 不为设备绑定引入 TPM、加密狗、在线授权、账号系统或复杂硬件防护。
- 不引入在线服务器、账号系统或复杂公钥基础设施。
