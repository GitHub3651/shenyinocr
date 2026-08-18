// 文件作用：本文件用于组织模板新建、预览、编辑、保存、发布和取消等应用用例。
// 主要职责：组织模板新建、预览、编辑、保存、发布和取消等应用用例。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/template_application_service.h"

#include "engines/barcode/barcode_decoder.h"
#include "recipes/recipe_store.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace {

// 函数说明：setError 函数更新或应用对应的配置和状态。
void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

// 函数说明：barcodeFailureMessage 函数实现名称所表示的处理步骤。
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

// 函数说明：TemplateApplicationService 构造函数创建组件并初始化其依赖和初始状态。
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

// 函数说明：beginNew 函数创建、准备或启动对应流程。
bool TemplateApplicationService::beginNew(
        const ProductRecipe &recipe,
        QString *errorMessage)
{
    return m_session.beginNew(recipe, errorMessage);
}

// 函数说明：beginEdit 函数创建、准备或启动对应流程。
bool TemplateApplicationService::beginEdit(
        const QString &recipeId,
        QString *errorMessage)
{
    return m_session.beginEdit(*m_store, recipeId, errorMessage);
}

// 函数说明：cancel 函数检查相关状态并返回判断结果。
void TemplateApplicationService::cancel()
{
    m_session.reset();
}

// 函数说明：isActive 函数检查相关状态并返回判断结果。
bool TemplateApplicationService::isActive() const
{
    return m_session.isActive();
}

// 函数说明：draft 函数实现名称所表示的处理步骤。
const ProductRecipe &TemplateApplicationService::draft() const
{
    return m_session.recipe();
}

// 函数说明：workspacePath 函数实现名称所表示的处理步骤。
QString TemplateApplicationService::workspacePath() const
{
    return m_session.workspacePath();
}

QMap<QString, QString>
// 函数说明：assetSourcePaths 函数实现名称所表示的处理步骤。
TemplateApplicationService::assetSourcePaths() const
{
    return m_session.assetSourcePaths();
}

// 函数说明：replaceDraft 函数更新或应用对应的配置和状态。
bool TemplateApplicationService::replaceDraft(
        const ProductRecipe &recipe,
        const QMap<QString, QString> &assetSourcePaths,
        QString *errorMessage)
{
    return m_session.replaceDraft(
                recipe, assetSourcePaths, errorMessage);
}

// 函数说明：updateProfile 函数更新或应用对应的配置和状态。
bool TemplateApplicationService::updateProfile(
        int profileIndex,
        const RecipeProfile &profile,
        QString *errorMessage)
{
    return m_session.updateProfile(
                profileIndex, profile, errorMessage);
}

// 函数说明：publish 函数保存或发布对应的数据和资源。
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

// 函数说明：listRecipes 函数实现名称所表示的处理步骤。
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

// 函数说明：loadPreparedRecipe 函数读取、等待或计算对应的数据。
bool TemplateApplicationService::loadPreparedRecipe(
        const QString &recipeId,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage) const
{
    return m_store->loadPreparedRecipe(
                recipeId, preparedRecipe, errorMessage);
}

// 函数说明：stageInitialProfileAssets 函数实现名称所表示的处理步骤。
bool TemplateApplicationService::stageInitialProfileAssets(
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    return m_session.stageInitialProfileAssets(
                assets,
                recipe, profile, assetSourcePaths, errorMessage);
}

// 函数说明：stageCharacterAssets 函数实现名称所表示的处理步骤。
bool TemplateApplicationService::stageCharacterAssets(
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    return m_session.stageCharacterAssets(
                profileIndex,
                characterImages, recipe, profile,
                assetSourcePaths, errorMessage);
}

// 函数说明：mapDisplayRectToImage 函数校验、转换或恢复对应数据。
QRect TemplateApplicationService::mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const
{
    return m_geometryService.mapDisplayRectToImage(
                displayRect, geometry);
}

TemplateProfileGeometry
// 函数说明：buildProfileGeometry 函数创建、准备或启动对应流程。
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

// 函数说明：validateBarcodeTemplate 函数校验、转换或恢复对应数据。
bool TemplateApplicationService::validateBarcodeTemplate(
        const cv::Mat &sourceImage,
        const QRect &sourceRect,
        const TemplateBarcodeValidationOptions &options,
        QString *failureReason) const
{
    BarcodeReadResult decoded;
    auto fail = [&](BarcodeReadStatus status,
                    const QString &diagnostic) {
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
    const bool readable =
            decoded.status == BarcodeReadStatus::Success
            && decoded.readable
            && (!decoded.rawBytes.isEmpty() || !decoded.text.isEmpty());
    if (!readable) {
        decoded.readable = false;
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

// 函数说明：publishedRecipeIdsByMode 函数返回各检测模式当前发布的配方。
const QMap<QString, QString> &
TemplateApplicationService::publishedRecipeIdsByMode() const
{
    return m_publishedRecipeIdsByMode;
}

// 函数说明：replacePublishedRecipeIdsByMode 函数更新或应用对应的配置和状态。
void TemplateApplicationService::replacePublishedRecipeIdsByMode(
        const QMap<QString, QString> &recipeIds)
{
    m_publishedRecipeIdsByMode = recipeIds;
}

// 函数说明：rememberPublishedRecipe 函数实现名称所表示的处理步骤。
void TemplateApplicationService::rememberPublishedRecipe(
        const QString &modeId,
        const QString &recipeId)
{
    m_publishedRecipeIdsByMode.insert(modeId, recipeId);
}

// 函数说明：forgetPublishedRecipe 函数实现名称所表示的处理步骤。
void TemplateApplicationService::forgetPublishedRecipe(
        const QString &modeId)
{
    m_publishedRecipeIdsByMode.remove(modeId);
}

const PreparedRecipeSnapshot &
// 函数说明：activePreparedRecipe 函数实现名称所表示的处理步骤。
TemplateApplicationService::activePreparedRecipe() const
{
    return m_activePreparedRecipe;
}

// 函数说明：setActivePreparedRecipe 函数更新或应用对应的配置和状态。
void TemplateApplicationService::setActivePreparedRecipe(
        const PreparedRecipeSnapshot &preparedRecipe)
{
    m_activePreparedRecipe = preparedRecipe;
}

const std::vector<WordTemplateProfile> &
// 函数说明：wordProfiles 函数实现名称所表示的处理步骤。
TemplateApplicationService::wordProfiles() const
{
    return m_wordProfiles;
}

// 函数说明：clearWordProfiles 函数停止流程、清理状态或释放对应资源。
void TemplateApplicationService::clearWordProfiles()
{
    m_wordProfiles.clear();
}

// 函数说明：replaceWordProfiles 函数更新或应用对应的配置和状态。
void TemplateApplicationService::replaceWordProfiles(
        const std::vector<WordTemplateProfile> &profiles)
{
    m_wordProfiles = profiles;
}

// 函数说明：replaceWordProfile 函数更新或应用对应的配置和状态。
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
