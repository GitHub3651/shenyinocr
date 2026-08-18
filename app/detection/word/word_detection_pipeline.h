// 文件作用：本文件用于执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 主要职责：执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_WORD_WORD_DETECTION_PIPELINE_H
#define DETECTION_WORD_WORD_DETECTION_PIPELINE_H

#include "detection/positioning/detection_pose.h"
#include "detection/common/character_template_matcher.h"

#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <functional>
#include <vector>

// 组件说明：WordDetectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct WordDetectionResult
{
    QStringList targetUnits;
    int targetCharacterCount = 0;
    int detectedCharacterCount = 0;
    bool isOk = false;
};

// 组件说明：WordDetectionWorkOutput 数据结构集中保存该流程需要的一组相关数据。
struct WordDetectionWorkOutput
{
    DetectionResult detectionResult;
    WordDetectionResult wordResult;
    DetectionPose pose;
    QString templateName;
    QStringList detectedUnits;
    QStringList matchDetails;
    QStringList missingUnits;
    QString reason;
    bool roiValid = false;
};

// 组件说明：WordDetectionPipeline 组件提供对应设备或检测能力的统一实现。
class WordDetectionPipeline
{
public:
    typedef std::function<int(cv::Mat &dateRoi)> CharacterMatchFunction;

    WordDetectionResult detect(
            cv::Mat &dateRoi,
            const QString &targetText,
            const CharacterMatchFunction &matchCharacters) const;

    WordDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QString &targetText,
            const QString &templateName,
            const PreparedCharacterTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent) const;

    WordDetectionWorkOutput detectPreparedDateRoi(
            const DetectionWorkItem &item,
            const OrientedDateRoi &orientedDateRoi,
            const QString &targetText,
            const QString &templateName,
            const PreparedCharacterTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent) const;
};

#endif // DETECTION_WORD_WORD_DETECTION_PIPELINE_H
