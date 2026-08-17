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
#include <cstdint>
#include <iomanip>
#include <memory>
#include <functional>
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
#include <QtWidgets/QMainWindow>
#include "TissueRollDetector.h"
#include <QImage>
#include "imagelabel.h"
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
#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "recipes/product_recipe.h"
#include "recipes/recipe_store.h"
#include "recipes/template_mode_memory.h"
#include "runtime/template_runtime_profile.h"
#include "devices/barcode/barcode_decoder.h"
#include "devices/ocr/ocr_engine.h"
#include "runtime/inspection_runtime_controller.h"
#include "ui/controllers/operation_ui_policy.h"
#include "ui/controllers/settings_edit_state.h"

using namespace cv;

namespace Ui {
class Widget;
}

class QLabel;
class QComboBox;
class QFrame;
class QDialog;
class QPushButton;
class InspectionResultCoordinator;
class InspectionRuntimeUiCoordinator;
class MachineSettingsPageController;
class TemplateEditorController;
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
    using OcrEngineFactory =
        std::function<std::shared_ptr<IOcrEngine>()>;

    explicit Widget(
        const OcrEngineFactory &ocrEngineFactory,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
        const std::shared_ptr<InspectionRuntimeController> &runtimeController,
        const std::shared_ptr<InspectionRuntimePort> &runtimePort,
        const std::shared_ptr<InspectionApplicationService> &inspectionService,
        const std::shared_ptr<SettingsApplicationService> &settingsService,
        const std::shared_ptr<RecipeStore> &recipeStore,
        QWidget *parent = nullptr);
    ~Widget();

    // ========== 工具函数 ==========
    std::string qstr2str(const QString qstr);
    QString str2qstr(const std::string str)
    {
        return QString::fromUtf8(str.data());
    }

    // ========== 公共方法 ==========
    void initWidget();                  ///< 初始化界面
//    void saveImageByMVS(QString savePath, QString format);  ///通过MVS自带的函数保存
    void display(const Mat* image);     ///< 显示图像

