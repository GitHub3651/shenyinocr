#include "detection/positioning/inspection_positioner.h"

#include "detection/common/profile_pose_selector.h"

#include <chrono>

bool InspectionPositioner::configure(
    InspectionTrackingKind trackingKind,
    const std::vector<WordTrackingProfile> &profiles,
    const std::vector<cv::Point2f> &singleDatePolygon,
    const cv::Mat &singleTrackingTemplate)
{
    m_trackingKind = trackingKind;
    m_singleDatePolygon = singleDatePolygon;
    m_singleMatcher.clear();
    m_profiles.clear();
    if (trackingKind == InspectionTrackingKind::WholeFrame) {
        return true;
    }
    if (trackingKind == InspectionTrackingKind::SingleTemplate) {
        return singleDatePolygon.size() >= 3U
                && m_singleMatcher.init(singleTrackingTemplate);
    }
    for (const WordTrackingProfile &profile : profiles) {
        if (profile.profileIndex < 0
                || profile.trackingTemplate.empty()
                || profile.datePoly.size() < 3U) {
            m_profiles.clear();
            return false;
        }
        ProfileState state;
        state.name = profile.name;
        state.profileIndex = profile.profileIndex;
        state.barcodePolygon = profile.barcodePoly;
        state.datePolygon = profile.datePoly;
        if (!state.matcher.init(profile.trackingTemplate)) {
            m_profiles.clear();
            return false;
        }
        m_profiles.push_back(state);
    }
    return !m_profiles.empty();
}

DetectionPose InspectionPositioner::locate(const cv::Mat &image) const
{
    if (m_trackingKind == InspectionTrackingKind::WholeFrame
            || image.empty()) {
        return DetectionPose();
    }
    if (m_trackingKind == InspectionTrackingKind::SingleTemplate) {
        return m_singleMatcher.match(image, m_singleDatePolygon);
    }
    if (m_profiles.empty()) {
        return DetectionPose();
    }

    const std::chrono::steady_clock::time_point started =
            std::chrono::steady_clock::now();
    cv::Mat gray;
    cv::Mat smallGray;
    if (!m_profiles.front().matcher.prepareFrame(
                image, &gray, &smallGray)) {
        return DetectionPose();
    }
    std::vector<DetectionPose> poses(m_profiles.size());
    cv::parallel_for_(
                cv::Range(0, static_cast<int>(m_profiles.size())),
                [&](const cv::Range &range) {
        for (int i = range.start; i < range.end; ++i) {
            const ProfileState &state =
                    m_profiles[static_cast<std::size_t>(i)];
            poses[static_cast<std::size_t>(i)] =
                    state.matcher.matchPrepared(
                        gray, smallGray, state.datePolygon);
        }
    });

    ProfilePoseSelector selector;
    for (std::size_t i = 0; i < m_profiles.size(); ++i) {
        const ProfileState &state = m_profiles[i];
        selector.consider(
                    poses[i],
                    state.profileIndex,
                    state.name,
                    state.barcodePolygon);
    }
    DetectionPose pose = selector.selection().pose;
    pose.trackingElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
    return pose;
}

InspectionTrackingKind InspectionPositioner::trackingKind() const
{
    return m_trackingKind;
}
