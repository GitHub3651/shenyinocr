// CameraThread.h
// 相机线程类 - 负责硬件触发模式下的图像采集和目标跟踪

#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H

#include <QThread>
#include <QObject>
#include <QDebug>
#include <QImage>
#include <atomic>
#include <memory>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/feature.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include "cmvcamera.h"
#include "Zhuizong.h"
#include "TrackingPoseMatcher.h"
#include "TrackingTypes.h"
#include "TissueRollDetector.h"

using namespace cv;

/**
 * @brief 相机线程类
 * 功能：硬件触发模式下的图像采集、目标跟踪和检测
 *
 * 简化版本：只支持1个检测框 + 1个追踪框
 *
 * 主要特性：
 * - 支持硬件触发相机采集
 * - 支持图像旋转（0°/90°/180°/270°）
 * - 使用MOSSE算法进行目标跟踪
 * - 1个检测框用于目标检测
 * - 1个追踪框用于跟踪运动
 * - 可配置的检测延时
 */
class CameraThread : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     * @param camera 相机对象指针
     */
    explicit CameraThread(QObject *parent = nullptr, CMvCamera *camera = nullptr);

    /**
     * @brief 析构函数
     */
    ~CameraThread();

    /**
     * @brief 停止跟踪并重置所有状态
     */
    void stopTracking();

    /**
     * @brief 强制停止线程（超时则终止）
     */
    void forceStop();

    /**
     * @brief 请求停止线程（正常停止）
     */
    void requestStop();

    // 🔥 新增：设置预设框
    void setPresetBoxes(const std::vector<cv::Point2f>& datePoly, const cv::Rect2d& trackBox);

    // 🔥 新增：清除预设框
    void clearPresetBoxes();

    // 纸巾检测使用整图，不需要追踪模板和定位姿态。
    void setBypassTracking(bool enabled);

    void setWordTemplateTrackingProfiles(const std::vector<WordTrackingProfile>& profiles);
    void clearWordTemplateTrackingProfiles();

    // 🔥 新增：接收从硬盘加载的静态完美模板
    void setPreloadedTemplate(const cv::Mat& tpl) {
        clearWordTemplateTrackingProfiles();
        if (!tpl.empty()) {
            m_trackingTemplate = tpl.clone();
            tracking = m_poseMatcher.init(m_trackingTemplate);
        }
    }

    // ========== 公共配置变量 ==========
    QString receivedata;          ///< 检测延时时间（毫秒）
    QString choicedata = "MOSSE"; ///< 追踪算法名称（当前固定为MOSSE）

public slots:
    /**
     * @brief 接收延时设置
     * @param data 延时时间（毫秒，字符串格式）
     */
    void received(QString data);

    /**
     * @brief 接收图像旋转角度设置
     * @param a 旋转角度 (0:无旋转, 1:顺时针90°, 2:逆时针90°, 3:180°)
     */
    void receiveangle1(int a);

    /**
     * @brief 接收图像颜色通道设置
     * @param a 颜色通道 (0:彩色通道, 1:彩色通道, 2:彩色通道, 3:彩色通道)
     */
    void receivecolorchannel(int c);

protected:
    /**
     * @brief 线程主运行函数
     * 循环获取图像、更新跟踪、发送检测信号
     */
    void run() override;

    /**
     * @brief 检查硬件触发上升沿
     * @return true 检测到上升沿，false 未检测到
     */
    bool CheckRisingEdge();

signals:
    /**
     * @brief 图像准备好的信号（传递图像指针）
     * @param image 图像Mat指针
     */
    void imageReady(Mat *image);

    /**
     * @brief 线程结束信号
     */
    void threadFinished();

    /**
     * @brief 发送图像用于显示
     * @param image 图像Mat指针
     */
    void signal_messImage(cv::Mat image);

    /**
     * @brief 发送检测框进行检测
     * @param image 图像Mat指针
     * @param pose 当前检测姿态
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

private:
    // ========== 相机相关 ==========
    CMvCamera *m_pcMyCamera;  ///< 相机对象指针

    // ========== 线程控制 ==========
    bool m_running;                          ///< 线程运行标志
    std::atomic<bool> m_stopRequested{false}; ///< 停止请求标志（原子变量，线程安全）

    // ========== 跟踪相关 ==========
    bool tracking = false;               ///< 是否正在跟踪（默认false）
    std::unique_ptr<Zhuizong> zhuizong;  ///< 跟踪辅助工具对象
    std::vector<cv::Scalar> colors;      ///< 框的颜色列表
    cv::Mat m_trackingTemplate;
    TrackingPoseMatcher m_poseMatcher;

    struct WordTrackingState {
        QString name;
        int profileIndex = -1;
        std::vector<cv::Point2f> datePoly;
        TrackingPoseMatcher matcher;
        bool ready = false;
    };
    std::vector<WordTrackingState> m_wordTrackingProfiles;
    bool m_wordMultiTemplateMode = false;

    // ========== 配置参数 ==========
    int angle2 = 0;  ///< 图像旋转角度 (默认0，不旋转)
    int colorc = 0;  ///< 颜色通道 （默认0，彩色）

    // ========== 时间相关 ==========
    std::chrono::steady_clock::time_point lastDetectionTime;  ///< 上次检测时间点

    // 🔥 新增：预设框相关成员
    std::vector<cv::Point2f> presetDatePoly;   // 预设的生产日期相对多边形
    cv::Rect2d presetTrackingBox;    // 预设的跟踪框
    bool usePresetBoxes;              // 是否使用预设框
    bool bypassTracking;              // 是否跳过追踪，直接发送整图检测
};

#endif // CAMERATHREAD_H
