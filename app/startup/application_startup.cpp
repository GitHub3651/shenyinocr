// 文件作用：本文件用于组装设置、配方、设备、运行时、应用服务和界面对象，建立程序唯一对象图。
// 主要职责：组装设置、配方、设备、运行时、应用服务和界面对象，建立程序唯一对象图。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "startup/application_startup.h"

#include "startup/runtime_guard.h"
#include "startup/single_instance_guard.h"
#include "system_support/crash/windows_crash_handler.h"
#include "system_support/logging/application_logger.h"

#include "engines/barcode/vendor/barcode_decoder_adapter.h"
#include "devices/camera/vendor/hikvision_camera_device.h"
#include "engines/ocr/vendor/paddle_ocr_engine.h"
#include "devices/plc/vendor/snap7_plc_device.h"
#include "application/inspection_application_service.h"
#include "application/template_application_service.h"
#include "detection/detection_registry.h"
#include "application/settings_application_service.h"
#include "runtime/inspection_plc_controller.h"
#include "runtime/inspection_runtime.h"
#include "runtime/camera_session.h"
#include "recipes/recipe_store.h"
#include "system_support/settings/machine_settings_store.h"
#include "ui/main_window.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QTranslator>

#include <memory>
#include <utility>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

namespace {

// 函数说明：showRuntimeGuardExitMessage 函数实现名称所表示的处理步骤。
void showRuntimeGuardExitMessage(const QString &message)
{
    QMessageBox messageBox(
                QMessageBox::Critical,
                QStringLiteral("提示"),
                message,
                QMessageBox::NoButton);
    messageBox.addButton(
                QStringLiteral("确认退出"),
                QMessageBox::AcceptRole);
    messageBox.setWindowModality(Qt::ApplicationModal);
    messageBox.exec();
}

// 函数说明：installQtTranslations 函数实现名称所表示的处理步骤。
void installQtTranslations(QApplication *application,
                           QTranslator *qtBaseTranslator,
                           QTranslator *qtTranslator)
{
    if (!application || !qtBaseTranslator || !qtTranslator) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString translationPath = QLibraryInfo::path(
                QLibraryInfo::TranslationsPath);
#else
    const QString translationPath = QLibraryInfo::location(
                QLibraryInfo::TranslationsPath);
#endif
    if (qtBaseTranslator->load(
                QStringLiteral("qtbase_zh_CN"), translationPath)) {
        application->installTranslator(qtBaseTranslator);
    }
    if (qtTranslator->load(
                QStringLiteral("qt_zh_CN"), translationPath)) {
        application->installTranslator(qtTranslator);
    }
}

} // namespace

