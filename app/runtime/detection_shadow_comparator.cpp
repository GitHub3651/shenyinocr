// 文件作用：本文件用于对比主检测结果和影子检测结果，记录差异而不改变正式判定。
// 主要职责：对比主检测结果和影子检测结果，记录差异而不改变正式判定。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/detection_shadow_comparator.h"

#include <algorithm>
#include <cmath>

namespace {
// 函数说明：appendDifference 函数实现名称所表示的处理步骤。
void appendDifference(
    DetectionShadowComparison *comparison,
    const QString &field)
{
    if (!comparison->differences.contains(field)) {
        comparison->differences.append(field);
    }
}

// 函数说明：coordinateMatches 函数实现名称所表示的处理步骤。
bool coordinateMatches(int primary, int shadow, int tolerance)
{
    const long long delta = static_cast<long long>(primary)
            - static_cast<long long>(shadow);
    const long long limit = static_cast<long long>(tolerance);
    return delta >= -limit && delta <= limit;
}

// 函数说明：scoreMatches 函数实现名称所表示的处理步骤。
bool scoreMatches(double primary, double shadow, double tolerance)
{
    if (std::isnan(primary) || std::isnan(shadow)) {
        return std::isnan(primary) && std::isnan(shadow);
    }
    if (std::isinf(primary) || std::isinf(shadow)) {
        return primary == shadow;
    }
    return std::abs(primary - shadow) <= tolerance;
}

// 函数说明：polygonField 函数实现名称所表示的处理步骤。
QString polygonField(int index, const char *suffix)
{
    return QStringLiteral("overlay[%1].%2")
            .arg(index)
            .arg(QString::fromLatin1(suffix));
}
}

// 函数说明：compare 函数实现名称所表示的处理步骤。
DetectionShadowComparison DetectionShadowComparator::compare(
    const DetectionResult &primary,
    const DetectionResult &shadow,
    const DetectionShadowComparisonOptions &options)
{
    DetectionShadowComparison comparison;
    const int coordinateTolerance = std::max(0, options.coordinateTolerance);
    const double scoreTolerance = std::max(0.0, options.scoreTolerance);

    if (primary.modeId != shadow.modeId) {
        appendDifference(&comparison, QStringLiteral("result.modeId"));
    }
    if (primary.verdict != shadow.verdict) {
        appendDifference(&comparison, QStringLiteral("result.verdict"));
    }
    if (primary.status != shadow.status) {
        appendDifference(&comparison, QStringLiteral("result.status"));
    }
    if (primary.recognizedText != shadow.recognizedText) {
        appendDifference(
                    &comparison,
                    QStringLiteral("result.recognizedText"));
    }
    if (options.compareDiagnostic
            && primary.diagnostic != shadow.diagnostic) {
        appendDifference(
                    &comparison,
                    QStringLiteral("result.diagnostic"));
    }

    const std::vector<DetectionOverlayPolygon> &primaryPolygons =
            primary.overlay.polygons;
    const std::vector<DetectionOverlayPolygon> &shadowPolygons =
            shadow.overlay.polygons;
    if (primaryPolygons.size() != shadowPolygons.size()) {
        appendDifference(&comparison, QStringLiteral("overlay.count"));
    }

    const std::size_t polygonCount = std::min(
                primaryPolygons.size(),
                shadowPolygons.size());
    for (std::size_t polygonIndex = 0;
         polygonIndex < polygonCount;
         ++polygonIndex) {
        const DetectionOverlayPolygon &primaryPolygon =
                primaryPolygons[polygonIndex];
        const DetectionOverlayPolygon &shadowPolygon =
                shadowPolygons[polygonIndex];
        const int index = static_cast<int>(polygonIndex);

        if (primaryPolygon.role != shadowPolygon.role) {
            appendDifference(
                        &comparison,
                        polygonField(index, "role"));
        }
        if (!scoreMatches(
                    primaryPolygon.score,
                    shadowPolygon.score,
                    scoreTolerance)) {
            appendDifference(
                        &comparison,
                        polygonField(index, "score"));
        }
        if (primaryPolygon.points.size()
                != shadowPolygon.points.size()) {
            appendDifference(
                        &comparison,
                        polygonField(index, "points.count"));
        }

        const std::size_t pointCount = std::min(
                    primaryPolygon.points.size(),
                    shadowPolygon.points.size());
        for (std::size_t pointIndex = 0;
             pointIndex < pointCount;
             ++pointIndex) {
            const cv::Point &primaryPoint =
                    primaryPolygon.points[pointIndex];
            const cv::Point &shadowPoint =
                    shadowPolygon.points[pointIndex];
            if (!coordinateMatches(
                        primaryPoint.x,
                        shadowPoint.x,
                        coordinateTolerance)
                    || !coordinateMatches(
                        primaryPoint.y,
                        shadowPoint.y,
                        coordinateTolerance)) {
                appendDifference(
                            &comparison,
                            QStringLiteral("overlay[%1].points[%2]")
                            .arg(index)
                            .arg(static_cast<int>(pointIndex)));
            }
        }
    }

    return comparison;
}
