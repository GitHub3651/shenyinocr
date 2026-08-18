// 文件作用：本文件用于显示检测图像，并处理模板制作时的矩形、多边形和鼠标键盘绘制交互。
// 主要职责：显示检测图像，并处理模板制作时的矩形、多边形和鼠标键盘绘制交互。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/widgets/image_label.h"
#include <QPainter>
#include <QPen>

// 🔥 仿照 ui/main_window.cpp，加入这句声明，彻底解决底层发送的中文乱码问题
#pragma execution_character_set("utf-8")

// 函数说明：ImageLabel 构造函数创建组件并初始化其依赖和初始状态。
ImageLabel::ImageLabel(QWidget *parent) : QLabel(parent) {
    m_currentStep = STEP_TRACKING;
    setFocusPolicy(Qt::StrongFocus);
}

// 函数说明：setPixmap 函数更新或应用对应的配置和状态。
void ImageLabel::setPixmap(const QPixmap &pixmap) {
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::setPixmap(pixmap);
}

// 函数说明：setAutoFitPixmap 函数更新或应用对应的配置和状态。
void ImageLabel::setAutoFitPixmap(const QPixmap &pixmap) {
    m_autoFitSourcePixmap = pixmap;
    m_autoFitPixmapEnabled = !pixmap.isNull();
    updateAutoFitPixmap();
}

// 函数说明：clear 函数停止流程、清理状态或释放对应资源。
void ImageLabel::clear() {
    m_autoFitPixmapEnabled = false;
    m_autoFitSourcePixmap = QPixmap();
    QLabel::clear();
}

// 函数说明：updateAutoFitPixmap 函数更新或应用对应的配置和状态。
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

// 函数说明：resizeEvent 函数实现名称所表示的处理步骤。
void ImageLabel::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    updateAutoFitPixmap();
}

// 函数说明：isDetectionPolyComplete 函数检查相关状态并返回判断结果。
bool ImageLabel::isDetectionPolyComplete() const {
    return m_currentStep == STEP_DONE && m_detectionPoly.size() >= 3;
}

// 函数说明：setTemplateDrawingEnabled 函数更新或应用对应的配置和状态。
void ImageLabel::setTemplateDrawingEnabled(bool enabled) {
    m_templateDrawingEnabled = enabled;
    if (!m_templateDrawingEnabled) {
        m_isInteracting = false;
    } else {
        setFocus(Qt::OtherFocusReason);
    }
}

// 函数说明：isTemplateDrawingEnabled 函数检查相关状态并返回判断结果。
bool ImageLabel::isTemplateDrawingEnabled() const {
    return m_templateDrawingEnabled;
}

// 函数说明：setBarcodeRegionRequired 函数更新或应用对应的配置和状态。
void ImageLabel::setBarcodeRegionRequired(bool required) {
    if (m_barcodeRegionRequired == required) {
        return;
    }
    m_barcodeRegionRequired = required;
    resetDrawingStep();
}

// 函数说明：retryBarcodeRegion 函数实现名称所表示的处理步骤。
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
    m_isInteracting = false;
    update();
}

// 函数说明：clearSelection 函数停止流程、清理状态或释放对应资源。
void ImageLabel::clearSelection() {
    resetDrawingStep();
}

// 函数说明：mousePressEvent 函数实现名称所表示的处理步骤。
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

// 函数说明：mouseMoveEvent 函数实现名称所表示的处理步骤。
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

// 函数说明：mouseReleaseEvent 函数实现名称所表示的处理步骤。
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

// 函数说明：keyPressEvent 函数实现名称所表示的处理步骤。
void ImageLabel::keyPressEvent(QKeyEvent *event) {
    if (m_templateDrawingEnabled && event->key() == Qt::Key_Escape) {
        resetDrawingStep();
        emit signal_templateGuideEvent("template_reset", 0);
        event->accept();
        return;
    }

    QLabel::keyPressEvent(event);
}

// 函数说明：paintEvent 函数实现名称所表示的处理步骤。
void ImageLabel::paintEvent(QPaintEvent *event) {
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

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
