// 文件作用：本文件用于把检测结果、原图和覆盖图形组合成统一的界面呈现数据。
// 主要职责：把检测结果、原图和覆盖图形组合成统一的界面呈现数据。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"
#include "runtime/inspection_presentation.h"

#include <QImage>

#include <functional>

// 组件说明：TissueRollPresentation 数据结构集中传递该流程需要的只读数据或回调。
struct TissueRollPresentation
{
    cv::Point2f center;
    cv::Size2f outerAxes;
    cv::Point2f innerCenter;
    cv::Size2f innerAxes;
};

// 组件说明：InspectionPresentationRenderState 数据结构集中保存该流程需要的一组相关数据。
struct InspectionPresentationRenderState
{
    DetectionPose pose;
    std::vector<DetectionOverlayPolygon> detailPolygons;
    bool stampIsOverlap = false;
    TissueRollPresentation tissueRoll;
    bool hasTissueRoll = false;
};

// 组件说明：InspectionPresentationViewBindings 数据结构集中传递该流程需要的只读数据或回调。
struct InspectionPresentationViewBindings
{
    std::function<void(const QImage &)> showImage;
    std::function<void(DetectionVerdictViewStyle)> showVerdictStyle;
    std::function<void(const QString &)> showVerdictText;
    std::function<void(const QString &)> showRecognitionText;
    std::function<void(const QString &)> showTemplateName;
    std::function<void(int)> showTotalCount;
    std::function<void(int)> showNgCount;
    std::function<void(double)> showPassRate;
    std::function<void(const QString &)> showElapsedText;

    bool isValid() const;
};

// 组件说明：InspectionPresentationRenderer 组件封装本文件中与其名称对应的单一职责。
class InspectionPresentationRenderer
{
public:
    void bindView(const InspectionPresentationViewBindings &bindings);
    bool hasViewBindings() const;

    void clear();
    void clearTransientView();
    bool present(const InspectionPresentation &snapshot);
    bool presentFrame(const QImage &image);
    void presentTotalAndNgCounts(int totalCount, int ngCount);
    void presentNgCount(int ngCount);
    const ProductKey &lastPresentedProductKey() const;

    void installDetectionResult(
        const DetectionResult &result,
        const DetectionPose &pose,
        bool stampIsOverlap = false);
    void installTissueRoll(
        const TissueRollPresentation &roll,
        bool hasTissueRoll);
    void updatePose(const DetectionPose &pose);

    const InspectionPresentationRenderState &state() const;
    QImage renderFrame(
        const cv::Mat &image,
        bool includeTissueOverlay) const;

private:
    InspectionPresentationRenderState m_state;
    InspectionPresentationViewBindings m_viewBindings;
    ProductKey m_lastPresentedProductKey;
};
