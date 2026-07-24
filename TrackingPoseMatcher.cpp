#include "TrackingPoseMatcher.h"

#include <mutex>

bool TrackingPoseMatcher::init(const cv::Mat& trackingTemplateBgr)
{
    clear();

    if (trackingTemplateBgr.empty()) {
        return false;
    }

    if (trackingTemplateBgr.channels() == 3) {
        cv::cvtColor(trackingTemplateBgr, templateGray, cv::COLOR_BGR2GRAY);
    } else if (trackingTemplateBgr.channels() == 4) {
        cv::cvtColor(trackingTemplateBgr, templateGray, cv::COLOR_BGRA2GRAY);
    } else {
        templateGray = trackingTemplateBgr.clone();
    }

    if (templateGray.empty() || templateGray.cols < 5 || templateGray.rows < 5) {
        clear();
        return false;
    }

    const int h = templateGray.rows;
    const int w = templateGray.cols;
    const cv::Point2f center(w / 2.0f, h / 2.0f);

    for (int angle = -45; angle <= 45; angle += 2) {
        cv::Mat rotation = cv::getRotationMatrix2D(center, angle, 1.0);
        const double cosv = std::abs(rotation.at<double>(0, 0));
        const double sinv = std::abs(rotation.at<double>(0, 1));
        const int rotatedW = std::max(1, static_cast<int>((h * sinv) + (w * cosv)));
        const int rotatedH = std::max(1, static_cast<int>((h * cosv) + (w * sinv)));

        rotation.at<double>(0, 2) += (rotatedW / 2.0) - center.x;
        rotation.at<double>(1, 2) += (rotatedH / 2.0) - center.y;

        cv::Mat rotated;
        cv::warpAffine(templateGray, rotated, rotation, cv::Size(rotatedW, rotatedH), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
        preRotatedTemplates.push_back(rotated);
        preRotatedAngles.push_back(angle);

        cv::Mat rotatedSmall;
        const int smallW = std::max(1, static_cast<int>(rotated.cols * pyramidScale));
        const int smallH = std::max(1, static_cast<int>(rotated.rows * pyramidScale));
        cv::resize(rotated, rotatedSmall, cv::Size(smallW, smallH), 0, 0, cv::INTER_AREA);
        preRotatedTemplatesSmall.push_back(rotatedSmall);
    }

    return !preRotatedTemplates.empty();
}

void TrackingPoseMatcher::clear()
{
    templateGray.release();
    preRotatedTemplates.clear();
    preRotatedAngles.clear();
    preRotatedTemplatesSmall.clear();
}

bool TrackingPoseMatcher::isReady() const
{
    return !templateGray.empty() && !preRotatedTemplates.empty() && !preRotatedTemplatesSmall.empty();
}

bool TrackingPoseMatcher::prepareFrame(
    const cv::Mat& frameBgr,
    cv::Mat* gray,
    cv::Mat* smallGray) const
{
    if (!gray || !smallGray || frameBgr.empty()) {
        return false;
    }

    if (frameBgr.channels() == 3) {
        cv::cvtColor(
                    frameBgr,
                    *gray,
                    cv::COLOR_BGR2GRAY);
    } else if (frameBgr.channels() == 4) {
        cv::cvtColor(
                    frameBgr,
                    *gray,
                    cv::COLOR_BGRA2GRAY);
    } else if (frameBgr.channels() == 1) {
        *gray = frameBgr;
    } else {
        gray->release();
        smallGray->release();
        return false;
    }

    if (gray->empty()) {
        smallGray->release();
        return false;
    }

    cv::resize(
                *gray,
                *smallGray,
                cv::Size(),
                pyramidScale,
                pyramidScale,
                cv::INTER_AREA);
    return !smallGray->empty();
}

DetectionPose TrackingPoseMatcher::match(
    const cv::Mat& frameBgr,
    const std::vector<cv::Point2f>& relDatePoly) const
{
    if (!isReady() || frameBgr.empty()) {
        return DetectionPose();
    }

    cv::Mat gray;
    cv::Mat smallGray;
    if (!prepareFrame(
                frameBgr,
                &gray,
                &smallGray)) {
        return DetectionPose();
    }

    return matchPrepared(
                gray,
                smallGray,
                relDatePoly);
}

DetectionPose TrackingPoseMatcher::matchPrepared(
    const cv::Mat& gray,
    const cv::Mat& smallGray,
    const std::vector<cv::Point2f>& relDatePoly) const
{
    DetectionPose pose;
    if (!isReady()
            || gray.empty()
            || smallGray.empty()) {
        return pose;
    }

    double bestValSmall = -1.0;
    cv::Point bestLocSmall;
    int bestAngleIdx = -1;
    std::mutex lock;

    std::vector<int> searchIndices;
    for (size_t i = 0; i < preRotatedTemplatesSmall.size(); i += 3) {
        searchIndices.push_back(static_cast<int>(i));
    }
    if (!searchIndices.empty() && searchIndices.back() != static_cast<int>(preRotatedTemplatesSmall.size() - 1)) {
        searchIndices.push_back(static_cast<int>(preRotatedTemplatesSmall.size() - 1));
    }

    cv::parallel_for_(cv::Range(0, static_cast<int>(searchIndices.size())), [&](const cv::Range& range) {
        double localBestVal = -1.0;
        cv::Point localBestLoc;
        int localBestIdx = -1;

        for (int i = range.start; i < range.end; ++i) {
            const int realIdx = searchIndices[static_cast<size_t>(i)];
            const cv::Mat& smallTpl = preRotatedTemplatesSmall[static_cast<size_t>(realIdx)];
            if (smallTpl.rows > smallGray.rows || smallTpl.cols > smallGray.cols) {
                continue;
            }

            cv::Mat matchResult;
            cv::matchTemplate(smallGray, smallTpl, matchResult, cv::TM_CCOEFF_NORMED);

            double maxVal = 0.0;
            cv::Point maxLoc;
            cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);
            if (maxVal > localBestVal) {
                localBestVal = maxVal;
                localBestLoc = maxLoc;
                localBestIdx = realIdx;
            }
        }

        std::lock_guard<std::mutex> guard(lock);
        if (localBestVal > bestValSmall) {
            bestValSmall = localBestVal;
            bestLocSmall = localBestLoc;
            bestAngleIdx = localBestIdx;
        }
    });

    if (bestAngleIdx < 0 || bestValSmall < 0.3) {
        return pose;
    }

    const int neighborIndices[] = {bestAngleIdx - 2, bestAngleIdx - 1, bestAngleIdx + 1, bestAngleIdx + 2};
    for (const int idx : neighborIndices) {
        if (idx < 0 || idx >= static_cast<int>(preRotatedTemplatesSmall.size())) {
            continue;
        }

        const cv::Mat& smallTpl = preRotatedTemplatesSmall[static_cast<size_t>(idx)];
        if (smallTpl.rows > smallGray.rows || smallTpl.cols > smallGray.cols) {
            continue;
        }

        cv::Mat matchResult;
        cv::matchTemplate(smallGray, smallTpl, matchResult, cv::TM_CCOEFF_NORMED);

        double maxVal = 0.0;
        cv::Point maxLoc;
        cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);
        if (maxVal > bestValSmall) {
            bestValSmall = maxVal;
            bestLocSmall = maxLoc;
            bestAngleIdx = idx;
        }
    }

    const cv::Point roughLoc(cvRound(bestLocSmall.x / pyramidScale), cvRound(bestLocSmall.y / pyramidScale));
    const cv::Mat& bestTpl = preRotatedTemplates[static_cast<size_t>(bestAngleIdx)];
    const int padding = 40;
    const int sx = std::max(0, roughLoc.x - padding);
    const int sy = std::max(0, roughLoc.y - padding);
    const int sw = std::min(gray.cols - sx, bestTpl.cols + padding * 2);
    const int sh = std::min(gray.rows - sy, bestTpl.rows + padding * 2);

    if (sw < bestTpl.cols || sh < bestTpl.rows) {
        return pose;
    }

    const cv::Rect exactRoi(sx, sy, sw, sh);
    cv::Mat exactArea = gray(exactRoi);
    cv::Mat matchResult;
    cv::matchTemplate(exactArea, bestTpl, matchResult, cv::TM_CCOEFF_NORMED);

    double maxVal = 0.0;
    cv::Point maxLoc;
    cv::minMaxLoc(matchResult, nullptr, &maxVal, nullptr, &maxLoc);
    if (maxVal < 0.3) {
        return pose;
    }

    const cv::Point bestLoc(maxLoc.x + sx, maxLoc.y + sy);
    const cv::Point2f center(bestLoc.x + bestTpl.cols / 2.0f, bestLoc.y + bestTpl.rows / 2.0f);
    return buildDetectionPose(center,
                              cv::Size2f(static_cast<float>(templateGray.cols), static_cast<float>(templateGray.rows)),
                              relDatePoly,
                              static_cast<float>(preRotatedAngles[static_cast<size_t>(bestAngleIdx)]),
                              static_cast<float>(maxVal));
}
