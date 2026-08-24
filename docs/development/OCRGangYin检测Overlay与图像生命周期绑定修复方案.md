# OCRGangYin 检测 Overlay 与图像生命周期绑定修复方案

## 1. 状态与基线

- 状态：代码实施完成，待用户统一验证。
- 代码基线：`1363fe1`（`fix: 统一图像提示栏模板来源与检测状态`）。
- 本方案只处理检测覆盖图与当前图像生命周期不一致的问题。
- 工作区中的日志系统方案及其索引改动属于其他会话，本方案不修改、不提交。

## 2. 问题

修改前，`InspectionPresentationRenderer` 把最近一次检测结果的 `DetectionOverlay`
保存在成员状态中。模板原图或相机预览刷新时又复用了同一个渲染入口，导致上一张
检测图的框和置信度被重新绘制到新图上。

根因不是图片控件没有清屏，而是渲染器把“上一张图的 Overlay”错误地带入了
“当前图片”的渲染。

## 3. 目标

1. 渲染器无状态，不保存上一帧 Overlay。
2. 检测图必须在同一次调用中传入图片和本次 `DetectionOverlay`。
3. 模板原图、相机预览只渲染原图，不允许隐式取得 Overlay。
4. 删除因旧缓存状态产生的清理接口和互斥锁。
5. 保持检测算法、Overlay 样式、存图、PLC 和 UI 行为不变。

## 4. 最简实现

渲染器只保留两个明确入口：

```cpp
static QImage renderRawFrame(const cv::Mat &image);
static QImage renderDetectionFrame(
    const cv::Mat &image,
    const DetectionOverlay &overlay);
```

- `renderRawFrame()`：转换并返回原图，不绘制任何 Overlay。
- `renderDetectionFrame()`：克隆本次图片，并只绘制同次传入的 Overlay。
- 删除 `InspectionPresentationRenderState`、`clear()`、
  `installDetectionResult()`、`state()` 和 `m_state`。
- `ResultService` 的检测完成路径直接把
  `originalImage + accepted.result.overlay` 同次传给渲染器。
- 预览链只传图片；`InspectionApplicationService` 仍保留纸巾生产中禁止预览的现有判断，
  但不再向 Runtime 传递“是否绘制纸巾 Overlay”的参数。
- 删除无调用者的 `ResultService::clear()`，以及只保护渲染器缓存的
  `m_presentationMutex`。
- 删除无人消费的 `ResultServiceProcessOutcome`，`process()` 改为无返回值，
  只保留事务内部实际使用的统计和 PLC 局部变量。
- 删除旧状态渲染遗留的 `preparePresentation` 回调，直接从同一个
  `DetectionCompletion` 构建呈现数据。
- 删除 `ResultService::renderPreviewFrame()` 单行转发；Runtime 直接调用
  `renderRawFrame()`，Application 到 Runtime 的边界保持不变。
- 删除上述遗留产生的无用头文件依赖。

## 5. 文件范围

只修改以下生产文件：

1. `app/runtime/inspection_presentation_renderer.h`
2. `app/runtime/inspection_presentation_renderer.cpp`
3. `app/runtime/result_service.h`
4. `app/runtime/result_service.cpp`
5. `app/runtime/inspection_runtime.h`
6. `app/runtime/inspection_runtime.cpp`
7. `app/application/inspection_application_service.cpp`

计划文档和计划索引随实施状态同步更新。

## 6. 实施步骤

1. 将 Renderer 改为无成员状态的原图/检测图双入口。
2. 修改 ResultService：检测图显式传 Overlay，预览图只传图片。
3. 精简 Runtime 预览签名并保留 ApplicationService 的纸巾预览门禁。
4. 删除旧状态、清理入口、互斥锁及全部旧符号引用。
5. 删除 Outcome、延迟呈现回调、ResultService 预览转发和对应无用 include。
6. 运行静态引用、差异和构建前门禁；完整 Qt 构建及交互验证交由用户统一执行。

## 7. 验收标准

1. 检测完成后切换到模板原图，上一张检测框和置信度立即消失。
2. 新相机预览帧替换检测图时，不携带上一帧 Overlay。
3. 正式检测结果仍显示本次检测产生的钢印、OCR、二维码或纸巾 Overlay。
4. 连续检测时，每张图只显示自己的 Overlay。
5. 代码中不存在 Renderer 缓存状态、安装结果或清空状态的接口。
6. 未修改检测算法、模板数据、PLC、存图或 ImageLabel 绘图逻辑。

## 8. 统一验证路径

1. 启动任一会产生检测框的模板模式并完成一次检测。
2. 停止检测，切换模板或刷新模板产品图像，确认旧框同步消失。
3. 启动相机预览，确认预览帧不显示上一检测结果。
4. 再次启动检测，确认当前检测框正常显示。
5. 分别抽查钢印、OCR、二维码和纸巾模式。

## 9. 实施结果

- 七个计划内生产文件已完成修改。
- Renderer 已改为无实例、无成员状态的静态渲染入口。
- 正式检测只通过 `renderDetectionFrame(image, overlay)` 绘制本次 Overlay。
- 模板原图和相机预览只通过 `renderRawFrame(image)` 显示原图。
- 旧状态结构、安装/清空接口、Renderer 成员和专用互斥锁已删除。
- 无消费 Outcome、`preparePresentation` 回调、ResultService 预览转发和对应无用 include 已删除。
- 清理批次额外净减少 28 行；全部生产代码累计净减少 79 行（新增 55 行、删除 134 行）。
- 旧符号零引用，`git diff --check` 通过。
- 当前命令行环境未发现 MSVC 构建环境和 `jom`；完整 Qt 构建及第 8 节交互验证待用户统一执行。
