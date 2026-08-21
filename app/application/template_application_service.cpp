// 文件作用：实现模板编辑草稿和 TemplateStore 之间的应用用例。
#include "application/template_application_service.h"

#include "engines/barcode/barcode_decoder.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString barcodeFailureMessage(const BarcodeReadResult &result)
{
    switch (result.status) {
    case BarcodeReadStatus::DecoderUnavailable:
        return result.errorReason.trimmed().isEmpty()
                ? QStringLiteral("BarcodeDecoder.dll 不可用，无法验证二维码。")
                : QStringLiteral("BarcodeDecoder.dll 不可用：%1")
                  .arg(result.errorReason);
    case BarcodeReadStatus::InvalidRoi:
        return QStringLiteral("二维码框选区域无效，请重新框选。");
    case BarcodeReadStatus::Timeout:
        return QStringLiteral("二维码扫描超时，请重新框选完整、清晰的二维码区域。");
    case BarcodeReadStatus::InternalError:
        return result.errorReason.trimmed().isEmpty()
                ? QStringLiteral("二维码解码器发生内部错误。")
                : QStringLiteral("二维码解码器发生内部错误：%1")
                  .arg(result.errorReason);
    case BarcodeReadStatus::NotFound:
        return QStringLiteral("当前框选区域内没有扫描到可读的二维码。");
    case BarcodeReadStatus::Success:
        break;
    }
    return QStringLiteral("当前框选区域内没有扫描到可读的二维码。");
}

QVector<QPointF> qtPolygon(const std::vector<cv::Point2f> &polygon)
{
    QVector<QPointF> result;
    result.reserve(static_cast<int>(polygon.size()));
    for (const cv::Point2f &point : polygon) {
        result.append(QPointF(point.x, point.y));
    }
    return result;
}

cv::Mat imageFromQImage(const QImage &source)
{
    if (source.isNull()) {
        return cv::Mat();
    }
    const QImage image = source.convertToFormat(QImage::Format_Grayscale8);
    return cv::Mat(image.height(), image.width(), CV_8UC1,
                   const_cast<uchar *>(image.constBits()),
                   static_cast<std::size_t>(image.bytesPerLine())).clone();
}

}

TemplateApplicationService::TemplateApplicationService(
        const std::shared_ptr<TemplateStore> &store,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder)
    : m_store(store),
      m_barcodeDecoder(barcodeDecoder)
{
    if (!m_store || !m_barcodeDecoder) {
        throw std::invalid_argument(
                    "TemplateApplicationService requires store and decoder");
    }
}

