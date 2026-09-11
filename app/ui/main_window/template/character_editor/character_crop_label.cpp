#include "ui/main_window/template/character_editor/character_crop_label.h"

#include <QMouseEvent>
#include <QPainter>

CharacterCropLabel::CharacterCropLabel(QWidget *parent)
    : QLabel(parent)
{
    setMouseTracking(true);
}

void CharacterCropLabel::setSourceImage(const QImage &image)
{
    m_image = image;
    updateGeometry();
    update();
}

void CharacterCropLabel::setItems(const QList<TemplateCharacterBox> &items)
{
    m_items.clear();
    const QRect imageBounds(0, 0, m_image.width(), m_image.height());
    for (const TemplateCharacterBox &item : items) {
        TemplateCharacterBox normalizedItem = item;
        normalizedItem.rect = item.rect.normalized().intersected(imageBounds);
        if (normalizedItem.rect.width() > 2 && normalizedItem.rect.height() > 2) {
            m_items.append(normalizedItem);
        }
    }
    update();
    emit itemsChanged();
}

QList<TemplateCharacterBox> CharacterCropLabel::items() const
{
    QList<TemplateCharacterBox> normalizedItems;
    for (const TemplateCharacterBox &item : m_items) {
        TemplateCharacterBox normalizedItem = item;
        normalizedItem.rect = item.rect.normalized();
        if (normalizedItem.rect.width() > 2 && normalizedItem.rect.height() > 2) {
            normalizedItems.append(normalizedItem);
        }
    }
    return normalizedItems;
}

QList<TemplateCharacterBox> CharacterCropLabel::previewItems() const
{
    QList<TemplateCharacterBox> normalizedItems = items();
    if (m_drawing && !m_currentRect.isNull()) {
        TemplateCharacterBox currentItem;
        currentItem.rect = m_currentRect.normalized().intersected(
                    QRect(0, 0, m_image.width(), m_image.height()));
        if (currentItem.rect.width() > 2 && currentItem.rect.height() > 2) {
            normalizedItems.append(currentItem);
        }
    }
    return normalizedItems;
}

void CharacterCropLabel::undoLast()
{
    if (!m_items.isEmpty()) {
        m_items.removeLast();
        update();
        emit itemsChanged();
    }
}

void CharacterCropLabel::clearRects()
{
    m_items.clear();
    m_drawing = false;
    m_currentRect = QRect();
    update();
    emit itemsChanged();
}

void CharacterCropLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font());

    if (m_image.isNull()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("没有可显示的喷码区域图像"));
        return;
    }

    const QRect targetRect = imageTargetRect();
    painter.drawImage(targetRect, m_image);

    int index = 1;
    for (const TemplateCharacterBox &item : m_items) {
        const QRect widgetRect = imageToWidgetRect(
                    item.rect.normalized(), targetRect);
        painter.setPen(QPen(QColor(0, 120, 255), 2));
        painter.drawRect(widgetRect);
        painter.drawText(widgetRect.topLeft() + QPoint(4, -4),
                         QString::number(index++));
    }

    if (m_drawing && !m_currentRect.isNull()) {
        painter.setPen(QPen(QColor(255, 140, 0), 2, Qt::DashLine));
        painter.drawRect(imageToWidgetRect(
                             m_currentRect.normalized(), targetRect));
    }
}

void CharacterCropLabel::mousePressEvent(QMouseEvent *event)
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

void CharacterCropLabel::mouseMoveEvent(QMouseEvent *event)
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
    emit itemsChanged();
}

void CharacterCropLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_drawing) {
        QLabel::mouseReleaseEvent(event);
        return;
    }

    m_drawing = false;
    QRect finalRect = m_currentRect.normalized().intersected(
                QRect(0, 0, m_image.width(), m_image.height()));
    if (finalRect.width() > 2 && finalRect.height() > 2) {
        TemplateCharacterBox item;
        item.rect = finalRect;
        m_items.append(item);
    }
    m_currentRect = QRect();
    update();
    emit itemsChanged();
}

QRect CharacterCropLabel::imageTargetRect() const
{
    if (m_image.isNull()) {
        return QRect();
    }

    const QSize availableSize = size() - QSize(20, 20);
    const QSize scaledSize = m_image.size().scaled(
                availableSize, Qt::KeepAspectRatio);
    return QRect(QPoint((width() - scaledSize.width()) / 2,
                        (height() - scaledSize.height()) / 2),
                 scaledSize);
}

QPoint CharacterCropLabel::widgetToImagePoint(
        const QPoint &widgetPoint) const
{
    const QRect targetRect = imageTargetRect();
    if (!targetRect.contains(widgetPoint)
            || targetRect.width() <= 0
            || targetRect.height() <= 0) {
        return QPoint(-1, -1);
    }

    const double xRatio = static_cast<double>(m_image.width())
            / targetRect.width();
    const double yRatio = static_cast<double>(m_image.height())
            / targetRect.height();
    return QPoint(
                qBound(0,
                       static_cast<int>((widgetPoint.x() - targetRect.x())
                                        * xRatio),
                       m_image.width() - 1),
                qBound(0,
                       static_cast<int>((widgetPoint.y() - targetRect.y())
                                        * yRatio),
                       m_image.height() - 1));
}

QRect CharacterCropLabel::imageToWidgetRect(
        const QRect &imageRect,
        const QRect &targetRect) const
{
    const double xRatio = static_cast<double>(targetRect.width())
            / m_image.width();
    const double yRatio = static_cast<double>(targetRect.height())
            / m_image.height();
    return QRect(
                QPoint(targetRect.x()
                       + static_cast<int>(imageRect.x() * xRatio),
                       targetRect.y()
                       + static_cast<int>(imageRect.y() * yRatio)),
                QSize(static_cast<int>(imageRect.width() * xRatio),
                      static_cast<int>(imageRect.height() * yRatio)));
}
