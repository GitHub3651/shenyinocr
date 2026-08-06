#pragma once

#include <QMetaType>
#include <QString>
#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>

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

inline cv::Point2f rotateRelativePoint(const cv::Point2f& pt, float angleDeg)
{
    const double rad = angleDeg * CV_PI / 180.0;
    const float cosv = static_cast<float>(std::cos(rad));
    const float sinv = static_cast<float>(std::sin(rad));
    // Match OpenCV's image-coordinate rotation convention used by
    // getRotationMatrix2D/warpAffine: positive angle is counter-clockwise.
    return cv::Point2f(pt.x * cosv + pt.y * sinv, -pt.x * sinv + pt.y * cosv);
}

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

inline std::vector<cv::Point> buildRotatedDatePoly(
    const cv::Point2f& center,
    const std::vector<cv::Point2f>& relDatePoly,
    float angleDeg)
{
    return buildRotatedRelativePoly(center, relDatePoly, angleDeg);
}

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
