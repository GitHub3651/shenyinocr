#ifndef WIDGET_H
#define WIDGET_H

#include <QString>
#include <QStringList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    void browseOutputFile();
    void generateLicenseFile();
    void browseDatFile();
    void readDatFile();
    void refreshExpiresEditor();
    void refreshDefaultModes();
    QStringList selectedFeatureModeIds() const;
    QString defaultLicensePath() const;

private:
    Ui::Widget *ui;
};

#endif // WIDGET_H