bool TemplateApplicationService::beginNew(
        DetectionMode mode,
        QString *errorMessage)
{
    if (mode == DetectionMode::Tissue) {
        setError(errorMessage, QStringLiteral("纸巾检测不使用模板。"));
        return false;
    }
    m_draft = EditableTemplate();
    m_draft.settings.detectionMode = mode;
    m_currentDirectoryPath.clear();
    m_active = true;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool TemplateApplicationService::beginEdit(
        const QString &directoryPath,
        DetectionMode expectedMode,
        QString *errorMessage)
{
    TemplateStoreError error;
    EditableTemplate loaded;
    if (!m_store->loadEditable(
            directoryPath, expectedMode, &loaded, &error)) {
        setError(errorMessage, storeErrorMessage(error));
        return false;
    }
    m_draft = loaded;
    m_currentDirectoryPath = QDir::cleanPath(
                QFileInfo(directoryPath).absoluteFilePath());
    m_active = true;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void TemplateApplicationService::cancel()
{
    m_active = false;
    m_currentDirectoryPath.clear();
    m_draft = EditableTemplate();
}

bool TemplateApplicationService::isActive() const
{
    return m_active;
}

const EditableTemplate &TemplateApplicationService::draft() const
{
    return m_draft;
}

bool TemplateApplicationService::replaceDraft(
        const EditableTemplate &value,
        QString *errorMessage)
{
    if (!m_active || value.settings.detectionMode == DetectionMode::Tissue) {
        setError(errorMessage, QStringLiteral("当前没有可编辑模板。"));
        return false;
    }
    m_draft = value;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool TemplateApplicationService::save(
        const QString &directoryPath,
        bool preserveExistingContents,
        PreparedTemplateSnapshot *preparedTemplate,
        QString *errorMessage)
{
    if (!m_active) {
        setError(errorMessage, QStringLiteral("当前没有可保存模板。"));
        return false;
    }
    TemplateStoreError error;
    if (!m_store->save(directoryPath, m_draft,
                       preserveExistingContents, &error)) {
        setError(errorMessage, storeErrorMessage(error));
        return false;
    }
    const QString normalized = QDir::cleanPath(
                QFileInfo(directoryPath).absoluteFilePath());
    m_currentDirectoryPath = normalized;
    PreparedTemplateSnapshot prepared;
    if (m_store->loadPrepared(normalized,
                              m_draft.settings.detectionMode,
                              &prepared, &error)) {
        m_activePreparedTemplate = prepared;
        if (preparedTemplate) {
            *preparedTemplate = prepared;
        }
    } else if (preparedTemplate) {
        preparedTemplate->reset();
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

TemplateSummary TemplateApplicationService::readSummary(
        const QString &directoryPath,
        DetectionMode expectedMode,
        TemplateStoreError *error) const
{
    return m_store->readSummary(directoryPath, expectedMode, error);
}

bool TemplateApplicationService::loadPreparedTemplate(
        const QString &directoryPath,
        DetectionMode expectedMode,
        PreparedTemplateSnapshot *preparedTemplate,
        QString *errorMessage) const
{
    TemplateStoreError error;
    if (!m_store->loadPrepared(directoryPath, expectedMode,
                               preparedTemplate, &error)) {
        setError(errorMessage, storeErrorMessage(error));
        return false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool TemplateApplicationService::updateTemplates(
        const QStringList &directoryPaths,
        DetectionMode expectedMode,
        const std::function<void(TemplateSettings *)> &update,
        QString *resultMessage)
{
    QVector<EditableTemplate> values;
    QStringList validationFailures;
    values.reserve(directoryPaths.size());
    for (const QString &path : directoryPaths) {
        EditableTemplate value;
        TemplateStoreError error;
        if (!m_store->loadEditable(path, expectedMode, &value, &error)) {
            validationFailures.append(
                        QStringLiteral("%1\n%2\n%3")
                        .arg(QFileInfo(path).fileName(), path,
                             storeErrorMessage(error)));
            continue;
        }
        update(&value.settings);
        values.append(value);
    }
    if (!validationFailures.isEmpty()) {
        setError(resultMessage,
                 QStringLiteral("预验证失败，本次没有修改任何模板：\n\n%1")
                 .arg(validationFailures.join(QStringLiteral("\n\n"))));
        return false;
    }
    if (values.isEmpty()) {
        setError(resultMessage, QStringLiteral("当前模式没有已选择模板。"));
        return false;
    }

    QStringList succeeded;
    for (int index = 0; index < values.size(); ++index) {
        const QString &path = directoryPaths.at(index);
        TemplateStoreError error;
        if (!m_store->save(path, values.at(index), true, &error)) {
            QStringList pending;
            for (int pendingIndex = index + 1;
                 pendingIndex < directoryPaths.size(); ++pendingIndex) {
                pending.append(QStringLiteral("%1\n%2")
                               .arg(QFileInfo(directoryPaths.at(pendingIndex))
                                    .fileName(),
                                    directoryPaths.at(pendingIndex)));
            }
            const QString succeededText = succeeded.isEmpty()
                    ? QStringLiteral("无")
                    : succeeded.join(QStringLiteral("\n\n"));
            const QString pendingText = pending.isEmpty()
                    ? QStringLiteral("无")
                    : pending.join(QStringLiteral("\n\n"));
            setError(resultMessage,
                     QStringLiteral("批量保存未全部完成。\n\n"
                                    "已成功：\n%1\n\n"
                                    "保存失败：\n%2\n%3\n\n"
                                    "尚未处理：\n%4")
                     .arg(succeededText,
                          QFileInfo(path).fileName(),
                          path + QStringLiteral("\n")
                          + storeErrorMessage(error),
                          pendingText));
            return false;
        }
        succeeded.append(QStringLiteral("%1\n%2")
                         .arg(QFileInfo(path).fileName(), path));
    }
    if (resultMessage) {
        resultMessage->clear();
    }
    return true;
}

bool TemplateApplicationService::stageInitialAssets(
        const InitialTemplateAssets &assets,
        TemplateSettings *settings,
        EditableTemplate *value,
        QString *errorMessage) const
{
    if (!settings || !value || assets.rawImage.empty()
            || assets.trackingImageRect.width <= 0
            || assets.trackingImageRect.height <= 0
            || assets.trackingImageRect.x < 0
            || assets.trackingImageRect.y < 0
            || assets.trackingImageRect.x
               + assets.trackingImageRect.width > assets.rawImage.cols
            || assets.trackingImageRect.y
               + assets.trackingImageRect.height > assets.rawImage.rows) {
        setError(errorMessage, QStringLiteral("模板原图或定位区域无效。"));
        return false;
    }
    EditableTemplate candidate = *value;
    candidate.settings = *settings;
    candidate.settings.datePolygon = qtPolygon(assets.datePolygon);
    candidate.settings.barcodePolygon = qtPolygon(assets.barcodePolygon);
    candidate.settings.stampPolygon = qtPolygon(assets.stampPolygon);
    candidate.rawImage = assets.rawImage.clone();
    candidate.trackingTemplate = assets.rawImage(
                assets.trackingImageRect).clone();
    candidate.stampRingTemplate = assets.stampRing.clone();
    *settings = candidate.settings;
    *value = candidate;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool TemplateApplicationService::stageCharacterAssets(
        const QMap<QString, QImage> &characterImages,
        EditableTemplate *value,
        QString *errorMessage) const
{
    if (!value) {
        setError(errorMessage, QStringLiteral("字符模板写入目标无效。"));
        return false;
    }
    QVector<TemplateCharacterAsset> assets;
    for (auto it = characterImages.constBegin();
         it != characterImages.constEnd(); ++it) {
        const QString name = it.key().trimmed();
        const cv::Mat image = imageFromQImage(it.value());
        if (name.isEmpty() || image.empty()) {
            setError(errorMessage, QStringLiteral("字符模板名称或图像无效。"));
            return false;
        }
        TemplateCharacterAsset asset;
        asset.fileName = name + QStringLiteral(".png");
        asset.normalizedBaseName = name.toLower();
        asset.image = image;
        assets.append(asset);
    }
    value->characterAssets = assets;
    value->replaceCharacterAssets = true;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QRect TemplateApplicationService::mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const
{
    return m_geometryService.mapDisplayRectToImage(displayRect, geometry);
}

TemplateGeometryResult TemplateApplicationService::buildGeometry(
        const TemplateDrawingInput &input,
        const TemplateDisplayGeometry &geometry) const
{
    return m_geometryService.buildGeometry(input, geometry);
}

bool TemplateApplicationService::validateBarcodeTemplate(
        const cv::Mat &sourceImage,
        const QRect &sourceRect,
        const TemplateBarcodeValidationOptions &options,
        QString *failureReason) const
{
    BarcodeReadResult decoded;
    auto fail = [&](BarcodeReadStatus status, const QString &diagnostic) {
        decoded.status = status;
        decoded.readable = false;
        decoded.errorReason = diagnostic;
        if (failureReason) {
            *failureReason = barcodeFailureMessage(decoded);
        }
        return false;
    };
    if (sourceImage.empty() || sourceRect.width() <= 5
            || sourceRect.height() <= 5) {
        return fail(BarcodeReadStatus::InvalidRoi,
                    QStringLiteral("Template barcode ROI is invalid"));
    }
    const cv::Rect bounds(0, 0, sourceImage.cols, sourceImage.rows);
    cv::Rect roi(sourceRect.x(), sourceRect.y(),
                 sourceRect.width(), sourceRect.height());
    const int padding = cvRound(
                static_cast<double>(std::min(roi.width, roi.height))
                * std::max(0, options.roiPaddingPercent) / 100.0);
    roi = cv::Rect(roi.x - padding, roi.y - padding,
                   roi.width + padding * 2,
                   roi.height + padding * 2) & bounds;
    if (roi.width <= 5 || roi.height <= 5) {
        return fail(BarcodeReadStatus::InvalidRoi,
                    QStringLiteral("Template barcode ROI is outside image"));
    }
    cv::Mat gray;
    const cv::Mat crop = sourceImage(roi);
    if (crop.channels() == 1) {
        crop.convertTo(gray, CV_8U);
    } else if (crop.channels() == 3) {
        cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
    } else if (crop.channels() == 4) {
        cv::cvtColor(crop, gray, cv::COLOR_BGRA2GRAY);
    }
    if (gray.empty()) {
        return fail(BarcodeReadStatus::InvalidRoi,
                    QStringLiteral("Template barcode ROI conversion failed"));
    }
    if (!gray.isContinuous()) {
        gray = gray.clone();
    }
    BarcodeDecodeOptions decodeOptions;
    decodeOptions.formatMask = options.formatMask;
    decodeOptions.roiPaddingPercent = options.roiPaddingPercent;
    decodeOptions.maxDecodeTimeMs = options.maxDecodeTimeMs;
    decodeOptions.enableFallback = options.enableFallback;
    decoded = m_barcodeDecoder->decode(gray, decodeOptions);
    const bool readable = decoded.status == BarcodeReadStatus::Success
            && decoded.readable
            && (!decoded.rawBytes.isEmpty() || !decoded.text.isEmpty());
    if (!readable) {
        if (failureReason) {
            *failureReason = barcodeFailureMessage(decoded);
        }
        return false;
    }
    if (failureReason) {
        failureReason->clear();
    }
    return true;
}

QString TemplateApplicationService::currentDirectoryPath() const
{
    return m_currentDirectoryPath;
}

PreparedTemplateSnapshot
TemplateApplicationService::activePreparedTemplate() const
{
    return m_activePreparedTemplate;
}

void TemplateApplicationService::setActivePreparedTemplate(
        const PreparedTemplateSnapshot &preparedTemplate)
{
    m_activePreparedTemplate = preparedTemplate;
}

QString TemplateApplicationService::storeErrorMessage(
        const TemplateStoreError &error)
{
    QStringList lines;
    lines.append(error.userMessage.isEmpty()
                 ? (error.code.isEmpty()
                    ? QStringLiteral("模板操作失败。") : error.code)
                 : error.userMessage);
    if (!error.path.isEmpty()) {
        lines.append(error.path);
    }
    if (!error.diagnostic.isEmpty()
            && error.diagnostic != error.path) {
        lines.append(error.diagnostic);
    }
    return lines.join(QStringLiteral("\n"));
}
