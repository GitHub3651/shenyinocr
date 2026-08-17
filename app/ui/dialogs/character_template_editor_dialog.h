#ifndef UI_DIALOGS_CHARACTER_TEMPLATE_EDITOR_DIALOG_H
#define UI_DIALOGS_CHARACTER_TEMPLATE_EDITOR_DIALOG_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>

#include "recipes/product_recipe.h"

class QLabel;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QGridLayout;

// Pure UI dialog: returns boxes, names and cropped images. It never writes
// recipe files, settings or formal assets.
class CharacterTemplateEditorDialog : public QDialog
{
public:
    explicit CharacterTemplateEditorDialog(
        const QImage &sourceImage,
        const RecipeProfile &initialProfile,
        QWidget *parent = nullptr);

    int savedCount() const;
    RecipeProfile resultProfile() const;
    QMap<QString, QImage> characterImages() const;

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
    QString nextAvailableFileName(
        const QString &baseName,
        const QStringList &reservedFileNames = QStringList()) const;
    void refreshSaveNamePreviews();
    bool saveTemplates();

    QImage m_sourceImage;
    RecipeProfile m_resultProfile;
    QMap<QString, QImage> m_characterImages;
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

#endif // UI_DIALOGS_CHARACTER_TEMPLATE_EDITOR_DIALOG_H
