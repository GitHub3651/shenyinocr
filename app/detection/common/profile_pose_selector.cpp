// 文件作用：本文件用于根据定位结果和Profile模板评分选择当前产品使用的Profile。
// 主要职责：根据定位结果和Profile模板评分选择当前产品使用的Profile。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "profile_pose_selector.h"

// 函数说明：consider 函数实现名称所表示的处理步骤。
bool ProfilePoseSelector::consider(
        const DetectionPose &pose,
        int profileIndex,
        const QString &profileName,
        const std::vector<cv::Point2f> &relativeBarcodePoly)
{
    if (!pose.valid
            || (m_selection.pose.valid
                && !(pose.score > m_selection.pose.score))) {
        return false;
    }

    m_selection.pose = pose;
    m_selection.pose.barcodePoly =
            buildRotatedRelativePoly(
                pose.anchorCenter,
                relativeBarcodePoly,
                pose.angleDeg);
    m_selection.pose.wordTemplateProfileIndex = profileIndex;
    m_selection.profileName = profileName;
    return true;
}

// 函数说明：selection 函数读取、等待或计算对应的数据。
const ProfilePoseSelection &ProfilePoseSelector::selection() const
{
    return m_selection;
}
