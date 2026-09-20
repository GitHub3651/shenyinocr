#include "widget.h"
#include "ui_widget.h"
#include "contracts/detection_mode.h"
#include "system_support/license/license_codec.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>

namespace {

QString modeDisplayName(const QString &modeId)
{
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromId(modeId);
    return descriptor
            ? QString::fromUtf8(descriptor->displayName)
            : modeId;
}

QString licenseInfoText(const QString &filePath, QString *errorMessage)
{
    const LicenseDecodeResult result = LicenseCodec::readFile(filePath);
    if (!result.succeeded()) {
        if (errorMessage) {
            *errorMessage =
                    result.status == LicenseCodecStatus::FileReadFailed
                    ? QStringLiteral("文件读取失败或内容为空。")
                    : QStringLiteral("文件格式无效。");
        }
        return QString();
    }

    QStringList featureNames;
    for (const QString &modeId : result.license.featureModeIds) {
        featureNames.append(modeDisplayName(modeId));
    }

    const QString expires = result.license.permanent
            ? QStringLiteral("长期有效")
            : result.license.expiresDate.toString(
                QStringLiteral("yyyy-MM-dd"));
    const QString dateStatus = result.license.permanent
            ? QStringLiteral("长期有效")
            : (QDate::currentDate() <= result.license.expiresDate
               ? QStringLiteral("有效")
               : QStringLiteral("已超过"));
    const bool unactivated =
            result.license.deviceBinding == QLatin1String("UNACTIVATED")
            && result.license.deviceCode == QLatin1String("UNACTIVATED");
    const QString bindingMethod = unactivated
            ? QStringLiteral("未激活")
            : result.license.deviceBinding;

    QString text;
    text += QStringLiteral("文件路径：")
            + QDir::toNativeSeparators(filePath) + QLatin1Char('\n');
    text += QStringLiteral("有效期：") + expires + QLatin1Char('\n');
    text += QStringLiteral("有效期状态：") + dateStatus + QLatin1Char('\n');
    text += QStringLiteral("授权功能：")
            + featureNames.join(QStringLiteral("、")) + QLatin1Char('\n');
    text += QStringLiteral("默认模式：")
            + modeDisplayName(result.license.defaultModeId)
            + QLatin1Char('\n');
    text += QStringLiteral("设备绑定方式：")
            + bindingMethod + QLatin1Char('\n');
    text += QStringLiteral("设备绑定状态：")
            + (unactivated ? QStringLiteral("未激活")
                           : QStringLiteral("已激活"));
    return text;
}

} // namespace

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->outputEdit->setText(defaultLicensePath());
    ui->datEdit->setText(defaultLicensePath());
    ui->licenseInfoEdit->setReadOnly(true);
    ui->expiresTypeComboBox->addItem(
                QStringLiteral("具体日期"), QStringLiteral("date"));
    ui->expiresTypeComboBox->addItem(
                QStringLiteral("长期有效"), QStringLiteral("permanent"));
    ui->expiresEdit->setDate(QDate::currentDate().addYears(1));
    for (const DetectionModeDescriptor &descriptor
         : detectionModeDescriptors()) {
        QListWidgetItem *item = new QListWidgetItem(
                    QString::fromUtf8(descriptor.displayName),
                    ui->featuresListWidget);
        item->setData(Qt::UserRole, QLatin1String(descriptor.modeId));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    ui->deviceBindingEdit->setText(QStringLiteral("未激活"));
    ui->statusLabel->setText(
                QStringLiteral("请选择有效期、授权功能和默认模式。"));
    refreshExpiresEditor();
    refreshDefaultModes();

    connect(ui->browseOutputButton, &QPushButton::clicked,
            this, &Widget::browseOutputFile);
    connect(ui->generateButton, &QPushButton::clicked,
            this, &Widget::generateLicenseFile);
    connect(ui->browseDatButton, &QPushButton::clicked,
            this, &Widget::browseDatFile);
    connect(ui->readDatButton, &QPushButton::clicked,
            this, &Widget::readDatFile);
    connect(ui->expiresTypeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshExpiresEditor(); });
    connect(ui->featuresListWidget, &QListWidget::itemChanged,
            this, [this](QListWidgetItem *) { refreshDefaultModes(); });
}

