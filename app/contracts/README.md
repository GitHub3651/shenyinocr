# contracts：全项目共享的稳定业务词汇

## 一句话理解

`contracts` 只定义跨多个模块都要认识、而且必须保持唯一含义的轻量业务合同。当前主要统一回答：“系统到底有哪五种检测模式，每种模式叫什么、需要什么定位和资源、结果如何展示”。

## 为什么需要这个目录

如果没有这里，UI、设置、配方、Detection 和 Runtime 很容易分别维护一套模式编号和规则，最终出现：

- UI 下拉框第 2 项与配方中的模式 ID 不一致；
- 某层认为纸巾需要模板，另一层认为不需要；
- 新增模式要在十几个位置重复硬编码；
- 二维码默认参数在多个模块出现不同数字。

所以这里不是“多余的中间层”，而是少量、稳定、全局唯一的词典。

## 当前文件树

```text
contracts/
├─ barcode_parameter_defaults.h
├─ detection_mode.h
├─ detection_mode.cpp
└─ README.md
```

## 逐文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `barcode_parameter_defaults.h` | 二维码格式掩码、ROI 外扩比例、最大解码时间和 fallback 开关的唯一默认值。 | 只定义默认合同；不执行解码，也不保存配方。 |
| `detection_mode.h` | 定义五种稳定 `DetectionMode`、定位类型 `DetectionTrackingKind`、`DetectionModeDescriptor` 及 ID 转换接口。 | 只放轻量类型和查询接口；不能 include Pipeline、UI 控件或 Runtime 对象。 |
| `detection_mode.cpp` | 唯一登记五种模式的 Recipe ID、UI ID、中文名、日志名、启动错误、定位要求、资源要求、呈现策略、存图策略和耗时精度。 | 新增/修改条目是全局行为变化，必须同步配方、Detection、UI 和回归。 |

## 当前五种模式

```text
DetectionModeDescriptor 唯一清单
├─ 钢印检测
├─ 字库匹配
├─ 深度学习 OCR
├─ 纸巾检测
└─ 二维码 + 三期
```

具体稳定 ID 和属性以 `detection_mode.cpp` 为准，不要从 UI 下拉框序号或文件夹名称反推模式。

## 谁依赖它

```text
contracts/detection_mode
├─→ system_support/settings   保存当前模式 ID
├─→ recipes                   校验配方模式和资源要求
├─→ application               启动前置检查和提示
├─→ detection                 Registry 选择 Pipeline
├─→ runtime                   使用通用呈现/存图策略
└─→ ui                        构造模式下拉框和显示名称
```

依赖应当从其他层单向指向 `contracts`；本目录不应反向 include 上述模块。

## 新增第六种模式时怎么改

1. 在 `detection_mode.h` 添加稳定枚举值。
2. 在 `detection_mode.cpp` 添加一条完整 Descriptor。
3. 在 ProductRecipe/Profile 中添加确实需要的新字段与校验。
4. 在 Detection 中新增 Pipeline，并只在 Registry 装配一次。
5. 补充启动前置检查；UI 模式列表会从 Descriptor 生成。

如果新增模式还要求修改 Runtime 的线程、队列或结果链，通常说明模式特性泄漏到了错误层级，应先重新检查设计。

## 允许放什么

- 小型枚举、ID、轻量值对象和纯查询函数。
- 跨三层以上共享且必须只有一个定义的稳定规则。
- 不拥有资源、不执行 I/O、没有线程和 UI 副作用的默认值。

## 禁止放什么

- “为了方便”而搬入的大型业务对象。
- Detection Pipeline、RecipeStore、Runtime、ApplicationService 或 QWidget。
- 文件读写、设备访问、线程、算法执行和弹窗。
- 与某个具体供应商 SDK 绑定的类型。

## 维护红线

- 不要在 UI、Settings、Recipes、Runtime 再建第二套模式列表。
- 不要用下拉框索引代替稳定模式 ID。
- 不要在多个文件复制二维码默认数字。
- Descriptor 字段发生变化时，要核对所有消费者而不是只保证编译通过。

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单和开发者维护指南。
