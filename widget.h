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
#include <iomanip>
#include <vector>
#include <QMetaType>
#include <QTranslator>
#include <QSettings>
#include <QStringList>
#include <QStandardItemModel>
#include "waitting.h"
#include <string>
#include <QSqlDatabase>
#include "databasesetting.h"
#include "enlarge.h"
#include "choosebarcodedialog.h"
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
#include "MvErrorDefine.h"
#include "CameraParams.h"
#include "MvCameraControl.h"
#include "cmvcamera.h"
#include "mythread.h"
#include "CameraThread.h"
#include "TissueRollDetector.h"
#include "snap7.h"
#include "PaddleOCR/include/config.h"
#include <PaddleOCR/include/ocr_det.h>
#include <PaddleOCR/include/ocr_rec.h>
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
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <utility> // for std::pair
#include <queue>
#include <templatematch.h>
#include <Detector.h>
#include "TrackingTypes.h"

using namespace cv;
using namespace PaddleOCR;

namespace Ui {
class Widget;
}

class MultiCameraWidget;
class QLabel;
class QComboBox;
class QFrame;
class QDialog;
class QPushButton;

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

    // ========== OCR相关对象 ==========
    OCRConfig *config = nullptr;        ///< OCR配置
    DBDetector *det = nullptr;          ///< 文本检测器
    Classifier *cls = nullptr;          ///< 文本分类器
    CRNNRecognizer *rec = nullptr;      ///< 文本识别器

    // ========== 相机相关 ==========
    int nRet = -1;                      ///< 返回值
    void* m_handle = NULL;              ///< 句柄

    // ========== 公共方法 ==========
    void initWidget();                  ///< 初始化界面
    void saveImage(QString format, QString savePath);    ///< 保存图像
    void saveImage2(QString format, QString savePath);
    void saveImage2(QString format, QString savePath, const QString &fileBaseName);
    void saveRawImage(QString format, QString savePath, const cv::Mat &image, const QString &fileBaseName);
    bool shouldSaveRecognitionBoxImage() const;
    bool shouldSaveNoRecognitionBoxImage() const;
    void saveResultImages(QString format, const QString &resultDirName, const cv::Mat &image);
    void saveWordResultImages(QString format, const QString &resultDirName, const cv::Mat &image);
    void saveImage2Async(QString format, QString savePath);   ///< 保存图像2
//    void saveImageByMVS(QString savePath, QString format);  ///通过MVS自带的函数保存
    void display(const Mat* image);     ///< 显示图像
    void saveSettingsToDir(const QString &dirPath);
    bool loadSettingsFromDir(const QString &dirPath);

signals:
    // ========== 信号定义 ==========
    void captureFrame(Mat image);       ///< 捕获帧信号
    void sendDataTo(QString);           ///< 发送数据信号
    void pipei();                       ///< 匹配信号
    void imgmuban(Mat*img);             ///< 模板图像信号
    void imgshibie(Mat *img);           ///< 识别图像信号
    void kernal(int n);                 ///< 核大小信号
    void jiancestring(String targetstring1);  ///< 检测字符串信号
    void caijianchicun(int width_min,int width_max,int height_min,int height_max,
                       int block_size1,int horizontalKernel,int verticalKernel);  ///< 裁剪尺寸信号
    void ssim(int s);                   ///< SSIM信号
    void rotate(int angle);             ///< 旋转角度信号
    void choosechannel(int color);      ///< 颜色通道信号

private slots:
    // ========== 界面相关槽函数 ==========
    void showscreen();                  ///< 显示屏幕
    void slot_displayAndDetect(cv::Mat *image);  ///< 显示和检测槽

    // ========== 检测相关槽函数 ==========
    void slot_readAndDetect(cv::Mat *image, DetectionPose pose);   ///< 读取并检测（主检测框）
    void slot_readAndDetect3(cv::Mat *image, DetectionPose pose);  ///< 读取并检测3（模板匹配）
    void slot_readAndDetect4(cv::Mat *image, DetectionPose pose); ///< 读取并检测4（字库匹配）
    void slot_handleTissueResult(cv::Mat *image, TissueRollResult tissueResult); ///< 处理纸巾检测结果

    // ❌ 已移除：void slot_readAndDetect2() - 额外检测框处理函数（简化版不支持）

    // ========== 按钮点击槽函数 ==========
    void on_VideoShoot_clicked();       ///< 单词采集按钮
