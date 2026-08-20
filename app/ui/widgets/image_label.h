#ifndef IMAGELABEL_H
// 文件作用：本文件用于显示检测图像，并处理模板制作时的矩形、多边形和鼠标键盘绘制交互。
// 主要职责：显示检测图像，并处理模板制作时的矩形、多边形和鼠标键盘绘制交互。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#define IMAGELABEL_H

#include <QLabel>
#include <QRect>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPolygon>
#include <QPixmap>
#include <QResizeEvent>

// 组件说明：ImageLabel 组件负责对应界面区域的显示和用户交互。
class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ImageLabel(QWidget *parent = nullptr);

    void clearSelection();

    void setPixmap(const QPixmap &pixmap);
    void setAutoFitPixmap(const QPixmap &pixmap);
    void clear();

    // ================= 模板区域绘制接口 =================
    QPolygon getDetectionPoly() const { return m_detectionPoly; }
    QRect getTrackingRect() const { return m_trackingRect; }
    QRect getBarcodeRect() const { return m_barcodeRect; }
    bool isDetectionPolyComplete() const;
    void setTemplateDrawingEnabled(bool enabled);
    bool isTemplateDrawingEnabled() const;
    void setBarcodeRegionRequired(bool required);
    void retryBarcodeRegion();
    void resetDrawingStep();

signals:
    void mousePressed(QMouseEvent *event);
    void mouseMoved(QMouseEvent *event);
    void mouseReleased(QMouseEvent *event);

    void signal_templateGuideEvent(QString eventName, int pointCount);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum DrawStep {
        STEP_TRACKING,
        STEP_BARCODE,
        STEP_DETECTION_POLY,
        STEP_DONE
    };
    DrawStep m_currentStep = STEP_TRACKING;

    QPolygon m_detectionPoly;
    QPoint m_tempPolyPoint;
    QRect m_trackingRect;
    QRect m_barcodeRect;
    bool m_barcodeRegionRequired = false;
    bool m_isInteracting = false;
    bool m_templateDrawingEnabled = false;
    QPoint m_startPoint;
    QPixmap m_autoFitSourcePixmap;
    bool m_autoFitPixmapEnabled = false;

    void updateAutoFitPixmap();
};

#endif // IMAGELABEL_H
