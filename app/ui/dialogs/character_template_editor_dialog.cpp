#include "ui/dialogs/character_template_editor_dialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#pragma execution_character_set("utf-8")

class CharacterTemplateEditorDialog::CropImageLabel : public QLabel
{
public:
    explicit CropImageLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setMouseTracking(true);
        setMinimumSize(760, 260);
        setAlignment(Qt::AlignCenter);
        setStyleSheet("QLabel { background-color: #f7f9fc; border: 1px solid #dcdfe6; }");
    }

    void setChangedCallback(const std::function<void()> &callback)
    {
        m_changedCallback = callback;
    }

    void setImage(const QImage &image)
    {
        m_image = image;
        updateGeometry();
        update();
    }

    void setItems(const QList<CharacterTemplateEditorDialog::CharacterBox> &items)
    {
        m_items.clear();
        const QRect imageBounds(0, 0, m_image.width(), m_image.height());
        for (const CharacterTemplateEditorDialog::CharacterBox &item : items) {
            CharacterTemplateEditorDialog::CharacterBox normalizedItem = item;
            normalizedItem.rect = item.rect.normalized().intersected(imageBounds);
            if (normalizedItem.rect.width() > 2 && normalizedItem.rect.height() > 2) {
                m_items.append(normalizedItem);
            }
        }
        update();
        notifyChanged();
    }

    QList<CharacterTemplateEditorDialog::CharacterBox> items() const
    {
        QList<CharacterTemplateEditorDialog::CharacterBox> normalizedItems;
        for (const CharacterTemplateEditorDialog::CharacterBox &item : m_items) {
            CharacterTemplateEditorDialog::CharacterBox normalizedItem = item;
            normalizedItem.rect = item.rect.normalized();
            if (normalizedItem.rect.width() > 2 && normalizedItem.rect.height() > 2) {
                normalizedItems.append(normalizedItem);
            }
        }
        return normalizedItems;
    }

    QList<CharacterTemplateEditorDialog::CharacterBox> previewItems() const
    {
        QList<CharacterTemplateEditorDialog::CharacterBox> normalizedItems = items();
        if (m_drawing && !m_currentRect.isNull()) {
            CharacterTemplateEditorDialog::CharacterBox currentItem;
            currentItem.rect = m_currentRect.normalized().intersected(QRect(0, 0, m_image.width(), m_image.height()));
            if (currentItem.rect.width() > 2 && currentItem.rect.height() > 2) {
                normalizedItems.append(currentItem);
            }
        }
        return normalizedItems;
    }

    void undoLast()
    {
        if (!m_items.isEmpty()) {
            m_items.removeLast();
            update();
            notifyChanged();
        }
    }

    void clearRects()
    {
        m_items.clear();
        m_drawing = false;
        m_currentRect = QRect();
        update();
        notifyChanged();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLabel::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (m_image.isNull()) {
            painter.setPen(Qt::gray);
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("\u6CA1\u6709\u53EF\u663E\u793A\u7684\u55B7\u7801\u533A\u57DF\u56FE\u50CF"));
            return;
        }

        const QRect targetRect = imageTargetRect();
        painter.drawImage(targetRect, m_image);

        int index = 1;
        for (int i = 0; i < m_items.size(); ++i) {
            const CharacterTemplateEditorDialog::CharacterBox &item = m_items.at(i);
            const QRect widgetRect = imageToWidgetRect(item.rect.normalized(), targetRect);
            painter.setPen(QPen(QColor(0, 120, 255), 2));
            painter.drawRect(widgetRect);
            painter.drawText(widgetRect.topLeft() + QPoint(4, -4), QString::number(index++));
        }

        if (m_drawing && !m_currentRect.isNull()) {
            painter.setPen(QPen(QColor(255, 140, 0), 2, Qt::DashLine));
            painter.drawRect(imageToWidgetRect(m_currentRect.normalized(), targetRect));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || m_image.isNull()) {
            QLabel::mousePressEvent(event);
            return;
        }

        const QPoint imagePoint = widgetToImagePoint(event->pos());
        if (imagePoint.x() < 0 || imagePoint.y() < 0) {
            return;
        }

        m_drawing = true;
        m_startPoint = imagePoint;
        m_currentRect = QRect(m_startPoint, m_startPoint);
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_drawing) {
            QLabel::mouseMoveEvent(event);
            return;
        }

        QPoint imagePoint = widgetToImagePoint(event->pos());
        imagePoint.setX(qBound(0, imagePoint.x(), m_image.width() - 1));
        imagePoint.setY(qBound(0, imagePoint.y(), m_image.height() - 1));
        m_currentRect = QRect(m_startPoint, imagePoint);
        update();
        notifyChanged();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !m_drawing) {
            QLabel::mouseReleaseEvent(event);
            return;
        }

        m_drawing = false;
        QRect finalRect = m_currentRect.normalized();
        finalRect = finalRect.intersected(QRect(0, 0, m_image.width(), m_image.height()));
        if (finalRect.width() > 2 && finalRect.height() > 2) {
            CharacterTemplateEditorDialog::CharacterBox item;
            item.rect = finalRect;
            m_items.append(item);
        }
        m_currentRect = QRect();
        update();
        notifyChanged();
    }

