/**
 * @file ui/pages/template_editor_profile_commands.cpp
 * @brief 模板Profile目标文本、阈值和字符资源命令。
 */

#include "ui/pages/template_editor_page.h"
#include "ui/pages/template_editor_support.h"

#include "contracts/detection_mode.h"
#include "application/inspection_application_service.h"
#include "application/settings_application_service.h"
#include "ui/dialogs/character_template_editor_dialog.h"
#include "ui/widgets/image_label.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/controllers/settings_edit_state.h"
#include "ui/dialogs/recipe_selection_dialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#pragma execution_character_set("utf-8")

using namespace TemplateEditorSupport;

void TemplateEditorPage::displayWordTemplateRawImage(
        const WordTemplateProfile &profile)
{
    if (!m_view.image_undetected || profile.rawImage.empty()) {
        return;
    }
    const QImage image = imageFromBgrMat(profile.rawImage);
    if (image.isNull()) {
        return;
    }
    m_view.image_undetected->setScaledContents(false);
    m_view.image_undetected->setAlignment(Qt::AlignCenter);
    m_view.image_undetected->setAutoFitPixmap(
                QPixmap::fromImage(image));
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    updateImageDisplayStatusText(
                QStringLiteral("正在显示模板【%1】的产品图像")
                .arg(profile.name));
}

bool TemplateEditorPage::loadWordDigitTemplatesFromProfile(
        const WordTemplateProfile &profile,
        const QStringList &baseNames,
        std::vector<cv::Mat> *templates,
        std::vector<int> *templateTargetIndexes,
        QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!templates || !templateTargetIndexes
            || baseNames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("目标字符或输出参数无效。");
        }
        return false;
    }

    std::vector<cv::Mat> selected;
    std::vector<int> indexes;
    QVector<bool> found(baseNames.size(), false);
    for (int targetIndex = 0;
         targetIndex < baseNames.size(); ++targetIndex) {
        for (const PreparedRecipeCharacterAsset &asset
             : profile.characterAssets) {
            if (preparedRecipeCharacterAssetMatchesTarget(
                    asset.normalizedBaseName,
                    baseNames.at(targetIndex))) {
                selected.push_back(asset.image.clone());
                indexes.push_back(targetIndex);
                found[targetIndex] = true;
            }
        }
    }

    QStringList missing;
    for (int i = 0; i < found.size(); ++i) {
        if (!found.at(i)) {
            missing.append(baseNames.at(i));
        }
    }
    if (!missing.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "配方中缺少目标字符资产：%1")
                    .arg(missing.join(QLatin1Char(' ')));
        }
        return false;
    }

    templates->swap(selected);
    templateTargetIndexes->swap(indexes);
    return true;
}

bool TemplateEditorPage::saveWordRecipeProfile(
        int profileIndex,
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()
            || profileIndex < 0
            || profileIndex >= static_cast<int>(
                m_templateService->wordProfiles().size())
            || profileIndex
               >= m_templateService->draft().profiles.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方Profile无效。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_templateService->draft()
            .profiles.at(profileIndex).name;
    updated.assetKeys =
            m_templateService->draft()
            .profiles.at(profileIndex).assetKeys;
    return m_templateService->updateProfile(
                profileIndex, updated, errorMessage);
}

bool TemplateEditorPage::publishWordTemplateRecipeEdit(
        int profileIndex,
        QString *errorMessage)
{
    QVector<int> profileIndexes;
    profileIndexes.append(profileIndex);
    return publishWordTemplateRecipeEdits(
                profileIndexes, errorMessage);
}

bool TemplateEditorPage::publishWordTemplateRecipeEdits(
        const QVector<int> &profileIndexes,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前产品配方没有有效编辑会话。");
        }
        return false;
    }

    ProductRecipe candidate = m_templateService->draft();
    for (int profileIndex : profileIndexes) {
        if (profileIndex < 0
                || profileIndex >= static_cast<int>(
                    m_templateService->wordProfiles().size())
                || profileIndex >= candidate.profiles.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "当前配方编辑Profile无效。");
            }
            return false;
        }
        RecipeProfile updated =
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(
                        profileIndex)].settings;
        updated.name = candidate.profiles.at(profileIndex).name;
        updated.assetKeys =
                candidate.profiles.at(profileIndex).assetKeys;
        candidate.profiles[profileIndex] = updated;
    }

    if (!m_templateService->replaceDraft(
                candidate,
                m_templateService->assetSourcePaths(),
                errorMessage)) {
        return false;
    }
    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(

                &prepared,
                errorMessage)) {
        return false;
    }
    m_templateService->setActivePreparedRecipe(prepared);
    m_templateService->rememberPublishedRecipe(
                currentDetectModeId(),
                prepared->recipe->recipeId);
    saveSettings(false);
    return true;
}

