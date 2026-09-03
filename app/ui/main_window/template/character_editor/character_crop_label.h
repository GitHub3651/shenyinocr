// 文件作用：显示字符模板源图，并维护用户框选的字符区域。
#pragma once

#include "templates/template_store.h"

#include <QImage>
#include <QLabel>
#include <QList>
#include <QPoint>
#include <QRect>

class CharacterCropLabel : public QLabel
{
    Q_OBJECT

public:
    explicit CharacterCropLabel(QWidget *parent = nullptr);

    void setSourceImage(const QImage &image);
    void setItems(const QList<TemplateCharacterBox> &items);
    QList<TemplateCharacterBox> items() const;
    QList<TemplateCharacterBox> previewItems() const;
    void undoLast();
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
    QList<TemplateCharacterBox> m_items;
    bool m_drawing = false;
    QPoint m_startPoint;
    QRect m_currentRect;
};
