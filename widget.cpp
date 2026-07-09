/**
 * @file widget.cpp
 * @brief 工业视觉识别系统主窗口实现文件
 * @details 实现图像采集、OCR识别、模板匹配、PLC通信等核心功能
 * @author 优化版本
 * @date 2024
 */

#include "widget.h"
#include "ui_widget.h"
#include "SerialPort.h"
#include "waitting.h"
#include "databasesetting.h"
#include "enlarge.h"
#include "choosebarcodedialog.h"
#include "snap7.h"
#include "multicamerawidget.h"
#include "charactertemplatecropdialog.h"


// Qt核心组件
#include <QTimer>
#include <QThread>
#include <QFileDialog>
#include <QAbstractItemView>
#include <QImageReader>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QLineEdit>
#include <QMetaType>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QPixmap>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QDesktopServices>
#include <QDateTime>
#include <QApplication>
#include <QTranslator>
#include <QIcon>
#include <QCamera>
#include <QCameraInfo>
#include <QDesktopWidget>
#include <QSettings>
#include <QSplashScreen>
#include <QTextCodec>
#include <QElapsedTimer>
#include <QDir>
#include <QStandardPaths>
#include <QTreeView>
#include <QComboBox>
#include <QGridLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolTip>
#include <QWhatsThis>
#include <QCursor>
#include <QFrame>

// Qt串口和SQL
#include <QtSerialPort/QtSerialPort>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QVariantList>
#include <QtSql/QSqlDatabase>

// 标准库
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>

#pragma execution_character_set("utf-8")
using namespace std;

// OpenCV全局变量
cv::Point pt1, pt2;

// ==========================================
// 防重叠标定辅助功能 (精简版：去除了图像上的红色字体)
// ==========================================
struct PolygonUIState {
    cv::Mat displayImg;
    cv::Mat tempImg;
    std::string windowName;
    std::vector<cv::Point> points;
};

// ==========================================
// 钢印多边形描点功能 (点击画点，按回车键完成)
// ==========================================
static void polyMouseCallback(int event, int x, int y, int flags, void* userdata) {
    PolygonUIState* state = reinterpret_cast<PolygonUIState*>(userdata);
    if (event == cv::EVENT_LBUTTONDOWN) {
        state->points.push_back(cv::Point(x, y));
        state->tempImg = state->displayImg.clone();
        // 绘制已有的点和线
        for (size_t i = 0; i < state->points.size(); ++i) {
            cv::circle(state->tempImg, state->points[i], 3, cv::Scalar(0, 0, 255), -1);
            if (i > 0) {
                cv::line(state->tempImg, state->points[i - 1], state->points[i], cv::Scalar(0, 255, 0), 2);
            }
        }
        cv::imshow(state->windowName, state->tempImg);
    } else if (event == cv::EVENT_MOUSEMOVE && !state->points.empty()) {
        // 鼠标悬停时的预览辅助线
        cv::Mat hoverImg = state->tempImg.clone();
        cv::line(hoverImg, state->points.back(), cv::Point(x, y), cv::Scalar(255, 0, 0), 1);
        cv::imshow(state->windowName, hoverImg);
    }
}




// ==========================================
// 快速矩形标定功能 (拖拽并松开鼠标即完成)
// ==========================================
struct QuickROIState {
    cv::Mat displayImg;
    cv::Mat tempImg;
    std::string windowName;
    cv::Rect roi;
    cv::Point startPt;
    bool isDrawing = false;
    bool isDone = false;
};

static void quickMouseCallback(int event, int x, int y, int flags, void* userdata) {
    QuickROIState* state = reinterpret_cast<QuickROIState*>(userdata);

    if (event == cv::EVENT_LBUTTONDOWN) {
        state->startPt = cv::Point(x, y);
        state->isDrawing = true;
        state->isDone = false;
    }
    else if (event == cv::EVENT_MOUSEMOVE && state->isDrawing) {
        state->tempImg = state->displayImg.clone();
        cv::rectangle(state->tempImg, state->startPt, cv::Point(x, y), cv::Scalar(0, 255, 0), 2);
        cv::imshow(state->windowName, state->tempImg);
    }
    else if (event == cv::EVENT_LBUTTONUP) {
        state->roi = cv::Rect(state->startPt, cv::Point(x, y));
        // 处理反向拖拽的情况
        if (state->roi.width < 0) { state->roi.x += state->roi.width; state->roi.width = std::abs(state->roi.width); }
        if (state->roi.height < 0) { state->roi.y += state->roi.height; state->roi.height = std::abs(state->roi.height); }

        state->isDrawing = false;
        state->isDone = true; // 标记绘制完成
    }
}

static std::vector<cv::Point> getPolygonROI(const cv::Mat& img, const std::string& windowTitle) {
    cv::Mat displayImg = img.clone();
    int screenHeightLimit = 800;
    double scale = 1.0;
    if (displayImg.rows > screenHeightLimit) {
        scale = static_cast<double>(screenHeightLimit) / displayImg.rows;
        cv::resize(displayImg, displayImg, cv::Size(), scale, scale);
    }

    PolygonUIState state;
    state.displayImg = displayImg;
    state.tempImg = displayImg.clone();
    state.windowName = windowTitle;

    cv::namedWindow(windowTitle);
    cv::setMouseCallback(windowTitle, polyMouseCallback, &state);

    while (true) {
        cv::imshow(windowTitle, state.tempImg);
        int key = cv::waitKey(10) & 0xFF;
        if (key == 13) { // Enter键确认
            if (state.points.size() >= 3) {
                cv::line(state.tempImg, state.points.back(), state.points.front(), cv::Scalar(0, 255, 0), 2);
                cv::imshow(windowTitle, state.tempImg);
                cv::waitKey(300);
            }
            break;
        } else if (key == 27) { // ESC键取消
            state.points.clear();
            break;
        }
    }
    // 恢复 widget1.cpp 的简单销毁模式，不再手动注销 callback
    cv::destroyWindow(windowTitle);

    std::vector<cv::Point> finalPts;
    for (auto& pt : state.points) {
        finalPts.push_back(cv::Point(static_cast<int>(pt.x / scale), static_cast<int>(pt.y / scale)));
    }
    return finalPts;
}

static cv::Rect getQuickRectROI(const cv::Mat& img, const std::string& windowTitle) {
    cv::Mat displayImg = img.clone();
    int screenHeightLimit = 800;
    double scale = 1.0;
    if (displayImg.rows > screenHeightLimit) {
        scale = static_cast<double>(screenHeightLimit) / displayImg.rows;
        cv::resize(displayImg, displayImg, cv::Size(), scale, scale);
    }

    QuickROIState state;
    state.displayImg = displayImg;
    state.tempImg = displayImg.clone();
    state.windowName = windowTitle;

    cv::namedWindow(windowTitle);
    cv::setMouseCallback(windowTitle, quickMouseCallback, &state);

    while (!state.isDone) {
        cv::imshow(windowTitle, state.tempImg);
        int key = cv::waitKey(10) & 0xFF;
        if (key == 27) break;
    }

    cv::destroyWindow(windowTitle);

    cv::Rect finalRoi = state.roi;
    finalRoi.x = static_cast<int>(finalRoi.x / scale);
    finalRoi.y = static_cast<int>(finalRoi.y / scale);
    finalRoi.width = static_cast<int>(finalRoi.width / scale);
    finalRoi.height = static_cast<int>(finalRoi.height / scale);

    return finalRoi;
}





struct CVDrawResult {
    std::vector<cv::Point> poly;
    double score;
};
static std::vector<CVDrawResult> g_lastDrawResults;
static DetectionPose g_lastPose;
static qint64 g_lastDetectTime = 0;

// ============ 新增：用于绘制钢印的数据缓存 ============
static std::vector<cv::Point> g_lastStampPoly; // 保存钢印的多边形坐标
static bool g_lastStampIsOverlap = false;      // 记录钢印是否发生重叠
static bool g_allowTissueDetectionFrameDisplay = false;
static TissueRollItem g_lastTissueRoll;
static bool g_hasLastTissueRoll = false;

static cv::Point2f transformPoint(const cv::Mat& affine, const cv::Point2f& pt)
{
    return cv::Point2f(
        static_cast<float>(affine.at<double>(0, 0) * pt.x + affine.at<double>(0, 1) * pt.y + affine.at<double>(0, 2)),
        static_cast<float>(affine.at<double>(1, 0) * pt.x + affine.at<double>(1, 1) * pt.y + affine.at<double>(1, 2))
    );
}

static std::vector<cv::Point> transformPolygon(const std::vector<cv::Point>& poly, const cv::Mat& affine)
{
    std::vector<cv::Point> transformed;
    transformed.reserve(poly.size());
    for (const auto& pt : poly) {
        const cv::Point2f mapped = transformPoint(affine, cv::Point2f(static_cast<float>(pt.x), static_cast<float>(pt.y)));
        transformed.emplace_back(cvRound(mapped.x), cvRound(mapped.y));
    }
    return transformed;
}

static cv::Rect expandAndClampRect(const cv::Rect& rect, int padding, const cv::Size& bounds)
{
    cv::Rect expanded(rect.x - padding,
                      rect.y - padding,
                      rect.width + padding * 2,
                      rect.height + padding * 2);
    return expanded & cv::Rect(0, 0, bounds.width, bounds.height);
}

static cv::Point getPolygonTopCenter(const std::vector<cv::Point>& poly)
{
    if (poly.empty()) {
        return cv::Point();
    }
    if (poly.size() == 1) {
        return poly.front();
    }

    std::vector<cv::Point> sorted = poly;
    std::sort(sorted.begin(), sorted.end(), [](const cv::Point& lhs, const cv::Point& rhs) {
        if (lhs.y != rhs.y) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });

    const cv::Point& p1 = sorted[0];
    const cv::Point& p2 = sorted[1];
    return cv::Point((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
}

static OrientedDateRoi prepareOrientedDateRoi(const cv::Mat& src, const DetectionPose& pose, int padding)
{
    OrientedDateRoi oriented;
    if (src.empty() || !pose.valid || pose.datePoly.size() < 3) {
        return oriented;
    }

    oriented.rotationMatrix = cv::getRotationMatrix2D(pose.anchorCenter, -pose.angleDeg, 1.0);
    cv::invertAffineTransform(oriented.rotationMatrix, oriented.inverseRotationMatrix);
    cv::warpAffine(src, oriented.rotatedImage, oriented.rotationMatrix, src.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    oriented.rotatedDatePoly = transformPolygon(pose.datePoly, oriented.rotationMatrix);
    if (oriented.rotatedDatePoly.size() < 3) {
        return oriented;
    }

    oriented.roi = expandAndClampRect(cv::boundingRect(oriented.rotatedDatePoly), padding, oriented.rotatedImage.size());
    if (oriented.roi.width <= 0 || oriented.roi.height <= 0) {
        return oriented;
    }

    oriented.croppedImage = oriented.rotatedImage(oriented.roi).clone();
    if (oriented.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (oriented.croppedImage.channels() == 1) {
            cv::cvtColor(oriented.croppedImage, converted, cv::COLOR_GRAY2BGR);
        } else if (oriented.croppedImage.channels() == 4) {
            cv::cvtColor(oriented.croppedImage, converted, cv::COLOR_BGRA2BGR);
        } else {
            converted = oriented.croppedImage.clone();
        }
        oriented.croppedImage = converted;
    }

    oriented.valid = !oriented.croppedImage.empty();
    return oriented;
}

static std::vector<CVDrawResult> mapMatchResultsToOriginal(
    const std::vector<std::tuple<cv::Rect, double, size_t>>& matchResults,
    const OrientedDateRoi& oriented,
    const cv::Size& originalSize)
{
    std::vector<CVDrawResult> mapped;
    mapped.reserve(matchResults.size());

    for (const auto& match : matchResults) {
        cv::Rect rect = std::get<0>(match);
        rect.x += oriented.roi.x;
        rect.y += oriented.roi.y;

        const std::vector<cv::Point> rectPoly = {
            cv::Point(rect.x, rect.y),
            cv::Point(rect.x + rect.width, rect.y),
            cv::Point(rect.x + rect.width, rect.y + rect.height),
            cv::Point(rect.x, rect.y + rect.height)
        };
        std::vector<cv::Point> mappedPoly = transformPolygon(rectPoly, oriented.inverseRotationMatrix);
        cv::Rect mappedBounds = cv::boundingRect(mappedPoly) &
                                cv::Rect(0, 0, originalSize.width, originalSize.height);
        if (mappedBounds.width <= 0 || mappedBounds.height <= 0) {
            continue;
        }

        CVDrawResult drawResult;
        drawResult.poly = std::move(mappedPoly);
        drawResult.score = std::get<1>(match);
        mapped.push_back(drawResult);
    }

    return mapped;
}

/**
 * @brief Widget构造函数
 * @param parent 父窗口指针
 * @details 初始化UI、相机、OCR模型、定时器等核心组件
 */
Widget::Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget),
      timer(new QTimer(this)),
      timer1(new QTimer(this)),
      imageIndex(0),
      color(1),
      templatematch(nullptr),
      tracking(true),
      zhuizong(nullptr),
      first(false),
      savefirst(false),
      imageLabel(nullptr)
{
    ui->setupUi(this);

    initStyle();

    // UI 文件中已经是 ImageLabel，直接使用
    imageLabel=ui->image_undetected;

    // 注册Qt元类型，用于跨线程信号传递
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<cv::Mat *>("cv::Mat*");
    qRegisterMetaType<cv::Rect2d>("cv::Rect2d");
    qRegisterMetaType<std::vector<cv::Point>>("std::vector<cv::Point>");
    qRegisterMetaType<DetectionPose>("DetectionPose");
    qRegisterMetaType<TissueRollResult>("TissueRollResult");
    qRegisterMetaType<QString>("QString");

    // 初始化追踪对象（使用智能指针）
    unique_ptr<Zhuizong> zhuizong = make_unique<Zhuizong>();

    // 初始化PLC客户端
    client = new TS7Client;

    // 初始化窗口组件
    initWidget();
    qDebug() << "1. initWidget执行完毕 ";

    // 加载OCR配置文件
    config = new OCRConfig("config1.txt");
    config->PrintConfigInfo();
    qDebug() << "2. config.txt 读取完毕";

    // 初始化检测器（DBNet模型）
    det = new DBDetector(config->det_model_dir, config->use_gpu, config->gpu_id,
                         config->gpu_mem, config->cpu_math_library_num_threads,
                         config->use_mkldnn, config->max_side_len, config->det_db_thresh,
                         config->det_db_box_thresh, config->det_db_unclip_ratio,
                         config->visualize, config->use_tensorrt, config->use_fp16);
    qDebug() << "3. DBDetector 模型加载完毕";

    // 初始化分类器（角度分类）
    if (config->use_angle_cls == true)
    {
        cls = new Classifier(config->cls_model_dir, config->use_gpu, config->gpu_id,
                             config->gpu_mem, config->cpu_math_library_num_threads,
                             config->use_mkldnn, config->cls_thresh,
                             config->use_tensorrt, config->use_fp16);
        qDebug() << "4. Classifier 角度分类模型加载完毕";
    }

    // 初始化识别器（CRNN模型）
    rec = new CRNNRecognizer(config->rec_model_dir, config->use_gpu, config->gpu_id,
                             config->gpu_mem, config->cpu_math_library_num_threads,
                             config->use_mkldnn, config->char_list_file,
                             config->use_tensorrt, config->use_fp16);
    qDebug() << "5. CRNNRecognizer 模型加载完毕";

    // 初始化统计变量
    hasValidBoxes = false;
    savedTrackingBox = cv::Rect2d(0, 0, 0, 0);
    savedDatePoly.clear();
    recognitionCompletedFlag = false;
    isCollecting = false;
    totalImages = 0;
    ngImages = 0;
    allResults = "";
    wrongindex = ui->lineEdit_12->text().toInt();

    // 设置文本框自动换行
    ui->dateEdit->setWordWrapMode(QTextOption::WordWrap);
    setupWordTemplateEditorCombo();
    setupTemplateGuide();
    setupCharacterSplitSettingsDialog();

    // 禁用焦点滚动调节（防误触）- 遍历全局所有下拉框和数字输入框，一劳永逸
    QList<QComboBox *> comboBoxes = this->findChildren<QComboBox *>();
    for (QComboBox *cb : comboBoxes) {
        cb->installEventFilter(this);
    }
    QList<QAbstractSpinBox *> spinBoxes = this->findChildren<QAbstractSpinBox *>();
    for (QAbstractSpinBox *sb : spinBoxes) {
        sb->installEventFilter(this);
    }

    // 连接定时器信号
    connect(timer, &QTimer::timeout, this, &Widget::rightremove);
    connect(imageLabel, &ImageLabel::signal_templateGuideEvent,
            this, &Widget::handleTemplateGuideEvent);

    qDebug() << "6. 变量初始化与信号连接完毕";

    // 设置默认值并加载保存的设置
    setupDefaultValues();
    qDebug() << "7. setupDefaultValues 执行完毕";

    loadSettings();
    qDebug() << "8. loadSettings 执行完毕";

    loadLastTemplateConfig(); // 加载模板图像
    updateCurrentTemplateName();

    QTimer::singleShot(1000, this, [this]() {
        // 1. 先把从界面获取的文本存为一个 QString 变量
        QString targetIp = ui->lineEdit->text();

        QByteArray ad = targetIp.toUtf8();
        Address = ad.data();

        int tmp = client->ConnectTo(Address, 0, 1);
        if (tmp == 0) {
            QMessageBox::information(this, "提示", "PLC 自动连接成功");
        } else {
            // 2. 使用 QString::arg() 动态拼接字符串
            // %1 会被替换为 targetIp 的真实内容
            QString errorMsg = QString("PLC 自动连接失败！\n尝试连接的地址：%1\n请检查网络或稍后手动连接！").arg(targetIp);

            QMessageBox::warning(this, "警告", errorMsg);
        }
    });


    qDebug() << "9. loadLastTemplateConfig 执行完毕 (Widget构造结束!)";
}

/**
 * @brief Widget析构函数
 * @details 清理所有资源，关闭相机、停止线程、删除临时文件
 */
Widget::~Widget()
{
    qDebug() << "Widget destructor called";

    // 先停止线程并断开信号，避免窗口销毁时 queued signal 再访问 ui。
    bool myThreadStopped = true;
    bool cameraThreadStopped = true;
    if (myThread) {
        disconnect(myThread, nullptr, this, nullptr);
        disconnect(this, nullptr, myThread, nullptr);
        if (myThread->isRunning()) {
            myThread->requestStop();
            myThread->stop();
            if (!myThread->wait(3000)) {
                qDebug() << "WARNING: myThread did not stop in destructor";
                myThreadStopped = false;
            }
        }
        if (myThreadStopped && !myThread->isRunning()) {
            delete myThread;
        } else {
            myThread->setParent(nullptr);
        }
        myThread = nullptr;
    }

    if (cameraThread) {
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);
        if (cameraThread->isRunning()) {
            cameraThread->requestStop();
            if (!cameraThread->wait(3000)) {
                qDebug() << "WARNING: cameraThread did not stop in destructor";
                cameraThreadStopped = false;
            }
        }
        if (cameraThreadStopped && !cameraThread->isRunning()) {
            delete cameraThread;
        } else {
            cameraThread->setParent(nullptr);
        }
        cameraThread = nullptr;
    }

    // 线程退出后再关闭相机，避免工作线程仍在访问相机对象。
    if (m_pcMyCamera && myThreadStopped && cameraThreadStopped)
    {
        m_pcMyCamera->Close();
        delete m_pcMyCamera;
        m_pcMyCamera = nullptr;
    } else if (m_pcMyCamera) {
        qDebug() << "WARNING: camera not released because worker thread is still running";
    }

    if (client) {
        if (client->Connected()) {
            client->Disconnect();
        }
        delete client;
        client = nullptr;
    }

    delete templatematch;
    templatematch = nullptr;

    if (myThreadStopped) {
        delete myImage;
        myImage = nullptr;
    } else {
        qDebug() << "WARNING: myImage not released because myThread is still running";
    }

    try {
        cv::destroyAllWindows();
    } catch (...) {}

    delete ui;
    ui = nullptr;

    QString filePath = "muban.png";
    QFile file(filePath);
    if (file.exists())
    {
        file.remove();
    }

    qDebug() << "Widget destroyed";
}
/**
 * @brief 初始化Widget组件
 * @details 创建图像保存文件夹、初始化图像对象、创建工作线程、连接信号槽
 */
void Widget::initWidget()
{
    // 初始化设备打开标志
    m_bOpenDevice = false;

    // 创建图像保存文件夹
    QString imagePath = QDir::currentPath() + "/myImage/";
    QDir dstDir(imagePath);
    if (!dstDir.exists())
    {
        if (!dstDir.mkdir(imagePath))
        {
            qDebug() << "创建Image文件夹失败！";
        }
    }

    // 初始化图像指针
    myImage = new Mat();

    // 创建工作线程
    myThread = new MyThread();

    // 创建模板匹配对象
    templatematch = new TemplateMatch();

    // 连接线程信号槽 - 图像显示
    connect(myThread, &MyThread::signal_messImage, this, [this](cv::Mat img) {
        this->slot_displayAndDetect(&img);
    }, Qt::QueuedConnection);

    // 连接线程信号槽 - 图像检测（根据检测模式选择不同的处理函数）
    QObject::connect(myThread, &MyThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    });
    QObject::connect(myThread, &MyThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);


    // 连接其他信号槽
    connect(myThread, SIGNAL(signal_cleanlabel()), this, SLOT(slot_clearResultLabel()));
    connect(this, &Widget::rotate, myThread, &MyThread::receiveangle);
    connect(this, &Widget::choosechannel,myThread,&MyThread::receivecolorchannel1);
    connect(this, &Widget::sendDataTo, myThread, &MyThread::received);
    connect(this, &Widget::imgshibie, templatematch, &TemplateMatch::receshibie);
    connect(this, &Widget::caijianchicun, templatematch, &TemplateMatch::caijiansize);
    connect(this, &Widget::kernal, templatematch, &TemplateMatch::kernel);
    connect(this, &Widget::ssim, templatematch, &TemplateMatch::ssimvalue);




}



/**
 * @brief QImage转换为cv::Mat
 * @param image 输入的QImage对象
 * @return cv::Mat 转换后的OpenCV Mat对象
 */
cv::Mat QImage2cvMat(QImage image)
{
    cv::Mat mat;
    switch (image.format())
    {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        cv::Mat mat_temp = cv::Mat(image.height(), image.width(), CV_8UC4,
                                   (void *)image.constBits(), image.bytesPerLine());
        cvtColor(mat_temp, mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                      (void *)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_Indexed8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                      (void *)image.constBits(), image.bytesPerLine());
        break;
    }
    return mat;
}

/**
 * @brief QString转换为std::string
 * @param qstr 输入的QString
 * @return std::string 转换后的标准字符串
 */
string Widget::qstr2str(const QString qstr)
{
    QByteArray cdata = qstr.toLocal8Bit();
    return std::string(cdata);
}

/**
 * @brief 正确剔除操作（发送信号给PLC）
 * @details 向PLC写入0值，表示产品合格，停止定时器
 */
void Widget::rightremove()
{
    if (!client->Connected())
    {
        return;
    }

    uint8_t value = 0;
    byte remove_data[1] = {0};
    remove_data[0] = (unsigned char)(0xFF & value);

    // 写入DB1.1033位置，1个字节
    int tmp2 = client->WriteArea(S7AreaDB, 1, 1033, 1, S7WLByte, remove_data);
    if (tmp2 != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }
    timer->stop();
}

/**
 * @brief 错误剔除操作（发送信号给PLC）
 * @details 向PLC写入49值，表示产品不合格，需要剔除，100ms后恢复
 */
void Widget::wrongremove()
{
    if (!client->Connected())
    {
        return;
    }

    uint8_t value = 49;
    byte remove_data[1] = {0};
    remove_data[0] = (unsigned char)(0xFF & value);

    // 写入DB1.1033位置，1个字节
    int tmp2 = client->WriteArea(S7AreaDB, 1, 1033, 1, S7WLByte, remove_data);
    if (tmp2 != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }
    else
    {
        // 100ms后调用rightremove恢复信号
        timer->start(100);
    }
}

/**
 * @brief 保存ui图像（带选择框裁剪）
 * @param format 图像格式（如jpg、png、bmp）
 * @param savePath 保存路径
 * @details 根据用户绘制的选择框裁剪图像并保存为模板
 */
void Widget::saveImage(QString format, QString savePath)
{
    // 检查是否有图像可保存
    if (ui->image_undetected->pixmap() == nullptr)
    {
        QMessageBox::warning(this, "警告", "保存失败,未采集到图像!");
        return;
    }

    // 获取QLabel中显示的图像
    QPixmap pixmap = *(ui->image_undetected->pixmap());
    QImage img = pixmap.toImage();
    Mat* image = QImageToMat(img);  // 假设QImageToMat是正确的转换函数
    if (image == nullptr || image->empty())
    {
        QMessageBox::warning(this, "警告", "图像转换失败!");
        return;
    }

    // 处理格式字符串（去除可能的点号）
    if (format.startsWith("."))
    {
        format = format.mid(1);
    }

    // 确保保存目录存在
    QDir dir;
    if (!dir.mkpath(savePath))
    {
        qDebug() << "目录创建失败!";
        QMessageBox::warning(this, "警告", "保存失败,无法创建目录!");
        delete image;
        return;
    }

    // 获取用户在QLabel上绘制的选择框
    QRect selectionRect = imageLabel->getSelectionRect();
    if (selectionRect.isNull() || selectionRect.width() <= 0 || selectionRect.height() <= 0)
    {
        QMessageBox::warning(this, "警告", "未选择有效区域!");
        delete image;
        return;
    }

    // 关键修复：计算图像在QLabel中的实际显示尺寸和偏移（解决缩放/留白问题）
    // 1. 获取原始图像尺寸（OpenCV Mat: cols=宽, rows=高）
    QSize originalImageSize(image->cols, image->rows);
    // 2. 获取QLabel的显示尺寸
    QSize labelSize = imageLabel->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0)
    {
        QMessageBox::warning(this, "警告", "图像显示区域无效!");
        delete image;
        return;
    }

    // 3. 计算图像在QLabel中的实际缩放尺寸（按QLabel的缩放模式，通常是保持宽高比）
    // 注意：需与QLabel的实际缩放模式一致（如ui->image_undetected的scaledContents属性）
    QSize scaledImageSize = originalImageSize.scaled(labelSize, Qt::KeepAspectRatio);

    // 4. 计算图像在QLabel中的偏移量（因居中显示导致的留白补偿）
    int xOffset = (labelSize.width() - scaledImageSize.width()) / 2;   // 水平偏移（左留白）
    int yOffset = (labelSize.height() - scaledImageSize.height()) / 2; // 垂直偏移（上留白）

    // 5. 修正用户选择框：排除QLabel的留白区域，只保留图像显示区域内的部分
    QRectF adjustedRect(
                selectionRect.left() - xOffset,    // 减去水平偏移，得到相对于图像显示区域的X坐标
                selectionRect.top() - yOffset,     // 减去垂直偏移，得到相对于图像显示区域的Y坐标
                selectionRect.width(),
                selectionRect.height()
                );

    // 6. 确保修正后的区域完全在图像显示区域内（避免超出显示范围）
    QRectF validImageRect(0, 0, scaledImageSize.width(), scaledImageSize.height());
    adjustedRect = adjustedRect.intersected(validImageRect);
    if (adjustedRect.isNull() || adjustedRect.width() <= 0 || adjustedRect.height() <= 0)
    {
        QMessageBox::warning(this, "警告", "选择区域超出图像范围!");
        delete image;
        return;
    }

    // 7. 计算正确的缩放比例（原始图像尺寸 / 显示尺寸）
    double xRatio = static_cast<double>(originalImageSize.width()) / scaledImageSize.width();
    double yRatio = static_cast<double>(originalImageSize.height()) / scaledImageSize.height();

    // 8. 将修正后的区域转换为原始图像的ROI（OpenCV坐标）
    cv::Rect roi(
                static_cast<int>(adjustedRect.left() * xRatio),    // 原始图像中的X起点
                static_cast<int>(adjustedRect.top() * yRatio),     // 原始图像中的Y起点
                static_cast<int>(adjustedRect.width() * xRatio),   // 原始图像中的宽度
                static_cast<int>(adjustedRect.height() * yRatio)   // 原始图像中的高度
                );

    // 9. 最终校验：确保ROI在原始图像边界内
    roi &= cv::Rect(0, 0, image->cols, image->rows);
    if (roi.width <= 0 || roi.height <= 0)
    {
        QMessageBox::warning(this, "警告", "无效的裁剪区域!");
        delete image;
        return;
    }

    // 裁剪图像
    cv::Mat croppedImage = (*image)(roi);

    // 保存裁剪后的模板图像
    muban = &croppedImage;  // 注意：这里是指针引用，需确保croppedImage生命周期有效
    std::string savename = "muban.png";
    if (cv::imwrite(savename, croppedImage))
    {
        QMessageBox::information(this, "提示", "模板muban.png保存成功!");
    }
    else
    {
        QMessageBox::warning(this, "警告", "保存失败!可能不支持该图像格式或路径错误。");
    }

    // 释放资源
    delete image;
}

