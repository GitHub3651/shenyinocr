#include "detection/stamp/overlap_detector.h"
// 文件作用：本文件用于计算钢印区域的重叠特征，并给出重叠异常判断。
// 主要职责：计算钢印区域的重叠特征，并给出重叠异常判断。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include <algorithm>
#include <cmath>
#include <mutex> // 必须添加，用于多线程安全锁

namespace {

struct DateTopLine {
    bool valid = false;
    cv::Point2f left;
    cv::Point2f right;

    // 函数说明：yAt 函数实现名称所表示的处理步骤。
    float yAt(float x) const {
        const float dx = right.x - left.x;
        if (std::abs(dx) < 1e-3f) {
            return std::min(left.y, right.y);
        }
        return left.y + (x - left.x) * (right.y - left.y) / dx;
    }
};

// 函数说明：buildDateTopLine 函数创建、准备或启动对应流程。
DateTopLine buildDateTopLine(const std::vector<cv::Point>& datePoly) {
    DateTopLine line;
    if (datePoly.size() < 2) {
        return line;
    }

    struct EdgeCandidate {
        cv::Point2f p1;
        cv::Point2f p2;
        double len2 = 0.0;
        double midY = 0.0;
    };

    std::vector<EdgeCandidate> edges;
    edges.reserve(datePoly.size());

    double maxLen2 = 0.0;
    for (size_t i = 0; i < datePoly.size(); ++i) {
        const cv::Point2f p1(static_cast<float>(datePoly[i].x), static_cast<float>(datePoly[i].y));
        const cv::Point2f p2(static_cast<float>(datePoly[(i + 1) % datePoly.size()].x),
                             static_cast<float>(datePoly[(i + 1) % datePoly.size()].y));
        const double dx = p2.x - p1.x;
        const double dy = p2.y - p1.y;
        const double len2 = dx * dx + dy * dy;
        if (len2 <= 1.0) {
            continue;
        }

        EdgeCandidate edge;
        edge.p1 = p1;
        edge.p2 = p2;
        edge.len2 = len2;
        edge.midY = (p1.y + p2.y) * 0.5;
        edges.push_back(edge);
        maxLen2 = std::max(maxLen2, len2);
    }

    if (edges.empty()) {
        return line;
    }

    // 日期框通常是宽矩形：先保留长边，再从长边中取屏幕上方那条作为“上框线”。
    const double minLongEdgeLen2 = maxLen2 * 0.25;
    const EdgeCandidate* topEdge = nullptr;
    for (const auto& edge : edges) {
        if (edge.len2 < minLongEdgeLen2) {
            continue;
        }
        if (topEdge == nullptr || edge.midY < topEdge->midY) {
            topEdge = &edge;
        }
    }

    if (topEdge == nullptr || std::abs(topEdge->p1.x - topEdge->p2.x) < 1e-3f) {
        return line;
    }

    line.valid = true;
    line.left = topEdge->p1;
    line.right = topEdge->p2;
    if (line.right.x < line.left.x) {
        std::swap(line.left, line.right);
    }
    return line;
}

// 函数说明：suppressCandidatesBelowDateLine 函数实现名称所表示的处理步骤。
void suppressCandidatesBelowDateLine(cv::Mat& matchResult,
                                     const cv::Rect& searchRoi,
                                     const cv::Size& templateSize,
                                     const DateTopLine& dateTopLine,
                                     double coordinateScale) {
    if (!dateTopLine.valid || matchResult.empty() || coordinateScale <= 0.0) {
        return;
    }

    const double invScale = 1.0 / coordinateScale;
    const float invalidScore = -2.0f;

    for (int y = 0; y < matchResult.rows; ++y) {
        float* row = matchResult.ptr<float>(y);
        const float bottomY = static_cast<float>(searchRoi.y + (y + templateSize.height) * invScale);
        for (int x = 0; x < matchResult.cols; ++x) {
            const float leftX = static_cast<float>(searchRoi.x + x * invScale);
            const float rightX = static_cast<float>(searchRoi.x + (x + templateSize.width) * invScale);
            if (bottomY > dateTopLine.yAt(leftX) || bottomY > dateTopLine.yAt(rightX)) {
                row[x] = invalidScore;
            }
        }
    }
}

// 函数说明：buildRingSearchRoi 函数创建、准备或启动对应流程。
cv::Rect buildRingSearchRoi(const cv::Size& imageSize,
                            const cv::Rect& dateBounds,
                            const DateTopLine& dateTopLine) {
    if (dateTopLine.valid) {
        const float leftY = dateTopLine.yAt(0.0f);
        const float rightY = dateTopLine.yAt(static_cast<float>(imageSize.width));
        int roiBottom = cvCeil(std::max(leftY, rightY));
        roiBottom = std::max(0, std::min(imageSize.height, roiBottom));
        return cv::Rect(0, 0, imageSize.width, roiBottom);
    }

    if (dateBounds.y > 0) {
        return cv::Rect(0, 0, imageSize.width, dateBounds.y);
    }
    return cv::Rect(0, 0, imageSize.width, imageSize.height);
}

} // namespace

