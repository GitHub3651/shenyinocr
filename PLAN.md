# 字库匹配多模板分阶段实现计划

## 总原则
- 只改 `comboBox_4 = 字库匹配` 相关逻辑；模板匹配、深度模型、纸巾检测不动。
- 每次只实现一个阶段；阶段完成后只做轻量静态检查，不完整编译。
- 每个阶段都由你在 Qt Creator 中执行 `Run qmake / Clean / Rebuild / Run` 验证。
- 多模板路径不保存，不做启动自动加载；每次启动后重新选择模板。
- 单选一个模板时必须保持现有行为不变。

## 阶段 1：多选模板文件夹入口
- 修改“选择模板”按钮：
  - 非字库模式保持当前单选逻辑。
  - 字库匹配模式使用非原生 `QFileDialog` 支持多选文件夹。
  - 选 1 个文件夹：调用现有 `loadSettingsFromDir()`，清空多模板缓存。
  - 选多个文件夹：只缓存路径并提示“已选择 N 个字库模板”，暂不加载模板内容、不接入检测。
- 新增最小成员变量：
  - `QStringList m_wordTemplateDirPaths`
  - `bool m_wordMultiTemplateMode`
- 验证点：单选模板仍正常；多选不会崩溃，界面能提示数量。

## 阶段 2：多模板 Profile 只读加载
- 新增内存结构：
  ```cpp
  struct WordTemplateProfile {
      QString name;
      QString dirPath;
      cv::Mat trackingTemplate;
      std::vector<cv::Point2f> datePoly;
      QString targetText;
      int targetCount = 0;
      std::vector<cv::Mat> digitTemplates;
  };
  ```
- 多选模板后逐个读取：
  - `tracking_template.bmp`
  - `calibrate_config.yaml` 的 `date_poly`
  - `app_settings.appset` 的 `dateEdit_value`
  - 目标字符对应图片
- 不调用 `loadSettingsFromDir()` 循环写 UI。
- 缺文件或字符图片缺失的模板跳过并提示；全部无效则退出多模板模式。
- 验证点：多选后能加载有效模板数量；坏模板能提示并跳过。

## 阶段 3：当前编辑模板下拉框
- 在目标字符区域附近增加一个小下拉框，例如 `wordTemplateComboBox`。
- 只在字库多模板模式下显示或启用。
- 多模板加载成功后，下拉框显示模板文件夹名称。
- 工人切换下拉框时：
  - `dateEdit` 显示该模板的 `profile.targetText`
  - 不影响其他模板
- 单模板模式下保持原 UI 逻辑。
- 验证点：多模板下切换模板，`dateEdit` 能显示不同目标字符。

## 阶段 4：修改 `textsure_btn` 多模板行为
- 单模板：`on_textsure_btn_clicked()` 保持当前逻辑。
- 多模板：
  - 如果正在检测，提示“请先停止检测后再修改模板字符”。
  - 读取当前 `dateEdit`。
  - 只更新下拉框当前选中的 profile。
  - 重新从该模板文件夹加载字符图片到 `profile.digitTemplates`。
  - 把新的 `dateEdit_value` 写回该模板文件夹的 `app_settings.appset`。
  - 其他 profile 不受影响。
- 验证点：修改 A 模板字符不会影响 B 模板；重新选择 A 模板能读回新字符。

## 阶段 5：拆出字库识别共用函数
- 从 `slot_readAndDetect4()` 中抽出 helper，例如：
  ```cpp
  void runWordTemplateDetection(
      cv::Mat *image,
      const DetectionPose &pose,
      const std::vector<cv::Mat> &templates,
      const QString &targetText
  );
  ```
- 单模板调用：
  - `digitTemplates`
  - `ui->dateEdit->toPlainText()`
- 多模板以后调用：
  - `profile.digitTemplates`
  - `profile.targetText`
- OK/NG 统计、绘制、存图、PLC 剔除逻辑沿用现有代码。
- 验证点：此阶段完成后，单模板字库匹配结果必须和原来一致。

## 阶段 6：软触发多模板自动选择
- 给 `DetectionPose` 增加：
  ```cpp
  int wordTemplateProfileIndex = -1;
  ```
- `MyThread` 增加多模板 tracking 缓存和 setter。
- 单模板时仍用原来的 `m_poseMatcher.match()`。
- 多模板时遍历所有 profile 的 `tracking_template.bmp + datePoly`：
  - 调用同一种 `TrackingPoseMatcher::match()`。
  - 取 `pose.score` 最高的有效结果。
  - 写入 `bestPose.wordTemplateProfileIndex`。
  - 仍通过现有信号发送图像和 pose，不新增 Qt 信号。
- Widget 收到图像后：
  - 如果 `wordTemplateProfileIndex >= 0`，用对应 profile 调用阶段 5 的 helper。
  - 否则走原单模板逻辑或判 NG。
- 验证点：软触发下产品 A/B 能自动选择对应模板并用对应目标字符判定。

## 阶段 7：硬触发多模板自动选择
- 在 `CameraThread` 中复制阶段 6 的策略。
- 接口和字段尽量与 `MyThread` 一致。
- Widget 启动硬触发检测时，把多模板 profiles 的 tracking 数据传给 `CameraThread`。
- 验证点：硬触发下产品 A/B 也能自动选择对应模板并判定。

## 阶段 8：收尾和保护逻辑
- 切换 `comboBox_4` 到非字库模式时，清空或停用多模板状态。
- 启动字库检测前检查：
  - 单模板：沿用当前检查。
  - 多模板：至少有 1 个有效 profile。
- 多模板模式下禁止保存“多模板路径”到全局设置。
- 日志中打印本帧选中的模板名称和 score，方便现场排查误选。
- 验证点：模式切换不影响其他检测模式；重启软件后不会自动加载上次多模板。

## 每阶段验证方式
- 不在命令行完整编译。
- 我只做 `rg`、查看 diff、必要的语法结构检查。
- 你在 Qt Creator 中执行：
  1. `Run qmake`
  2. `Clean`
  3. `Rebuild`
  4. `Run`
- 如果报错，你只发错误行号和错误信息；我只按报错位置做小修，不扩大改动范围。
