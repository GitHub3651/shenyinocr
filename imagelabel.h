#ifndef IMAGELABEL_H
#define IMAGELABEL_H

#include <QLabel>
#include <QRect>
#include <QVector>
#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPolygonF>

class ImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ImageLabel(QWidget *parent = nullptr);

    // ================= 原有的函数声明（原封不动恢复，防报错） =================
    void setColor(int color);
    void addSelectionRect(const QRect &rect, int color);
    QRect getSelectionRect() const;
    void clearSelection();
    void setSelectionRect(const QRect &rect);
    void setStartPoint(const QPoint &point);
    QPoint getStartPoint() const;
    void setDrawing(bool draw);
    void clearGreenRects();
    void clearredRects();
    void clearblueRects();
    void addSelectionPolygon(const QPolygonF &polygon, int color);
    bool isDrawing() const;

    bool allowBlueDraw = false;
    bool blueRectRedrawn = false;
    int rectAdded = 0;

    // 重写setPixmap
    void setPixmap(const QPixmap &pixmap);

    // ================= 🔥 新增：双框追踪专用接口 =================
    QPolygon getDetectionPoly() const { return m_detectionPoly; }
    QRect getTrackingRect() const { return m_trackingRect; }
    bool isDetectionPolyComplete() const;
    void setTemplateDrawingEnabled(bool enabled);
    bool isTemplateDrawingEnabled() const;
    void resetDrawingStep();

signals:
    void mousePressed(QMouseEvent *event);
    void mouseMoved(QMouseEvent *event);
    void mouseReleased(QMouseEvent *event);

    // 🔥 新增：发送文本提示信号给 Widget
    void signal_hintMessage(QString msg);
    void signal_templateGuideEvent(QString eventName, int pointCount);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    struct ColoredRect {
        QRect rect;
        int color;
    };

    // ================= 原有变量（恢复） =================
    bool drawing = false;
    QPoint startPoint;
    QRect selectionRect;
    QRect selectionRect1;
    QList<QRect> redRects;
    QList<QRect> greenRects;
    QList<QRect> blueRects;
    QList<QRect> yellowRects;
    QVector<QPolygonF> redPolygons;
    QVector<QPolygonF> greenPolygons;
    QVector<QPolygonF> bluePolygons;
    QVector<ColoredRect> rectangles;
    int m_color = 1;

    // ================= 🔥 新增左键画框状态机 =================
    enum DrawStep {
        STEP_TRACKING,        // tracking box 
        STEP_DETECTION_POLY,  // detection poly 
        STEP_DONE             // done 
    };
    DrawStep m_currentStep = STEP_TRACKING;

    QPolygon m_detectionPoly;
    QPoint m_tempPolyPoint;
    QRect m_trackingRect;
    bool m_isInteracting = false;
    bool m_templateDrawingEnabled = false;
    QPoint m_startPoint;
};

#endif // IMAGELABEL_H
