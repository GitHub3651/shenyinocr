#ifndef DETECTION_COMMON_PROFILE_POSE_SELECTOR_H
#define DETECTION_COMMON_PROFILE_POSE_SELECTOR_H

#include "TrackingTypes.h"

#include <QString>

#include <vector>

struct ProfilePoseSelection
{
    DetectionPose pose;
    QString profileName;
};

class ProfilePoseSelector
{
public:
    bool consider(const DetectionPose &pose,
                  int profileIndex,
                  const QString &profileName,
                  const std::vector<cv::Point2f> &relativeBarcodePoly);

    const ProfilePoseSelection &selection() const;

private:
    ProfilePoseSelection m_selection;
};

#endif // DETECTION_COMMON_PROFILE_POSE_SELECTOR_H