void TemplateEditorPage::applyCurrentTargetText()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改目标字符。"));
        return;
    }
    if (!m_templateService->isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先创建或加载产品配方。"));
        return;
    }

    const QString targetText =
            m_view.dateEdit->toPlainText();
    if (targetText.trimmed().isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("目标字符不能为空。"));
        return;
    }

    if (isWordFamilyMode(currentDetectModeId())) {
        const int profileIndex =
                currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError(
                        QStringLiteral("提示"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }

        const WordTemplateProfile previous =
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile profile = previous;
        std::vector<cv::Mat> templates;
        std::vector<int> indexes;
        QString validationError;
        const QStringList targetUnits =
                preparedRecipeTargetUnits(targetText);
        if (!loadWordDigitTemplatesFromProfile(
                    profile,
                    targetUnits,
                    &templates,
                    &indexes,
                    &validationError)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "目标字符对应的配方资产不完整：\n%1")
                        .arg(validationError));
            return;
        }

        profile.settings.targetText = targetText;
        profile.targetCount = targetUnits.size();
        profile.digitTemplates.swap(templates);
        profile.digitTemplateTargetIndexes.swap(indexes);
        if (!m_templateService->replaceWordProfile(
                    profileIndex, profile)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }

        QString publishError;
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            m_templateService->replaceWordProfile(
                        profileIndex, previous);
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "目标字符未生效，正式配方保持不变。"),
                        publishError);
            return;
        }

        clearTemplateTargetTextDirty();
        showParameterInfo(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "当前Profile目标字符已事务保存。"));
        return;
    }

    if (isSingleTemplateRecipeMode(currentDetectModeId())) {
        RecipeProfile settings =
                m_templateService->draft()
                .profiles.first();
        settings.targetText = targetText;
        QString publishError;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "目标字符未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
        clearTemplateTargetTextDirty();
        showParameterInfo(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "目标字符已事务保存。"));
        return;
    }

    showParameterInfoAsError(
                QStringLiteral("提示"),
                QStringLiteral("当前模式不使用目标字符。"));
}

void TemplateEditorPage::applyBatchTargetText()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentTargetText();
        return;
    }
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板字符。"));
        return;
    }
    if (!m_templateService->isActive()
            || m_templateService->wordProfiles().empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const QString targetText =
            m_view.dateEdit->toPlainText();
    const QStringList targetUnits =
            preparedRecipeTargetUnits(targetText);
    if (targetUnits.isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("目标字符不能为空。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_templateService->wordProfiles();
    std::vector<WordTemplateProfile> updated = previous;
    for (WordTemplateProfile &profile : updated) {
        std::vector<cv::Mat> templates;
        std::vector<int> indexes;
        QString validationError;
        if (!loadWordDigitTemplatesFromProfile(
                    profile,
                    targetUnits,
                    &templates,
                    &indexes,
                    &validationError)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "Profile【%1】缺少目标字符资产：\n%2")
                        .arg(profile.name, validationError));
            return;
        }
        profile.settings.targetText = targetText;
        profile.targetCount = targetUnits.size();
        profile.digitTemplates.swap(templates);
        profile.digitTemplateTargetIndexes.swap(indexes);
    }
    m_templateService->replaceWordProfiles(updated);

    QVector<int> indexes;
    for (int i = 0;
         i < static_cast<int>(m_templateService->wordProfiles().size());
         ++i) {
        indexes.append(i);
    }
    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                indexes, &publishError)) {
        m_templateService->replaceWordProfiles(previous);
        if (m_templateService->activePreparedRecipe()
                && m_templateService->activePreparedRecipe()->recipe) {
            m_templateService->beginEdit(

                        m_templateService->activePreparedRecipe()->recipe->recipeId,
                        nullptr);
        }
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "批量目标字符未生效，正式配方保持不变。"),
                    publishError);
        return;
    }

    clearTemplateTargetTextDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral(
                    "全部Profile目标字符已事务保存。"));
}

void TemplateEditorPage::applyCurrentImageThreshold()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                m_view.lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_templateService->isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先创建或加载产品配方。"));
        return;
    }

    QString publishError;
    if (isWordFamilyMode(currentDetectModeId())) {
        const int profileIndex =
                currentWordTemplateProfileIndex();
        if (profileIndex < 0) {
            showParameterInfoAsError(
                        QStringLiteral("提示"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }
        const WordTemplateProfile previous =
                m_templateService->wordProfiles()[
                    static_cast<std::size_t>(profileIndex)];
        WordTemplateProfile profile = previous;
        profile.settings.imageThresholdPercent = threshold;
        if (!m_templateService->replaceWordProfile(
                    profileIndex, profile)) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("当前编辑Profile无效。"));
            return;
        }
        if (!saveWordRecipeProfile(
                    profileIndex,
                    profile.settings,
                    &publishError)
                || !publishWordTemplateRecipeEdit(
                    profileIndex,
                    &publishError)) {
            m_templateService->replaceWordProfile(
                        profileIndex, previous);
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "图像阈值未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
    } else if (isSingleTemplateRecipeMode(
                   currentDetectModeId())) {
        RecipeProfile settings =
                m_templateService->draft()
                .profiles.first();
        settings.imageThresholdPercent = threshold;
        if (!republishSingleTemplateRecipeSettings(
                    settings, &publishError)) {
            if (m_templateService->activePreparedRecipe()
                    && m_templateService->activePreparedRecipe()->recipe) {
                m_templateService->beginEdit(

                            m_templateService->activePreparedRecipe()->recipe->recipeId,
                            nullptr);
            }
            showParameterInfoWithRedWarning(
                        QStringLiteral("提示"),
                        QStringLiteral(
                            "图像阈值未生效，正式配方保持不变。"),
                        publishError);
            return;
        }
    } else {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("当前模式不使用图像合格阈值。"));
        return;
    }


    clearTemplateImageThresholdDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("图像阈值已事务保存：%1")
                .arg(threshold));
}