/**
 * @brief 保存当前显示的图像（无裁剪）
 * @param format 图像格式
 * @param savePath 保存路径
 * @details 保存完整的检测图像到指定目录，用于OK/NG样本收集
 */
//异步保存相机图像 减少耗时
void Widget::saveImage2Async(QString format, QString savePath)
{
    // 捕获当前相机状态和相关参数，避免异步过程中相机状态变化
    if (!m_pcMyCamera || !m_bOpenDevice)
    {
        qDebug() << "保存失败，相机对象无效或未打开";
        return;
    }

    // 确保格式正确
    if (format.startsWith("."))
    {
        format = format.mid(1);
    }

    if (format.isEmpty())
    {
        format = "png";
    }

    // 确保目录存在
    QDir dir;
    if (!dir.mkpath(savePath))
    {
        qDebug() << "目录创建失败！路径：" << savePath;
        return;
    }

    // 确保路径以斜杠结尾
    if (!savePath.endsWith("/") && !savePath.endsWith("\\"))
    {
        savePath += "/";
    }

    // 生成文件名
    QString curDate = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz");
    QString saveName = savePath + curDate + "." + format;

    // 使用QtConcurrent在后台线程执行图像获取和保存操作
    QtConcurrent::run([this, saveName, format]() {
        try {
            // 方法1：使用ReadBuffer代替GetImageBuffer
            cv::Mat capturedImage;
            int result = m_pcMyCamera->ReadBuffer(capturedImage);

            // 如果ReadBuffer失败，尝试使用GetImage（基于回调的方法）
            if (result != 0 || capturedImage.empty()) {
                qDebug() << "ReadBuffer失败，尝试使用GetImage...";
                capturedImage = m_pcMyCamera->GetImage();
            }

            // 检查图像是否有效
            if (capturedImage.empty()) {
                qDebug() << "获取图像失败：空图像";
                return;
            }

            // 在后台线程中进行图像转换和保存
            QImage qImage;
            if (capturedImage.channels() == 3) {
                cv::Mat rgbImage;
                cv::cvtColor(capturedImage, rgbImage, cv::COLOR_BGR2RGB);
                qImage = QImage(rgbImage.data, rgbImage.cols, rgbImage.rows,
                                static_cast<int>(rgbImage.step), QImage::Format_RGB888).copy();
            }
            else if (capturedImage.channels() == 1) {
                qImage = QImage(capturedImage.data, capturedImage.cols, capturedImage.rows,
                                static_cast<int>(capturedImage.step), QImage::Format_Grayscale8).copy();
            }
            else {
                qDebug() << "不支持的图像通道数：" << capturedImage.channels();
                return;
            }

            // 保存图像
            if (!qImage.save(saveName, format.toUpper().toStdString().c_str())) {
                qDebug() << "保存图像失败！";
            }
            else {
                qDebug() << "异步保存图像成功：" << saveName;
            }
        }
        catch(const std::exception& e) {
            qDebug() << "异步保存过程中发生异常:" << e.what();
        }
        catch(...) {
            qDebug() << "异步保存过程中发生未知异常";
        }
    });
}



//// 在 Widget 或需要保存图像的地方调用
//void Widget::saveImageByMVS(QString savePath, QString format)
//{
//    int nRet = MV_OK;

//    // 相机状态校验
//    if (!m_pcMyCamera || !m_bOpenDevice) {
//        qDebug() << "保存失败：相机未打开";
//        return;
//    }

//    // 1. 格式预处理（对齐第二个函数：仅去前缀、判空，保留原白名单校验增强兼容性）
//    format = format.trimmed();
//    if (format.startsWith(".")) {
//        format = format.mid(1);
//    }
//    if (format.isEmpty()) {
//        format = "png";
//    }
//    // 保留原白名单校验（避免无效格式，比第二个函数更严谨）
//    QStringList validFormats = {"bmp", "jpeg", "png", "tiff"};
//    if (!validFormats.contains(format.toLower())) {
//        qDebug() << "unsupport format" << format << "，默认使用 png";
//        format = "png";
//    }

//    // 2. 确保目录存在（对齐第二个函数：先处理目录，再补全路径）
//    QDir dir;
//    if (!dir.mkpath(savePath)) {
//        qDebug() << "path create fail" << savePath;
//        return;
//    }

//    // 3. 补全路径分隔符（对齐第二个函数：目录创建后补全）
//    if (!savePath.endsWith("/") && !savePath.endsWith("\\")) {
//        savePath += "/";
//    }

//    // 4. 生成完整路径（与第二个函数完全一致：时间戳格式、命名规则）
//    QString curDate = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz");
//    QString fullSavePath = savePath + curDate + "." + format;

//    // 关键修正：先声明 saveParam，再用其成员做路径长度检查
//    MV_SAVE_IMG_TO_FILE_PARAM saveParam;
//    memset(&saveParam, 0, sizeof(MV_SAVE_IMG_TO_FILE_PARAM)); // 整体清零，避免枚举类型初始化错误

//    // 路径长度检查（保留原严谨性，避免缓冲区溢出）
//    if (fullSavePath.toLocal8Bit().length() >= sizeof(saveParam.pImagePath)) {
//        qDebug() << "path too long" << fullSavePath;
//        return;
//    }

//    // 定义帧信息结构体
//    MV_FRAME_OUT stFrameOut;
//    memset(&stFrameOut, 0, sizeof(MV_FRAME_OUT)); // 整体清零，包含嵌套子结构体

//    // 获取图像
//    nRet = m_pcMyCamera->GetImageBuffer(&stFrameOut, 1000);
//    if (nRet != MV_OK) {
//        qDebug() << "get image fail wrong info：" << nRet;
//        return;
//    }

//    // 检查帧数据有效性
//    if (!stFrameOut.pBufAddr) {
//        qDebug() << "get image fail data is empty";
//        m_pcMyCamera->FreeImageBuffer(&stFrameOut);
//        return;
//    }

//    // 填充 saveParam 其他参数
//    saveParam.enPixelType = static_cast<enum MvGvspPixelType>(stFrameOut.stFrameInfo.enPixelType);
//    saveParam.pData = stFrameOut.pBufAddr;
//    saveParam.nDataLen = stFrameOut.stFrameInfo.nFrameLen;
//    saveParam.nWidth = stFrameOut.stFrameInfo.nWidth;
//    saveParam.nHeight = stFrameOut.stFrameInfo.nHeight;

//    // 映射保存格式
//    QString lowerFormat = format.toLower();
//    if (lowerFormat == "bmp") saveParam.enImageType = MV_Image_Bmp;
//    else if (lowerFormat == "jpeg") saveParam.enImageType = MV_Image_Jpeg;
//    else if (lowerFormat == "png") saveParam.enImageType = MV_Image_Png;
//    else if (lowerFormat == "tiff") saveParam.enImageType = MV_Image_Tif;

//    // 编码质量
//    if (lowerFormat == "jpeg") saveParam.nQuality = 60;
//    else if (lowerFormat == "png") saveParam.nQuality = 5;
//    else saveParam.nQuality = 0;

//    // 保存路径（与原逻辑一致，确保安全拷贝）
//    const char* filePath = fullSavePath.toLocal8Bit().data();
//    strncpy_s(saveParam.pImagePath, filePath, sizeof(saveParam.pImagePath) - 1);
//    saveParam.pImagePath[sizeof(saveParam.pImagePath) - 1] = '\0';

//    // 插值方法
//    saveParam.iMethodValue = 1;

//    // 保存图像
//    nRet = m_pcMyCamera->SaveImageToFile(&saveParam);
//    if (nRet == MV_OK) {
//        qDebug() << "save success：" << fullSavePath;
//    } else {
//        qDebug() << "save fail wrong info：" << nRet;
//    }

//    // 释放缓冲区
//    m_pcMyCamera->FreeImageBuffer(&stFrameOut);
//}



////异步保存ui上的图像
void Widget::saveImage2(QString format, QString savePath)
{
    const QString fileBaseName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    saveImage2(format, savePath, fileBaseName);
}

void Widget::saveImage2(QString format, QString savePath, const QString &fileBaseName)
{
    // 1. 主线程中先校验UI图像和参数（避免跨线程访问UI）
    const QPixmap* curPixmap = ui->image_undetected->pixmap();
    if (!curPixmap) {
        QMessageBox::warning(this, "警告", "保存失败,未采集到图像！");
        return;
    }

    // 复制UI图像到主线程局部变量（避免跨线程访问UI控件）
    QPixmap pixmap = *curPixmap;
    QImage img = pixmap.toImage();

    // 处理文件格式
    if (format.startsWith(".")) {
        format = format.mid(1);
    }
    if (format.isEmpty()) {
        format = "png"; // 默认格式
    }

    // 确保目录存在
    QDir dir;
    if (!dir.mkpath(savePath)) {
        qDebug() << "目录创建失败！路径：" << savePath;
        return;
    }

    // 确保路径以斜杠结尾
    if (!savePath.endsWith("/") && !savePath.endsWith("\\")) {
        savePath += "/";
    }

    // 生成带时间戳的文件名（主线程生成，避免线程安全问题）
    QString baseName = fileBaseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    }
    QString saveName = savePath + baseName + "." + format;

    // 2. 使用QtConcurrent在后台线程执行保存操作（核心异步逻辑）
    QtConcurrent::run([=]() { // 捕获复制后的局部变量，避免跨线程访问UI
        try {
            // 后台线程中执行保存（QImage是可重入的，支持跨线程操作）
            if (img.save(saveName, format.toUpper().toStdString().c_str())) {
                qDebug() << "UI图像异步保存成功：" << saveName;
            } else {
                qDebug() << "UI图像异步保存失败！路径：" << saveName;
            }
        } catch (const std::exception& e) {
            qDebug() << "UI图像异步保存异常:" << e.what();
        } catch (...) {
            qDebug() << "UI图像异步保存发生未知异常";
        }
    });
}

void Widget::saveRawImage(QString format, QString savePath, const cv::Mat &image, const QString &fileBaseName)
{
    if (image.empty()) {
        qDebug() << "无框原图保存失败，图像为空";
        return;
    }

    QImage img = cvMatToQImage(image);
    if (img.isNull()) {
        qDebug() << "无框原图保存失败，图像格式不支持";
        return;
    }

    if (format.startsWith(".")) {
        format = format.mid(1);
    }
    if (format.isEmpty()) {
        format = "png";
    }

    QDir dir;
    if (!dir.mkpath(savePath)) {
        qDebug() << "无框原图目录创建失败！路径：" << savePath;
        return;
    }

    if (!savePath.endsWith("/") && !savePath.endsWith("\\")) {
        savePath += "/";
    }

    QString baseName = fileBaseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    }
    QString saveName = savePath + baseName + "." + format;

    QtConcurrent::run([=]() {
        try {
            if (img.save(saveName, format.toUpper().toStdString().c_str())) {
                qDebug() << "无框原图异步保存成功：" << saveName;
            } else {
                qDebug() << "无框原图异步保存失败！路径：" << saveName;
            }
        } catch (const std::exception& e) {
            qDebug() << "无框原图异步保存异常:" << e.what();
        } catch (...) {
            qDebug() << "无框原图异步保存发生未知异常";
        }
    });
}

bool Widget::shouldSaveRecognitionBoxImage() const
{
    if (!ui || !ui->comboBox_saveImageType) {
        return true;
    }

    const int index = ui->comboBox_saveImageType->currentIndex();
    return index == 0 || index == 1;
}

bool Widget::shouldSaveNoRecognitionBoxImage() const
{
    if (!ui || !ui->comboBox_saveImageType) {
        return false;
    }

    const int index = ui->comboBox_saveImageType->currentIndex();
    return index == 0 || index == 2;
}

void Widget::saveResultImages(QString format, const QString &resultDirName, const cv::Mat &image)
{
    if (selectedDir.trimmed().isEmpty()) {
        qDebug() << "检测图像保存失败，图像保存路径为空";
        return;
    }

    QString resultName = resultDirName.trimmed();
    if (resultName.isEmpty()) {
        resultName = "unknown";
    }

    const QString fileBaseName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    if (shouldSaveRecognitionBoxImage()) {
        saveImage2(format, selectedDir + "/" + resultName + "/", fileBaseName);
    }
    if (shouldSaveNoRecognitionBoxImage()) {
        saveRawImage(format, selectedDir + "/" + resultName + "_raw/", image, fileBaseName);
    }
}

void Widget::saveWordResultImages(QString format, const QString &resultDirName, const cv::Mat &image)
{
    saveResultImages(format, resultDirName, image);
}

/**
 * @brief 显示图像槽函数
 * @param image OpenCV Mat图像指针
 * @details 将OpenCV图像转换为QPixmap并显示在UI上
 */
void Widget::slot_displayAndDetect(cv::Mat *image)
{
    // 1. 校验图像有效性
    if (!image || image->empty()) return;

    // 纸巾检测生产运行时，画面应当和检测结果绑定。
    // 普通预览帧不再覆盖界面，只有检测槽主动放行的那一帧会显示。
    const bool tissueMode = ui->comboBox_4->currentIndex() == 3;
    const bool productionRunning = isCollecting || !ui->plcbtn->isEnabled();
    if (tissueMode && productionRunning && !g_allowTissueDetectionFrameDisplay) {
        return;
    }

    // 2. 深拷贝原图，准备作为画板
    cv::Mat displayImg;
    if (image->channels() == 3) {
        displayImg = image->clone();
    } else if (image->channels() == 1) {
        cv::cvtColor(*image, displayImg, cv::COLOR_GRAY2BGR);
    } else {
        return;
    }

    // 3. 核心重绘机制：只要缓存里还有上一轮检测结果，就持续绘制，直到被新结果覆盖或主动清空。
    if (!g_lastDrawResults.empty() ||
        !g_lastPose.trackingPoly.empty() ||
        !g_lastPose.datePoly.empty() ||
        !g_lastStampPoly.empty() ||
        (tissueMode && g_hasLastTissueRoll)) {

        // 动态计算自适应比例
        double dynamicScale = std::max(1.0, displayImg.rows / 800.0);
        double fontScale = 0.4 * dynamicScale;

        // 框和字的粗细
        int boxThickness = std::max(2, static_cast<int>(2 * dynamicScale));
        int textThickness = std::max(1, static_cast<int>(1.5 * dynamicScale));

        for (const auto& res : g_lastDrawResults) {
            if (res.poly.size() < 4) {
                continue;
            }

            // 画字符绿框
            std::vector<std::vector<cv::Point>> charPolys = {res.poly};
            cv::polylines(displayImg, charPolys, true, cv::Scalar(0, 255, 0), boxThickness);

            // 分数大于等于0才显示数字 (带描边显示)
            if (res.score >= 0) {
                std::string scoreText = std::to_string(static_cast<int>(res.score * 100));
                int baseline = 0;
                cv::Size textSize = cv::getTextSize(scoreText, cv::FONT_HERSHEY_SIMPLEX, fontScale, textThickness, &baseline);

                cv::Point textAnchor = getPolygonTopCenter(res.poly);
                int textX = std::max(0, std::min(textAnchor.x - textSize.width / 2, displayImg.cols - textSize.width));
                int textY = std::max(textSize.height, std::min(textAnchor.y - 5, displayImg.rows));

                cv::putText(displayImg, scoreText, cv::Point(textX, textY),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(0, 0, 0), textThickness + 2);
                cv::putText(displayImg, scoreText, cv::Point(textX, textY),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(0, 255, 255), textThickness);
            }
        }

        if (!g_lastPose.trackingPoly.empty()) {
            std::vector<std::vector<cv::Point>> trackingPolys = {g_lastPose.trackingPoly};
            cv::polylines(displayImg, trackingPolys, true, cv::Scalar(255, 0, 0), boxThickness);
        }

        // ================== 绘制喷码检测区域 ==================
        if (!g_lastPose.datePoly.empty()) {
            std::vector<std::vector<cv::Point>> datePolys = {g_lastPose.datePoly};
            cv::polylines(displayImg, datePolys, true, cv::Scalar(0, 255, 0), boxThickness);
        }

        // ================== 绘制钢印多边形 ==================
        if (!g_lastStampPoly.empty()) {
            // 正常颜色为黄色，重叠则显示红色
            cv::Scalar stampColor = g_lastStampIsOverlap ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 255);
            std::vector<std::vector<cv::Point>> polys = {g_lastStampPoly};
            cv::polylines(displayImg, polys, true, stampColor, boxThickness);
        }

        if (tissueMode && g_hasLastTissueRoll) {
            const TissueRollItem& roll = g_lastTissueRoll;
            const int outerRadius = std::max(1, cvRound(std::max(roll.outerAxes.width, roll.outerAxes.height)));
            const int innerRadius = std::max(1, cvRound(std::max(roll.innerAxes.width, roll.innerAxes.height)));

            cv::circle(displayImg,
                       cv::Point(cvRound(roll.center.x), cvRound(roll.center.y)),
                       outerRadius,
                       cv::Scalar(0, 255, 255), boxThickness);
            cv::circle(displayImg,
                       cv::Point(cvRound(roll.innerCenter.x), cvRound(roll.innerCenter.y)),
                       innerRadius,
                       cv::Scalar(255, 0, 0), boxThickness);
        }
    }

    // 4. OpenCV Mat 转 Qt QImage 显示
    QImage img((const uchar *)displayImg.data, displayImg.cols, displayImg.rows, displayImg.step, QImage::Format_RGB888);
    img = img.rgbSwapped();

    // 🔥 【核心修改：这里彻底删除了 QPainter 绘制“日期”和“钢印”中文标签的所有代码】 🔥

    // 5. 渲染到 UI
    QSize labelSize = ui->image_undetected->size();
    QPixmap pixmap = QPixmap::fromImage(img);
    QPixmap scaledPixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    ui->image_undetected->setScaledContents(false);
    ui->image_undetected->setAlignment(Qt::AlignCenter);
    ui->image_undetected->setPixmap(scaledPixmap);
    if (!imageLabel || !imageLabel->isTemplateDrawingEnabled()) {
        updateImageDisplayStatusText("正在显示相机采集图像...");
    }
}


/**
 * @brief 根据 comboBox_4 当前识别模式分发检测逻辑
 * @param image 输入图像指针
 * @param pose 当前检测姿态
 */
void Widget::dispatchDetectionByMode(cv::Mat *image, DetectionPose pose)
{
    if (!image || image->empty()) {
        qDebug() << "[DETECTION_DISPATCH] Empty image, skip detection.";
        return;
    }

    const int mode = ui->comboBox_4->currentIndex();
    if (mode == 2) {
        slot_readAndDetect(image, pose);
    } else if (mode == 0) {
        slot_readAndDetect3(image, pose);
    } else if (mode == 1) {
        if (pose.wordTemplateProfileIndex >= 0) {
            const int profileIndex = pose.wordTemplateProfileIndex;
            if (m_wordMultiTemplateMode
                    && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
                const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
                if (profile.targetText.trimmed().isEmpty() || profile.digitTemplates.empty()) {
                    qDebug() << "[WORD_MULTI_TEMPLATE] Selected profile has no target text/templates:"
                             << profileIndex
                             << profile.name;
                    return;
                }

                qDebug() << "[WORD_MULTI_TEMPLATE] Widget dispatch profile:"
                         << profileIndex
                         << profile.name
                         << "score:" << pose.score;

                const QString currentUsedTemplateName = profile.name.isEmpty()
                        ? QDir(profile.dirPath).dirName()
                        : profile.name;
                ui->currentTemplateName->setText(currentUsedTemplateName.isEmpty()
                                                 ? QString("--")
                                                 : currentUsedTemplateName);

                runWordTemplateDetection(image,
                                         pose,
                                         profile.digitTemplates,
                                         profile.digitTemplateTargetIndexes,
                                         profile.targetText,
                                         profile.imageThresholdText,
                                         currentUsedTemplateName);
                return;
            }

            qDebug() << "[WORD_MULTI_TEMPLATE] Invalid profile index from pose:"
                     << profileIndex
                     << "profile count:" << static_cast<int>(m_wordTemplateProfiles.size());
            return;
        }

        slot_readAndDetect4(image, pose);
    } else if (mode == 3) {
        qDebug() << "[DETECTION_DISPATCH] Tissue mode is handled in worker thread.";
    } else {
        qDebug() << "[DETECTION_DISPATCH] Unsupported comboBox_4 index:" << mode;
    }
}

/**
 * @brief OCR识别检测槽函数
 * @param image 输入图像指针
 * @param pose 当前检测姿态
 * @details 使用PaddleOCR进行文字识别，支持中英文、数字识别
 */
void Widget::slot_readAndDetect(cv::Mat *image, DetectionPose pose)
{
    // 1. 检查延迟剔除队列
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1)
    {
        qDebug() << "[PLC_LOG] Triggering delayed wrongremove, wrongindex:" << wrongindex;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty())
    {
        qDebug() << "[OCR_ERROR] Invalid input image. Image is null or empty.";
        return;
    }

    // 按周期清理数据，不再清理 imageLabel 的矩形，因为不再绘制
    if (judge)
    {
        j = 1;
        x++;
        judge = false;
    }
    if ((j - 1) % x == 0)
    {
        // imageLabel->clearGreenRects(); // 去掉框显示，不再需要清理
        detectedRects.clear();
        string1.clear();
    }

    qDebug() << "----------------- OCR PROCESS START -----------------";
    OrientedDateRoi oriented = prepareOrientedDateRoi(*image, pose, 0);
    if (!oriented.valid) {
        qDebug() << "[OCR_ERROR] Invalid selection area!";
        return;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();
    g_lastPose = pose;
    g_lastDrawResults.clear();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;

    // ================== 2. 执行 OCR 识别 (原生 Run API) ==================
    QString target_qstring = setdatetime();
    std::string target_string = target_qstring.toStdString();
    ui->imagenum->setText(QString::number(totalImages));

    std::vector<std::vector<std::vector<int>>> boxes;
    det->Run(croppedImage, boxes);

    // 使用原生 Run 函数确保识别率与 MainWindow 一致
    std::vector<std::string> raw_str_res;
    rec->Run(boxes, croppedImage, cls, raw_str_res);

    // 只需要提取字符串，不需要再计算坐标 Rect 映射到 UI 了
    std::vector<std::string> sorted_res = raw_str_res;
    // 如果有多行文字，可以根据 boxes 里的 y 坐标对 raw_str_res 进行排序，
    // 这里为了简洁，假设识别顺序正常，直接处理结果。

    allResults.clear();

    // ================== 3. 结果清洗与拼接 ==================
    for (size_t i = 0; i < sorted_res.size(); i++)
    {
        std::string res_str = sorted_res[i];

        // 过滤字符
        res_str.erase(std::remove_if(res_str.begin(), res_str.end(), [this](char c)
        {
            return !(isAlnumOrChinese(c) || c == '-' || c == '.' || c == ':');
        }), res_str.end());

        if (res_str.empty()) continue;

        if (!allResults.empty()) allResults += '\n';
        allResults += res_str;
    }

    // ================== 4. UI 文本更新与 PLC 判定 ==================
    ui->resultlabel_7->setText(QString::fromStdString(allResults));
    ui->resultlabel_7->setWordWrap(true);

    // 🔥 此处删掉了 imageLabel->addSelectionRect 和 imageLabel->update()
    // 界面上不会再出现任何检测框

    qDebug() << "[OCR_LOG] Final String:" << QString::fromStdString(allResults);

    // PLC 判定及存图逻辑
    if (j % x == 0)
    {
        if (allResults.empty())
        {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3))
                saveImage2Async("jpg", selectedDir + "/ng/");

            ngImages++;
            totalImages++;
            ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        }
        else
        {
            if (allResults == target_string)
            {
                totalImages++;
                if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3))
                    saveImage2Async("jpg", selectedDir + "/ok/");

                ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font><br>"));
                rightremove();
            }
            else
            {
                if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3))
                    saveImage2Async("jpg", selectedDir + "/ng/");

                ngImages++;
                totalImages++;
                ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
                if (wrongindex == 0) wrongremove();
                else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
            }
        }
    }

    // 更新统计
    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0.0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString("%1").arg(ngImages));
    ui->imagenum->setText(QString("%1").arg(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    qDebug() << "----------------- OCR PROCESS END -----------------";
    j++;
}

/**
 * @brief 钢印检测槽函数
 * @param image 输入图像指针
 * @param diffbox 检测区域
 * @details 使用SSIM算法进行模板相似度匹配
 */

//原始图像版
void Widget::slot_readAndDetect3(cv::Mat *image, DetectionPose pose)
{
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        qDebug() << "PLC延迟剔除触发，当前总数:" << totalImages;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) return;

    if (judge) { j = 1; x++; judge = false; }
    if ((j - 1) % x == 0) {
        imageLabel->clearGreenRects();
        detectedRects.clear();
        string1.clear();
    }
    
    OrientedDateRoi oriented = prepareOrientedDateRoi(*image, pose, 20);
    if (!oriented.valid) {
        QMessageBox::warning(this, "警告", "识别区域超出原图范围！");
        return;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();

    emit imgshibie(&croppedImage);
    ui->imagenum->setText(QString::number(totalImages));

    QString targetString = ui->dateEdit->toPlainText();
    int targetNum = 0;
    QRegularExpression regex(R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matchIt = regex.globalMatch(targetString);
    while (matchIt.hasNext()) { matchIt.next(); targetNum++; }
    if (targetNum == 0 && !targetString.isEmpty()) targetNum = targetString.length();

    int detectNum = templatematch->run3(digitTemplates);
    bool charIsOk = (detectNum == targetNum);

    // ===================== 2. 钢印防重叠检测 (完全无 Padding) =====================
    bool overlapIsOk = false;
    g_lastStampPoly.clear();

    if (!QFile::exists(currentTemplateDirPath + "/calibrate_config.yaml")) {
        qDebug() << "[ERROR] Missing overlap config!";
        overlapIsOk = false;
    } else {
        DetectResult overlapRes = overlapDetector.processImage(*image, pose.datePoly);
        overlapIsOk = overlapRes.isOk;

        g_lastStampPoly = overlapRes.finalStampPoly;
        g_lastStampIsOverlap = !overlapIsOk;
    }

    // ===================== 3. UI 数据更新与画面重绘 =====================
    g_lastDrawResults = mapMatchResultsToOriginal(templatematch->lastMatchResults, oriented, image->size());
    g_lastPose = pose;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    slot_displayAndDetect(image);

    // ===================== 4. 综合判定与 PLC 剔除输出 =====================
    if (j % x == 0) {
        if (!charIsOk || !overlapIsOk) {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ng", *image);
            }
            ngImages++;
            totalImages++;

            if (!charIsOk && overlapIsOk) ui->resultlabel->setText(QString("<font size='10' color='red'>错误(喷码不合格)</font>"));
            else if (charIsOk && !overlapIsOk) ui->resultlabel->setText(QString("<font size='10' color='red'>错误(钢印重叠)</font>"));
            else ui->resultlabel->setText(QString("<font size='10' color='red'>错误(喷码与钢印均不合格)</font>"));

            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ok", *image);
            }
            ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font><br>"));
            rightremove();
        }
    }

    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}



/**
 * @brief 字库匹配检测槽函数
 * @param image 输入图像指针
 * @param diffbox 检测区域
 * @details 使用字符模板库进行字符数量匹配检测
 */
void Widget::slot_readAndDetect4(cv::Mat *image, DetectionPose pose)
{
    runWordTemplateDetection(image,
                             pose,
                             digitTemplates,
                             digitTemplateTargetIndexes,
                             ui->dateEdit->toPlainText(),
                             ui->lineEdit_yuzhi->text(),
                             QDir(currentTemplateDirPath).dirName());
}

