#include "detection/positioning/tracking_pose_matcher.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

bool TrackingPoseMatcher::init(const cv::Mat &trackingTemplateBgr)
{
    clear();
    if (trackingTemplateBgr.empty()) {
        return false;
    }
    if (trackingTemplateBgr.channels() == 3) {
        cv::cvtColor(
                    trackingTemplateBgr,
                    m_templateGray,
                    cv::COLOR_BGR2GRAY);
    } else if (trackingTemplateBgr.channels() == 4) {
        cv::cvtColor(
                    trackingTemplateBgr,
                    m_templateGray,
                    cv::COLOR_BGRA2GRAY);
    } else {
        m_templateGray = trackingTemplateBgr.clone();
    }
    if (m_templateGray.cols < 5 || m_templateGray.rows < 5) {
        clear();
        return false;
    }

    const cv::Point2f center(
                m_templateGray.cols / 2.0f,
                m_templateGray.rows / 2.0f);
    for (int angle = -45; angle <= 45; angle += 2) {
        cv::Mat rotation = cv::getRotationMatrix2D(center, angle, 1.0);
        const double cosine = std::abs(rotation.at<double>(0, 0));
        const double sine = std::abs(rotation.at<double>(0, 1));
        const int width = std::max(
                    1,
                    static_cast<int>(
                        m_templateGray.rows * sine
                        + m_templateGray.cols * cosine));
        const int height = std::max(
                    1,
                    static_cast<int>(
                        m_templateGray.rows * cosine
                        + m_templateGray.cols * sine));
        rotation.at<double>(0, 2) += width / 2.0 - center.x;
        rotation.at<double>(1, 2) += height / 2.0 - center.y;

        cv::Mat rotated;
        cv::warpAffine(
                    m_templateGray,
                    rotated,
                    rotation,
                    cv::Size(width, height),
                    cv::INTER_LINEAR,
                    cv::BORDER_REPLICATE);
        m_rotatedTemplates.push_back(rotated);
        m_rotatedAngles.push_back(angle);

        cv::Mat small;
        cv::resize(
                    rotated,
                    small,
                    cv::Size(
                        std::max(1, static_cast<int>(
                                     rotated.cols * m_pyramidScale)),
                        std::max(1, static_cast<int>(
                                     rotated.rows * m_pyramidScale))),
                    0,
                    0,
                    cv::INTER_AREA);
        m_smallRotatedTemplates.push_back(small);
    }
    return isReady();
}

void TrackingPoseMatcher::clear()
{
    m_templateGray.release();
    m_rotatedTemplates.clear();
    m_rotatedAngles.clear();
    m_smallRotatedTemplates.clear();
}

bool TrackingPoseMatcher::isReady() const
{
    return !m_templateGray.empty()
            && !m_rotatedTemplates.empty()
            && m_rotatedTemplates.size()
               == m_smallRotatedTemplates.size();
}

bool TrackingPoseMatcher::prepareFrame(
    const cv::Mat &frameBgr,
    cv::Mat *gray,
    cv::Mat *smallGray) const
{
    if (!gray || !smallGray || frameBgr.empty()) {
        return false;
    }
    if (frameBgr.channels() == 3) {
        cv::cvtColor(frameBgr, *gray, cv::COLOR_BGR2GRAY);
    } else if (frameBgr.channels() == 4) {
        cv::cvtColor(frameBgr, *gray, cv::COLOR_BGRA2GRAY);
    } else if (frameBgr.channels() == 1) {
        *gray = frameBgr;
    } else {
        return false;
    }
    cv::resize(
                *gray,
                *smallGray,
                cv::Size(),
                m_pyramidScale,
                m_pyramidScale,
                cv::INTER_AREA);
    return !smallGray->empty();
}

DetectionPose TrackingPoseMatcher::match(
    const cv::Mat &frameBgr,
    const std::vector<cv::Point2f> &relativeDatePolygon) const
{
    cv::Mat gray;
    cv::Mat smallGray;
    if (!isReady()
            || !prepareFrame(frameBgr, &gray, &smallGray)) {
        return DetectionPose();
    }
    return matchPrepared(gray, smallGray, relativeDatePolygon);
}

