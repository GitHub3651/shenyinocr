# OCRGangYin 模板重新取景状态图像设置可编辑方案

## 1. 状态与范围

- 状态：代码已实施，待用户验证。
- 权威范围：模板画面已冻结、主按钮显示“重新取景”时的图像设置页权限。
- 只调整 `OperationUiPolicy` 及现有设置页权限映射，不修改模板取景流程、相机采集时序、设置保存语义、检测算法、PLC、模板 Schema、`.ui` 或 QSS。

## 2. 最终行为

- `TemplateFrozen` 状态允许调整图像设置页中的图像保存、相机曝光、相机增益、颜色通道和图像旋转。
- 相机曝光和增益仍要求相机已打开；应用后，用户点击“重新取景”进入的新预览使用调整后的参数。
- `TemplatePreviewing`、`Detecting` 和 `Stopping` 状态继续禁用图像设置。
- 参数设定、PLC 通讯、软件设置、模板选择和模板编辑仍沿用原权限，不因本方案放开。

## 3. 实现边界

1. `OperationUiSnapshot` 增加唯一的 `imageSettings` 权限；空闲态和 `TemplateFrozen` 可用。
2. `cameraSettings` 在相机已打开时允许 `CameraReady` 和 `TemplateFrozen`。
3. `MachineSettingsPage` 将图像保存、颜色通道和图像旋转绑定到 `imageSettings`；曝光和增益继续绑定 `cameraSettings`。
4. 图像保存浏览、颜色通道确认和图像旋转设置按钮使用同一 `imageSettings` 权限。

## 4. 静态门禁与用户验证

- `OperationUiPolicy` 仍是唯一权限计算入口，不在控件槽中增加状态判断。
- 图像页各编辑器、标签和按钮使用一致权限；其他设置页仍使用原权限。
- `git diff --check` 通过；不执行项目限制禁止的构建和运行。
- 用户在 Qt Creator 中构建并进入模板冻结状态，确认图像设置页全部可调整；进入实时取景后确认该页重新禁用；检测和停止期间保持禁用。