void Widget::runWordTemplateDetection(cv::Mat *image,
                                      const DetectionPose &pose,
                                      const std::vector<cv::Mat> &templates,
                                      const std::vector<int> &templateTargetIndexes,
                                      const QString &targetString,
                                      const QString &imageThresholdText,
                                      const QString &templateName)
{
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) {
        qDebug().noquote() << "[WORD_DETECT] skip: input image is empty";
        return;
    }

    if (judge) { j = 1; x++; judge = false; }
    if ((j - 1) % x == 0) {
        imageLabel->clearGreenRects();
        detectedRects.clear();
        string1.clear();
    }

    OrientedDateRoi oriented = prepareOrientedDateRoi(*image, pose, 20);
    if (!oriented.valid) {
        qDebug().noquote() << QString("[WORD_DETECT] template=%1 result=NG reason=日期ROI无效或超出原图范围 poseValid=%2 poseScore=%3")
                              .arg(templateName.isEmpty() ? QString("--") : templateName)
                              .arg(pose.valid ? QString("true") : QString("false"))
                              .arg(pose.score, 0, 'f', 4);
        return;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();
    emit imgshibie(&croppedImage);
    ui->imagenum->setText(QString::number(totalImages));

    int targetNum = 0;
    QRegularExpression regex(R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matchIt = regex.globalMatch(targetString);
    while (matchIt.hasNext()) { matchIt.next(); targetNum++; }
    if (targetNum == 0 && !targetString.isEmpty()) targetNum = targetString.length();

    bool thresholdOk = false;
    int thresholdValue = static_cast<int>(imageThresholdText.trimmed().toDouble(&thresholdOk));
    if (!thresholdOk) {
        thresholdValue = static_cast<int>(ui->lineEdit_yuzhi->text().trimmed().toDouble(&thresholdOk));
    }
    if (thresholdOk) {
        templatematch->ssimvalue(thresholdValue);
    }

    int detectNum = templatematch->run3(templates, templateTargetIndexes);
    QString judgeResult = (detectNum == targetNum ? "ok" : "no");

    const QStringList targetUnits = parseWordTemplateBaseNames(targetString);
    QStringList detectedUnits;
    QStringList matchDetails;
    detectedUnits.reserve(static_cast<int>(templatematch->lastMatchResults.size()));
    matchDetails.reserve(static_cast<int>(templatematch->lastMatchResults.size()));
    for (int i = 0; i < static_cast<int>(templatematch->lastMatchResults.size()); ++i) {
        const auto &match = templatematch->lastMatchResults[static_cast<size_t>(i)];
        const cv::Rect rect = std::get<0>(match);
        const double score = std::get<1>(match);
        const int targetIndex = static_cast<int>(std::get<2>(match));
        const QString unit = targetUnits.value(targetIndex, QString("#%1").arg(targetIndex));
        detectedUnits.append(unit);
        matchDetails.append(QString("%1:%2 score=%3 rect=(%4,%5,%6,%7) targetIndex=%8")
                            .arg(i + 1)
                            .arg(unit)
                            .arg(score, 0, 'f', 3)
                            .arg(rect.x)
                            .arg(rect.y)
                            .arg(rect.width)
                            .arg(rect.height)
                            .arg(targetIndex));
    }

    QStringList missingUnits;
    for (int i = 0; i < targetUnits.size(); ++i) {
        bool found = false;
        for (const auto &match : templatematch->lastMatchResults) {
            if (static_cast<int>(std::get<2>(match)) == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            missingUnits.append(QString("%1:%2").arg(i + 1).arg(targetUnits.at(i)));
        }
    }

    QString reason;
    if (judgeResult == "ok") {
        reason = "识别数量等于目标数量";
    } else if (detectNum < targetNum) {
        reason = QString("识别数量少于目标数量，少%1个").arg(targetNum - detectNum);
    } else {
        reason = QString("识别数量多于目标数量，多%1个").arg(detectNum - targetNum);
    }
    if (!missingUnits.isEmpty()) {
        reason += QString("；未匹配目标=%1").arg(missingUnits.join(", "));
    }

    qDebug().noquote() << QString("[WORD_DETECT] template=%1 result=%2 reason=%3 targetText=\"%4\" targetCount=%5 detectedCount=%6 threshold=%7 poseScore=%8 roi=(%9,%10,%11,%12) detected=\"%13\"")
                          .arg(templateName.isEmpty() ? QString("--") : templateName)
                          .arg(judgeResult == "ok" ? QString("OK") : QString("NG"))
                          .arg(reason)
                          .arg(targetString)
                          .arg(targetNum)
                          .arg(detectNum)
                          .arg(thresholdOk ? QString::number(thresholdValue) : QString("无效"))
                          .arg(pose.score, 0, 'f', 4)
                          .arg(oriented.roi.x)
                          .arg(oriented.roi.y)
                          .arg(oriented.roi.width)
                          .arg(oriented.roi.height)
                          .arg(detectedUnits.join(""));
    qDebug().noquote() << QString("[WORD_DETECT_DETAIL] %1")
                          .arg(matchDetails.isEmpty() ? QString("no matched boxes") : matchDetails.join(" | "));

    g_lastDrawResults = mapMatchResultsToOriginal(templatematch->lastMatchResults, oriented, image->size());
    g_lastPose = pose;
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    slot_displayAndDetect(image);

    if (j % x == 0) {
        if (judgeResult == "no") {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveWordResultImages("png", "ng", *image);
            }
            ngImages++;
            totalImages++;
            ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveWordResultImages("png", "ok", *image);
            }
            ui->resultlabel->setText(QString("<font size='10' color='SpringGreen'>正确！</font><br>"));
            rightremove();
        }
    }

    double hegerate = (totalImages > 0) ? (1 - static_cast<double>(ngImages) / totalImages) * 100 : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}

/**
 * @brief 纸巾卷检测结果处理槽函数
 * @param image 输入整张相机图像
 * @param tissueResult 工作线程计算出的纸巾检测结果
 */
void Widget::slot_handleTissueResult(cv::Mat *image, TissueRollResult tissueResult)
{
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
        qDebug() << "[TISSUE_DETECT] Delayed wrongremove triggered, wrongindex:" << wrongindex;
        wrongremove();
        removalQueue.pop();
    }

    currentImagesSnapshot = totalImages;
    auto start = std::chrono::high_resolution_clock::now();

    if (!image || image->empty()) {
        qDebug() << "[TISSUE_DETECT] Invalid input image.";
        return;
    }

    if (judge) {
        j = 1;
        x++;
        judge = false;
    }
    if ((j - 1) % x == 0) {
        detectedRects.clear();
        string1.clear();
    }

    const bool isOk = tissueResult.isOk;

    g_lastDrawResults.clear();
    g_lastPose = DetectionPose();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    if (tissueResult.rollFound) {
        g_lastTissueRoll = tissueResult.roll;
        g_hasLastTissueRoll = true;
    } else {
        g_lastTissueRoll = TissueRollItem();
        g_hasLastTissueRoll = false;
    }
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    ui->resultlabel->setText(isOk ? "OK" : "NG");
    ui->resultlabel->setStyleSheet(isOk
        ? "background-color: #eef1f6; border-radius: 6px; font-size: 36px; font-weight: 900; color: #20b455;"
        : "background-color: #eef1f6; border-radius: 6px; font-size: 36px; font-weight: 900; color: #ff4d4f;");
    ui->resultlabel->setWordWrap(true);
    if (tissueResult.rollFound) {
        ui->resultlabel_7->setText(QString("粗糙度：%1").arg(tissueResult.roll.roughnessScore, 0, 'f', 3));
    } else {
        ui->resultlabel_7->setText("粗糙度：--");
    }
    ui->resultlabel_7->setWordWrap(true);

    g_allowTissueDetectionFrameDisplay = true;
    slot_displayAndDetect(image);
    g_allowTissueDetectionFrameDisplay = false;

    qDebug() << "[TISSUE_DETECT]" << QString::fromStdString(tissueResult.message);
    qDebug() << "[TISSUE_DETECT_DEBUG]"
             << "image" << tissueResult.imageWidth << "x" << tissueResult.imageHeight
             << "processingTimeMs" << tissueResult.processingTimeMs
             << "rollFound" << tissueResult.rollFound
             << "overall" << (tissueResult.isOk ? "OK" : "NG");
    if (tissueResult.rollFound) {
        const TissueRollItem& roll = tissueResult.roll;
        qDebug() << "[TISSUE_DETECT_DEBUG]"
                 << (roll.isOk ? "OK" : "NG")
                 << "reason" << QString::fromStdString(roll.rejectReason)
                 << "rough" << roll.roughnessScore
                 << "roughNg" << roll.roughnessNg
                 << "ringPixels" << roll.ringPixelCount
                 << "outerCenter" << roll.center.x << roll.center.y
                 << "outerRadius" << roll.outerAxes.width
                 << "outerBbox" << roll.outerBbox.x << roll.outerBbox.y
                 << roll.outerBbox.width << roll.outerBbox.height
                 << "innerFound" << roll.innerHoleFound
                 << "innerCenter" << roll.innerCenter.x << roll.innerCenter.y
                 << "innerRadius" << roll.innerAxes.width;
    }

    if (j % x == 0) {
        if (!isOk) {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ng", *image);
            }
            ngImages++;
            totalImages++;

            if (wrongindex == 0) {
                wrongremove();
            } else {
                removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
            }
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3)) {
                saveResultImages("png", "ok", *image);
            }
            rightremove();
        }
    }

    const double hegerate = (totalImages > 0)
        ? (1 - static_cast<double>(ngImages) / totalImages) * 100
        : 0;
    ui->lineBoxIndex_6->setText(QString::number(hegerate, 'f', 1));
    ui->ngnum->setText(QString::number(ngImages));
    ui->imagenum->setText(QString::number(totalImages));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (tissueResult.processingTimeMs > 0) {
        duration = tissueResult.processingTimeMs;
    }
    ui->speedLabel->setText(QString("检测耗时 %1 毫秒").arg(duration));

    j++;
}

/**
 * @brief 软触发拍照按钮点击槽函数
 * @details 发送软触发信号给相机，采集一张图像并进行识别
 */
void Widget::on_VideoShoot_clicked()
{
    if (!m_bOpenDevice) {
        QMessageBox::warning(this, "警告", "采集失败,请打开设备！");
        return;
    }

    // 设置曝光（建议在拍照前确保设置生效）
    int exposureValue = ui->spinBox->value();
    m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);

    try {
        m_pcMyCamera->SetEnumValue("TriggerMode", 1);
        m_pcMyCamera->SetEnumValue("TriggerSource", 7); // 软触发
    } catch (...) {
        QMessageBox::warning(this, "警告", "相机配置失败！");
        return;
    }

    // 执行触发
    m_pcMyCamera->CommandExecute("TriggerSoftware");

    // 等待图像传输完成（根据你的相机性能调整）
    QThread::msleep(ui->spinBox->value() / 1000 + 100);

    // 🔥 核心修改：将采集到的图像存入类成员变量 myImage，而不是局部变量
    // 这样图像就能在函数结束后继续存在于内存中
    *myImage = m_pcMyCamera->GetImage();

    if (myImage->empty()) {
        QMessageBox::warning(this, "警告", "未能获取有效图像！");
        return;
    }

    // 处理旋转逻辑（直接作用于成员变量）
    int rotationIndex = ui->comboBox_2->currentIndex();
    if (rotationIndex == 1) cv::rotate(*myImage, *myImage, cv::ROTATE_90_CLOCKWISE);
    else if (rotationIndex == 2) cv::rotate(*myImage, *myImage, cv::ROTATE_90_COUNTERCLOCKWISE);
    else if (rotationIndex == 3) cv::rotate(*myImage, *myImage, cv::ROTATE_180);

    // 在 UI 上显示最新的这一帧
    slot_displayAndDetect(myImage);

    const bool needsTemplateDrawing = (ui->comboBox_4->currentIndex() == 0
                                       || ui->comboBox_4->currentIndex() == 1);
    imageLabel->setTemplateDrawingEnabled(needsTemplateDrawing);
    if (needsTemplateDrawing) {
        imageLabel->resetDrawingStep();
        showTemplateGuideForCurrentMode();
    } else {
        hideTemplateGuide();
    }
}
/**
 * @brief 连续拍照按钮点击槽函数
 * @details 启动工作线程，进入连续采集识别模式
 */
//void Widget::on_ReShoot_clicked()
//{
//    qDebug() << "=== on_ReShoot_clicked() called ===";

//    if (!m_bOpenDevice) {
//        QMessageBox::warning(this, "警告", "采集失败,请打开设备！");
//        return;
//    }

//    int exposureValue = ui->spinBox->value();
//    m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);
//    if((ui->comboBox_4->currentIndex() == 0)||(ui->comboBox_4->currentIndex() == 1))
//    {
//        if (digitTemplates.empty()) {
//            QMessageBox::warning(this, "警告", "模板图像为空！ 请确认目标字符");
//            return;
//        }
//    }



//    // ✅ 核心修复：确保线程已正确初始化
//    ensureThreadsReady();

//    // 如果 myThread 还是 null，重新创建
//    if (!myThread) {
//        reinitializeMyThread();
//    }

//    // 设置参数
//    int number = ui->lineEdit_yuzhi->text().toDouble();
//    emit ssim(number);

//    int index = ui->comboBox_2->currentIndex();
//    switch (index) {
//    case 1: angleValue = 1; break;
//    case 2: angleValue = 2; break;
//    case 3: angleValue = 3; break;
//    default: angleValue = 0;
//    }
//    emit rotate(angleValue);

//    QString text = ui->lineEdit_4->text();
//    emit sendDataTo(text);

//    // 设置为软触发模式
//    m_pcMyCamera->SetEnumValue("TriggerSource", 7);

//    // 传递相机和图像指针给线程
//    myThread->getCameraPtr(m_pcMyCamera);
//    myThread->getImagePtr(myImage);

//    // 启动线程
//    if (!myThread->isRunning()) {
//        myThread->start();
//        ui->statusLabel->setText("软触发模式运行中...");
//    }
//    ui->plcbtn->setEnabled(false);  // 禁用按钮，防止重复点击
//    ui->VideoShoot->setEnabled(false);
//    ui->ReShoot->setEnabled(false);
//    qDebug() << "=== on_ReShoot_clicked() completed ===";
//}

/**
 * @brief 曝光值变化槽函数
 * @param value 新的曝光值
 */
void Widget::onSpinBoxValueChanged(int value)
{
    exposureValue = value;
}

/**
 * @brief 显示主窗口
 */
void Widget::showscreen()
{
    // 读取配置文件
    QSettings *configIniRead = new QSettings("D:\\SystemInifiles\\ConfigName.ini", QSettings::IniFormat);
    configIniRead->setIniCodec("GBK");
    delete configIniRead;

    setWindowIcon(QIcon(":/2.png"));
    setWindowTitle(tr("识别系统"));
    this->show();
}

void Widget::addConfirmAllParamError(const QString &message)
{
    const QString normalizedMessage = message.trimmed();
    if (normalizedMessage.isEmpty() || m_confirmAllParamErrors.contains(normalizedMessage)) {
        return;
    }

    m_confirmAllParamErrors.append(normalizedMessage);
}

void Widget::showParameterInfo(const QString &title, const QString &message)
{
    if (m_confirmAllParamsRunning) {
        return;
    }

    QMessageBox::information(this, title, message);
}

void Widget::showParameterInfoAsError(const QString &title, const QString &message)
{
    if (m_confirmAllParamsRunning) {
        addConfirmAllParamError(message);
        return;
    }

    QMessageBox::information(this, title, message);
}

void Widget::showParameterWarning(const QString &title, const QString &message)
{
    if (m_confirmAllParamsRunning) {
        addConfirmAllParamError(message);
        return;
    }

    QMessageBox::warning(this, title, message);
}

void Widget::showParameterCritical(const QString &title, const QString &message)
{
    if (m_confirmAllParamsRunning) {
        addConfirmAllParamError(message);
        return;
    }

    QMessageBox::critical(this, title, message);
}

void Widget::updateCurrentTemplateName()
{
    if (!m_currentTemplateNameVisible || currentTemplateDirPath.isEmpty()) {
        ui->currentTemplateName->setText("--");
        return;
    }

    QDir templateDir(currentTemplateDirPath);
    if (!templateDir.exists()) {
        ui->currentTemplateName->setText("--");
        return;
    }

    const QString templateName = templateDir.dirName();
    if (!templateName.isEmpty()) {
        ui->currentTemplateName->setText(templateName);
    } else {
        ui->currentTemplateName->setText("--");
    }
}

void Widget::updateSaveDirButtonText()
{
    if (!ui || !ui->lineEdit_imageSavePath || !ui->pushButton_browseImageSavePath) {
        return;
    }

    const QString saveDir = selectedDir.trimmed();
    ui->pushButton_browseImageSavePath->setText("浏览");
    ui->pushButton_browseImageSavePath->setToolTip("点击选择图像保存路径");

    if (saveDir.isEmpty()) {
        ui->lineEdit_imageSavePath->clear();
        ui->lineEdit_imageSavePath->setToolTip("");
        return;
    }

    ui->lineEdit_imageSavePath->setText(saveDir);
    ui->lineEdit_imageSavePath->setToolTip(saveDir);
}

void Widget::updateTissueRoughnessUiVisibility()
{
    if (!ui) {
        return;
    }

    const bool showTissueThreshold = (ui->comboBox_4->currentIndex() == 3);
    ui->label_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->lineEdit_tissueRoughnessThreshold->setVisible(showTissueThreshold);
    ui->pushButton_tissueRoughnessThreshold->setVisible(showTissueThreshold);
}

void Widget::setupTemplateGuide()
{
    if (!ui || m_templateGuideFrame) {
        return;
    }

    m_templateGuideFrame = new QFrame(ui->imagedisplayBox);
    m_templateGuideFrame->setObjectName("templateGuideFrame");
    m_templateGuideFrame->setFrameShape(QFrame::NoFrame);
    m_templateGuideFrame->setStyleSheet(
                "#templateGuideFrame {"
                "background-color: transparent;"
                "border: none;"
                "}");
    m_templateGuideFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout *guideLayout = new QVBoxLayout(m_templateGuideFrame);
    guideLayout->setContentsMargins(12, 0, 12, 4);
    guideLayout->setSpacing(0);

    m_templateGuideTitleLabel = new QLabel(m_templateGuideFrame);
    m_templateGuideTitleLabel->setStyleSheet("color: #1677d2; font-size: 18px; font-weight: bold; border: none; background: transparent;");
    m_templateGuideTitleLabel->setWordWrap(true);
    m_templateGuideTitleLabel->hide();

    m_templateGuideBodyLabel = new QLabel(m_templateGuideFrame);
    m_templateGuideBodyLabel->setStyleSheet("color: #000000; font-size: 18px; font-weight: normal; border: none; background: transparent;");
    m_templateGuideBodyLabel->setWordWrap(false);

    guideLayout->addWidget(m_templateGuideBodyLabel);

    ui->verticalLayout_InnerImg->insertWidget(0, m_templateGuideFrame);
    hideTemplateGuide();
}

void Widget::updateTemplateGuideText(const QString &title, const QString &body)
{
    if (!m_templateGuideFrame || !m_templateGuideTitleLabel || !m_templateGuideBodyLabel) {
        return;
    }

    Q_UNUSED(title);
    m_templateGuideTitleLabel->clear();
    m_templateGuideTitleLabel->hide();
    QString guideText = body.trimmed();
    if (!guideText.startsWith("【操作提示】")) {
        guideText.prepend("【操作提示】");
    }
    if (!guideText.contains("【按下esc退出当前模板制作】")) {
        guideText.append("  【按下esc退出当前模板制作】");
    }
    m_templateGuideBodyLabel->setText(guideText);
    m_templateGuideFrame->setVisible(true);
}

void Widget::hideTemplateGuide()
{
    if (m_templateGuideFrame) {
        m_templateGuideFrame->hide();
    }
}

void Widget::updateImageDisplayStatusText(const QString &body)
{
    if (!m_templateGuideFrame || !m_templateGuideTitleLabel || !m_templateGuideBodyLabel) {
        return;
    }

    m_templateGuideTitleLabel->clear();
    m_templateGuideTitleLabel->hide();
    m_templateGuideBodyLabel->setText(body.trimmed());
    m_templateGuideFrame->setVisible(true);
}

void Widget::showTemplateGuideForCurrentMode()
{
    const int modeIndex = ui->comboBox_4->currentIndex();

    if (modeIndex == 0) {
        updateTemplateGuideText("模板匹配模板制作",
                                "请按住鼠标左键拖动，框选定位区域。");
        return;
    }

    if (modeIndex == 1) {
        updateTemplateGuideText("字库匹配模板制作",
                                "请按住鼠标左键拖动，框选定位区域。");
        return;
    }

    hideTemplateGuide();
}

void Widget::handleTemplateGuideEvent(const QString &eventName, int pointCount)
{
    if (!m_templateGuideFrame || !m_templateGuideFrame->isVisible()) {
        return;
    }

    const int modeIndex = ui->comboBox_4->currentIndex();
    if (modeIndex != 0 && modeIndex != 1) {
        hideTemplateGuide();
        return;
    }

    const QString title = (modeIndex == 1) ? "字库匹配模板制作" : "模板匹配模板制作";

    if (eventName == "tracking_started") {
        updateTemplateGuideText(title,
                                "松开鼠标左键完成定位区域。");
    } else if (eventName == "template_reset") {
        updateTemplateGuideText(title,
                                "已清空当前框线，请重新按住鼠标左键拖动，框选定位区域。");
    } else if (eventName == "tracking_too_small") {
        updateTemplateGuideText(title,
                                "定位区域太小，请重新框选更大的定位区域。");
    } else if (eventName == "tracking_done") {
        updateTemplateGuideText(title,
                                "请用鼠标左键依次点击喷码区域边缘，右键闭合。");
    } else if (eventName == "poly_point_added") {
        updateTemplateGuideText(title,
                                QString("已选择 %1 个点，继续点击边缘或右键闭合。").arg(pointCount));
    } else if (eventName == "poly_too_few") {
        updateTemplateGuideText(title,
                                QString("至少需要 3 个点，当前 %1 个，请继续点击喷码区域边缘。").arg(pointCount));
    } else if (eventName == "poly_done") {
        updateTemplateGuideText(title,
                                "喷码检测区域已完成，请点击【保存模板】。");
        QTimer::singleShot(0, this, [this]() {
            if (!imageLabel || !imageLabel->isTemplateDrawingEnabled()) {
                return;
            }

            QMessageBox saveMessageBox(this);
            saveMessageBox.setIcon(QMessageBox::Question);
            saveMessageBox.setWindowTitle("保存模板");
            saveMessageBox.setText("喷码检测区域已闭合。\n\n是否立即保存当前产品模板？");
            QPushButton *saveButton = saveMessageBox.addButton("保存", QMessageBox::AcceptRole);
            saveMessageBox.addButton("取消", QMessageBox::RejectRole);
            saveMessageBox.setDefaultButton(saveButton);
            saveMessageBox.exec();

            if (saveMessageBox.clickedButton() == saveButton) {
                on_pushButton_5_clicked();
            }
        });
    }
}

void Widget::setupCharacterSplitSettingsDialog()
{
    if (!ui || m_characterSplitSettingsDialog || !ui->tab2_frame3 || !ui->tab2_frame2) {
        return;
    }

    QWidget *settingsParent = ui->tab2_frame3->parentWidget();
    QGridLayout *settingsLayout = qobject_cast<QGridLayout *>(settingsParent ? settingsParent->layout() : nullptr);
    QWidget *splitButtonParent = ui->Saveimage ? ui->Saveimage->parentWidget() : nullptr;
    QFormLayout *splitButtonForm = qobject_cast<QFormLayout *>(splitButtonParent ? splitButtonParent->layout() : nullptr);
    if (!settingsParent || !settingsLayout || !splitButtonParent || !splitButtonForm || !ui->Saveimage) {
        return;
    }

    int saveImageRow = -1;
    QFormLayout::ItemRole saveImageRole = QFormLayout::FieldRole;
    splitButtonForm->getWidgetPosition(ui->Saveimage, &saveImageRow, &saveImageRole);
    if (saveImageRow < 0) {
        return;
    }

    settingsLayout->removeWidget(ui->tab2_frame3);
    settingsLayout->removeWidget(ui->tab2_frame2);

    QWidget *splitButtonRow = new QWidget(splitButtonParent);
    QHBoxLayout *splitButtonLayout = new QHBoxLayout(splitButtonRow);
    splitButtonLayout->setContentsMargins(0, 0, 0, 0);
    splitButtonLayout->setSpacing(8);

    const QString splitToolButtonStyle =
            "QToolButton {"
            "background-color: transparent;"
            "border: 1px solid #ebeef5;"
            "border-radius: 4px;"
            "color: #333333;"
            "padding: 5px 10px;"
            "}"
            "QToolButton:hover {"
            "background-color: #f2f6fc;"
            "}"
            "QToolButton:pressed {"
            "background-color: #ebeef5;"
            "}";
    const QString splitPushButtonStyle =
            "QPushButton {"
            "background-color: transparent;"
            "border: 1px solid #ebeef5;"
            "border-radius: 4px;"
            "color: #333333;"
            "padding: 5px 10px;"
            "}"
            "QPushButton:hover {"
            "background-color: #f2f6fc;"
            "}"
            "QPushButton:pressed {"
            "background-color: #ebeef5;"
            "}";

    splitButtonForm->removeWidget(ui->Saveimage);
    ui->Saveimage->setParent(splitButtonRow);
    ui->Saveimage->setStyleSheet(splitToolButtonStyle);
    ui->Saveimage->setToolTip("自动把当前框选的喷码检测区域分割成单个字符模板图片，用于后续字库匹配。");
    ui->Saveimage->installEventFilter(this);
    ui->Saveimage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    splitButtonLayout->addWidget(ui->Saveimage);

    m_characterSplitSettingsButton = new QPushButton("字符自动分割设置", splitButtonRow);
    m_characterSplitSettingsButton->setStyleSheet(splitPushButtonStyle);
    m_characterSplitSettingsButton->setMinimumHeight(42);
    m_characterSplitSettingsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    splitButtonLayout->addWidget(m_characterSplitSettingsButton);

    m_manualCharacterCropButton = new QPushButton("分割字符模板", splitButtonRow);
    m_manualCharacterCropButton->setStyleSheet(splitPushButtonStyle);
    m_manualCharacterCropButton->setMinimumHeight(42);
    m_manualCharacterCropButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_manualCharacterCropButton->setToolTip("打开当前产品模板的喷码区域图，手动框选字符并批量保存字符模板图片。");
    m_manualCharacterCropButton->installEventFilter(this);
    splitButtonLayout->addWidget(m_manualCharacterCropButton);

    splitButtonForm->setWidget(saveImageRow, saveImageRole, splitButtonRow);

    m_characterSplitSettingsDialog = new QDialog(this);
    m_characterSplitSettingsDialog->setWindowTitle("字符自动分割设置");
    m_characterSplitSettingsDialog->setModal(true);
    m_characterSplitSettingsDialog->setMinimumWidth(760);
    const QString splitHelpText =
            "这里用于调整【自动分割】字符模板图片时使用的参数。\n\n"
            "字符尺寸限制：用于过滤过小或过大的字符区域。\n"
            "高级形态学参数：用于调整字符粘连、断裂、背景噪声时的分割效果。\n\n"
            "一般情况下保持默认值即可；只有自动分割出来的字符不完整、粘连或多出杂点时再调整。";
    m_characterSplitSettingsDialog->setProperty("characterSplitHelpText", splitHelpText);
    m_characterSplitSettingsDialog->setWhatsThis(splitHelpText);
    m_characterSplitSettingsDialog->installEventFilter(this);

    QVBoxLayout *dialogLayout = new QVBoxLayout(m_characterSplitSettingsDialog);
    dialogLayout->setContentsMargins(12, 12, 12, 12);
    dialogLayout->setSpacing(8);

    dialogLayout->addWidget(ui->tab2_frame3);
    dialogLayout->addWidget(ui->tab2_frame2);
    ui->tab2_frame3->setWhatsThis(splitHelpText);
    ui->tab2_frame2->setWhatsThis(splitHelpText);
    ui->tab2_frame3->show();
    ui->tab2_frame2->show();

    connect(m_characterSplitSettingsButton, &QPushButton::clicked,
            this, &Widget::showCharacterSplitSettingsDialog);
    connect(m_manualCharacterCropButton, &QPushButton::clicked,
            this, &Widget::showManualCharacterTemplateCropDialog);
}

void Widget::showCharacterSplitSettingsDialog()
{
    if (!m_characterSplitSettingsDialog) {
        return;
    }

    m_characterSplitSettingsDialog->exec();
}