private:
    void notifyChanged()
    {
        if (m_changedCallback) {
            m_changedCallback();
        }
    }

    QRect imageTargetRect() const
    {
        if (m_image.isNull()) {
            return QRect();
        }

        const QSize availableSize = size() - QSize(20, 20);
        const QSize scaledSize = m_image.size().scaled(availableSize, Qt::KeepAspectRatio);
        const int x = (width() - scaledSize.width()) / 2;
        const int y = (height() - scaledSize.height()) / 2;
        return QRect(QPoint(x, y), scaledSize);
    }

    QPoint widgetToImagePoint(const QPoint &widgetPoint) const
    {
        const QRect targetRect = imageTargetRect();
        if (!targetRect.contains(widgetPoint) || targetRect.width() <= 0 || targetRect.height() <= 0) {
            return QPoint(-1, -1);
        }

        const double xRatio = static_cast<double>(m_image.width()) / targetRect.width();
        const double yRatio = static_cast<double>(m_image.height()) / targetRect.height();
        const int imageX = qBound(0, static_cast<int>((widgetPoint.x() - targetRect.x()) * xRatio), m_image.width() - 1);
        const int imageY = qBound(0, static_cast<int>((widgetPoint.y() - targetRect.y()) * yRatio), m_image.height() - 1);
        return QPoint(imageX, imageY);
    }

    QRect imageToWidgetRect(const QRect &imageRect, const QRect &targetRect) const
    {
        const double xRatio = static_cast<double>(targetRect.width()) / m_image.width();
        const double yRatio = static_cast<double>(targetRect.height()) / m_image.height();
        return QRect(QPoint(targetRect.x() + static_cast<int>(imageRect.x() * xRatio),
                            targetRect.y() + static_cast<int>(imageRect.y() * yRatio)),
                     QSize(static_cast<int>(imageRect.width() * xRatio),
                           static_cast<int>(imageRect.height() * yRatio)));
    }

    QImage m_image;
    QList<CharacterTemplateEditorDialog::CharacterBox> m_items;
    std::function<void()> m_changedCallback;
    bool m_drawing = false;
    QPoint m_startPoint;
    QRect m_currentRect;
};

CharacterTemplateEditorDialog::CharacterTemplateEditorDialog(const QImage &sourceImage,
                                                         const RecipeProfile &initialProfile,
                                                         QWidget *parent)
    : QDialog(parent),
      m_sourceImage(sourceImage),
      m_resultProfile(initialProfile)
{
    loadSavedCharacterBoxes();
    buildUi();
}

int CharacterTemplateEditorDialog::savedCount() const
{
    return m_savedCount;
}

RecipeProfile CharacterTemplateEditorDialog::resultProfile() const
{
    return m_resultProfile;
}

QMap<QString, QImage> CharacterTemplateEditorDialog::characterImages() const
{
    return m_characterImages;
}

