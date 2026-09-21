#include "ui/main_window/template/character_editor/character_template_editor_dialog.h"
#include "system_support/logging/log_categories.h"
#include "ui/main_window/template/character_editor/character_ocr_engine.h"
#include "ui/main_window/template/character_editor/character_crop_label.h"
#include "ui_character_template_editor_dialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <exception>

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

QString filteredTemplateCharacter(const QString &text)
{
    const QString value = text.trimmed();
    const QStringList units =
            TemplateStore::templateTargetUnits(value);
    if (value.size() != 1 || units.size() != 1) {
        return QString();
    }
    return value;
}

cv::Mat bgrMatFromQImage(const QImage &source)
{
    QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbView(
                rgb.height(),
                rgb.width(),
                CV_8UC3,
                rgb.bits(),
                rgb.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(rgbView, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

} // namespace

CharacterTemplateEditorDialog::CharacterTemplateEditorDialog(
        const QString &templatePath,
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
    ui->lineEdit_currentCharacterTemplate->setText(templatePath);

    const QString configPath =
            QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("config_ocr.txt"));
    try {
        m_ocrEngine.reset(new CharacterOcrEngine(configPath));
    }
    catch (const std::exception &error) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=character_template.ocr_init_failed diagnostic=%1")
                   .arg(QString::fromLocal8Bit(error.what()));
    }

    ui->label_characterCropCanvas->setSourceImage(m_sourceImage);
    ui->label_characterCropCanvas->setItems(
                m_resultSettings.characterBoxes);
    connect(ui->label_characterCropCanvas, &CharacterCropLabel::itemsChanged,
            this, &CharacterTemplateEditorDialog::refreshCharacterPreviewList);
    connect(ui->pushButton_removeLastCharacterBox, &QPushButton::clicked,
            ui->label_characterCropCanvas, &CharacterCropLabel::removeLast);
    connect(ui->pushButton_clearCharacterBoxes, &QPushButton::clicked,
            ui->label_characterCropCanvas, &CharacterCropLabel::clearRects);
    connect(ui->pushButton_autoSplitCharacters, &QPushButton::clicked,
            this, &CharacterTemplateEditorDialog::autoSplitCharacters);
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

    const QImage sampleImage(QStringLiteral(":/png/sample1.png"));
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
        return a.rect.center().y() < b.rect.center().y();
    });

    QVector<QVector<TemplateCharacterBox>> rows;
    for (const TemplateCharacterBox &box : m_sortedBoxes) {
        if (rows.isEmpty()) {
            rows.append(QVector<TemplateCharacterBox>{box});
            continue;
        }

        const QVector<TemplateCharacterBox> &row = rows.constLast();
        int centerYTotal = 0;
        int heightTotal = 0;
        for (const TemplateCharacterBox &rowBox : row) {
            centerYTotal += rowBox.rect.center().y();
            heightTotal += rowBox.rect.height();
        }

        const int rowAverageCenterY = centerYTotal / row.size();
        const int rowAverageHeight = heightTotal / row.size();
        const int rowTolerance = qMax(
                    8, qMin(rowAverageHeight, box.rect.height()) / 2);
        if (qAbs(box.rect.center().y() - rowAverageCenterY)
                <= rowTolerance) {
            rows.last().append(box);
        } else {
            rows.append(QVector<TemplateCharacterBox>{box});
        }
    }

    m_sortedBoxes.clear();
    for (QVector<TemplateCharacterBox> &row : rows) {
        std::sort(row.begin(), row.end(),
                  [](const TemplateCharacterBox &a,
                     const TemplateCharacterBox &b) {
            return a.rect.center().x() < b.rect.center().x();
        });
        for (const TemplateCharacterBox &box : row) {
            m_sortedBoxes.append(box);
        }
    }

    recognizeUnnamedCharacterNames();
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
        const QRect rect = m_sortedBoxes.at(i).rect;
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

