// 文件作用：实现最高分模板选择；同分时保留路径列表中较早的模板。
#include "detection/common/template_pose_selector.h"

bool TemplatePoseSelector::consider(
        const DetectionPose &pose,
        int templateIndex,
        const QString &templateName,
        const std::vector<cv::Point2f> &relativeBarcodePoly)
{
    if (!pose.valid
            || (m_selection.pose.valid
                && !(pose.score > m_selection.pose.score))) {
        return false;
    }
    m_selection.pose = pose;
    m_selection.pose.barcodePoly = buildRotatedRelativePoly(
                pose.anchorCenter, relativeBarcodePoly, pose.angleDeg);
    m_selection.pose.wordTemplateIndex = templateIndex;
    m_selection.templateName = templateName;
    return true;
}

const TemplatePoseSelection &TemplatePoseSelector::selection() const
{
    return m_selection;
}
