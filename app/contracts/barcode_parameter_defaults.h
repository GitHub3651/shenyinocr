// 文件作用：本文件用于集中保存二维码检测参数的唯一默认值和合法范围。
// 主要职责：集中保存二维码检测参数的唯一默认值和合法范围。
// 模块位置：合同层；负责稳定枚举、默认值和跨模块轻量数据，不承载运行副作用。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

namespace BarcodeParameterDefaults {

// 组件说明：以下常量集中定义二维码检测的默认格式、ROI、超时和回退策略。
static const unsigned int FormatMask = 1u;
static const int RoiPaddingPercent = 8;
static const int MaxDecodeTimeMs = 60;
static const bool EnableFallback = true;

} // namespace BarcodeParameterDefaults