signals:
    // ========== 信号定义 ==========
    void imgshibie(Mat *img);           ///< 识别图像信号
    void jiancestring(String targetstring1);  ///< 检测字符串信号
    void ssim(int s);                   ///< SSIM信号

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class TemplateEditorController;

    cv::Mat m_loadedTrackingTemplate;
    void showParameterInfo(const QString &title, const QString &message);
    void showParameterInfoWithRedWarning(const QString &title,
                                         const QString &message,
                                         const QString &warningMessage);
    void showParameterInfoAsError(const QString &title, const QString &message);
    void showParameterWarning(const QString &title, const QString &message);
    void showParameterCritical(const QString &title, const QString &message);
    bool applyCameraExposureValue(int exposureValue, QString *errorMessage);
    bool applyCameraExposureFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyCameraGainFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyCameraHardwareSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcTriggerModeFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcRunSettingsFromUi(QStringList *errors, bool showSuccessMessage);
    bool applyPlcTriggerModeForRun(
        const MachineSettings &settings,
        QStringList *errors);
    bool applyPlcRunSettingsForRun(
        const MachineSettings &settings,
        QStringList *errors);
    bool hasDirtySettings() const;
    QString dirtySettingsMessage() const;
    void restoreUnappliedSettingsFromApplied();
    void setupRecipeProfileDirtyTracking();
    void refreshTemplateTargetTextDirty();
    void refreshTemplateImageThresholdDirty();
    void refreshRecipeProfileDirty();
    void markTemplateTargetTextDirty();
    void markTemplateImageThresholdDirty();
    void clearTemplateTargetTextDirty();
    void clearTemplateImageThresholdDirty();
    void clearRecipeProfileDirty();
    void updateRecipeProfileDirtyUi();
    void updateHardwareParameterUiEnabled();
    void updateCurrentTemplateName();
    void updateSaveDirButtonText();
    void updateImageSaveOptionsVisibility();
    void updateTissueRoughnessUiVisibility();
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
    void restoreDefaultMachineSettings();
    QString detectModeIdForIndex(int index) const;
    QString currentDetectModeId() const;
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
    bool startTemplatePreview();
    bool freezeTemplatePreview();
    bool stopTemplatePreview(int waitTimeMs = 1500);
    void resetTemplateCaptureState();
    void updateOperationUiState();
    OperationUiState operationUiState() const;
    bool isCameraOpen() const;
    bool isInspectionBusy() const;
    const MachineSettings &machineSettings() const;
    void updateMachineSettingsDraft(const MachineSettings &settings);
    void bindInspectionRuntimePort();
    bool executeInspectionStart(
        const InspectionStartExecutionCommand &command,
        InspectionRuntimeStartTransaction &startTransaction,
        QString *errorMessage);
    void rollbackInspectionStart();
    void presentStartFailure(const StartInspectionResult &result);
    void finishInspectionStopUi(const StopInspectionResult &result);
    bool startDetectionWorkerForMode(
        InspectionRuntimeStartTransaction &startTransaction,
        int modeIndex,
        const MachineSettings &settings,
        const PreparedRecipeSnapshot &prepared,
        const InspectionProfileSnapshot &profileSnapshot,
        QString *errorMessage);
    void enterInspectionFault(
        InspectionFaultReason reason,
        const QString &diagnostic);
    void presentInspectionFault();
    bool confirmInspectionFaultRecovery();
    bool reconcileInspectionFaultProducts(
        QString *summary,
        QString *errorMessage);
    bool requestFaultFallbackNgPulse(
        const ProductKey &productKey,
        QString *errorMessage);
    bool writeInspectionPlcOutput(
        std::uint8_t value,
        QString *errorMessage);
    void recordFaultedPlcOutput(const ProductKey &productKey);
    void checkInspectionPlcHealth();
    void restoreNormalFaultUi();

    // ========== UI对象 ==========
    Ui::Widget *ui;                     ///< UI界面指针
    QLineEdit *m_softwareDataDirLineEdit = nullptr;
    std::shared_ptr<InspectionApplicationService>
            m_inspectionApplicationService;
    std::shared_ptr<SettingsApplicationService>
            m_settingsApplicationService;
    std::shared_ptr<InspectionRuntimePort> m_inspectionRuntimePort;
    MachineSettings &m_appliedMachineSettings;
    std::shared_ptr<RecipeStore> m_recipeStore;
    QString m_currentDetectModeId;
    bool m_applyingMachineSettings = false;
    bool m_updatingMachineSettingsUi = false;
    SettingsEditState m_settingsEditState;
    std::unique_ptr<MachineSettingsPageController> m_settingsPageController;
    std::unique_ptr<TemplateEditorController> m_templateEditorController;

    enum class TemplateCaptureState {
        Idle,
        Previewing,
        Frozen
    };
    TemplateCaptureState m_templateCaptureState =
            TemplateCaptureState::Idle;
    cv::Mat m_lastTemplatePreviewFrame;
    quint64 m_templatePreviewSessionId = 0;

    using OperationState = OperationUiState;
    std::atomic<bool> m_resultBoundDisplayActive{false};
    bool m_applicationExitInProgress = false;
    InspectionRuntimeController &m_runtimeController;
    std::unique_ptr<InspectionResultCoordinator> m_resultCoordinator;
    std::unique_ptr<InspectionRuntimeUiCoordinator>
            m_runtimeUiCoordinator;
    bool m_faultAlarmPresented = false;
    ProductKey m_activePlcOutputProductKey;
    std::vector<ProductKey> m_pendingPlcResetProductKeys;

    // ========== 定时器 ==========
    QTimer *timer;                      ///< 定时器
    QTimer *m_timer;                    ///< 定时器2
    QTimer *timer1;                     ///< 定时器3
    QTimer *m_plcHealthTimer = nullptr; ///< 运行中PLC连接监视
    QTimer *m_templateCaptureAttentionTimer = nullptr;
    bool m_templateCaptureAttentionOn = false;

    // ========== 图像相关 ==========
    int imageIndex;                     ///< 图像索引
    QStringList imageFiles;             ///< 图像文件列表
    bool recognitionCompletedFlag;      ///< 识别完成标志

    // ========== 识别框坐标 ==========
    int old_m_x1=0, old_m_x2=0, old_m_y1=0, old_m_y2=0; ///< 旧识别框坐标
    int a1, a2, b1, b2;                 ///< 坐标辅助变量
    int m_x1, m_x2, m_y1, m_y2;        ///< 识别框的四个坐标

    // ========== 采集和设备相关 ==========
    // ========== 图像对象 ==========
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
    int PLCmode;                        ///< PLC模式

    // ========== 设置和UI ==========
    QMap<QString, bool> settings;       ///< 设置映射
    QPointer<ImageLabel> imageLabel;    ///< 图像标签指针

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
    std::vector<std::vector<QRect>> allDetectedRects; ///< 所有检测到的矩形
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
    TemplateMatch* templatematch;       ///< 模板匹配对象
    cv::Rect trackWindow;               ///< 跟踪窗口
    std::vector<Scalar> colors;         ///< 颜色向量
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
    std::vector<Mat> digitTemplates;    ///< 数字模板
    std::vector<int> digitTemplateTargetIndexes; ///< 字库模板图对应的目标字符位置
    std::vector<Mat> digitRegions;      ///< 数字区域
    bool savefirst;                     ///< 第一次保存标志
    QString selectedDir;                ///< 选择的目录

    bool m_barcodeWordRunActive = false; ///< 当前采集线程是否按二维码+三期快照分发
    std::shared_ptr<IOcrEngine> m_ocrEngine;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
    bool loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const;
    void refreshWordTemplateRecipeProfile(
        WordTemplateProfile *profile) const;
    bool saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage);
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
    bool republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage);
    void refreshWordTemplateEditorCombo();
    void applyWordTemplateEditorSelection(int comboIndex);
    void setCurrentWordTemplateEditIndex(int profileIndex);
    int currentWordTemplateProfileIndex() const;
    void displayWordTemplateRawImage(const WordTemplateProfile &profile);
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
    bool saveSettings(bool showErrorMessage = true);                ///< 保存设置
    void applyMachineSettingsToUi(const MachineSettings &settings);
    void applyRecipeProfileToUi(const RecipeProfile &settings);
    void setupNonPersistentDefaults();  ///< 设置不属于公共配置的初始值
    void initStyle();  // 声明后才能在 cpp 中实现和调用
};

#endif // WIDGET_H
