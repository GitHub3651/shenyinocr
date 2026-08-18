// 文件作用：本文件用于根据定位结果和Profile模板评分选择当前产品使用的Profile。
// 主要职责：根据定位结果和Profile模板评分选择当前产品使用的Profile。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_COMMON_PROFILE_POSE_SELECTOR_H
#define DETECTION_COMMON_PROFILE_POSE_SELECTOR_H

#include "detection/positioning/detection_pose.h"

#include <QString>

#include <vector>

// 组件说明：ProfilePoseSelection 数据结构集中保存该流程需要的一组相关数据。
struct ProfilePoseSelection
{
    DetectionPose pose;
    QString profileName;
};

// 组件说明：ProfilePoseSelector 组件封装本文件中与其名称对应的单一职责。
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
