// 文件作用：本文件是五种检测模式的唯一装配边界。
#pragma once

#include "contracts/detection_mode.h"
#include "detection/detection_profile_snapshot.h"
#include "detection/positioning/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
#include "recipes/prepared_recipe.h"

#include <QString>

#include <functional>
#include <memory>

class IBarcodeDecoder;
class IOcrEngine;

struct DetectionRuntimeReadiness
{
    bool ready = true;
    QString errorMessage;
};

struct DetectionRegistryRequest
{
    DetectionMode mode = DetectionMode::Stamp;
    PreparedRecipeSnapshot preparedRecipe;
    DetectionProfileSnapshot profileSnapshot;
    FramePreprocessSettings framePreprocess;
};

typedef std::function<DetectionCompletion(
    const std::shared_ptr<const FrameData> &)> DetectionExecutor;

struct DetectionPipelineCreationResult
{
    DetectionExecutor executor;
    QString errorMessage;
    QString startFailureMessage;
    QString workerLogName;

    bool isAccepted() const
    {
        return static_cast<bool>(executor) && errorMessage.isEmpty();
    }
};

class DetectionRegistry
{
public:
    DetectionRegistry(
        const std::shared_ptr<IOcrEngine> &ocrEngine,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder);

    DetectionRuntimeReadiness prepare(DetectionMode mode) const;
    DetectionPipelineCreationResult create(
        const DetectionRegistryRequest &request) const;

private:
    std::shared_ptr<IOcrEngine> m_ocrEngine;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
};