void Widget::showManualCharacterTemplateCropDialog()
{
    if (!ui || ui->comboBox_4->currentIndex() != 1) {
        showParameterInfoAsError("提示", "手动切割字符模板只用于字库匹配模式。");
        return;
    }

    QString templateDirPath;
    int profileIndex = -1;
    if (m_wordMultiTemplateMode) {
        profileIndex = currentWordTemplateProfileIndex();
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
            showParameterInfoAsError("提示", "请先选择当前编辑的产品模板。");
            return;
        }
        templateDirPath = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)].dirPath;
    } else {
        templateDirPath = currentTemplateDirPath;
    }

    if (templateDirPath.trimmed().isEmpty() || !QDir(templateDirPath).exists()) {
        showParameterInfoAsError("提示", "请先选择产品模板文件夹。");
        return;
    }

    const QString rawImagePath = QDir(templateDirPath).filePath("template_raw.png");
    QImage rawImage(rawImagePath);
    if (rawImage.isNull()) {
        showParameterCritical("严重警告", "当前产品模板缺少 template_raw.png，无法手动切割字符模板。");
        return;
    }

    const QString settingsFilePath = QDir(templateDirPath).filePath("app_settings.appset");
    QSettings templateSettings(settingsFilePath, QSettings::IniFormat);
    cv::Rect2d trackingBox(
                templateSettings.value("trackingBox_x", 0).toDouble(),
                templateSettings.value("trackingBox_y", 0).toDouble(),
                templateSettings.value("trackingBox_width", 0).toDouble(),
                templateSettings.value("trackingBox_height", 0).toDouble());
    bool trackingBoxValid = trackingBox.width > 0 && trackingBox.height > 0;
    if (!trackingBoxValid
            && QDir(templateDirPath).absolutePath() == QDir(currentTemplateDirPath).absolutePath()
            && hasValidBoxes
            && savedTrackingBox.width > 0
            && savedTrackingBox.height > 0) {
        trackingBox = savedTrackingBox;
        trackingBoxValid = true;
    }
    if (!trackingBoxValid) {
        showParameterCritical("严重警告", "当前产品模板缺少有效定位区域，无法还原喷码检测区域。");
        return;
    }

    const QString yamlPath = QDir(templateDirPath).filePath("calibrate_config.yaml");
    CalibrationData calib;
    if (!QFile::exists(yamlPath)
            || !calib.load(yamlPath.toLocal8Bit().toStdString())
            || calib.date_poly.empty()) {
        showParameterCritical("严重警告", "当前产品模板缺少有效喷码检测区域，无法手动切割字符模板。");
        return;
    }

    QPolygonF datePolygon;
    const QPointF trackingCenter(trackingBox.x + trackingBox.width / 2.0,
                                 trackingBox.y + trackingBox.height / 2.0);
    for (const cv::Point2f &point : calib.date_poly) {
        datePolygon << QPointF(trackingCenter.x() + point.x,
                               trackingCenter.y() + point.y);
    }

    QRect cropRect = datePolygon.boundingRect().toAlignedRect()
            .intersected(QRect(0, 0, rawImage.width(), rawImage.height()));
    if (cropRect.width() <= 0 || cropRect.height() <= 0) {
        showParameterCritical("严重警告", "喷码检测区域超出模板图像范围，无法手动切割字符模板。");
        return;
    }

    CharacterTemplateCropDialog dialog(rawImage.copy(cropRect), templateDirPath, this);
    if (dialog.exec() != QDialog::Accepted || dialog.savedCount() <= 0) {
        return;
    }

    const QString targetText = ui->dateEdit->toPlainText();
    QString reloadMessage;
    if (!targetText.trimmed().isEmpty()) {
        const QStringList baseNames = parseWordTemplateBaseNames(targetText);
        std::vector<cv::Mat> reloadedTemplates;
        std::vector<int> reloadedTemplateTargetIndexes;
        QString loadError;
        if (loadWordDigitTemplatesFromDir(templateDirPath,
                                          baseNames,
                                          &reloadedTemplates,
                                          &reloadedTemplateTargetIndexes,
                                          &loadError,
                                          true)) {
            if (m_wordMultiTemplateMode && profileIndex >= 0
                    && profileIndex < static_cast<int>(m_wordTemplateProfiles.size())) {
                WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
                profile.digitTemplates = reloadedTemplates;
                profile.digitTemplateTargetIndexes = reloadedTemplateTargetIndexes;
            } else {
                digitTemplates = reloadedTemplates;
                digitTemplateTargetIndexes = reloadedTemplateTargetIndexes;
            }
        } else {
            reloadMessage = QString("\n\n字符模板已保存，但当前目标字符仍有图片未加载成功：\n%1").arg(loadError);
        }
    }

    showParameterInfo("提示",
                      QString("已保存 %1 张字符模板图片。%2")
                      .arg(dialog.savedCount())
                      .arg(reloadMessage));
}

void Widget::setupWordTemplateEditorCombo()
{
    if (ui) {
        const QString commonPushButtonStyle =
                "QPushButton {"
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #333333;"
                "padding: 5px 10px;"
                "}"
                "QPushButton:hover {"
                "background-color: #f2f6fc;"
                "}"
                "QPushButton:pressed {"
                "background-color: #ebeef5;"
                "}";

        if (ui->textsure_btn) ui->textsure_btn->setStyleSheet(commonPushButtonStyle);
        if (ui->batchTextsure_btn) ui->batchTextsure_btn->setStyleSheet(commonPushButtonStyle);
        if (ui->WriteVDpushButton) ui->WriteVDpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_7) ui->pushButton_7->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_3) ui->pushButton_3->setStyleSheet(commonPushButtonStyle);
        if (ui->sureButton) ui->sureButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_9) ui->pushButton_9->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_12) ui->pushButton_12->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_tissueRoughnessThreshold) ui->pushButton_tissueRoughnessThreshold->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_2) ui->pushButton_2->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton) ui->pushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->plcmodebtn) ui->plcmodebtn->setStyleSheet(commonPushButtonStyle);
        if (ui->ConnectpushButton) ui->ConnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->DisconnectpushButton) ui->DisconnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_8) ui->pushButton_8->setStyleSheet(commonPushButtonStyle);
        if (ui->pushButton_browseImageSavePath) ui->pushButton_browseImageSavePath->setStyleSheet(commonPushButtonStyle);
    }

    if (ui && ui->textsure_btn && ui->batchTextsure_btn) {
        ui->textsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        ui->batchTextsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        if (ui->textsure_btn->parentWidget()) {
            ui->textsure_btn->parentWidget()->setStyleSheet(
                        "#textConfirmButtonContainer { border: none; background: transparent; padding: 0px; }");
        }

        QHBoxLayout *buttonLayout = qobject_cast<QHBoxLayout *>(ui->textsure_btn->parentWidget()
                ? ui->textsure_btn->parentWidget()->layout()
                : nullptr);
        if (buttonLayout) {
            buttonLayout->setStretch(0, 1);
            buttonLayout->setStretch(1, 1);
        }

        ui->textsure_btn->setToolTip("只保存当前编辑模板的目标字符，并重新加载该模板的字符图片。");
        ui->batchTextsure_btn->setToolTip("把当前目标字符保存到所有已选择的字库模板，并分别重新加载字符图片。");
        ui->textsure_btn->installEventFilter(this);
        ui->batchTextsure_btn->installEventFilter(this);
    }

    if (ui && ui->pushButton_browseImageSavePath) {
        ui->pushButton_browseImageSavePath->installEventFilter(this);
    }

    if (ui && ui->pushButton_11) {
        ui->pushButton_11->setToolTip("保存当前界面上的设置。\n单模板时：写入当前产品模板文件夹，同时保存为软件下次启动的默认设置。\n多模板时：按提示写入已选择的产品模板文件夹，同时保存为软件下次启动的默认设置。");
        ui->pushButton_11->installEventFilter(this);
    }

    if (ui && ui->pushButton_7) {
        ui->pushButton_7->setToolTip("确认当前选择的颜色通道，用于后续图像处理和识别。");
        ui->pushButton_7->installEventFilter(this);
    }

    if (ui) {
        if (ui->pushButton_10) {
            ui->pushButton_10->setToolTip("清空当前尚未发出的剔除队列。\n适用于异常停机、误判、手动停止后，防止之前累计的剔除信号继续输出。");
            ui->pushButton_10->installEventFilter(this);
        }
        if (ui->label_4) {
            ui->label_4->setToolTip("图像判定合格的分数阈值。\n识别匹配分数低于该值时，通常判为不合格；数值越高，判定越严格。");
            ui->label_4->installEventFilter(this);
        }
        if (ui->label_27) {
            ui->label_27->setToolTip("设置图像进入识别前的旋转方向。\n当相机安装方向、产品摆放方向和模板方向不一致时，需要调整这里。");
            ui->label_27->installEventFilter(this);
        }
        if (ui->label_16) {
            ui->label_16->setToolTip("设置相机增益。\n增益越高画面越亮，但噪声也可能增加；一般先调曝光，曝光不足时再调增益。");
            ui->label_16->installEventFilter(this);
        }
        if (ui->label_14) {
            ui->label_14->setToolTip("PLC拍照信号保持多久。\n相机偶尔漏拍、触发不稳定时可适当加大；正常不要过大，避免影响下一次触发节拍。");
            ui->label_14->installEventFilter(this);
        }
        if (ui->label_13) {
            ui->label_13->setToolTip("相机收到 PLC 拍照信号后，再等待多久才真正曝光采图。\n通常在拍照距离基本正确后，用它做小范围微调。\n画面中产品还没到合适位置就加大；产品已经走过或喷码偏后就减小。");
            ui->label_13->installEventFilter(this);
        }
        if (ui->label_6) {
            ui->label_6->setToolTip("检测拍照点到剔除机构中心的实际产线距离。\n剔除太早通常加大；剔除太晚通常减小。");
            ui->label_6->installEventFilter(this);
        }
        if (ui->label_10) {
            ui->label_10->setToolTip("剔除机构保持动作的时长。\n不合格品剔不干净就加大；影响相邻合格品或动作拖尾就减小。");
            ui->label_10->installEventFilter(this);
        }
        if (ui->label_17) {
            ui->label_17->setToolTip("选择第几路剔除输出或第几个剔除口。\n现场有多个气嘴、推杆或剔除工位时使用；填错会从错误位置剔除。");
            ui->label_17->installEventFilter(this);
        }
        if (ui->label_8) {
            ui->label_8->setToolTip("上游传感器触发点到相机拍照中心的实际产线距离。\nPLC 根据这个距离判断产品走到相机位置后再发出拍照信号。\n画面中产品还没到拍照位置，说明触发偏早，适当加大；产品已经走过拍照位置，说明触发偏晚，适当减小。");
            ui->label_8->installEventFilter(this);
        }
        if (ui->comboBox_3) {
            ui->comboBox_3->setToolTip("PLC触发工作模式。\n连续触发模式：产线连续经过时，PLC按连续节拍触发相机采图和检测。\n间歇触发模式：产品分批、停顿或按间隔到位时，PLC按间歇方式触发采图和检测。");
            ui->comboBox_3->installEventFilter(this);
        }
    }

    if (ui && ui->confirmAllParamsButton) {
        ui->confirmAllParamsButton->setToolTip("依次确认当前界面上的所有参数；目标字符只按当前确认字符逻辑处理，不会批量覆盖所有模板字符。");
        ui->confirmAllParamsButton->installEventFilter(this);
    }

    if (ui && ui->VideoShoot) {
        ui->VideoShoot->installEventFilter(this);
    }

    if (ui && ui->batchTextsure_btn) {
        ui->batchTextsure_btn->hide();
    }

    if (m_wordTemplateEditComboBox || !ui || !ui->dateEdit || !ui->lineEdit_yuzhi) {
        return;
    }

    auto shiftGridRowsDown = [](QGridLayout *gridLayout, int firstRow) {
        struct MovedWidget {
            QWidget *widget = nullptr;
            int row = 0;
            int column = 0;
            int rowSpan = 1;
            int columnSpan = 1;
            Qt::Alignment alignment;
        };

        std::vector<MovedWidget> movedWidgets;
        for (int i = gridLayout->count() - 1; i >= 0; --i) {
            QLayoutItem *item = gridLayout->itemAt(i);
            if (!item || !item->widget()) {
                continue;
            }

            int row = 0;
            int column = 0;
            int rowSpan = 1;
            int columnSpan = 1;
            gridLayout->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
            if (row < firstRow) {
                continue;
            }

            MovedWidget moved;
            moved.widget = item->widget();
            moved.row = row;
            moved.column = column;
            moved.rowSpan = rowSpan;
            moved.columnSpan = columnSpan;
            moved.alignment = item->alignment();
            movedWidgets.push_back(moved);
            gridLayout->removeWidget(moved.widget);
        }

        for (auto it = movedWidgets.rbegin(); it != movedWidgets.rend(); ++it) {
            gridLayout->addWidget(it->widget,
                                  it->row + 1,
                                  it->column,
                                  it->rowSpan,
                                  it->columnSpan,
                                  it->alignment);
        }
    };

    QWidget *parentWidget = ui->dateEdit->parentWidget();
    if (parentWidget) {
        QGridLayout *targetLayout = qobject_cast<QGridLayout *>(parentWidget->layout());
        if (targetLayout) {
            shiftGridRowsDown(targetLayout, 2);
        }

        m_wordTemplateEditWidget = new QWidget(parentWidget);
        m_wordTemplateEditWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditWidget->setFixedHeight(50);
        QHBoxLayout *editorLayout = new QHBoxLayout(m_wordTemplateEditWidget);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(6);

        m_wordTemplateEditLabel = new QLabel("当前编辑模板:", m_wordTemplateEditWidget);
        m_wordTemplateEditLabel->setFixedHeight(50);
        m_wordTemplateEditLabel->setStyleSheet(
                    "background-color: #ffffff;"
                    "border: 1px solid #ebeef5;"
                    "border-radius: 4px;"
                    "color: #333333;"
                    "padding: 5px 10px;");
        m_wordTemplateEditComboBox = new QComboBox(m_wordTemplateEditWidget);
        m_wordTemplateEditComboBox->setObjectName("wordTemplateComboBox");
        m_wordTemplateEditComboBox->setMinimumHeight(50);
        m_wordTemplateEditComboBox->setMaximumHeight(50);
        m_wordTemplateEditComboBox->setMinimumWidth(160);
        m_wordTemplateEditComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditComboBox->setStyleSheet(
                    "QComboBox {"
                    "background-color: #ffffff;"
                    "border: 1px solid #ebeef5;"
                    "border-radius: 4px;"
                    "color: #333333;"
                    "padding: 5px 10px;"
                    "}");

        editorLayout->addWidget(m_wordTemplateEditLabel);
        editorLayout->addWidget(m_wordTemplateEditComboBox, 1);
        if (targetLayout) {
            targetLayout->addWidget(m_wordTemplateEditWidget, 2, 1, 1, 2);
        }

        connect(m_wordTemplateEditComboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int index) {
                    applyWordTemplateEditorSelection(index);
                });

        m_wordTemplateEditWidget->hide();
    }

    QWidget *thresholdParentWidget = ui->lineEdit_yuzhi->parentWidget();
    if (thresholdParentWidget) {
        QGridLayout *thresholdLayout = qobject_cast<QGridLayout *>(thresholdParentWidget->layout());
        int thresholdInsertRow = 2;
        int thresholdInsertColumn = 1;
        if (thresholdLayout) {
            for (int i = 0; i < thresholdLayout->count(); ++i) {
                QLayoutItem *item = thresholdLayout->itemAt(i);
                if (!item || item->widget() != ui->lineEdit_yuzhi) {
                    continue;
                }

                int row = 0;
                int column = 0;
                int rowSpan = 1;
                int columnSpan = 1;
                thresholdLayout->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
                thresholdInsertRow = row + rowSpan;
                thresholdInsertColumn = column;
                break;
            }
            shiftGridRowsDown(thresholdLayout, thresholdInsertRow);
        }

        m_wordThresholdEditWidget = new QWidget(thresholdParentWidget);
        m_wordThresholdEditWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordThresholdEditWidget->setFixedHeight(50);
        QHBoxLayout *thresholdLayoutBox = new QHBoxLayout(m_wordThresholdEditWidget);
        thresholdLayoutBox->setContentsMargins(0, 0, 0, 0);
        thresholdLayoutBox->setSpacing(6);

        m_wordThresholdEditLabel = new QLabel("当前阈值模板:", m_wordThresholdEditWidget);
        m_wordThresholdEditLabel->setFixedHeight(50);
        m_wordThresholdEditComboBox = new QComboBox(m_wordThresholdEditWidget);
        m_wordThresholdEditComboBox->setObjectName("wordThresholdTemplateComboBox");
        m_wordThresholdEditComboBox->setMinimumHeight(50);
        m_wordThresholdEditComboBox->setMaximumHeight(50);
        m_wordThresholdEditComboBox->setMinimumWidth(160);
        m_wordThresholdEditComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        thresholdLayoutBox->addWidget(m_wordThresholdEditLabel);
        thresholdLayoutBox->addWidget(m_wordThresholdEditComboBox, 1);
        if (thresholdLayout) {
            thresholdLayout->addWidget(m_wordThresholdEditWidget, thresholdInsertRow, thresholdInsertColumn);
        }

        connect(m_wordThresholdEditComboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int index) {
                    if (!m_wordThresholdEditComboBox || index < 0 || !m_wordMultiTemplateMode) {
                        return;
                    }
                    bool ok = false;
                    const int profileIndex = m_wordThresholdEditComboBox->itemData(index).toInt(&ok);
                    if (ok) {
                        setCurrentWordTemplateEditIndex(profileIndex);
                    }
                });

        m_wordThresholdEditWidget->hide();
    }

    connect(ui->comboBox_4,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                updateTissueRoughnessUiVisibility();
                if (imageLabel) {
                    imageLabel->setTemplateDrawingEnabled(false);
                }
                hideTemplateGuide();
                if (index != 1 && m_wordMultiTemplateMode) {
                    clearWordMultiTemplateState();
                    return;
                }
                refreshWordTemplateEditorCombo();
            });
    updateTissueRoughnessUiVisibility();
}

void Widget::clearWordMultiTemplateState()
{
    m_wordTemplateDirPaths.clear();
    m_wordTemplateProfiles.clear();
    m_wordMultiTemplateMode = false;
    m_currentWordTemplateEditIndex = -1;
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
}

void Widget::refreshWordTemplateEditorCombo()
{
    const bool shouldShow = m_wordMultiTemplateMode
            && ui->comboBox_4->currentIndex() == 1
            && !m_wordTemplateProfiles.empty();

    if (ui->batchTextsure_btn) {
        ui->batchTextsure_btn->setVisible(shouldShow);
    }

    if (!m_wordTemplateEditComboBox || !m_wordTemplateEditWidget) {
        return;
    }

    auto fillCombo = [this, shouldShow](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        QSignalBlocker blocker(comboBox);
        comboBox->clear();

        if (shouldShow) {
            for (int i = 0; i < static_cast<int>(m_wordTemplateProfiles.size()); ++i) {
                const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(i)];
                const QString displayName = profile.name.isEmpty()
                        ? QString("模板%1").arg(i + 1)
                        : profile.name;
                comboBox->addItem(displayName, i);
            }
        }
    };

    fillCombo(m_wordTemplateEditComboBox);
    fillCombo(m_wordThresholdEditComboBox);

    m_wordTemplateEditWidget->setVisible(shouldShow);
    if (m_wordThresholdEditWidget) {
        m_wordThresholdEditWidget->setVisible(shouldShow);
    }

    if (shouldShow) {
        int profileIndex = m_currentWordTemplateEditIndex;
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
            profileIndex = 0;
        }
        setCurrentWordTemplateEditIndex(profileIndex);
    } else {
        m_currentWordTemplateEditIndex = -1;
    }
}

void Widget::applyWordTemplateEditorSelection(int comboIndex)
{
    if (!m_wordTemplateEditComboBox || comboIndex < 0 || !m_wordMultiTemplateMode) {
        return;
    }

    bool ok = false;
    const int profileIndex = m_wordTemplateEditComboBox->itemData(comboIndex).toInt(&ok);
    if (!ok || profileIndex < 0 || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return;
    }

    setCurrentWordTemplateEditIndex(profileIndex);
}

void Widget::setCurrentWordTemplateEditIndex(int profileIndex)
{
    if (!m_wordMultiTemplateMode
            || profileIndex < 0
            || profileIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return;
    }

    m_currentWordTemplateEditIndex = profileIndex;

    auto syncCombo = [profileIndex](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        int comboIndex = -1;
        for (int i = 0; i < comboBox->count(); ++i) {
            if (comboBox->itemData(i).toInt() == profileIndex) {
                comboIndex = i;
                break;
            }
        }

        if (comboIndex >= 0) {
            QSignalBlocker blocker(comboBox);
            comboBox->setCurrentIndex(comboIndex);
        }
    };

    syncCombo(m_wordTemplateEditComboBox);
    syncCombo(m_wordThresholdEditComboBox);

    const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
    {
        QSignalBlocker blocker(ui->dateEdit);
        ui->dateEdit->setPlainText(profile.targetText);
    }
    {
        QSignalBlocker blocker(ui->lineEdit_yuzhi);
        ui->lineEdit_yuzhi->setText(profile.imageThresholdText);
    }

    qDebug() << "[WORD_MULTI_TEMPLATE] editing profile:"
             << profileIndex
             << profile.name
             << profile.dirPath
             << "threshold:" << profile.imageThresholdText;

    displayWordTemplateRawImage(profile.dirPath);
}

int Widget::currentWordTemplateProfileIndex() const
{
    if (!m_wordMultiTemplateMode
            || m_currentWordTemplateEditIndex < 0
            || m_currentWordTemplateEditIndex >= static_cast<int>(m_wordTemplateProfiles.size())) {
        return -1;
    }
    return m_currentWordTemplateEditIndex;
}

void Widget::displayWordTemplateRawImage(const QString &dirPath)
{
    if (!ui || !ui->image_undetected || dirPath.isEmpty()) {
        return;
    }

    const QString rawImagePath = QDir(dirPath).filePath("template_raw.png");
    if (!QFile::exists(rawImagePath)) {
        qDebug() << "[WORD_MULTI_TEMPLATE] template_raw.png not found:" << rawImagePath;
        return;
    }

    QPixmap rawPixmap;
    if (!rawPixmap.load(rawImagePath)) {
        qDebug() << "[WORD_MULTI_TEMPLATE] template_raw.png load failed:" << rawImagePath;
        return;
    }

    QSize labelSize = ui->image_undetected->size();
    if (!labelSize.isValid() || labelSize.isEmpty()) {
        labelSize = rawPixmap.size();
    }

    const QPixmap scaledPixmap = rawPixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->image_undetected->setScaledContents(false);
    ui->image_undetected->setAlignment(Qt::AlignCenter);
    ui->image_undetected->setPixmap(scaledPixmap);

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
    }
    updateImageDisplayStatusText(QString("正在显示模板【%1】的产品图像")
                                 .arg(QDir(dirPath).dirName()));
}

QStringList Widget::parseWordTemplateBaseNames(const QString &targetText) const
{
    QStringList baseNames;
    QRegularExpression regex(R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matchIt = regex.globalMatch(targetText);

    while (matchIt.hasNext()) {
        QRegularExpressionMatch match = matchIt.next();
        QString unit;
        if (!match.captured(1).isEmpty()) unit = match.captured(1);
        else if (!match.captured(2).isEmpty()) unit = match.captured(2);
        else if (!match.captured(3).isEmpty()) unit = match.captured(3);
        else if (!match.captured(4).isEmpty()) unit = match.captured(4);

        if (!unit.isEmpty()) {
            baseNames.append(unit.toLower());
        }
    }

    return baseNames;
}

QStringList Widget::wordTemplateImagePathsForKey(const QDir &directory,
                                                 const QString &searchKey,
                                                 bool includeVariants) const
{
    QStringList exactPaths;
    QStringList variantPaths;
    const QString normalizedKey = searchKey.trimmed().toLower();
    if (normalizedKey.isEmpty() || !directory.exists()) {
        return QStringList();
    }

    static const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tiff"};
    const QFileInfoList fileList = directory.entryInfoList(
                filters,
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &fileInfo : fileList) {
        const QString baseName = fileInfo.completeBaseName().toLower();
        if (baseName == normalizedKey) {
            exactPaths.append(fileInfo.absoluteFilePath());
        } else if (includeVariants && baseName.startsWith(normalizedKey)) {
            const QString suffix = baseName.mid(normalizedKey.length());
            if (suffix.startsWith("_") || suffix.startsWith("-") || suffix.startsWith("(")) {
                variantPaths.append(fileInfo.absoluteFilePath());
            }
        }
    }

    exactPaths.sort(Qt::CaseInsensitive);
    variantPaths.sort(Qt::CaseInsensitive);
    exactPaths.append(variantPaths);
    return exactPaths;
}

bool Widget::loadWordDigitTemplatesFromDir(const QString &dirPath,
                                           const QStringList &baseNames,
                                           std::vector<cv::Mat> *templates,
                                           std::vector<int> *templateTargetIndexes,
                                           QString *errorMessage,
                                           bool includeVariants) const
{
    if (!templates || !templateTargetIndexes) {
        if (errorMessage) {
            *errorMessage = "内部参数无效";
        }
        return false;
    }

    templates->clear();
    templateTargetIndexes->clear();

    QDir directory(dirPath);
    if (!directory.exists()) {
        if (errorMessage) {
            *errorMessage = "产品模板文件夹不存在";
        }
        return false;
    }

    if (baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "目标字符为空或解析失败";
        }
        return false;
    }

    QStringList failedNames;
    for (int targetIndex = 0; targetIndex < baseNames.size(); ++targetIndex) {
        const QString searchKey = baseNames.at(targetIndex).trimmed().toLower();
        const QStringList imagePaths = wordTemplateImagePathsForKey(directory, searchKey, includeVariants);
        if (imagePaths.isEmpty()) {
            failedNames.append(searchKey);
            continue;
        }

        bool allVariantsLoaded = true;
        for (const QString &imagePath : imagePaths) {
            QFile file(imagePath);
            if (!file.open(QIODevice::ReadOnly)) {
                failedNames.append(QFileInfo(imagePath).completeBaseName() + "(无法打开)");
                allVariantsLoaded = false;
                continue;
            }

            const QByteArray data = file.readAll();
            cv::Mat templateImg;
            try {
                std::vector<uchar> buf(data.begin(), data.end());
                templateImg = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
            } catch (...) {
                qDebug() << "[WORD_TEMPLATE] digit imdecode crashed:" << imagePath;
            }

            if (templateImg.empty()) {
                failedNames.append(QFileInfo(imagePath).completeBaseName() + "(读取损坏)");
                allVariantsLoaded = false;
                continue;
            }

            templates->push_back(templateImg);
            templateTargetIndexes->push_back(targetIndex);
        }

        if (!allVariantsLoaded) {
            continue;
        }
    }

    if (!failedNames.isEmpty()) {
        templates->clear();
        templateTargetIndexes->clear();
        if (errorMessage) {
            *errorMessage = QString("以下字符未找到对应图片，或图片读取失败：\n[ %1 ]")
                    .arg(failedNames.join(" "));
        }
        return false;
    }

    return !templates->empty();
}

bool Widget::applyTissueRoughnessThresholdFromUi(bool showMessage)
{
    bool ok = false;
    const double threshold = ui->lineEdit_tissueRoughnessThreshold->text().trimmed().toDouble(&ok);
    if (!ok || threshold <= 0.0) {
        if (showMessage) {
            showParameterWarning("参数错误", "粗糙度阈值必须是大于0的数字");
        }
        return false;
    }

    TissueRollDetector::setDefaultRoughnessThreshold(threshold);
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(threshold, 'f', 3));
    if (showMessage) {
        showParameterInfo("提示", "粗糙度阈值设置成功");
    }
    return true;
}

/**
 * @brief 曝光确定按钮点击槽函数
 * @details 设置相机曝光值
 */
void Widget::on_sureButton_clicked()
{
    if (m_bOpenDevice == false)
    {
        showParameterWarning("警告", "未打开相机，无法设置曝光！");
        return;
    }
    else
    {
        int exposureValue = ui->spinBox->value();
        qDebug() << "SetExposureTime:" <<exposureValue<<m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);
        showParameterInfo("提示", "相机曝光设置成功！");
    }
}

/**
 * @brief 保存模板图像按钮点击槽函数
 * @details 提取当前选择区域，分割字符并保存为模板
 */
void Widget::on_Saveimage_clicked()
{

    if (ui->image_undetected->pixmap() == nullptr)
    {
        QMessageBox::warning(this, "警告", "保存失败,未采集到图像!");
        return;
    }

    // 获取程序运行目录
    QString currentPath = QDir::currentPath();
    qDebug() << "Current Path: " << currentPath;

    // 删除所有.png文件
    QDir dir(currentPath);
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.png" << "*.PNG", QDir::Files);

    foreach (const QFileInfo &fileInfo, files)
    {
        QString filePath = fileInfo.absoluteFilePath();
        qDebug() << "Found file: " << filePath;

        if (QFile::remove(filePath))
        {
            qDebug() << "Successfully removed: " << filePath;
        }
    }

    // 获取图像处理参数
    bool ok1;
    int width_min = ui->lineEdit_5->text().toInt(&ok1);
    int width_max = ui->lineEdit_9->text().toInt(&ok1);
    int height_min = ui->lineEdit_10->text().toInt(&ok1);
    int height_max = ui->lineEdit_11->text().toInt(&ok1);
    int block_size1 = ui->lineEdit_13->text().toInt(&ok1);
    int kernelsize = ui->lineEdit_15->text().toInt(&ok1);
    int horizontalKernel = ui->lineEdit_18->text().toInt(&ok1);
    int verticalKernel = ui->lineEdit_19->text().toInt(&ok1);

    // 统一判断：是否为有效整数 + 均为大于1的奇数
    bool isParamValid = true;
    // 再判断是否都满足「大于1且是奇数」
    if (block_size1 <= 1 || block_size1 % 2 != 1
            || kernelsize <= 1 || kernelsize % 2 != 1
            || horizontalKernel <= 1 || horizontalKernel % 2 != 1
            || verticalKernel <= 1 || verticalKernel % 2 != 1) {
        isParamValid = false;
    }

    // 统一弹窗警告
    if (!isParamValid) {
        showParameterWarning("参数错误", "图像处理参数必须均为大于1的奇数，请修正后重试！");
        return;
    }


    // 发送参数给模板匹配对象
    emit caijianchicun(width_min, width_max, height_min, height_max, block_size1,
                       horizontalKernel, verticalKernel);
    emit kernal(kernelsize);

    // 保存模板图像
    saveImage("bmp", QDir::currentPath() + "/myImage/");

    cv::Mat muban = cv::imread("muban.png");
    templatematch->extractDigits(muban, digitTemplates);

    // 保存分割后的字符图像
    for (size_t i = 0; i < digitTemplates.size(); ++i)
    {
        std::stringstream ss;
        ss << (i + 1) << ".png";
        std::string filename = ss.str();
        cv::imwrite(filename, digitTemplates[i]);
    }
}

