// 文件作用：本文件用于定义定位结果、跟踪类型和坐标变换所需的轻量数据。
// 主要职责：定义定位结果、跟踪类型和坐标变换所需的轻量数据。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QMetaType>
#include <QDateTime>
#include <QString>
#include <opencv2/opencv.hpp>
#include <cmath>
#include <memory>
#include <vector>

struct ProductKey {
    QString runId;
    quint64 sequence = 0;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return !runId.trimmed().isEmpty() && sequence > 0;
    }
};

struct FrameData {
    ProductKey productKey;
    quint64 frameNumber = 0;
    int cameraIndex = 0;
    QDateTime timestampUtc;
    cv::Mat originalImage;
};

enum class AlgorithmVerdict {
    NotEvaluated,
    Ok,
    Ng
};

enum class DetectionStatus {
    Completed,
    SystemFault,
    Cancelled
};

struct DetectionOverlayPolygon {
    QString role;
    std::vector<cv::Point> points;
    double score = 0.0;
};

struct DetectionOverlay {
    std::vector<DetectionOverlayPolygon> polygons;
};

struct DetectionResult {
    QString modeId;
    AlgorithmVerdict verdict = AlgorithmVerdict::NotEvaluated;
    DetectionStatus status = DetectionStatus::SystemFault;
    QString recognizedText;
    QString diagnostic;
    DetectionOverlay overlay;
    double elapsedMs = 0.0;
};

struct DetectionCompletion {
    std::shared_ptr<const FrameData> frame;
    DetectionResult result;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return frame
                && frame->productKey.isValid()
                && !frame->originalImage.empty();
    }
};

// 函数说明：makeFrameData 函数创建、准备或启动对应流程。
inline std::shared_ptr<const FrameData> makeFrameData(
    const ProductKey &productKey,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc,
    const cv::Mat &originalImage)
{
    std::shared_ptr<FrameData> frame(new FrameData);
    frame->productKey = productKey;
    frame->frameNumber = frameNumber;
    frame->cameraIndex = cameraIndex;
    frame->timestampUtc = timestampUtc;
    frame->originalImage = originalImage.clone();
    return frame;
}

struct DetectionPose {
    bool valid = false;
    std::vector<cv::Point> barcodePoly;
    std::vector<cv::Point> datePoly;
    std::vector<cv::Point> trackingPoly;
    cv::Point2f anchorCenter = cv::Point2f(0.0f, 0.0f);
    float angleDeg = 0.0f;
    float score = 0.0f;
    int wordTemplateProfileIndex = -1;
    double trackingElapsedMs = 0.0;
};

Q_DECLARE_METATYPE(DetectionPose)

struct DetectionWorkItem {
    std::shared_ptr<const FrameData> frame;
    DetectionPose pose;
    bool hasPose = false;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return frame
                && frame->productKey.isValid()
                && !frame->originalImage.empty();
    }
};

// 函数说明：makeDetectionWorkItem 函数创建、准备或启动对应流程。
inline DetectionWorkItem makeDetectionWorkItem(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionPose &pose)
{
    DetectionWorkItem item;
    item.frame = frame;
    item.pose = pose;
    item.hasPose = true;
    return item;
}

struct WordTrackingProfile {
    QString name;
    int profileIndex = -1;
    cv::Mat trackingTemplate;
    std::vector<cv::Point2f> barcodePoly;
    std::vector<cv::Point2f> datePoly;
};

struct OrientedDateRoi {
    bool valid = false;
    cv::Mat rotatedImage;
    std::vector<cv::Point> rotatedDatePoly;
    cv::Rect roi;
    cv::Mat croppedImage;
    cv::Mat rotationMatrix;
    cv::Mat inverseRotationMatrix;
};

struct OrientedBarcodeRoi {
    bool valid = false;
    cv::Mat rotatedImage;
    std::vector<cv::Point> rotatedBarcodePoly;
    cv::Rect roi;
    cv::Mat grayRoi;
    cv::Mat rotationMatrix;
    cv::Mat inverseRotationMatrix;
};

// 函数说明：rotateRelativePoint 函数实现名称所表示的处理步骤。
inline cv::Point2f rotateRelativePoint(const cv::Point2f& pt, float angleDeg)
{
    const double rad = angleDeg * CV_PI / 180.0;
    const float cosv = static_cast<float>(std::cos(rad));
    const float sinv = static_cast<float>(std::sin(rad));
    // Match OpenCV's image-coordinate rotation convention used by
    // getRotationMatrix2D/warpAffine: positive angle is counter-clockwise.
    return cv::Point2f(pt.x * cosv + pt.y * sinv, -pt.x * sinv + pt.y * cosv);
}

// 函数说明：buildRotatedTrackingPoly 函数创建、准备或启动对应流程。
inline std::vector<cv::Point> buildRotatedTrackingPoly(const cv::Point2f& center, const cv::Size2f& size, float angleDeg)
{
    const float halfW = size.width / 2.0f;
    const float halfH = size.height / 2.0f;
    const std::vector<cv::Point2f> relCorners = {
        cv::Point2f(-halfW, -halfH),
        cv::Point2f(halfW, -halfH),
        cv::Point2f(halfW, halfH),
        cv::Point2f(-halfW, halfH)
    };

    std::vector<cv::Point> poly;
    poly.reserve(relCorners.size());
    for (const auto& corner : relCorners) {
        const cv::Point2f rotated = rotateRelativePoint(corner, angleDeg);
        poly.emplace_back(cvRound(center.x + rotated.x), cvRound(center.y + rotated.y));
    }
    return poly;
}

// 函数说明：buildRotatedRelativePoly 函数创建、准备或启动对应流程。
inline std::vector<cv::Point> buildRotatedRelativePoly(
    const cv::Point2f& center,
    const std::vector<cv::Point2f>& relativePoly,
    float angleDeg)
{
    std::vector<cv::Point> poly;
    poly.reserve(relativePoly.size());
    for (const auto& pt : relativePoly) {
        const cv::Point2f rotated = rotateRelativePoint(pt, angleDeg);
        poly.emplace_back(cvRound(center.x + rotated.x), cvRound(center.y + rotated.y));
    }
    return poly;
}

// 函数说明：buildRotatedDatePoly 函数创建、准备或启动对应流程。
inline std::vector<cv::Point> buildRotatedDatePoly(
    const cv::Point2f& center,
    const std::vector<cv::Point2f>& relDatePoly,
    float angleDeg)
{
    return buildRotatedRelativePoly(center, relDatePoly, angleDeg);
}

// 函数说明：buildDetectionPose 函数创建、准备或启动对应流程。
inline DetectionPose buildDetectionPose(const cv::Point2f& center,
                                        const cv::Size2f& trackingSize,
                                        const std::vector<cv::Point2f>& relDatePoly,
                                        float angleDeg,
                                        float score)
{
    DetectionPose pose;
    pose.valid = true;
    pose.anchorCenter = center;
    pose.angleDeg = angleDeg;
    pose.score = score;
    pose.trackingPoly = buildRotatedTrackingPoly(center, trackingSize, angleDeg);
    pose.datePoly = buildRotatedDatePoly(center, relDatePoly, angleDeg);
    return pose;
}
