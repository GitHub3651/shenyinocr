/**
 * @file ui/pages/template_editor_recipe.cpp
 * @brief 模板模式、Profile选择和配方发布交互。
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

void TemplateEditorPage::setupWordTemplateEditorCombo()
{
    const QString commonPushButtonStyle =
            "QPushButton {"
            "background-color: transparent;"
            "border: 1px solid #ebeef5;"
            "border-radius: 4px;"
            "color: #333333;"
            "padding: 5px 10px;"
            "}"
            "QPushButton:hover {"
            "background-color: #f2f6fc;"
            "}"
            "QPushButton:pressed {"
            "background-color: #ebeef5;"
            "}"
            "QPushButton:disabled {"
            "background-color: #f2f3f5;"
            "color: #a8abb2;"
            "border-color: #dcdfe6;"
            "}";

    {

        if (m_view.textsure_btn) m_view.textsure_btn->setStyleSheet(commonPushButtonStyle);
        if (m_view.batchTextsure_btn) m_view.batchTextsure_btn->setStyleSheet(commonPushButtonStyle);
        if (m_view.batchImageThresholdButton) m_view.batchImageThresholdButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.WriteVDpushButton) m_view.WriteVDpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_7) m_view.pushButton_7->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_3) m_view.pushButton_3->setStyleSheet(commonPushButtonStyle);
        if (m_view.sureButton) m_view.sureButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_9) m_view.pushButton_9->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_12) m_view.pushButton_12->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_tissueRoughnessThreshold) m_view.pushButton_tissueRoughnessThreshold->setStyleSheet(commonPushButtonStyle);
        if (m_view.plcmodebtn) m_view.plcmodebtn->setStyleSheet(commonPushButtonStyle);
        if (m_view.ConnectpushButton) m_view.ConnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.DisconnectpushButton) m_view.DisconnectpushButton->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_8) m_view.pushButton_8->setStyleSheet(commonPushButtonStyle);
        if (m_view.pushButton_browseImageSavePath) m_view.pushButton_browseImageSavePath->setStyleSheet(commonPushButtonStyle);
    }

    if (m_view.textsure_btn && m_view.batchTextsure_btn) {
        m_view.textsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_view.batchTextsure_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QHBoxLayout *buttonLayout = qobject_cast<QHBoxLayout *>(m_view.textsure_btn->parentWidget()
                ? m_view.textsure_btn->parentWidget()->layout()
                : nullptr);
        if (buttonLayout) {
            buttonLayout->setStretch(0, 1);
            buttonLayout->setStretch(1, 1);
        }

        m_view.textsure_btn->setToolTip("只保存当前编辑模板的目标字符，并重新加载该模板的字符图片。");
        m_view.batchTextsure_btn->setToolTip("把当前目标字符保存到所有已选择的字库模板，并分别重新加载字符图片。");
        m_view.textsure_btn->installEventFilter(m_view.eventFilterTarget);
        m_view.batchTextsure_btn->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.pushButton_browseImageSavePath) {
        m_view.pushButton_browseImageSavePath->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.pushButton_7) {
        m_view.pushButton_7->setToolTip("确认当前选择的颜色通道，用于后续图像处理和识别。");
        m_view.pushButton_7->installEventFilter(m_view.eventFilterTarget);
    }

    {
        if (m_view.checkBox) {
            m_view.checkBox->setToolTip(
                        "控制检测的触发方式。\n"
                        "勾选：使用 PLC 外部触发信号控制相机拍照和检测，启动前必须连接 PLC。\n"
                        "不勾选：使用软件软触发，启动后由相机连续采集并检测。");
            m_view.checkBox->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.pushButton_10) {
            m_view.pushButton_10->setToolTip("清空当前尚未发出的剔除队列。\n适用于异常停机、误判、手动停止后，防止之前累计的剔除信号继续输出。");
            m_view.pushButton_10->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_4) {
            m_view.label_4->setToolTip("图像判定合格的分数阈值。\n识别匹配分数低于该值时，通常判为不合格；数值越高，判定越严格。");
            m_view.label_4->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_27) {
            m_view.label_27->setToolTip("设置图像进入识别前的旋转方向。\n当相机安装方向、产品摆放方向和模板方向不一致时，需要调整这里。");
            m_view.label_27->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_16) {
            m_view.label_16->setToolTip("设置相机增益。\n增益越高画面越亮，但噪声也可能增加；一般先调曝光，曝光不足时再调增益。");
            m_view.label_16->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_14) {
            m_view.label_14->setToolTip("PLC拍照信号保持多久。\n相机偶尔漏拍、触发不稳定时可适当加大；正常不要过大，避免影响下一次触发节拍。");
            m_view.label_14->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_13) {
            m_view.label_13->setToolTip("相机收到 PLC 拍照信号后，再等待多久才真正曝光采图。\n通常在拍照距离基本正确后，用它做小范围微调。\n画面中产品还没到合适位置就加大；产品已经走过或喷码偏后就减小。");
            m_view.label_13->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_6) {
            m_view.label_6->setToolTip("检测拍照点到剔除机构中心的实际产线距离。\n剔除太早通常加大；剔除太晚通常减小。");
            m_view.label_6->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_10) {
            m_view.label_10->setToolTip("剔除机构保持动作的时长。\n不合格品剔不干净就加大；影响相邻合格品或动作拖尾就减小。");
            m_view.label_10->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_17) {
            m_view.label_17->setToolTip("选择第几路剔除输出或第几个剔除口。\n现场有多个气嘴、推杆或剔除工位时使用；填错会从错误位置剔除。");
            m_view.label_17->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.label_8) {
            m_view.label_8->setToolTip("上游传感器触发点到相机拍照中心的实际产线距离。\nPLC 根据这个距离判断产品走到相机位置后再发出拍照信号。\n画面中产品还没到拍照位置，说明触发偏早，适当加大；产品已经走过拍照位置，说明触发偏晚，适当减小。");
            m_view.label_8->installEventFilter(m_view.eventFilterTarget);
        }
        if (m_view.comboBox_3) {
            m_view.comboBox_3->setToolTip("PLC触发工作模式。\n连续触发模式：产线连续经过时，PLC按连续节拍触发相机采图和检测。\n间歇触发模式：产品分批、停顿或按间隔到位时，PLC按间歇方式触发采图和检测。");
            m_view.comboBox_3->installEventFilter(m_view.eventFilterTarget);
        }
    }

    if (m_view.VideoShoot) {
        m_view.VideoShoot->installEventFilter(m_view.eventFilterTarget);
    }

    if (m_view.batchTextsure_btn) {
        m_view.batchTextsure_btn->hide();
    }
    if (m_view.batchImageThresholdButton) {
        m_view.batchImageThresholdButton->setToolTip(
                    "把当前图像合格阈值保存到所有已选择的产品模板。");
        m_view.batchImageThresholdButton->installEventFilter(m_view.eventFilterTarget);
        m_view.batchImageThresholdButton->hide();
    }

    if (m_wordTemplateEditComboBox || !m_view.dateEdit
            || !m_view.lineEdit_yuzhi) {
        return;
    }

    QWidget *parentWidget = m_view.dateEdit->parentWidget();
    if (parentWidget) {
        QGridLayout *targetLayout = qobject_cast<QGridLayout *>(parentWidget->layout());

        m_wordTemplateEditWidget = new QWidget(parentWidget);
        m_wordTemplateEditWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditWidget->setFixedHeight(50);
        QHBoxLayout *editorLayout = new QHBoxLayout(m_wordTemplateEditWidget);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(6);

        const QString editorBoxStyle =
                "background-color: transparent;"
                "border: 1px solid #ebeef5;"
                "border-radius: 4px;"
                "color: #333333;"
                "padding: 5px 10px;";

        m_wordTemplateEditLabel = new QLabel("当前产品模板:", m_wordTemplateEditWidget);
        m_wordTemplateEditLabel->setFixedHeight(50);
        m_wordTemplateEditLabel->setStyleSheet(editorBoxStyle);

        m_wordTemplateEditComboBox = new QComboBox(m_wordTemplateEditWidget);
        m_wordTemplateEditComboBox->setObjectName("wordTemplateComboBox");
        m_wordTemplateEditComboBox->setMinimumHeight(50);
        m_wordTemplateEditComboBox->setMaximumHeight(50);
        m_wordTemplateEditComboBox->setMinimumWidth(160);
        m_wordTemplateEditComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wordTemplateEditComboBox->setStyleSheet(
                    "QComboBox {"
                    "background-color: transparent;"
                    "border: 1px solid #ebeef5;"
                    "border-radius: 4px;"
                    "color: #333333;"
                    "padding: 5px 10px;"
                    "}"
                    "QComboBox:disabled {"
                    "color: #a8abb2;"
                    "}");

        m_publishTemplateGroupButton = new QPushButton(
                    QStringLiteral("\u53D1\u5E03\u6A21\u677F\u7EC4"),
                    m_wordTemplateEditWidget);
        m_publishTemplateGroupButton->setObjectName(
                    QStringLiteral("publishTemplateGroupButton"));
        m_publishTemplateGroupButton->setFixedHeight(50);
        m_publishTemplateGroupButton->setToolTip(
                    QStringLiteral(
                        "\u628A\u5F53\u524D\u901A\u8FC7\u65E7\u201C\u9009\u62E9\u6A21\u677F\u201D"
                        "\u52A0\u8F7D\u7684\u591A\u4E2AProfile\uFF0C\u6309\u5F53\u524D\u987A\u5E8F"
                        "\u53D1\u5E03\u4E3A\u4E00\u4E2A\u4EA7\u54C1\u914D\u65B9\u3002"));
        m_publishTemplateGroupButton->setStyleSheet(commonPushButtonStyle);

        m_publishedRecipeButton = new QPushButton(
                    QStringLiteral("\u5DF2\u53D1\u5E03\u914D\u65B9"),
                    m_wordTemplateEditWidget);
        m_publishedRecipeButton->setObjectName(
                    QStringLiteral("publishedRecipeButton"));
        m_publishedRecipeButton->setFixedHeight(50);
        m_publishedRecipeButton->setToolTip(
                    QStringLiteral(
                        "\u4ECE\u8F6F\u4EF6\u914D\u65B9\u5E93\u4E2D\u9009\u62E9"
                        "\u5F53\u524D\u6A21\u5F0F\u5DF2\u7ECF\u53D1\u5E03\u7684"
                        "\u4EA7\u54C1\u914D\u65B9\u3002"));
        m_publishedRecipeButton->setStyleSheet(commonPushButtonStyle);

        editorLayout->addWidget(m_wordTemplateEditLabel);
        editorLayout->addWidget(m_wordTemplateEditComboBox, 1);
        editorLayout->addWidget(m_publishTemplateGroupButton);
        editorLayout->addWidget(m_publishedRecipeButton);
        if (targetLayout) {
            targetLayout->addWidget(m_wordTemplateEditWidget, 0, 0, 1, 3);
        }

        connect(m_wordTemplateEditComboBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this,
                [this](int index) {
                    applyWordTemplateEditorSelection(index);
                });
        connect(m_publishTemplateGroupButton,
                &QPushButton::clicked,
                this,
                [this]() {
                    if (isWordFamilyMode(currentDetectModeId())) {
                        publishCurrentWordTemplateGroup();
                    } else {
                        publishCurrentSingleTemplateRecipe();
                    }
                });
        connect(m_publishedRecipeButton,
                &QPushButton::clicked,
                this,
                &TemplateEditorPage::selectPublishedRecipe);

        m_wordTemplateEditComboBox->hide();
        m_wordTemplateEditWidget->hide();
    }

    if (m_callbacks.updateTissueVisibility) {
        m_callbacks.updateTissueVisibility();
    }
}



void TemplateEditorPage::clearWordMultiTemplateState()
{
    clearBarcodeTemplateValidation();
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    m_currentTemplateDisplayName.clear();
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    {
        QSignalBlocker targetBlocker(m_view.dateEdit);
        QSignalBlocker thresholdBlocker(m_view.lineEdit_yuzhi);
        m_view.dateEdit->clear();
        m_view.lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

void TemplateEditorPage::clearSingleTemplateRecipeState()
{
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    m_currentTemplateDisplayName.clear();
    {
        QSignalBlocker targetBlocker(m_view.dateEdit);
        QSignalBlocker thresholdBlocker(m_view.lineEdit_yuzhi);
        m_view.dateEdit->clear();
        m_view.lineEdit_yuzhi->setText(QString::number(
            RecipeProfile::DefaultImageThresholdPercent));
    }
    m_currentTemplateNameVisible = false;
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
}

QString TemplateEditorPage::detectModeIdForIndex(int index) const
{
    return TemplateModeMemory::modeIdForIndex(index);
}

QString TemplateEditorPage::currentDetectModeId() const
{
    return detectModeIdForIndex(m_view.comboBox_4 ? m_view.comboBox_4->currentIndex() : 1);
}

void TemplateEditorPage::restoreTemplatesForMode(
        const QString &modeId,
        bool showMessage)
{
    clearWordMultiTemplateState();
    clearSingleTemplateRecipeState();
    m_templateService->cancel();
    m_templateService->setActivePreparedRecipe(PreparedRecipeSnapshot());
    const QString recipeId = m_templateService->modeMemory()
            .publishedRecipeIdsByMode().value(modeId).trimmed();
    if (recipeId.isEmpty()) {
        updateCurrentTemplateName();
        return;
    }

    DetectionMode mode;
    QString errorMessage;
    QStringList pending;
    const bool modeValid = detectionModeFromUiId(modeId, &mode);
    bool restored = false;
    if (modeValid && mode == DetectionMode::Tissue) {
        restored = activatePublishedTissueRecipe(
                    recipeId, false, &errorMessage);
    } else if (modeValid
               && (mode == DetectionMode::Word
                   || mode == DetectionMode::BarcodeWord)) {
        restored = activatePublishedWordRecipe(
                    recipeId, modeId, false,
                    &pending, &errorMessage);
    } else if (modeValid) {
        restored = activatePublishedSingleTemplateRecipe(
                    recipeId, modeId, false, &errorMessage);
    }
    if (restored) {
        updateCurrentTemplateName();
        return;
    }

    m_templateService->forgetPublishedRecipe(modeId);
    saveSettings(false);
    updateCurrentTemplateName();
    if (showMessage) {
        showParameterWarning(
                    QStringLiteral("提示"),
                    QStringLiteral("上次产品配方无法恢复，记录已清除；不会读取旧模板目录：\n%1")
                    .arg(errorMessage));
    }
}

void TemplateEditorPage::refreshWordTemplateEditorCombo()
{
    const bool isWordMode = isWordFamilyMode(currentDetectModeId());
    const bool isSingleMode = isSingleTemplateRecipeMode(
                currentDetectModeId());
    const bool isStampMode =
            currentDetectModeId() == QStringLiteral("stamp_detection");
    const bool isTemplateMode = isWordMode || isSingleMode;
    DetectionMode selectedMode = DetectionMode::Word;
    const bool isRecipeMode = detectionModeFromUiId(
                currentDetectModeId(), &selectedMode);
    const bool hasWordProfiles = isWordMode && !m_templateService->wordProfiles().empty();

    if (m_view.batchTextsure_btn) {
        m_view.batchTextsure_btn->setVisible(isWordMode && m_templateService->wordProfiles().size() > 1);
    }
    if (m_view.batchImageThresholdButton) {
        m_view.batchImageThresholdButton->setVisible(
                    isWordMode && m_templateService->wordProfiles().size() > 1);
    }

    if (m_manualCharacterCropButton) {
        m_manualCharacterCropButton->setVisible(isWordMode || isStampMode);
    }

    if (m_publishTemplateGroupButton) {
        const bool canPublishWordGroup =
                isWordMode
                && !m_templateService->wordProfiles().empty()
                && m_templateService->isActive();
        const bool canPublishSingleTemplate =
                isSingleMode
                && m_templateService->isActive();
        m_publishTemplateGroupButton->setVisible(
                    canPublishWordGroup || canPublishSingleTemplate);
        m_publishTemplateGroupButton->setText(
                    isSingleMode
                    ? QStringLiteral("\u53D1\u5E03\u5F53\u524D\u6A21\u677F")
                    : QStringLiteral("\u53D1\u5E03\u6A21\u677F\u7EC4"));
        m_publishTemplateGroupButton->setToolTip(
                    QStringLiteral("事务保存当前产品配方及其完整资源目录。"));
    }

    if (!m_wordTemplateEditComboBox || !m_wordTemplateEditWidget) {
        return;
    }

    auto fillCombo = [this, hasWordProfiles, isSingleMode](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        QSignalBlocker blocker(comboBox);
        comboBox->clear();

        if (hasWordProfiles) {
            for (int i = 0; i < static_cast<int>(m_templateService->wordProfiles().size()); ++i) {
                const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(i)];
                const QString displayName = profile.name.isEmpty()
                        ? QString("模板%1").arg(i + 1)
                        : profile.name;
                comboBox->addItem(displayName, i);
            }
        } else if (isSingleMode) {
            QString displayName = m_currentTemplateDisplayName.trimmed();
            comboBox->addItem(displayName.isEmpty()
                              ? QStringLiteral("--")
                              : displayName,
                              -1);
        }
    };

    fillCombo(m_wordTemplateEditComboBox);

    m_wordTemplateEditWidget->setVisible(isRecipeMode);
    if (m_wordTemplateEditLabel) {
        m_wordTemplateEditLabel->setText(
                    selectedMode == DetectionMode::Tissue
                    ? QStringLiteral("\u5F53\u524D\u4EA7\u54C1\u914D\u65B9:")
                    : QStringLiteral("\u5F53\u524D\u7F16\u8F91\u6A21\u677F:"));
    }
    m_wordTemplateEditComboBox->setVisible(isTemplateMode);
    if (m_publishedRecipeButton) {
        m_publishedRecipeButton->setVisible(isRecipeMode);
    }

    if (!isWordMode) {
        m_currentWordTemplateEditIndex = -1;
        return;
    }

    if (hasWordProfiles) {
        int profileIndex = m_currentWordTemplateEditIndex;
        if (profileIndex < 0 || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
            profileIndex = 0;
        }
        setCurrentWordTemplateEditIndex(profileIndex);
    } else {
        m_currentWordTemplateEditIndex = -1;
    }
}

void TemplateEditorPage::applyWordTemplateEditorSelection(int comboIndex)
{
    if (!m_wordTemplateEditComboBox || comboIndex < 0) {
        return;
    }

    bool ok = false;
    const int profileIndex = m_wordTemplateEditComboBox->itemData(comboIndex).toInt(&ok);
    if (!ok || profileIndex < 0 || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
        return;
    }

    setCurrentWordTemplateEditIndex(profileIndex);
}

void TemplateEditorPage::setCurrentWordTemplateEditIndex(int profileIndex)
{
    if (profileIndex < 0
            || profileIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
        return;
    }

    m_currentWordTemplateEditIndex = profileIndex;

    auto syncCombo = [profileIndex](QComboBox *comboBox) {
        if (!comboBox) {
            return;
        }

        int comboIndex = -1;
        for (int i = 0; i < comboBox->count(); ++i) {
            if (comboBox->itemData(i).toInt() == profileIndex) {
                comboIndex = i;
                break;
            }
        }

        if (comboIndex >= 0) {
            QSignalBlocker blocker(comboBox);
            comboBox->setCurrentIndex(comboIndex);
        }
    };

    syncCombo(m_wordTemplateEditComboBox);

    const WordTemplateProfile &profile = m_templateService->wordProfiles()[static_cast<std::size_t>(profileIndex)];
    {
        QSignalBlocker blocker(m_view.dateEdit);
        m_view.dateEdit->setPlainText(profile.settings.targetText);
    }
    {
        QSignalBlocker blocker(m_view.lineEdit_yuzhi);
        m_view.lineEdit_yuzhi->setText(QString::number(static_cast<int>(profile.settings.imageThresholdPercent)));
    }
    refreshRecipeProfileDirty();

    qDebug() << "[WORD_TEMPLATE_PROFILE] editing profile:"
             << profileIndex
             << profile.name
             << "threshold:" << profile.settings.imageThresholdPercent;

    displayWordTemplateRawImage(profile);
}

void TemplateEditorPage::publishCurrentWordTemplateGroup()
{
    publishCurrentRecipeSession();
}

void TemplateEditorPage::publishCurrentSingleTemplateRecipe()
{
    publishCurrentRecipeSession();
}

void TemplateEditorPage::publishCurrentRecipeSession()
{
    if (!m_templateService->isActive()) {
        showParameterInfoAsError(
                    QStringLiteral("提示"),
                    QStringLiteral("请先通过【保存模板】创建产品配方。"));
        return;
    }
    QString errorMessage;
    PreparedRecipeSnapshot prepared;
    if (!m_templateService->publish(
            &prepared, &errorMessage)) {
        showParameterCritical(
                    QStringLiteral("严重警告"),
                    QStringLiteral("产品配方事务保存失败，原配方保持不变：\n%1")
                    .arg(errorMessage));
        return;
    }
    m_templateService->setActivePreparedRecipe(prepared);
    showParameterInfo(
                QStringLiteral("提示"),
                QStringLiteral("当前产品配方已完整保存。"));
}

void TemplateEditorPage::selectPublishedRecipe()
{
    if (isInspectionBusy() || templateOperationActive()) {
        showParameterWarning(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u8BF7\u5148\u505C\u6B62\u8BC6\u522B\u6216\u9000\u51FA"
                        "\u6A21\u677F\u5236\u4F5C\uFF0C\u518D\u9009\u62E9"
                        "\u5DF2\u53D1\u5E03\u914D\u65B9\u3002"));
        return;
    }

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(currentDetectModeId(), &detectionMode)) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F\u65E0\u6548\u3002"));
        return;
    }

    TemplateRecipeCatalog catalog;
    QString catalogError;
    if (!m_templateService->listRecipes(&catalog, &catalogError)) {
        showParameterCritical(
                    QStringLiteral("\u4E25\u91CD\u8B66\u544A"),
                    QStringLiteral(
                        "\u4EA7\u54C1\u914D\u65B9\u5217\u8868\u8BFB\u53D6\u5931\u8D25\uFF1A\n%1")
                    .arg(catalogError));
        return;
    }

    QVector<TemplateRecipeCatalogEntry> matchingRecipes;
    for (const TemplateRecipeCatalogEntry &entry : catalog.recipes) {
        if (entry.detectionMode == detectionMode) {
            matchingRecipes.append(entry);
        }
    }
    if (matchingRecipes.isEmpty()) {
        showParameterInfoAsError(
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral(
                        "\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F\u8FD8\u6CA1\u6709"
                        "\u53EF\u52A0\u8F7D\u7684\u5DF2\u53D1\u5E03\u914D\u65B9\u3002"));
        return;
    }

    RecipeSelectionDialog dialog(matchingRecipes, dialogParent());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList pendingMessages;
    QString activationError;
    bool activated = false;
    if (detectionMode == DetectionMode::Tissue) {
        activated = activatePublishedTissueRecipe(
                    dialog.selectedRecipeId(), true,
                    &activationError);
    } else if (isWordFamilyMode(currentDetectModeId())) {
        activated = activatePublishedWordRecipe(
                    dialog.selectedRecipeId(),
                    currentDetectModeId(), true,
                    &pendingMessages, &activationError);
    } else {
        activated = activatePublishedSingleTemplateRecipe(
                    dialog.selectedRecipeId(),
                    currentDetectModeId(), true,
                    &activationError);
    }
    if (!activated) {
        return;
    }
    saveSettings(false);

    const ProductRecipe &activeRecipe =
            m_templateService->draft();
    QString message = detectionMode == DetectionMode::Tissue
            ? QStringLiteral(
                "\u5DF2\u52A0\u8F7D\u7EB8\u5DFE\u4EA7\u54C1\u914D\u65B9\u201C%1\u201D\u3002")
              .arg(activeRecipe.displayName)
            : QStringLiteral(
                "\u5DF2\u52A0\u8F7D\u4EA7\u54C1\u914D\u65B9\u201C%1\u201D\uFF0C"
                "\u5171 %2 \u4E2AProfile\u3002")
              .arg(activeRecipe.displayName)
              .arg(activeRecipe.profiles.size());
    if (!pendingMessages.isEmpty()) {
        message += QStringLiteral(
                    "\n\n\u4EE5\u4E0BProfile\u76EE\u6807\u5B57\u7B26"
                    "\u5F85\u786E\u8BA4\uFF1A\n%1")
                .arg(pendingMessages.join(QLatin1Char('\n')));
        showParameterWarning(QStringLiteral("\u63D0\u793A"), message);
    } else {
        showParameterInfo(QStringLiteral("\u63D0\u793A"), message);
    }
}

bool TemplateEditorPage::activatePublishedTissueRecipe(
        const QString &recipeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_templateService->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode
               != DetectionMode::Tissue) {
        const QString message = loadError.isEmpty()
                ? QStringLiteral("纸巾产品配方无效。")
                : loadError;
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral("纸巾产品配方准备失败：\n%1")
                        .arg(message));
        }
        return false;
    }
    QString sessionError;
    if (!m_templateService->beginEdit(
                recipeId,
                &sessionError)) {
        if (errorMessage) {
            *errorMessage = sessionError;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        sessionError);
        }
        return false;
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName =
            prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    m_view.lineEdit_tissueRoughnessThreshold->setText(
                QString::number(
                    prepared->tissue.roughnessThreshold,
                    'f', 3));
    const QString modeId =
            detectionModeUiId(DetectionMode::Tissue);
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorPage::activatePublishedWordRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QStringList *pendingMessages,
        QString *errorMessage)
{
    if (pendingMessages) {
        pendingMessages->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Word
                && detectionMode != DetectionMode::BarcodeWord)) {
        const QString message = QStringLiteral(
                    "已发布字库配方只用于字库匹配和二维码+三期模式。");
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterInfoAsError(QStringLiteral("提示"), message);
        }
        return false;
    }

    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_templateService->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode != detectionMode
            || prepared->profiles.isEmpty()) {
        const QString message = loadError.isEmpty()
                ? QStringLiteral("产品配方模式或Profile无效。")
                : loadError;
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方准备失败，当前模板保持不变：\n%1")
                        .arg(message));
        }
        return false;
    }

    std::vector<WordTemplateProfile> loadedProfiles;
    loadedProfiles.reserve(
                static_cast<std::size_t>(prepared->profiles.size()));
    for (const PreparedRecipeProfile &source : prepared->profiles) {
        WordTemplateProfile profile;
        profile.name = source.definition.name;
        profile.rawImage = source.rawImage.clone();
        profile.trackingTemplate = source.trackingTemplate.clone();
        profile.barcodePoly = source.barcodePolygon;
        profile.datePoly = source.datePolygon;
        profile.settings = source.definition;
        profile.targetCount =
                preparedRecipeTargetUnits(
                    source.definition.targetText).size();
        profile.characterAssets.reserve(
                    source.characterAssets.size());
        for (const PreparedRecipeCharacterAsset &asset
             : source.characterAssets) {
            PreparedRecipeCharacterAsset copy = asset;
            copy.image = asset.image.clone();
            profile.characterAssets.push_back(copy);
        }
        for (const cv::Mat &character : source.characterTemplates) {
            profile.digitTemplates.push_back(character.clone());
        }
        profile.digitTemplateTargetIndexes =
                source.characterTemplateTargetIndexes;
        loadedProfiles.push_back(profile);
    }

    QString sessionError;
    if (!m_templateService->beginEdit(
                recipeId, &sessionError)) {
        if (errorMessage) {
            *errorMessage = sessionError;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方编辑会话无法建立，当前模板保持不变：\n%1")
                        .arg(sessionError));
        }
        return false;
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
    }
    hideTemplateGuide();
    m_templateService->replaceWordProfiles(loadedProfiles);
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    m_currentTemplateNameVisible = true;
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    if (!m_templateService->wordProfiles().empty()) {
        setCurrentWordTemplateEditIndex(0);
    }
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();

    qDebug() << "[RECIPE_SELECT] selected prepared word recipe:"
             << prepared->recipe->recipeId
             << prepared->recipe->displayName
             << "profiles:" << m_templateService->wordProfiles().size();
    return true;
}


bool TemplateEditorPage::activatePublishedSingleTemplateRecipe(
        const QString &recipeId,
        const QString &modeId,
        bool showErrorMessage,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const auto fail = [this, showErrorMessage, errorMessage](
            const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        if (showErrorMessage) {
            showParameterCritical(
                        QStringLiteral("严重警告"),
                        QStringLiteral(
                            "产品配方资源无法完整准备，当前模板保持不变：\n%1")
                        .arg(message));
        }
        return false;
    };

    DetectionMode detectionMode;
    if (!detectionModeFromUiId(modeId, &detectionMode)
            || (detectionMode != DetectionMode::Stamp
                && detectionMode != DetectionMode::Ocr)) {
        return fail(QStringLiteral(
                        "已发布单模板配方只用于模板匹配和深度OCR模式。"));
    }

    PreparedRecipeSnapshot prepared;
    QString loadError;
    if (!m_templateService->loadPreparedRecipe(
                recipeId, &prepared, &loadError)
            || !prepared
            || !prepared->recipe
            || prepared->recipe->detectionMode != detectionMode
            || prepared->profiles.size() != 1) {
        return fail(loadError.isEmpty()
                    ? QStringLiteral(
                        "单模板产品配方必须且只能包含一个Profile。")
                    : loadError);
    }

    const PreparedRecipeProfile &profile = prepared->profiles.first();

    QString sessionError;
    if (!m_templateService->beginEdit(

                recipeId,
                &sessionError)) {
        return fail(sessionError);
    }

    resetTemplateCaptureState();
    clearBarcodeTemplateValidation();
    if (imageLabel) {
        imageLabel->setTemplateDrawingEnabled(false);
        imageLabel->clearSelection();
    }
    hideTemplateGuide();
    m_templateService->clearWordProfiles();
    m_currentWordTemplateEditIndex = -1;
    m_templateService->setActivePreparedRecipe(prepared);
    m_currentTemplateDisplayName = prepared->recipe->displayName;
    applyRecipeProfileToUi(profile.definition);
    m_currentTemplateNameVisible = true;
    m_templateService->rememberPublishedRecipe(
                modeId, prepared->recipe->recipeId);
    updateCurrentTemplateName();
    refreshWordTemplateEditorCombo();
    clearRecipeProfileDirty();
    return true;
}

bool TemplateEditorPage::republishSingleTemplateRecipeSettings(
        const RecipeProfile &settings,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_templateService->isActive()
            || m_templateService->draft().profiles.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "当前已发布单模板没有有效编辑会话。");
        }
        return false;
    }

    RecipeProfile updated = settings;
    updated.name =
            m_templateService->draft().profiles.first().name;
    updated.assetKeys =
            m_templateService->draft().profiles.first().assetKeys;
    if (!m_templateService->updateProfile(
                0, updated, errorMessage)) {
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

int TemplateEditorPage::currentWordTemplateProfileIndex() const
{
    if (m_currentWordTemplateEditIndex < 0
            || m_currentWordTemplateEditIndex >= static_cast<int>(m_templateService->wordProfiles().size())) {
        return -1;
    }
    return m_currentWordTemplateEditIndex;
}