/**
 * @brief 获取目标字符串
 * @return QString 目标字符串
 */
QString Widget::setdatetime()
{
    QString datetime = ui->dateEdit->toPlainText();
    return datetime;
}

/**
 * @brief PLC连接按钮点击槽函数
 * @details 连接到西门子PLC
 */
void Widget::on_ConnectpushButton_clicked()
{
    QByteArray ad(ui->lineEdit->text().toUtf8());
    Address = ad.data();

    int tmp = client->ConnectTo(Address, 0, 1);

    if (tmp == 0)
    {
        QMessageBox::information(this, "success", "PLC连接成功");
    }
    else
    {
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
}

/**
 * @brief PLC断开按钮点击槽函数
 */
void Widget::on_DisconnectpushButton_clicked()
{
    int tmp = client->Disconnect();

    if (tmp == 0)
    {
        QMessageBox::information(this, "success", "PLC断开成功");
    }
    else
    {
        QMessageBox::critical(this, "error", "PLC断开失败");
    }
}

///**
// * @brief 写入延时按钮点击槽函数
// * @details 向PLC DB1.920写入DWORD值（延时时间）
// */
//void Widget::on_WriteVDpushButton_2_clicked()
//{
//    if (!client->Connected())
//    {
//        return;
//    }

//    uint32_t value2 = ui->lineEdit_7->text().toUInt();
//    byte delay_data[4] = {0};

//    // 大小端转换
//    delay_data[3] = (unsigned char)(0xFF & value2);
//    delay_data[2] = (unsigned char)((0xFF00 & value2) >> 8);
//    delay_data[1] = (unsigned char)((0xFF0000 & value2) >> 16);
//    delay_data[0] = (unsigned char)((0xFF000000 & value2) >> 24);

//    // 写入DB1.920
//    int tmp2 = client->WriteArea(S7AreaDB, 1, 920, 4, S7WLDWord, delay_data);
//    if (tmp2 != 0)
//    {
//        QMessageBox::warning(this, "error", "设置失败");
//    }
//    else
//    {
//        QMessageBox::information(this, "success", "设置成功");
//    }
//}

///**
// * @brief 写入延时时间按钮点击槽函数
// * @details 向PLC DB1.980写入WORD值（延时时间）
// */
//void Widget::on_WriteVDpushButton_3_clicked()
//{
//    if (!client->Connected())
//    {
//        return;
//    }

//    uint16_t value4 = ui->lineEdit_8->text().toUInt();
//    byte delay_time[2] = {0};

//    // 大小端转换
//    delay_time[1] = (unsigned char)(0xFF & value4);
//    delay_time[0] = (unsigned char)((0xFF00 & value4) >> 8);

//    // 写入DB1.980
//    int tmp4 = client->WriteArea(S7AreaDB, 1, 980, 2, S7WLWord, delay_time);
//    if (tmp4 != 0)
//    {
//        QMessageBox::warning(this, "error", "设置失败");
//    }
//    else
//    {
//        QMessageBox::information(this, "success", "设置成功");
//    }
//}

/**
 * @brief 写入批次时间按钮点击槽函数
 * @details 向PLC DB1.982写入WORD值（批次时间）
 */
void Widget::on_pushButton_8_clicked()
{
if (!client->Connected())
{
    showParameterWarning("警告", "PLC未连接！");
    return;
}

//剔除位置
wrongindex = ui->lineEdit_12->text().toInt();
//    QMessageBox::information(this, "提示", "剔除位置设置成功");


//剔除时间
uint16_t value4 = ui->lineEdit_8->text().toUInt();
byte delay_time[2] = {0};

// 大小端转换
delay_time[1] = (unsigned char)(0xFF & value4);
delay_time[0] = (unsigned char)((0xFF00 & value4) >> 8);

// 写入DB1.980
int tmp4 = client->WriteArea(S7AreaDB, 1, 980, 2, S7WLWord, delay_time);
if (tmp4 != 0)
{
    showParameterWarning("error", "设置剔除时间失败");
    return;
}



//剔除距离
uint32_t value2 = ui->lineEdit_7->text().toUInt();
byte delay_data[4] = {0};

// 大小端转换
delay_data[3] = (unsigned char)(0xFF & value2);
delay_data[2] = (unsigned char)((0xFF00 & value2) >> 8);
delay_data[1] = (unsigned char)((0xFF0000 & value2) >> 16);
delay_data[0] = (unsigned char)((0xFF000000 & value2) >> 24);

// 写入DB1.920
int tmp2 = client->WriteArea(S7AreaDB, 1, 920, 4, S7WLDWord, delay_data);
if (tmp2 != 0)
{
    showParameterWarning("error", "设置剔除距离失败");
    return;
}




//拍照时间
uint16_t value5 = ui->lineEdit_20->text().toUInt();
byte pz_time[2] = {0};

// 大小端转换
pz_time[1] = (unsigned char)(0xFF & value5);
pz_time[0] = (unsigned char)((0xFF00 & value5) >> 8);

// 写入DB1.982
int tmp5 = client->WriteArea(S7AreaDB, 1, 982, 2, S7WLWord, pz_time);
if (tmp5 != 0)
{
    showParameterWarning("error", "设置拍照时间失败");
    return;
}

//相机延时
QString text = ui->lineEdit_4->text();
emit sendDataTo(text);

//拍照距离

uint32_t value = ui->lineEdit_6->text().toUInt();
byte v_data[4] = {0};

// 大小端转换
v_data[3] = (unsigned char)(0xFF & value);
v_data[2] = (unsigned char)((0xFF00 & value) >> 8);
v_data[1] = (unsigned char)((0xFF0000 & value) >> 16);
v_data[0] = (unsigned char)((0xFF000000 & value) >> 24);

// 写入DB1.924
int tmp = client->WriteArea(S7AreaDB, 1, 924, 4, S7WLDWord, v_data);
if (tmp != 0)
{
    showParameterWarning("error", "设置拍照距离失败");
    return;
}

showParameterInfo("提示", "所有设置已经完成！");
}

/**
 * @brief cv::Mat转换为QImage
 * @param mat 输入的cv::Mat对象
 * @return QImage 转换后的QImage对象
 */
QImage Widget::cvMatToQImage(const cv::Mat &mat)
{
    if (mat.type() == CV_8UC1)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }
    else if (mat.type() == CV_8UC3)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    else if (mat.type() == CV_8UC4)
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                     QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {
        qDebug() << "ERROR: Mat could not be converted to QImage.";
        return QImage();
    }
}



void Widget::on_cancel_clicked()
{
    qDebug() << "=== on_cancel_clicked() START ===";

    // Step 2: 请求线程停止，保留当前模板状态，便于再次启动
    if (myThread) {
        myThread->requestStop();
    }

    if (cameraThread) {
        cameraThread->requestStop();
    }

    // 🔥 Step 3: myThread - 保持原逻辑
    bool myThreadWasRunning = false;
    bool myThreadStopped = true;
    if (myThread && myThread->isRunning()) {
        myThreadWasRunning = true;
        myThread->stop();
        if (!myThread->wait(3000)) {
            qDebug() << "WARNING: myThread did not stop";
            myThreadStopped = false;
        } else {
            myThread->stopTracking();
        }
    }

    // 🔥 Step 4: cameraThread - 停止逻辑
    bool needRestartCamera = false;
    bool cameraThreadStopped = true;
    if (cameraThread != nullptr) {
        needRestartCamera = true;
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);

        cameraThread->requestStop();
        if (!cameraThread->wait(3000)) {
            qDebug() << "WARNING: cameraThread did not stop";
            cameraThreadStopped = false;
        } else {
            cameraThread->stopTracking();
            cameraThread->deleteLater();
            cameraThread = nullptr;
        }
    }

    if (!myThreadStopped || !cameraThreadStopped) {
        ui->statusLabel->setText("停止中，请稍后再关闭相机");
        ui->plcbtn->setText("停止中...");
        ui->plcbtn->setEnabled(false);
        ui->VideoShoot->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        isCollecting = true;
        return;
    }

    // 🔥 Step 5: 如果cameraThread运行过，重启相机
    if ((needRestartCamera || myThreadWasRunning) && m_pcMyCamera) {
        try {
            m_pcMyCamera->Close();
            delete m_pcMyCamera;
            m_pcMyCamera = NULL;
            m_bOpenDevice = false;

            QThread::msleep(100);

            m_pcMyCamera = new CMvCamera;
            int nRet = m_pcMyCamera->Open(m_stDevList.pDeviceInfo[0]);

            if (MV_OK == nRet) {
                m_pcMyCamera->SetEnumValue("TriggerMode", 1);
                m_pcMyCamera->SetEnumValue("TriggerSource", 7);
                m_pcMyCamera->SetFloatValue("ExposureTime", 500);
                m_pcMyCamera->SetFloatValue("TriggerDelay", 0);
                m_pcMyCamera->RegisterImageCallBack();
                m_pcMyCamera->StartGrabbing();

                m_bOpenDevice = true;
                ui->statusLabel->setText("相机已打开");
            } else {
                delete m_pcMyCamera;
                m_pcMyCamera = nullptr;
            }
        } catch (...) {}
    }

    // Step 6: 处理事件队列
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    // Step 7: 清理临时绘制状态；生产统计保留，由“清零”按钮负责清空
    detectedRects.clear();
    selectionRect1 = QRect();

    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->setColor(1);
        imageLabel->clearSelection();
    }
    hideTemplateGuide();

    g_lastDrawResults.clear();
    g_lastPose = DetectionPose();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = 0;

    first = false;
    x = 1;
    j = 1;
    judge = false;

    ui->statusLabel->setText("已停止");
    ui->plcbtn->setText("启动");
    ui->plcbtn->setEnabled(true);
    ui->VideoShoot->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    isCollecting = false;

    qDebug() << "=== on_cancel_clicked() COMPLETED ===";
}



bool Widget::isChineseChar(unsigned char c)
{
    return (c & 0x80) != 0;
}

bool Widget::isAlnumOrChinese(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || isChineseChar(static_cast<unsigned char>(c));
}

/**
 * @brief 目标字符确定按钮点击槽函数
 */

void Widget::on_textsure_btn_clicked()
{
    if (ui->comboBox_4->currentIndex() == 1 && m_wordMultiTemplateMode)
    {
        if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
            showParameterWarning("提示", "请先停止检测后再修改模板字符");
            return;
        }

        if (m_wordTemplateProfiles.empty()) {
            showParameterInfoAsError("提示", "请先选择字库模板");
            return;
        }

        const int profileIndex = currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError("提示", "当前编辑模板无效");
            return;
        }

        WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        const QString newMubiaozifu = ui->dateEdit->toPlainText();

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        QString loadError;
        const QStringList baseNamesToFind = parseWordTemplateBaseNames(newMubiaozifu);
        if (!newMubiaozifu.trimmed().isEmpty()
                && !loadWordDigitTemplatesFromDir(profile.dirPath,
                                                  baseNamesToFind,
                                                  &tempTemplates,
                                                  &tempTemplateTargetIndexes,
                                                  &loadError)) {
            showParameterCritical("严重警告",
                QString("当前模板 [%1] 字符图片加载失败：\n%2\n\n本次更新已撤销。")
                .arg(profile.name)
                .arg(loadError));
            return;
        }

        const QString settingsFilePath = QDir(profile.dirPath).filePath("app_settings.appset");
        if (!QFile::exists(settingsFilePath)) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 缺少 app_settings.appset，无法保存目标字符。")
                                  .arg(profile.name));
            return;
        }

        QSettings settings(settingsFilePath, QSettings::IniFormat);
        settings.setValue("dateEdit_value", newMubiaozifu);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 的目标字符写入失败。")
                                  .arg(profile.name));
            return;
        }

        profile.targetText = newMubiaozifu;
        profile.targetCount = baseNamesToFind.size();
        profile.digitTemplates = tempTemplates;
        profile.digitTemplateTargetIndexes = tempTemplateTargetIndexes;

        const QString profileName = profile.name.isEmpty()
                ? QDir(profile.dirPath).dirName()
                : profile.name;
        showParameterInfo("提示",
                          QString("已更新产品模板 %1 的目标字符。\n其他产品模板未修改。")
                          .arg(profileName));
        return;
    }

    if((ui->comboBox_4->currentIndex() == 0)||(ui->comboBox_4->currentIndex() == 1))
    {
        // 1. 检查是否存在有效的模板路径
        if (currentTemplateDirPath.isEmpty()) {
            showParameterInfoAsError("提示", "请先选择产品模板文件夹");
            return;
        }
        // 2. 读取当前修改后的目标字符
        QString newMubiaozifu = ui->dateEdit->toPlainText();
        const bool shouldWriteSingleWordTarget = (ui->comboBox_4->currentIndex() == 1 && !m_wordMultiTemplateMode);
        auto writeSingleWordTargetText = [this, &newMubiaozifu]() -> bool {
            const QString settingsFilePath = QDir(currentTemplateDirPath).filePath("app_settings.appset");
            if (!QFile::exists(settingsFilePath)) {
                showParameterCritical("严重警告", "当前产品模板缺少 app_settings.appset，无法保存目标字符。");
                return false;
            }

            QSettings settings(settingsFilePath, QSettings::IniFormat);
            settings.setValue("dateEdit_value", newMubiaozifu);
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                showParameterCritical("严重警告", "当前产品模板的目标字符写入失败。");
                return false;
            }

            return true;
        };

        if (newMubiaozifu.isEmpty()) {
            if (shouldWriteSingleWordTarget && !writeSingleWordTargetText()) {
                return;
            }
            digitTemplates.clear();
            digitTemplateTargetIndexes.clear();
            showParameterInfoAsError("提示", "目标字符为空，已清空模板");
            return;
        }

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        QString loadError;
        const QStringList baseNamesToFind = parseWordTemplateBaseNames(newMubiaozifu);
        const bool includeVariantTemplates = (ui->comboBox_4->currentIndex() == 1);

        if (!loadWordDigitTemplatesFromDir(currentTemplateDirPath,
                                           baseNamesToFind,
                                           &tempTemplates,
                                           &tempTemplateTargetIndexes,
                                           &loadError,
                                           includeVariantTemplates)) {
            // 如果有任何图片读取失败或丢失，绝不更新到全局的 digitTemplates，同时给出严厉警告
            showParameterCritical("严重警告",
                QString("%1\n\n请检查产品模板文件夹内的字符图片是否存在或是否损坏（支持中文，无需关心后缀和大小写）！\n本次更新已撤销。")
                .arg(loadError));
            return;
        }

        if (shouldWriteSingleWordTarget && !writeSingleWordTargetText()) {
            return;
        }

        // 5. 全部成功后，再更新到全局容器
        digitTemplates = tempTemplates;
        digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        showParameterInfo("提示",
                          QString("目标字符确认成功，目标字符 %1 个，字符模板图 %2 张！")
                          .arg(baseNamesToFind.size())
                          .arg(static_cast<int>(digitTemplates.size())));
    }
    else{
     showParameterInfo("提示", "目标字符确认成功");
    }


}



void Widget::on_batchTextsure_btn_clicked()
{
    if (ui->comboBox_4->currentIndex() != 1 || !m_wordMultiTemplateMode) {
        on_textsure_btn_clicked();
        return;
    }

    if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
        showParameterWarning("提示", "请先停止检测后再批量修改模板字符");
        return;
    }

    if (m_wordTemplateProfiles.empty()) {
        showParameterInfoAsError("提示", "请先选择字库模板");
        return;
    }

    const QString newMubiaozifu = ui->dateEdit->toPlainText();
    const bool needLoadDigitTemplates = !newMubiaozifu.trimmed().isEmpty();
    QStringList baseNamesToFind;
    if (needLoadDigitTemplates) {
        baseNamesToFind = parseWordTemplateBaseNames(newMubiaozifu);
        if (baseNamesToFind.isEmpty()) {
            showParameterInfoAsError("提示", "目标字符解析失败");
            return;
        }
    }

    const int oldProfileIndex = currentWordTemplateProfileIndex();
    int successCount = 0;
    QStringList failedMessages;

    for (WordTemplateProfile &profile : m_wordTemplateProfiles) {
        const QString profileName = profile.name.isEmpty()
                ? QDir(profile.dirPath).dirName()
                : profile.name;

        QDir directory(profile.dirPath);
        if (profile.dirPath.isEmpty() || !directory.exists()) {
            failedMessages.append(QString("%1：产品模板文件夹不存在").arg(profileName));
            continue;
        }

        const QString settingsFilePath = directory.filePath("app_settings.appset");
        if (!QFile::exists(settingsFilePath)) {
            failedMessages.append(QString("%1：缺少 app_settings.appset").arg(profileName));
            continue;
        }

        std::vector<cv::Mat> tempTemplates;
        std::vector<int> tempTemplateTargetIndexes;
        if (needLoadDigitTemplates) {
            QString loadError;
            if (!loadWordDigitTemplatesFromDir(profile.dirPath,
                                               baseNamesToFind,
                                               &tempTemplates,
                                               &tempTemplateTargetIndexes,
                                               &loadError)) {
                failedMessages.append(QString("%1：%2")
                                      .arg(profileName)
                                      .arg(loadError));
                continue;
            }
        }

        QSettings settings(settingsFilePath, QSettings::IniFormat);
        settings.setValue("dateEdit_value", newMubiaozifu);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            failedMessages.append(QString("%1：目标字符写入失败").arg(profileName));
            continue;
        }

        profile.targetText = newMubiaozifu;
        profile.targetCount = baseNamesToFind.size();
        profile.digitTemplates = tempTemplates;
        profile.digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        ++successCount;
    }

    if (oldProfileIndex >= 0) {
        setCurrentWordTemplateEditIndex(oldProfileIndex);
    }

    if (successCount == 0) {
        showParameterCritical("严重警告",
                              QString("所有模板的目标字符批量保存失败：\n%1")
                              .arg(failedMessages.join("\n")));
        return;
    }

    if (!failedMessages.isEmpty()) {
        QMessageBox::warning(this,
                             "提示",
                             QString("已成功保存 %1 个模板，失败 %2 个：\n%3")
                             .arg(successCount)
                             .arg(failedMessages.size())
                             .arg(failedMessages.join("\n")));
        return;
    }

    showParameterInfo("提示", "已将当前目标字符保存到所有已选择的产品模板。");
}





/**
 * @brief QImage转换为cv::Mat指针
 * @param image 输入的QImage对象
 * @return cv::Mat* 转换后的Mat指针
 */
Mat *Widget::QImageToMat(const QImage &image)
{
    cv::Mat *mat = nullptr;
    if (image.isNull())
    {
        qWarning() << "QImage is null";
        return nullptr;
    }

    qDebug() << "QImage format:" << image.format();
    switch (image.format())
    {
    case QImage::Format_RGB32:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC4,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_ARGB32:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC4,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_BGRA2BGR);
        break;
    }
    case QImage::Format_RGB888:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC3,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_RGB2BGR);
        break;
    }
    case QImage::Format_Grayscale8:
    {
        mat = new cv::Mat(image.height(), image.width(), CV_8UC1,
                          const_cast<uchar *>(image.bits()), image.bytesPerLine());
        cv::cvtColor(*mat, *mat, cv::COLOR_RGB2BGR);
        break;
    }
    default:
    {
        qWarning() << "QImage format not handled in switch:" << image.format();
        break;
    }
    }

    if (!mat || mat->empty())
    {
        qWarning() << "Failed to convert QImage to cv::Mat";
        return nullptr;
    }
    return mat;
}

///**
// * @brief 延时确定按钮点击槽函数
// * @details 设置相机采集延时
// */
//void Widget::on_delayButton_clicked()
//{
//    QString text = ui->lineEdit_4->text();
//    emit sendDataTo(text);
//    QMessageBox::information(this, "提示", "相机延时设置成功");
//}

/**
 * @brief 清空结果标签槽函数
 */
void Widget::slot_clearResultLabel()
{
    ui->resultlabel_7->clear();
    allResults = "";
}

/**
 * @brief 窗口关闭事件
 * @param event 关闭事件对象
 * @details 关闭时保存设置，销毁所有OpenCV窗口
 */
void Widget::closeEvent(QCloseEvent *event)
{
    cv::destroyAllWindows();
    saveSettings();
    event->accept();
}

/**
 * @brief 图像参数确定按钮点击槽函数
 * @details 设置模板匹配的图像处理参数
 */
void Widget::on_pushButton_clicked()
{
    bool ok1;
    int width_min = ui->lineEdit_5->text().toInt(&ok1);
    int width_max = ui->lineEdit_9->text().toInt(&ok1);
    int height_min = ui->lineEdit_10->text().toInt(&ok1);
    int height_max = ui->lineEdit_11->text().toInt(&ok1);
    int block_size1 = ui->lineEdit_13->text().toInt(&ok1);
    int kernelsize = ui->lineEdit_15->text().toInt(&ok1);
    int horizontalKernel = ui->lineEdit_18->text().toInt(&ok1);
    int verticalKernel = ui->lineEdit_19->text().toInt(&ok1);

    // 统一判断：是否为有效整数 + 均为大于1的奇数
    bool isParamValid = true;
    // 再判断是否都满足「大于1且是奇数」
    if (block_size1 <= 1 || block_size1 % 2 != 1
            || kernelsize <= 1 || kernelsize % 2 != 1
            || horizontalKernel <= 1 || horizontalKernel % 2 != 1
            || verticalKernel <= 1 || verticalKernel % 2 != 1) {
        isParamValid = false;
    }

    // 统一弹窗警告
    if (!isParamValid) {
        showParameterWarning("参数错误", "图像处理参数必须均为大于1的奇数，请修正后重试！");
        return;
    }


    emit caijianchicun(width_min, width_max, height_min, height_max, block_size1,
                       horizontalKernel, verticalKernel);
    emit kernal(kernelsize);

    showParameterInfo("提示", "图像参数设置成功");
}

/**
 * @brief 阈值确定按钮点击槽函数
 * @details 设置相似度判断阈值
 */
void Widget::on_pushButton_3_clicked()
{
    if (ui->comboBox_4->currentIndex() == 1 && m_wordMultiTemplateMode) {
        if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting) {
            showParameterWarning("提示", "请先停止检测后再修改模板阈值");
            return;
        }

        const int profileIndex = currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError("提示", "当前阈值模板无效");
            return;
        }

        const QString thresholdText = ui->lineEdit_yuzhi->text().trimmed();
        bool thresholdOk = false;
        const int thresholdValue = static_cast<int>(thresholdText.toDouble(&thresholdOk));
        if (!thresholdOk) {
            showParameterWarning("参数错误", "图像合格阈值必须是数字");
            return;
        }

        WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(profileIndex)];
        const QString settingsFilePath = QDir(profile.dirPath).filePath("app_settings.appset");
        if (!QFile::exists(settingsFilePath)) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 缺少 app_settings.appset，无法保存图像阈值。")
                                  .arg(profile.name));
            return;
        }

        QSettings settings(settingsFilePath, QSettings::IniFormat);
        settings.setValue("lineEdit_yuzhi_value", thresholdText);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            showParameterCritical("严重警告",
                                  QString("当前模板 [%1] 的图像阈值写入失败。")
                                  .arg(profile.name));
            return;
        }

        profile.imageThresholdText = thresholdText;
        emit ssim(thresholdValue);
        showParameterInfo("提示",
                          QString("模板 [%1] 图像阈值设置成功：%2")
                          .arg(profile.name)
                          .arg(thresholdText));
        return;
    }

    int number = ui->lineEdit_yuzhi->text().toDouble();
    emit ssim(number);
    showParameterInfo("提示", "阈值设置成功");
}

/**
 * @brief 保存当前图像按钮点击槽函数
 * @details 打开文件保存对话框，保存当前显示的图像
 */
