// 文件作用：本文件用于提供字符模板切分、命名和编辑所需的对话框交互。
// 主要职责：提供字符模板切分、命名和编辑所需的对话框交互。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef UI_DIALOGS_CHARACTER_TEMPLATE_EDITOR_DIALOG_H
#define UI_DIALOGS_CHARACTER_TEMPLATE_EDITOR_DIALOG_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>

#include "templates/template_store.h"

class QLabel;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
// 组件说明：QGridLayout 组件封装本文件中与其名称对应的单一职责。
class QGridLayout;

// Pure UI dialog: returns boxes, names and cropped images. It never writes
// template files, settings or formal assets.
class CharacterTemplateEditorDialog : public QDialog
{
public:
    explicit CharacterTemplateEditorDialog(
        const QImage &sourceImage,
        const TemplateSettings &initialSettings,
        QWidget *parent = nullptr);

    int savedCount() const;
    TemplateSettings resultSettings() const;
    QMap<QString, QImage> characterImages() const;

private:
    // 组件说明：CharacterBox 数据结构集中保存该流程需要的一组相关数据。
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
    TemplateSettings m_resultSettings;
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