void CharacterTemplateEditorDialog::refreshCharacterPreviewList()
{
    QLayoutItem *item = nullptr;
    while ((item = ui->gridLayout_preview->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QVector<TemplateCharacterBox> boxes =
            ui->label_characterCropCanvas->previewItems();
    if (boxes.isEmpty()) {
        QLabel *emptyLabel = new QLabel(QStringLiteral("当前还没有字符框，请在左侧框选字符。"));
        emptyLabel->setObjectName(QStringLiteral("label_characterPreviewEmpty"));
        emptyLabel->setWordWrap(true);
        emptyLabel->setAlignment(Qt::AlignCenter);
        ui->gridLayout_preview->addWidget(
                    emptyLabel, 0, 0, Qt::AlignCenter);
        return;
    }

    const int confirmedCount =
            ui->label_characterCropCanvas->items().size();
    const int columns = 3;
    QStringList reservedFileNames;
    for (int i = 0; i < boxes.size(); ++i) {
        const TemplateCharacterBox box = boxes.at(i);
        const QRect rect = box.rect;
        QWidget *itemWidget = new QWidget();
        itemWidget->setFixedSize(110, 118);
        QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(4);

        QLabel *previewLabel = new QLabel(itemWidget);
        previewLabel->setObjectName(QStringLiteral("label_characterPreviewImage"));
        previewLabel->setFixedSize(92, 56);
        previewLabel->setAlignment(Qt::AlignCenter);
        const QImage previewImage = m_sourceImage.copy(rect);
        previewLabel->setPixmap(QPixmap::fromImage(previewImage).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        if (i < confirmedCount) {
            QToolButton *deleteButton = new QToolButton(previewLabel);
            deleteButton->setObjectName(
                        QStringLiteral("toolButton_deleteCharacterBox"));
            deleteButton->setIcon(
                        QIcon(QStringLiteral(":/svg/unused/trash.svg")));
            deleteButton->setIconSize(QSize(16, 16));
            deleteButton->setToolTip(QStringLiteral("删除此字符框"));
            deleteButton->setFixedSize(24, 24);
            deleteButton->move(previewLabel->width()
                               - deleteButton->width(),
                               0);
            connect(deleteButton, &QToolButton::clicked,
                    this, [this, i]() {
                ui->label_characterCropCanvas->removeAt(i);
            });
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

    m_resultSettings.characterSourceSize = m_sourceImage.size();
    m_characterImages.clear();
    QStringList reservedFileNames;
    for (int index = 0; index < m_sortedBoxes.size(); ++index) {
        const QString name = m_nameEdits.at(index)->text().trimmed();
        const QString fileName =
                nextAvailableFileName(
                    characterStorageStem(name), reservedFileNames);
        reservedFileNames.append(fileName);
        const QRect rect = m_sortedBoxes.at(index).rect;
        m_sortedBoxes[index].name = name;
        m_characterImages.insert(fileName, m_sourceImage.copy(rect));
    }

    m_resultSettings.characterBoxes = m_sortedBoxes;
    return !m_characterImages.isEmpty();
}

void CharacterTemplateEditorDialog::autoSplitCharacters()
{
    if (!m_ocrEngine) {
        QMessageBox::information(
                    this,
                    QStringLiteral("自动分割不可用"),
                    QStringLiteral(
                        "深度 OCR 自动分割功能当前不可用，"
                        "请继续手动框选字符。"));
        return;
    }

    cv::Mat source = bgrMatFromQImage(m_sourceImage);
    try {
        const std::vector<OcrRecognitionItem> result =
                m_ocrEngine->segmentCharacters(source);
        QVector<TemplateCharacterBox> generatedBoxes;
        for (const OcrRecognitionItem &item : result) {
            if (filteredTemplateCharacter(
                    QString::fromStdString(item.text)).isEmpty()) {
                continue;
            }

            const cv::Rect detectedRect = cv::boundingRect(item.box);
            TemplateCharacterBox box;
            box.rect = QRect(
                        detectedRect.x,
                        detectedRect.y,
                        detectedRect.width,
                        detectedRect.height);
            if (box.rect.width() < 4 || box.rect.height() < 4) {
                continue;
            }
            generatedBoxes.append(box);
        }

        if (generatedBoxes.isEmpty()) {
            QMessageBox::information(
                        this,
                        QStringLiteral("自动分割"),
                        QStringLiteral(
                            "没有识别到可分割的字符，"
                            "请继续手动框选。"));
            return;
        }

        if (!ui->label_characterCropCanvas->items().isEmpty()
                && QMessageBox::question(
                    this,
                    QStringLiteral("替换字符框"),
                    QStringLiteral(
                        "自动分割将一次性替换现有全部字符框，"
                        "是否继续？"),
                    QMessageBox::Ok | QMessageBox::Cancel,
                    QMessageBox::Cancel) != QMessageBox::Ok) {
            return;
        }

        ui->label_characterCropCanvas->setItems(generatedBoxes);
    }
    catch (const std::exception &error) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=character_template.auto_split_failed diagnostic=%1")
                   .arg(QString::fromLocal8Bit(error.what()));
        QMessageBox::warning(
                    this,
                    QStringLiteral("自动分割失败"),
                    QStringLiteral(
                        "深度 OCR 自动分割失败，"
                        "请继续手动框选字符。"));
    }
}

void CharacterTemplateEditorDialog::recognizeUnnamedCharacterNames()
{
    if (!m_ocrEngine) {
        return;
    }

    cv::Mat source = bgrMatFromQImage(m_sourceImage);
    for (int index = 0; index < m_sortedBoxes.size(); ++index) {
        TemplateCharacterBox &box = m_sortedBoxes[index];
        if (!box.name.trimmed().isEmpty()) {
            continue;
        }

        const QRect rect = box.rect;
        cv::Mat characterImage = source(
                    cv::Rect(rect.x(),
                             rect.y(),
                             rect.width(),
                             rect.height()));
        try {
            const QString recognizedText =
                    filteredTemplateCharacter(
                        QString::fromStdString(
                            m_ocrEngine->recognizeCharacter(
                                characterImage)));
            if (!recognizedText.isEmpty()) {
                box.name = recognizedText;
            }
        }
        catch (const std::exception &error) {
            qCWarning(logTemplate).noquote()
                    << QStringLiteral(
                        "event=character_template.ocr_name_failed "
                        "index=%1 diagnostic=%2")
                       .arg(index)
                       .arg(QString::fromLocal8Bit(error.what()));
        }
    }
}
