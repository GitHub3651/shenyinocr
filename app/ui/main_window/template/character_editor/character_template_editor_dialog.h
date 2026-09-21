#pragma once

#include "templates/template_store.h"

#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class QLabel;
class QLineEdit;
class CharacterOcrEngine;

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

    TemplateSettings resultSettings() const;
    QMap<QString, QImage> characterImages() const;

private:
    void rebuildNamePage();
    void refreshCharacterPreviewList();
    bool isSystemTemplateFile(const QString &fileName) const;
    QString nextAvailableFileName(
        const QString &baseName,
        const QStringList &reservedFileNames = QStringList()) const;
    void refreshSaveNamePreviews();
    bool saveTemplates();
    void moveToNamingPage();
    void saveAndAccept();
    void autoSplitCharacters();
    void recognizeUnnamedCharacterNames();

    std::unique_ptr<Ui::CharacterTemplateEditorDialog> ui;
    QImage m_sourceImage;
    TemplateSettings m_resultSettings;
    QMap<QString, QImage> m_characterImages;
    QList<QLineEdit *> m_nameEdits;
    QList<QLabel *> m_nameErrorLabels;
    QList<QLabel *> m_saveNameLabels;
    QVector<TemplateCharacterBox> m_sortedBoxes;
    std::unique_ptr<CharacterOcrEngine> m_ocrEngine;
};
