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


// Qt核心组件
#include <QTimer>
#include <QThread>
#include <QFileDialog>
#include <QImageReader>
#include <QLabel>
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
#include <QInputDialog>

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

    connect(imageLabel, &ImageLabel::signal_hintMessage, this, [this](QString msg){
            ui->statusLabel->setText(msg);
            // ui->statusLabel->setStyleSheet("QLabel{color:#2ecc71; font-weight:bold;}"); // 可选：加上这行可以让字体变绿色加粗更醒目
        });
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

    if (client) {
            if (client->Connected()) {
                client->Disconnect();
            }
            // 如果 client 是在构造函数 new 出来的，记得 delete 防止内存泄漏
            delete client;
            client = nullptr;
        }

    // 先关闭所有窗口
    try {
        cv::destroyAllWindows();
    } catch (...) {}

    delete ui;
    delete myImage;

    // 关闭相机
    if (m_pcMyCamera)
    {
        m_pcMyCamera->Close();
        delete m_pcMyCamera;
        m_pcMyCamera = NULL;
    }

    // 停止 myThread（不使用terminate）
    if (myThread) {
        if (myThread->isRunning()) {
            myThread->requestStop();
            myThread->stop();
            if (!myThread->wait(2000)) {
                qDebug() << "WARNING: myThread did not stop in destructor";
                // 不调用 terminate，让它自然结束
            }
        }
        delete myThread;
    }

    // 停止 cameraThread（不使用terminate）
    if (cameraThread) {
        if (cameraThread->isRunning()) {
            cameraThread->requestStop();
            if (!cameraThread->wait(2000)) {
                qDebug() << "WARNING: cameraThread did not stop in destructor";
                // 不调用 terminate
            }
        }
        delete cameraThread;
    }

    delete templatematch;

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
    QString curDate = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss.zzz");
    QString saveName = savePath + curDate + "." + format;

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

        // ================== 绘制生产日期多边形 ==================
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
                QString saveDir = selectedDir + "/ng/";
                saveImage2("png", saveDir);
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
                QString saveDir = selectedDir + "/ok/";
                saveImage2("png", saveDir);
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
    if (!removalQueue.empty() && totalImages >= removalQueue.front().second - 1) {
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
    QString judgeResult = (detectNum == targetNum ? "ok" : "no");

    g_lastDrawResults = mapMatchResultsToOriginal(templatematch->lastMatchResults, oriented, image->size());
    g_lastPose = pose;
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = QDateTime::currentMSecsSinceEpoch();

    slot_displayAndDetect(image);

    if (j % x == 0) {
        if (judgeResult == "no") {
            if ((ui->comboBox->currentIndex() == 1) || (ui->comboBox->currentIndex() == 3))
                saveImage2("png", selectedDir + "/ng/");
            ngImages++;
            totalImages++;
            ui->resultlabel->setText(QString("<font size='10' color='red'>错误！</font>"));
            if (wrongindex == 0) wrongremove();
            else removalQueue.push(std::make_pair(totalImages, totalImages + wrongindex));
        } else {
            totalImages++;
            if ((ui->comboBox->currentIndex() == 2) || (ui->comboBox->currentIndex() == 3))
                saveImage2("png", selectedDir + "/ok/");
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
                saveImage2("png", selectedDir + "/ng/");
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
                saveImage2("png", selectedDir + "/ok/");
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

    // 注意：这里我们只拍照显示，不强制运行识别。用户可以在这张图上画框。
    imageLabel->resetDrawingStep();
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
        myThread->stopTracking();
    }

    if (cameraThread) {
        cameraThread->requestStop();
        cameraThread->stopTracking();
    }

    // 🔥 Step 3: myThread - 保持原逻辑
    bool myThreadWasRunning = false;
    if (myThread && myThread->isRunning()) {
        myThreadWasRunning = true;
        myThread->stop();
        if (!myThread->wait(500)) {
            qDebug() << "WARNING: myThread did not stop";
        }
    }

    // 🔥 Step 4: cameraThread - 停止逻辑
    bool needRestartCamera = false;
    if (cameraThread != nullptr) {
        needRestartCamera = true;
        disconnect(cameraThread, nullptr, this, nullptr);
        disconnect(this, nullptr, cameraThread, nullptr);

        cameraThread->requestStop();
        if (!cameraThread->wait(500)) {
            cameraThread->terminate();
            cameraThread->wait();
        }

        cameraThread->deleteLater();
        cameraThread = nullptr;
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

    // Step 7: 清理UI和变量
    detectedRects.clear();
    selectionRect1 = QRect();

    if (imageLabel) {
        imageLabel->clearGreenRects();
        imageLabel->setColor(1);
        imageLabel->clearSelection();
    }

    ui->resultlabel->clear();
    ui->imagenum->clear();
    ui->ngnum->clear();
    ui->resultlabel_7->clear();
    ui->speedLabel->clear();
    ui->lineBoxIndex_6->clear();

    g_lastDrawResults.clear();
    g_lastPose = DetectionPose();
    g_lastStampPoly.clear();
    g_lastStampIsOverlap = false;
    g_lastDetectTime = 0;

    ngImages = 0;
    totalImages = 0;
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
    if((ui->comboBox_4->currentIndex() == 0)||(ui->comboBox_4->currentIndex() == 1))
    {
        // 1. 检查是否存在有效的模板路径
        if (currentTemplateDirPath.isEmpty()) {
            showParameterInfoAsError("提示", "请先选择模板文件夹");
            return;
        }
        // 2. 读取当前修改后的目标字符
        QString newMubiaozifu = ui->dateEdit->toPlainText();
        if (newMubiaozifu.isEmpty()) {
            digitTemplates.clear();
            showParameterInfoAsError("提示", "目标字符为空，已清空模板");
            return;
        }

        // ================== 修复 1：升级正则表达式，加入中文支持 ==================
        QStringList baseNamesToFind;
        // 升级正则表达式：允许 [数字、字母、中文] 后面跟带括号的数字作为一个整体
        QRegularExpression regex(R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
        QRegularExpressionMatchIterator matchIt = regex.globalMatch(newMubiaozifu);

        while (matchIt.hasNext()) {
            QRegularExpressionMatch match = matchIt.next();
            QString unit;
            if (!match.captured(1).isEmpty()) unit = match.captured(1);
            else if (!match.captured(2).isEmpty()) unit = match.captured(2);
            else if (!match.captured(3).isEmpty()) unit = match.captured(3);
            else if (!match.captured(4).isEmpty()) unit = match.captured(4); // 提取到中文字符

            baseNamesToFind.append(unit.toLower());
        }

        // ================== 修复 2：无视后缀名，建立基础名映射 ==================
        QDir directory(currentTemplateDirPath);
        QMap<QString, QString> filePathMap;
        static const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tiff"};

        QFileInfoList fileList = directory.entryInfoList(
                    filters,
                    QDir::Files | QDir::NoDotAndDotDot);

        for (const QFileInfo &fileInfo : fileList) {
            QString baseName = fileInfo.completeBaseName().toLower();
            if (!filePathMap.contains(baseName)) {
                filePathMap.insert(baseName, fileInfo.absoluteFilePath());
            }
        }

        // ================== 修复 3：使用内存流解码解决中文路径 BUG ==================
        std::vector<cv::Mat> tempTemplates;
        bool hasMissing = false;
        QString missingNames;

        for (const QString &searchKey : baseNamesToFind) {
            if (filePathMap.contains(searchKey)) {
                QFile file(filePathMap[searchKey]);
                if (file.open(QIODevice::ReadOnly)) {
                    QByteArray data = file.readAll();
                    std::vector<uchar> buf(data.begin(), data.end());
                    cv::Mat templateImg = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);

                    if (templateImg.empty()) {
                        hasMissing = true;
                        missingNames += searchKey + "(读取损坏) ";
                    } else {
                        tempTemplates.push_back(templateImg);
                    }
                } else {
                    hasMissing = true;
                    missingNames += searchKey + "(无法打开) ";
                }
            } else {
                hasMissing = true;
                missingNames += searchKey + " ";
            }
        }

        // ================== 修复 4：友好的报警和隔离机制 ==================
        if (hasMissing) {
            // 如果有任何图片读取失败或丢失，绝不更新到全局的 digitTemplates，同时给出严厉警告
            showParameterCritical("严重警告",
                QString("以下字符未在文件夹中找到对应图片，或图片读取失败：\n[ %1 ]\n\n请检查模板文件夹内的图片是否存在或是否损坏（支持中文，无需关心后缀和大小写）！\n本次更新已撤销。").arg(missingNames));
            return;
        }

        // 5. 全部成功后，再更新到全局容器
        digitTemplates = tempTemplates;
        showParameterInfo("提示", QString("目标字符确认成功，共加载 %1 个模板！").arg(digitTemplates.size()));
    }
    else{
     showParameterInfo("提示", "目标字符确认成功");
    }


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
        QMessageBox::warning(this, "提示", "请先拍照获取图像！");
        return;
    }

    bool ok;
    QString newFolderName = QInputDialog::getText(this, "保存模板", "请输入文件夹名称：", QLineEdit::Normal, "", &ok);
    if (!ok || newFolderName.isEmpty()) return;

    QString savePath = QDir("D:/muban/").absoluteFilePath(newFolderName);
    QDir dir(savePath);
    if (!dir.mkpath(".")) return;

    // 1. 获取标定数据
    QRect uiTrackRect = imageLabel->getTrackingRect();
    QPolygon uiDetectPoly = imageLabel->getDetectionPoly();

    if (uiTrackRect.isNull() || uiDetectPoly.isEmpty() || uiDetectPoly.size() < 3) {
        QMessageBox::warning(this, "警告", "请在图上画好【追踪锚点】并闭合【生产日期多边形】！");
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
    m_currentTemplateNameVisible = true;
    updateCurrentTemplateName();
    QMessageBox::information(this, "成功", "模板及双框配置已全部保存！");
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
    settings.setValue("comboBox_2_value", ui->comboBox_2->currentText());
    settings.setValue("comboBox_3_value", ui->comboBox_3->currentText());
    settings.setValue("comboBox_4_value", ui->comboBox_4->currentText());
    // 新增：保存模板路径
    settings.setValue("saveDirPath", selectedDir);
    settings.setValue("TemplateDirPath", currentTemplateDirPath);
    settings.setValue("lineEdit_value", ui->lineEdit->text()); // 保存IP地址


    // 🔥 新增：保存框坐标
    if (hasValidBoxes) {
        settings.setValue("trackingBox_x", savedTrackingBox.x);
        settings.setValue("trackingBox_y", savedTrackingBox.y);
        settings.setValue("trackingBox_width", savedTrackingBox.width);
        settings.setValue("trackingBox_height", savedTrackingBox.height);

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
// 功能：从用户输入解析模板文件名，选择模板文件夹
void Widget::on_pushButton_4_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(nullptr, "选择模板文件夹",
                                                        "D:/muban",
                                                        QFileDialog::ShowDirsOnly);
    if (dirPath.isEmpty()) return;

    currentTemplateDirPath = dirPath;

    saveSettings(); // 保存路径
    m_currentTemplateNameVisible = loadSettingsFromDir(dirPath);
    updateCurrentTemplateName();
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
            QMessageBox::warning(this, "提示", "模板加载失败，请检查模板文件夹");
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
void Widget::on_pushButton_6_clicked()
{
    selectedDir = QFileDialog::getExistingDirectory(
                this,
                "选择目标文件夹",
                "C:/",
                QFileDialog::ShowDirsOnly);
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

    // 🔥 加载双框坐标
    if (settings.contains("hasValidBoxes") && settings.value("hasValidBoxes").toBool()) {
        savedTrackingBox.x = settings.value("trackingBox_x", 0).toDouble();
        savedTrackingBox.y = settings.value("trackingBox_y", 0).toDouble();
        savedTrackingBox.width = settings.value("trackingBox_width", 0).toDouble();
        savedTrackingBox.height = settings.value("trackingBox_height", 0).toDouble();

        hasValidBoxes = true;
        qDebug() << "box load success";
    } else {
        hasValidBoxes = false;
        qDebug() << "no usesful box";
    }

    // 🔥 新增：加载局部静态追踪模板 (Anchor Template)
    QString tplPath = dirPath + "/tracking_template.bmp";
    m_loadedTrackingTemplate = cv::imread(tplPath.toLocal8Bit().toStdString(), cv::IMREAD_COLOR);
    if (!m_loadedTrackingTemplate.empty()) {
        qDebug() << "成功加载锚点追踪模板图片：" << tplPath;
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

    if (settings.contains("saveDirPath")) {
        selectedDir = settings.value("saveDirPath").toString();
    }
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
    settings.setValue("comboBox_2_value", ui->comboBox_2->currentText());
    settings.setValue("comboBox_3_value", ui->comboBox_3->currentText());
    settings.setValue("comboBox_4_value", ui->comboBox_4->currentText());
    settings.setValue("comboBox_5_value", ui->comboBox_5->currentText());
    // 新增：保存模板路径
    settings.setValue("saveDirPath", selectedDir);
    settings.setValue("TemplateDirPath", currentTemplateDirPath);
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
    ui->comboBox_4->setCurrentText("字库匹配");
    ui->comboBox_2->setCurrentText("无旋转");
    ui->comboBox_3->setCurrentText("间歇触发模式");
    ui->checkBox->setChecked(true);
    applyTissueRoughnessThresholdFromUi(false);
}

// ================= 拦截滚轮误操作事件 =================
bool Widget::eventFilter(QObject *watched, QEvent *event)
{
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
    const bool isTissueMode = (ui->comboBox_4->currentIndex() == 3);
    const QString runningTemplateName = isTissueMode
            ? QString("无")
            : QDir(currentTemplateDirPath).dirName();

    // 🔥 核心修改：不再从界面动态抓取框，而是严格要求有预载的模板
    if (!isTissueMode && (!hasValidBoxes || m_loadedTrackingTemplate.empty())) {
        QMessageBox::warning(this, "操作规范", "缺乏追踪模板，无法启动！\n\n1. 如果是新产品：请先【拍照】，画好双框并点击【保存模板】\n2. 如果是换线复用：请先点击【加载模板】");
        return;
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
            QMessageBox::information(this, "提示", "已在采集中，若要停止请点击【取消识别】按钮");
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
        return;
    }
    qDebug() << "9.2 loadLastTemplateConfig: newMubiaozifu 不为空";

    // ================== 修复 1：升级正则表达式，加入中文支持 ==================
    QStringList baseNamesToFind;
    try {
        QRegularExpression regex(R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
        QRegularExpressionMatchIterator matchIt = regex.globalMatch(newMubiaozifu);

        while (matchIt.hasNext()) {
            QRegularExpressionMatch match = matchIt.next();
            QString unit;
            if (!match.captured(1).isEmpty()) unit = match.captured(1);
            else if (!match.captured(2).isEmpty()) unit = match.captured(2);
            else if (!match.captured(3).isEmpty()) unit = match.captured(3);
            else if (!match.captured(4).isEmpty()) unit = match.captured(4); // 提取到中文字符

            // 统一转为小写以实现不区分大小写的匹配（对中文无影响）
            baseNamesToFind.append(unit.toLower());
        }
    } catch (...) {
        qDebug() << "9.X 正则表达式执行异常崩溃！";
        return;
    }
    qDebug() << "9.3 loadLastTemplateConfig: 正则表达式匹配完成";

    // ================== 修复 2：无视后缀名，建立基础名映射 ==================
    QDir directory(currentTemplateDirPath);
    QMap<QString, QString> filePathMap;
    static const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tiff"};

    QFileInfoList fileList = directory.entryInfoList(
                filters,
                QDir::Files | QDir::NoDotAndDotDot);

    for (const QFileInfo &fileInfo : fileList) {
        // completeBaseName 剥离后缀，如 "A.png" -> "a" 或 "生.jpg" -> "生"
        QString baseName = fileInfo.completeBaseName().toLower();
        if (!filePathMap.contains(baseName)) {
            filePathMap.insert(baseName, fileInfo.absoluteFilePath());
        }
    }
    qDebug() << "9.4 loadLastTemplateConfig: 文件列表读取完成";

    // ================== 修复 3：使用内存流解码解决中文路径 BUG ==================
    std::vector<cv::Mat> tempTemplates;
    bool hasMissing = false;

    for (const QString &searchKey : baseNamesToFind) {
        if (filePathMap.contains(searchKey)) {
            // 严禁使用 cv::imread 读取中文路径，改用 QFile 读成 byte 后再用 OpenCV 解码
            QFile file(filePathMap[searchKey]);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                try {
                    std::vector<uchar> buf(data.begin(), data.end());
                    cv::Mat templateImg = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);

                    if (templateImg.empty()) {
                        hasMissing = true; // 图像损坏解码失败
                    } else {
                        tempTemplates.push_back(templateImg);
                    }
                } catch (...) {
                    qDebug() << "9.X OpenCV imdecode 异常崩溃！";
                }
            } else {
                hasMissing = true; // 文件无法打开
            }
        } else {
            hasMissing = true; // 文件夹里压根没这张图
        }
    }
    qDebug() << "9.5 loadLastTemplateConfig: 模板图片读取完成";

    // ================== 修复 4：防死锁隔离保护 ==================
    if (hasMissing) {
        digitTemplates.clear();
        qDebug() << "[ERROR] 模板文件夹中的图片缺失或读取失败，已清空模板以保护程序！";
    } else {
        digitTemplates = tempTemplates;
        qDebug() << "[INFO] 模板加载成功，数量: " << digitTemplates.size();
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
    // 1. 检查是否已经加载了模板文件夹
    if (currentTemplateDirPath.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前没有加载任何模板！\n请先点击【加载模板】后再尝试更新参数。");
        return;
    }

    // 2. 检查该文件夹在硬盘上是否仍然存在
    QDir dir(currentTemplateDirPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, "错误", "当前使用的模板文件夹不存在或已被删除，无法更新参数！");
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

    QMessageBox::information(this, "成功", QString("已成功更新当前模板的参数配置！\n(模板：%1)\n注：原始追踪框与识别框坐标保持不变。").arg(dir.dirName()));
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
