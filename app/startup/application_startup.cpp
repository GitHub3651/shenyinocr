#include "startup/application_startup.h"

#include "startup/runtime_guard.h"
#include "startup/single_instance_guard.h"
#include "system_support/crash/windows_crash_handler.h"
#include "system_support/logging/application_logger.h"

#include "devices/barcode/barcode_decoder_adapter.h"
#include "devices/camera/hikvision_camera_device.h"
#include "devices/ocr/paddle_ocr_engine.h"
#include "devices/plc/snap7_plc_device.h"
#include "application/inspection_application_service.h"
#include "application/inspection_runtime_port.h"
#include "application/settings_application_service.h"
#include "runtime/inspection_plc_controller.h"
#include "runtime/inspection_runtime_controller.h"
#include "recipes/recipe_store.h"
#include "system_support/settings/machine_settings_store.h"
#include "widget.h"

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

void showRuntimeGuardExitMessage(const QString &message)
{
    QMessageBox messageBox(
                QMessageBox::Critical,
                QStringLiteral("\u63D0\u793A"),
                message,
                QMessageBox::NoButton);
    messageBox.addButton(
                QStringLiteral("\u786E\u8BA4\u9000\u51FA"),
                QMessageBox::AcceptRole);
    messageBox.setWindowModality(Qt::ApplicationModal);
    messageBox.exec();
}

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
                    QStringLiteral("\u7CFB\u7EDF\u521D\u59CB\u5316\u5931\u8D25\uFF0C"
                                   "\u8BF7\u8054\u7CFB\u4F9B\u5E94\u5546\u3002"));
        return -1;
    }

    QTimer runtimeGuardTimer;
    QObject::connect(
                &runtimeGuardTimer,
                &QTimer::timeout,
                []() {
        if (!RuntimeGuard::check()) {
            showRuntimeGuardExitMessage(
                        QStringLiteral("\u7A0B\u5E8F\u51FA\u9519\uFF0C\u5373\u5C06\u9000\u51FA\uFF0C"
                                       "\u8BF7\u8054\u7CFB\u4F9B\u5E94\u5546\u3002"));
            QCoreApplication::quit();
        }
    });
    runtimeGuardTimer.start(24 * 60 * 60 * 1000);

    SingleInstanceGuard singleInstanceGuard(QStringLiteral("ecust"));
    if (!singleInstanceGuard.acquire()) {
        QMessageBox::warning(
                    nullptr,
                    QStringLiteral("Warning"),
                    QStringLiteral("\u7A0B\u5E8F\u8FD0\u884C\u4E2D\u907F\u514D\u91CD\u590D\u6253\u5F00"));
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
        const std::shared_ptr<InspectionRuntimeController>
                runtimeController(
                    new InspectionRuntimeController(
                        InspectionRuntimeController::RunIdFactory(),
                        plcController));
        const std::shared_ptr<InspectionRuntimePort> runtimePort(
                    new InspectionRuntimePort);
        const std::shared_ptr<InspectionApplicationService>
                inspectionService(
                    new InspectionApplicationService(
                        runtimeController,
                        runtimePort,
                        settingsService,
                        recipeStore));
        const QString ocrConfigPath = QDir(applicationDirectory).filePath(
                    QStringLiteral("config1.txt"));
        const Widget::OcrEngineFactory ocrEngineFactory =
                [ocrConfigPath]() {
            return std::shared_ptr<IOcrEngine>(
                        new PaddleOcrEngine(ocrConfigPath));
        };
        const std::shared_ptr<IBarcodeDecoder> barcodeDecoder(
                    new BarcodeDecoderAdapter);

        Widget window(
                    cameraDevice,
                    ocrEngineFactory,
                    barcodeDecoder,
                    runtimeController,
                    runtimePort,
                    inspectionService,
                    settingsService,
                    recipeStore);
        window.showMaximized();
        result = application.exec();
    }

    cv::destroyAllWindows();
    ApplicationLogger::shutdown();
    return result;
}
