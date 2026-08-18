// 文件作用：本文件用于定义应用层统一操作结果和错误信息，供界面显示结构化执行状态。
// 主要职责：定义应用层统一操作结果和错误信息，供界面显示结构化执行状态。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>

// 组件说明：ApplicationError 数据结构集中保存该流程需要的一组相关数据。
struct ApplicationError
{
    QString code;
    QString userMessage;
    QString diagnostic;

    // 函数说明：isEmpty 函数检查相关状态并返回判断结果。
    bool isEmpty() const
    {
        return code.isEmpty();
    }
};

// 组件说明：OperationResult 数据结构保存一次操作的结果、状态和错误信息。
struct OperationResult
{
    bool success = false;
    ApplicationError error;

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return success;
    }

    // 函数说明：accepted 函数实现名称所表示的处理步骤。
    static OperationResult accepted()
    {
        OperationResult result;
        result.success = true;
        return result;
    }

    // 函数说明：rejected 函数实现名称所表示的处理步骤。
    static OperationResult rejected(
        const QString &code,
        const QString &userMessage,
        const QString &diagnostic = QString())
    {
        OperationResult result;
        result.error.code = code;
        result.error.userMessage = userMessage;
        result.error.diagnostic = diagnostic;
        return result;
    }
};
