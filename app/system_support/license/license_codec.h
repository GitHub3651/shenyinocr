// 文件作用：本文件用于读取、解码并验证软件授权信息，为启动流程提供授权判断结果。
// 主要职责：读取、解码并验证软件授权信息，为启动流程提供授权判断结果。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QDate>
#include <QString>

// 组件说明：LicenseFileError 枚举列出该组件允许使用的稳定状态和选项。
enum class LicenseFileError
{
    None,
    InvalidDate,
    FileReadFailed,
    InvalidFormat,
    FileWriteFailed
};

// 组件说明：LicenseReadResult 数据结构保存一次操作的结果、状态和错误信息。
struct LicenseReadResult
{
    LicenseFileError error = LicenseFileError::FileReadFailed;
    QDate expiresDate;

    bool succeeded() const;
};

// 组件说明：LicenseCodec 组件封装本文件中与其名称对应的单一职责。
class LicenseCodec
{
public:
    static LicenseReadResult readFile(const QString &filePath);
    static LicenseFileError writeFile(const QDate &expiresDate,
                                      const QString &filePath);
};