// 函数说明：run 函数执行对应事件或业务处理。
int ApplicationStartup::run(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QTranslator qtBaseTranslator;
    QTranslator qtTranslator;
    installQtTranslations(
                &application, &qtBaseTranslator, &qtTranslator);
    qRegisterMetaType<cv::Mat>("cv::Mat");
    QApplication::setQuitOnLastWindowClosed(true);
    QObject::connect(
                &application,
                &QCoreApplication::aboutToQuit,
                []() {
        cv::destroyAllWindows();
        QThread::msleep(100);
    });

    if (!RuntimeGuard::check()) {
        showRuntimeGuardExitMessage(
                    QStringLiteral("系统初始化失败，"
                                   "请联系供应商。"));
        return -1;
    }

    QTimer runtimeGuardTimer;
    QObject::connect(
                &runtimeGuardTimer,
                &QTimer::timeout,
                []() {
        if (!RuntimeGuard::check()) {
            showRuntimeGuardExitMessage(
                        QStringLiteral("程序出错，即将退出，"
                                       "请联系供应商。"));
            QCoreApplication::quit();
        }
    });
    runtimeGuardTimer.start(24 * 60 * 60 * 1000);

    SingleInstanceGuard singleInstanceGuard(QStringLiteral("ecust"));
    if (!singleInstanceGuard.acquire()) {
        QMessageBox::warning(
                    nullptr,
                    QStringLiteral("Warning"),
                    QStringLiteral("程序运行中避免重复打开"));
        return 0;
    }

    const QString applicationDirectory =
            QCoreApplication::applicationDirPath();
    if (ApplicationLogger::install(applicationDirectory)) {
        qDebug() << "Logging to"
                 << QDir(ApplicationLogger::logDirectoryPath())
                    .filePath(QStringLiteral("app_log_%1.txt").arg(
                        QDate::currentDate().toString(
                            QStringLiteral("yyyy-MM-dd"))));
    } else {
        qWarning() << "Failed to initialize application logging";
    }
    WindowsCrashHandler::install();

    int result = 0;
    {
        const QString applicationDataRoot =
                QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation);
        if (applicationDataRoot.trimmed().isEmpty()) {
            QMessageBox::critical(
                        nullptr,
                        QStringLiteral("设置错误"),
                        QStringLiteral("无法确定当前用户的应用数据目录。"));
            return -1;
        }
        const std::shared_ptr<MachineSettingsStore> settingsStore(
                    new MachineSettingsStore(applicationDataRoot));
        MachineSettings startupSettings;
        MachineSettingsLoadStatus settingsStatus =
                MachineSettingsLoadStatus::FirstRun;
        MachineSettingsStoreError settingsError;
        if (!settingsStore->load(&startupSettings,
                                 &settingsStatus,
                                 &settingsError)) {
            QMessageBox::critical(
                        nullptr,
                        QStringLiteral("设置文件损坏"),
                        settingsError.userMessage
                        + QStringLiteral("\n\n")
                        + settingsError.code);
            return -1;
        }
        const std::shared_ptr<RecipeStore> recipeStore(
                    new RecipeStore(settingsStore->recipesRootPath()));
        const std::shared_ptr<SettingsApplicationService> settingsService(
                    new SettingsApplicationService(
                        settingsStore, startupSettings));

        const std::shared_ptr<ICameraDevice> cameraDevice(
                    new HikvisionCameraDevice);
        std::unique_ptr<IPlcDevice> plcDevice(new Snap7PlcDevice);
        InspectionPlcAddressMap plcAddresses;
        plcAddresses.triggerModeDb = startupSettings.plcTriggerModeDb;
        plcAddresses.triggerModeOffset =
                startupSettings.plcTriggerModeOffset;
        plcAddresses.resultDb = startupSettings.plcResultDb;
        plcAddresses.resultOffset = startupSettings.plcResultOffset;
        plcAddresses.rejectTimeOffset =
                startupSettings.plcRejectTimeOffset;
        plcAddresses.rejectDistanceOffset =
                startupSettings.plcRejectDistanceOffset;
        plcAddresses.photoTimeOffset =
                startupSettings.plcPhotoTimeOffset;
        plcAddresses.photoDistanceOffset =
                startupSettings.plcPhotoDistanceOffset;
        const std::shared_ptr<InspectionPlcController> plcController(
                    new InspectionPlcController(
                        std::move(plcDevice), plcAddresses));
        const QString ocrConfigPath = QDir(applicationDirectory).filePath(
                    QStringLiteral("config1.txt"));
        const std::shared_ptr<IOcrEngine> ocrEngine(
                    new PaddleOcrEngine(ocrConfigPath));
        const std::shared_ptr<IBarcodeDecoder> barcodeDecoder(
                    new BarcodeDecoderAdapter);
        const std::shared_ptr<DetectionRegistry> detectionRegistry(
                    new DetectionRegistry(ocrEngine, barcodeDecoder));
        const std::shared_ptr<InspectionRuntime>
                runtime(
                    new InspectionRuntime(
                        InspectionRuntime::RunIdFactory(),
                        plcController,
                        detectionRegistry));
        const std::shared_ptr<CameraSession> cameraSession(
                    new CameraSession(
                        cameraDevice,
                        runtime.get()));
        const std::shared_ptr<InspectionApplicationService>
                inspectionService(
                    new InspectionApplicationService(
                        runtime,
                        cameraSession,
                        settingsService,
                        recipeStore));
        const std::shared_ptr<TemplateApplicationService>
                templateService(
                    new TemplateApplicationService(
                        recipeStore,
                         barcodeDecoder,
                         settingsService->editorWorkspacesRootPath()));
        std::unique_ptr<InspectionPage> inspectionPage;
        std::unique_ptr<MachineSettingsPage> machineSettingsPage;
        std::unique_ptr<TemplateEditorPage> templateEditorPage;
        MainWindow window(
                    inspectionService,
                    settingsService,
                    templateService);
        inspectionPage.reset(new InspectionPage(
                    &window,
                    window.inspectionPageViewBindings(),
                    window.templateAttentionTimerForComposition(),
                    window.templateAttentionFlagForComposition(),
                    window.inspectionPageCallbacks()));
        machineSettingsPage.reset(new MachineSettingsPage(
                    window.machineSettingsPageViewBindings(),
                    settingsService.get(),
                    window.settingsEditStateForComposition(),
                    window.selectedDirectoryForComposition(),
                    window.applyingSettingsFlagForComposition(),
                    window.updatingSettingsUiFlagForComposition(),
                    window.machineSettingsPageCallbacks()));
        templateEditorPage.reset(new TemplateEditorPage(
                    window.templateEditorViewBindings(),
                    templateService.get(),
                    inspectionService.get(),
                    settingsService.get(),
                    machineSettingsPage.get(),
                    window.settingsEditStateForComposition(),
                    window.templateEditorPageCallbacks()));
        window.attachPages(
                    inspectionPage.get(),
                    machineSettingsPage.get(),
                    templateEditorPage.get());
        window.showMaximized();
        result = application.exec();
    }

    cv::destroyAllWindows();
    ApplicationLogger::shutdown();
    return result;
}