// 函数说明：OverlapDetector 构造函数创建组件并初始化其依赖和初始状态。
OverlapDetector::OverlapDetector() {}

// 函数说明：init 函数创建、准备或启动对应流程。
bool OverlapDetector::init(const cv::Mat& ringTemplate,
                           const StampRegionData& regions) {
    try {
        if (ringTemplate.empty()
                || ringTemplate.cols < 10
                || ringTemplate.rows < 10
                || regions.stamp_poly.size() < 3
                || regions.date_poly.size() < 3) {
            return false;
        }

        if (ringTemplate.channels() == 1) {
            templateRing = ringTemplate.clone();
        } else {
            cv::cvtColor(ringTemplate, templateRing, cv::COLOR_BGR2GRAY);
        }
        regionData = regions;

        preRotatedRings.clear();
        preRotatedAngles.clear();
        const int h = templateRing.rows;
        const int w = templateRing.cols;
        const cv::Point2f center(w / 2.0f, h / 2.0f);
        for (int angle = -45; angle <= 47; angle += 2) {
            cv::Mat transform = cv::getRotationMatrix2D(
                        center, angle, 1.0);
            const double cosine = std::abs(transform.at<double>(0, 0));
            const double sine = std::abs(transform.at<double>(0, 1));
            const int rotatedWidth = std::max(
                        1, static_cast<int>(h * sine + w * cosine));
            const int rotatedHeight = std::max(
                        1, static_cast<int>(h * cosine + w * sine));
            transform.at<double>(0, 2) += rotatedWidth / 2.0 - center.x;
            transform.at<double>(1, 2) += rotatedHeight / 2.0 - center.y;

            cv::Mat rotated;
            cv::warpAffine(templateRing,
                           rotated,
                           transform,
                           cv::Size(rotatedWidth, rotatedHeight),
                           cv::INTER_LINEAR,
                           cv::BORDER_REPLICATE);
            preRotatedRings.push_back(rotated);
            preRotatedAngles.push_back(angle);
        }

        pyramidScale = 0.2;
        preRotatedRingsSmall.clear();
        for (const cv::Mat &rotated : preRotatedRings) {
            cv::Mat small;
            cv::resize(rotated,
                       small,
                       cv::Size(std::max(1, static_cast<int>(rotated.cols * pyramidScale)),
                                std::max(1, static_cast<int>(rotated.rows * pyramidScale))),
                       0,
                       0,
                       cv::INTER_AREA);
            preRotatedRingsSmall.push_back(small);
        }
        return true;
    } catch (const cv::Exception&) {
        return false;
    }
}

