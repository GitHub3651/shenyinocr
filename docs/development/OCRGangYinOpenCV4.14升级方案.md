# OCRGangYin OpenCV 4.14 升级方案

## 1. 文档状态

- 文档日期：2026-09-23。
- 当前状态：代码和依赖已实施，静态检查通过，待用户统一验证。
- 权威范围：项目内 OpenCV 3.4.1 开发包、主工程链接、Release 运行库及部署清单统一切换到 OpenCV 4.14.0。
- 当前进度：OpenCV 4.14.0 官方 Windows x64 `vc16` 开发包、主工程链接、Release 运行库、部署清单和依赖记录已同步；旧版本开发包及运行库已清除，生产 C++ 业务代码未修改，等待用户在 Qt Creator 统一验证。

## 2. 目标

1. 将 `third_party/opencv` 完整替换为 OpenCV 4.14.0 官方 Windows x64、MSVC 2019 `vc16` 开发包，并由当前 MSVC 2022 Kit 直接链接。
2. 主工程 Release 和 Debug 分别只链接 `opencv_world4140.lib` 和 `opencv_world4140d.lib`。
3. Release 发布目录只保留并部署 `opencv_world4140.dll`，不混放旧版本或拆分模块 DLL。
4. 项目现有检测算法、识别流程、参数、阈值、模型、界面和设备行为全部保持不变。

## 3. 可行性与影响结论

### 3.1 可行性

升级已按现有接口完成，不需要修改生产 C++ 源码。

- 生产代码使用 `cv::Mat`、`resize`、`warpAffine`、`warpPerspective`、`matchTemplate`、`minMaxLoc`、`findContours`、`minAreaRect`、`connectedComponentsWithStats`、`GaussianBlur`、`Sobel`、`Laplacian`、`parallel_for_`、`imdecode`、`imencode` 等 OpenCV 4.14 仍提供的接口。
- 未发现 `IplImage`、`CvMat` 或已删除的旧式 C API 调用。
- `minAreaRect` 的角度语义变化不影响当前代码；当前后处理只读取矩形顶点和尺寸，不读取 `RotatedRect::angle`。
- 相机 SDK、条码 DLL 和 Paddle Inference DLL 均不跨 DLL 边界传递 `cv::Mat`，不需要同步改造第三方接口。
- 升级前主程序只导入 `opencv_world341.dll`；原发布目录中的五个 OpenCV 拆分模块 DLL 没有被主程序或现有厂商 DLL 直接导入。

### 3.2 速度与识别结果

- 本次升级不修改算法、参数、模型或输入图像，因此不会主动改变识别规则，也不把精确度提升作为升级结果承诺。
- OpenCV 4.14 的内部实现、编译器优化和 CPU 调度可能使部分算子更快或更慢，单纯换包不能保证整体检测速度一定提升。
- 浮点计算、轮廓和几何算子的内部实现变化可能在边界样本上产生轻微数值差异，不能承诺逐像素或逐分数完全一致。
- 本次升级值得执行的主要原因是清理原先混杂的 3.4.1、3.4.6、4.4.0 文件，并统一使用可由当前 VS2022 x64 Kit 链接的官方 4.14.0 包；不以修改业务算法换取速度或精确度变化。

## 4. 最终目录与链接

`third_party/opencv` 使用单一版本，保留主工程需要的头文件、导入库、运行库和许可证：

```text
third_party/opencv/
├─ include/opencv2/**
├─ x64/vc16/lib/
│  ├─ opencv_world4140.lib
│  └─ opencv_world4140d.lib
├─ x64/vc16/bin/
│  ├─ opencv_world4140.dll
│  └─ opencv_world4140d.dll
└─ LICENSE
```

Release 发布目录中的 OpenCV 文件为：

```text
dist/ShengYin/opencv_world4140.dll
```

旧的 `opencv_world341*`、`opencv_world346*`、`opencv_world440*`、OpenCV 3.4.1 拆分模块及 `vc15` 目录不保留。

## 5. 实施范围

### 5.1 替换开发包

1. 删除当前混合版本的 `third_party/opencv` 内容。
2. 放入 OpenCV 4.14.0 官方 Windows x64 `vc16` 的头文件、Release/Debug 导入库、Release/Debug 运行库和许可证。
3. 确认头文件中的版本为 4.14.0，导入库和 DLL 均为 x64，且 Release 导入库与 `opencv_world4140.dll` 来自同一开发包。

