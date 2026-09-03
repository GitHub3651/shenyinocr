#ifndef OCRGANGYIN_UI_INSPECTION_IMAGE_CANVAS_H
// 文件作用：显示检测图像，并处理模板制作时的模式化矩形、多边形和鼠标键盘交互。
// 模块位置：界面层；只保存显示坐标和绘制状态，不拥有模板业务、磁盘或检测算法。
#define OCRGANGYIN_UI_INSPECTION_IMAGE_CANVAS_H

#include "contracts/detection_mode.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPixmap>
#include <QPolygon>
#include <QRect>
#include <QResizeEvent>

class InspectionImageCanvas : public QLabel
{
    Q_OBJECT

public:
    enum class DrawingStep {
        Idle,
        TrackingAnchor,
        DetectionPolygon,
        BarcodeRegion,
        StampAnchor,
        StampPolygon,
        DateAnchor,
        DatePolygon,
        Complete
    };
    Q_ENUM(DrawingStep)

    enum class DrawingEvent {
        StepStarted,
        StepCompleted,
        RegionTooSmall,
        PointAdded,
        TooFewPoints,
        Reset,
        WorkflowCompleted
    };
    Q_ENUM(DrawingEvent)

    explicit InspectionImageCanvas(QWidget *parent = nullptr);

    void setPixmap(const QPixmap &pixmap);
    void setAutoFitPixmap(const QPixmap &pixmap);
    void clear();

    void beginTemplateDrawing(DetectionMode mode);
    void cancelTemplateDrawing();
    DetectionMode templateDrawingMode() const;
    bool isTemplateDrawingEnabled() const;
    bool isTemplateDrawingComplete() const;

    QRect trackingAnchorRect() const;
    QRect barcodeRect() const;
    QRect stampAnchorRect() const;
    QPolygon datePolygon() const;
    QPolygon stampPolygon() const;

    void retryBarcodeRegion();

signals:
    void templateDrawingChanged(
        InspectionImageCanvas::DrawingStep step,
        InspectionImageCanvas::DrawingEvent event,
        int pointCount);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRectF *activeRect();
    QPolygonF *activePolygon();
    void clearTemplateGeometry();
    void finishCurrentStep();
    void advanceStep(int pointCount);
    void emitStepChanged(DrawingStep step,
                         DrawingEvent event,
                         int pointCount = 0);
    void updateAutoFitPixmap();

    DrawingStep m_drawingStep = DrawingStep::Idle;
    DetectionMode m_templateDrawingMode = DetectionMode::Tissue;
    QRectF m_trackingAnchorRect;
    QRectF m_barcodeRect;
    QRectF m_stampAnchorRect;
    QPolygonF m_datePolygon;
    QPolygonF m_stampPolygon;
    QPointF m_tempPolyPoint;
    bool m_isInteracting = false;
    QPointF m_startPoint;
    QPixmap m_autoFitSourcePixmap;
    bool m_autoFitPixmapEnabled = false;
};

#endif // OCRGANGYIN_UI_INSPECTION_IMAGE_CANVAS_H
