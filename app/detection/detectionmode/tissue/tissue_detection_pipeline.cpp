// 文件作用：本文件用于执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 主要职责：执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "tissue_detection_pipeline.h"
#include "contracts/detection_mode.h"

// 函数说明：TissueDetectionPipeline 构造函数创建组件并初始化其依赖和初始状态。
TissueDetectionPipeline::TissueDetectionPipeline(
        double roughnessThreshold)
    : m_detector(roughnessThreshold)
{
}

// 函数说明：detect 函数执行对应事件或业务处理。
TissueRollResult TissueDetectionPipeline::detect(
        const cv::Mat &image) const
{
    return m_detector.processImage(image);
}

// 函数说明：toDetectionResult 函数校验、转换或恢复对应数据。
DetectionResult TissueDetectionPipeline::toDetectionResult(
    const TissueRollResult &tissueResult)
{
    DetectionResult result;
    result.modeId = detectionModeUiId(DetectionMode::Tissue);
    result.verdict = tissueResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = tissueResult.rollFound
            ? QStringLiteral("粗糙度：%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("粗糙度：--");
    result.presentationText = result.recognizedText;
    result.hasPresentationText = true;
    result.diagnostic = QString::fromStdString(tissueResult.message);

    if (tissueResult.rollFound) {
        DetectionOverlayEllipse outer;
        outer.role = QStringLiteral("tissue_outer");
        outer.center = tissueResult.roll.center;
        outer.axes = tissueResult.roll.outerAxes;
        result.overlay.ellipses.push_back(outer);

        DetectionOverlayEllipse inner;
        inner.role = QStringLiteral("tissue_inner");
        inner.center = tissueResult.roll.innerCenter;
        inner.axes = tissueResult.roll.innerAxes;
        result.overlay.ellipses.push_back(inner);
    }
    return result;
}

// 函数说明：roughnessThreshold 函数实现名称所表示的处理步骤。
double TissueDetectionPipeline::roughnessThreshold() const
{
    return m_detector.roughnessThreshold();
}
