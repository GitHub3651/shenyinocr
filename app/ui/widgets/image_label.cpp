// 文件作用：显示检测图像，并处理模板制作时的模式化矩形、多边形和鼠标键盘交互。
// 模块位置：界面层；只保存显示坐标和绘制状态，不拥有模板业务、磁盘或检测算法。
#include "ui/widgets/image_label.h"

#include <QColor>
#include <QPainter>
#include <QPen>

namespace {

ImageLabel::DrawingStep initialDrawingStep(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp:
        return ImageLabel::DrawingStep::StampAnchor;
    case DetectionMode::Word:
    case DetectionMode::Ocr:
    case DetectionMode::BarcodeWord:
        return ImageLabel::DrawingStep::TrackingAnchor;
    case DetectionMode::Tissue:
        return ImageLabel::DrawingStep::Idle;
    }
    return ImageLabel::DrawingStep::Idle;
}

}

ImageLabel::ImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void ImageLabel::setPixmap(const QPixmap &pixmap)
{
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::setPixmap(pixmap);
}

void ImageLabel::setAutoFitPixmap(const QPixmap &pixmap)
{
    m_autoFitSourcePixmap = pixmap;
    m_autoFitPixmapEnabled = !pixmap.isNull();
    updateAutoFitPixmap();
}

void ImageLabel::clear()
{
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::clear();
}

