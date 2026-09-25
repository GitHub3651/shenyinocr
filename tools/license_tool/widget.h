#pragma once

#include <QStringList>
#include <QWidget>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private:
    void generateActivationCode();
    void copyActivationCode();
    void inspectActivationCode();
    void inspectLicenseFile();
    void refreshRequestStatus();
    void refreshExpiresEditor();
    void refreshDefaultModes();
    QStringList selectedFeatureModeIds() const;

    Ui::Widget *ui;
};
