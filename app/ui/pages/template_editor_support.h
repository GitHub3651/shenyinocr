// 文件作用：本文件用于提供模板编辑页面共用的图像、坐标和模式转换辅助函数。
// 主要职责：提供模板编辑页面共用的图像、坐标和模式转换辅助函数。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QImage>
#include <QString>

#include <opencv2/core.hpp>

#include <string>
#include <vector>

// 组件说明：QLabel 组件负责对应界面区域的显示和用户交互。
class QLabel;

namespace TemplateEditorSupport {

void setLabelTextIfChanged(QLabel *label, const QString &text);
bool parseIntValue(const QString &text, int *value);
bool isSingleTemplateRecipeMode(const QString &modeId);
QImage imageFromBgrMat(const cv::Mat &image);
std::vector<cv::Point> getPolygonROI(const cv::Mat &image, const std::string &windowTitle);
cv::Rect getQuickRectROI(const cv::Mat &image, const std::string &windowTitle);

} // namespace TemplateEditorSupport
