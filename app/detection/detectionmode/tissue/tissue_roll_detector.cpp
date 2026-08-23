// 文件作用：本文件用于计算纸巾纹理粗糙度并判断卷料表面是否合格。
// 主要职责：计算纸巾纹理粗糙度并判断卷料表面是否合格。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "detection/detectionmode/tissue/tissue_roll_detector.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

// 组件说明：OuterGeometry 保存纸巾卷外圆的中心、半径和包围框。
struct OuterGeometry
{
    cv::Point2f center;
    float radius = 0.0f;
    cv::Rect bbox;
};

// 组件说明：InnerGeometry 保存纸巾卷内孔的检测位置和有效状态。
struct InnerGeometry
{
    cv::Point2f center;
    float radius = 0.0f;
    bool found = false;
};

// 组件说明：OuterRadiusResult 保存外圆半径扫描结果和拒绝原因。
struct OuterRadiusResult
{
    bool found = false;
    float radius = 0.0f;
    std::vector<float> validRadii;
    std::string rejectReason;
};

// 函数说明：clampDouble 把浮点数限制在指定上下界内。
double clampDouble(double value, double low, double high)
{
    return std::max(low, std::min(value, high));
}

// 函数说明：clampRect 把矩形限制在图像有效范围内。
cv::Rect clampRect(const cv::Rect& rect, const cv::Size& bounds)
{
    return rect & cv::Rect(0, 0, bounds.width, bounds.height);
}

// 函数说明：circleBbox 根据圆心和半径生成图像范围内的包围框。
cv::Rect circleBbox(const cv::Point2f& center, float radius, const cv::Size& bounds)
{
    cv::Rect rect(cvRound(center.x - radius),
                  cvRound(center.y - radius),
                  cvRound(radius * 2.0f),
                  cvRound(radius * 2.0f));
    return clampRect(rect, bounds);
}

