#pragma once
// 文件作用：本文件用于计算钢印区域的重叠特征，并给出重叠异常判断。
// 主要职责：计算钢印区域的重叠特征，并给出重叠异常判断。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include <opencv2/opencv.hpp>
#include <vector>

/**
 * @brief 标定数据结构体（纯数据层）
 */
// 组件说明：CalibrationData 保存钢印、日期和二维码区域的标定坐标。
struct CalibrationData {
    // 钢印区域多边形，存储的是相对于“拉环中心”的相对偏移量 (dx, dy)
    std::vector<cv::Point2f> stamp_poly;

    // 生产日期多边形，存储的是相对于“追踪锚点框中心”的相对偏移量 (dx, dy)
    std::vector<cv::Point2f> date_poly;

    // 二维码矩形四角，存储的是相对于“追踪锚点框中心”的相对偏移量
    // 顺序固定为：左上、右上、右下、左下
    std::vector<cv::Point2f> barcode_poly;

};

/**
 * @brief 检测结果报告结构体
 */
// 组件说明：DetectResult 保存重叠检测判定、定位数据和最终多边形。
struct DetectResult {
    bool isOk;               // 最终判定结论：true 为合格(无重叠)，false 为异常(NG，有重叠)
    int overlapPixels;       // 钢印多边形和日期多边形的像素物理重叠数量

    bool foundRing;          // 是否成功找到了拉环
    cv::Point locRing;       // 拉环匹配框的左上角坐标
    double valRing;          // 拉环匹配的置信度分数 (保留最高得分用于诊断)
    int angleRing;           // 最终确定的拉环旋转角度
    cv::Size shapeRing;      // 旋转后的拉环模板的边界尺寸

    std::vector<cv::Point> finalStampPoly; // 经过仿射变换后，映射在当前原图上的【钢印绝对物理坐标多边形】
    std::vector<cv::Point> finalDatePoly;  // 映射在当前原图上的【生产日期绝对物理坐标多边形】
};

/**
 * @brief 核心重叠检测算法类 ("发动机")
 */
// 组件说明：OverlapDetector 负责拉环定位、区域变换和钢印日期重叠判断。
class OverlapDetector {
public:
    OverlapDetector();

    // 运行时只接受 PreparedRecipe 已解码的快照资产。
    bool init(const cv::Mat& ringTemplate,
              const CalibrationData& calibration);

    /**
     * @brief 执行核心视觉检测、匹配及碰撞判断
     * @param bgrImage 输入待检测的三通道彩色原图或灰度图
     * @param datePoly PreparedRecipe 快照中的生产日期多边形
     * @return DetectResult 返回结果包体
     */
    DetectResult processImage(const cv::Mat& bgrImage, const std::vector<cv::Point>& datePoly);

    // 获取生产日期的相对多边形坐标
    std::vector<cv::Point2f> getDatePoly() const { return calibData.date_poly; }

private:
    cv::Mat templateRing;
    CalibrationData calibData;

    // ================= 核心加速数据结构 =================
    // 缓存预计算好的旋转拉环模板及其对应角度（原尺寸，用于精配确认）
    std::vector<cv::Mat> preRotatedRings;
    std::vector<int> preRotatedAngles;

    // 缓存极速金字塔粗配专用模板（大幅降低 CPU 运算量）
    std::vector<cv::Mat> preRotatedRingsSmall;
    double pyramidScale; // 金字塔降采样比例
};
