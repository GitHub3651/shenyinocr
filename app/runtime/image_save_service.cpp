#include "runtime/image_save_service.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>

#include <exception>

namespace {
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

std::size_t ImageSaveService::workerCount() const
{
    return m_workerCount;
}

std::size_t ImageSaveService::outstandingTaskCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outstandingTaskCount;
}

quint64 ImageSaveService::failedTaskCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_failedTaskCount;
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
                        L"\u65e0\u6cd5\u521b\u5efa\u5b58\u56fe\u76ee\u5f55\uff1a%1")
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
                        L"\u4e0d\u652f\u6301\u7684\u5b58\u56fe\u50cf\u7d20\u683c\u5f0f\uff1a%1")
                    .arg(item.filePath);
        }
        return false;
    }
    if (!image.save(item.filePath, item.format.constData())) {
        if (errorMessage) {
            *errorMessage = QString::fromWCharArray(
                        L"\u56fe\u50cf\u5199\u5165\u5931\u8d25\uff1a%1")
                    .arg(item.filePath);
        }
        return false;
    }
    qDebug() << "[IMAGE_SAVE] saved" << item.filePath;
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
                                L"\u672a\u77e5\u5b58\u56fe\u5931\u8d25")
                            : errorMessage;
                }
            }
        } catch (const std::exception &exception) {
            failed = true;
            latestError = QString::fromWCharArray(
                        L"\u5b58\u56fe\u5f02\u5e38\uff1a%1")
                    .arg(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
            failed = true;
            latestError = QString::fromWCharArray(
                        L"\u5b58\u56fe\u53d1\u751f\u672a\u77e5\u5f02\u5e38");
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
            emit taskFailed(totalFailed, latestError);
        }
    }
}