// 函数说明：ensureBgr 把输入图像统一转换为三通道BGR图像。
cv::Mat ensureBgr(const cv::Mat& image)
{
    if (image.empty()) {
        return cv::Mat();
    }
    if (image.channels() == 3) {
        return image;
    }
    if (image.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }
    if (image.channels() == 4) {
        cv::Mat bgr;
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    return image.clone();
}

// 函数说明：medianFloat 计算浮点数组的中位数。
float medianFloat(std::vector<float> values)
{
    if (values.empty()) {
        return 0.0f;
    }

    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    return values[middle];
}

// 函数说明：percentileUchar 计算灰度图像像素的指定百分位值。
double percentileUchar(const cv::Mat& gray, double percent)
{
    std::vector<uchar> values;
    values.reserve(static_cast<size_t>(gray.rows) * gray.cols);
    for (int y = 0; y < gray.rows; ++y) {
        const uchar* row = gray.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x) {
            values.push_back(row[x]);
        }
    }

    if (values.empty()) {
        return 0.0;
    }

    size_t index = static_cast<size_t>(std::round((percent / 100.0) * (values.size() - 1)));
    index = std::min(index, values.size() - 1);
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

// 函数说明：appendThreshold 规范化候选阈值并加入阈值列表。
void appendThreshold(std::vector<int>& thresholds, double value)
{
    int threshold = static_cast<int>(std::round(clampDouble(value, 20.0, 140.0)));
    thresholds.push_back(threshold);
}

// 函数说明：detectInnerCircle 检测纸巾卷内孔并返回内圆几何信息。
bool detectInnerCircle(const cv::Mat& gray, InnerGeometry& inner, std::string& rejectReason)
{
    const int h = gray.rows;
    const int w = gray.cols;
    const double imageArea = static_cast<double>(h) * w;
    const double minSide = std::max(1, std::min(h, w));

    cv::Mat otsuMask;
    const double otsuThreshold = cv::threshold(gray,
                                               otsuMask,
                                               0,
                                               255,
                                               cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    const double p1 = percentileUchar(gray, 1.0);
    const double p3 = percentileUchar(gray, 3.0);
    const double p5 = percentileUchar(gray, 5.0);
    const double p10 = percentileUchar(gray, 10.0);

    std::vector<int> thresholds;
    appendThreshold(thresholds, otsuThreshold);
    appendThreshold(thresholds, std::min(otsuThreshold, 100.0));
    appendThreshold(thresholds, std::min(otsuThreshold, 90.0));
    appendThreshold(thresholds, std::min(otsuThreshold, 80.0));
    appendThreshold(thresholds, std::min(otsuThreshold, 70.0));
    appendThreshold(thresholds, std::min(otsuThreshold, 60.0));
    appendThreshold(thresholds, p1 + 25.0);
    appendThreshold(thresholds, p3 + 25.0);
    appendThreshold(thresholds, p5 + 25.0);
    appendThreshold(thresholds, p10 + 15.0);
    const int fixedThresholds[] = {35, 40, 45, 50, 60, 70, 80, 90, 100, 110};
    for (int value : fixedThresholds) {
        appendThreshold(thresholds, value);
    }

    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    double bestScore = -1.0;
    cv::Point2f bestCenter;
    float bestRadius = 0.0f;

    for (int threshold : thresholds) {
        cv::Mat dark;
        cv::compare(gray, cv::Scalar(threshold), dark, cv::CMP_LE);

        cv::morphologyEx(dark, dark, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
        cv::morphologyEx(dark, dark, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);

        cv::Mat labels;
        cv::Mat stats;
        cv::Mat centroids;
        const int labelCount = cv::connectedComponentsWithStats(dark, labels, stats, centroids, 8);

        for (int label = 1; label < labelCount; ++label) {
            const int x = stats.at<int>(label, cv::CC_STAT_LEFT);
            const int y = stats.at<int>(label, cv::CC_STAT_TOP);
            const int bw = stats.at<int>(label, cv::CC_STAT_WIDTH);
            const int bh = stats.at<int>(label, cv::CC_STAT_HEIGHT);
            const int area = stats.at<int>(label, cv::CC_STAT_AREA);

            if (area < std::max(500.0, imageArea * 0.00035)) {
                continue;
            }
            if (x <= 2 || y <= 2 || x + bw >= w - 2 || y + bh >= h - 2) {
                continue;
            }

            cv::Mat componentMask;
            cv::compare(labels, cv::Scalar(label), componentMask, cv::CMP_EQ);

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(componentMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (contours.empty()) {
                continue;
            }

            int bestContourIndex = 0;
            double bestContourArea = 0.0;
            for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
                const double contourArea = cv::contourArea(contours[i]);
                if (contourArea > bestContourArea) {
                    bestContourArea = contourArea;
                    bestContourIndex = i;
                }
            }

            const std::vector<cv::Point>& contour = contours[bestContourIndex];
            const double contourArea = cv::contourArea(contour);
            if (contourArea < std::max(500.0, imageArea * 0.00035)) {
                continue;
            }

            const double perimeter = cv::arcLength(contour, true);
            if (perimeter <= 1.0) {
                continue;
            }

            const double circularity = 4.0 * CV_PI * contourArea / (perimeter * perimeter + 1e-6);
            if (circularity < 0.35) {
                continue;
            }

            cv::Point2f center;
            float radius = 0.0f;
            cv::minEnclosingCircle(contour, center, radius);
            if (radius <= 3.0f) {
                continue;
            }
            if (radius < minSide * 0.035 || radius > minSide * 0.32) {
                continue;
            }

            const double fillRatio = contourArea / (CV_PI * radius * radius + 1e-6);
            if (fillRatio < 0.28) {
                continue;
            }

            const double dx = (center.x - w * 0.5) / std::max(w * 0.5, 1.0);
            const double dy = (center.y - h * 0.5) / std::max(h * 0.5, 1.0);
            const double centerDistance = std::sqrt(dx * dx + dy * dy);
            if (centerDistance > 0.70) {
                continue;
            }

            const double centerScore = 1.0 - std::min(centerDistance, 0.70) / 0.70;
            const double meanGray = cv::mean(gray, componentMask)[0];
            const double darknessScore = (255.0 - meanGray) / 255.0;
            const double score = contourArea
                                 * std::max(circularity, 0.01)
                                 * std::max(fillRatio, 0.01)
                                 * (0.5 + centerScore)
                                 * (0.5 + darknessScore);

            if (score > bestScore) {
                bestScore = score;
                bestCenter = center;
                bestRadius = radius;
            }
        }
    }

    if (bestScore <= 0.0) {
        rejectReason = "inner circle not found";
        return false;
    }

    inner.center = bestCenter;
    inner.radius = bestRadius;
    inner.found = true;
    rejectReason.clear();
    return true;
}

// 函数说明：detectOuterRadiusByRadialScan 通过径向扫描估计纸巾卷外圆半径。
OuterRadiusResult detectOuterRadiusByRadialScan(const cv::Mat& gray,
                                                const cv::Point2f& center,
                                                float rInner)
{
    OuterRadiusResult result;
    const int h = gray.rows;
    const int w = gray.cols;

    const float left = center.x;
    const float top = center.y;
    const float right = static_cast<float>(w - 1) - center.x;
    const float bottom = static_cast<float>(h - 1) - center.y;
    const int rMax = static_cast<int>(std::min(std::min(left, top), std::min(right, bottom)));

    const int rStart = static_cast<int>(rInner * 1.6f);
    const int rEnd = static_cast<int>(rMax * 0.98f);
    if (rEnd <= rStart + 30) {
        result.rejectReason = "outer search range too small";
        return result;
    }

    const int angleCount = 240;
    std::vector<float> radii;
    radii.reserve(angleCount);

    const int count = rEnd - rStart + 1;
    cv::Mat mapX(1, count, CV_32F);
    cv::Mat mapY(1, count, CV_32F);
    cv::Mat sampled;
    cv::Mat valuesFloat;
    cv::Mat smooth;
    for (int i = 0; i < angleCount; ++i) {
        const double theta = 2.0 * CV_PI * i / angleCount;
        const double cosT = std::cos(theta);
        const double sinT = std::sin(theta);

        float* mapXRow = mapX.ptr<float>(0);
        float* mapYRow = mapY.ptr<float>(0);
        for (int k = 0; k < count; ++k) {
            const int r = rStart + k;
            mapXRow[k] = static_cast<float>(center.x + r * cosT);
            mapYRow[k] = static_cast<float>(center.y + r * sinT);
        }

        cv::remap(gray, sampled, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        sampled.convertTo(valuesFloat, CV_32F);

        cv::GaussianBlur(valuesFloat, smooth, cv::Size(11, 1), 0);

        const float* smoothRow = smooth.ptr<float>(0);
        int minIndex = -1;
        float minGrad = 0.0f;
        for (int k = 0; k < count - 1; ++k) {
            const float grad = smoothRow[k + 1] - smoothRow[k];
            if (minIndex < 0 || grad < minGrad) {
                minGrad = grad;
                minIndex = k;
            }
        }

        if (minIndex < 0) {
            continue;
        }

        const float edgeStrength = -minGrad;
        if (edgeStrength < 8.0f) {
            continue;
        }

        const int leftIndex = std::max(0, minIndex - 6);
        const int rightIndex = std::min(count - 1, minIndex + 6);
        if (rightIndex <= minIndex) {
            continue;
        }

        double insideSum = 0.0;
        int insideCount = 0;
        for (int k = leftIndex; k <= minIndex; ++k) {
            insideSum += smoothRow[k];
            ++insideCount;
        }

        double outsideSum = 0.0;
        int outsideCount = 0;
        for (int k = minIndex + 1; k <= rightIndex; ++k) {
            outsideSum += smoothRow[k];
            ++outsideCount;
        }

        if (insideCount <= 0 || outsideCount <= 0) {
            continue;
        }

        const double insideMean = insideSum / insideCount;
        const double outsideMean = outsideSum / outsideCount;
        if (insideMean - outsideMean < 10.0) {
            continue;
        }

        const float rCandidate = static_cast<float>(rStart + minIndex);
        radii.push_back(rCandidate);
    }

    if (radii.size() < 50) {
        std::ostringstream reason;
        reason << "too few radial edge points: " << radii.size();
        result.rejectReason = reason.str();
        return result;
    }

    const float binWidth = 2.0f;
    const int binCount = std::max(1, static_cast<int>((rEnd - rStart) / binWidth) + 1);
    std::vector<int> histogram(binCount, 0);
    for (float radius : radii) {
        int binIndex = static_cast<int>((radius - rStart) / binWidth);
        binIndex = std::max(0, std::min(binIndex, binCount - 1));
        ++histogram[binIndex];
    }

    int bestBin = 0;
    for (int i = 1; i < binCount; ++i) {
        if (histogram[i] > histogram[bestBin]) {
            bestBin = i;
        }
    }

    const float rPeak = static_cast<float>(rStart + bestBin * binWidth + binWidth * 0.5f);
    std::vector<float> validRadii;
    validRadii.reserve(radii.size());
    for (float radius : radii) {
        if (std::abs(radius - rPeak) < 25.0f) {
            validRadii.push_back(radius);
        }
    }

    if (validRadii.size() < 30) {
        std::ostringstream reason;
        reason << "outer radius peak not clear: valid=" << validRadii.size()
               << ", raw=" << radii.size()
               << ", r_peak=" << rPeak;
        result.rejectReason = reason.str();
        return result;
    }

    const float rOuter = medianFloat(validRadii);
    result.found = true;
    result.radius = rOuter;
    result.validRadii = validRadii;
    return result;
}

// 函数说明：buildConcentricRingMask 生成内外圆之间的同心环形掩膜。
void buildConcentricRingMask(const cv::Size& size,
                             const cv::Point2f& center,
                             float rInner,
                             float rOuter,
                             cv::Mat& ringMask)
{
    ringMask = cv::Mat::zeros(size, CV_8UC1);
    const cv::Rect bbox = circleBbox(center, rOuter, size);
    if (bbox.empty()) {
        return;
    }

    const float innerRadius2 = rInner * rInner;
    const float outerRadius2 = rOuter * rOuter;
    for (int y = bbox.y; y < bbox.y + bbox.height; ++y) {
        uchar* row = ringMask.ptr<uchar>(y);
        for (int x = bbox.x; x < bbox.x + bbox.width; ++x) {
            const float dx = x - center.x;
            const float dy = y - center.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 >= innerRadius2 && distance2 <= outerRadius2) {
                row[x] = 255;
            }
        }
    }
}

// 函数说明：percentile 计算浮点数组的指定百分位值。
double percentile(std::vector<float>& values, double percent)
{
    if (values.empty()) {
        return 0.0;
    }
    size_t index = static_cast<size_t>(std::round((percent / 100.0) * (values.size() - 1)));
    index = std::min(index, values.size() - 1);
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

// 函数说明：computeRoughnessScore 根据环形区域纹理计算粗糙度分数。
double computeRoughnessScore(const cv::Mat& gray, const cv::Mat& ringMask)
{
    int validCount = cv::countNonZero(ringMask);
    if (validCount <= 0) {
        return 0.0;
    }

    cv::Mat grayFloat;
    gray.convertTo(grayFloat, CV_32F);
    cv::Mat blur;
    cv::GaussianBlur(grayFloat, blur, cv::Size(0, 0), 3.0, 3.0);
    cv::Mat highPass;
    cv::absdiff(grayFloat, blur, highPass);
    cv::Mat laplacian;
    cv::Laplacian(grayFloat, laplacian, CV_32F, 3);
    cv::Mat sobelX;
    cv::Mat sobelY;
    cv::Sobel(grayFloat, sobelX, CV_32F, 1, 0, 3);
    cv::Sobel(grayFloat, sobelY, CV_32F, 0, 1, 3);
    cv::Mat gradient;
    cv::magnitude(sobelX, sobelY, gradient);

    std::vector<float> highPassValues;
    highPassValues.reserve(validCount);
    double lapAbsSum = 0.0;
    double gradientSum = 0.0;
    for (int y = 0; y < gray.rows; ++y) {
        const uchar* maskRow = ringMask.ptr<uchar>(y);
        const float* highRow = highPass.ptr<float>(y);
        const float* lapRow = laplacian.ptr<float>(y);
        const float* gradRow = gradient.ptr<float>(y);
        for (int x = 0; x < gray.cols; ++x) {
            if (maskRow[x] == 0) {
                continue;
            }
            highPassValues.push_back(highRow[x]);
            lapAbsSum += std::abs(lapRow[x]);
            gradientSum += gradRow[x];
        }
    }

    return percentile(highPassValues, 90.0) * 0.70
           + (lapAbsSum / validCount) * 0.018
           + (gradientSum / validCount) * 0.030;
}

// 函数说明：scaleOuterGeometry 把缩放图上的外圆几何还原到原图坐标。
OuterGeometry scaleOuterGeometry(const OuterGeometry& geometry,
                                 double scaleX,
                                 double scaleY,
                                 const cv::Size& originalSize)
{
    const double radiusScale = (scaleX + scaleY) * 0.5;

    OuterGeometry scaled = geometry;
    scaled.center = cv::Point2f(static_cast<float>(geometry.center.x * scaleX),
                                static_cast<float>(geometry.center.y * scaleY));
    scaled.radius = static_cast<float>(geometry.radius * radiusScale);
    scaled.bbox = circleBbox(scaled.center, scaled.radius, originalSize);
    return scaled;
}

// 函数说明：scaleInnerGeometry 把缩放图上的内圆几何还原到原图坐标。
InnerGeometry scaleInnerGeometry(const InnerGeometry& geometry, double scaleX, double scaleY)
{
    const double radiusScale = (scaleX + scaleY) * 0.5;

    InnerGeometry scaled = geometry;
    scaled.center = cv::Point2f(static_cast<float>(geometry.center.x * scaleX),
                                static_cast<float>(geometry.center.y * scaleY));
    scaled.radius = static_cast<float>(geometry.radius * radiusScale);
    scaled.found = geometry.found;
    return scaled;
}

// 函数说明：baseMessage 生成纸巾检测结果使用的基础诊断信息。
std::string baseMessage(int imageWidth,
                        int imageHeight,
                        int detectWidth,
                        int detectHeight,
                        double roughnessThreshold)
{
    std::ostringstream message;
    message << std::fixed << std::setprecision(3);
    message << "image=" << imageWidth << "x" << imageHeight
            << " detectImage=" << detectWidth << "x" << detectHeight
            << " geometry=inner-radial-concentric"
            << " thresholds(rough<" << roughnessThreshold << ")";
    return message.str();
}

} // namespace

// 函数说明：TissueRollDetector 构造函数保存当前纸巾检测参数。
TissueRollDetector::TissueRollDetector(
        double roughnessThreshold)
    : m_roughnessThreshold(roughnessThreshold)
{
}

// 函数说明：processImage 完成纸巾卷定位、粗糙度计算和最终判定。
TissueRollResult TissueRollDetector::processImage(const cv::Mat& image) const
{
    TissueRollResult result;
    cv::Mat bgr = ensureBgr(image);
    if (bgr.empty()) {
        result.isOk = false;
        result.message = "empty image";
        return result;
    }

    result.imageWidth = bgr.cols;
    result.imageHeight = bgr.rows;

    cv::Mat workBgr;
    if (bgr.cols > 2 && bgr.rows > 2) {
        cv::resize(bgr, workBgr, cv::Size((bgr.cols + 1) / 2, (bgr.rows + 1) / 2), 0.0, 0.0, cv::INTER_AREA);
    } else {
        workBgr = bgr;
    }

    const double scaleX = static_cast<double>(bgr.cols) / std::max(workBgr.cols, 1);
    const double scaleY = static_cast<double>(bgr.rows) / std::max(workBgr.rows, 1);
    const double areaScale = scaleX * scaleY;
    const double radiusScale = (scaleX + scaleY) * 0.5;

    cv::Mat gray;
    cv::cvtColor(workBgr, gray, cv::COLOR_BGR2GRAY);

    cv::Mat grayBlur;
    cv::GaussianBlur(gray, grayBlur, cv::Size(5, 5), 0.0);

    const std::string prefix = baseMessage(result.imageWidth,
                                           result.imageHeight,
                                           workBgr.cols,
                                           workBgr.rows,
                                           m_roughnessThreshold);

    InnerGeometry innerSmall;
    std::string rejectReason;
    if (!detectInnerCircle(grayBlur, innerSmall, rejectReason)) {
        result.rollFound = false;
        result.isOk = false;
        result.message = prefix + " rollFound=false reason=" + rejectReason;
        return result;
    }

    OuterRadiusResult radial = detectOuterRadiusByRadialScan(grayBlur, innerSmall.center, innerSmall.radius);
    if (!radial.found) {
        std::ostringstream message;
        message << prefix
                << " rollFound=false reason=" << radial.rejectReason
                << " center=(" << innerSmall.center.x * scaleX << "," << innerSmall.center.y * scaleY << ")"
                << " innerRadius=" << innerSmall.radius * radiusScale;
        result.rollFound = false;
        result.isOk = false;
        result.message = message.str();
        return result;
    }

    OuterGeometry outerSmall;
    outerSmall.center = innerSmall.center;
    outerSmall.radius = radial.radius;
    outerSmall.bbox = circleBbox(outerSmall.center, outerSmall.radius, gray.size());

    cv::Mat ringMask;
    buildConcentricRingMask(gray.size(), innerSmall.center, innerSmall.radius, outerSmall.radius, ringMask);

    const cv::Rect roughnessRoi = outerSmall.bbox;
    const cv::Mat grayRoi = gray(roughnessRoi);
    const cv::Mat ringMaskRoi = ringMask(roughnessRoi);
    const int ringPixelCount = cv::countNonZero(ringMaskRoi);

    const double roughnessScore = computeRoughnessScore(grayRoi, ringMaskRoi);
    const bool roughnessNg =
            roughnessScore >= m_roughnessThreshold;
    const bool rollOk = !roughnessNg;

    OuterGeometry outer = scaleOuterGeometry(outerSmall, scaleX, scaleY, bgr.size());
    InnerGeometry inner = scaleInnerGeometry(innerSmall, scaleX, scaleY);

    TissueRollItem item;
    item.isOk = rollOk;
    item.roughnessScore = roughnessScore;
    item.roughnessNg = roughnessNg;
    item.ringPixelCount = cvRound(ringPixelCount * areaScale);
    item.center = outer.center;
    item.outerAxes = cv::Size2f(outer.radius, outer.radius);
    item.outerBbox = outer.bbox;
    item.innerCenter = inner.center;
    item.innerAxes = cv::Size2f(inner.radius, inner.radius);
    item.innerHoleFound = true;
    if (roughnessNg) {
        item.rejectReason += "roughness ";
    }
    if (item.rejectReason.empty()) {
        item.rejectReason = "none";
    }

    result.rollFound = true;
    result.roll = item;
    result.isOk = rollOk;

    std::ostringstream message;
    message << prefix
            << " rollFound=true"
            << (item.isOk ? " OK" : " NG")
            << " reason=" << item.rejectReason
            << " rough=" << item.roughnessScore
            << " ringPixels=" << item.ringPixelCount
            << " center=(" << item.center.x << "," << item.center.y << ")"
            << " outerRadius=" << item.outerAxes.width
            << " outerBbox=(" << item.outerBbox.x << "," << item.outerBbox.y
            << "," << item.outerBbox.width << "," << item.outerBbox.height << ")"
            << " innerFound=true"
            << " innerCenter=(" << item.innerCenter.x << "," << item.innerCenter.y << ")"
            << " innerRadius=" << item.innerAxes.width;

    result.message = message.str();
    return result;
}

// 函数说明：roughnessThreshold 返回当前检测方案使用的粗糙度阈值。
double TissueRollDetector::roughnessThreshold() const
{
    return m_roughnessThreshold;
}
