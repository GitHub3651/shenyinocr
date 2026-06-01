#include "multicamerawidget.h"
#include "ui_multicamerawidget.h"

#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTreeWidgetItem>

namespace {
QString utf8Text(const char *text)
{
    return QString::fromUtf8(text);
}
}

MultiCameraWidget::MultiCameraWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MultiCameraWidget)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::Window);
    setWindowTitle(utf8Text("\xE5\xA4\x9A\xE7\x9B\xB8\xE6\x9C\xBA\xE6\xA8\xA1\xE5\xBC\x8F"));
    resize(1280, 800);

    initCameraList();

    connect(ui->returnSingleCameraButton, &QPushButton::clicked, this, &QWidget::close);
}

MultiCameraWidget::~MultiCameraWidget()
{
    delete ui;
}

void MultiCameraWidget::initCameraList()
{
    ui->cameraTree->setColumnCount(3);
    ui->cameraTree->setHeaderLabels(QStringList()
                                    << utf8Text("\xE7\x9B\xB8\xE6\x9C\xBA")
                                    << utf8Text("\xE5\x9C\xA8\xE7\xBA\xBF\xE7\x8A\xB6\xE6\x80\x81")
                                    << QStringLiteral("SN"));

    QTreeWidgetItem *camera1 = new QTreeWidgetItem(ui->cameraTree);
    camera1->setText(0, QStringLiteral("Camera 1"));
    camera1->setText(1, utf8Text("\xE6\x9C\xAA\xE6\x89\xAB\xE6\x8F\x8F"));
    camera1->setText(2, QStringLiteral("--"));

    QTreeWidgetItem *camera2 = new QTreeWidgetItem(ui->cameraTree);
    camera2->setText(0, QStringLiteral("Camera 2"));
    camera2->setText(1, utf8Text("\xE6\x9C\xAA\xE6\x89\xAB\xE6\x8F\x8F"));
    camera2->setText(2, QStringLiteral("--"));

    ui->cameraTree->expandAll();
    ui->cameraTree->resizeColumnToContents(0);
}
