#pragma once

#include "detection/common/detection_pose.h"

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
