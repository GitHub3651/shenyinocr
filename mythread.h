#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include "cmvcamera.h"
#include "Zhuizong.h"
#include "TrackingPoseMatcher.h"
#include "TrackingTypes.h"
#include "TissueRollDetector.h"

using namespace cv;
using namespace std;

/**
 * @brief MyThread 工作线程类
 * @details 负责软触发模式下的图像采集、目标跟踪和检测
 */
class MyThread : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit MyThread(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MyThread();

    /**
     * @brief 停止相机并关闭窗口
     */
    void stop();

    /**
     * @brief 请求停止线程（线程安全）
     */
    void requestStop();

    /**
     * @brief 开始跟踪（外部调用）
     */
    void startTracking();

    /**
     * @brief 停止跟踪
     */
    void stopTracking();

    /**
     * @brief 获取相机指针
     * @param camera 相机对象指针
     */
    void getCameraPtr(CMvCamera *camera);

    /**
     * @brief 获取图像指针
     * @param image 图像Mat指针
     */
    void getImagePtr(cv::Mat *image);

    /**
     * @brief 检查上升沿（用于硬件触发）
     * @return true 检测到上升沿，false 未检测到
     */
    bool CheckRisingEdge();

    // 🔥 新增：设置预设框和多边形
    void setPresetBoxes(const std::vector<cv::Point2f>& datePoly, const cv::Rect2d& trackBox);

    // 🔥 新增：清除预设框
    void clearPresetBoxes();

    // 纸巾检测使用整图，不需要追踪模板和定位姿态。
    void setBypassTracking(bool enabled);

    // 🔥 新增：接收从硬盘加载的静态完美模板
    void setPreloadedTemplate(const cv::Mat& tpl) {
        if (!tpl.empty()) {
            m_trackingTemplate = tpl.clone();
            m_tracking.store(m_poseMatcher.init(m_trackingTemplate));
        }
    }

signals:
    /**
     * @brief 发送图像信号（用于显示）
     * @param image 图像指针
     */
    void signal_messImage(cv::Mat image);

    /**
     * @brief 发送检测信号
     * @param image 图像指针
     * @param pose 检测姿态
     */
    void signal_sendForDetection(cv::Mat image, DetectionPose pose);

    /**
     * @brief 发送纸巾检测结果
     * @param image 已检测的整帧图像
     * @param result 纸巾检测结果
     */
    void signal_sendTissueResult(cv::Mat image, TissueRollResult result);

    /**
     * @brief 清除标签信号
     */
    void signal_cleanlabel();

    void signal_boxesSelected(DetectionPose pose);

public slots:
    /**
     * @brief 接收延时设置
     * @param data 延时时间（毫秒）
     */
    void received(QString data);

    /**
     * @brief 接收图像旋转角度设置
     * @param a 旋转角度标志 (0:无旋转, 1:顺时针90°, 2:逆时针90°, 3:180°)
     */
    void receiveangle(int a);
    /**
     * @brief 接收图像颜色通道设置
     * @param a 颜色通道 (0:彩色通道, 1:红色通道, 2:绿色通道, 3:蓝色通道)
     */
    void receivecolorchannel1(int c);

protected:
    /**
     * @brief 线程主运行函数
     */
    void run() override;

private:
    // Qt相关
    QImage *myImage;                    // QImage图像对象

    // OpenCV相关
    Zhuizong *zhuizong;                 // 追踪辅助类
    cv::Mat m_trackingTemplate;
    TrackingPoseMatcher m_poseMatcher;

    // 相机相关
    CMvCamera *cameraPtr;               // 相机指针
    cv::Mat *imagePtr;                  // 图像Mat指针

    // 配置参数
    int angle1;                         // 图像旋转角度 (0-3)
    int colorc1;                        // 图像颜色通道
    QString receivedata = "300";        // 检测间隔时间（毫秒）

    // 线程控制（线程安全）
    std::atomic<bool> m_stopRequested;  // 停止请求标志
    std::atomic<bool> m_tracking;       // 跟踪状态标志

    // 时间控制
    std::chrono::steady_clock::time_point lastDetectionTime;  // 上次检测时间

    // 🔥 新增：预设框相关成员
    std::vector<cv::Point2f> presetDatePoly;   // 预设的生产日期多边形相对坐标
    cv::Rect2d presetTrackingBox;    // 预设的跟踪框
    bool usePresetBoxes;              // 是否使用预设框
    bool bypassTracking;              // 是否跳过追踪，直接发送整图检测
};

#endif // MYTHREAD_H