void TemplateEditorPage::applyCurrentTissueThreshold()
{
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再修改纸巾配方阈值。"));
        return;
    }
    bool valid = false;
    const double threshold =
            m_view.lineEdit_tissueRoughnessThreshold
            ->text().trimmed().toDouble(&valid);
    if (!valid || !std::isfinite(threshold) || threshold <= 0.0) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral("纸巾粗糙度阈值必须是大于0的有限数字。"));
        return;
    }
    if (!m_templateService->isActive()
            || !m_templateService->activePreparedRecipe()
            || !m_templateService->activePreparedRecipe()->recipe
            || m_templateService->activePreparedRecipe()->recipe->detectionMode
               != DetectionMode::Tissue) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先使用【保存模板】创建或加载纸巾产品配方。"));
        return;
    }

    const QString recipeId =
            m_templateService->activePreparedRecipe()->recipe->recipeId;
    ProductRecipe candidate = m_templateService->draft();
    candidate.tissueParameters.roughnessThreshold = threshold;
    QString errorMessage;
    PreparedRecipeSnapshot updated;
    if (!m_templateService->replaceDraft(
            candidate, QMap<QString, QString>(), &errorMessage)
            || !m_templateService->publish(
                &updated, &errorMessage)) {
        m_templateService->beginEdit(
                    recipeId, nullptr);
        m_view.lineEdit_tissueRoughnessThreshold->setText(
                    QString::number(
                        m_templateService->activePreparedRecipe()->tissue
                        .roughnessThreshold,
                        'f', 3));
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("纸巾阈值未生效，正式配方保持不变。"),
                    errorMessage);
        return;
    }

    m_templateService->setActivePreparedRecipe(updated);
    m_view.lineEdit_tissueRoughnessThreshold->setText(
                QString::number(threshold, 'f', 3));
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("纸巾粗糙度阈值已事务保存到当前产品配方。"));
}

void TemplateEditorPage::applyBatchImageThreshold()
{
    if (!isWordFamilyMode(currentDetectModeId())) {
        applyCurrentImageThreshold();
        return;
    }
    if (isInspectionBusy()) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("请先停止检测后再批量修改模板阈值。"));
        return;
    }
    int threshold = 0;
    if (!parseIntValue(
                m_view.lineEdit_yuzhi->text(), &threshold)
            || threshold < 0 || threshold > 100) {
        showParameterWarning(
                    QStringLiteral("参数错误"),
                    QStringLiteral(
                        "图像合格阈值必须是0到100之间的整数（单位：%）。"));
        return;
    }
    if (!m_templateService->isActive()
            || m_templateService->wordProfiles().empty()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先加载字库产品配方。"));
        return;
    }

    const std::vector<WordTemplateProfile> previous =
            m_templateService->wordProfiles();
    std::vector<WordTemplateProfile> updated = previous;
    QVector<int> profileIndexes;
    for (int i = 0;
         i < static_cast<int>(updated.size());
         ++i) {
        updated[static_cast<std::size_t>(i)]
                .settings.imageThresholdPercent = threshold;
        profileIndexes.append(i);
    }
    m_templateService->replaceWordProfiles(updated);

    QString publishError;
    if (!publishWordTemplateRecipeEdits(
                profileIndexes, &publishError)) {
        m_templateService->replaceWordProfiles(previous);
        if (m_templateService->activePreparedRecipe()
                && m_templateService->activePreparedRecipe()->recipe) {
            m_templateService->beginEdit(

                        m_templateService->activePreparedRecipe()->recipe->recipeId,
                        nullptr);
        }
        showParameterInfoWithRedWarning(
                    QStringLiteral("提示"),
                    QStringLiteral(
                        "批量图像阈值未生效，正式配方保持不变。"),
                    publishError);
        return;
    }


    clearTemplateImageThresholdDirty();
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral(
                    "全部Profile图像阈值已事务保存：%1")
                .arg(threshold));
}
