// 文件作用：本文件用于组装设置、模板、设备、运行时、应用服务和界面对象，建立程序唯一对象图。
// 主要职责：组装设置、模板、设备、运行时、应用服务和界面对象，建立程序唯一对象图。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "startup/application_startup.h"

#include "startup/runtime_guard.h"
#include "startup/single_instance_guard.h"
#include "system_support/crash/windows_crash_handler.h"
#include "system_support/logging/application_logger.h"
#include "system_support/logging/log_categories.h"

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
#include "templates/template_store.h"
#include "system_support/settings/app_settings_store.h"
#include "ui/main_window/main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <memory>
#include <utility>

#include <opencv2/core.hpp>

namespace {

QString settingsLoadStatusName(AppSettingsLoadStatus status)
{
    switch (status) {
    case AppSettingsLoadStatus::Loaded:
        return QStringLiteral("loaded");
    case AppSettingsLoadStatus::FirstRun:
        return QStringLiteral("first_run");
    case AppSettingsLoadStatus::ResetRequired:
        return QStringLiteral("reset_required");
    }
    return QStringLiteral("unknown");
}

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
                           QTranslator *qtTranslator,
                           QTranslator *applicationTranslator)
{
    if (!application || !qtBaseTranslator || !qtTranslator
            || !applicationTranslator) {
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
    if (applicationTranslator->load(
                QStringLiteral(":/Translate_CN.qm"))) {
        application->installTranslator(applicationTranslator);
    }
}

} // namespace

// 函数说明：run 函数执行对应事件或业务处理。
int ApplicationStartup::run(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QTranslator qtBaseTranslator;
    QTranslator qtTranslator;
    QTranslator applicationTranslator;
    installQtTranslations(
                &application, &qtBaseTranslator, &qtTranslator,
                &applicationTranslator);
    {
        QFile themeFile(QStringLiteral(":/qss/app_theme.qss"));
        if (!themeFile.open(QFile::ReadOnly)) {
            qCWarning(logUi).noquote()
                    << "event=ui.style_load_failed path=:/qss/app_theme.qss";
        } else {
            application.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
        }
    }
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<InspectionPresentation>("InspectionPresentation");
    qRegisterMetaType<InspectionFaultSnapshot>("InspectionFaultSnapshot");
    QApplication::setQuitOnLastWindowClosed(true);
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
                    QStringLiteral("警告"),
                    QStringLiteral("程序运行中避免重复打开"));
        return 0;
    }

    const QString applicationDirectory =
            QCoreApplication::applicationDirPath();
    QString loggingError;
    if (!ApplicationLogger::start(&loggingError)) {
        qCWarning(logStartup).noquote()
                << QStringLiteral("event=log.start_failed reason=%1")
                   .arg(loggingError);
    }
    const QString applicationVersion = application.applicationVersion()
            .trimmed().isEmpty()
            ? QStringLiteral("unknown")
            : application.applicationVersion().trimmed();
    qCInfo(logStartup).noquote()
            << QStringLiteral(
                "event=app.start version=%1 build=Release pid=%2 executable=%3")
               .arg(applicationVersion)
               .arg(QCoreApplication::applicationPid())
               .arg(QCoreApplication::applicationFilePath());
    WindowsCrashHandler::install();

    int result = 0;
    {
        const QString applicationDataRoot =
                QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation);
        if (applicationDataRoot.trimmed().isEmpty()) {
            qCCritical(logStartup).noquote()
                    << "event=settings.directory_unavailable";
            QMessageBox::critical(
                        nullptr,
                        QStringLiteral("设置错误"),
                        QStringLiteral("无法确定当前用户的应用数据目录。"));
            qCInfo(logStartup).noquote()
                    << "event=app.stop result=-1 reason=settings_directory_unavailable";
            ApplicationLogger::stop();
            return -1;
        }
        const std::shared_ptr<AppSettingsStore> settingsStore(
                    new AppSettingsStore(applicationDataRoot));
        AppSettings startupSettings;
        AppSettingsLoadStatus settingsStatus =
                AppSettingsLoadStatus::FirstRun;
        AppSettingsStoreError settingsError;
        if (!settingsStore->load(&startupSettings,
                                 &settingsStatus,
                                 &settingsError)) {
            if (settingsError.code
                    == QLatin1String("SETTINGS_RESET_REQUIRED")) {
                startupSettings = AppSettings::defaults();
                if (settingsStore->save(startupSettings,
                                        &settingsError)) {
                    settingsStatus = AppSettingsLoadStatus::Loaded;
                } else {
                    qCCritical(logStartup).noquote()
                            << QStringLiteral(
                                "event=settings.reset_failed code=%1 reason=%2")
                               .arg(settingsError.code,
                                    settingsError.userMessage);
                    QMessageBox::critical(
                                nullptr,
                                QStringLiteral("设置清空失败"),
                                settingsError.userMessage
                                + QStringLiteral("\n\n")
                                + settingsError.code);
                    qCInfo(logStartup).noquote()
                            << "event=app.stop result=-1 reason=settings_reset_failed";
                    ApplicationLogger::stop();
                    return -1;
                }
            } else {
            qCCritical(logStartup).noquote()
                    << QStringLiteral(
                        "event=settings.load_failed code=%1 reason=%2")
                       .arg(settingsError.code, settingsError.userMessage);
            QMessageBox::critical(
                        nullptr,
                        QStringLiteral("设置文件损坏"),
                        settingsError.userMessage
                        + QStringLiteral("\n\n")
                        + settingsError.code);
            qCInfo(logStartup).noquote()
                    << "event=app.stop result=-1 reason=settings_load_failed";
            ApplicationLogger::stop();
            return -1;
            }
        }
        qCInfo(logStartup).noquote()
                << QStringLiteral(
                    "event=settings.loaded status=%1 root=%2 mode=%3 trigger=%4 saveMode=%5")
                   .arg(settingsLoadStatusName(settingsStatus))
                   .arg(QDir::toNativeSeparators(applicationDataRoot))
                   .arg(startupSettings.detectModeId)
                   .arg(startupSettings.triggerEnabled
                        ? QStringLiteral("hardware")
                        : QStringLiteral("software"))
                   .arg(startupSettings.imageSaveModeId);
        const std::shared_ptr<TemplateStore> templateStore(
                    new TemplateStore);
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
                        templateStore));
        const std::shared_ptr<TemplateApplicationService>
                templateService(
                    new TemplateApplicationService(
                        templateStore,
                        barcodeDecoder));
        qCInfo(logStartup).noquote()
                << "event=services.assembled camera=hikvision plc=snap7 ocr=paddle barcode=vendor";
        MainWindow window(
                    inspectionService,
                    runtime.get(),
                    settingsService,
                    templateService);
        window.showMaximized();
        qCInfo(logStartup).noquote() << "event=app.ready";
        result = application.exec();
    }

    qCInfo(logStartup).noquote()
            << QStringLiteral("event=app.stop result=%1").arg(result);
    ApplicationLogger::stop();
    return result;
}
