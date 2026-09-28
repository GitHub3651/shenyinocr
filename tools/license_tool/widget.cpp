#include "widget.h"
#include "ui_widget.h"
#include "contracts/detection_mode.h"
#include "system_support/license/license_codec.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
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

void refreshDayOptions(QComboBox *yearComboBox,
                       QComboBox *monthComboBox,
                       QComboBox *dayComboBox)
{
    const int previousDay = dayComboBox->currentData().toInt();
    const int daysInMonth = QDate(
                yearComboBox->currentData().toInt(),
                monthComboBox->currentData().toInt(),
                1).daysInMonth();
    dayComboBox->clear();
    for (int day = 1; day <= daysInMonth; ++day) {
        dayComboBox->addItem(
                    QStringLiteral("%1 日").arg(
                        day, 2, 10, QLatin1Char('0')),
                    day);
    }
    const int selectedDay = previousDay > 0
            ? qMin(previousDay, daysInMonth)
            : 1;
    dayComboBox->setCurrentIndex(
                dayComboBox->findData(selectedDay));
}

void initializeDateOptions(QComboBox *yearComboBox,
                           QComboBox *monthComboBox,
                           QComboBox *dayComboBox,
                           const QDate &date,
                           int firstYear,
                           int lastYear)
{
    for (int year = firstYear; year <= lastYear; ++year) {
        yearComboBox->addItem(
                    QStringLiteral("%1 年").arg(year), year);
    }
    for (int month = 1; month <= 12; ++month) {
        monthComboBox->addItem(
                    QStringLiteral("%1 月").arg(
                        month, 2, 10, QLatin1Char('0')),
                    month);
    }
    yearComboBox->setCurrentIndex(
                yearComboBox->findData(date.year()));
    monthComboBox->setCurrentIndex(
                monthComboBox->findData(date.month()));
    refreshDayOptions(yearComboBox, monthComboBox, dayComboBox);
    dayComboBox->setCurrentIndex(
                dayComboBox->findData(date.day()));
}

QDate selectedDate(QComboBox *yearComboBox,
                   QComboBox *monthComboBox,
                   QComboBox *dayComboBox)
{
    return QDate(yearComboBox->currentData().toInt(),
                 monthComboBox->currentData().toInt(),
                 dayComboBox->currentData().toInt());
}

