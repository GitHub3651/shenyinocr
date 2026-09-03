// 文件作用：本文件用于提供字符模板切分、命名和编辑所需的对话框交互。
// 主要职责：提供字符模板切分、命名和编辑所需的对话框交互。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/main_window/template/character_editor/character_template_editor_dialog.h"
#include "ui/main_window/template/character_editor/character_crop_label.h"
#include "ui_character_template_editor_dialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>

namespace {

void setInputError(QLineEdit *edit, bool hasError)
{
    if (edit->property("hasError").toBool() == hasError) {
        return;
    }
    edit->setProperty("hasError", hasError);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
    edit->update();
}

} // namespace

CharacterTemplateEditorDialog::CharacterTemplateEditorDialog(
        const QImage &sourceImage,
        const TemplateSettings &initialSettings,
        QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CharacterTemplateEditorDialog),
      m_sourceImage(sourceImage),
      m_resultSettings(initialSettings)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    loadSavedCharacterBoxes();

    ui->label_characterCropCanvas->setSourceImage(m_sourceImage);
    ui->label_characterCropCanvas->setItems(m_initialBoxes);
    connect(ui->label_characterCropCanvas, &CharacterCropLabel::itemsChanged,
            this, &CharacterTemplateEditorDialog::refreshCharacterPreviewList);
    connect(ui->pushButton_undoCharacterBox, &QPushButton::clicked,
            ui->label_characterCropCanvas, &CharacterCropLabel::undoLast);
    connect(ui->pushButton_clearCharacterBoxes, &QPushButton::clicked,
            ui->label_characterCropCanvas, &CharacterCropLabel::clearRects);
    connect(ui->pushButton_nextCharacterNaming, &QPushButton::clicked,
            this, &CharacterTemplateEditorDialog::moveToNamingPage);
    connect(ui->pushButton_cancelCharacterDrawing, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(ui->pushButton_backToCharacterDrawing, &QPushButton::clicked,
            this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->page_draw);
    });
    connect(ui->pushButton_saveCharacterTemplates, &QPushButton::clicked,
            this, &CharacterTemplateEditorDialog::saveAndAccept);
    connect(ui->pushButton_cancelCharacterNaming, &QPushButton::clicked,
            this, &QDialog::reject);

    const QImage sampleImage(QStringLiteral(":/sample1.png"));
    const bool hasSample = !sampleImage.isNull();
    ui->label_characterSampleTitle->setVisible(hasSample);
    ui->label_characterSample->setVisible(hasSample);
    if (hasSample) {
        ui->label_characterSample->setPixmap(
                    QPixmap::fromImage(sampleImage).scaled(
                        640, 150, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation));
    }
    refreshCharacterPreviewList();
}

CharacterTemplateEditorDialog::~CharacterTemplateEditorDialog() = default;

int CharacterTemplateEditorDialog::savedCount() const
{
    return m_savedCount;
}

TemplateSettings CharacterTemplateEditorDialog::resultSettings() const
{
    return m_resultSettings;
}

QMap<QString, QImage> CharacterTemplateEditorDialog::characterImages() const
{
    return m_characterImages;
}

void CharacterTemplateEditorDialog::moveToNamingPage()
{
    m_sortedBoxes = ui->label_characterCropCanvas->items();
    if (m_sortedBoxes.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("提示"),
                             QStringLiteral("请先框选至少一个字符。"));
        return;
    }

    std::sort(m_sortedBoxes.begin(), m_sortedBoxes.end(),
              [](const TemplateCharacterBox &a,
                 const TemplateCharacterBox &b) {
        const int rowTolerance = qMax(
                    8, qMin(a.rect.height(), b.rect.height()) / 2);
        if (qAbs(a.rect.center().y() - b.rect.center().y())
                > rowTolerance) {
            return a.rect.center().y() < b.rect.center().y();
        }
        return a.rect.center().x() < b.rect.center().x();
    });

    rebuildNamePage();
    ui->stackedWidget->setCurrentWidget(ui->page_names);
}

void CharacterTemplateEditorDialog::saveAndAccept()
{
    if (saveTemplates()) {
        accept();
    }
}