void Widget::on_pushButton_5_clicked()
{
    if (!myImage || myImage->empty()) {
        QMessageBox::warning(this, "提示", "请先点击【制作模板】拍照获取图像。");
        return;
    }
    if (!imageLabel->isTemplateDrawingEnabled()) {
        QMessageBox::warning(this, "提示",
                             QStringLiteral("\u8bf7\u5148\u70b9\u51fb\u3010\u5236\u4f5c\u6a21\u677f\u3011\u62cd\u7167\uff0c\u5e76\u5b8c\u6210\u5b9a\u4f4d\u533a\u57df\u548c\u55b7\u7801\u68c0\u6d4b\u533a\u57df\u6846\u9009\u3002"));
        return;
    }

    const bool isWordTemplateMode = (ui->comboBox_4->currentIndex() == 1);

    // 字库匹配模式下，保存前先检查框选状态，避免输入名称后才发现无法保存。
    QRect uiTrackRect = imageLabel->getTrackingRect();
    QPolygon uiDetectPoly = imageLabel->getDetectionPoly();

    if (isWordTemplateMode) {
        if (uiTrackRect.isNull()) {
            QMessageBox::warning(this, "提示", "请先框选定位区域。");
            return;
        }
        if (uiTrackRect.width() <= 5 || uiTrackRect.height() <= 5) {
            QMessageBox::warning(this, "提示", "定位区域太小，请重新框选。");
            return;
        }
        if (uiDetectPoly.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先框选喷码检测区域。");
            return;
        }
        if (uiDetectPoly.size() < 3 || !imageLabel->isDetectionPolyComplete()) {
            QMessageBox::warning(this, "提示", "喷码检测区域未闭合或点数不足，请重新框选。");
            return;
        }
    }

    auto defaultTemplateBaseDir = [this]() -> QString {
        if (!templateBaseDirPath.trimmed().isEmpty() && QDir(templateBaseDirPath).exists()) {
            return QDir(templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    QDialog inputDialog(this);
    inputDialog.setWindowTitle("保存模板");
    inputDialog.setWindowFlags(inputDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *mainLayout = new QVBoxLayout(&inputDialog);
    QFormLayout *formLayout = new QFormLayout();

    QLineEdit *nameEdit = new QLineEdit(&inputDialog);
    formLayout->addRow("产品模板文件夹名称：", nameEdit);
    QLabel *nameErrorLabel = new QLabel("模板文件夹名称不能为空", &inputDialog);
    nameErrorLabel->setStyleSheet("color: #d93025;");
    formLayout->addRow("", nameErrorLabel);

    QLineEdit *baseDirEdit = new QLineEdit(defaultTemplateBaseDir(), &inputDialog);
    QPushButton *browseButton = new QPushButton("浏览", &inputDialog);
    QHBoxLayout *baseDirLayout = new QHBoxLayout();
    baseDirLayout->addWidget(baseDirEdit);
    baseDirLayout->addWidget(browseButton);
    formLayout->addRow("模板文件夹保存目录：", baseDirLayout);

    QLabel *hintLabel = new QLabel(
                "保存后会记录当前产品的定位区域、喷码检测区域和参数配置。",
                &inputDialog);
    hintLabel->setWordWrap(true);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定", &inputDialog);
    QPushButton *cancelButton = new QPushButton("取消", &inputDialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(hintLabel);
    mainLayout->addLayout(buttonLayout);

    connect(browseButton, &QPushButton::clicked, this, [this, baseDirEdit]() {
        const QString selectedBaseDir = QFileDialog::getExistingDirectory(
                    this,
                    "选择模板文件夹保存目录",
                    baseDirEdit->text().trimmed().isEmpty() ? QDir::homePath() : baseDirEdit->text(),
                    QFileDialog::ShowDirsOnly);
        if (!selectedBaseDir.isEmpty()) {
            baseDirEdit->setText(QDir(selectedBaseDir).absolutePath());
        }
    });
    auto updateNameState = [nameEdit, nameErrorLabel, okButton]() {
        const bool isEmpty = nameEdit->text().trimmed().isEmpty();
        okButton->setEnabled(!isEmpty);
        nameErrorLabel->setVisible(isEmpty);
        nameEdit->setStyleSheet(isEmpty ? "QLineEdit { border: 1px solid #d93025; }" : "");
    };
    connect(nameEdit, &QLineEdit::textChanged, &inputDialog, updateNameState);
    updateNameState();
    connect(okButton, &QPushButton::clicked, &inputDialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &inputDialog, &QDialog::reject);

    if (inputDialog.exec() != QDialog::Accepted) return;

    QString newFolderName = nameEdit->text().trimmed();
    if (newFolderName.isEmpty()) return;

    QString baseDirPath = baseDirEdit->text().trimmed();
    if (baseDirPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择模板文件夹保存目录。");
        return;
    }
    baseDirPath = QDir(baseDirPath).absolutePath();

    QString savePath = QDir(baseDirPath).absoluteFilePath(newFolderName);
    QDir dir(savePath);
    if (isWordTemplateMode && dir.exists()) {
        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("确认覆盖");
        confirmBox.setText(QString("产品模板 [%1] 已存在。\n\n"
                                   "继续保存会覆盖该产品模板中的定位区域、喷码检测区域和参数配置。\n"
                                   "是否继续？").arg(newFolderName));
        QPushButton *overwriteButton = confirmBox.addButton("覆盖", QMessageBox::AcceptRole);
        QPushButton *cancelButton = confirmBox.addButton("取消", QMessageBox::RejectRole);
        confirmBox.setDefaultButton(cancelButton);
        confirmBox.exec();
        if (confirmBox.clickedButton() != overwriteButton) {
            return;
        }
    }
    if (!dir.mkpath(".")) {
        QMessageBox::warning(this, "错误", "产品模板文件夹创建失败，无法保存模板。");
        return;
    }
    templateBaseDirPath = baseDirPath;

    if (!isWordTemplateMode && (uiTrackRect.isNull() || uiDetectPoly.isEmpty() || uiDetectPoly.size() < 3)) {
        QMessageBox::warning(this, "警告",
                             "保存模板前，请先在图像上完成以下操作：\n\n"
                             "1. 框选定位区域\n"
                             "2. 框选并闭合喷码检测区域\n\n"
                             "完成后再点击【保存模板】。");
        return;
    }

    // 2. 转换坐标 (使用局部 clone 确保计算基准稳定)
    cv::Mat calibImg = myImage->clone();

    auto toPhysicalPoint = [&](QPoint uiPt) -> cv::Point2f {
        QSize labelSize = imageLabel->size();
        QSize imgSize(calibImg.cols, calibImg.rows);
        QSize scaledSize = imgSize.scaled(labelSize, Qt::KeepAspectRatio);
        int xOff = (labelSize.width() - scaledSize.width()) / 2;
        int yOff = (labelSize.height() - scaledSize.height()) / 2;
        double ratio = (double)imgSize.width() / scaledSize.width();

        float px = (uiPt.x() - xOff) * ratio;
        float py = (uiPt.y() - yOff) * ratio;
        return cv::Point2f(px, py);
    };

    auto toPhysicalRect = [&](QRect uiRect) -> cv::Rect2d {
        cv::Point2f tl = toPhysicalPoint(uiRect.topLeft());
        cv::Point2f br = toPhysicalPoint(uiRect.bottomRight());
        cv::Rect2d phys(tl.x, tl.y, br.x - tl.x, br.y - tl.y);
        
        phys.x = std::max(0.0, phys.x);
        phys.y = std::max(0.0, phys.y);
        if (phys.x + phys.width > calibImg.cols) phys.width = calibImg.cols - phys.x;
        if (phys.y + phys.height > calibImg.rows) phys.height = calibImg.rows - phys.y;
        return phys;
    };

    savedTrackingBox = toPhysicalRect(uiTrackRect);
    
    // 计算多边形的绝对物理坐标，并存入 YAML 相对坐标 (相对于追踪框中心)
    std::vector<cv::Point2f> absDatePoly;
    for (const QPoint& pt : uiDetectPoly) {
        absDatePoly.push_back(toPhysicalPoint(pt));
    }
    
    cv::Point2f trackCenter(savedTrackingBox.x + savedTrackingBox.width / 2.0, 
                            savedTrackingBox.y + savedTrackingBox.height / 2.0);
                            
    std::vector<cv::Point2f> relDatePoly;
    for (const auto& pt : absDatePoly) {
        relDatePoly.push_back(cv::Point2f(pt.x - trackCenter.x, pt.y - trackCenter.y));
    }
    savedDatePoly = relDatePoly;
    hasValidBoxes = true;

    // 3. 物理保存
    cv::imwrite(dir.absoluteFilePath("template_raw.png").toLocal8Bit().toStdString(), calibImg);
    cv::Mat tplImg = calibImg(savedTrackingBox).clone();
    cv::imwrite(dir.absoluteFilePath("tracking_template.bmp").toLocal8Bit().toStdString(), tplImg);
    m_loadedTrackingTemplate = tplImg.clone();

    currentTemplateDirPath = savePath;

    QString yamlPath = savePath + "/calibrate_config.yaml";
    {
        cv::FileStorage fs(yamlPath.toLocal8Bit().toStdString(), cv::FileStorage::WRITE);
        fs << "date_poly" << relDatePoly;
        fs.release();
    }

    // 4. 特征标定 (仅模式 0)
    if (ui->comboBox_4->currentIndex() == 0) {
        QMessageBox::information(this, "标定提示", "即将标定吸管口和钢印区。");

        // 吸管口标定
        cv::Rect ringRect = getQuickRectROI(calibImg, "ROI_1");
        if (ringRect.width > 5 && ringRect.height > 5) {
            cv::Mat ringTpl = calibImg(ringRect).clone();
            QString ringPath = savePath + "/template_ring.bmp";
            cv::imwrite(ringPath.toLocal8Bit().toStdString(), ringTpl);

            // 计算中心点用于相对坐标转换 (仿照 widget1.cpp 逻辑)
            cv::Point2f cRing(ringRect.x + ringRect.width / 2.0f, ringRect.y + ringRect.height / 2.0f);

            // 钢印多边形标定
            std::vector<cv::Point> stampPts = getPolygonROI(calibImg, "ROI_2");
            if (stampPts.size() >= 3) {
                // 转换相对坐标并使用 FileStorage 保存 (关键：确保引擎能读懂)
                std::vector<cv::Point2f> relStamp;
                for (const auto& pt : stampPts) {
                    relStamp.push_back(cv::Point2f(pt.x - cRing.x, pt.y - cRing.y));
                }

                cv::FileStorage fs(yamlPath.toLocal8Bit().toStdString(), cv::FileStorage::WRITE);
                fs << "stamp_poly" << relStamp;
                fs << "date_poly" << relDatePoly;
                fs.release();

                // 重新初始化检测引擎
                initOverlapDetectorFromCurrentDir();
            }
        }
    }

    // 5. 保存所有配置
    saveSettingsToDir(savePath);
    saveSettings();
    imageLabel->setTemplateDrawingEnabled(false);
    imageLabel->clearSelection();
    hideTemplateGuide();
    m_currentTemplateNameVisible = true;
    updateCurrentTemplateName();
    if (isWordTemplateMode) {
        QMessageBox splitMessageBox(this);
        splitMessageBox.setIcon(QMessageBox::Information);
        splitMessageBox.setWindowTitle("保存成功");
        splitMessageBox.setText("产品模板已保存成功。\n\n是否立即切割字符模板？");
        QPushButton *splitButton = splitMessageBox.addButton("确定", QMessageBox::AcceptRole);
        splitMessageBox.addButton("取消", QMessageBox::RejectRole);
        splitMessageBox.setDefaultButton(splitButton);
        splitMessageBox.exec();

        if (splitMessageBox.clickedButton() == splitButton) {
            showManualCharacterTemplateCropDialog();
        }
    } else {
        QMessageBox::information(this, "成功", "模板及双框配置已全部保存！");
    }
}

// 先定义一个保存参数到指定文件夹的函数（可放在Widget类中）
void Widget::saveSettingsToDir(const QString &dirPath)
{
    // 确保目标文件夹存在，不存在则创建
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath("."); // 创建文件夹（包括多级目录）
    }

    // 配置文件路径：用户选择的文件夹 + "app_settings.appset"
    QString settingsFilePath = dirPath + "/app_settings.appset";
    QSettings settings(settingsFilePath, QSettings::IniFormat); // 强制使用INI格式

    // 保存所有参数（与原逻辑一致，只是路径改为指定文件夹）
    settings.setValue("spinbox_value", ui->spinBox->text());
    settings.setValue("lineEdit_14_value", ui->lineEdit_14->text()); // 相机增益
    settings.setValue("lineEdit_6_value", ui->lineEdit_6->text());
    settings.setValue("lineEdit_7_value", ui->lineEdit_7->text());
    settings.setValue("lineEdit_8_value", ui->lineEdit_8->text());
    settings.setValue("lineEdit_20_value", ui->lineEdit_20->text());
    settings.setValue("lineEdit_12_value", ui->lineEdit_12->text());
    settings.setValue("lineEdit_4_value", ui->lineEdit_4->text());
    settings.setValue("lineEdit_13_value", ui->lineEdit_13->text());
    settings.setValue("lineEdit_15_value", ui->lineEdit_15->text());
    settings.setValue("lineEdit_18_value", ui->lineEdit_18->text());
    settings.setValue("lineEdit_19_value", ui->lineEdit_19->text());
    settings.setValue("lineEdit_yuzhi_value", ui->lineEdit_yuzhi->text());
    settings.setValue("lineEdit_tissueRoughnessThreshold_value", ui->lineEdit_tissueRoughnessThreshold->text());
    settings.setValue("dateEdit_value", ui->dateEdit->toPlainText());
    settings.setValue("comboBox_value", ui->comboBox->currentText());
    settings.setValue("comboBox_saveImageType_value", ui->comboBox_saveImageType->currentText());
    settings.setValue("comboBox_2_value", ui->comboBox_2->currentText());
    settings.setValue("comboBox_3_value", ui->comboBox_3->currentText());
    settings.setValue("comboBox_4_value", ui->comboBox_4->currentText());
    // 新增：保存模板路径
    settings.setValue("saveDirPath", selectedDir);
    settings.setValue("TemplateDirPath", currentTemplateDirPath);
    settings.setValue("lineEdit_value", ui->lineEdit->text()); // 保存IP地址


    const bool currentTrackingBoxValid = hasValidBoxes
            && savedTrackingBox.width > 0
            && savedTrackingBox.height > 0;
    const bool shouldWriteCurrentTrackingBox = currentTrackingBoxValid && !m_wordMultiTemplateMode;

    bool trackingTemplateFileValid = false;
    const QString trackingTemplatePath = dir.absoluteFilePath("tracking_template.bmp");
    QFile trackingTemplateFile(trackingTemplatePath);
    if (trackingTemplateFile.open(QIODevice::ReadOnly)) {
        const QByteArray trackingTemplateBytes = trackingTemplateFile.readAll();
        if (!trackingTemplateBytes.isEmpty()) {
            const uchar *trackingTemplateData = reinterpret_cast<const uchar *>(trackingTemplateBytes.constData());
            std::vector<uchar> trackingTemplateBuffer(trackingTemplateData,
                                                      trackingTemplateData + trackingTemplateBytes.size());
            trackingTemplateFileValid = !cv::imdecode(trackingTemplateBuffer, cv::IMREAD_COLOR).empty();
        }
    }

    bool datePolyFileValid = false;
    const QString calibratePath = dir.absoluteFilePath("calibrate_config.yaml");
    if (QFile::exists(calibratePath)) {
        CalibrationData calib;
        if (calib.load(calibratePath.toLocal8Bit().toStdString())) {
            datePolyFileValid = !calib.date_poly.empty();
        }
    }

    const bool existingTemplateFilesValid = trackingTemplateFileValid && datePolyFileValid;

    // 保存框有效标记。多模板批量保存参数时，不使用当前界面的单模板框覆盖每个产品模板。
    if (shouldWriteCurrentTrackingBox) {
        settings.setValue("trackingBox_x", savedTrackingBox.x);
        settings.setValue("trackingBox_y", savedTrackingBox.y);
        settings.setValue("trackingBox_width", savedTrackingBox.width);
        settings.setValue("trackingBox_height", savedTrackingBox.height);

        settings.setValue("hasValidBoxes", true);

    } else if (existingTemplateFilesValid) {
        settings.setValue("hasValidBoxes", true);
    } else {
        settings.setValue("hasValidBoxes", false);
    }
}
void Widget::initOverlapDetectorFromCurrentDir() {
    if (currentTemplateDirPath.isEmpty()) {
        qDebug() << "[DEBUG] currentTemplateDirPath is EMPTY. Skipping engine init.";
        return;
    }

    // 1. 定义文件路径
    QString ringPath = currentTemplateDirPath + "/template_ring.bmp";
    QString yamlPath = currentTemplateDirPath + "/calibrate_config.yaml";

    // 2. 获取绝对路径（用于排查由于相对路径导致的加载失败）
    QFileInfo ringInfo(ringPath);
    QFileInfo yamlInfo(yamlPath);

    qDebug() << "============ Path Debug Info ============";
    qDebug() << "Template Dir: " << currentTemplateDirPath;
    qDebug() << "Absolute Ring Path: " << ringInfo.absoluteFilePath();
    qDebug() << "Ring File Exists? " << (ringInfo.exists() ? "YES" : "NO");
    qDebug() << "Absolute YAML Path: " << yamlInfo.absoluteFilePath();
    qDebug() << "YAML File Exists? " << (yamlInfo.exists() ? "YES" : "NO");
    qDebug() << "=========================================";

    if (ringInfo.exists() && yamlInfo.exists()) {
        // 使用 toLocal8Bit().toStdString() 以支持 Windows 下的本地编码路径
        try {
            bool ok = overlapDetector.init(ringPath.toLocal8Bit().toStdString(),
                                           yamlPath.toLocal8Bit().toStdString());
            if (!ok) {
                qDebug() << "[ERROR] overlapDetector.init returned FALSE. Check if BMP is corrupted.";
            } else {
                qDebug() << "[SUCCESS] Overlap Engine is initialized and ready.";
            }
        } catch (...) {
            qDebug() << "[致命错误] overlapDetector.init 内部发生 C++ 崩溃！可能是 OpenCV 异常或 YAML 解析错误！";
        }
    } else {
        qDebug() << "[ERROR] Cannot start engine: One or more files missing on disk.";
    }
}

/**
 * @brief 加载字库按钮点击槽函数
 * @details 支持带括号的字符格式，如"0(1)"表示0字符的第1个变体
 */
// 按钮pushButton_4的点击事件槽函数
// 功能：从用户输入解析模板文件名，选择产品模板文件夹
void Widget::on_pushButton_4_clicked()
{
    QString dirPath;
    auto templateDialogStartDir = [this]() -> QString {
        if (!templateBaseDirPath.trimmed().isEmpty() && QDir(templateBaseDirPath).exists()) {
            return QDir(templateBaseDirPath).absolutePath();
        }

        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (desktopPath.trimmed().isEmpty()) {
            desktopPath = QDir::homePath();
        }
        return QDir(desktopPath).absolutePath();
    };

    if (ui->comboBox_4->currentIndex() == 1) {
        QFileDialog dialog(this, "选择产品模板文件夹", templateDialogStartDir());
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setLabelText(QFileDialog::LookIn, "查找范围:");
        dialog.setLabelText(QFileDialog::FileName, "文件夹:");
        dialog.setLabelText(QFileDialog::FileType, "文件类型:");
        dialog.setLabelText(QFileDialog::Accept, "选择");
        dialog.setLabelText(QFileDialog::Reject, "取消");
        dialog.setNameFilter("所有文件 (*)");

        QListView *listView = dialog.findChild<QListView *>("listView");
        if (listView) {
            listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        }
        QTreeView *treeView = dialog.findChild<QTreeView *>();
        if (treeView) {
            treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
            treeView->setHeaderHidden(true);
            treeView->setColumnHidden(1, true);
            treeView->setColumnHidden(2, true);
            treeView->setColumnHidden(3, true);
        }

        if (dialog.exec() != QDialog::Accepted) return;

        QStringList selectedDirs;
        const QStringList dialogSelectedDirs = dialog.selectedFiles();
        for (const QString &selectedDirPath : dialogSelectedDirs) {
            const QString cleanDir = QDir(selectedDirPath).absolutePath();
            if (!cleanDir.isEmpty() && !selectedDirs.contains(cleanDir)) {
                selectedDirs.append(cleanDir);
            }
        }

        if (selectedDirs.isEmpty()) return;
        const QFileInfo firstSelectedDirInfo(selectedDirs.first());
        if (firstSelectedDirInfo.dir().exists()) {
            templateBaseDirPath = firstSelectedDirInfo.dir().absolutePath();
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        hideTemplateGuide();

        if (selectedDirs.size() > 1) {
            std::vector<WordTemplateProfile> loadedProfiles;
            QStringList loadedDirPaths;
            QStringList skippedMessages;
            QStringList pendingTargetMessages;

            for (const QString &selectedDirPath : selectedDirs) {
                QDir templateDir(selectedDirPath);

                QString targetText;
                QString imageThresholdText;
                QStringList baseNames;
                const QString settingsFilePath = templateDir.filePath("app_settings.appset");
                if (!QFile::exists(settingsFilePath)) {
                    skippedMessages.append(QString("%1：缺少 app_settings.appset").arg(templateDir.dirName()));
                    qDebug() << "[WORD_MULTI_TEMPLATE] skip app_settings missing:" << selectedDirPath;
                    continue;
                } else {
                    QSettings settings(settingsFilePath, QSettings::IniFormat);
                    targetText = settings.value("dateEdit_value").toString();
                    imageThresholdText = settings.value("lineEdit_yuzhi_value", ui->lineEdit_yuzhi->text()).toString();
                    if (imageThresholdText.trimmed().isEmpty()) {
                        imageThresholdText = ui->lineEdit_yuzhi->text();
                    }
                    baseNames = parseWordTemplateBaseNames(targetText);
                    if (targetText.trimmed().isEmpty() || baseNames.isEmpty()) {
                        pendingTargetMessages.append(QString("%1：目标字符为空或解析失败，目标字符待设置").arg(templateDir.dirName()));
                        qDebug() << "[WORD_MULTI_TEMPLATE] target text pending:" << selectedDirPath;
                    }
                }

                const QString yamlPath = templateDir.filePath("calibrate_config.yaml");
                if (!QFile::exists(yamlPath)) {
                    skippedMessages.append(QString("%1：缺少 calibrate_config.yaml").arg(templateDir.dirName()));
                    qDebug() << "[WORD_MULTI_TEMPLATE] skip calibrate_config missing:" << selectedDirPath;
                    continue;
                }

                CalibrationData calib;
                if (!calib.load(yamlPath.toLocal8Bit().toStdString()) || calib.date_poly.empty()) {
                    skippedMessages.append(QString("%1：date_poly 读取失败").arg(templateDir.dirName()));
                    qDebug() << "[WORD_MULTI_TEMPLATE] skip date_poly invalid:" << selectedDirPath;
                    continue;
                }

                const QString trackingPath = templateDir.filePath("tracking_template.bmp");
                if (!QFile::exists(trackingPath)) {
                    skippedMessages.append(QString("%1：缺少 tracking_template.bmp").arg(templateDir.dirName()));
                    qDebug() << "[WORD_MULTI_TEMPLATE] skip tracking_template missing:" << selectedDirPath;
                    continue;
                }

                cv::Mat trackingTemplate;
                QFile trackingFile(trackingPath);
                if (trackingFile.open(QIODevice::ReadOnly)) {
                    QByteArray trackingData = trackingFile.readAll();
                    try {
                        std::vector<uchar> trackingBuffer(trackingData.begin(), trackingData.end());
                        trackingTemplate = cv::imdecode(trackingBuffer, cv::IMREAD_COLOR);
                    } catch (...) {
                        qDebug() << "[WORD_MULTI_TEMPLATE] tracking_template imdecode crashed:" << selectedDirPath;
                    }
                }
                if (trackingTemplate.empty()) {
                    skippedMessages.append(QString("%1：tracking_template.bmp 读取失败").arg(templateDir.dirName()));
                    qDebug() << "[WORD_MULTI_TEMPLATE] skip tracking_template empty:" << selectedDirPath;
                    continue;
                }

                std::vector<cv::Mat> loadedDigitTemplates;
                std::vector<int> loadedDigitTemplateTargetIndexes;
                if (!baseNames.isEmpty()) {
                    QString loadError;
                    if (!loadWordDigitTemplatesFromDir(templateDir.absolutePath(),
                                                       baseNames,
                                                       &loadedDigitTemplates,
                                                       &loadedDigitTemplateTargetIndexes,
                                                       &loadError)) {
                        pendingTargetMessages.append(QString("%1：字符图片不完整，目标字符待重新确认：%2")
                                                     .arg(templateDir.dirName())
                                                     .arg(loadError));
                        qDebug() << "[WORD_MULTI_TEMPLATE] digit file count mismatch, target pending:"
                                 << selectedDirPath
                                 << loadError;
                    } else {
                        qDebug() << "[WORD_MULTI_TEMPLATE] digit templates loaded:"
                                 << selectedDirPath
                                 << "target chars:" << baseNames.size()
                                 << "template images:" << static_cast<int>(loadedDigitTemplates.size());
                    }
                }

                WordTemplateProfile profile;
                profile.name = templateDir.dirName();
                profile.dirPath = templateDir.absolutePath();
                profile.trackingTemplate = trackingTemplate;
                profile.datePoly = calib.date_poly;
                profile.targetText = targetText;
                profile.imageThresholdText = imageThresholdText;
                profile.targetCount = baseNames.size();
                profile.digitTemplates = loadedDigitTemplates;
                profile.digitTemplateTargetIndexes = loadedDigitTemplateTargetIndexes;
                loadedDirPaths.append(profile.dirPath);
                loadedProfiles.push_back(profile);
            }

            if (loadedProfiles.empty()) {
                m_wordTemplateDirPaths.clear();
                m_wordTemplateProfiles.clear();
                m_wordMultiTemplateMode = false;
                currentTemplateDirPath.clear();
                m_currentTemplateNameVisible = false;
                updateCurrentTemplateName();
                refreshWordTemplateEditorCombo();
                QString detailMessage = "所选字库模板配置全部无效，未进入多模板模式。";
                if (!skippedMessages.isEmpty()) {
                    detailMessage += "\n\n具体原因：\n" + skippedMessages.join("\n");
                }
                if (!pendingTargetMessages.isEmpty()) {
                    detailMessage += "\n\n目标字符待设置：\n" + pendingTargetMessages.join("\n");
                }
                showParameterCritical("严重警告", detailMessage);
                return;
            }

            m_wordTemplateDirPaths = loadedDirPaths;
            m_wordTemplateProfiles.swap(loadedProfiles);
            m_wordMultiTemplateMode = true;
            currentTemplateDirPath.clear();
            m_currentTemplateNameVisible = false;
            updateCurrentTemplateName();
            refreshWordTemplateEditorCombo();
            saveSettings();

            qDebug() << "[WORD_MULTI_TEMPLATE] selected dirs:" << m_wordTemplateDirPaths;
            if (!skippedMessages.isEmpty() || !pendingTargetMessages.isEmpty()) {
                QString detailMessage = QString("已加载 %1 个字库模板").arg(static_cast<int>(m_wordTemplateProfiles.size()));
                if (!pendingTargetMessages.isEmpty()) {
                    detailMessage += "\n\n以下模板已加载，但目标字符待设置：\n" + pendingTargetMessages.join("\n");
                }
                if (!skippedMessages.isEmpty()) {
                    detailMessage += "\n\n以下模板已跳过：\n" + skippedMessages.join("\n");
                }
                showParameterWarning("提示",
                                     detailMessage);
            } else {
                showParameterInfo("提示",
                                  QString("已加载 %1 个有效字库模板")
                                  .arg(static_cast<int>(m_wordTemplateProfiles.size())));
            }
            return;
        }

        dirPath = selectedDirs.first();
    } else {
        dirPath = QFileDialog::getExistingDirectory(nullptr, "选择产品模板文件夹",
                                                    templateDialogStartDir(),
                                                    QFileDialog::ShowDirsOnly);
        if (dirPath.isEmpty()) return;
        const QFileInfo selectedDirInfo(dirPath);
        if (selectedDirInfo.dir().exists()) {
            templateBaseDirPath = selectedDirInfo.dir().absolutePath();
        }
        if (imageLabel) {
            imageLabel->setTemplateDrawingEnabled(false);
        }
        hideTemplateGuide();
    }

    if (dirPath.isEmpty()) return;

    m_wordTemplateDirPaths.clear();
    m_wordTemplateProfiles.clear();
    m_wordMultiTemplateMode = false;
    refreshWordTemplateEditorCombo();
    currentTemplateDirPath = dirPath;

    saveSettings(); // 保存路径
    m_currentTemplateNameVisible = loadSettingsFromDir(dirPath);
    updateCurrentTemplateName();
    if (ui->comboBox_4->currentIndex() == 1 && imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearGreenRects();
        imageLabel->clearSelection();
        hideTemplateGuide();
        displayWordTemplateRawImage(dirPath);
    }
    wrongindex = ui->lineEdit_12->text().toInt();

    qDebug()<<"currentTemplate"<<currentTemplateDirPath;

    initOverlapDetectorFromCurrentDir();

    //设置PLC参数
    if (!ui->checkBox->isChecked())
    {
        QMessageBox::information(this, "提示", "模板已选择");
        return;
    }

    //判断plc是否连接
    if (!client->Connected())
    {
        QMessageBox::warning(this, "警告", "已启用触发，但PLC未连接！");
        if (m_currentTemplateNameVisible) {
            QMessageBox::information(this, "提示", "模板已选择");
        } else {
            QMessageBox::warning(this, "提示", "模板加载失败，请检查产品模板文件夹");
        }
        return;
    }

    uint32_t value = ui->lineEdit_6->text().toUInt();
    byte v_data[4] = {0};

    // 大小端转换
    v_data[3] = (unsigned char)(0xFF & value);
    v_data[2] = (unsigned char)((0xFF00 & value) >> 8);
    v_data[1] = (unsigned char)((0xFF0000 & value) >> 16);
    v_data[0] = (unsigned char)((0xFF000000 & value) >> 24);

    // 写入DB1.924
    int tmp = client->WriteArea(S7AreaDB, 1, 924, 4, S7WLDWord, v_data);
    if (tmp != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }


    uint32_t value2 = ui->lineEdit_7->text().toUInt();
    byte delay_data[4] = {0};

    // 大小端转换
    delay_data[3] = (unsigned char)(0xFF & value2);
    delay_data[2] = (unsigned char)((0xFF00 & value2) >> 8);
    delay_data[1] = (unsigned char)((0xFF0000 & value2) >> 16);
    delay_data[0] = (unsigned char)((0xFF000000 & value2) >> 24);

    // 写入DB1.920
    int tmp2 = client->WriteArea(S7AreaDB, 1, 920, 4, S7WLDWord, delay_data);
    if (tmp2 != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }

    uint16_t value4 = ui->lineEdit_8->text().toUInt();
    byte delay_time[2] = {0};

    // 大小端转换
    delay_time[1] = (unsigned char)(0xFF & value4);
    delay_time[0] = (unsigned char)((0xFF00 & value4) >> 8);

    // 写入DB1.980
    int tmp4 = client->WriteArea(S7AreaDB, 1, 980, 2, S7WLWord, delay_time);
    if (tmp4 != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }


    uint16_t value5 = ui->lineEdit_20->text().toUInt();
    byte pz_time[2] = {0};

    // 大小端转换
    pz_time[1] = (unsigned char)(0xFF & value5);
    pz_time[0] = (unsigned char)((0xFF00 & value5) >> 8);

    // 写入DB1.982
    int tmp5 = client->WriteArea(S7AreaDB, 1, 982, 2, S7WLWord, pz_time);
    if (tmp5 != 0)
    {
        QMessageBox::warning(this, "error", "设置失败");
    }



    QMessageBox::information(this, "提示", "模板已选择");
}

/**
 * @brief 选择保存文件夹按钮点击槽函数
 */
void Widget::on_pushButton_browseImageSavePath_clicked()
{
    const QString dirPath = QFileDialog::getExistingDirectory(
                this,
                "选择图像保存路径",
                selectedDir.isEmpty() ? QString("C:/") : selectedDir,
                QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) {
        return;
    }

    selectedDir = dirPath;
    updateSaveDirButtonText();
    qDebug() << "save file path:" << selectedDir;
}



/**
 * @brief 清空总数统计按钮点击槽函数
 */
void Widget::on_cut_cancelButton_2_clicked()
{
    totalImages = 0;
    ngImages = 0;
    ui->ngnum->setText(QString("%1").arg(ngImages));
    ui->imagenum->setText(QString("%1").arg(totalImages));
}

/**
 * @brief 清空NG数统计按钮点击槽函数
 */
void Widget::on_cut_cancelButton_3_clicked()
{
    ngImages = 0;
    ui->ngnum->setText(QString("%1").arg(ngImages));
}

/**
 * @brief 旋转角度确定按钮点击槽函数
 * @details 设置图像旋转角度（0°、90°、180°、270°）
 */
void Widget::on_pushButton_9_clicked()
{
    int index = ui->comboBox_2->currentIndex();
    switch (index)
    {
    case 1:
        angleValue = 1;
        break;
    case 2:
        angleValue = 2;
        break;
    case 3:
        angleValue = 3;
        break;
    default:
        angleValue = 0;
    }

    emit rotate(angleValue);
    showParameterInfo("提示", "旋转角度设置成功");
}



bool Widget::loadSettingsFromDir(const QString &dirPath)
{
    // 配置文件路径：用户选择的文件夹 + "app_settings.appset"
    QString settingsFilePath = dirPath + "/app_settings.appset";
    const bool settingsFileExists = QFile::exists(settingsFilePath);
    QSettings settings(settingsFilePath, QSettings::IniFormat); // 对应保存时的INI格式

    if (settings.contains("spinbox_value")) ui->spinBox->setValue(settings.value("spinbox_value").toInt());
    if (settings.contains("lineEdit_14_value")) ui->lineEdit_14->setText(settings.value("lineEdit_14_value").toString()); // 初始化增益显示
    if (settings.contains("lineEdit_6_value")) ui->lineEdit_6->setText(settings.value("lineEdit_6_value").toString());
    if (settings.contains("lineEdit_7_value")) ui->lineEdit_7->setText(settings.value("lineEdit_7_value").toString());
    if (settings.contains("lineEdit_8_value")) ui->lineEdit_8->setText(settings.value("lineEdit_8_value").toString());
    if (settings.contains("lineEdit_20_value")) ui->lineEdit_20->setText(settings.value("lineEdit_20_value").toString());
    if (settings.contains("lineEdit_12_value")) ui->lineEdit_12->setText(settings.value("lineEdit_12_value").toString());
    if (settings.contains("lineEdit_4_value")) ui->lineEdit_4->setText(settings.value("lineEdit_4_value").toString());
    if (settings.contains("lineEdit_13_value")) ui->lineEdit_13->setText(settings.value("lineEdit_13_value").toString());
    if (settings.contains("lineEdit_15_value")) ui->lineEdit_15->setText(settings.value("lineEdit_15_value").toString());
    if (settings.contains("lineEdit_18_value")) ui->lineEdit_18->setText(settings.value("lineEdit_18_value").toString());
    if (settings.contains("lineEdit_19_value")) ui->lineEdit_19->setText(settings.value("lineEdit_19_value").toString());
    if (settings.contains("lineEdit_yuzhi_value")) ui->lineEdit_yuzhi->setText(settings.value("lineEdit_yuzhi_value").toString());
    if (settings.contains("lineEdit_tissueRoughnessThreshold_value")) {
        ui->lineEdit_tissueRoughnessThreshold->setText(settings.value("lineEdit_tissueRoughnessThreshold_value").toString());
    }
    applyTissueRoughnessThresholdFromUi(false);

    if (settings.contains("lineEdit_value")) {
        ui->lineEdit->setText(settings.value("lineEdit_value").toString());
    }

    if (settings.contains("dateEdit_value")) {
        ui->dateEdit->setPlainText(settings.value("dateEdit_value").toString());
    }

    if (settings.contains("comboBox_value")) {
        QString value = settings.value("comboBox_value").toString();
        int index = ui->comboBox->findText(value);
        if (index >= 0) ui->comboBox->setCurrentIndex(index);
    }
    if (settings.contains("comboBox_saveImageType_value")) {
        QString value = settings.value("comboBox_saveImageType_value").toString();
        int index = ui->comboBox_saveImageType->findText(value);
        if (index >= 0) ui->comboBox_saveImageType->setCurrentIndex(index);
    }
    if (settings.contains("comboBox_2_value")) {
        QString value = settings.value("comboBox_2_value").toString();
        int index = ui->comboBox_2->findText(value);
        if (index >= 0) ui->comboBox_2->setCurrentIndex(index);
    }
    if (settings.contains("comboBox_3_value")) {
        QString value1 = settings.value("comboBox_3_value").toString();
        int index = ui->comboBox_3->findText(value1);
        if (index >= 0) ui->comboBox_3->setCurrentIndex(index);
    }
    if (settings.contains("comboBox_4_value")) {
        QString value2 = settings.value("comboBox_4_value").toString();
        int index = ui->comboBox_4->findText(value2);
        if (index >= 0) ui->comboBox_4->setCurrentIndex(index);
    }

    if (settings.contains("TemplateDirPath")) {
        currentTemplateDirPath = settings.value("TemplateDirPath").toString();
    }
    if (settings.contains("saveDirPath")) {
        selectedDir = settings.value("saveDirPath").toString();
    }
    updateSaveDirButtonText();
    updateTissueRoughnessUiVisibility();

    const cv::Rect2d loadedTrackingBox(
                settings.value("trackingBox_x", 0).toDouble(),
                settings.value("trackingBox_y", 0).toDouble(),
                settings.value("trackingBox_width", 0).toDouble(),
                settings.value("trackingBox_height", 0).toDouble());
    const bool storedTrackingBoxValid = settings.contains("trackingBox_width")
            && settings.contains("trackingBox_height")
            && loadedTrackingBox.width > 0
            && loadedTrackingBox.height > 0;
    const bool configSaysValidBoxes = settings.contains("hasValidBoxes")
            && settings.value("hasValidBoxes").toBool();

    // 🔥 加载双框坐标
    if (configSaysValidBoxes || storedTrackingBoxValid) {
        savedTrackingBox = loadedTrackingBox;
        hasValidBoxes = true;
        if (!configSaysValidBoxes && storedTrackingBoxValid) {
            qDebug() << "[TEMPLATE_REPAIR] hasValidBoxes=false, but tracking box coordinates are valid. Restoring tracking box:" << dirPath;
        }
        qDebug() << "box load success";
    } else {
        savedTrackingBox = cv::Rect2d(0, 0, 0, 0);
        hasValidBoxes = false;
        qDebug() << "no usesful box";
    }

    // 🔥 新增：加载局部静态追踪模板 (Anchor Template)
    QString tplPath = dirPath + "/tracking_template.bmp";
    m_loadedTrackingTemplate.release();
    QFile trackingTemplateFile(tplPath);
    if (trackingTemplateFile.open(QIODevice::ReadOnly)) {
        const QByteArray trackingTemplateBytes = trackingTemplateFile.readAll();
        if (!trackingTemplateBytes.isEmpty()) {
            const uchar *trackingTemplateData = reinterpret_cast<const uchar *>(trackingTemplateBytes.constData());
            std::vector<uchar> trackingTemplateBuffer(trackingTemplateData,
                                                      trackingTemplateData + trackingTemplateBytes.size());
            m_loadedTrackingTemplate = cv::imdecode(trackingTemplateBuffer, cv::IMREAD_COLOR);
        }
    }
    if (!m_loadedTrackingTemplate.empty()) {
        qDebug() << "成功加载定位模板图片：" << tplPath;
    } else {
        qDebug() << "警告：未找到 tracking_template.bmp";
    }

    savedDatePoly.clear();
    QString yamlPath = dirPath + "/calibrate_config.yaml";
    if (QFile::exists(yamlPath)) {
        CalibrationData calib;
        if (calib.load(yamlPath.toLocal8Bit().toStdString())) {
            savedDatePoly = calib.date_poly;
        }
    }

    const bool templateFilesValid = !m_loadedTrackingTemplate.empty() && !savedDatePoly.empty();
    if (templateFilesValid) {
        if (!hasValidBoxes) {
            qDebug() << "[TEMPLATE_REPAIR] hasValidBoxes=false, but tracking_template.bmp and calibrate_config.yaml are valid. Repairing template flag:" << dirPath;
        }
        hasValidBoxes = true;
        if (settingsFileExists && settings.status() == QSettings::NoError) {
            settings.setValue("hasValidBoxes", true);
            settings.sync();
        }
    } else {
        hasValidBoxes = false;
    }

    updateCurrentTemplateName();
    return settingsFileExists && settings.status() == QSettings::NoError;
}


/**
 * @brief 加载设置
 * @details 从QSettings加载所有参数设置
 */
void Widget::loadSettings()
{
    QSettings settings("YourCompany", "YourApplication");

    if (settings.contains("spinbox_value"))
        ui->spinBox->setValue(settings.value("spinbox_value").toInt());
        
    if (settings.contains("lineEdit_14_value"))
        ui->lineEdit_14->setText(settings.value("lineEdit_14_value").toString());

    if (settings.contains("lineEdit_6_value"))
        ui->lineEdit_6->setText(settings.value("lineEdit_6_value").toString());

    if (settings.contains("lineEdit_7_value"))
        ui->lineEdit_7->setText(settings.value("lineEdit_7_value").toString());

    if (settings.contains("lineEdit_8_value"))
        ui->lineEdit_8->setText(settings.value("lineEdit_8_value").toString());

    if (settings.contains("lineEdit_20_value"))
        ui->lineEdit_20->setText(settings.value("lineEdit_20_value").toString());

    if (settings.contains("lineEdit_12_value"))
        ui->lineEdit_12->setText(settings.value("lineEdit_12_value").toString());

    if (settings.contains("lineEdit_4_value"))
        ui->lineEdit_4->setText(settings.value("lineEdit_4_value").toString());

    if (settings.contains("lineEdit_13_value"))
        ui->lineEdit_13->setText(settings.value("lineEdit_13_value").toString());

    if (settings.contains("lineEdit_15_value"))
        ui->lineEdit_15->setText(settings.value("lineEdit_15_value").toString());

    if (settings.contains("lineEdit_18_value"))
        ui->lineEdit_18->setText(settings.value("lineEdit_18_value").toString());

    if (settings.contains("lineEdit_19_value"))
        ui->lineEdit_19->setText(settings.value("lineEdit_19_value").toString());

    if (settings.contains("lineEdit_yuzhi_value"))
        ui->lineEdit_yuzhi->setText(settings.value("lineEdit_yuzhi_value").toString());

    if (settings.contains("lineEdit_tissueRoughnessThreshold_value"))
        ui->lineEdit_tissueRoughnessThreshold->setText(settings.value("lineEdit_tissueRoughnessThreshold_value").toString());

    applyTissueRoughnessThresholdFromUi(false);

    if (settings.contains("dateEdit_value"))
        ui->dateEdit->setPlainText(settings.value("dateEdit_value").toString());

    if (settings.contains("lineEdit_value")) {
        ui->lineEdit->setText(settings.value("lineEdit_value").toString());
    }

    if (settings.contains("comboBox_value"))
    {
        QString value = settings.value("comboBox_value").toString();
        int index = ui->comboBox->findText(value);
        if (index >= 0)
            ui->comboBox->setCurrentIndex(index);
    }

    if (settings.contains("comboBox_saveImageType_value"))
    {
        QString value = settings.value("comboBox_saveImageType_value").toString();
        int index = ui->comboBox_saveImageType->findText(value);
        if (index >= 0)
            ui->comboBox_saveImageType->setCurrentIndex(index);
    }

    if (settings.contains("comboBox_2_value"))
    {
        QString value = settings.value("comboBox_2_value").toString();
        int index = ui->comboBox_2->findText(value);
        if (index >= 0)
            ui->comboBox_2->setCurrentIndex(index);
    }

    if (settings.contains("comboBox_3_value"))
    {
        QString value1 = settings.value("comboBox_3_value").toString();
        int index = ui->comboBox_3->findText(value1);
        if (index >= 0)
            ui->comboBox_3->setCurrentIndex(index);
    }

    if (settings.contains("comboBox_4_value"))
    {
        QString value2 = settings.value("comboBox_4_value").toString();
        int index = ui->comboBox_4->findText(value2);
        if (index >= 0)
            ui->comboBox_4->setCurrentIndex(index);
    }

    if (settings.contains("comboBox_5_value"))
    {
        QString value3 = settings.value("comboBox_5_value").toString();
        int index = ui->comboBox_5->findText(value3);
        if (index >= 0)
            ui->comboBox_5->setCurrentIndex(index);
    }

    if (settings.contains("TemplateDirPath")) {
        currentTemplateDirPath = settings.value("TemplateDirPath").toString();
    }

    if (settings.contains("templateBaseDirPath")) {
        templateBaseDirPath = settings.value("templateBaseDirPath").toString();
    }

    if (settings.contains("saveDirPath")) {
        selectedDir = settings.value("saveDirPath").toString();
    }
    updateSaveDirButtonText();
    updateTissueRoughnessUiVisibility();
}

/**
 * @brief 保存设置
 * @details 保存所有参数设置到QSettings
 */
void Widget::saveSettings()
{
    QSettings settings("YourCompany", "YourApplication");

    settings.setValue("spinbox_value", ui->spinBox->text());
    settings.setValue("lineEdit_14_value", ui->lineEdit_14->text()); // 固化全局相机增益
    settings.setValue("lineEdit_6_value", ui->lineEdit_6->text());
    settings.setValue("lineEdit_7_value", ui->lineEdit_7->text());
    settings.setValue("lineEdit_8_value", ui->lineEdit_8->text());
    settings.setValue("lineEdit_20_value", ui->lineEdit_20->text());
    settings.setValue("lineEdit_12_value", ui->lineEdit_12->text());
    settings.setValue("lineEdit_4_value", ui->lineEdit_4->text());
    settings.setValue("lineEdit_13_value", ui->lineEdit_13->text());
    settings.setValue("lineEdit_15_value", ui->lineEdit_15->text());
    settings.setValue("lineEdit_18_value", ui->lineEdit_18->text());
    settings.setValue("lineEdit_19_value", ui->lineEdit_19->text());
    settings.setValue("lineEdit_yuzhi_value", ui->lineEdit_yuzhi->text());
    settings.setValue("lineEdit_tissueRoughnessThreshold_value", ui->lineEdit_tissueRoughnessThreshold->text());
//    settings.setValue("dateEdit_value", ui->dateEdit->toPlainText());
    settings.setValue("comboBox_value", ui->comboBox->currentText());
    settings.setValue("comboBox_saveImageType_value", ui->comboBox_saveImageType->currentText());
    settings.setValue("comboBox_2_value", ui->comboBox_2->currentText());
    settings.setValue("comboBox_3_value", ui->comboBox_3->currentText());
    settings.setValue("comboBox_4_value", ui->comboBox_4->currentText());
    settings.setValue("comboBox_5_value", ui->comboBox_5->currentText());
    // 新增：保存模板路径
    settings.setValue("saveDirPath", selectedDir);
    settings.setValue("TemplateDirPath", m_wordMultiTemplateMode ? QString() : currentTemplateDirPath);
    settings.setValue("templateBaseDirPath", templateBaseDirPath);
    settings.setValue("lineEdit_value", ui->lineEdit->text()); // 保存IP地址
}

/**
 * @brief 设置默认值
 * @details 为所有UI控件设置初始默认值
 */
void Widget::setupDefaultValues()
{
    ui->lineEdit_6->setText("50");
    ui->lineEdit_7->setText("300");
    ui->lineEdit_8->setText("500");
    ui->lineEdit_20->setText("500");
    ui->lineEdit_18->setText("1");
    ui->lineEdit_19->setText("3");
    ui->lineEdit_12->setText("0");
    ui->lineEdit_4->setText("300");
    ui->lineEdit_13->setText("11");
    ui->lineEdit_15->setText("3");
    ui->lineEdit_yuzhi->setText("70");
    ui->lineEdit_tissueRoughnessThreshold->setText(QString::number(TissueRollDetector::defaultRoughnessThreshold(), 'f', 3));
    ui->dateEdit->setPlainText("");
    ui->spinBox->setValue(800);
    ui->lineEdit_14->setText("1.0"); // 默认增益
    ui->comboBox->setCurrentText("不保存图像");
    ui->comboBox_saveImageType->setCurrentText("只保存带识别框图像");
    ui->comboBox_4->setCurrentText("字库匹配");
    ui->comboBox_2->setCurrentText("无旋转");
    ui->comboBox_3->setCurrentText("间歇触发模式");
    ui->checkBox->setChecked(true);
    updateSaveDirButtonText();
    updateTissueRoughnessUiVisibility();
    applyTissueRoughnessThresholdFromUi(false);
}

// ================= 拦截滚轮误操作事件 =================
bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_characterSplitSettingsDialog
            && event->type() == QEvent::EnterWhatsThisMode) {
        QString helpText = m_characterSplitSettingsDialog->property("characterSplitHelpText").toString();
        if (helpText.isEmpty()) {
            helpText = "这里用于调整自动分割字符模板图片时使用的参数。";
        }
        QWhatsThis::showText(QCursor::pos(), helpText, m_characterSplitSettingsDialog);
        QWhatsThis::leaveWhatsThisMode();
        return true;
    }

    if (watched == ui->textsure_btn
            || watched == ui->batchTextsure_btn
            || watched == ui->pushButton_browseImageSavePath
            || watched == ui->pushButton_11
            || watched == ui->pushButton_7
            || watched == ui->pushButton_10
            || watched == ui->label_4
            || watched == ui->label_27
            || watched == ui->label_16
            || watched == ui->label_14
            || watched == ui->label_13
            || watched == ui->label_6
            || watched == ui->label_10
            || watched == ui->label_17
            || watched == ui->label_8
            || watched == ui->comboBox_3
            || watched == ui->Saveimage
            || watched == m_manualCharacterCropButton
            || watched == ui->confirmAllParamsButton
            || watched == ui->VideoShoot) {
        QWidget *button = qobject_cast<QWidget *>(watched);
        if (!button) {
            return QWidget::eventFilter(watched, event);
        }

        if (event->type() == QEvent::Enter) {
            QTimer::singleShot(500, this, [this, button, watched]() {
                if (!button->underMouse()) {
                    return;
                }

                QString tooltipText;
                if (watched == ui->VideoShoot) {
                    switch (ui->comboBox_4->currentIndex()) {
                    case 0:
                        tooltipText =
                                "制作模板匹配产品模板：\n\n"
                                "1. 点击后拍摄当前产品图像。\n"
                                "2. 在图像上框选定位区域和检测区域。\n"
                                "3. 点击【保存模板】保存产品模板。";
                        break;
                    case 1:
                        tooltipText =
                                "制作字库产品模板步骤：\n\n"
                                "1. 点击后拍摄当前产品图像。\n"
                                "2. 在图像上按住鼠标左键，框选定位区域。\n"
                                "3. 用鼠标左键依次点击喷码区域边缘。\n"
                                "4. 点击鼠标右键闭合喷码检测区域。\n"
                                "5. 点击【保存模板】保存产品模板。";
                        break;
                    case 2:
                        tooltipText =
                                "深度模型模式通常不需要制作传统产品模板。\n\n"
                                "请确认模型文件和相关参数已经配置完成。";
                        break;
                    case 3:
                        tooltipText =
                                "纸巾检测通常不需要制作产品模板。\n\n"
                                "请设置纸巾检测粗糙度阈值后启动检测。";
                        break;
                    default:
                        tooltipText = "点击后拍摄当前图像，用于制作产品模板。";
                        break;
                    }
                } else {
                    tooltipText = button->toolTip();
                }

                if (!tooltipText.isEmpty()) {
                    QToolTip::showText(QCursor::pos(), tooltipText, button);
                }
            });
            return false;
        }

        if (event->type() == QEvent::Leave) {
            QToolTip::hideText();
            return false;
        }

        if (event->type() == QEvent::ToolTip) {
            return true;
        }
    }

    if (event->type() == QEvent::Wheel) {
        // 利用类的继承关系，全局拦截所有 QComboBox 和 QAbstractSpinBox(如QSpinBox, QDoubleSpinBox)
        if (watched->inherits("QComboBox") || watched->inherits("QAbstractSpinBox")) {
            return true; // 返回 true 表示事件已处理（被丢弃），彻底禁止滚轮
        }
    }
    return QWidget::eventFilter(watched, event);
}

//关闭相机按钮
void Widget::on_CloseCamera_clicked()
{
    if (!isCollecting && myThread && myThread->isRunning()) {
        myThread->requestStop();
        myThread->stop();
        if (!myThread->wait(3000)) {
            QMessageBox::warning(this, "警告", "相机正在检测采图中！\n请先点击【停止识别】完全停止检测后，再关闭相机。");
            return;
        }
    }

    if (!isCollecting && cameraThread && cameraThread->isRunning()) {
        cameraThread->requestStop();
        if (!cameraThread->wait(3000)) {
            QMessageBox::warning(this, "警告", "相机正在检测采图中！\n请先点击【停止识别】完全停止检测后，再关闭相机。");
            return;
        }
    }

    // 如果系统正在采集中（软触发或硬触发线程在跑），拦截关闭并提示
    if ((myThread && myThread->isRunning()) || (cameraThread && cameraThread->isRunning()) || isCollecting)
    {
        QMessageBox::warning(this, "警告", "相机正在检测采图中！\n请先点击【停止识别】完全停止检测后，再关闭相机。");
        return;
    }

    if (m_pcMyCamera)
    {
        m_pcMyCamera->Close();
        delete m_pcMyCamera;
        m_pcMyCamera = NULL;
        m_bOpenDevice = false;
    }
    // 清空文本并将文本置0
    ui->resultlabel->clear();
    imageLabel->setTemplateDrawingEnabled(false);
    hideTemplateGuide();
    imageLabel->clear();
    ui->image_undetected->clear();
    ui->imagenum->clear();
    ui->ngnum->clear();
    //    ui->ocrResult->clear();
    ui->resultlabel_7->clear();
    ui->speedLabel->clear();
    ngImages = 0;
    totalImages = 0;
    //    qDebug()<<"totaltime"<<totalTime<<"s";
    //    totalTime=0;
    // 标记相机关闭状态
    m_bOpenDevice = false;
    ui->statusLabel->setText("相机已关闭");
    ui->statusLabel->setStyleSheet("QLabel{color:#e74c3c; font-weight:bold;}");
}

void Widget::on_MultiCameraMode_clicked()
{
    if (!m_multiCameraWidget) {
        m_multiCameraWidget = new MultiCameraWidget(this);
        m_multiCameraWidget->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_multiCameraWidget, &QObject::destroyed, this, [this]() {
            m_multiCameraWidget = nullptr;
        });
    }

    m_multiCameraWidget->show();
    m_multiCameraWidget->raise();
    m_multiCameraWidget->activateWindow();
}


void Widget::on_plcbtn_clicked()
{
    qDebug() << "=== on_plcbtn_clicked() START ===";

    if (!m_bOpenDevice)
    {
        QMessageBox::warning(this, "警告", "采集失败,请打开设备！");
        return;
    }

    updateCurrentTemplateName();
    const bool isWordMode = (ui->comboBox_4->currentIndex() == 1);
    const bool isTissueMode = (ui->comboBox_4->currentIndex() == 3);
    const bool isWordMultiMode = isWordMode
            && m_wordMultiTemplateMode
            && !m_wordTemplateProfiles.empty();
    const QString runningTemplateName = isTissueMode
            ? QString("无")
            : (isWordMultiMode ? QString("字库多模板") : QDir(currentTemplateDirPath).dirName());

    if (isWordMode && m_wordMultiTemplateMode && m_wordTemplateProfiles.empty()) {
        QMessageBox::warning(this, "提示", "当前字库多模板缓存为空，请重新选择产品模板文件夹。");
        return;
    }

    // 启动检测只检查真实模板文件，不再让历史 hasValidBoxes=false 单独阻止启动。
    QStringList productTemplateErrors;
    if (!isTissueMode && !isWordMultiMode) {
        if (currentTemplateDirPath.trimmed().isEmpty()) {
            productTemplateErrors.append("未选择产品模板文件夹");
        }
        if (m_loadedTrackingTemplate.empty()) {
            productTemplateErrors.append("定位模板图片 tracking_template.bmp 缺失或读取失败");
        }
        if (savedDatePoly.empty()) {
            productTemplateErrors.append("喷码检测区域 calibrate_config.yaml/date_poly 缺失或读取失败");
        }
    }
    if (!productTemplateErrors.isEmpty()) {
        QMessageBox::warning(this, "操作规范",
                             QString("缺少可用产品模板，无法启动检测。\n\n"
                                     "具体原因：\n%1\n\n"
                                     "如果是新产品：\n"
                                     "请先【拍照】，框选定位区域和喷码检测区域，然后点击【保存模板】。\n\n"
                                     "如果是已有产品：\n"
                                     "请点击【选择模板】，选择对应产品模板文件夹。")
                             .arg(productTemplateErrors.join("\n")));
        return;
    }

    std::vector<WordTrackingProfile> wordTrackingProfilesForRun;
    if (isWordMultiMode) {
        QStringList pendingProfiles;
        for (int i = 0; i < static_cast<int>(m_wordTemplateProfiles.size()); ++i) {
            const WordTemplateProfile &profile = m_wordTemplateProfiles[static_cast<size_t>(i)];
            const QString profileName = profile.name.isEmpty()
                    ? QDir(profile.dirPath).dirName()
                    : profile.name;

            if (profile.targetText.trimmed().isEmpty() || profile.digitTemplates.empty()) {
                pendingProfiles.append(profileName);
                continue;
            }

            WordTrackingProfile trackingProfile;
            trackingProfile.name = profileName;
            trackingProfile.profileIndex = i;
            trackingProfile.trackingTemplate = profile.trackingTemplate;
            trackingProfile.datePoly = profile.datePoly;
            wordTrackingProfilesForRun.push_back(trackingProfile);
        }

        if (!pendingProfiles.isEmpty()) {
            QMessageBox::warning(this,
                                 "提示",
                                 QString("以下字库模板还没有确认目标字符，不能启动多模板检测：\n%1")
                                 .arg(pendingProfiles.join("\n")));
            return;
        }
        if (wordTrackingProfilesForRun.empty()) {
            QMessageBox::warning(this, "提示", "没有可用的字库多模板定位配置。");
            return;
        }
    }

    if (!m_allParamsConfirmed) {
        QMessageBox messageBox(this);
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowTitle("操作确认");
        messageBox.setText("当前没有点击“确认所有参数”按钮，是否继续运行");

        QPushButton *continueButton = messageBox.addButton("继续运行", QMessageBox::AcceptRole);
        QPushButton *cancelButton = messageBox.addButton("取消", QMessageBox::RejectRole);
        messageBox.setDefaultButton(cancelButton);
        messageBox.exec();

        if (messageBox.clickedButton() != continueButton) {
            return;
        }
    }
    m_allParamsConfirmed = false;
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();

    // ==========================================================
    // 以下为原有启动线程逻辑，完全保留你所有的 PLC/相机 流程
    // ==========================================================
    if (ui->checkBox->isChecked())
    {
        // 外部触发/硬触发模式逻辑
        int exposureValue = ui->spinBox->value();
        float gainValue = ui->lineEdit_14->text().toFloat();
        m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);
        m_pcMyCamera->SetFloatValue("Gain", gainValue);

        if (isCollecting) {
            QMessageBox::information(this, "提示", "已在采集中，若要停止请点击【停止识别】按钮");
            return;
        }

        j = 1;
        ui->image_undetected->clear();
        ui->imagenum->clear();
        ui->ngnum->clear();
        ui->resultlabel_7->clear();
        ui->speedLabel->clear();
        ngImages = 0;
        totalImages = 0;

        // 重置相机状态
        if (m_pcMyCamera) {
            try {
                m_pcMyCamera->StopGrabbing();
                QThread::msleep(200);
                m_pcMyCamera->SetEnumValue("TriggerMode", 1);
                m_pcMyCamera->SetEnumValue("TriggerSource", 0); // 硬触发
                m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);
                m_pcMyCamera->SetFloatValue("Gain", gainValue); // 恢复写入增益
                m_pcMyCamera->SetFloatValue("TriggerDelay", 0);
                m_pcMyCamera->RegisterImageCallBack();
                m_pcMyCamera->StartGrabbing();

                m_pcMyCamera->SetEnumValue("LineDebouncerTime", 5000.0); // 硬触发

                QThread::msleep(100);
            } catch (...) {
                QMessageBox::critical(this, "错误", "相机初始化失败！");
                return;
            }
        }

        // 清理并新建硬触发线程
        if (cameraThread) {
            if (cameraThread->isRunning()) {
                cameraThread->requestStop();
                cameraThread->wait(1500);
            }
            disconnect(cameraThread, nullptr, this, nullptr);
            delete cameraThread;
        }

        cameraThread = new CameraThread(this, m_pcMyCamera);

        cameraThread->setBypassTracking(isTissueMode);
        if (isTissueMode) {
            cameraThread->clearPresetBoxes();
            cameraThread->clearWordTemplateTrackingProfiles();
        } else if (isWordMultiMode) {
            cameraThread->clearPresetBoxes();
            cameraThread->setWordTemplateTrackingProfiles(wordTrackingProfilesForRun);
        } else {
            // 🔥 核心修改：将双框坐标和静态模板喂给线程
            cameraThread->setPresetBoxes(savedDatePoly, savedTrackingBox);
            cameraThread->setPreloadedTemplate(m_loadedTrackingTemplate);
        }

        // 连接所有功能信号
        connect(this, &Widget::rotate, cameraThread, &CameraThread::receiveangle1);
        connect(this, &Widget::choosechannel,cameraThread,&CameraThread::receivecolorchannel);
        connect(cameraThread, &CameraThread::signal_cleanlabel, this, &Widget::slot_clearResultLabel, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_messImage, this, [this](cv::Mat img) {
            this->slot_displayAndDetect(&img);
        }, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_boxesSelected, this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
            this->dispatchDetectionByMode(&img, pose);
        }, Qt::QueuedConnection);
        connect(cameraThread, &CameraThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
            this->slot_handleTissueResult(&img, result);
        }, Qt::QueuedConnection);

        // 发送各项参数
        emit rotate(ui->comboBox_2->currentIndex() == 1 ? 1 : (ui->comboBox_2->currentIndex() == 2 ? 2 : (ui->comboBox_2->currentIndex() == 3 ? 3 : 0)));
        emit choosechannel(ui->comboBox_5->currentIndex() == 1 ? 1 : (ui->comboBox_5->currentIndex() == 2 ? 2 : (ui->comboBox_5->currentIndex() == 3 ? 3 : 0)));
        emit sendDataTo(ui->lineEdit_4->text());
        emit jiancestring(ui->dateEdit->toPlainText().toStdString());

        emit caijianchicun(ui->lineEdit_5->text().toInt(), ui->lineEdit_9->text().toInt(), ui->lineEdit_10->text().toInt(), ui->lineEdit_11->text().toInt(), ui->lineEdit_13->text().toInt(), ui->lineEdit_18->text().toInt(), ui->lineEdit_19->text().toInt());
        emit kernal(ui->lineEdit_15->text().toInt());
        emit ssim(ui->lineEdit_yuzhi->text().toDouble());

        cameraThread->start();
        if (!cameraThread->wait(100)) {
            isCollecting = true;
            ui->statusLabel->setText(QString("触发模式运行中\n产品模板：%1").arg(runningTemplateName));
            ui->plcbtn->setText("采集中...");
            ui->plcbtn->setEnabled(false);
            ui->VideoShoot->setEnabled(false);
            ui->pushButton_4->setEnabled(false);
        } else {
            isCollecting = false;
        }
    }
    else
    {
        // 软触发/连续模式逻辑
        int exposureValue = ui->spinBox->value();
        float gainValue = ui->lineEdit_14->text().toFloat();
        m_pcMyCamera->SetFloatValue("ExposureTime", exposureValue);
        m_pcMyCamera->SetFloatValue("Gain", gainValue); // 软触发切入时也保持增益同步

        ensureThreadsReady();
        if (!myThread) reinitializeMyThread();

        myThread->setBypassTracking(isTissueMode);
        if (isTissueMode) {
            myThread->clearPresetBoxes();
            myThread->clearWordTemplateTrackingProfiles();
        } else if (isWordMultiMode) {
            myThread->clearPresetBoxes();
            myThread->setWordTemplateTrackingProfiles(wordTrackingProfilesForRun);
        } else {
            // 🔥 核心修改：将双框坐标和静态模板喂给线程
            myThread->setPresetBoxes(savedDatePoly, savedTrackingBox);
            myThread->setPreloadedTemplate(m_loadedTrackingTemplate);
        }

        connect(myThread, &MyThread::signal_boxesSelected, this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

        // 发送参数
        emit ssim(ui->lineEdit_yuzhi->text().toDouble());
        emit rotate(ui->comboBox_2->currentIndex() == 1 ? 1 : (ui->comboBox_2->currentIndex() == 2 ? 2 : (ui->comboBox_2->currentIndex() == 3 ? 3 : 0)));
        emit choosechannel(ui->comboBox_5->currentIndex() == 1 ? 1 : (ui->comboBox_5->currentIndex() == 2 ? 2 : (ui->comboBox_5->currentIndex() == 3 ? 3 : 0)));
        emit sendDataTo(ui->lineEdit_4->text());

        m_pcMyCamera->SetEnumValue("TriggerSource", 7); // 软触发
        m_pcMyCamera->SetFloatValue("Gain", gainValue); // 软触发重新设置增益
        myThread->getCameraPtr(m_pcMyCamera);
        myThread->getImagePtr(myImage);

        if (!myThread->isRunning()) {
            myThread->start();
            ui->statusLabel->setText(QString("软触发模式运行中\n产品模板：%1").arg(runningTemplateName));
            ui->plcbtn->setEnabled(false);
            ui->VideoShoot->setEnabled(false);
            ui->pushButton_4->setEnabled(false);
        }
    }


    qDebug() << "=== on_plcbtn_clicked() COMPLETED ===";
}
// 检测相机
void Widget::on_HandwareDetect_clicked()
{
    if (m_bOpenDevice)
    {
        QMessageBox::warning(this, "警告", "相机已连接！");
        return;
    }

    // 查找设备
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = CMvCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_stDevList);
    if (MV_OK != nRet || m_stDevList.nDeviceNum == 0)
    {
        QMessageBox::warning(this, "警告", "未找到相机设备！");
        return;
    }

    //连接PLC
    QByteArray ad(ui->lineEdit->text().toUtf8());
    Address = ad.data();

    int tmp = client->ConnectTo(Address, 0, 1);

    if (tmp != 0)
    {
        QMessageBox::critical(this, "error", "PLC连接失败");
    }
    else{
    qDebug()<<"opencamera，plc connect success";
    }

    // 打开设备
    m_pcMyCamera = new CMvCamera;
    if (m_pcMyCamera == nullptr)
    {
        return;
    }

    // 假设只有一个相机，直接打开第一个设备
    int nIndex = 0;
    nRet = m_pcMyCamera->Open(m_stDevList.pDeviceInfo[nIndex]);
    //    qDebug() << "Connect:" << nRet;
    if (MV_OK != nRet)
    {
        delete m_pcMyCamera;
        m_pcMyCamera = nullptr;
        QMessageBox::warning(this, "警告", "打开设备失败！");
        return;
    }
    else
    {
        ui->statusLabel->setText("相机已打开");
        ui->statusLabel->setStyleSheet("QLabel{color:#2ecc71; font-weight:bold;}");
        QMessageBox::information(this, "提示", "相机打开成功！");
    }

    // 设置为触发模式
    m_pcMyCamera->SetEnumValue("TriggerMode", 1);
    // 设置触发源为编码器触发
    m_pcMyCamera->SetEnumValue("TriggerSource", 0);
    // 设置默认曝光时间
    m_pcMyCamera->SetFloatValue("ExposureTime", 500);
    m_pcMyCamera->SetFloatValue("TriggerDelay", 0);
    // 开启相机采集
    m_pcMyCamera->RegisterImageCallBack();
    m_pcMyCamera->StartGrabbing();
    //        connect(cameraThread,&CameraThread::threaderror,this,&Widget::onthreaderrormessage);

    myThread->getCameraPtr(m_pcMyCamera);
    myThread->getImagePtr(myImage);

    m_bOpenDevice = true;
}

