// 文件作用：本文件用于按保存策略异步写入原图和标注图，并通过有界队列提供写盘反压。
// 主要职责：按保存策略异步写入原图和标注图，并通过有界队列提供写盘反压。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/image_save_service.h"

#include "system_support/logging/log_categories.h"

#include <QDir>
#include <QFileInfo>

#include <exception>

namespace {

constexpr int kImageJpegQuality = 80;

// 函数说明：frameImage 函数实现名称所表示的处理步骤。
QImage frameImage(const cv::Mat &image)
{
    if (image.type() == CV_8UC1) {
        return QImage(
                    image.data,
                    image.cols,
                    image.rows,
                    static_cast<int>(image.step),
                    QImage::Format_Grayscale8)
                .copy();
    }
    if (image.type() == CV_8UC3) {
        return QImage(
                    image.data,
                    image.cols,
                    image.rows,
                    static_cast<int>(image.step),
                    QImage::Format_RGB888)
                .rgbSwapped();
    }
    if (image.type() == CV_8UC4) {
        return QImage(
                    image.data,
                    image.cols,
                    image.rows,
                    static_cast<int>(image.step),
                    QImage::Format_ARGB32)
                .copy();
    }
    return QImage();
}
}

// 函数说明：ImageSaveService 构造函数创建组件并初始化其依赖和初始状态。
ImageSaveService::ImageSaveService(
    std::size_t capacity,
    const WriteFunction &writeFunction,
    std::size_t workerCount,
    QObject *parent)
    : QObject(parent),
      m_capacity(capacity > 0 ? capacity : 1),
      m_workerCount(workerCount > 0 ? workerCount : 1),
      m_writeFunction(writeFunction ? writeFunction : &ImageSaveService::writeImage)
{
    m_workers.reserve(m_workerCount);
    try {
        for (std::size_t index = 0; index < m_workerCount; ++index) {
            m_workers.push_back(std::thread(
                                    &ImageSaveService::workerLoop,
                                    this));
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_taskAvailable.notify_all();
        for (std::thread &worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

// 函数说明：~ImageSaveService 析构函数按生命周期要求释放组件持有的资源。
ImageSaveService::~ImageSaveService()
{
    shutdown();
}

// 函数说明：submit 函数执行对应事件或业务处理。
ImageSaveSubmitResult ImageSaveService::submit(const ImageSaveTask &task)
{
    ImageSaveSubmitResult result;
    if (!task.isValid()) {
        result.status = ImageSaveSubmitStatus::InvalidTask;
        return result;
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_spaceAvailable.wait(lock, [this]() {
            return m_stopping
                    || m_outstandingTaskCount < m_capacity;
        });
        if (m_stopping) {
            result.status = ImageSaveSubmitStatus::Stopping;
            return result;
        }
        m_tasks.push_back(task);
        ++m_outstandingTaskCount;
        result.status = ImageSaveSubmitStatus::Accepted;
    }

    m_taskAvailable.notify_one();
    return result;
}

// 函数说明：capacity 函数实现名称所表示的处理步骤。
std::size_t ImageSaveService::capacity() const
{
    return m_capacity;
}

// 函数说明：workerCount 函数实现名称所表示的处理步骤。
std::size_t ImageSaveService::workerCount() const
{
    return m_workerCount;
}

// 函数说明：shutdown 函数实现名称所表示的处理步骤。
void ImageSaveService::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) {
            // A previous shutdown already owns the join below or completed it.
        } else {
            m_stopping = true;
        }
    }
    m_taskAvailable.notify_all();
    m_spaceAvailable.notify_all();
    for (std::thread &worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// 函数说明：writeImage 函数保存或发布对应的数据和资源。
bool ImageSaveService::writeImage(
    const ImageSaveItem &item,
    QString *errorMessage)
{
    const QFileInfo fileInfo(item.filePath);
    QDir directory;
    if (!directory.mkpath(fileInfo.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(
                        L"无法创建存图目录：%1")
                    .arg(fileInfo.absolutePath());
        }
        return false;
    }

    const QImage image = !item.image.isNull()
            ? item.image
            : frameImage(item.frame->originalImage);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(
                        L"不支持的存图像素格式：%1")
                    .arg(item.filePath);
        }
        return false;
    }
    if (!image.save(
            item.filePath,
            item.format.constData(),
            kImageJpegQuality)) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(
                        L"图像写入失败：%1")
                    .arg(item.filePath);
        }
        return false;
    }
    qCInfo(logRuntime).noquote()
            << QStringLiteral(
                "event=image_save.completed path=%1 format=%2 bytes=%3")
               .arg(QDir::toNativeSeparators(item.filePath))
               .arg(QString::fromLatin1(item.format))
               .arg(QFileInfo(item.filePath).size());
    return true;
}

// 函数说明：workerLoop 函数实现名称所表示的处理步骤。
void ImageSaveService::workerLoop()
{
    for (;;) {
        ImageSaveTask task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_taskAvailable.wait(lock, [this]() {
                return m_stopping || !m_tasks.empty();
            });
            if (m_tasks.empty()) {
                if (m_stopping) {
                    return;
                }
                continue;
            }
            task = m_tasks.front();
            m_tasks.pop_front();
        }

        bool failed = false;
        QString latestError;
        try {
            for (const ImageSaveItem &item : task.items) {
                QString errorMessage;
                if (!m_writeFunction(item, &errorMessage)) {
                    failed = true;
                    latestError = errorMessage.trimmed().isEmpty()
                            ? QString::fromWCharArray(
                                L"未知存图失败")
                            : errorMessage;
                }
            }
        } catch (const std::exception &exception) {
            failed = true;
            latestError = QString::fromWCharArray(
                        L"存图异常：%1")
                    .arg(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
            failed = true;
            latestError = QString::fromWCharArray(
                        L"存图发生未知异常");
        }

        quint64 totalFailed = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_outstandingTaskCount > 0) {
                --m_outstandingTaskCount;
            }
            if (failed) {
                totalFailed = ++m_failedTaskCount;
            }
        }
        m_spaceAvailable.notify_one();
        if (failed) {
            if (totalFailed == 1) {
                qCCritical(logRuntime).noquote()
                        << QStringLiteral(
                            "event=image_save.failed totalFailed=%1 reason=%2")
                           .arg(totalFailed)
                           .arg(latestError);
            }
            emit taskFailed(totalFailed, latestError);
        }
    }
}
