// widget.h
// 主窗口类 - 视觉检测跟踪系统
// 已修改以兼容简化版线程类（只有1个检测框）

#ifndef WIDGET_H
#define WIDGET_H

#ifndef GLOG_NO_ABBREVIATED_SEVERITIES
#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#endif

#include <QWidget>
#include "omp.h"
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include <chrono>
#include <atomic>
#include <iomanip>
#include <memory>
#include <vector>
#include <QMetaType>
#include <QTranslator>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QStandardItemModel>
#include <string>
#include <QSqlDatabase>
#include <windows.h>
#include <dbt.h>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QTextCodec>
#include <QImageReader>
#include <cstring>
#include <numeric>
#include <QImage>
#include <QThread>
#include <QtWidgets/QMainWindow>
#include "mythread.h"
#include "CameraThread.h"
#include "TissueRollDetector.h"
#include <QImage>
#include "imagelabel.h"
#include "Zhuizong.h"
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/feature.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <QGraphicsScene>
#include <QCloseEvent>
#include <algorithm>
#include <cctype>
#include <regex>
#include <templatematch.h>
#include <Detector.h>
#include "TrackingTypes.h"
#include "appsettingsmanager.h"
#include "recipes/product_recipe.h"
#include "recipes/template_profile_assets.h"
#include "recipes/template_recipe_workflow.h"
#include "devices/barcode/barcode_decoder_adapter.h"
#include "devices/camera/camera_device.h"
#include "devices/ocr/ocr_engine.h"
#include "devices/plc/plc_device.h"
#include "runtime/inspection_start_preflight.h"
#include "runtime/inspection_runtime_controller.h"
#include "ui/presenters/detection_result_presenter.h"

using namespace cv;

namespace Ui {
class Widget;
}

class MultiCameraWidget;
class QLabel;
class QComboBox;
class QFrame;
class QDialog;
class QPushButton;
class ImageSaveService;
class DetectionCompletionController;
struct RecipeSelection;
struct OcrDetectionResult;
struct StampDetectionWorkOutput;
struct WordDetectionWorkOutput;
struct BarcodeWordDetectionWorkOutput;
struct InspectionProfileSnapshot;
class InspectionRuntimeStartTransaction;