// PLC模式选择
void Widget::on_plcmodebtn_clicked()
{
    PLCmode = ui->comboBox_3->currentIndex();
    if (!client->Connected())
    { // 未连接则不执行
        showParameterWarning("警告", "PLC未连接！");
        return;
    }

    if (PLCmode == 0)
    {
        uint8_t value = 0;

        byte mode_data[1] = {0}; // Buffer to hold the data to write to PLC

        // 设置要写入的字节
        mode_data[0] = (unsigned char)(0xFF & value);

        // 写入DB1的1032位置，写入1个字节
        int tmp2 = client->WriteArea(S7AreaDB, 1, 1032, 1, S7WLByte, mode_data); // 使用S7WLByte确保只写入1个字节
        if (tmp2 != 0)
        {
            showParameterWarning("error", "设置连续模式失败");
            return;
        }
        showParameterInfo("提示", "连续模式设置成功");
    }
    else if (PLCmode == 1)
    {
        uint8_t value = 1;

        byte mode_data[1] = {0}; // Buffer to hold the data to write to PLC

        // 设置要写入的字节
        mode_data[0] = (unsigned char)(0xFF & value);

        // 写入DB1的1032位置，写入1个字节
        int tmp2 = client->WriteArea(S7AreaDB, 1, 1032, 1, S7WLByte, mode_data); // 使用S7WLByte确保只写入1个字节
        if (tmp2 != 0)
        {
            showParameterWarning("error", "设置间歇模式失败");
            return;
        }
        showParameterInfo("提示", "间歇模式设置成功");
    }
}

