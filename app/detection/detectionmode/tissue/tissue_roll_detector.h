// 文件作用：本文件用于计算纸巾纹理粗糙度并判断卷料表面是否合格。
// 主要职责：计算纸巾纹理粗糙度并判断卷料表面是否合格。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H
#define DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H

#include <QMetaType>
#include <opencv2/opencv.hpp>
#include <string>

// 组件说明：TissueRollItem 数据结构集中保存该流程需要的一组相关数据。
struct TissueRollItem
{
    bool isOk = true;
    double roughnessScore = 0.0;
    bool roughnessNg = false;
    int ringPixelCount = 0;
    cv::Point2f center;
    cv::Size2f outerAxes;
    cv::Rect outerBbox;
    cv::Point2f innerCenter;
    cv::Size2f innerAxes;
    bool innerHoleFound = false;
    std::string rejectReason;
};

// 组件说明：TissueRollResult 数据结构保存一次操作的结果、状态和错误信息。
struct TissueRollResult
{
    bool isOk = true;
    bool rollFound = false;
    int imageWidth = 0;
    int imageHeight = 0;
    int processingTimeMs = 0;
    TissueRollItem roll;
    std::string message;
};

Q_DECLARE_METATYPE(TissueRollResult)

// 组件说明：TissueRollDetector 组件提供对应设备或检测能力的统一实现。
class TissueRollDetector
{
public:
    explicit TissueRollDetector(double roughnessThreshold);

    TissueRollResult processImage(const cv::Mat& image) const;
    double roughnessThreshold() const;

private:
    double m_roughnessThreshold = 6.0;
};

#endif // DETECTION_TISSUE_TISSUE_ROLL_DETECTOR_H
