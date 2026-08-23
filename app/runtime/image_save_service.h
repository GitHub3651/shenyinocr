// 文件作用：本文件用于按保存策略异步写入原图和标注图，并通过有界队列提供写盘反压。
// 主要职责：按保存策略异步写入原图和标注图，并通过有界队列提供写盘反压。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/common/detection_pose.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct ImageSaveItem {
    QImage image;
    std::shared_ptr<const FrameData> frame;
    QString filePath;
    QByteArray format;
    int quality = -1;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return (!image.isNull()
                || (frame && !frame->originalImage.empty()))
                && !filePath.trimmed().isEmpty()
                && !format.trimmed().isEmpty()
                && quality >= -1
                && quality <= 100;
    }
};

struct ImageSaveTask {
    ProductKey productKey;
    std::vector<ImageSaveItem> items;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        if (!productKey.isValid() || items.empty()) {
            return false;
        }
        for (const ImageSaveItem &item : items) {
            if (!item.isValid()) {
                return false;
            }
        }
        return true;
    }
};

enum class ImageSaveSubmitStatus {
    Accepted,
    InvalidTask,
    Stopping
};

struct ImageSaveSubmitResult {
    ImageSaveSubmitStatus status = ImageSaveSubmitStatus::InvalidTask;

    // 函数说明：isAccepted 函数检查相关状态并返回判断结果。
    bool isAccepted() const
    {
        return status == ImageSaveSubmitStatus::Accepted;
    }
};

// 组件说明：ImageSaveService 组件封装对应业务职责和生命周期边界。
class ImageSaveService : public QObject
{
    Q_OBJECT

public:
    using WriteFunction = std::function<bool(
        const ImageSaveItem &item,
        QString *errorMessage)>;

    explicit ImageSaveService(
        std::size_t capacity = 32,
        const WriteFunction &writeFunction = WriteFunction(),
        std::size_t workerCount = 2,
        QObject *parent = nullptr);
    ~ImageSaveService() override;

    ImageSaveSubmitResult submit(const ImageSaveTask &task);
    std::size_t capacity() const;
    std::size_t workerCount() const;
    void shutdown();

signals:
    void taskFailed(quint64 totalFailed, QString latestError);

private:
    static bool writeImage(
        const ImageSaveItem &item,
        QString *errorMessage);
    void workerLoop();

    const std::size_t m_capacity;
    const std::size_t m_workerCount;
    const WriteFunction m_writeFunction;
    mutable std::mutex m_mutex;
    std::condition_variable m_taskAvailable;
    std::condition_variable m_spaceAvailable;
    std::deque<ImageSaveTask> m_tasks;
    std::size_t m_outstandingTaskCount = 0;
    quint64 m_failedTaskCount = 0;
    bool m_stopping = false;
    std::vector<std::thread> m_workers;
};