//    void on_ReShoot_clicked();          ///< 重新采集按钮
    void on_HandwareDetect_clicked();   ///< 相机检测按钮
    void on_CloseCamera_clicked();      ///< 关闭相机按钮
    void onSpinBoxValueChanged(int value); ///< 旋转框值改变
    void on_sureButton_clicked();       ///< 确定按钮
    void on_Saveimage_clicked();        ///< 保存图像按钮

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
    QImage cvMatToQImage(const cv::Mat& mat); ///< Mat转QImage
    Mat* QImageToMat(const QImage &image);    ///< QImage转Mat
    void on_cancel_clicked();           ///< 取消按钮
    void slot_clearResultLabel();       ///< 清除结果标签
    void closeEvent(QCloseEvent *event) override; ///< 关闭事件
    void slot_saveBoxesFromThread(DetectionPose pose); ///接收运行时姿态

    // ========== 字符处理函数 ==========
    bool isChineseChar(unsigned char c);         ///< 判断是否为中文字符
    bool isAlnumOrChinese(char c);              ///< 判断是否为字母数字或中文

    // ========== 模式和功能按钮 ==========
    void on_plcmodebtn_clicked();       ///< PLC模式按钮
    void on_eliminatebutton_clicked();  ///< 消除按钮

    // ========== 其他按钮 ==========
    void on_pushButton_2_clicked();
    void on_pushButton_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_cut_cancelButton_2_clicked();
    void on_cut_cancelButton_3_clicked();


    void on_pushButton_10_clicked();

    void on_pushButton_7_clicked();


    void on_pushButton_11_clicked();

    void on_pushButton_12_clicked();

    void on_pushButton_tissueRoughnessThreshold_clicked();

    void on_confirmAllParamsButton_clicked();
    void on_MultiCameraMode_clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    cv::Mat m_loadedTrackingTemplate;
    void showParameterInfo(const QString &title, const QString &message);
    void showParameterInfoAsError(const QString &title, const QString &message);
    void showParameterWarning(const QString &title, const QString &message);
    void showParameterCritical(const QString &title, const QString &message);
    void addConfirmAllParamError(const QString &message);
    void updateCurrentTemplateName();
    void updateSaveDirButtonText();
    void updateTissueRoughnessUiVisibility();
    bool applyTissueRoughnessThresholdFromUi(bool showMessage);
    void setupTemplateGuide();
    void showTemplateGuideForCurrentMode();
    void hideTemplateGuide();
    void updateImageDisplayStatusText(const QString &body);
    void updateTemplateGuideText(const QString &title, const QString &body);
    void handleTemplateGuideEvent(const QString &eventName, int pointCount);
    void setupCharacterSplitSettingsDialog();
    void showCharacterSplitSettingsDialog();

    // ========== UI对象 ==========
    Ui::Widget *ui;                     ///< UI界面指针
    MultiCameraWidget *m_multiCameraWidget = nullptr;
    bool m_confirmAllParamsRunning = false;
    bool m_allParamsConfirmed = false;
    bool m_currentTemplateNameVisible = false;
    QWidget *m_wordTemplateEditWidget = nullptr;
    QLabel *m_wordTemplateEditLabel = nullptr;
    QComboBox *m_wordTemplateEditComboBox = nullptr;
    QWidget *m_wordThresholdEditWidget = nullptr;
    QLabel *m_wordThresholdEditLabel = nullptr;
    QComboBox *m_wordThresholdEditComboBox = nullptr;
    QFrame *m_templateGuideFrame = nullptr;
    QLabel *m_templateGuideTitleLabel = nullptr;
    QLabel *m_templateGuideBodyLabel = nullptr;
    QDialog *m_characterSplitSettingsDialog = nullptr;
    QPushButton *m_characterSplitSettingsButton = nullptr;
    int m_currentWordTemplateEditIndex = -1;
    QStringList m_confirmAllParamErrors;

    // ========== 定时器 ==========
    QTimer *timer;                      ///< 定时器
    QTimer *m_timer;                    ///< 定时器2
    QTimer *timer1;                     ///< 定时器3

    // ========== 图像相关 ==========
    int imageIndex;                     ///< 图像索引
    QString imagePath;                  ///< 图像路径
    QStringList imageFiles;             ///< 图像文件列表
    bool recognitionCompletedFlag;      ///< 识别完成标志
    int ngImages;                       ///< NG图像数量

    // ========== 识别框坐标 ==========
    int old_m_x1=0, old_m_x2=0, old_m_y1=0, old_m_y2=0; ///< 旧识别框坐标
    int a1, a2, b1, b2;                 ///< 坐标辅助变量
    int m_x1, m_x2, m_y1, m_y2;        ///< 识别框的四个坐标

    // ========== 采集和设备相关 ==========
    bool isCollecting;                  ///< 是否正在采集
    bool m_bOpenDevice;                 ///< 设备是否打开
    MV_CC_DEVICE_INFO_LIST m_stDevList; ///< 设备列表

    // ========== 相机和线程对象 ==========
    CMvCamera *m_pcMyCamera = NULL;     ///< 相机对象指针
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
    TS7Client* client;                  ///< PLC客户端
    char *Address;                      ///< PLC地址
    int Rack = 0;                       ///< 机架号
    int Slot = 1;                       ///< 槽号
    int PLCmode;                        ///< PLC模式
    std::queue<std::pair<int, int>> removalQueue; ///< 移除队列

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
    int x = 1;                          ///< 识别框数量（简化版固定为1）
    vector<vector<QRect>> allDetectedRects; ///< 所有检测到的矩形
    std::vector<QRect> detectedRects;   ///< 检测到的矩形
    QRect dingweiRect;                  ///< 定位矩形
    QRect selectionRect;                ///< 选择矩形
    QRect selectionRect1;               ///< 选择矩形1
    // 保存的框坐标
    cv::Rect2d savedTrackingBox;    // 保存的跟踪框
    std::vector<cv::Point2f> savedDatePoly; // 保存的生产日期相对多边形
    bool hasValidBoxes;              // 是否有有效的框坐标


    // ========== 识别结果相关 ==========
    String allResults;                  ///< 所有结果
    QVector<std::string> string1;       ///< 字符串向量
    int j = 1;                          ///< 识别次数统计
    int k = 1;                          ///< 计数器

    // ========== 跟踪相关 ==========
    bool tracking;                      ///< 是否正在跟踪
    bool judge = false;                 ///< 判断标志（注意：此变量在代码中有多种用途）
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
    int totalImages;                    ///< 总图像数量
    int currentImagesSnapshot;          ///< 当前图像快照

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
        std::vector<cv::Point2f> datePoly;
        QString targetText;
        QString imageThresholdText;
        int targetCount = 0;
        std::vector<cv::Mat> digitTemplates;
        std::vector<int> digitTemplateTargetIndexes;
    };

    QStringList m_wordTemplateDirPaths; ///< 字库多模板路径缓存
    std::vector<WordTemplateProfile> m_wordTemplateProfiles; ///< 字库多模板配置缓存
    bool m_wordMultiTemplateMode = false; ///< 字库多模板模式标志
    QStringList parseWordTemplateBaseNames(const QString &targetText) const;
    QStringList wordTemplateImagePathsForKey(const QDir &directory,
                                             const QString &searchKey,
                                             bool includeVariants = true) const;
    bool loadWordDigitTemplatesFromDir(const QString &dirPath,
                                       const QStringList &baseNames,
                                       std::vector<cv::Mat> *templates,
                                       std::vector<int> *templateTargetIndexes,
                                       QString *errorMessage,
                                       bool includeVariants = true) const;
    void setupWordTemplateEditorCombo();
    void clearWordMultiTemplateState();
    void refreshWordTemplateEditorCombo();
    void applyWordTemplateEditorSelection(int comboIndex);
    void setCurrentWordTemplateEditIndex(int profileIndex);
    int currentWordTemplateProfileIndex() const;
    void displayWordTemplateRawImage(const QString &dirPath);
    void runWordTemplateDetection(cv::Mat *image,
                                  const DetectionPose &pose,
                                  const std::vector<cv::Mat> &templates,
                                  const std::vector<int> &templateTargetIndexes,
                                  const QString &targetText,
                                  const QString &imageThresholdText,
                                  const QString &templateName);

    // ========== 设置相关函数 ==========
    void loadSettings();                ///< 加载设置
    void saveSettings();                ///< 保存设置
    void setupDefaultValues();          ///< 设置默认值
    void dispatchDetectionByMode(cv::Mat *image, DetectionPose pose); ///< 根据识别模式分发检测逻辑
    void loadLastTemplateConfig();        // 新增：加载模板图像
    QString currentTemplateDirPath;       // 新增：持久化模板路径
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
