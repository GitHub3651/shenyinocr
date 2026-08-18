// 文件作用：本文件用于安装Windows异常处理入口并在崩溃时触发诊断信息保存。
// 主要职责：安装Windows异常处理入口并在崩溃时触发诊断信息保存。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

// 组件说明：WindowsCrashHandler 组件封装本文件中与其名称对应的单一职责。
class WindowsCrashHandler
{
public:
    static void install();
};
