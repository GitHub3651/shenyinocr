#pragma once

#include <QImage>
#include <QString>

#include <opencv2/core.hpp>

#include <string>
#include <vector>

class QLabel;

namespace TemplateEditorSupport {

void setLabelTextIfChanged(QLabel *label, const QString &text);
bool parseIntValue(const QString &text, int *value);
bool isSingleTemplateRecipeMode(const QString &modeId);
QImage imageFromBgrMat(const cv::Mat &image);
std::vector<cv::Point> getPolygonROI(const cv::Mat &image, const std::string &windowTitle);
cv::Rect getQuickRectROI(const cv::Mat &image, const std::string &windowTitle);

} // namespace TemplateEditorSupport
