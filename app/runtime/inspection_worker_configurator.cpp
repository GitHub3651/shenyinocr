#include "runtime/inspection_worker_configurator.h"

#include "CameraThread.h"
#include "mythread.h"

namespace {
template <typename Worker>
void configureTracking(
    Worker *worker,
    const InspectionRunPlan &plan,
    const std::vector<WordTrackingProfile> &wordProfiles,
    const std::vector<cv::Point2f> &datePolygon,
    const cv::Rect2d &trackingBox,
    const cv::Mat &trackingTemplate)
{
    if (!worker) {
        return;
    }

    worker->setBypassTracking(
                plan.trackingKind
                == InspectionTrackingKind::WholeFrame);
    switch (plan.trackingKind) {
    case InspectionTrackingKind::WholeFrame:
        worker->clearPresetBoxes();
        worker->clearWordTemplateTrackingProfiles();
        break;
    case InspectionTrackingKind::WordProfiles:
        worker->clearPresetBoxes();
        worker->setWordTemplateTrackingProfiles(wordProfiles);
        break;
    case InspectionTrackingKind::SingleTemplate:
    default:
        worker->setPresetBoxes(datePolygon, trackingBox);
        worker->setPreloadedTemplate(trackingTemplate);
        break;
    }
}
}

void InspectionWorkerConfigurator::configureSoftwareWorker(
    MyThread *worker,
    const InspectionRunPlan &plan,
    const std::vector<WordTrackingProfile> &wordProfiles,
    const std::vector<cv::Point2f> &datePolygon,
    const cv::Rect2d &trackingBox,
    const cv::Mat &trackingTemplate)
{
    configureTracking(
                worker,
                plan,
                wordProfiles,
                datePolygon,
                trackingBox,
                trackingTemplate);
}

void InspectionWorkerConfigurator::configureHardwareWorker(
    CameraThread *worker,
    const InspectionRunPlan &plan,
    const std::vector<WordTrackingProfile> &wordProfiles,
    const std::vector<cv::Point2f> &datePolygon,
    const cv::Rect2d &trackingBox,
    const cv::Mat &trackingTemplate)
{
    if (worker) {
        worker->setBarcodeWordHardTriggerMode(
                    plan.barcodeWordHardTriggerMode);
    }
    configureTracking(
                worker,
                plan,
                wordProfiles,
                datePolygon,
                trackingBox,
                trackingTemplate);
}
