#ifndef CHARACTERTEMPLATECROPDIALOG_H
#define CHARACTERTEMPLATECROPDIALOG_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

class QLabel;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QGridLayout;

class CharacterTemplateCropDialog : public QDialog
{
public:
    explicit CharacterTemplateCropDialog(const QImage &sourceImage,
                                         const QString &templateDirPath,
                                         QWidget *parent = nullptr);

    int savedCount() const;

private:
    struct CharacterBox
    {
        QRect rect;
        QString name;
    };

    class CropImageLabel;

    void buildUi();
    void rebuildNamePage();
    void loadSavedCharacterBoxes();
    void refreshCharacterPreviewList();
    bool isSystemTemplateFile(const QString &fileName) const;
    QString nextAvailableFileName(const QString &baseName, const QStringList &reservedFileNames = QStringList()) const;
    void refreshSaveNamePreviews();
    bool saveTemplates();
    bool saveCharacterBoxesToSettings(const QList<CharacterBox> &boxes, QString *errorMessage) const;
    bool removeOldCharacterTemplateImages() const;

    QImage m_sourceImage;
    QString m_templateDirPath;
    QList<CharacterBox> m_initialBoxes;
    CropImageLabel *m_cropLabel = nullptr;
    QWidget *m_drawPage = nullptr;
    QWidget *m_namePage = nullptr;
    QStackedWidget *m_stack = nullptr;
    QVBoxLayout *m_nameListLayout = nullptr;
    QScrollArea *m_previewScrollArea = nullptr;
    QGridLayout *m_previewGrid = nullptr;
    QList<QLineEdit *> m_nameEdits;
    QList<QLabel *> m_nameErrorLabels;
    QList<QLabel *> m_saveNameLabels;
    QList<CharacterBox> m_sortedBoxes;
    int m_savedCount = 0;
};

#endif // CHARACTERTEMPLATECROPDIALOG_H
