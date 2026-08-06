#include "imagelabel.h"
#include <QPainter>
#include <QPen>

// 🔥 仿照 widget.cpp，加入这句声明，彻底解决底层发送的中文乱码问题
#pragma execution_character_set("utf-8")

ImageLabel::ImageLabel(QWidget *parent) : QLabel(parent) {
    m_currentStep = STEP_TRACKING;
    setFocusPolicy(Qt::StrongFocus);
}

void ImageLabel::setPixmap(const QPixmap &pixmap) {
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::setPixmap(pixmap);
}

void ImageLabel::setAutoFitPixmap(const QPixmap &pixmap) {
    m_autoFitSourcePixmap = pixmap;
    m_autoFitPixmapEnabled = !pixmap.isNull();
    updateAutoFitPixmap();
}

void ImageLabel::clear() {
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::clear();
}

void ImageLabel::updateAutoFitPixmap() {
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

void ImageLabel::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    updateAutoFitPixmap();
}

// =====================================================================
// 恢复你原来丢失的函数实现（保持兼容）
// =====================================================================
void ImageLabel::setColor(int color) { m_color = color; }

void ImageLabel::addSelectionRect(const QRect &rect, int color) {
    rectangles.append({rect, color});
    update();
}

QRect ImageLabel::getSelectionRect() const { return selectionRect; }

void ImageLabel::setSelectionRect(const QRect &rect) {
    selectionRect = rect;
    update();
}

void ImageLabel::setStartPoint(const QPoint &point) { startPoint = point; }

QPoint ImageLabel::getStartPoint() const { return startPoint; }

void ImageLabel::setDrawing(bool draw) { drawing = draw; }

void ImageLabel::clearGreenRects() {
    greenRects.clear();
    update();
}

void ImageLabel::clearredRects() {
    redRects.clear();
    redPolygons.clear();
    update();
}

void ImageLabel::clearblueRects() {
    blueRects.clear();
    bluePolygons.clear();
    update();
}

void ImageLabel::addSelectionPolygon(const QPolygonF &polygon, int color) {
    if (color == 1) greenPolygons.append(polygon);
    else if (color == 2) redPolygons.append(polygon);
    else if (color == 3) bluePolygons.append(polygon);
    update();
}

bool ImageLabel::isDrawing() const { return drawing; }

bool ImageLabel::isDetectionPolyComplete() const {
    return m_currentStep == STEP_DONE && m_detectionPoly.size() >= 3;
}

void ImageLabel::setTemplateDrawingEnabled(bool enabled) {
    m_templateDrawingEnabled = enabled;
    if (!m_templateDrawingEnabled) {
        m_isInteracting = false;
    } else {
        setFocus(Qt::OtherFocusReason);
    }
}

bool ImageLabel::isTemplateDrawingEnabled() const {
    return m_templateDrawingEnabled;
}

void ImageLabel::setBarcodeRegionRequired(bool required) {
    if (m_barcodeRegionRequired == required) {
        return;
    }
    m_barcodeRegionRequired = required;
    resetDrawingStep();
}

void ImageLabel::retryBarcodeRegion() {
    if (!m_barcodeRegionRequired || m_trackingRect.isNull()) {
        resetDrawingStep();
        return;
    }

    m_currentStep = STEP_BARCODE;
    m_barcodeRect = QRect();
    m_detectionPoly.clear();
    m_tempPolyPoint = QPoint();
    m_isInteracting = false;
    update();
}

// =====================================================================
// 全左键顺序画双框交互逻辑（带中文提示，不再乱码）
// =====================================================================

void ImageLabel::resetDrawingStep() {
    m_currentStep = STEP_TRACKING;
    m_trackingRect = QRect();
    m_barcodeRect = QRect();
    m_detectionPoly.clear();
    selectionRect = QRect();
    m_isInteracting = false;
    update();
}

void ImageLabel::clearSelection() {
    selectionRect = QRect();
    selectionRect1 = QRect();
    rectangles.clear();
    resetDrawingStep();
}

