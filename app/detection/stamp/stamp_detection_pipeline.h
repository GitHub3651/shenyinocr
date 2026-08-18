// 文件作用：本文件用于执行钢印模板定位、字符匹配、重叠检查和最终判定。
// 主要职责：执行钢印模板定位、字符匹配、重叠检查和最终判定。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H
#define DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H

#include "detection/positioning/detection_pose.h"
#include "detection/common/character_template_matcher.h"

#include <QString>

#include <opencv2/core.hpp>

#include <functional>
#include <vector>

// 组件说明：StampOverlapResult 数据结构保存一次操作的结果、状态和错误信息。
struct StampOverlapResult
{
    bool isOk = false;
    std::vector<cv::Point> finalStampPoly;
};

// 组件说明：StampDetectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct StampDetectionResult
{
    int targetCharacterCount = 0;
    int detectedCharacterCount = 0;
    bool characterIsOk = false;
    bool overlapIsOk = false;
    bool isOk = false;
    std::vector<cv::Point> finalStampPoly;
};

// 组件说明：StampDetectionWorkOutput 数据结构集中保存该流程需要的一组相关数据。
struct StampDetectionWorkOutput
{
    DetectionResult detectionResult;
    StampDetectionResult stampResult;
    DetectionPose pose;
    bool roiValid = false;
    bool hasOverlapDetection = false;
};

// 组件说明：StampDetectionPipeline 组件提供对应设备或检测能力的统一实现。
class StampDetectionPipeline
{
public:
    typedef std::function<int(cv::Mat &dateRoi)> CharacterMatchFunction;
    typedef std::function<StampOverlapResult(
            const cv::Mat &sourceImage,
            const std::vector<cv::Point> &datePoly)>
            OverlapDetectionFunction;

    StampDetectionResult detect(
            cv::Mat &dateRoi,
            const cv::Mat &sourceImage,
            const std::vector<cv::Point> &datePoly,
            const QString &targetText,
            const CharacterMatchFunction &matchCharacters,
            const OverlapDetectionFunction &detectOverlap) const;

    StampDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QString &targetText,
            const PreparedCharacterTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent,
            const OverlapDetectionFunction &detectOverlap) const;
};

#endif // DETECTION_STAMP_STAMP_DETECTION_PIPELINE_H