void ImageLabel::updateAutoFitPixmap()
{
    if (!m_autoFitPixmapEnabled || m_autoFitSourcePixmap.isNull()
            || width() <= 0 || height() <= 0) {
        return;
    }

    QLabel::setPixmap(
                m_autoFitSourcePixmap.scaled(
                    size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
}

void ImageLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateAutoFitPixmap();
}

QRect *ImageLabel::activeRect()
{
    switch (m_drawingStep) {
    case DrawingStep::TrackingAnchor:
    case DrawingStep::DateAnchor:
        return &m_trackingAnchorRect;
    case DrawingStep::BarcodeRegion:
        return &m_barcodeRect;
    case DrawingStep::StampAnchor:
        return &m_stampAnchorRect;
    default:
        return nullptr;
    }
}

QPolygon *ImageLabel::activePolygon()
{
    switch (m_drawingStep) {
    case DrawingStep::DetectionPolygon:
    case DrawingStep::DatePolygon:
        return &m_datePolygon;
    case DrawingStep::StampPolygon:
        return &m_stampPolygon;
    default:
        return nullptr;
    }
}

void ImageLabel::clearTemplateGeometry()
{
    m_trackingAnchorRect = QRect();
    m_barcodeRect = QRect();
    m_stampAnchorRect = QRect();
    m_datePolygon.clear();
    m_stampPolygon.clear();
    m_tempPolyPoint = QPoint();
    m_isInteracting = false;
}

void ImageLabel::beginTemplateDrawing(DetectionMode mode)
{
    m_templateDrawingMode = mode;
    clearTemplateGeometry();
    m_drawingStep = initialDrawingStep(mode);
    if (m_drawingStep != DrawingStep::Idle) {
        setFocus(Qt::OtherFocusReason);
        emitStepChanged(m_drawingStep, DrawingEvent::StepStarted);
    }
    update();
}

void ImageLabel::cancelTemplateDrawing()
{
    clearTemplateGeometry();
    m_drawingStep = DrawingStep::Idle;
    update();
}

DetectionMode ImageLabel::templateDrawingMode() const
{
    return m_templateDrawingMode;
}

bool ImageLabel::isTemplateDrawingEnabled() const
{
    return m_drawingStep != DrawingStep::Idle;
}

bool ImageLabel::isTemplateDrawingComplete() const
{
    return m_drawingStep == DrawingStep::Complete;
}

QRect ImageLabel::trackingAnchorRect() const
{
    return m_trackingAnchorRect;
}

QRect ImageLabel::barcodeRect() const
{
    return m_barcodeRect;
}

QRect ImageLabel::stampAnchorRect() const
{
    return m_stampAnchorRect;
}

QPolygon ImageLabel::datePolygon() const
{
    return m_datePolygon;
}

QPolygon ImageLabel::stampPolygon() const
{
    return m_stampPolygon;
}

void ImageLabel::retryBarcodeRegion()
{
    if (m_templateDrawingMode != DetectionMode::BarcodeWord
            || m_trackingAnchorRect.isNull()) {
        clearTemplateGeometry();
        m_drawingStep = initialDrawingStep(m_templateDrawingMode);
        if (m_drawingStep != DrawingStep::Idle) {
            emitStepChanged(m_drawingStep, DrawingEvent::Reset);
            setFocus(Qt::OtherFocusReason);
        }
        update();
        return;
    }

    m_barcodeRect = QRect();
    m_datePolygon.clear();
    m_tempPolyPoint = QPoint();
    m_isInteracting = false;
    m_drawingStep = DrawingStep::BarcodeRegion;
    emitStepChanged(m_drawingStep, DrawingEvent::StepStarted);
    update();
}

void ImageLabel::emitStepChanged(
        DrawingStep step, DrawingEvent event, int pointCount)
{
    emit templateDrawingChanged(step, event, pointCount);
}

void ImageLabel::advanceStep(int pointCount)
{
    const DrawingStep completedStep = m_drawingStep;
    switch (m_templateDrawingMode) {
    case DetectionMode::Word:
    case DetectionMode::Ocr:
        if (m_drawingStep == DrawingStep::TrackingAnchor) {
            m_drawingStep = DrawingStep::DetectionPolygon;
        } else if (m_drawingStep == DrawingStep::DetectionPolygon) {
            m_drawingStep = DrawingStep::Complete;
        }
        break;
    case DetectionMode::BarcodeWord:
        if (m_drawingStep == DrawingStep::TrackingAnchor) {
            m_drawingStep = DrawingStep::BarcodeRegion;
        } else if (m_drawingStep == DrawingStep::BarcodeRegion) {
            m_drawingStep = DrawingStep::DatePolygon;
        } else if (m_drawingStep == DrawingStep::DatePolygon) {
            m_drawingStep = DrawingStep::Complete;
        }
        break;
    case DetectionMode::Stamp:
        if (m_drawingStep == DrawingStep::StampAnchor) {
            m_drawingStep = DrawingStep::StampPolygon;
        } else if (m_drawingStep == DrawingStep::StampPolygon) {
            m_drawingStep = DrawingStep::DateAnchor;
        } else if (m_drawingStep == DrawingStep::DateAnchor) {
            m_drawingStep = DrawingStep::DatePolygon;
        } else if (m_drawingStep == DrawingStep::DatePolygon) {
            m_drawingStep = DrawingStep::Complete;
        }
        break;
    case DetectionMode::Tissue:
        m_drawingStep = DrawingStep::Idle;
        break;
    }

    const DrawingStep followingStep = m_drawingStep;
    // 二维码完成回调可能同步要求重画，不能再发送已经过期的下一步事件。
    emitStepChanged(completedStep, DrawingEvent::StepCompleted, pointCount);
    if (m_drawingStep != followingStep) {
        update();
        return;
    }
    if (m_drawingStep == DrawingStep::Complete) {
        emitStepChanged(
                    DrawingStep::Complete,
                    DrawingEvent::WorkflowCompleted,
                    pointCount);
    } else if (m_drawingStep != DrawingStep::Idle) {
        emitStepChanged(m_drawingStep, DrawingEvent::StepStarted);
    }
    update();
}

void ImageLabel::finishCurrentStep()
{
    if (QRect *rect = activeRect()) {
        *rect = rect->normalized();
        if (rect->width() <= 5 || rect->height() <= 5) {
            *rect = QRect();
            emitStepChanged(m_drawingStep, DrawingEvent::RegionTooSmall);
            update();
            return;
        }
        advanceStep(0);
        return;
    }

    QPolygon *polygon = activePolygon();
    if (!polygon) {
        return;
    }
    if (polygon->size() < 3) {
        emitStepChanged(
                    m_drawingStep,
                    DrawingEvent::TooFewPoints,
                    polygon->size());
        update();
        return;
    }
    const int pointCount = polygon->size();
    m_tempPolyPoint = QPoint();
    advanceStep(pointCount);
}

void ImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (m_drawingStep == DrawingStep::Idle
            || m_drawingStep == DrawingStep::Complete) {
        QLabel::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);
    if (QRect *rect = activeRect()) {
        if (event->button() == Qt::LeftButton) {
            m_isInteracting = true;
            m_startPoint = event->pos();
            *rect = QRect(m_startPoint, m_startPoint);
            event->accept();
            update();
            return;
        }
    } else if (QPolygon *polygon = activePolygon()) {
        if (event->button() == Qt::LeftButton) {
            *polygon << event->pos();
            m_tempPolyPoint = event->pos();
            emitStepChanged(
                        m_drawingStep,
                        DrawingEvent::PointAdded,
                        polygon->size());
            event->accept();
            update();
            return;
        }
        if (event->button() == Qt::RightButton) {
            finishCurrentStep();
            event->accept();
            return;
        }
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isInteracting) {
        if (QRect *rect = activeRect()) {
            rect->setBottomRight(event->pos());
            event->accept();
            update();
            return;
        }
    }
    const QPolygon *polygon = activePolygon();
    if (polygon && !polygon->isEmpty()) {
        m_tempPolyPoint = event->pos();
        event->accept();
        update();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isInteracting
            && activeRect()) {
        m_isInteracting = false;
        finishCurrentStep();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void ImageLabel::keyPressEvent(QKeyEvent *event)
{
    if (m_drawingStep != DrawingStep::Idle
            && event->key() == Qt::Key_Escape) {
        clearTemplateGeometry();
        m_drawingStep = initialDrawingStep(m_templateDrawingMode);
        if (m_drawingStep != DrawingStep::Idle) {
            emitStepChanged(m_drawingStep, DrawingEvent::Reset);
            setFocus(Qt::OtherFocusReason);
        }
        update();
        event->accept();
        return;
    }
    QLabel::keyPressEvent(event);
}

void ImageLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto drawRect = [&painter](const QRect &rect,
                                    const QColor &color) {
        if (rect.isNull()) {
            return;
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 3, Qt::SolidLine));
        painter.drawRect(rect);
    };
    const auto drawPolygon = [this, &painter](
            const QPolygon &polygon,
            const QColor &color,
            DrawingStep activeStep) {
        if (polygon.isEmpty()) {
            return;
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 3, Qt::SolidLine));
        if (m_drawingStep == activeStep) {
            painter.drawPolyline(polygon);
            painter.setPen(QPen(color, 2, Qt::DashLine));
            painter.drawLine(polygon.last(), m_tempPolyPoint);
            painter.drawLine(m_tempPolyPoint, polygon.first());
        } else {
            painter.drawPolygon(polygon);
        }
        painter.setPen(QPen(color, 1, Qt::SolidLine));
        painter.setBrush(color);
        for (const QPoint &point : polygon) {
            painter.drawEllipse(point, 4, 4);
        }
    };

    const QColor trackingColor(Qt::blue);
    const QColor barcodeColor(Qt::yellow);
    const QColor stampAnchorColor(255, 140, 0);
    const QColor stampPolygonColor(255, 69, 0);
    const QColor datePolygonColor(Qt::green);

    switch (m_templateDrawingMode) {
    case DetectionMode::Word:
    case DetectionMode::Ocr:
        drawRect(m_trackingAnchorRect, trackingColor);
        drawPolygon(m_datePolygon, datePolygonColor,
                    DrawingStep::DetectionPolygon);
        break;
    case DetectionMode::BarcodeWord:
        drawRect(m_trackingAnchorRect, trackingColor);
        drawRect(m_barcodeRect, barcodeColor);
        drawPolygon(m_datePolygon, datePolygonColor,
                    DrawingStep::DatePolygon);
        break;
    case DetectionMode::Stamp:
        drawRect(m_stampAnchorRect, stampAnchorColor);
        drawPolygon(m_stampPolygon, stampPolygonColor,
                    DrawingStep::StampPolygon);
        drawRect(m_trackingAnchorRect, trackingColor);
        drawPolygon(m_datePolygon, datePolygonColor,
                    DrawingStep::DatePolygon);
        break;
    case DetectionMode::Tissue:
        break;
    }
}
