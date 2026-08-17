#include "application/template_application_service.h"

#include "devices/barcode/barcode_decoder.h"
#include "recipes/recipe_store.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>

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
        return QStringLiteral(
                    "二维码扫描超时，请重新框选完整、清晰的二维码区域。");
    case BarcodeReadStatus::InternalError:
        return result.errorReason.trimmed().isEmpty()
                ? QStringLiteral("二维码解码器发生内部错误。")
                : QStringLiteral("二维码解码器发生内部错误：%1")
                  .arg(result.errorReason);
    case BarcodeReadStatus::NotFound:
        return QStringLiteral(
                    "当前框选区域内没有扫描到可读的 Data Matrix 二维码。");
    case BarcodeReadStatus::Success:
        break;
    }
    return QStringLiteral(
                "当前框选区域内没有扫描到可读的 Data Matrix 二维码。");
}

} // namespace

TemplateApplicationService::TemplateApplicationService(
        const std::shared_ptr<RecipeStore> &store,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder,
        const QString &editorWorkspacesRootPath)
    : m_store(store),
      m_barcodeDecoder(barcodeDecoder),
      m_session(editorWorkspacesRootPath)
{
    if (!m_store || !m_barcodeDecoder) {
        throw std::invalid_argument(
                    "TemplateApplicationService requires store and decoder");
    }
}

bool TemplateApplicationService::beginNew(
        const ProductRecipe &recipe,
        QString *errorMessage)
{
    return m_session.beginNew(recipe, errorMessage);
}

bool TemplateApplicationService::beginEdit(
        const QString &recipeId,
        QString *errorMessage)
{
    return m_session.beginEdit(*m_store, recipeId, errorMessage);
}

void TemplateApplicationService::cancel()
{
    m_session.reset();
}

bool TemplateApplicationService::isActive() const
{
    return m_session.isActive();
}

const ProductRecipe &TemplateApplicationService::draft() const
{
    return m_session.recipe();
}

QString TemplateApplicationService::workspacePath() const
{
    return m_session.workspacePath();
}

QMap<QString, QString>
TemplateApplicationService::assetSourcePaths() const
{
    return m_session.assetSourcePaths();
}

bool TemplateApplicationService::replaceDraft(
        const ProductRecipe &recipe,
        const QMap<QString, QString> &assetSourcePaths,
        QString *errorMessage)
{
    return m_session.replaceDraft(
                recipe, assetSourcePaths, errorMessage);
}

bool TemplateApplicationService::updateProfile(
        int profileIndex,
        const RecipeProfile &profile,
        QString *errorMessage)
{
    return m_session.updateProfile(
                profileIndex, profile, errorMessage);
}

bool TemplateApplicationService::publish(
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage)
{
    if (!m_session.publish(*m_store, preparedRecipe, errorMessage)) {
        return false;
    }
    m_activePreparedRecipe = *preparedRecipe;
    return true;
}

bool TemplateApplicationService::listRecipes(
        TemplateRecipeCatalog *catalog,
        QString *errorMessage) const
{
    RecipeCatalog storedCatalog;
    if (!m_store->listRecipes(&storedCatalog, errorMessage)) {
        return false;
    }
    if (!catalog) {
        return true;
    }
    catalog->recipes.clear();
    catalog->invalidRecipes.clear();
    for (const RecipeCatalogEntry &entry : storedCatalog.recipes) {
        TemplateRecipeCatalogEntry dto;
        dto.recipeId = entry.recipeId;
        dto.displayName = entry.displayName;
        dto.detectionMode = entry.detectionMode;
        dto.profileCount = entry.profileCount;
        catalog->recipes.append(dto);
    }
    for (const RecipeCatalogIssue &issue : storedCatalog.invalidRecipes) {
        TemplateRecipeCatalogIssue dto;
        dto.directoryName = issue.directoryName;
        dto.message = issue.message;
        catalog->invalidRecipes.append(dto);
    }
    return true;
}

bool TemplateApplicationService::loadPreparedRecipe(
        const QString &recipeId,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage) const
{
    return m_store->loadPreparedRecipe(
                recipeId, preparedRecipe, errorMessage);
}

bool TemplateApplicationService::stageInitialProfileAssets(
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    return m_assetService.stageInitialProfileAssets(
                m_session.workspacePath(), assets,
                recipe, profile, assetSourcePaths, errorMessage);
}

bool TemplateApplicationService::stageCharacterAssets(
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    return m_assetService.stageCharacterAssets(
                m_session.workspacePath(), profileIndex,
                characterImages, recipe, profile,
                assetSourcePaths, errorMessage);
}

QRect TemplateApplicationService::mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const
{
    return m_geometryService.mapDisplayRectToImage(
                displayRect, geometry);
}

