// 文件作用：本文件用于根据当前模板选择定位方式，并输出检测区域对应的位置姿态。
// 主要职责：根据当前模板选择定位方式，并输出检测区域对应的位置姿态。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "detection/positioning/inspection_positioner.h"

#include "detection/common/template_pose_selector.h"

#include <chrono>

// 函数说明：configure 函数更新或应用对应的配置和状态。
bool InspectionPositioner::configure(
    DetectionTrackingKind trackingKind,
    const std::vector<WordTrackingTemplate> &templates,
    const std::vector<cv::Point2f> &singleDatePolygon,
    const cv::Mat &singleTrackingTemplate)
{
    m_trackingKind = trackingKind;
    m_singleDatePolygon = singleDatePolygon;
    m_singleMatcher.clear();
    m_templates.clear();
    if (trackingKind == DetectionTrackingKind::WholeFrame) {
        return true;
    }
    if (trackingKind == DetectionTrackingKind::SingleTemplate) {
        return singleDatePolygon.size() >= 3U
                && m_singleMatcher.init(singleTrackingTemplate);
    }
    for (const WordTrackingTemplate &source : templates) {
        if (source.templateIndex < 0
                || source.trackingTemplate.empty()
                || source.datePoly.size() < 3U) {
            m_templates.clear();
            return false;
        }
        TemplateState state;
        state.name = source.name;
        state.templateIndex = source.templateIndex;
        state.barcodePolygon = source.barcodePoly;
        state.datePolygon = source.datePoly;
        if (!state.matcher.init(source.trackingTemplate)) {
            m_templates.clear();
            return false;
        }
        m_templates.push_back(state);
    }
    return !m_templates.empty();
}

// 函数说明：locate 函数实现名称所表示的处理步骤。
DetectionPose InspectionPositioner::locate(const cv::Mat &image) const
{
    if (m_trackingKind == DetectionTrackingKind::WholeFrame
            || image.empty()) {
        return DetectionPose();
    }
    if (m_trackingKind == DetectionTrackingKind::SingleTemplate) {
        return m_singleMatcher.match(image, m_singleDatePolygon);
    }
    if (m_templates.empty()) {
        return DetectionPose();
    }

    const std::chrono::steady_clock::time_point started =
            std::chrono::steady_clock::now();
    cv::Mat gray;
    cv::Mat smallGray;
    if (!m_templates.front().matcher.prepareFrame(
                image, &gray, &smallGray)) {
        return DetectionPose();
    }
    std::vector<DetectionPose> poses(m_templates.size());
    cv::parallel_for_(
                cv::Range(0, static_cast<int>(m_templates.size())),
                [&](const cv::Range &range) {
        for (int i = range.start; i < range.end; ++i) {
            const TemplateState &state =
                    m_templates[static_cast<std::size_t>(i)];
            poses[static_cast<std::size_t>(i)] =
                    state.matcher.matchPrepared(
                        gray, smallGray, state.datePolygon);
        }
    });

    TemplatePoseSelector selector;
    for (std::size_t i = 0; i < m_templates.size(); ++i) {
        const TemplateState &state = m_templates[i];
        selector.consider(
                    poses[i],
                    state.templateIndex,
                    state.name,
                    state.barcodePolygon);
    }
    DetectionPose pose = selector.selection().pose;
    pose.trackingElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
    return pose;
}
