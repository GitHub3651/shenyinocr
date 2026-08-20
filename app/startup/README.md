# startup：进程入口和完整对象图的组合根

## 一句话理解

`startup` 负责把整个软件“组装起来”：创建 QApplication，安装日志/崩溃处理，检查单实例和授权，构造所有具体设备、Engine、Store、Runtime、ApplicationService、Page 和 MainWindow，然后进入 Qt 事件循环。

它可以知道所有具体实现，但不能包含检测算法、业务判定或 PLC 时序。

## 当前文件树

```text
startup/
├─ main.cpp
├─ application_startup.h
├─ application_startup.cpp
├─ runtime_guard.h
├─ runtime_guard.cpp
├─ single_instance_guard.h
├─ single_instance_guard.cpp
└─ README.md
```

## 启动流程

```text
main
→ ApplicationStartup::run
→ QApplication
→ 翻译 / 样式
→ 单实例检查
→ 授权检查
→ 日志 / Windows 崩溃处理
→ 构造具体硬件与识别实现
   ├─ HikvisionCameraDevice
   ├─ Snap7PlcDevice
   ├─ PaddleOcrEngine
   └─ BarcodeDecoderAdapter
→ 构造 Store / DetectionRegistry / Runtime
→ 构造 ApplicationService
→ 构造 MainWindow
→ 构造 InspectionPage / MachineSettingsPage / TemplateEditorPage
→ MainWindow::attachPages
→ show + QApplication::exec
→ 按依赖逆序清理
```

## 逐文件说明

| 文件 | 当前职责 | 维护边界 |
|---|---|---|
| `main.cpp` | 最小进程入口，只把 `argc/argv` 交给 `ApplicationStartup::run`。 | 不添加设备、窗口、样式和业务逻辑。 |
| `application_startup.h` | 声明组合根 `ApplicationStartup` 和静态 `run`。 | 保持头轻量，不向外暴露 vendor 类型。 |
| `application_startup.cpp` | 执行全部进程初始化、构造对象图、注入依赖、显示窗口和退出清理。 | 允许构造与连接，不允许产品判定、算法和 PLC 时序。 |
| `runtime_guard.h` | 声明启动授权检查入口。 | 只表达能否继续启动。 |
| `runtime_guard.cpp` | 从应用目录读取 `license.ini`，调用 `LicenseCodec` 并检查到期日期。 | 授权格式由 Codec 唯一维护；这里决定启动提示和退出。 |
| `single_instance_guard.h` | 声明基于 `QSharedMemory` 的单实例守卫并持有其生命周期。 | 共享键必须稳定。 |
| `single_instance_guard.cpp` | 使用固定键 `ecust` 创建共享内存；创建失败表示已有实例。 | 不把业务状态写入共享内存，也不做进程扫描。 |

## 为什么“具体实现”只能在这里出现

Runtime 依赖的是 `ICameraDevice`、`IPlcDevice`、`IOcrEngine` 和 `IBarcodeDecoder` 接口。必须有一个地方选择当前部署实际用海康、Snap7、Paddle 和 Barcode DLL，这个地方就是组合根。

这样更换设备时只需要：

```text
实现新接口适配器
→ 在 ApplicationStartup 替换 new 的具体类型
→ 其余 Application / Runtime / Detection / UI 保持不变
```

如果具体实现由业务对象在内部偷偷 `new`，对象所有权、测试替换和退出顺序都会变得不可控。

## 页面组合关系

`main_window.ui` 只有一份。Startup 先创建 `MainWindow`，再从窗口取得三组显式 ViewBindings 来构造三个区域管理器，最后 attach：

```text
MainWindow（拥有 Ui::MainWindow 和真实控件）
├─ InspectionPage          管理检测结果区域
├─ MachineSettingsPage     管理整机设置区域
└─ TemplateEditorPage      管理模板制作区域
```

这三个 Page 不是三个独立窗口，也不拥有 `Ui::MainWindow`。

## 生命周期与销毁顺序

- 硬件/Engine/Store/Runtime/ApplicationService 的所有者在 `run` 栈和智能指针中明确建立。
- Page 的生命周期必须覆盖 MainWindow 使用它们的期间。
- 退出时先停止采集和 Runtime，取消队列并 join，再销毁相机/PLC/Engine。
- 日志和崩溃处理在业务对象清理完成后关闭。

禁止依靠全局静态对象的未定义销毁顺序。

## 允许放什么

- 进程级初始化和退出清理。
- 具体实现选择、对象创建、依赖注入和顶层信号连接。
- 翻译、样式、日志、授权、单实例、崩溃处理的安装。

## 禁止放什么

- 检测模式判断、阈值、产品判定和模板算法。
- PLC 结果值、复位延时或地址编码。
- UI 页面内部交互和控件 enable/disable。
- 为解决构造依赖而引入全局单例或 Service Locator。

## 新增依赖时怎么做

1. 先在正确低层定义稳定接口和实现。
2. 明确谁拥有对象、谁只借用指针/引用。
3. 在 `ApplicationStartup::run` 构造一次。
4. 通过构造参数逐层注入真正需要它的对象。
5. 核对异常/提前返回时对象能否安全逆序释放。
6. 核对 Release 部署是否包含新的 DLL、模型或配置。

新增、删除或移动本目录代码文件时，请同步更新本 README、qmake 工程清单和开发者维护指南。