/**
 * @brief 主窗口类
 *
 * 功能：
 * - 相机控制和图像采集
 * - OCR识别和检测
 * - 模板匹配和字库匹配
 * - PLC通信
 * - 图像跟踪
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

    // ========== 工具函数 ==========
    std::string qstr2str(const QString qstr);
    QString str2qstr(const std::string str)
    {
        return QString::fromUtf8(str.data());
    }

    // ========== 相机相关 ==========
    int nRet = -1;                      ///< 返回值
    void* m_handle = NULL;              ///< 句柄

    // ========== 公共方法 ==========
    void initWidget();                  ///< 初始化界面
//    void saveImageByMVS(QString savePath, QString format);  ///通过MVS自带的函数保存
    void display(const Mat* image);     ///< 显示图像
    bool saveSettingsToDir(const QString &dirPath);
    bool loadSettingsFromDir(const QString &dirPath, bool showErrorMessage = true);

signals:
    // ========== 信号定义 ==========
    void captureFrame(Mat image);       ///< 捕获帧信号
    void sendDataTo(QString);           ///< 发送数据信号
    void pipei();                       ///< 匹配信号
    void imgshibie(Mat *img);           ///< 识别图像信号
    void jiancestring(String targetstring1);  ///< 检测字符串信号
    void ssim(int s);                   ///< SSIM信号
    void rotate(int angle);             ///< 旋转角度信号
    void choosechannel(int color);      ///< 颜色通道信号

private slots:
    // ========== 界面相关槽函数 ==========
    void showscreen();                  ///< 显示屏幕
    void slot_displayAndDetect(cv::Mat *image);  ///< 显示和检测槽

    // ========== 按钮点击槽函数 ==========
    void on_VideoShoot_clicked();       ///< 单词采集按钮
//    void on_ReShoot_clicked();          ///< 重新采集按钮
    void on_HandwareDetect_clicked();   ///< 相机检测按钮
    void on_CloseCamera_clicked();      ///< 关闭相机按钮
    void onSpinBoxValueChanged(int value); ///< 旋转框值改变
    void on_sureButton_clicked();       ///< 确定按钮

    // ========== 工具函数 ==========
    QString setdatetime();              ///< 设置日期时间


    // ========== PLC相关槽函数 ==========
    void on_plcbtn_clicked();           ///< PLC按钮
    void on_ConnectpushButton_clicked(); ///< 连接PLC按钮
    void on_DisconnectpushButton_clicked(); ///< 断开PLC按钮
    void on_WriteVDpushButton_clicked(); ///< 写入VD按钮
    void rightremove();                 ///< 合格移除
    void wrongremove();                 ///< 不合格移除

    // ========== 其他槽函数 ==========
    void on_textsure_btn_clicked();     ///< 文本确定按钮
    void on_batchTextsure_btn_clicked(); ///< 批量文本确定按钮
    void on_batchImageThresholdButton_clicked(); ///< 批量设置字库模板图像阈值
    Mat* QImageToMat(const QImage &image);    ///< QImage转Mat
    void on_cancel_clicked();           ///< 取消按钮
    void slot_clearResultLabel();       ///< 清除结果标签
    void closeEvent(QCloseEvent *event) override; ///< 关闭事件
    void slot_saveBoxesFromThread(DetectionPose pose); ///接收运行时姿态

    // ========== 模式和功能按钮 ==========
    void on_plcmodebtn_clicked();       ///< PLC模式按钮
    void on_eliminatebutton_clicked();  ///< 消除按钮

    // ========== 其他按钮 ==========
    void on_pushButton_3_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_browseImageSavePath_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_cut_cancelButton_2_clicked();
    void on_cut_cancelButton_3_clicked();


    void on_pushButton_10_clicked();

    void on_pushButton_7_clicked();


    void on_pushButton_12_clicked();

    void on_pushButton_tissueRoughnessThreshold_clicked();

    void on_MultiCameraMode_clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    cv::Mat m_loadedTrackingTemplate;
    void showParameterInfo(const QString &title, const QString &message);
    void showParameterInfoWithRedWarning(const QString &title,
                                         const QString &message,
                                         const QString &warningMessage);
    void showParameterInfoAsError(const QString &title, const QString &message);
    void showParameterWarning(const QString &title, const QString &message);
    void showParameterCritical(const QString &title, const QString &message);
    bool queryCameraExposureRange(int *minimumValue,
                                  int *maximumValue,
                                  double *currentValue,
                                  QString *errorMessage);
    bool applyCameraExposureValue(int exposureValue, QString *errorMessage);
    bool applySavedCameraExposure(QString *adjustmentMessage, QString *errorMessage);
    bool applyCameraExposureFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyCameraGainFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyCameraHardwareSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyRuntimeThreadSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    enum class HardwareDependency {
        None,
        Camera,
        PlcConnection,
        PlcRuntime
    };
    struct GlobalSettingBinding {
        QString key;
        QWidget *editor = nullptr;
        QLabel *label = nullptr;
        QString originalLabelText;
        bool requireApply = false;
        bool dirty = false;
        HardwareDependency hardwareDependency = HardwareDependency::None;
    };
    struct HardwareActionBinding {
        QWidget *control = nullptr;
        HardwareDependency hardwareDependency = HardwareDependency::None;
    };
    void setupGlobalSettingBindings();
    void registerGlobalSetting(const QString &key,
                               QWidget *editor,
                               QLabel *label,
                               bool requireApply,
                               HardwareDependency hardwareDependency = HardwareDependency::None);
    void registerHardwareAction(QWidget *control,
                                HardwareDependency hardwareDependency);
    void setupNumericInputValidators();
    bool isGlobalSettingDirtyByValue(const QString &key) const;
    void refreshGlobalSettingDirty(const QString &key);
    void refreshGlobalSettingsDirty(const QStringList &keys);
    void refreshAllGlobalSettingDirty();
    void markGlobalSettingDirty(const QString &key);
    void clearGlobalSettingDirty(const QString &key);
    void clearGlobalSettingsDirty(const QStringList &keys);
    void clearAllGlobalSettingDirty();
    void updateGlobalSettingDirtyUi(const QString &key);
    void updateAppliedGlobalSettingFromUi(const QString &key);
    void updateAppliedGlobalSettingsFromUi(const QStringList &keys);
    void syncImmediateGlobalSettingsFromUi();
    QStringList dirtyGlobalSettingNames() const;
    QStringList dirtyTemplateSettingNames() const;
    QStringList dirtySettingNames() const;
    bool hasDirtySettings() const;
    QString dirtySettingsMessage() const;
    void restoreUnappliedSettingsFromApplied();
    void setupTemplatePrivateSettingDirtyTracking();
    void refreshTemplateTargetTextDirty();
    void refreshTemplateImageThresholdDirty();
    void refreshTemplatePrivateSettingDirty();
    void markTemplateTargetTextDirty();
    void markTemplateImageThresholdDirty();
    void clearTemplateTargetTextDirty();
    void clearTemplateImageThresholdDirty();
    void clearTemplatePrivateSettingDirty();
    void updateTemplatePrivateSettingDirtyUi();
    void restoreCameraHardwareUiFromApplied();
    void restorePlcRunUiFromApplied();
    QString hardwareDisabledStyle(QWidget *widget) const;
    void setHardwareControlEnabled(QWidget *widget,
                                   bool enabled,
                                   const QString &disabledReason,
                                   bool showDisabledReason = true);
    void updateHardwareParameterUiEnabled();
    void updateCurrentTemplateName();
    void updateSaveDirButtonText();
    void updateImageSaveOptionsVisibility();
    void updateTissueRoughnessUiVisibility();
    bool applyTissueRoughnessThresholdFromUi(bool showMessage);
    void updateTissueRecipeParameters(double roughnessThreshold);
    void setupTemplateGuide();
    void adjustTemplateGuideHeight();
    void showTemplateGuideForCurrentMode();
    void hideTemplateGuide();
    void updateImageDisplayStatusText(const QString &body);
    void updateTemplateGuideText(const QString &title, const QString &body);
    void handleTemplateGuideEvent(const QString &eventName, int pointCount);
    void setupManualCharacterCropUi();
    void showManualCharacterTemplateCropDialog();
    void showStampCharacterTemplateCropDialog();
    void showPublishedRecipeCharacterTemplateCropDialog(int profileIndex);
    void setupSoftwareSettingsPage();
    void clearCurrentSoftwareData();
    void restoreDefaultGlobalSettings();
    QString detectModeIdForIndex(int index) const;
    QString currentDetectModeId() const;
    QStringList currentTemplatePathsForMode(const QString &modeId) const;
    void storeCurrentTemplatePathsForMode(const QString &modeId);
    void restoreTemplatesForMode(const QString &modeId, bool showMessage);
    bool activatePublishedWordRecipe(const QString &recipeId,
                                     const QString &modeId,
                                     bool showErrorMessage,
                                     QStringList *pendingMessages,
                                     QString *errorMessage);
    bool activatePublishedSingleTemplateRecipe(const QString &recipeId,
                                               const QString &modeId,
                                               bool showErrorMessage,
                                               QString *errorMessage);
    void connectTemplatePreviewSignals(MyThread *thread);
    bool startTemplatePreview();
    bool freezeTemplatePreview();
    bool stopTemplatePreview(int waitTimeMs = 1500);
    void resetTemplateCaptureState();
    bool hasTemplateDrawingSelection() const;
    void updateOperationUiState();
    void clearInspectionTransientDisplay();
    bool hasRunningInspectionThread() const;
    void handleStreamingFrame(const cv::Mat &image);
    bool shouldSuppressStreamingFrame() const;
    qint64 resultPresentationElapsedMs(
        const DetectionCompletion &completion) const;
    void connectSoftwarePreviewSignals(MyThread *thread);
    void connectHardwarePreviewSignals(CameraThread *thread);
    void connectSoftwareDetectionSignals(MyThread *thread);
    void connectHardwareDetectionSignals(CameraThread *thread);
    bool postSoftwareDetectionUiWork(
        const UiCompletionMailbox::Work &work);
    DetectionWorker::FailureConsumer detectionWorkerFailureConsumer();
    bool installDetectionWorker(
        InspectionRuntimeStartTransaction &startTransaction,
        int modeIndex,
        const std::shared_ptr<DetectionWorker> &worker,
        const QString &startFailureMessage,
        const QString &workerLogName,
        QString *errorMessage);
    bool startDetectionWorkerForMode(
        InspectionRuntimeStartTransaction &startTransaction,
        int modeIndex,
        const InspectionProfileSnapshot &profileSnapshot,
        QString *errorMessage);
    void submitSoftwareDetectionFrame(const cv::Mat &image);
    void submitSoftwarePositionedDetectionFrame(
        const cv::Mat &image,
        const DetectionPose &pose);
    void handleSoftwareTissueCompletion(
        const DetectionCompletion &completion,
        const TissueRollResult &tissueResult);
    void handleSoftwareOcrCompletion(
        const DetectionCompletion &completion,
        const DetectionPose &pose);
    void handleSoftwareStampCompletion(
        const DetectionCompletion &completion,
        const StampDetectionWorkOutput &output);
    void handleSoftwareWordCompletion(
        const DetectionCompletion &completion,
        const WordDetectionWorkOutput &output);
    void handleSoftwareBarcodeWordCompletion(
        const DetectionCompletion &completion,
        const BarcodeWordDetectionWorkOutput &output);
    void finalizeOcrResult(
        const DetectionPose &pose,
        const OcrDetectionResult &ocrResult,
        const DetectionCompletion &acceptedCompletion,
        double elapsedMs);
    void finalizeTissueResult(
        const TissueRollResult &tissueResult,
        const DetectionCompletion &acceptedCompletion);
    void finalizeSoftwareStampResult(
        const StampDetectionWorkOutput &output,
        const DetectionCompletion &acceptedCompletion);
    void finalizeSoftwareWordResult(
        const WordDetectionWorkOutput &output,
        const DetectionCompletion &acceptedCompletion);
    void finalizeSoftwareBarcodeWordResult(
        const BarcodeWordDetectionWorkOutput &output,
        const DetectionCompletion &acceptedCompletion);

    // ========== UI对象 ==========
    Ui::Widget *ui;                     ///< UI界面指针
    MultiCameraWidget *m_multiCameraWidget = nullptr;
    bool m_currentTemplateNameVisible = false;
    QWidget *m_wordTemplateEditWidget = nullptr;
    QLabel *m_wordTemplateEditLabel = nullptr;
    QComboBox *m_wordTemplateEditComboBox = nullptr;
    QPushButton *m_publishTemplateGroupButton = nullptr;
    QFrame *m_templateGuideFrame = nullptr;
    QLabel *m_templateGuideTitleLabel = nullptr;
    QLabel *m_templateGuideBodyLabel = nullptr;
    QPushButton *m_manualCharacterCropButton = nullptr;
    QPushButton *m_publishedRecipeButton = nullptr;
    QLineEdit *m_softwareDataDirLineEdit = nullptr;
    int m_currentWordTemplateEditIndex = -1;
    QMap<QString, QStringList> m_templateDirPathsByMode;
    QMap<QString, QString> m_publishedRecipeIdsByMode;
    QMap<QString, GlobalSettingBinding> m_globalSettingBindings;
    QList<HardwareActionBinding> m_hardwareActionBindings;
    GlobalSettings m_appliedGlobalSettings;
    QString m_currentDetectModeId = "word_detection";
    bool m_globalSettingsLoaded = false;
    bool m_applyingGlobalSettings = false;
    bool m_updatingGlobalSettingsUi = false;
    bool m_templateTargetTextDirty = false;
    bool m_templateImageThresholdDirty = false;
    QString m_templateTargetLabelText;
    QString m_templateThresholdLabelText;

    enum class TemplateCaptureState {
        Idle,
        Previewing,
        Frozen
    };
    TemplateCaptureState m_templateCaptureState =
            TemplateCaptureState::Idle;
    cv::Mat m_lastTemplatePreviewFrame;
    quint64 m_templatePreviewSessionId = 0;

    enum class OperationState {
        CameraClosed,
        CameraReady,
        Detecting,
        Stopping,
        TemplatePreviewing,
        TemplateFrozen
    };
    OperationState m_operationState =
            OperationState::CameraClosed;
    std::atomic<bool> m_resultBoundDisplayActive{false};
    bool m_applicationExitInProgress = false;
    bool m_detectionRoiWarningActive = false;
    InspectionRuntimeController m_runtimeController;
    DetectionResultPresenter m_detectionResultPresenter;
    TissueRecipeParameters m_tissueRecipeParameters;
    std::unique_ptr<ImageSaveService> m_imageSaveService;
    std::unique_ptr<DetectionCompletionController>
            m_detectionCompletionController;
    quint64 m_imageSaveFailedCount = 0;
    QString m_latestImageSaveError;
    bool m_imageSaveWarningScheduled = false;

    // ========== 定时器 ==========
    QTimer *timer;                      ///< 定时器
    QTimer *m_timer;                    ///< 定时器2
    QTimer *timer1;                     ///< 定时器3
    QTimer *m_templateCaptureAttentionTimer = nullptr;
    bool m_templateCaptureAttentionOn = false;

    // ========== 图像相关 ==========
    int imageIndex;                     ///< 图像索引
    QString imagePath;                  ///< 图像路径
    QStringList imageFiles;             ///< 图像文件列表
    bool recognitionCompletedFlag;      ///< 识别完成标志

    // ========== 识别框坐标 ==========
    int old_m_x1=0, old_m_x2=0, old_m_y1=0, old_m_y2=0; ///< 旧识别框坐标
    int a1, a2, b1, b2;                 ///< 坐标辅助变量
    int m_x1, m_x2, m_y1, m_y2;        ///< 识别框的四个坐标

    // ========== 采集和设备相关 ==========
    bool isCollecting;                  ///< 是否正在采集
    bool m_bOpenDevice;                 ///< 设备是否打开

    // ========== 相机和线程对象 ==========
    std::shared_ptr<ICameraDevice> m_cameraDevice; ///< 单相机设备边界
    MyThread *myThread = NULL;          ///< 软件触发线程
    CameraThread *cameraThread = NULL;         ///< 硬件触发线程

    // ========== 图像对象 ==========
    Mat *myImage = NULL;                ///< 原始图像
    Mat *processedImage = NULL;         ///< 处理后图像
    Mat *rotatedImage = NULL;           ///< 旋转后图像
    Mat *muban;                         ///< 模板图像
    Mat *frame;                         ///< 帧图像
    QImage *QmyImage = NULL;            ///< Qt图像对象
    OverlapDetector overlapDetector;  ///< 防重叠检测引擎实例


    // ========== 参数设置 ==========
    QString datatime;                   ///< 日期时间
    int exposureValue;                  ///< 曝光值
    int color = 1;                      ///< 颜色标志
    int angleValue=0;                   ///< 旋转角度
    int colorchannel=0;                 ///< 颜色通道

    // ========== PLC相关 ==========
    std::unique_ptr<IPlcDevice> m_plcDevice; ///< PLC设备边界
    int PLCmode;                        ///< PLC模式

    // ========== 设置和UI ==========
    QMap<QString, bool> settings;       ///< 设置映射
    QPointer<ImageLabel> imageLabel;    ///< 图像标签指针
    void initOverlapDetectorFromCurrentDir(); ///< 从当前模板文件夹加载防重叠配置

    // ========== 图像处理相关 ==========
    cv::Mat croppedImage;               ///< 裁剪图像
    bool isDrawingEnabled = false;      ///< 是否启用绘制
    cv::Mat affineMatrix;               ///< 仿射矩阵
    QRect redRect;                      ///< 红色矩形
    QRect blueRect;                     ///< 蓝色矩形
    cv::Mat img1;                       ///< 图像1
    cv::Mat img2;                       ///< 图像2
    cv::Rect roi;                       ///< 感兴趣区域
    Mat roi1;                           ///< ROI区域1
    Mat roi2;                           ///< ROI区域2
    double xRatio;                      ///< X轴比例
    double yRatio;                      ///< Y轴比例
    bool first;                         ///< 第一次标志

    // ========== 检测框相关 ==========
    vector<vector<QRect>> allDetectedRects; ///< 所有检测到的矩形
    std::vector<QRect> detectedRects;   ///< 检测到的矩形
    QRect dingweiRect;                  ///< 定位矩形
    QRect selectionRect;                ///< 选择矩形
    QRect selectionRect1;               ///< 选择矩形1
    // 保存的框坐标
    cv::Rect2d savedTrackingBox;    // 保存的跟踪框
    std::vector<cv::Point2f> savedBarcodePoly; // 二维码相对于定位锚点中心的四角
    std::vector<cv::Point2f> savedDatePoly; // 保存的生产日期相对多边形
    bool hasValidBoxes;              // 是否有有效的框坐标


    // ========== 识别结果相关 ==========
    String allResults;                  ///< 所有结果
    QVector<std::string> string1;       ///< 字符串向量
    int k = 1;                          ///< 计数器

    // ========== 跟踪相关 ==========
    bool tracking;                      ///< 是否正在跟踪
    Zhuizong *zhuizong;                 ///< 跟踪对象
    TemplateMatch* templatematch;       ///< 模板匹配对象
    cv::Rect trackWindow;               ///< 跟踪窗口
    vector<Scalar> colors;              ///< 颜色向量
    cv::Ptr<cv::MultiTracker> multiTracker; ///< 多目标跟踪器
    std::chrono::steady_clock::time_point lastDetectionTime; ///< 上次检测时间

    // ========== 多边形相关 ==========
    QVector<QPolygonF> newGreenPolygons; ///< 绿色多边形
    QVector<QPolygonF> newbluePolygons;  ///< 蓝色多边形
    QVector<QPolygonF> newredPolygons;   ///< 红色多边形

    // ========== 变换相关 ==========
    void calculateAffineMatrix();       ///< 计算仿射矩阵

    // ========== 模式和路径 ==========
    int mode;                           ///< 模式
    QString path;                       ///< 路径
    int wrongindex;                     ///< 错误索引

    // ========== 统计相关 ==========

    // ========== 模板匹配相关 ==========
    vector<Mat> digitTemplates;         ///< 数字模板
    std::vector<int> digitTemplateTargetIndexes; ///< 字库模板图对应的目标字符位置
    vector<Mat> digitRegions;           ///< 数字区域
    bool savefirst;                     ///< 第一次保存标志
    QString selectedDir;                ///< 选择的目录

    struct WordTemplateProfile {
        QString name;
        QString dirPath;
        cv::Mat trackingTemplate;
        std::vector<cv::Point2f> barcodePoly;
        std::vector<cv::Point2f> datePoly;
        TemplatePrivateSettings settings;
        RecipeProfile recipeProfile;
        TemplateProfileAssetManifest recipeAssetManifest;
        QMap<QString, QString> resolvedAssetPathsByRole;
        int targetCount = 0;
        std::vector<cv::Mat> digitTemplates;
        std::vector<int> digitTemplateTargetIndexes;
        TemplateMatchPreparedTemplates preparedDigitTemplates;
        mutable int preferredBarcodeStrategyId = -1;
        mutable unsigned int preferredBarcodeOptionFlags =
                BARCODE_DECODER_OPTION_NONE;
        mutable int consecutiveBarcodeFailures = 0;
    };

    std::vector<WordTemplateProfile> m_wordTemplateProfiles; ///< 字库多模板配置缓存
    TemplateRecipeDraftSession m_wordTemplateRecipeDraftSession; ///< 本次新建字库配方的编辑发布会话
    TemplateRecipeEditSession m_wordTemplateRecipeEditSession; ///< 当前已发布字库配方的参数编辑会话
    TemplateRecipeEditSession m_singleTemplateRecipeEditSession; ///< 当前已发布钢印/OCR单模板配方编辑会话
    QMap<QString, QString> m_singleTemplateResolvedAssetPathsByRole; ///< 当前单模板配方的只读运行资产
    QString m_currentTemplateDisplayName; ///< 已发布单模板使用配方显示名，避免显示内部Profile目录名
    bool m_barcodeWordRunActive = false; ///< 当前采集线程是否按二维码+三期快照分发
    std::unique_ptr<IOcrEngine> m_ocrEngine;
    std::unique_ptr<IBarcodeDecoder> m_barcodeDecoder;
    bool m_barcodeTemplateReadable = false;
    QRect m_validatedBarcodeRect;
    QString m_validatedBarcodeText;
    QStringList wordTemplateImagePathsForKey(const QDir &directory,
                                             const QString &searchKey,
                                             bool includeVariants = true) const;
    bool loadWordDigitTemplatesFromDir(const QString &dirPath,
                                       const QStringList &baseNames,
                                       std::vector<cv::Mat> *templates,
                                       std::vector<int> *templateTargetIndexes,
                                       QString *errorMessage,
                                       bool includeVariants = true) const;
    bool loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    bool loadWordTemplateProfileFromDir(const QString &dirPath,
                                        WordTemplateProfile *profile,
                                        QString *errorMessage);
    bool loadWordTemplateProfileFromRecipeSelection(
        const RecipeSelection &selection,
        int profileIndex,
        WordTemplateProfile *profile,
        QString *errorMessage);
    bool loadWordTemplateProfilesFromRecipeSelection(
        const RecipeSelection &selection,
        std::vector<WordTemplateProfile> *profiles,
        QStringList *pendingMessages,
        QString *errorMessage);
    void refreshWordTemplateProfileDigitCache(
        WordTemplateProfile *profile) const;
    InspectionProfileSnapshot createWordTemplateRunSnapshot() const;
    void refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const;
    bool saveWordTemplatePrivateSettings(
        int profileIndex,
        const TemplatePrivateSettings &settings,
        QString *errorMessage);
    void refreshWordTemplateRecipeAssets();
    void prepareWordTemplateRecipeDraft(const WordTemplateProfile &profile);
    bool publishWordTemplateRecipeDraft(QString *errorMessage);
    bool publishWordTemplateRecipeEdit(int profileIndex,
                                       QString *errorMessage);
    bool publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage);
    void setupWordTemplateEditorCombo();
    void publishCurrentWordTemplateGroup();
    void publishCurrentSingleTemplateRecipe();
    void selectPublishedRecipe();
    void setupDetectModeChangeTracking();
    void clearWordMultiTemplateState();
    void clearSingleTemplateRecipeState();
    bool loadSingleTemplateCharacterAssets(
        const QMap<QString, QString> &assetPathsByRole,
        const QStringList &targetUnits,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    bool republishSingleTemplateRecipeSettings(
        const TemplatePrivateSettings &settings,
        QString *errorMessage);
    void refreshWordTemplateEditorCombo();
    void applyWordTemplateEditorSelection(int comboIndex);
    void setCurrentWordTemplateEditIndex(int profileIndex);
    int currentWordTemplateProfileIndex() const;
    QString wordTemplateProfileAssetPath(
        const WordTemplateProfile &profile,
        const QString &role,
        const QString &legacyFileName) const;
    void displayWordTemplateRawImage(const WordTemplateProfile &profile);
    void displayWordTemplateRawImage(const QString &dirPath);
    void displayWordTemplateRawImageFile(const QString &rawImagePath,
                                         const QString &templateName);
    BarcodeDecodeOptions barcodeTemplateValidationOptions() const;
    bool validateBarcodeTemplateRect(
        const QRect &uiBarcodeRect,
        const BarcodeDecodeOptions &options,
        BarcodeReadResult *barcode,
        QString *failureReason);
    QString barcodeTemplateValidationFailureText(
        const BarcodeReadResult &barcode) const;
    void clearBarcodeTemplateValidation();

    // ========== 设置相关函数 ==========
    void loadSettings();                ///< 加载设置
    bool saveSettings(bool showErrorMessage = true);                ///< 保存设置
    GlobalSettings collectGlobalSettingsFromUi() const;
    void applyGlobalSettingsToUi(const GlobalSettings &settings);
    void applyTemplatePrivateSettingsToUi(const TemplatePrivateSettings &settings);
    void setupNonPersistentDefaults();  ///< 设置不属于公共配置的初始值
    void showDetectionRoiWarningOnce();
    void clearDetectionRoiWarning();
    void scheduleImageSaveWarning();
    QString currentTemplateDirPath;       // 非字库模式当前路径；字库模式仅由当前 profile 临时派生
    QString templateBaseDirPath;          // 产品模板父目录
    void initStyle();  // 声明后才能在 cpp 中实现和调用
    /**
         * @brief 重新初始化 myThread（软触发线程）
         * @details 安全地清理旧线程，创建新线程并连接信号槽
         */
    void reinitializeMyThread();

    /**
         * @brief 重新初始化 cameraThread（硬件触发线程）
         * @details 安全地清理旧线程，创建新线程并连接信号槽
         */
    void reinitializeCameraThread();

    /**
         * @brief 确保线程已就绪
         * @details 在启动线程前调用，检查并重新初始化必要的线程
         */
    void ensureThreadsReady();
};

#endif // WIDGET_H