QString licenseInfoText(const QString &filePath, QString &errorMessage)
{
    const LicenseDecodeResult result = LicenseCodec::readFile(filePath);
    if (result.status != LicenseCodecStatus::Success) {
        errorMessage = result.status == LicenseCodecStatus::FileReadFailed
                ? QStringLiteral("无法读取许可证文件，"
                                 "请检查文件是否存在及是否可访问。")
                : QStringLiteral("许可证文件格式无效或内容已损坏。");
        return QString();
    }

    QStringList featureNames;
    for (const QString &modeId : result.license.featureModeIds) {
        featureNames.append(
                    QString::fromUtf8(
                        detectionModeDescriptorFromId(modeId)->displayName));
    }

    const QString expires = result.license.permanent
            ? QStringLiteral("长期有效")
            : result.license.expiresDate.toString(
                QStringLiteral("yyyy-MM-dd"));
    QString dateStatus = QStringLiteral("长期有效");
    if (!result.license.permanent) {
        const QDate currentDate = QDateTime::currentDateTimeUtc()
                .addSecs(8 * 60 * 60).date();
        if (currentDate < result.license.issuedDate) {
            dateStatus = QStringLiteral("未到签发日期");
        } else if (currentDate <= result.license.expiresDate) {
            dateStatus = QStringLiteral("有效");
        } else {
            dateStatus = QStringLiteral("已超过");
        }
    }
    const bool unactivated =
            result.license.deviceBinding == QLatin1String("UNACTIVATED");
    const QString bindingMethod = unactivated
            ? QStringLiteral("未激活")
            : result.license.deviceBinding;

    QString text;
    text += QStringLiteral("文件路径：")
            + QDir::toNativeSeparators(filePath) + QLatin1Char('\n');
    text += QStringLiteral("签发日期：")
            + result.license.issuedDate.toString(
                QStringLiteral("yyyy-MM-dd")) + QLatin1Char('\n');
    text += QStringLiteral("有效期：") + expires + QLatin1Char('\n');
    text += QStringLiteral("有效期状态：") + dateStatus + QLatin1Char('\n');
    text += QStringLiteral("授权功能：")
            + featureNames.join(QStringLiteral("、")) + QLatin1Char('\n');
    text += QStringLiteral("默认模式：")
            + QString::fromUtf8(
                detectionModeDescriptorFromId(
                    result.license.defaultModeId)->displayName)
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
    const QDate currentDate = QDate::currentDate();
    const int firstYear = currentDate.year() - 10;
    const int lastYear = currentDate.year() + 20;
    initializeDateOptions(
                ui->issuedYearComboBox,
                ui->issuedMonthComboBox,
                ui->issuedDayComboBox,
                currentDate,
                firstYear,
                lastYear);
    initializeDateOptions(
                ui->expiresYearComboBox,
                ui->expiresMonthComboBox,
                ui->expiresDayComboBox,
                currentDate.addYears(1),
                firstYear,
                lastYear);
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
    connect(ui->issuedYearComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        refreshDayOptions(ui->issuedYearComboBox,
                          ui->issuedMonthComboBox,
                          ui->issuedDayComboBox);
    });
    connect(ui->issuedMonthComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        refreshDayOptions(ui->issuedYearComboBox,
                          ui->issuedMonthComboBox,
                          ui->issuedDayComboBox);
    });
    connect(ui->expiresYearComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        refreshDayOptions(ui->expiresYearComboBox,
                          ui->expiresMonthComboBox,
                          ui->expiresDayComboBox);
    });
    connect(ui->expiresMonthComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        refreshDayOptions(ui->expiresYearComboBox,
                          ui->expiresMonthComboBox,
                          ui->expiresDayComboBox);
    });
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
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("请选择输出文件。"));
        return;
    }

    const bool permanent =
            ui->expiresTypeComboBox->currentData().toString()
            == QLatin1String("permanent");
    const QDate issuedDate = selectedDate(
                ui->issuedYearComboBox,
                ui->issuedMonthComboBox,
                ui->issuedDayComboBox);
    const QDate expiresDate = selectedDate(
                ui->expiresYearComboBox,
                ui->expiresMonthComboBox,
                ui->expiresDayComboBox);
    if (!permanent && expiresDate < issuedDate) {
        const QString errorMessage =
                QStringLiteral("有效期不能早于签发日期，请重新选择。");
        QMessageBox::warning(
                    this, QStringLiteral("日期错误"), errorMessage);
        ui->statusLabel->setText(errorMessage);
        return;
    }

    const QStringList features = selectedFeatureModeIds();
    if (features.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("请至少选择一个授权功能。"));
        return;
    }
    const QString defaultModeId =
            ui->defaultModeComboBox->currentData().toString();

    LicenseData license;
    license.permanent = permanent;
    license.issuedDate = issuedDate;
    license.expiresDate = expiresDate;
    license.featureModeIds = features;
    license.defaultModeId = defaultModeId;
    license.deviceBinding = QStringLiteral("UNACTIVATED");
    license.deviceCode = QStringLiteral("UNACTIVATED");
    if (LicenseCodec::writeFile(license, outputPath)
            != LicenseCodecStatus::Success) {
        const QString errorMessage =
                QStringLiteral("无法写入许可证文件，"
                               "请检查输出路径、文件权限或文件是否正被占用。");
        QMessageBox::critical(
                    this, QStringLiteral("生成失败"), errorMessage);
        ui->statusLabel->setText(errorMessage);
        return;
    }

    ui->statusLabel->setText(QStringLiteral("license.ini 已生成。"));
    ui->datEdit->setText(outputPath);
    readDatFile();
    QMessageBox::information(this, QStringLiteral("生成完成"),
                             QStringLiteral("许可证文件已生成。"));
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
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("请选择文件。"));
        return;
    }

    QString errorMessage;
    const QString text = licenseInfoText(filePath, errorMessage);
    if (text.isEmpty()) {
        ui->licenseInfoEdit->clear();
        QMessageBox::critical(
                    this, QStringLiteral("读取失败"), errorMessage);
        return;
    }

    ui->licenseInfoEdit->setPlainText(text);
}

void Widget::refreshExpiresEditor()
{
    const bool enabled =
            ui->expiresTypeComboBox->currentData().toString()
            == QLatin1String("date");
    ui->expiresYearComboBox->setEnabled(enabled);
    ui->expiresMonthComboBox->setEnabled(enabled);
    ui->expiresDayComboBox->setEnabled(enabled);
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
