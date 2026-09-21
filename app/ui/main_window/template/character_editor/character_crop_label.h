#pragma once

#include "templates/template_store.h"

#include <QImage>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QVector>

class CharacterCropLabel : public QLabel
{
    Q_OBJECT

public:
    explicit CharacterCropLabel(QWidget *parent = nullptr);

    void setSourceImage(const QImage &image);
    void setItems(const QVector<TemplateCharacterBox> &items);
    QVector<TemplateCharacterBox> items() const;
    QVector<TemplateCharacterBox> previewItems() const;
    void removeAt(int index);
    void removeLast();
    void clearRects();

signals:
    void itemsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect imageTargetRect() const;
    QPoint widgetToImagePoint(const QPoint &widgetPoint) const;
    QRect imageToWidgetRect(const QRect &imageRect, const QRect &targetRect) const;

    QImage m_image;
    QVector<TemplateCharacterBox> m_items;
    bool m_drawing = false;
    QPoint m_startPoint;
    QRect m_currentRect;
};
