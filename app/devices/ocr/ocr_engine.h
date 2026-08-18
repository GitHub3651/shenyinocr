// 文件作用：本文件用于定义OCR引擎端口和统一文字识别结果，隔离具体模型实现。
// 主要职责：定义OCR引擎端口和统一文字识别结果，隔离具体模型实现。
// 模块位置：设备层；通过统一端口隔离相机、PLC、OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DEVICES_OCR_OCR_ENGINE_H
#define DEVICES_OCR_OCR_ENGINE_H

#include <opencv2/core.hpp>

#include <string>
#include <vector>

// 组件说明：IOcrEngine 组件提供对应设备或检测能力的统一实现。
class IOcrEngine
{
public:
    virtual ~IOcrEngine() = default;

    virtual std::vector<std::string> recognize(cv::Mat &image) = 0;
};

#endif // DEVICES_OCR_OCR_ENGINE_H
