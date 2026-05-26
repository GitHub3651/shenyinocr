#ifndef WIDGET_H
#define WIDGET_H

#include <QString>
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
    void refreshRequestInfo();
    void browseRequestFile();
    void browseOutputFile();
    void generateLicenseFile();
    void browseDatFile();
    void readDatFile();
    QString defaultCachePath(const QString &fileName) const;
    QString requestDirOutputPath(const QString &requestPath) const;

private:
    Ui::Widget *ui;
};

#endif // WIDGET_H