void ImageLabel::mousePressEvent(QMouseEvent *event) {
    if (!m_templateDrawingEnabled) {
        emit mousePressed(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);

    if (m_currentStep == STEP_DONE && event->button() == Qt::LeftButton) {
        resetDrawingStep();
    }

    if (m_currentStep == STEP_TRACKING) {
        if (event->button() == Qt::LeftButton) {
            m_isInteracting = true;
            m_startPoint = event->pos();
            m_trackingRect = QRect(m_startPoint, m_startPoint);
            emit signal_templateGuideEvent("tracking_started", 0);
        }
    } else if (m_currentStep == STEP_BARCODE) {
        if (event->button() == Qt::LeftButton) {
            m_isInteracting = true;
            m_startPoint = event->pos();
            m_barcodeRect = QRect(m_startPoint, m_startPoint);
            emit signal_templateGuideEvent("barcode_started", 0);
        }
    } else if (m_currentStep == STEP_DETECTION_POLY) {
        if (event->button() == Qt::LeftButton) {
            m_detectionPoly << event->pos();
            m_tempPolyPoint = event->pos();
            emit signal_templateGuideEvent("poly_point_added", m_detectionPoly.size());
            update();
        } else if (event->button() == Qt::RightButton) {
            if (m_detectionPoly.size() >= 3) {
                m_currentStep = STEP_DONE;
                emit signal_templateGuideEvent("poly_done", m_detectionPoly.size());
            } else {
                emit signal_templateGuideEvent("poly_too_few", m_detectionPoly.size());
            }
            update();
        }
    }
    emit mousePressed(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event) {
    if (!m_templateDrawingEnabled) {
        emit mouseMoved(event);
        return;
    }

    if (m_currentStep == STEP_TRACKING && m_isInteracting) {
        m_trackingRect.setBottomRight(event->pos());
        update();
    } else if (m_currentStep == STEP_BARCODE && m_isInteracting) {
        m_barcodeRect.setBottomRight(event->pos());
        update();
    } else if (m_currentStep == STEP_DETECTION_POLY) {
        m_tempPolyPoint = event->pos();
        update();
    }
    emit mouseMoved(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_templateDrawingEnabled) {
        emit mouseReleased(event);
        return;
    }

    if (m_currentStep == STEP_TRACKING && event->button() == Qt::LeftButton && m_isInteracting) {
        m_isInteracting = false;
        m_trackingRect = m_trackingRect.normalized();
        if (m_trackingRect.width() > 5 && m_trackingRect.height() > 5) {
            m_currentStep = m_barcodeRegionRequired
                    ? STEP_BARCODE
                    : STEP_DETECTION_POLY;
            m_barcodeRect = QRect();
            m_detectionPoly.clear();
            emit signal_templateGuideEvent("tracking_done", 0);
        } else {
            m_trackingRect = QRect();
            emit signal_templateGuideEvent("tracking_too_small", 0);
        }
        update();
    } else if (m_currentStep == STEP_BARCODE
               && event->button() == Qt::LeftButton
               && m_isInteracting) {
        m_isInteracting = false;
        m_barcodeRect = m_barcodeRect.normalized();
        if (m_barcodeRect.width() > 5 && m_barcodeRect.height() > 5) {
            m_currentStep = STEP_DETECTION_POLY;
            m_detectionPoly.clear();
            emit signal_templateGuideEvent("barcode_done", 0);
        } else {
            m_barcodeRect = QRect();
            emit signal_templateGuideEvent("barcode_too_small", 0);
        }
        update();
    }
    emit mouseReleased(event);
}

void ImageLabel::keyPressEvent(QKeyEvent *event) {
    if (m_templateDrawingEnabled && event->key() == Qt::Key_Escape) {
        resetDrawingStep();
        emit signal_templateGuideEvent("template_reset", 0);
        event->accept();
        return;
    }

    QLabel::keyPressEvent(event);
}

void ImageLabel::paintEvent(QPaintEvent *event) {
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 恢复绘制旧的矩形（保持对原有代码的兼容）
    painter.setPen(QPen(Qt::green, 2));
    for (const QRect& r : greenRects) painter.drawRect(r);
    painter.setPen(QPen(Qt::red, 2));
    for (const QRect& r : redRects) painter.drawRect(r);
    for (const ColoredRect& cr : rectangles) {
        if(cr.color == 1) painter.setPen(QPen(Qt::green, 2));
        else if(cr.color == 2) painter.setPen(QPen(Qt::red, 2));
        else painter.setPen(QPen(Qt::blue, 2));
        painter.drawRect(cr.rect);
    }

    // 画追踪框 (蓝色粗框)
    if (!m_trackingRect.isNull()) {
        painter.setPen(QPen(Qt::blue, 3, Qt::SolidLine));
        painter.drawRect(m_trackingRect);
    }

    // 画二维码区域（黄色）
    if (!m_barcodeRect.isNull()) {
        painter.setPen(QPen(Qt::yellow, 3, Qt::SolidLine));
        painter.drawRect(m_barcodeRect);
    }

    // 画生产日期多边形 (绿色)
    if (!m_detectionPoly.isEmpty()) {
        painter.setPen(QPen(Qt::green, 3, Qt::SolidLine));
        painter.drawPolyline(m_detectionPoly);

        // 如果还没画完，画一根跟随鼠标的虚线
        if (m_currentStep == STEP_DETECTION_POLY) {
            painter.setPen(QPen(Qt::green, 2, Qt::DashLine));
            painter.drawLine(m_detectionPoly.last(), m_tempPolyPoint);
            painter.drawLine(m_tempPolyPoint, m_detectionPoly.first()); // 闭合预览
        } else if (m_currentStep == STEP_DONE) {
            painter.setPen(QPen(Qt::green, 3, Qt::SolidLine));
            painter.drawPolygon(m_detectionPoly); // 闭合
        }
        
        // 画顶点圆圈
        painter.setBrush(Qt::green);
        for (const QPoint& pt : m_detectionPoly) {
            painter.drawEllipse(pt, 4, 4);
        }
    }
}
