// 文件作用：提供字符模板切分、命名和编辑所需的对话框交互。
#pragma once

#include "templates/template_store.h"

#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <memory>

class QLabel;
class QLineEdit;

namespace Ui {
class CharacterTemplateEditorDialog;
}

class CharacterTemplateEditorDialog : public QDialog
{
public:
    explicit CharacterTemplateEditorDialog(
        const QImage &sourceImage,
        const TemplateSettings &initialSettings,
        QWidget *parent = nullptr);
    ~CharacterTemplateEditorDialog() override;

    int savedCount() const;
    TemplateSettings resultSettings() const;
    QMap<QString, QImage> characterImages() const;

private:
    void rebuildNamePage();
    void loadSavedCharacterBoxes();
    void refreshCharacterPreviewList();
    bool isSystemTemplateFile(const QString &fileName) const;
    QString nextAvailableFileName(
        const QString &baseName,
        const QStringList &reservedFileNames = QStringList()) const;
    void refreshSaveNamePreviews();
    bool saveTemplates();
    void moveToNamingPage();
    void saveAndAccept();

    std::unique_ptr<Ui::CharacterTemplateEditorDialog> ui;
    QImage m_sourceImage;
    TemplateSettings m_resultSettings;
    QMap<QString, QImage> m_characterImages;
    QList<TemplateCharacterBox> m_initialBoxes;
    QList<QLineEdit *> m_nameEdits;
    QList<QLabel *> m_nameErrorLabels;
    QList<QLabel *> m_saveNameLabels;
    QList<TemplateCharacterBox> m_sortedBoxes;
    int m_savedCount = 0;
};
