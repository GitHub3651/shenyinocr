#include "runtime/inspection_presentation_renderer.h"

#include <algorithm>
#include <string>

namespace {
cv::Point polygonTopCenter(const std::vector<cv::Point> &polygon)
{
    if (polygon.empty()) {
        return cv::Point();
    }
    if (polygon.size() == 1) {
        return polygon.front();
    }

    std::vector<cv::Point> sorted = polygon;
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const cv::Point &left, const cv::Point &right) {
        if (left.y != right.y) {
            return left.y < right.y;
        }
        return left.x < right.x;
    });
    return cv::Point(
        (sorted[0].x + sorted[1].x) / 2,
        (sorted[0].y + sorted[1].y) / 2);
}

void drawPolygon(
    cv::Mat &image,
    const std::vector<cv::Point> &polygon,
    const cv::Scalar &color,
    int thickness)
{
    if (polygon.empty()) {
        return;
    }
    const std::vector<std::vector<cv::Point> > polygons(1, polygon);
    cv::polylines(image, polygons, true, color, thickness);
}

void drawCharacter(
    cv::Mat &image,
    const DetectionOverlayPolygon &polygon,
    double fontScale,
    int boxThickness,
    int textThickness)
{
    if (polygon.points.size() < 4) {
        return;
    }

    drawPolygon(
        image,
        polygon.points,
        cv::Scalar(0, 255, 0),
        boxThickness);
    if (polygon.score < 0) {
        return;
    }

    const std::string scoreText = std::to_string(
        static_cast<int>(polygon.score * 100));
    int baseline = 0;
    const cv::Size textSize = cv::getTextSize(
        scoreText,
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        textThickness,
        &baseline);
    const cv::Point textAnchor = polygonTopCenter(polygon.points);
    const int textX = std::max(
        0,
        std::min(
            textAnchor.x - textSize.width / 2,
            image.cols - textSize.width));
    const int textY = std::max(
        textSize.height,
        std::min(textAnchor.y - 5, image.rows));

    cv::putText(
        image,
        scoreText,
        cv::Point(textX, textY),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        cv::Scalar(0, 0, 0),
        textThickness + 2);
    cv::putText(
        image,
        scoreText,
        cv::Point(textX, textY),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        cv::Scalar(0, 255, 255),
        textThickness);
}

void drawTissueRoll(
    cv::Mat &image,
    const TissueRollPresentation &roll)
{
    if (image.empty()) {
        return;
    }

    const double dynamicScale = std::max(1.0, image.rows / 800.0);
    const int thickness = std::max(
        2,
        static_cast<int>(2 * dynamicScale));
    const int outerRadius = std::max(
        1,
        cvRound(std::max(roll.outerAxes.width, roll.outerAxes.height)));
    const int innerRadius = std::max(
        1,
        cvRound(std::max(roll.innerAxes.width, roll.innerAxes.height)));

    cv::circle(
        image,
        cv::Point(cvRound(roll.center.x), cvRound(roll.center.y)),
        outerRadius,
        cv::Scalar(0, 255, 255),
        thickness);
    cv::circle(
        image,
        cv::Point(cvRound(roll.innerCenter.x), cvRound(roll.innerCenter.y)),
        innerRadius,
        cv::Scalar(255, 0, 0),
        thickness);
}

void transformPolygonForPose(
    DetectionOverlayPolygon *polygon,
    const cv::Point2f &oldCenter,
    const cv::Point2f &newCenter,
    float angleDifference)
{
    if (!polygon) {
        return;
    }
    for (cv::Point &point : polygon->points) {
        const cv::Point2f relative(
            point.x - oldCenter.x,
            point.y - oldCenter.y);
        const cv::Point2f rotated = rotateRelativePoint(
            relative,
            angleDifference);
        point = cv::Point(
            cvRound(rotated.x + newCenter.x),
            cvRound(rotated.y + newCenter.y));
    }
}
}

bool InspectionPresentationViewBindings::isValid() const
{
    return showImage
            && showVerdictStyle
            && showVerdictText
            && showRecognitionText
            && showTemplateName
            && showTotalCount
            && showNgCount
            && showPassRate
            && showElapsedText;
}

void InspectionPresentationRenderer::bindView(
    const InspectionPresentationViewBindings &bindings)
{
    m_viewBindings = bindings;
}

bool InspectionPresentationRenderer::hasViewBindings() const
{
    return m_viewBindings.isValid();
}

void InspectionPresentationRenderer::clear()
{
    m_state = InspectionPresentationRenderState();
    m_lastPresentedProductKey = ProductKey();
}

void InspectionPresentationRenderer::clearTransientView()
{
    if (!hasViewBindings()) {
        return;
    }
    m_viewBindings.showVerdictText(QString());
    m_viewBindings.showRecognitionText(QString());
    m_viewBindings.showElapsedText(QString());
}

