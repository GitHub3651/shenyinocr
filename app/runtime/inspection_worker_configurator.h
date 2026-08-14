#pragma once

#include "runtime/inspection_run_configuration.h"
#include "TrackingTypes.h"

#include <opencv2/core.hpp>

#include <vector>

class MyThread;
class CameraThread;

class InspectionWorkerConfigurator
{
public:
    static void configureSoftwareWorker(
        MyThread *worker,
        const InspectionRunPlan &plan,
        const std::vector<WordTrackingProfile> &wordProfiles,
        const std::vector<cv::Point2f> &datePolygon,
        const cv::Rect2d &trackingBox,
        const cv::Mat &trackingTemplate);

    static void configureHardwareWorker(
        CameraThread *worker,
        const InspectionRunPlan &plan,
        const std::vector<WordTrackingProfile> &wordProfiles,
        const std::vector<cv::Point2f> &datePolygon,
        const cv::Rect2d &trackingBox,
        const cv::Mat &trackingTemplate);
};