void CharacterTemplateEditorDialog::buildUi()
{
    setWindowTitle(QStringLiteral("\u5206\u5272\u5B57\u7B26\u6A21\u677F"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(920, 760);

    m_stack = new QStackedWidget(this);

    m_drawPage = new QWidget(this);
    QVBoxLayout *drawLayout = new QVBoxLayout(m_drawPage);
    drawLayout->setContentsMargins(10, 10, 10, 10);
    drawLayout->setSpacing(8);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    contentLayout->addLayout(leftLayout, 1);
    contentLayout->addLayout(rightLayout);

    QLabel *drawHint = new QLabel(QStringLiteral("\u5728\u55B7\u7801\u533A\u57DF\u56FE\u50CF\u4E0A\u6309\u4F4F\u9F20\u6807\u5DE6\u952E\u62D6\u62FD\uFF0C\u9010\u4E2A\u6846\u9009\u5B57\u7B26\u3002\n\u5B57\u7B26\u6846\u9009\u987A\u5E8F\u4E0D\u9650\uFF0C\u7CFB\u7EDF\u4F1A\u5728\u547D\u540D\u524D\u81EA\u52A8\u6309\u4F4D\u7F6E\u6392\u5E8F\u3002"), m_drawPage);
    drawHint->setWordWrap(true);
    drawHint->setStyleSheet("QLabel { color: #333333; font-weight: bold; }");
    leftLayout->addWidget(drawHint);

    m_cropLabel = new CropImageLabel(m_drawPage);
    m_cropLabel->setImage(m_sourceImage);
    m_cropLabel->setChangedCallback([this]() {
        refreshCharacterPreviewList();
    });
    m_cropLabel->setItems(m_initialBoxes);
    leftLayout->addWidget(m_cropLabel, 1);

    QImage sampleImage(":/sample1.png");
    if (!sampleImage.isNull()) {
        QLabel *sampleTitle = new QLabel(QStringLiteral("\u7ED3\u679C\u793A\u610F\u56FE\uFF1A"), m_drawPage);
        sampleTitle->setStyleSheet("QLabel { color: #333333; font-weight: bold; }");
        leftLayout->addWidget(sampleTitle);

        QLabel *sampleLabel = new QLabel(m_drawPage);
        sampleLabel->setAlignment(Qt::AlignCenter);
        sampleLabel->setMinimumHeight(120);
        sampleLabel->setMaximumHeight(160);
        sampleLabel->setStyleSheet("QLabel { background-color: #ffffff; border: 1px solid #dcdfe6; }");
        sampleLabel->setPixmap(QPixmap::fromImage(sampleImage).scaled(640, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        leftLayout->addWidget(sampleLabel);
    }

    QLabel *existingTitle = new QLabel(QStringLiteral("\u5B57\u7B26\u6A21\u677F\u9884\u89C8\uFF1A"), m_drawPage);
    existingTitle->setStyleSheet("QLabel { color: #333333; font-weight: bold; }");
    rightLayout->addWidget(existingTitle);

    m_previewScrollArea = new QScrollArea(m_drawPage);
    m_previewScrollArea->setWidgetResizable(true);
    m_previewScrollArea->setMinimumWidth(260);
    m_previewScrollArea->setMaximumWidth(320);
    QWidget *existingListWidget = new QWidget(m_previewScrollArea);
    m_previewGrid = new QGridLayout(existingListWidget);
    m_previewGrid->setContentsMargins(8, 8, 8, 8);
    m_previewGrid->setSpacing(8);
    m_previewScrollArea->setWidget(existingListWidget);
    rightLayout->addWidget(m_previewScrollArea, 1);
    refreshCharacterPreviewList();

    drawLayout->addLayout(contentLayout, 1);

    QHBoxLayout *drawButtonLayout = new QHBoxLayout();
    QPushButton *undoButton = new QPushButton(QStringLiteral("\u64A4\u9500\u4E0A\u4E00\u4E2A\u5B57\u7B26\u6846"), m_drawPage);
    QPushButton *clearButton = new QPushButton(QStringLiteral("\u6E05\u7A7A\u6240\u6709\u5B57\u7B26\u6846"), m_drawPage);
    QPushButton *nextButton = new QPushButton(QStringLiteral("\u4E0B\u4E00\u6B65\u547D\u540D"), m_drawPage);
    QPushButton *cancelButton = new QPushButton(QStringLiteral("\u53D6\u6D88"), m_drawPage);
    drawButtonLayout->addWidget(undoButton);
    drawButtonLayout->addWidget(clearButton);
    drawButtonLayout->addStretch();
    drawButtonLayout->addWidget(nextButton);
    drawButtonLayout->addWidget(cancelButton);
    drawLayout->addLayout(drawButtonLayout);

    connect(undoButton, &QPushButton::clicked, this, [this]() {
        m_cropLabel->undoLast();
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_cropLabel->clearRects();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(nextButton, &QPushButton::clicked, this, [this]() {
        m_sortedBoxes = m_cropLabel->items();
        if (m_sortedBoxes.isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("\u63D0\u793A"),
                                 QStringLiteral("\u8BF7\u5148\u6846\u9009\u81F3\u5C11\u4E00\u4E2A\u5B57\u7B26\u3002"));
            return;
        }

        std::sort(m_sortedBoxes.begin(), m_sortedBoxes.end(), [](const CharacterBox &a, const CharacterBox &b) {
            const int rowTolerance = qMax(8, qMin(a.rect.height(), b.rect.height()) / 2);
            if (qAbs(a.rect.center().y() - b.rect.center().y()) > rowTolerance) {
                return a.rect.center().y() < b.rect.center().y();
            }
            return a.rect.center().x() < b.rect.center().x();
        });

        rebuildNamePage();
        m_stack->setCurrentWidget(m_namePage);
    });

    m_namePage = new QWidget(this);
    QVBoxLayout *namePageLayout = new QVBoxLayout(m_namePage);
    namePageLayout->setContentsMargins(10, 10, 10, 10);
    namePageLayout->setSpacing(8);

    QLabel *nameHint = new QLabel(QStringLiteral("\u8BF7\u4E3A\u6BCF\u4E2A\u5B57\u7B26\u56FE\u7247\u586B\u5199\u5B57\u7B26\u540D\u79F0\u3002\u4F8B\u5982\u5B57\u7B26 1 \u53EA\u586B\u5199 1\uFF0C\u91CD\u590D\u540D\u79F0\u4F1A\u81EA\u52A8\u751F\u6210 1(1)\u30011(2)\u3002"), m_namePage);
    nameHint->setWordWrap(true);
    nameHint->setStyleSheet("QLabel { color: #333333; font-weight: bold; }");
    namePageLayout->addWidget(nameHint);

    QScrollArea *scrollArea = new QScrollArea(m_namePage);
    scrollArea->setWidgetResizable(true);
    QWidget *nameListWidget = new QWidget(scrollArea);
    m_nameListLayout = new QVBoxLayout(nameListWidget);
    m_nameListLayout->setContentsMargins(0, 0, 0, 0);
    m_nameListLayout->setSpacing(8);
    scrollArea->setWidget(nameListWidget);
    namePageLayout->addWidget(scrollArea, 1);

    QHBoxLayout *nameButtonLayout = new QHBoxLayout();
    QPushButton *backButton = new QPushButton(QStringLiteral("\u8FD4\u56DE\u6846\u9009"), m_namePage);
    QPushButton *saveButton = new QPushButton(QStringLiteral("\u4FDD\u5B58\u5B57\u7B26\u6A21\u677F"), m_namePage);
    QPushButton *cancelNameButton = new QPushButton(QStringLiteral("\u53D6\u6D88"), m_namePage);
    nameButtonLayout->addWidget(backButton);
    nameButtonLayout->addStretch();
    nameButtonLayout->addWidget(saveButton);
    nameButtonLayout->addWidget(cancelNameButton);
    namePageLayout->addLayout(nameButtonLayout);

    connect(backButton, &QPushButton::clicked, this, [this]() {
        m_stack->setCurrentWidget(m_drawPage);
    });
    connect(cancelNameButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        if (saveTemplates()) {
            accept();
        }
    });

    m_stack->addWidget(m_drawPage);
    m_stack->addWidget(m_namePage);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_stack);
}

void CharacterTemplateEditorDialog::rebuildNamePage()
{
    QLayoutItem *item = nullptr;
    while ((item = m_nameListLayout->takeAt(0)) != nullptr) {
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

        QWidget *rowWidget = new QWidget(m_namePage);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        QLabel *indexLabel = new QLabel(QString::number(i + 1), rowWidget);
        indexLabel->setFixedWidth(28);
        indexLabel->setAlignment(Qt::AlignCenter);

        QLabel *previewLabel = new QLabel(rowWidget);
        previewLabel->setFixedSize(90, 54);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setStyleSheet("QLabel { border: 1px solid #dcdfe6; background-color: #ffffff; }");
        previewLabel->setPixmap(QPixmap::fromImage(preview).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLineEdit *nameEdit = new QLineEdit(rowWidget);
        nameEdit->setPlaceholderText(QStringLiteral("\u8BF7\u8F93\u5165\u5B57\u7B26\u540D\u79F0"));
        nameEdit->setText(m_sortedBoxes.at(i).name.trimmed());

        QLabel *errorLabel = new QLabel(QStringLiteral("\u5B57\u7B26\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A"), rowWidget);
        errorLabel->setStyleSheet("QLabel { color: #d93025; }");
        errorLabel->hide();

        rowLayout->addWidget(indexLabel);
        rowLayout->addWidget(previewLabel);
        rowLayout->addWidget(nameEdit, 1);
        rowLayout->addWidget(errorLabel);

        QLabel *saveNameLabel = new QLabel(rowWidget);
        saveNameLabel->setMinimumWidth(170);
        saveNameLabel->setStyleSheet("QLabel { color: #606266; }");
        rowLayout->addWidget(saveNameLabel);

        connect(nameEdit, &QLineEdit::textChanged, this, [this, nameEdit, errorLabel]() {
            const bool empty = nameEdit->text().trimmed().isEmpty();
            errorLabel->setVisible(empty);
            nameEdit->setStyleSheet(empty ? "QLineEdit { border: 1px solid #d93025; }" : "");
            refreshSaveNamePreviews();
        });

        m_nameEdits.append(nameEdit);
        m_nameErrorLabels.append(errorLabel);
        m_saveNameLabels.append(saveNameLabel);
        m_nameListLayout->addWidget(rowWidget);
    }

    m_nameListLayout->addStretch();
    refreshSaveNamePreviews();
}

void CharacterTemplateEditorDialog::loadSavedCharacterBoxes()
{
    m_initialBoxes.clear();

    const QRect imageBounds(0, 0, m_sourceImage.width(), m_sourceImage.height());
    for (const RecipeCharacterBox &savedBox : m_resultProfile.characterBoxes) {
        CharacterBox item;
        item.name = savedBox.name;
        item.rect = savedBox.rect.normalized().intersected(imageBounds);
        if (item.rect.width() > 2 && item.rect.height() > 2) {
            m_initialBoxes.append(item);
        }
    }
}

void CharacterTemplateEditorDialog::refreshCharacterPreviewList()
{
    if (!m_previewGrid) {
        return;
    }

    QLayoutItem *item = nullptr;
    while ((item = m_previewGrid->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QList<CharacterBox> boxes = m_cropLabel ? m_cropLabel->previewItems() : m_initialBoxes;
    if (boxes.isEmpty()) {
        QLabel *emptyLabel = new QLabel(QStringLiteral("\u5F53\u524D\u8FD8\u6CA1\u6709\u5B57\u7B26\u6846\uFF0C\u8BF7\u5728\u5DE6\u4FA7\u6846\u9009\u5B57\u7B26\u3002"));
        emptyLabel->setWordWrap(true);
        emptyLabel->setStyleSheet("QLabel { color: #909399; }");
        m_previewGrid->addWidget(emptyLabel, 0, 0);
        return;
    }

    const int columns = 2;
    QStringList reservedFileNames;
    for (int i = 0; i < boxes.size(); ++i) {
        const CharacterBox box = boxes.at(i);
        const QRect rect = box.rect.intersected(QRect(0, 0, m_sourceImage.width(), m_sourceImage.height()));
        QWidget *itemWidget = new QWidget();
        itemWidget->setFixedSize(110, 118);
        QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(4);

        QLabel *previewLabel = new QLabel(itemWidget);
        previewLabel->setFixedSize(92, 56);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setStyleSheet("QLabel { background-color: #ffffff; border: 1px solid #dcdfe6; }");
        if (rect.width() > 0 && rect.height() > 0) {
            const QImage previewImage = m_sourceImage.copy(rect);
            previewLabel->setPixmap(QPixmap::fromImage(previewImage).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        const QString boxName = box.name.trimmed();
        QString displayText = boxName.isEmpty() ? QStringLiteral("\u672A\u547D\u540D") : boxName;
        if (!boxName.isEmpty()) {
            const QString fileName = nextAvailableFileName(boxName, reservedFileNames);
            reservedFileNames.append(fileName);
            displayText += "\n" + fileName;
        }

        QLabel *fileNameLabel = new QLabel(displayText, itemWidget);
        fileNameLabel->setFixedHeight(54);
        fileNameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        fileNameLabel->setToolTip(displayText);
        fileNameLabel->setStyleSheet("QLabel { color: #333333; }");

        itemLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);
        itemLayout->addWidget(fileNameLabel);
        m_previewGrid->addWidget(itemWidget, i / columns, i % columns, Qt::AlignTop | Qt::AlignHCenter);
    }

    if (m_previewScrollArea && m_previewScrollArea->verticalScrollBar()) {
        QTimer::singleShot(0, this, [this]() {
            if (m_previewScrollArea && m_previewScrollArea->verticalScrollBar()) {
                QScrollBar *bar = m_previewScrollArea->verticalScrollBar();
                bar->setValue(bar->maximum());
            }
        });
    }
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

        const QString fileName = nextAvailableFileName(baseName, reservedFileNames);
        reservedFileNames.append(fileName);
        label->setText(QStringLiteral("\u5C06\u4FDD\u5B58\u4E3A\uFF1A%1").arg(fileName));
    }
}

bool CharacterTemplateEditorDialog::saveTemplates()
{
    if (m_sortedBoxes.isEmpty()
            || m_nameEdits.size() != m_sortedBoxes.size()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("\u63D0\u793A"),
                    QStringLiteral("\u6CA1\u6709\u53EF\u4FDD\u5B58\u7684\u5B57\u7B26\u6A21\u677F\u3002"));
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
        edit->setStyleSheet(
                    invalid
                    ? QStringLiteral("QLineEdit { border: 1px solid #d93025; }")
                    : QString());
        if (errorLabel) {
            errorLabel->setVisible(invalid);
        }
        if (invalid) {
            QMessageBox::warning(
                        this,
                        QStringLiteral("\u63D0\u793A"),
                        QStringLiteral("\u5B57\u7B26\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A\u6216\u5305\u542B \\ / : * ? \" < > |\u3002"));
            return false;
        }
    }

    m_resultProfile.characterBoxes.clear();
    m_resultProfile.characterSourceSize = m_sourceImage.size();
    m_characterImages.clear();
    QStringList reservedFileNames;
    for (int index = 0; index < m_sortedBoxes.size(); ++index) {
        const QString name = m_nameEdits.at(index)->text().trimmed();
        const QString fileName =
                nextAvailableFileName(name, reservedFileNames);
        reservedFileNames.append(fileName);
        const QRect rect = m_sortedBoxes.at(index).rect
                .normalized()
                .intersected(QRect(QPoint(0, 0), m_sourceImage.size()));
        if (rect.width() <= 0 || rect.height() <= 0) {
            return false;
        }
        m_sortedBoxes[index].name = name;
        RecipeCharacterBox box;
        box.name = name;
        box.rect = rect;
        m_resultProfile.characterBoxes.append(box);
        m_characterImages.insert(fileName, m_sourceImage.copy(rect));
    }

    m_savedCount = m_characterImages.size();
    m_cropLabel->setItems(m_sortedBoxes);
    refreshCharacterPreviewList();
    return m_savedCount > 0;
}
