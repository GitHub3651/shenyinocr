// 文件作用：从全部模板的并行定位结果中选择最高分模板。
#pragma once

#include "detection/positioning/detection_pose.h"

#include <QString>
#include <vector>

struct TemplatePoseSelection
{
    DetectionPose pose;
    QString templateName;
};

class TemplatePoseSelector
{
public:
    bool consider(const DetectionPose &pose,
                  int templateIndex,
                  const QString &templateName,
                  const std::vector<cv::Point2f> &relativeBarcodePoly);

    const TemplatePoseSelection &selection() const;

private:
    TemplatePoseSelection m_selection;
};
