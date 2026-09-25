#include "widget.h"

#include "activation_protocol.h"
#include "ui_widget.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFileDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>

namespace {

QString modeDisplayName(const QString &modeId)
{
    for (const LicenseToolModeDescriptor &descriptor
         : licenseToolModeDescriptors()) {
        if (modeId == QLatin1String(descriptor.modeId)) {
            return QString::fromUtf8(descriptor.displayName);
        }
    }
    return modeId;
}

QString authorizationText(bool permanent,
                          const QDate &expiresDate,
                          bool showExpiry,
                          const QStringList &featureModeIds,
                          const QString &defaultModeId)
{
    QStringList featureNames;
    for (const QString &modeId : featureModeIds) {
        featureNames.append(modeDisplayName(modeId));
    }
    return QStringLiteral(
                "有效期：%1\n授权模式：%2\n默认模式：%3\n"
                "显示许可证有效期：%4")
            .arg(permanent
                 ? QStringLiteral("长期有效")
                 : expiresDate.toString(QStringLiteral("yyyy-MM-dd")),
                 featureNames.join(QStringLiteral("、")),
                 modeDisplayName(defaultModeId),
                 showExpiry ? QStringLiteral("是") : QStringLiteral("否"));
}

void configureNumericCodeInput(QLineEdit *lineEdit, int maximumDigits)
{
    QObject::connect(lineEdit, &QLineEdit::textEdited,
                     lineEdit, [lineEdit, maximumDigits](const QString &text) {
        const int cursorPosition = lineEdit->cursorPosition();
        int digitsBeforeCursor = 0;
        for (int index = 0;
             index < cursorPosition && index < text.size();
             ++index) {
            const QChar character = text.at(index);
            if (character >= QLatin1Char('0')
                    && character <= QLatin1Char('9')) {
                ++digitsBeforeCursor;
            }
        }

        QString digits;
        for (const QChar character : text) {
            if (character >= QLatin1Char('0')
                    && character <= QLatin1Char('9')) {
                digits.append(character);
            }
        }
        digits = digits.left(maximumDigits);
        if (digitsBeforeCursor > digits.size()) {
            digitsBeforeCursor = digits.size();
        }

        QString formatted;
        for (int index = 0; index < digits.size(); ++index) {
            if (index > 0 && index % 4 == 0) {
                formatted.append(QLatin1Char(' '));
            }
            formatted.append(digits.at(index));
        }

        int formattedCursorPosition = 0;
        if (digitsBeforeCursor > 0) {
            formattedCursorPosition = digitsBeforeCursor
                    + (digitsBeforeCursor - 1) / 4;
            if (digitsBeforeCursor % 4 == 0
                    && digitsBeforeCursor < digits.size()) {
                ++formattedCursorPosition;
            }
        }
        lineEdit->setText(formatted);
        lineEdit->setCursorPosition(formattedCursorPosition);
    });
}

} // namespace

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->expiresTypeComboBox->addItem(
                QStringLiteral("具体日期"), QStringLiteral("date"));
    ui->expiresTypeComboBox->addItem(
                QStringLiteral("长期有效"), QStringLiteral("permanent"));
    ui->expiresEdit->setMinimumDate(QDate::currentDate());
    ui->expiresEdit->setMaximumDate(QDate(2293, 10, 14));
    ui->expiresEdit->setDate(QDate::currentDate().addYears(1));

    for (const LicenseToolModeDescriptor &descriptor
         : licenseToolModeDescriptors()) {
        QListWidgetItem *item = new QListWidgetItem(
                    QString::fromUtf8(descriptor.displayName),
                    ui->featuresListWidget);
        item->setData(Qt::UserRole, QLatin1String(descriptor.modeId));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    ui->requestStatusLabel->clear();
    ui->outputStatusLabel->clear();
    ui->copyActivationButton->setEnabled(false);
    refreshExpiresEditor();
    refreshDefaultModes();
    configureNumericCodeInput(ui->requestCodeEdit, 16);
    configureNumericCodeInput(ui->inspectActivationCodeEdit, 24);

    connect(ui->requestCodeEdit,
            &QLineEdit::textChanged,
            this, &Widget::refreshRequestStatus);
    connect(ui->expiresTypeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshExpiresEditor(); });
    connect(ui->featuresListWidget,
            &QListWidget::itemChanged,
            this, [this](QListWidgetItem *) { refreshDefaultModes(); });
    connect(ui->generateButton,
            &QPushButton::clicked,
            this, &Widget::generateActivationCode);
    connect(ui->copyActivationButton,
            &QPushButton::clicked,
            this, &Widget::copyActivationCode);
    connect(ui->inspectActivationButton,
            &QPushButton::clicked,
            this, &Widget::inspectActivationCode);
    connect(ui->inspectLicenseButton,
            &QPushButton::clicked,
            this, &Widget::inspectLicenseFile);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::generateActivationCode()
{
    ui->activationCodeEdit->clear();
    ui->copyActivationButton->setEnabled(false);
    const QString requestCode = ui->requestCodeEdit->text().trimmed();
    if (requestCode.isEmpty()) {
        ui->outputStatusLabel->setText(
                    QStringLiteral("请输入激活申请码。"));
        return;
    }

    ActivationRequestData request;
    if (!parseActivationRequestCode(requestCode, &request)) {
        ui->outputStatusLabel->setText(
                    QStringLiteral("申请码无效，请确认输入是否正确。"));
        return;
    }

    const QStringList features = selectedFeatureModeIds();
    if (features.isEmpty()) {
        ui->outputStatusLabel->setText(
                    QStringLiteral("请至少选择一个授权模式。"));
        return;
    }

    ActivationCodeData activation;
    activation.deviceBinding = request.deviceBinding;
    activation.deviceDigest = request.deviceDigest;
    activation.permanent =
            ui->expiresTypeComboBox->currentData().toString()
            == QLatin1String("permanent");
    activation.expiresDate = ui->expiresEdit->date();
    activation.showExpiry = ui->showExpiryCheckBox->isChecked();
    activation.featureModeIds = features;
    activation.defaultModeId =
            ui->defaultModeComboBox->currentData().toString();

    const QString activationCode = createActivationCode(activation);
    ui->activationCodeEdit->setText(
                QStringLiteral("%1 %2 %3 %4 %5 %6")
                .arg(activationCode.mid(0, 4))
                .arg(activationCode.mid(4, 4))
                .arg(activationCode.mid(8, 4))
                .arg(activationCode.mid(12, 4))
                .arg(activationCode.mid(16, 4))
                .arg(activationCode.mid(20, 4)));
    ui->copyActivationButton->setEnabled(true);
    ui->outputStatusLabel->setText(QStringLiteral("激活码已生成。"));
}