DetectionPose TrackingPoseMatcher::matchPrepared(
    const cv::Mat &gray,
    const cv::Mat &smallGray,
    const std::vector<cv::Point2f> &relativeDatePolygon) const
{
    DetectionPose emptyPose;
    if (!isReady() || gray.empty() || smallGray.empty()) {
        return emptyPose;
    }

    std::vector<int> searchIndexes;
    for (std::size_t i = 0;
         i < m_smallRotatedTemplates.size();
         i += 3U) {
        searchIndexes.push_back(static_cast<int>(i));
    }
    if (!searchIndexes.empty()
            && searchIndexes.back()
               != static_cast<int>(m_smallRotatedTemplates.size() - 1U)) {
        searchIndexes.push_back(
                    static_cast<int>(
                        m_smallRotatedTemplates.size() - 1U));
    }

    double bestSmallScore = -1.0;
    cv::Point bestSmallLocation;
    int bestAngleIndex = -1;
    std::mutex selectionMutex;
    cv::parallel_for_(
                cv::Range(0, static_cast<int>(searchIndexes.size())),
                [&](const cv::Range &range) {
        double localScore = -1.0;
        cv::Point localLocation;
        int localIndex = -1;
        for (int i = range.start; i < range.end; ++i) {
            const int index = searchIndexes[static_cast<std::size_t>(i)];
            const cv::Mat &candidate =
                    m_smallRotatedTemplates[
                    static_cast<std::size_t>(index)];
            if (candidate.rows > smallGray.rows
                    || candidate.cols > smallGray.cols) {
                continue;
            }
            cv::Mat response;
            cv::matchTemplate(
                        smallGray,
                        candidate,
                        response,
                        cv::TM_CCOEFF_NORMED);
            double score = 0.0;
            cv::Point location;
            cv::minMaxLoc(
                        response, nullptr, &score, nullptr, &location);
            if (score > localScore) {
                localScore = score;
                localLocation = location;
                localIndex = index;
            }
        }
        std::lock_guard<std::mutex> lock(selectionMutex);
        if (localScore > bestSmallScore) {
            bestSmallScore = localScore;
            bestSmallLocation = localLocation;
            bestAngleIndex = localIndex;
        }
    });
    if (bestAngleIndex < 0 || bestSmallScore < 0.3) {
        return emptyPose;
    }

    const int neighbors[] = {
        bestAngleIndex - 2,
        bestAngleIndex - 1,
        bestAngleIndex + 1,
        bestAngleIndex + 2
    };
    for (const int index : neighbors) {
        if (index < 0
                || index >= static_cast<int>(
                    m_smallRotatedTemplates.size())) {
            continue;
        }
        const cv::Mat &candidate =
                m_smallRotatedTemplates[static_cast<std::size_t>(index)];
        if (candidate.rows > smallGray.rows
                || candidate.cols > smallGray.cols) {
            continue;
        }
        cv::Mat response;
        cv::matchTemplate(
                    smallGray,
                    candidate,
                    response,
                    cv::TM_CCOEFF_NORMED);
        double score = 0.0;
        cv::Point location;
        cv::minMaxLoc(response, nullptr, &score, nullptr, &location);
        if (score > bestSmallScore) {
            bestSmallScore = score;
            bestSmallLocation = location;
            bestAngleIndex = index;
        }
    }

    const cv::Point roughLocation(
                cvRound(bestSmallLocation.x / m_pyramidScale),
                cvRound(bestSmallLocation.y / m_pyramidScale));
    const cv::Mat &bestTemplate =
            m_rotatedTemplates[static_cast<std::size_t>(bestAngleIndex)];
    const int padding = 40;
    const int startX = std::max(0, roughLocation.x - padding);
    const int startY = std::max(0, roughLocation.y - padding);
    const int width = std::min(
                gray.cols - startX,
                bestTemplate.cols + padding * 2);
    const int height = std::min(
                gray.rows - startY,
                bestTemplate.rows + padding * 2);
    if (width < bestTemplate.cols || height < bestTemplate.rows) {
        return emptyPose;
    }

    cv::Mat response;
    cv::matchTemplate(
                gray(cv::Rect(startX, startY, width, height)),
                bestTemplate,
                response,
                cv::TM_CCOEFF_NORMED);
    double score = 0.0;
    cv::Point location;
    cv::minMaxLoc(response, nullptr, &score, nullptr, &location);
    if (score < 0.3) {
        return emptyPose;
    }
    const cv::Point2f center(
                location.x + startX + bestTemplate.cols / 2.0f,
                location.y + startY + bestTemplate.rows / 2.0f);
    return buildDetectionPose(
                center,
                cv::Size2f(
                    static_cast<float>(m_templateGray.cols),
                    static_cast<float>(m_templateGray.rows)),
                relativeDatePolygon,
                static_cast<float>(
                    m_rotatedAngles[
                    static_cast<std::size_t>(bestAngleIndex)]),
                static_cast<float>(score));
}