TemplateProfileGeometry
TemplateApplicationService::buildProfileGeometry(
        const QRect &trackingDisplayRect,
        const QRect &barcodeDisplayRect,
        const QPolygon &dateDisplayPolygon,
        bool includeBarcode,
        const TemplateDisplayGeometry &geometry) const
{
    return m_geometryService.buildProfileGeometry(
                trackingDisplayRect, barcodeDisplayRect,
                dateDisplayPolygon, includeBarcode, geometry);
}

bool TemplateApplicationService::validateBarcodeTemplate(
        const cv::Mat &sourceImage,
        const QRect &sourceRect,
        const TemplateBarcodeValidationOptions &options,
        TemplateBarcodeValidationResult *result,
        QString *failureReason) const
{
    BarcodeReadResult decoded;
    auto publishResult = [&]() {
        if (result) {
            result->readable = decoded.readable;
            result->text = decoded.text;
        }
    };
    auto fail = [&](BarcodeReadStatus status,
                    const QString &diagnostic) {
        decoded.status = status;
        decoded.readable = false;
        decoded.errorReason = diagnostic;
        publishResult();
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
    const cv::Rect imageBounds(
                0, 0, sourceImage.cols, sourceImage.rows);
    cv::Rect roi(sourceRect.x(), sourceRect.y(),
                 sourceRect.width(), sourceRect.height());
    const int padding = cvRound(
                static_cast<double>(std::min(roi.width, roi.height))
                * std::max(0, options.roiPaddingPercent) / 100.0);
    roi = cv::Rect(roi.x - padding, roi.y - padding,
                   roi.width + padding * 2,
                   roi.height + padding * 2) & imageBounds;
    if (roi.width <= 5 || roi.height <= 5) {
        return fail(BarcodeReadStatus::InvalidRoi,
                    QStringLiteral("Template barcode ROI is outside image"));
    }

    cv::Mat gray;
    const cv::Mat crop = sourceImage(roi);
    if (crop.channels() == 1) {
        if (crop.depth() == CV_8U) {
            gray = crop.clone();
        } else {
            crop.convertTo(gray, CV_8U);
        }
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
    decoded.cornersInOriginal.clear();
    decoded.cornersInOriginal.reserve(decoded.cornersInRoi.size());
    for (const cv::Point2f &point : decoded.cornersInRoi) {
        decoded.cornersInOriginal.emplace_back(
                    point.x + roi.x, point.y + roi.y);
    }
    const bool readable =
            decoded.status == BarcodeReadStatus::Success
            && decoded.readable
            && (!decoded.rawBytes.isEmpty() || !decoded.text.isEmpty());
    if (!readable) {
        decoded.readable = false;
        publishResult();
        if (failureReason) {
            *failureReason = barcodeFailureMessage(decoded);
        }
        return false;
    }
    publishResult();
    if (failureReason) {
        failureReason->clear();
    }
    return true;
}

const TemplateModeMemory &TemplateApplicationService::modeMemory() const
{
    return m_modeMemory;
}

void TemplateApplicationService::replacePublishedRecipeIdsByMode(
        const QMap<QString, QString> &recipeIds)
{
    m_modeMemory.publishedRecipeIdsByMode() = recipeIds;
}

void TemplateApplicationService::rememberPublishedRecipe(
        const QString &modeId,
        const QString &recipeId)
{
    m_modeMemory.publishedRecipeIdsByMode().insert(modeId, recipeId);
}

void TemplateApplicationService::forgetPublishedRecipe(
        const QString &modeId)
{
    m_modeMemory.publishedRecipeIdsByMode().remove(modeId);
}

const PreparedRecipeSnapshot &
TemplateApplicationService::activePreparedRecipe() const
{
    return m_activePreparedRecipe;
}

void TemplateApplicationService::setActivePreparedRecipe(
        const PreparedRecipeSnapshot &preparedRecipe)
{
    m_activePreparedRecipe = preparedRecipe;
}

const std::vector<WordTemplateProfile> &
TemplateApplicationService::wordProfiles() const
{
    return m_wordProfiles;
}

void TemplateApplicationService::clearWordProfiles()
{
    m_wordProfiles.clear();
}

void TemplateApplicationService::replaceWordProfiles(
        const std::vector<WordTemplateProfile> &profiles)
{
    m_wordProfiles = profiles;
}

bool TemplateApplicationService::replaceWordProfile(
        int profileIndex,
        const WordTemplateProfile &profile)
{
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_wordProfiles.size())) {
        return false;
    }
    m_wordProfiles[static_cast<std::size_t>(profileIndex)] = profile;
    return true;
}