void CharacterTemplateEditorDialog::rebuildNamePage()
{
    QLayoutItem *item = nullptr;
    while ((item = ui->verticalLayout_nameList->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    m_nameEdits.clear();
    m_nameErrorLabels.clear();
    m_saveNameLabels.clear();

    for (int i = 0; i < m_sortedBoxes.size(); ++i) {
        const QRect rect = m_sortedBoxes.at(i).rect.intersected(QRect(0, 0, m_sourceImage.width(), m_sourceImage.height()));
        QImage preview = m_sourceImage.copy(rect);

        QWidget *rowWidget = new QWidget(ui->page_names);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        QLabel *indexLabel = new QLabel(QString::number(i + 1), rowWidget);
        indexLabel->setFixedWidth(28);
        indexLabel->setAlignment(Qt::AlignCenter);

        QLabel *previewLabel = new QLabel(rowWidget);
        previewLabel->setObjectName(QStringLiteral("label_characterPreviewImage"));
        previewLabel->setFixedSize(90, 54);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setPixmap(QPixmap::fromImage(preview).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLineEdit *nameEdit = new QLineEdit(rowWidget);
        nameEdit->setObjectName(QStringLiteral("lineEdit_characterName"));
        nameEdit->setPlaceholderText(QStringLiteral("请输入字符名称"));
        nameEdit->setText(m_sortedBoxes.at(i).name.trimmed());

        QLabel *errorLabel = new QLabel(QStringLiteral("字符名称不能为空"), rowWidget);
        errorLabel->setObjectName(QStringLiteral("label_characterNameError"));
        errorLabel->hide();

        rowLayout->addWidget(indexLabel);
        rowLayout->addWidget(previewLabel);
        rowLayout->addWidget(nameEdit, 1);
        rowLayout->addWidget(errorLabel);

        QLabel *saveNameLabel = new QLabel(rowWidget);
        saveNameLabel->setObjectName(QStringLiteral("label_characterSaveName"));
        saveNameLabel->setMinimumWidth(170);
        rowLayout->addWidget(saveNameLabel);

        connect(nameEdit, &QLineEdit::textChanged, this, [this, nameEdit, errorLabel]() {
            const bool empty = nameEdit->text().trimmed().isEmpty();
            errorLabel->setVisible(empty);
            setInputError(nameEdit, empty);
            refreshSaveNamePreviews();
        });

        m_nameEdits.append(nameEdit);
        m_nameErrorLabels.append(errorLabel);
        m_saveNameLabels.append(saveNameLabel);
        ui->verticalLayout_nameList->addWidget(rowWidget);
    }

    ui->verticalLayout_nameList->addStretch();
    refreshSaveNamePreviews();
}

void CharacterTemplateEditorDialog::loadSavedCharacterBoxes()
{
    m_initialBoxes.clear();

    const QRect imageBounds(0, 0, m_sourceImage.width(), m_sourceImage.height());
    for (const TemplateCharacterBox &savedBox : m_resultSettings.characterBoxes) {
        TemplateCharacterBox item;
        item.name = savedBox.name;
        item.rect = savedBox.rect.normalized().intersected(imageBounds);
        if (item.rect.width() > 2 && item.rect.height() > 2) {
            m_initialBoxes.append(item);
        }
    }
}

void CharacterTemplateEditorDialog::refreshCharacterPreviewList()
{
    QLayoutItem *item = nullptr;
    while ((item = ui->gridLayout_preview->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QList<TemplateCharacterBox> boxes =
            ui->label_characterCropCanvas->previewItems();
    if (boxes.isEmpty()) {
        QLabel *emptyLabel = new QLabel(QStringLiteral("当前还没有字符框，请在左侧框选字符。"));
        emptyLabel->setObjectName(QStringLiteral("label_characterPreviewEmpty"));
        emptyLabel->setWordWrap(true);
        ui->gridLayout_preview->addWidget(emptyLabel, 0, 0);
        return;
    }

    const int columns = 2;
    QStringList reservedFileNames;
    for (int i = 0; i < boxes.size(); ++i) {
        const TemplateCharacterBox box = boxes.at(i);
        const QRect rect = box.rect.intersected(QRect(0, 0, m_sourceImage.width(), m_sourceImage.height()));
        QWidget *itemWidget = new QWidget();
        itemWidget->setFixedSize(110, 118);
        QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(4);

        QLabel *previewLabel = new QLabel(itemWidget);
        previewLabel->setObjectName(QStringLiteral("label_characterPreviewImage"));
        previewLabel->setFixedSize(92, 56);
        previewLabel->setAlignment(Qt::AlignCenter);
        if (rect.width() > 0 && rect.height() > 0) {
            const QImage previewImage = m_sourceImage.copy(rect);
            previewLabel->setPixmap(QPixmap::fromImage(previewImage).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        const QString boxName = box.name.trimmed();
        QString displayText = boxName.isEmpty() ? QStringLiteral("未命名") : boxName;
        if (!boxName.isEmpty()) {
            const QString fileName = nextAvailableFileName(
                        characterStorageStem(boxName), reservedFileNames);
            reservedFileNames.append(fileName);
            displayText += "\n" + fileName;
        }

        QLabel *fileNameLabel = new QLabel(displayText, itemWidget);
        fileNameLabel->setObjectName(QStringLiteral("label_characterPreviewFileName"));
        fileNameLabel->setFixedHeight(54);
        fileNameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        fileNameLabel->setToolTip(displayText);
        itemLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);
        itemLayout->addWidget(fileNameLabel);
        ui->gridLayout_preview->addWidget(itemWidget, i / columns, i % columns, Qt::AlignTop | Qt::AlignHCenter);
    }

    QTimer::singleShot(0, this, [this]() {
        QScrollBar *bar = ui->scrollArea_preview->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

bool CharacterTemplateEditorDialog::isSystemTemplateFile(const QString &fileName) const
{
    const QString lowerName = fileName.trimmed().toLower();
    return lowerName == "template_raw.png"
            || lowerName == "tracking_template.bmp"
            || lowerName == "template_ring.bmp";
}

QString CharacterTemplateEditorDialog::nextAvailableFileName(const QString &baseName,
                                                           const QStringList &reservedFileNames) const
{
    auto isUsed = [this, &reservedFileNames](const QString &fileName) {
        return isSystemTemplateFile(fileName)
                || reservedFileNames.contains(fileName, Qt::CaseInsensitive);
    };

    QString fileName = baseName + ".png";
    if (!isUsed(fileName)) {
        return fileName;
    }

    int index = 1;
    while (true) {
        fileName = QString("%1(%2).png").arg(baseName).arg(index);
        if (!isUsed(fileName)) {
            return fileName;
        }
        ++index;
    }
}

void CharacterTemplateEditorDialog::refreshSaveNamePreviews()
{
    QStringList reservedFileNames;
    for (int i = 0; i < m_nameEdits.size() && i < m_saveNameLabels.size(); ++i) {
        QLineEdit *edit = m_nameEdits.at(i);
        QLabel *label = m_saveNameLabels.at(i);
        const QString baseName = edit->text().trimmed();
        if (baseName.isEmpty()) {
            label->setText(QString());
            continue;
        }

        const QString fileName = nextAvailableFileName(
                    characterStorageStem(baseName), reservedFileNames);
        reservedFileNames.append(fileName);
        label->setText(QStringLiteral("将保存为：%1").arg(fileName));
    }
}

bool CharacterTemplateEditorDialog::saveTemplates()
{
    if (m_sortedBoxes.isEmpty()
            || m_nameEdits.size() != m_sortedBoxes.size()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("提示"),
                    QStringLiteral("没有可保存的字符模板。"));
        return false;
    }

    const QRegularExpression invalidFileNameChars(
                R"([\\/:*?"<>|])");
    for (int index = 0; index < m_nameEdits.size(); ++index) {
        QLineEdit *edit = m_nameEdits.at(index);
        QLabel *errorLabel = index < m_nameErrorLabels.size()
                ? m_nameErrorLabels.at(index) : nullptr;
        const QString name = edit->text().trimmed();
        const bool invalid = name.isEmpty()
                || name.contains(invalidFileNameChars)
                || name == QLatin1String(".")
                || name == QLatin1String("..");
        setInputError(edit, invalid);
        if (errorLabel) {
            errorLabel->setVisible(invalid);
        }
        if (invalid) {
            QMessageBox::warning(
                        this,
                        QStringLiteral("提示"),
                        QStringLiteral("字符名称不能为空或包含 \\ / : * ? \" < > |。"));
            return false;
        }
    }

    m_resultSettings.characterBoxes.clear();
    m_resultSettings.characterSourceSize = m_sourceImage.size();
    m_characterImages.clear();
    QStringList reservedFileNames;
    for (int index = 0; index < m_sortedBoxes.size(); ++index) {
        const QString name = m_nameEdits.at(index)->text().trimmed();
        const QString fileName =
                nextAvailableFileName(
                    characterStorageStem(name), reservedFileNames);
        reservedFileNames.append(fileName);
        const QRect rect = m_sortedBoxes.at(index).rect
                .normalized()
                .intersected(QRect(QPoint(0, 0), m_sourceImage.size()));
        if (rect.width() <= 0 || rect.height() <= 0) {
            return false;
        }
        m_sortedBoxes[index].name = name;
        TemplateCharacterBox box;
        box.name = name;
        box.rect = rect;
        m_resultSettings.characterBoxes.append(box);
        m_characterImages.insert(fileName, m_sourceImage.copy(rect));
    }

    m_savedCount = m_characterImages.size();
    ui->label_characterCropCanvas->setItems(m_sortedBoxes);
    refreshCharacterPreviewList();
    return m_savedCount > 0;
}
