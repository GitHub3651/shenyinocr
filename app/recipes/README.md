# recipes：产品配方、运行快照、编辑会话与事务存储

## 一句话理解

`recipes` 管理“某一种产品如何检测”的数据和资源。它把可编辑、可保存的 `ProductRecipe` 转换为资源已加载的只读 `PreparedRecipe`，并通过编辑会话和事务 Store 保证发布失败不会破坏现有正式配方。

## 当前只有四组职责

```text
recipes/
├─ product_recipe.h/.cpp       可序列化的正式配方 Schema
├─ prepared_recipe.h/.cpp      资源加载后的只读运行快照
├─ recipe_editor_session.h/.cpp 编辑草稿与临时资源工作区
├─ recipe_store.h/.cpp         正式目录的加载与事务发布
└─ README.md
```

8 个代码文件只是 4 个类/概念的头源配对，不是 8 套配方系统。

## 配方和整机设置的区别

| 数据随什么变化 | 应放哪里 | 例子 |
|---|---|---|
| 随产品/模板变化 | `ProductRecipe/Profile` | 目标文字、ROI、字符模板、检测阈值、纸巾粗糙度、二维码参数 |
| 随整机/产线变化 | `MachineSettings` | 曝光、增益、旋转、通道、触发、PLC、存图目录 |
| 只用于一次界面操作 | UI Page 状态 | 当前画到了第几个点、对话框临时输入 |
| 只用于算法中间计算 | Detection 内部 | 梯度、轮廓、临时匹配候选 |

不要把产品参数和机器参数重新混回一个大配置文件。

## 两种配方形态

### ProductRecipe：保存形态

- 可序列化为 `recipe.json`。
- 保存 UUID、显示名、模式、Profile 参数和资产相对路径。
- 可以在 UI 编辑会话中修改。
- 不直接持有已解码的 OpenCV 图片或预编译模板。

### PreparedRecipe：运行形态

- 在开始检测前一次性加载所有必需资源。
- 形成 `shared_ptr<const ...>` 类型的只读快照。
- Pipeline 运行中只读内存，不临时访问磁盘。
- 资源缺失或损坏时应在准备阶段拒绝启动。

```text
recipe.json + 资产相对路径
→ ProductRecipe 校验
→ prepareRecipe 加载图片/YAML/字符资产
→ PreparedRecipeSnapshot
→ InspectionRunContext 冻结持有
→ Detection Pipeline 只读使用
```

## 逐文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `product_recipe.h` | 定义纸巾参数、二维码参数、字符框、Profile、产品 UUID/名称/模式/资产 Map，以及创建、校验、JSON 转换和只读快照接口。 | 资源只保存相对路径和稳定资产键。 |
| `product_recipe.cpp` | 实现五模式的 UUID、名称、Profile、ROI、参数范围、资源角色校验和 JSON 读写。 | 不加载图片、不执行算法；新增字段必须同时补默认、读、写和验证。 |
| `prepared_recipe.h` | 定义资源已加载后的 Recipe/Profile/CharacterAsset 运行模型和准备接口。 | 运行快照保持只读；不能把可编辑草稿交给线程。 |
| `prepared_recipe.cpp` | 安全解析资产路径，加载原图、定位模板、钢印环、校准多边形、字符图，并构建目标字符索引。 | 路径必须留在配方目录内；缺损资源不能静默跳过。 |
| `recipe_editor_session.h` | 声明编辑会话状态、首次 Profile 资源输入和工作区 API。 | 会话不能更改既有 recipeId 或检测模式。 |
| `recipe_editor_session.cpp` | 创建 UUID 临时工作区，载入草稿，暂存 PNG/BMP/YAML/字符图，最后委托 Store 发布。 | 工作区不是正式配方目录；析构只删除经过路径核对的本会话目录。 |
| `recipe_store.h` | 声明配方加载、Prepared 加载、目录枚举和整目录事务保存。 | 正式配方目录唯一写入口。 |
| `recipe_store.cpp` | 写同级临时目录、重新加载校验、备份旧目录、原子式替换，失败时恢复备份；也枚举损坏配方。 | 不能退化为逐文件覆盖；清理只能限定在 recipes 根目录。 |

## 模板编辑与发布流程

```text
TemplateEditorPage
→ TemplateApplicationService::begin/create/edit
→ RecipeEditorSession 创建工作区
→ 用户多次修改草稿、暂存资源
→ TemplateApplicationService::publish
→ RecipeStore::saveRecipe
   1. 写临时目录
   2. 从临时目录重载并比较
   3. 正式旧目录改名为备份
   4. 临时目录替换为正式目录
   5. 失败则恢复备份
→ loadPreparedRecipe 验证运行资源
→ 激活配方
```

发布成功前，旧正式配方必须始终可读。编辑会话失败时可以保留草稿供用户修正，但不能留下半个正式配方。

## 目录与资产安全

- `recipeId` 是真正身份，显示名称可变。
- 正式配方保存相对资产路径，不能保存用户机器绝对路径。
- 解析后路径必须确认仍位于当前配方目录下，防止 `..` 越界。
- 临时/备份目录必须位于 Store 管理的 recipes 根目录内。
- Pipeline 不允许根据相对路径自行补读磁盘。

## 允许放什么

- 配方 Schema、唯一默认值、类型/范围/交叉校验和 JSON 读写。
- 配方相关资产的准备和只读快照。
- 编辑工作区、发布事务、枚举与损坏诊断。
- 与产品模式/Profile 直接相关的数据结构。

## 禁止放什么

- 相机/PLC、运行线程、统计、存图策略和 QWidget。
- 正式图像算法和 Engine 调用。
- 整机设置字段。
- 对旧设置/旧模板/旧目录的兼容双读路径。
- 逐文件覆盖正式目录或失败后留下半成品。

## 新增一个配方字段

必须一次同步：

1. 数据 Schema 文档和唯一默认值。
2. C++ 字段。
3. JSON 写出与读入。
4. 类型、范围和交叉字段校验。
5. PreparedRecipe 的加载/准备（如果运行需要）。
6. TemplateApplicationService 与 UI 编辑绑定。
7. Pipeline 消费者。
8. 首次创建、保存重启、损坏拒绝和发布回滚验证。

## 推荐阅读顺序

```text
product_recipe.h/.cpp
→ prepared_recipe.h/.cpp
→ recipe_editor_session.h/.cpp
→ recipe_store.h/.cpp
→ ../application/template_application_service.*
```

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单、数据 Schema 和开发者维护指南。