### 5.2 修改主工程

修改 `app/AutoOCRproject.pro`：

1. OpenCV 头文件只保留 `$$THIRD_PARTY/opencv/include` 一个路径，并让 `DEPENDPATH` 指向同一目录。
2. 库目录从 `x64/vc15/lib` 改为 `x64/vc16/lib`。
3. Release 链接名从 `opencv_world341` 改为 `opencv_world4140`。
4. Debug 链接名从 `opencv_world341d` 改为 `opencv_world4140d`。
5. 删除不再需要的 `CV_IGNORE_DEBUG_BUILD_GUARD` 兼容开关，保持当前 C++ 标准、`/MT`、`/MTd` 和其他依赖设置不变。

### 5.3 修改发布运行库

1. 从 `dist/ShengYin` 删除现有 OpenCV 3.4.1 DLL。
2. 将同一 OpenCV 4.14.0 开发包中的 x64 Release `opencv_world4140.dll` 放入 `dist/ShengYin`。
3. 修改 `app/system_support/deployment/deploy_runtime.ps1`，OpenCV 必需文件只登记 `opencv_world4140.dll`。

### 5.4 更新版本记录

将 `third_party/DEPENDENCIES.md` 中的 OpenCV 版本、编译器和架构更新为 OpenCV 4.14.0、MSVC 2019 `vc16`、Windows x64，并记录由当前 MSVC 2022 Kit 链接。

## 6. 不修改的内容

- 不修改任何检测、模板匹配、OCR、二维码、图像预处理或后处理算法。
- 不修改阈值、模型、配置 Schema、界面、日志、线程、相机、PLC 或部署目录结构。
- 不修改 Paddle Inference、相机 SDK、条码 DLL、Qt 或其他第三方依赖版本。
- 不新增 OpenCV 版本选择、运行时探测、兼容目录、包装层或回退逻辑。
- 静态审查未发现需要修改的生产 `.cpp` 或 `.h` 文件，因此这些文件不在计划修改清单内。

## 7. 计划修改清单

| 路径 | 修改内容 |
|---|---|
| `third_party/opencv/**` | 完整替换为 OpenCV 4.14.0 官方 Windows x64 `vc16` 开发包 |
| `app/AutoOCRproject.pro` | 更新唯一头文件路径、库目录和 `opencv_world` 链接名，删除旧 Debug 兼容开关 |
| `dist/ShengYin/opencv*.dll` | 删除旧 OpenCV DLL，只保留 `opencv_world4140.dll` |
| `app/system_support/deployment/deploy_runtime.ps1` | 部署清单改为 `opencv_world4140.dll` |
| `third_party/DEPENDENCIES.md` | 记录 OpenCV 4.14.0、MSVC 2022、x64 和来源 |
| 本方案与计划索引 | 实施后更新状态 |

## 8. 实施后的检查

Agent 负责以下静态检查：

1. 工程和部署文件中不再引用 `opencv_world341`、`opencv_world346`、`opencv_world440`、`vc15` 或 OpenCV 3.4.1 拆分模块 DLL。
2. `third_party/opencv` 中不存在旧版本 OpenCV 头文件、库或 DLL。
3. `app/AutoOCRproject.pro` 的 include、depend、Release 和 Debug 路径均指向实际存在的 4.14.0 文件。
4. `dist/ShengYin/opencv_world4140.dll` 与开发包 Release DLL 一致。
5. 计划内文本文件通过 `git diff --check`。

## 9. 需要用户统一验证

项目限制规定 Agent 不执行 qmake、构建或主程序运行，因此请用户在当前 Qt Creator Kit 中完成：

1. 执行 Run qmake。
2. 执行 Release Rebuild。
3. 启动主程序，确认没有缺失或加载错误的 OpenCV DLL。
4. 打开相机，对当前实际使用的检测模式各执行一次现有检测流程，确认原来可识别的现场样本仍可识别，单帧处理没有明显变慢。

## 10. 完成状态

以下结果全部成立后，将本方案状态更新为“已完成”：

1. 仓库内 OpenCV 开发包、主工程链接、Release 运行库和部署清单均为 4.14.0。
2. 旧 OpenCV 版本文件和引用已清除。
3. 生产 C++ 业务代码没有因升级发生功能性修改。
4. 用户确认 Release 构建、启动和现有检测流程正常。