Widget::~Widget()
{
    delete ui;
}

void Widget::browseOutputFile()
{
    const QString filePath = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("选择输出文件"),
                ui->outputEdit->text(),
                QStringLiteral("INI Files (*.ini);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->outputEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::generateLicenseFile()
{
    const QString outputPath = ui->outputEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请选择输出文件。"));
        return;
    }

    const QStringList features = selectedFeatureModeIds();
    if (features.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请至少选择一个授权功能。"));
        return;
    }
    const QString defaultModeId =
            ui->defaultModeComboBox->currentData().toString();
    if (defaultModeId.isEmpty() || !features.contains(defaultModeId)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请选择授权功能中的默认模式。"));
        return;
    }

    LicenseData license;
    license.permanent = ui->expiresTypeComboBox->currentData().toString()
            == QLatin1String("permanent");
    license.expiresDate = ui->expiresEdit->date();
    license.featureModeIds = features;
    license.defaultModeId = defaultModeId;
    license.deviceBinding = QStringLiteral("UNACTIVATED");
    license.deviceCode = QStringLiteral("UNACTIVATED");
    if (LicenseCodec::writeFile(license, outputPath)
            != LicenseCodecStatus::Success) {
        const QString errorMessage = QStringLiteral("写入文件失败。");
        QMessageBox::critical(
                    this, QStringLiteral("提示"), errorMessage);
        ui->statusLabel->setText(errorMessage);
        return;
    }

    ui->statusLabel->setText(QStringLiteral("license.ini 已生成。"));
    ui->datEdit->setText(outputPath);
    readDatFile();
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("生成完成。"));
}

void Widget::browseDatFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择 license.ini 文件"),
                QFileInfo(ui->datEdit->text()).absolutePath(),
                QStringLiteral("INI Files (*.ini);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->datEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::readDatFile()
{
    const QString filePath = ui->datEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请选择文件。"));
        return;
    }

    QString errorMessage;
    const QString text = licenseInfoText(filePath, &errorMessage);
    if (text.isEmpty()) {
        ui->licenseInfoEdit->clear();
        QMessageBox::critical(
                    this, QStringLiteral("提示"), errorMessage);
        return;
    }

    ui->licenseInfoEdit->setPlainText(text);
}

void Widget::refreshExpiresEditor()
{
    ui->expiresEdit->setEnabled(
                ui->expiresTypeComboBox->currentData().toString()
                == QLatin1String("date"));
}

void Widget::refreshDefaultModes()
{
    const QString previousModeId =
            ui->defaultModeComboBox->currentData().toString();
    const QStringList features = selectedFeatureModeIds();
    ui->defaultModeComboBox->clear();
    for (const DetectionModeDescriptor &descriptor
         : detectionModeDescriptors()) {
        const QString modeId = QLatin1String(descriptor.modeId);
        if (features.contains(modeId)) {
            ui->defaultModeComboBox->addItem(
                        QString::fromUtf8(descriptor.displayName), modeId);
        }
    }
    const int previousIndex =
            ui->defaultModeComboBox->findData(previousModeId);
    if (previousIndex >= 0) {
        ui->defaultModeComboBox->setCurrentIndex(previousIndex);
    }
}

QStringList Widget::selectedFeatureModeIds() const
{
    QStringList selected;
    for (int index = 0;
         index < ui->featuresListWidget->count();
         ++index) {
        const QListWidgetItem *item =
                ui->featuresListWidget->item(index);
        if (item->checkState() == Qt::Checked) {
            selected.append(item->data(Qt::UserRole).toString());
        }
    }
    return selected;
}

QString Widget::defaultLicensePath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("license.ini"));
}
