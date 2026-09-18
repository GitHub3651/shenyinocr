#include "runtime/image_save_service.h"

#include "system_support/logging/log_categories.h"

#include <QDir>
#include <QFileInfo>

#include <exception>

namespace {

constexpr int kImageJpegQuality = 80;

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

ImageSaveService::ImageSaveService(
    std::size_t capacity,
    const WriteFunction &writeFunction,
    std::size_t workerCount)
    : m_capacity(capacity > 0 ? capacity : 1),
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

ImageSaveService::~ImageSaveService()
{
    shutdown();
}

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

std::size_t ImageSaveService::capacity() const
{
    return m_capacity;
}

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
        }
    }
}
