// 文件作用：本文件用于执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 主要职责：执行纸巾模式的图像处理、粗糙度检测和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "tissue_detection_pipeline.h"

// 函数说明：TissueDetectionPipeline 构造函数创建组件并初始化其依赖和初始状态。
TissueDetectionPipeline::TissueDetectionPipeline(
        const TissueRecipeParameters &parameters)
    : m_detector(parameters)
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
    result.modeId = QStringLiteral("tissue_detection");
    result.verdict = tissueResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = tissueResult.rollFound
            ? QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a--");
    result.diagnostic = QString::fromStdString(tissueResult.message);
    result.elapsedMs = static_cast<double>(
                tissueResult.processingTimeMs);

    if (tissueResult.rollFound) {
        const cv::Rect &box = tissueResult.roll.outerBbox;
        DetectionOverlayPolygon polygon;
        polygon.role = QStringLiteral("tissue_roll");
        polygon.points = {
            cv::Point(box.x, box.y),
            cv::Point(box.x + box.width, box.y),
            cv::Point(box.x + box.width, box.y + box.height),
            cv::Point(box.x, box.y + box.height)
        };
        polygon.score = tissueResult.roll.roughnessScore;
        result.overlay.polygons.push_back(polygon);
    }
    return result;
}

// 函数说明：roughnessThreshold 函数实现名称所表示的处理步骤。
double TissueDetectionPipeline::roughnessThreshold() const
{
    return m_detector.roughnessThreshold();
}
