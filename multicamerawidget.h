#ifndef MULTICAMERAWIDGET_H
#define MULTICAMERAWIDGET_H

#include <QWidget>

namespace Ui {
class MultiCameraWidget;
}

class MultiCameraWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MultiCameraWidget(QWidget *parent = nullptr);
    ~MultiCameraWidget();

private:
    Ui::MultiCameraWidget *ui;

    void initCameraList();
};

#endif // MULTICAMERAWIDGET_H
