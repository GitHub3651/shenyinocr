#include "profile_pose_selector.h"

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

const ProfilePoseSelection &ProfilePoseSelector::selection() const
{
    return m_selection;
}
