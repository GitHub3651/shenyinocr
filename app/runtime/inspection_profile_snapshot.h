#pragma once

#include "devices/barcode/barcode_types.h"
#include "detection/positioning/detection_pose.h"
#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "recipes/product_recipe.h"

#include <QString>

#include <vector>

struct InspectionProfileSource
{
    QString name;
    cv::Mat trackingTemplate;
    std::vector<cv::Point2f> barcodePoly;
    std::vector<cv::Point2f> datePoly;
    QString targetText;
    int imageThreshold =
            RecipeProfile::DefaultImageThresholdPercent;
    std::vector<cv::Mat> digitTemplates;
    std::vector<int> digitTemplateTargetIndexes;
    BarcodeDecodeOptions barcodeOptions;
    BarcodeWordDecodeStrategyState decodeStrategy;
};

struct DetectionModeWorkerProfile
{
    QString templateName;
    QString targetText;
    TemplateMatchPreparedTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    BarcodeDecodeOptions barcodeOptions;
    BarcodeWordDecodeStrategyState decodeStrategy;
};

struct InspectionProfileSnapshot
{
    std::vector<WordTrackingProfile> trackingProfiles;
    std::vector<DetectionModeWorkerProfile> detectionProfiles;

    bool isValid() const
    {
        return !trackingProfiles.empty()
                && trackingProfiles.size() == detectionProfiles.size();
    }

    void clear()
    {
        trackingProfiles.clear();
        detectionProfiles.clear();
    }
};

class InspectionProfileSnapshotBuilder
{
public:
    static InspectionProfileSnapshot create(
        const std::vector<InspectionProfileSource> &sources);
};