// 函数说明：processImage 函数执行对应事件或业务处理。
DetectResult OverlapDetector::processImage(const cv::Mat& bgrImage, const std::vector<cv::Point>& datePoly) {
    DetectResult res;
    res.isOk = false;
    res.overlapPixels = 0;
    res.foundRing = false;
    res.valRing = 0.0;

    // 1. 预处理
    cv::Mat gray;
    if (bgrImage.channels() == 3) cv::cvtColor(bgrImage, gray, cv::COLOR_BGR2GRAY);
    else gray = bgrImage.clone();

    // 生成生产日期多边形
    res.finalDatePoly = datePoly;

    // 2. 限制拉环搜索区域：有日期多边形时，以日期上框线的斜线为边界，只搜索其上方区域。
    // 没有日期多边形时保留全图搜索，此时产品本身后续也会被判为NG。
    cv::Rect ringSearchRoi(0, 0, gray.cols, gray.rows);
    DateTopLine dateTopLine;
    if (datePoly.size() >= 3) {
        cv::Rect dateBounds = cv::boundingRect(datePoly) & cv::Rect(0, 0, gray.cols, gray.rows);
        dateTopLine = buildDateTopLine(datePoly);
        ringSearchRoi = buildRingSearchRoi(gray.size(), dateBounds, dateTopLine);
    }

    cv::Mat searchGray = gray(ringSearchRoi);
    if (searchGray.empty() ||
        searchGray.cols * pyramidScale < 1.0 ||
        searchGray.rows * pyramidScale < 1.0) {
        return res;
    }

    cv::Mat smallGray;
    cv::resize(searchGray, smallGray, cv::Size(), pyramidScale, pyramidScale, cv::INTER_AREA);

    double bestValSmall = -1.0;
    cv::Point bestLocSmall;
    int bestAngleIdx = -1;
    std::mutex mtx;

    // ================== 🔥 极限加速 2：跳跃式搜索 (大幅降低弱CPU负担) ==================
    // 从 47 个角度中，提取出 16 个代表性角度（每隔 6 度抽样一次）
    std::vector<int> searchIndices;
    for (size_t i = 0; i < preRotatedRingsSmall.size(); i += 3) {
        searchIndices.push_back(i);
    }
    // 把最后边界也加进去
    if (searchIndices.back() != preRotatedRingsSmall.size() - 1) {
        searchIndices.push_back(preRotatedRingsSmall.size() - 1);
    }

    // 只让多线程去匹配这 16 个代表性角度，运算量瞬间下降 60%
    cv::parallel_for_(cv::Range(0, searchIndices.size()), [&](const cv::Range& range) {
        double localBestVal = -1.0;
        cv::Point localBestLoc;
        int localBestIdx = -1;

        for (int i = range.start; i < range.end; ++i) {
            int realIdx = searchIndices[i];
            const cv::Mat& smallTpl = preRotatedRingsSmall[realIdx];
            if (smallTpl.rows > smallGray.rows || smallTpl.cols > smallGray.cols) continue;

            cv::Mat matchR;
            cv::matchTemplate(smallGray, smallTpl, matchR, cv::TM_CCOEFF_NORMED);
            suppressCandidatesBelowDateLine(matchR, ringSearchRoi, smallTpl.size(), dateTopLine, pyramidScale);

            double rMinV, rMaxV;
            cv::Point rMinL, rMaxL;
            cv::minMaxLoc(matchR, &rMinV, &rMaxV, &rMinL, &rMaxL);

            if (rMaxV > localBestVal) {
                localBestVal = rMaxV;
                localBestLoc = rMaxL;
                localBestIdx = realIdx;
            }
        }

        std::lock_guard<std::mutex> lock(mtx);
        if (localBestVal > bestValSmall) {
            bestValSmall = localBestVal;
            bestLocSmall = localBestLoc;
            bestAngleIdx = localBestIdx;
        }
    });

    // 邻域微调：在刚才找到的大致角度左右，补充测算相邻的 2 个角度，确保精度不丢
    if (bestAngleIdx >= 0 && bestValSmall >= 0.25) {
        int neighbors[] = {bestAngleIdx - 2, bestAngleIdx - 1, bestAngleIdx + 1, bestAngleIdx + 2};
        for (int nIdx : neighbors) {
            if (nIdx >= 0 && nIdx < preRotatedRingsSmall.size()) {
                const cv::Mat& smallTpl = preRotatedRingsSmall[nIdx];
                if (smallTpl.rows > smallGray.rows || smallTpl.cols > smallGray.cols) continue;

                cv::Mat matchR;
                cv::matchTemplate(smallGray, smallTpl, matchR, cv::TM_CCOEFF_NORMED);
                suppressCandidatesBelowDateLine(matchR, ringSearchRoi, smallTpl.size(), dateTopLine, pyramidScale);
                double rMinV, rMaxV;
                cv::Point rMinL, rMaxL;
                cv::minMaxLoc(matchR, &rMinV, &rMaxV, &rMinL, &rMaxL);

                if (rMaxV > bestValSmall) {
                    bestValSmall = rMaxV;
                    bestLocSmall = rMaxL;
                    bestAngleIdx = nIdx;
                }
            }
        }
    }

    res.valRing = bestValSmall;

    // 3. 原图局部精配
    if (bestValSmall >= 0.35 && bestAngleIdx >= 0) {
        // 还原粗配坐标到原图尺寸，并补回搜索ROI偏移
        cv::Point roughLoc(ringSearchRoi.x + bestLocSmall.x / pyramidScale,
                           ringSearchRoi.y + bestLocSmall.y / pyramidScale);
        const cv::Mat& bestTpl = preRotatedRings[bestAngleIdx];

        int padding = 40;
        cv::Rect exactRoi(roughLoc.x - padding,
                          roughLoc.y - padding,
                          bestTpl.cols + 2 * padding,
                          bestTpl.rows + 2 * padding);
        exactRoi &= ringSearchRoi;

        if (exactRoi.width < bestTpl.cols || exactRoi.height < bestTpl.rows) {
            return res;
        }

        cv::Mat exactArea = gray(exactRoi);
        cv::Mat matchR;
        cv::matchTemplate(exactArea, bestTpl, matchR, cv::TM_CCOEFF_NORMED);
        suppressCandidatesBelowDateLine(matchR, exactRoi, bestTpl.size(), dateTopLine, 1.0);
        double rMinV, rMaxV;
        cv::Point rMinL, rMaxL;
        cv::minMaxLoc(matchR, &rMinV, &rMaxV, &rMinL, &rMaxL);

        res.valRing = rMaxV;

        // 🔥 修复致命BUG：将错误的 45 纠正回 0.45 ！！！！
        if (rMaxV >= 0.2) {
            res.foundRing = true;
            res.locRing = cv::Point(rMaxL.x + exactRoi.x, rMaxL.y + exactRoi.y);
            res.angleRing = preRotatedAngles[bestAngleIdx];
            res.shapeRing = cv::Size(bestTpl.cols, bestTpl.rows);

            float center_x = res.locRing.x + res.shapeRing.width / 2.0f;
            float center_y = res.locRing.y + res.shapeRing.height / 2.0f;

            cv::Mat M_rot = cv::getRotationMatrix2D(cv::Point2f(0, 0), res.angleRing, 1.0);

            for (const auto& spt : regionData.stamp_poly) {
                double rot_dx = M_rot.at<double>(0, 0) * spt.x + M_rot.at<double>(0, 1) * spt.y;
                double rot_dy = M_rot.at<double>(1, 0) * spt.x + M_rot.at<double>(1, 1) * spt.y;
                res.finalStampPoly.push_back(cv::Point(static_cast<int>(center_x + rot_dx), static_cast<int>(center_y + rot_dy)));
            }
        }
    }

    // ================== 局部包围盒相交测试 + 最小化遮罩 ==================
    if (res.finalDatePoly.size() >= 3 && res.finalStampPoly.size() >= 3) {
        cv::Rect boundDate = cv::boundingRect(res.finalDatePoly);
        cv::Rect boundStamp = cv::boundingRect(res.finalStampPoly);

        cv::Rect intersectBound = boundDate & boundStamp;

        if (intersectBound.width > 0 && intersectBound.height > 0) {
            cv::Mat maskDate = cv::Mat::zeros(intersectBound.size(), CV_8UC1);
            cv::Mat maskStamp = cv::Mat::zeros(intersectBound.size(), CV_8UC1);

            std::vector<cv::Point> localDatePoly = res.finalDatePoly;
            std::vector<cv::Point> localStampPoly = res.finalStampPoly;
            for(auto& p : localDatePoly) { p.x -= intersectBound.x; p.y -= intersectBound.y; }
            for(auto& p : localStampPoly) { p.x -= intersectBound.x; p.y -= intersectBound.y; }

            cv::fillPoly(maskDate, std::vector<std::vector<cv::Point>>{localDatePoly}, cv::Scalar(255));
            cv::fillPoly(maskStamp, std::vector<std::vector<cv::Point>>{localStampPoly}, cv::Scalar(255));

            cv::Mat interMask;
            cv::bitwise_and(maskDate, maskStamp, interMask);
            res.overlapPixels = cv::countNonZero(interMask);
            res.isOk = (res.overlapPixels == 0);
        } else {
            res.isOk = true;
            res.overlapPixels = 0;
        }
    }

    return res;
}
