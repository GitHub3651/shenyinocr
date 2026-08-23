# detection：正式检测算法

本目录输入一帧图像和启动时冻结的参数，输出统一 `DetectionResult`。它不控制相机、PLC、设置、模板磁盘或 UI。

## 文件树

```text
detection/
├─ detection_registry.h/.cpp             五模式 Pipeline 装配
├─ detection_template_snapshot.h/.cpp    多模板定位/字符运行快照
├─ common/
│  ├─ frame_preprocessor.h/.cpp
│  ├─ character_template_matcher.h/.cpp
│  ├─ detection_roi_geometry.h
│  ├─ detection_pose.h
│  ├─ tracking_pose_matcher.h/.cpp
│  ├─ inspection_positioner.h/.cpp        无/单/多模板定位
│  └─ template_pose_selector.h/.cpp       最高分模板选择
└─ detectionmode/
   ├─ stamp/                              刚印检测与重叠判断
   ├─ word/                               字库匹配
   ├─ ocr/                                深度 OCR
   ├─ tissue/                             纸巾检测
   └─ barcode_word/                       二维码+三期
```

## 一帧的流程

```text
FrameData
 → FramePreprocessor
 → InspectionPositioner
    ├─ WholeFrame：纸巾
    ├─ SingleTemplate：刚印、OCR
    └─ MultipleTemplates：并行评价全部有效模板
         → TemplatePoseSelector 取严格最高分
         → 同分保留路径顺序较早项
 → 当前模式 Pipeline
 → DetectionResult
```

`DetectionTemplateSnapshot` 的定位条目和检测条目按同一个紧凑运行数组建立。运行下标只在本次启动期间访问对应条目，不持久化、不建立路径索引 Map。

`detection` 根目录只保留检测模块的装配入口和运行快照；跨模式算法放在
`common`，具体模式算法放在 `detectionmode`。这样新增模式时只需新增一个
`detectionmode/<模式>` 子目录，并在 `DetectionRegistry` 中接入，不会把模式代码
散落到公共目录或根目录。

## 维护规则

- 运行中不得读取设置 JSON、外部模板文件夹或 UI。
- 多模板不能首个成功就返回；必须比较全部有效模板。
- 命中模板名称、参数和资源必须来自产生最高定位分数的同一个运行条目。
- 纸巾阈值来自运行快照，不在检测器内维护第二个业务默认值。
- 新增跨模式的检测能力放 `common`；只服务一个模式的代码放在
  `detectionmode/<模式>`，不要再在 `detection` 根目录或模式目录之间复制。