bool InspectionPresentationRenderer::present(
    const InspectionPresentation &snapshot)
{
    if (!hasViewBindings() || !snapshot.isValid()) {
        return false;
    }

    m_viewBindings.showImage(snapshot.image);
    m_viewBindings.showVerdictStyle(snapshot.verdictStyle);
    m_viewBindings.showVerdictText(
                snapshot.verdictStyle == DetectionVerdictViewStyle::Correct
                ? QStringLiteral("\u6b63\u786e")
                : QStringLiteral("\u9519\u8bef"));
    m_viewBindings.showRecognitionText(snapshot.recognitionText);
    if (snapshot.updatesTemplateName) {
        m_viewBindings.showTemplateName(snapshot.templateName);
    }
    m_viewBindings.showTotalCount(snapshot.statistics.totalCount);
    m_viewBindings.showNgCount(snapshot.statistics.ngCount);
    m_viewBindings.showPassRate(
                snapshot.statistics.passRatePercent());
    m_viewBindings.showElapsedText(snapshot.elapsedText);
    m_lastPresentedProductKey = snapshot.productKey;
    return true;
}

bool InspectionPresentationRenderer::presentFrame(const QImage &image)
{
    if (!hasViewBindings() || image.isNull()) {
        return false;
    }
    m_viewBindings.showImage(image);
    return true;
}

void InspectionPresentationRenderer::presentTotalAndNgCounts(
    int totalCount,
    int ngCount)
{
    if (!hasViewBindings()) {
        return;
    }
    m_viewBindings.showTotalCount(totalCount);
    m_viewBindings.showNgCount(ngCount);
}

void InspectionPresentationRenderer::presentNgCount(int ngCount)
{
    if (hasViewBindings()) {
        m_viewBindings.showNgCount(ngCount);
    }
}

const ProductKey &InspectionPresentationRenderer::lastPresentedProductKey() const
{
    return m_lastPresentedProductKey;
}

void InspectionPresentationRenderer::installDetectionResult(
    const DetectionResult &result,
    const DetectionPose &pose,
    bool stampIsOverlap)
{
    clear();
    m_state.pose = pose;
    m_state.stampIsOverlap = stampIsOverlap;
    for (const DetectionOverlayPolygon &polygon : result.overlay.polygons) {
        if (polygon.role == QLatin1String("character")
                || polygon.role == QLatin1String("stamp")) {
            m_state.detailPolygons.push_back(polygon);
        }
    }
}

void InspectionPresentationRenderer::installTissueRoll(
    const TissueRollPresentation &roll,
    bool hasTissueRoll)
{
    clear();
    if (hasTissueRoll) {
        m_state.tissueRoll = roll;
        m_state.hasTissueRoll = true;
    }
}

void InspectionPresentationRenderer::updatePose(
    const DetectionPose &pose)
{
    if (!pose.valid) {
        m_state.detailPolygons.clear();
        m_state.pose = pose;
        return;
    }

    if (m_state.pose.valid && !m_state.detailPolygons.empty()) {
        const float angleDifference =
                pose.angleDeg - m_state.pose.angleDeg;
        for (DetectionOverlayPolygon &polygon : m_state.detailPolygons) {
            transformPolygonForPose(
                &polygon,
                m_state.pose.anchorCenter,
                pose.anchorCenter,
                angleDifference);
        }
    }
    m_state.pose = pose;
}

const InspectionPresentationRenderState &InspectionPresentationRenderer::state() const
{
    return m_state;
}

QImage InspectionPresentationRenderer::renderFrame(
    const cv::Mat &image,
    bool includeTissueOverlay) const
{
    if (image.empty()) {
        return QImage();
    }

    cv::Mat displayImage;
    if (image.channels() == 3) {
        displayImage = image.clone();
    } else if (image.channels() == 1) {
        cv::cvtColor(image, displayImage, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, displayImage, cv::COLOR_BGRA2BGR);
    } else {
        return QImage();
    }

    const double dynamicScale = std::max(
        1.0,
        displayImage.rows / 800.0);
    const double fontScale = 0.4 * dynamicScale;
    const int boxThickness = std::max(
        2,
        static_cast<int>(2 * dynamicScale));
    const int textThickness = std::max(
        1,
        static_cast<int>(1.5 * dynamicScale));

    for (const DetectionOverlayPolygon &polygon :
         m_state.detailPolygons) {
        if (polygon.role == QLatin1String("character")) {
            drawCharacter(
                displayImage,
                polygon,
                fontScale,
                boxThickness,
                textThickness);
        }
    }

    drawPolygon(
        displayImage,
        m_state.pose.trackingPoly,
        cv::Scalar(255, 0, 0),
        boxThickness);
    drawPolygon(
        displayImage,
        m_state.pose.barcodePoly,
        cv::Scalar(0, 255, 255),
        boxThickness);
    drawPolygon(
        displayImage,
        m_state.pose.datePoly,
        cv::Scalar(0, 255, 0),
        boxThickness);

    for (const DetectionOverlayPolygon &polygon :
         m_state.detailPolygons) {
        if (polygon.role == QLatin1String("stamp")) {
            drawPolygon(
                displayImage,
                polygon.points,
                m_state.stampIsOverlap
                ? cv::Scalar(0, 0, 255)
                : cv::Scalar(0, 255, 255),
                boxThickness);
        }
    }

    if (includeTissueOverlay && m_state.hasTissueRoll) {
        drawTissueRoll(displayImage, m_state.tissueRoll);
    }

    const QImage rendered(
        displayImage.data,
        displayImage.cols,
        displayImage.rows,
        static_cast<int>(displayImage.step),
        QImage::Format_RGB888);
    return rendered.rgbSwapped();
}