void Widget::copyActivationCode()
{
    QApplication::clipboard()->setText(
                ui->activationCodeEdit->text());
    QMessageBox::information(
                this,
                QStringLiteral("提示"),
                QStringLiteral("激活码复制成功。"));
}

void Widget::inspectActivationCode()
{
    ActivationCodeData activation;
    if (!parseActivationCode(
                ui->inspectActivationCodeEdit->text(), &activation)) {
        ui->inspectionResultEdit->setPlainText(
                    QStringLiteral("激活码无效，请确认输入是否正确。"));
        return;
    }

    ui->inspectionResultEdit->setPlainText(
                QStringLiteral(
                    "内容类型：激活码\n设备绑定方式：%1\n设备摘要：%2\n%3")
                .arg(activation.deviceBinding,
                     activation.deviceDigest,
                     authorizationText(
                         activation.permanent,
                         activation.expiresDate,
                         activation.showExpiry,
                         activation.featureModeIds,
                         activation.defaultModeId)));
}

void Widget::inspectLicenseFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择 license.ini"),
                QString(),
                QStringLiteral("License Files (license.ini *.ini);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    ui->licensePathEdit->setText(filePath);
    LicenseFileData license;
    if (!readLicenseFile(filePath, &license)) {
        ui->inspectionResultEdit->setPlainText(
                    QStringLiteral("许可证文件无效或无法读取。"));
        return;
    }

    ui->inspectionResultEdit->setPlainText(
                QStringLiteral(
                    "内容类型：license.ini\n格式版本：5\n设备绑定方式：%1\n设备码：%2\n%3")
                .arg(license.deviceBinding,
                     license.deviceCode,
                     authorizationText(
                         license.permanent,
                         license.expiresDate,
                         license.showExpiry,
                         license.featureModeIds,
                         license.defaultModeId)));
}

void Widget::refreshRequestStatus()
{
    ActivationRequestData request;
    if (parseActivationRequestCode(
                ui->requestCodeEdit->text(), &request)) {
        ui->requestStatusLabel->setText(
                    QStringLiteral("申请码有效，绑定方式：%1")
                    .arg(request.deviceBinding));
    } else {
        ui->requestStatusLabel->clear();
    }
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
    for (const LicenseToolModeDescriptor &descriptor
         : licenseToolModeDescriptors()) {
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