// 模板匹配参数设置
void Widget::on_pushButton_2_clicked()
{
    // 确定裁剪尺寸
    bool ok1;
    int width_min = ui->lineEdit_5->text().toInt(&ok1);
    int width_max = ui->lineEdit_9->text().toInt(&ok1);
    int height_min = ui->lineEdit_10->text().toInt(&ok1);
    int height_max = ui->lineEdit_11->text().toInt(&ok1);
    int block_size1 = ui->lineEdit_13->text().toInt(&ok1);
    int kernelsize = ui->lineEdit_15->text().toInt(&ok1);
    int horizontalKernel = ui->lineEdit_18->text().toInt(&ok1);
    int verticalKernel = ui->lineEdit_19->text().toInt(&ok1);


    emit caijianchicun(width_min, width_max, height_min, height_max, block_size1,  horizontalKernel, verticalKernel);
    emit kernal(kernelsize);
    showParameterInfo("提示", "模板尺寸设置成功");
}

// 剔除位置设置
void Widget::on_eliminatebutton_clicked()
{
    wrongindex = ui->lineEdit_12->text().toInt();
    QMessageBox::information(this, "提示", "剔除位置设置成功");
}

//剔除队列复位 清空还未发出的剔除信号
void Widget::on_pushButton_10_clicked()
{
    // 清空剔除队列
    while (!removalQueue.empty())
    {
        removalQueue.pop();
    }
    QMessageBox::information(this, "提示", "剔除队列已清空！");
}

/**
 * @brief 重新初始化 myThread
 * @details 完全清理旧的 myThread 并创建新实例，重新连接所有信号槽
 */
void Widget::reinitializeMyThread()
{
    qDebug() << "=== Reinitializing myThread ===";

    // 步骤1: 如果旧线程还存在，先安全清理
    if (myThread) {
        qDebug() << "Cleaning up old myThread...";

        // 停止线程
        if (myThread->isRunning()) {
            myThread->requestStop();
            myThread->stop();

            // 等待线程完全停止
            if (!myThread->wait(2000)) {
                qDebug() << "WARNING: Old myThread did not stop within 2 seconds";
            }
        }

        // 断开所有信号连接
        disconnect(myThread, nullptr, this, nullptr);

        // 删除旧对象
        delete myThread;
        myThread = nullptr;
        qDebug() << "✓ Old myThread cleaned up";
    }

    // 步骤2: 创建新线程
    qDebug() << "Creating new myThread...";
    myThread = new MyThread();

    // 连接信号槽 - 图像显示
    connect(myThread, &MyThread::signal_messImage, this, [this](cv::Mat img) {
        this->slot_displayAndDetect(&img);
    }, Qt::QueuedConnection);
    // 连接信号槽 - 清除标签
    connect(myThread, &MyThread::signal_cleanlabel,
            this, &Widget::slot_clearResultLabel);

    // 🔥 新增：连接框坐标信号（关键！）
    connect(myThread, &MyThread::signal_boxesSelected,
            this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

    // 连接信号槽 - 图像检测（根据检测模式）
    QObject::connect(myThread, &MyThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    });
    QObject::connect(myThread, &MyThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);

    // 步骤6: 连接其他控制信号
    connect(this, &Widget::rotate, myThread, &MyThread::receiveangle);
    connect(this, &Widget::choosechannel,myThread,&MyThread::receivecolorchannel1);
    connect(this, &Widget::sendDataTo, myThread, &MyThread::received);

    // 步骤7: 如果相机已打开，传递相机指针
    if (m_pcMyCamera && m_bOpenDevice) {
        myThread->getCameraPtr(m_pcMyCamera);
        myThread->getImagePtr(myImage);
        qDebug() << "✓ Camera pointers passed to myThread";
    }

    qDebug() << "✓ myThread reinitialized successfully";
}

/**
 * @brief 重新初始化 cameraThread
 * @details 完全清理旧的 cameraThread 并创建新实例，重新连接所有信号槽
 */
void Widget::reinitializeCameraThread()
{
    qDebug() << "=== Reinitializing cameraThread ===";

    // 步骤1: 如果旧线程还存在，先安全清理
    if (cameraThread) {
        qDebug() << "Cleaning up old cameraThread...";

        // 停止线程
        if (cameraThread->isRunning()) {
            cameraThread->requestStop();
            cameraThread->stopTracking();

            // 关闭OpenCV窗口
            try {
                cv::destroyAllWindows();
            } catch (...) {
                qDebug() << "Exception destroying windows";
            }

            // 等待线程完全停止
            if (!cameraThread->wait(3000)) {
                qDebug() << "WARNING: Old cameraThread did not stop within 3 seconds";
            }
        }

        // 断开所有信号连接
        disconnect(cameraThread, nullptr, this, nullptr);

        // 删除旧对象
        delete cameraThread;
        cameraThread = nullptr;
        qDebug() << "✓ Old cameraThread cleaned up";
    }

    // 步骤2: 检查相机是否可用
    if (!m_pcMyCamera) {
        qDebug() << "ERROR: Cannot reinitialize cameraThread - camera is null";
        return;
    }

    // 步骤3: 创建新线程
    qDebug() << "Creating new cameraThread...";
    cameraThread = new CameraThread(this, m_pcMyCamera);

    // 步骤4: 连接信号槽 - 旋转角度 图像颜色通道
    connect(this, &Widget::rotate, cameraThread, &CameraThread::receiveangle1);
    connect(this, &Widget::choosechannel,cameraThread,&CameraThread::receivecolorchannel);

    // 步骤5: 连接信号槽 - 清除标签
    connect(cameraThread, &CameraThread::signal_cleanlabel,
            this, &Widget::slot_clearResultLabel);

    // 步骤6: 连接信号槽 - 图像显示
    connect(cameraThread, &CameraThread::signal_messImage, this, [this](cv::Mat img) {
        this->slot_displayAndDetect(&img);
    }, Qt::QueuedConnection);

    connect(cameraThread, &CameraThread::signal_boxesSelected,
            this, &Widget::slot_saveBoxesFromThread, Qt::QueuedConnection);

    // 步骤7: 连接信号槽 - 图像检测（根据检测模式）
    connect(cameraThread, &CameraThread::signal_sendForDetection, this, [this](cv::Mat img, DetectionPose pose) {
        this->dispatchDetectionByMode(&img, pose);
    }, Qt::QueuedConnection);
    connect(cameraThread, &CameraThread::signal_sendTissueResult, this, [this](cv::Mat img, TissueRollResult result) {
        this->slot_handleTissueResult(&img, result);
    }, Qt::QueuedConnection);

    // 步骤8: 连接模板匹配相关信号
    connect(this, &Widget::jiancestring, templatematch, &TemplateMatch::jianceshibiestr);
    connect(this, &Widget::sendDataTo, cameraThread, &CameraThread::received);
    connect(this, &Widget::imgmuban, templatematch, &TemplateMatch::recemuban);
    connect(this, &Widget::caijianchicun, templatematch, &TemplateMatch::caijiansize);

    qDebug() << "✓ cameraThread reinitialized successfully";
}

/**
 * @brief 确保线程已就绪
 * @details 在启动线程前调用此函数，检查并重新初始化必要的线程
 *          这是防止崩溃的关键函数
 */
void Widget::ensureThreadsReady()
{
    qDebug() << "=== Ensuring threads are ready ===";

    // 检查 myThread
    if (!myThread) {
        qDebug() << "myThread is null, reinitializing...";
        reinitializeMyThread();
    } else if (myThread->isRunning()) {
        qDebug() << "myThread is already running, stopping and reinitializing...";
        reinitializeMyThread();
    } else {
        qDebug() << "✓ myThread is ready";
    }

    // 检查 cameraThread（只在需要时）
    // 注意：cameraThread 通常在 on_plcbtn_clicked 中创建，这里不检查

    // 处理事件队列，确保清理完成
    QCoreApplication::processEvents();

    qDebug() << "✓ Threads readiness check completed";
}

//启动时预加载字符模板图像
void Widget::loadLastTemplateConfig()
{
    if (currentTemplateDirPath.isEmpty()) {
        return; // 无历史路径，直接返回
    }
    qDebug() << "9.1 loadLastTemplateConfig: currentTemplateDirPath 不为空";

    QString newMubiaozifu = ui->dateEdit->toPlainText();
    if (newMubiaozifu.isEmpty()) {
        // 若目标字符为空，清空模板列表
        digitTemplates.clear();
        digitTemplateTargetIndexes.clear();
        return;
    }
    qDebug() << "9.2 loadLastTemplateConfig: newMubiaozifu 不为空";

    QStringList baseNamesToFind = parseWordTemplateBaseNames(newMubiaozifu);
    qDebug() << "9.3 loadLastTemplateConfig: 正则表达式匹配完成";

    std::vector<cv::Mat> tempTemplates;
    std::vector<int> tempTemplateTargetIndexes;
    QString loadError;
    const bool includeVariantTemplates = (ui->comboBox_4->currentIndex() == 1);
    const bool loaded = loadWordDigitTemplatesFromDir(currentTemplateDirPath,
                                                      baseNamesToFind,
                                                      &tempTemplates,
                                                      &tempTemplateTargetIndexes,
                                                      &loadError,
                                                      includeVariantTemplates);
    qDebug() << "9.5 loadLastTemplateConfig: 模板图片读取完成";

    // ================== 修复 4：防死锁隔离保护 ==================
    if (!loaded) {
        digitTemplates.clear();
        digitTemplateTargetIndexes.clear();
        qDebug() << "[ERROR] 产品模板文件夹中的图片缺失或读取失败，已清空模板以保护程序！" << loadError;
    } else {
        digitTemplates = tempTemplates;
        digitTemplateTargetIndexes = tempTemplateTargetIndexes;
        qDebug() << "[INFO] 模板加载成功，目标字符数量:" << baseNamesToFind.size()
                 << "模板图片数量:" << digitTemplates.size();
    }

    try {
        initOverlapDetectorFromCurrentDir();
    } catch (...) {
        qDebug() << "9.X initOverlapDetectorFromCurrentDir 内部崩溃！";
    }
    qDebug() << "9.6 loadLastTemplateConfig: 防重叠模型加载完成";
}


/**
 * @brief 接收线程发射的框坐标信号并保存
 */
void Widget::slot_saveBoxesFromThread(DetectionPose pose)
{
    // 1. 如果目标离开了视野，立刻清空屏幕上的字符框和钢印框，保持画面干净
    if (!pose.valid) {
        g_lastDrawResults.clear();
        g_lastStampPoly.clear();
        g_lastStampIsOverlap = false;
    }
    // 2. 如果目标还在视野中，并且内存里有上一轮识别出的字符框
    else if (g_lastPose.valid && (!g_lastDrawResults.empty() || !g_lastStampPoly.empty())) {

        // 计算两帧之间的物理位移和旋转角度差
        float angleDiff = pose.angleDeg - g_lastPose.angleDeg;
        cv::Point2f oldCenter = g_lastPose.anchorCenter;
        cv::Point2f newCenter = pose.anchorCenter;

        // 让所有字符框跟随产品一起物理移动（AR视觉跟随）
        for (auto& res : g_lastDrawResults) {
            for (auto& pt : res.poly) {
                // 转为相对于旧中心的相对坐标
                cv::Point2f rel(pt.x - oldCenter.x, pt.y - oldCenter.y);
                // 叠加这两帧之间的微小旋转
                cv::Point2f rot = rotateRelativePoint(rel, angleDiff);
                // 叠加上新中心点，得出全新的绝对坐标
                pt = cv::Point(cvRound(rot.x + newCenter.x), cvRound(rot.y + newCenter.y));
            }
        }

        // 让黄/红色的钢印检测框也跟随产品一起移动
        for (auto& pt : g_lastStampPoly) {
            cv::Point2f rel(pt.x - oldCenter.x, pt.y - oldCenter.y);
            cv::Point2f rot = rotateRelativePoint(rel, angleDiff);
            pt = cv::Point(cvRound(rot.x + newCenter.x), cvRound(rot.y + newCenter.y));
        }
    }

    // 最后更新全局位姿
    g_lastPose = pose;
}

//加载UI样式表模板
void Widget::initStyle()
    {
        QFile file(":/qss/1.css");// 淡蓝色风格
        if(file.open(QFile::ReadOnly)){
            QString qss = QLatin1String(file.readAll());

            // 提取主色调用于设置系统调色板
            QString paletteColor = qss.mid(20,7);// 获取QSS中定义的主色
            qApp->setPalette(QPalette(QColor(paletteColor)));

            // 应用样式表（qApp 是全局应用程序对象，作用于所有控件）
            qApp->setStyleSheet(qss);

            file.close();
        }
    }



//设置颜色通道
void Widget::on_pushButton_7_clicked()
{
    int index = ui->comboBox_5->currentIndex();
    switch (index)
    {
    case 1:
         colorchannel= 1;
        break;
    case 2:
        colorchannel = 2;
        break;
    case 3:
        colorchannel = 3;
        break;
    default:
        colorchannel = 0;
    }

    emit choosechannel(colorchannel);
    showParameterInfo("提示", "颜色通道设置成功");

}


void Widget::on_pushButton_11_clicked()
{
    if (ui->comboBox_4->currentIndex() == 1 && m_wordMultiTemplateMode) {
        if (m_wordTemplateProfiles.empty()) {
            QMessageBox::warning(this, "提示", "当前没有加载任何字库多模板！");
            return;
        }

        QMessageBox thresholdMessageBox(this);
        thresholdMessageBox.setIcon(QMessageBox::Question);
        thresholdMessageBox.setWindowTitle("保存当前界面设置");
        thresholdMessageBox.setText("当前处于字库多模板模式。\n保存当前界面设置会将界面上的设置写入所有产品模板文件夹。\n\n是否将当前界面的图像合格阈值覆盖到每一个模板？");
        QPushButton *overwriteThresholdButton = thresholdMessageBox.addButton("覆盖图像阈值", QMessageBox::AcceptRole);
        QPushButton *skipThresholdButton = thresholdMessageBox.addButton("跳过图像阈值", QMessageBox::ActionRole);
        QPushButton *cancelButton = thresholdMessageBox.addButton("取消", QMessageBox::RejectRole);
        thresholdMessageBox.setDefaultButton(skipThresholdButton);
        thresholdMessageBox.exec();

        if (thresholdMessageBox.clickedButton() == cancelButton) {
            return;
        }

        const bool overwriteImageThreshold = (thresholdMessageBox.clickedButton() == overwriteThresholdButton);
        const QString globalImageThresholdText = ui->lineEdit_yuzhi->text().trimmed();
        if (overwriteImageThreshold) {
            bool thresholdOk = false;
            globalImageThresholdText.toDouble(&thresholdOk);
            if (!thresholdOk) {
                QMessageBox::warning(this, "参数错误", "当前界面的图像合格阈值不是有效数字，无法覆盖到所有模板。");
                return;
            }
        }

        const QString oldDateEditText = ui->dateEdit->toPlainText();
        const QString oldImageThresholdText = ui->lineEdit_yuzhi->text();
        const QString oldCurrentTemplateDirPath = currentTemplateDirPath;
        const int oldProfileIndex = currentWordTemplateProfileIndex();
        int savedCount = 0;
        QStringList failedTemplates;

        for (WordTemplateProfile &profile : m_wordTemplateProfiles) {
            QDir dir(profile.dirPath);
            if (profile.dirPath.isEmpty() || !dir.exists()) {
                failedTemplates.append(profile.name.isEmpty() ? profile.dirPath : profile.name);
                continue;
            }

            QString thresholdToSave = profile.imageThresholdText;
            if (overwriteImageThreshold) {
                thresholdToSave = globalImageThresholdText;
                profile.imageThresholdText = globalImageThresholdText;
            } else if (thresholdToSave.trimmed().isEmpty()) {
                thresholdToSave = oldImageThresholdText;
                profile.imageThresholdText = thresholdToSave;
            }

            {
                QSignalBlocker dateBlocker(ui->dateEdit);
                QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
                ui->dateEdit->setPlainText(profile.targetText);
                ui->lineEdit_yuzhi->setText(thresholdToSave);
                currentTemplateDirPath = profile.dirPath;
                saveSettingsToDir(profile.dirPath);
            }
            ++savedCount;
        }

        currentTemplateDirPath = oldCurrentTemplateDirPath;
        if (oldProfileIndex >= 0) {
            setCurrentWordTemplateEditIndex(oldProfileIndex);
        } else {
            QSignalBlocker blocker(ui->dateEdit);
            ui->dateEdit->setPlainText(oldDateEditText);
            QSignalBlocker thresholdBlocker(ui->lineEdit_yuzhi);
            ui->lineEdit_yuzhi->setText(oldImageThresholdText);
        }

        saveSettings();

        if (!failedTemplates.isEmpty()) {
            QMessageBox::warning(this,
                                 "提示",
                                 QString("已将当前界面设置保存到 %1 个字库模板。\n\n以下模板保存失败：\n%2")
                                 .arg(savedCount)
                                 .arg(failedTemplates.join("\n")));
            return;
        }

        QMessageBox::information(this,
                                 "成功",
                                 QString("已将当前界面设置保存到 %1 个字库模板。\n\n目标字符仍按每个模板自己的配置保存。\n图像阈值处理：%2")
                                 .arg(savedCount)
                                 .arg(overwriteImageThreshold ? "已覆盖到所有模板" : "已跳过覆盖，保留每个模板自己的阈值"));
        return;
    }

    // 1. 检查是否已经加载了产品模板文件夹
    if (currentTemplateDirPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前没有加载任何模板！\n请先点击【选择模板】后再尝试保存当前界面设置。");
        return;
    }

    // 2. 检查该文件夹在硬盘上是否仍然存在
    QDir dir(currentTemplateDirPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, "错误", "当前使用的产品模板文件夹不存在或已被删除，无法更新参数！");
        return;
    }

    // 3. 复用保存参数逻辑
    // 此时不会去读取 ImageLabel 上可能新画的框，
    // 内存中的 savedTrackingBox 依然是原模板的坐标。
    // 因此调用此函数会用最新的 UI 参数覆盖 app_settings.appset，但完美保留原始框坐标。
    saveSettingsToDir(currentTemplateDirPath);

    // 4. 同时更新全局配置记录（软件下次启动时的默认参数）
    saveSettings();

    // 5. 如果修改了目标字符，需要触发一次内存模板的重新加载机制
    // 以防止仅仅修改了字库却因为没有重新加载导致无法生效
    loadLastTemplateConfig();

    QMessageBox::information(this, "成功", QString("已成功保存当前界面设置！\n(模板：%1)\n注：原始定位区域与喷码检测区域保持不变。").arg(dir.dirName()));
}


//设置相机增益
void Widget::on_pushButton_12_clicked()
{
    if (m_pcMyCamera == nullptr || m_bOpenDevice == false) {
        showParameterWarning("提示", "相机未初始化或未打开，无法设置增益！");
        return;
    }

    // 首先获取当前相机允许的增益范围
    MVCC_FLOATVALUE stParam = {0};
    int nRet = m_pcMyCamera->GetFloatValue("Gain", &stParam);
    if (nRet != MV_OK) {
        showParameterWarning("提示", QString::fromLocal8Bit("无法获取相机增益支持的范围！错误码：%1").arg(nRet));
        return;
    }

    // 获取lineEdit_14中设置的增益值
    QString gainStr = ui->lineEdit_14->text();
    bool isOk = false;
    float gainValue = gainStr.toFloat(&isOk);

    if (!isOk) {
        showParameterWarning("提示", QString::fromLocal8Bit("请输入有效的增益数字！\n当前相机允许范围：%1 ~ %2").arg(stParam.fMin).arg(stParam.fMax));
        return;
    }

    // 检查输入值是否在支持的范围内
    if (gainValue < stParam.fMin || gainValue > stParam.fMax) {
        showParameterWarning("提示", QString::fromLocal8Bit("输入的增益值超出限制！\n当前相机允许范围：%1 ~ %2").arg(stParam.fMin).arg(stParam.fMax));
        // 可以选择自动规整到最大或最小值
        // gainValue = qBound(stParam.fMin, gainValue, stParam.fMax);
        // ui->lineEdit_14->setText(QString::number(gainValue));
        return;
    }

    // 调用SDK接口设置增益
    nRet = m_pcMyCamera->SetFloatValue("Gain", gainValue);
    if (nRet == MV_OK) {
        qDebug() << "SetGain success:" << gainValue;
        showParameterInfo("提示", "相机增益设置成功！");
    } else {
        qDebug() << "SetGain failed! Ret:" << nRet;
        showParameterWarning("提示", QString::fromLocal8Bit("相机增益设置失败！错误码：%1").arg(nRet));
    }
}

void Widget::on_pushButton_tissueRoughnessThreshold_clicked()
{
    applyTissueRoughnessThresholdFromUi(true);
}


void Widget::on_WriteVDpushButton_clicked()
{

    if (!client->Connected())
    {
        showParameterWarning("警告", "PLC未连接！");
        return;
    }

    uint32_t value = ui->lineEdit_6->text().toUInt();
    byte v_data[4] = {0};

    // 大小端转换
    v_data[3] = (unsigned char)(0xFF & value);
    v_data[2] = (unsigned char)((0xFF00 & value) >> 8);
    v_data[1] = (unsigned char)((0xFF0000 & value) >> 16);
    v_data[0] = (unsigned char)((0xFF000000 & value) >> 24);

    // 写入DB1.924
    int tmp = client->WriteArea(S7AreaDB, 1, 924, 4, S7WLDWord, v_data);
// 判断写入结果
    if (tmp != 0)
    {
        // 写入失败
        showParameterWarning("error", "设置拍照距离失败");
    }
    else
    {
        // 写入成功
        showParameterInfo("提示", "拍照距离设置成功");
    }
}

void Widget::on_confirmAllParamsButton_clicked()
{
    m_confirmAllParamsRunning = true;
    m_confirmAllParamErrors.clear();

    ui->textsure_btn->click();
    ui->WriteVDpushButton->click();
    ui->plcmodebtn->click();
    ui->pushButton_7->click();
    ui->pushButton_3->click();
    ui->pushButton_tissueRoughnessThreshold->click();
    ui->sureButton->click();
    ui->pushButton_12->click();
    ui->pushButton_9->click();
    ui->pushButton_2->click();
    ui->pushButton->click();
    ui->pushButton_8->click();

    m_confirmAllParamsRunning = false;
    m_allParamsConfirmed = true;

    if (m_confirmAllParamErrors.isEmpty()) {
        QMessageBox::information(this, "提示", "所有参数设置成功！");
    } else {
        QMessageBox::warning(this, "设置失败",
                             QString("以下参数设置失败：\n%1").arg(m_confirmAllParamErrors.join("\n")));
    }
}
