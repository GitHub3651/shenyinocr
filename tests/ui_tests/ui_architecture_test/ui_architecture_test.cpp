#include "ui/main_window.h"
#include "ui/pages/inspection_page.h"
#include "ui/pages/machine_settings_page.h"
#include "ui/pages/template_editor_page.h"
#include "ui/widgets/image_label.h"

#include <QLabel>
#include <QObject>
#include <QtTest>
#include <QWidget>

#include <type_traits>

class UiArchitectureTest : public QObject
{
    Q_OBJECT

private slots:
    void targetTypesAreConcrete();
    void startupCompositionContractsStayExplicit();
    void pagesAreNonCopyable();
};

void UiArchitectureTest::targetTypesAreConcrete()
{
    static_assert(std::is_base_of<QWidget, MainWindow>::value,
                  "MainWindow must remain the concrete top-level widget.");
    static_assert(std::is_base_of<QLabel, ImageLabel>::value,
                  "ImageLabel must remain the image interaction widget.");
    static_assert(std::is_base_of<QObject, MachineSettingsPage>::value,
                  "MachineSettingsPage must own settings-page interactions.");
    static_assert(std::is_base_of<QObject, TemplateEditorPage>::value,
                  "TemplateEditorPage must own template interactions.");
    QVERIFY(true);
}

void UiArchitectureTest::startupCompositionContractsStayExplicit()
{
    using AttachPages = void (MainWindow::*)(
        InspectionPage *, MachineSettingsPage *, TemplateEditorPage *);
    using ActualAttachPages = decltype(&MainWindow::attachPages);
    static_assert(std::is_same<AttachPages, ActualAttachPages>::value,
                  "MainWindow page injection contract changed.");

    static_assert(std::is_constructible<
                      InspectionPage,
                      QWidget *,
                      Ui::MainWindow *,
                      QTimer *,
                      bool *,
                      const InspectionPage::Callbacks &>::value,
                  "InspectionPage must remain startup-constructible.");
    static_assert(std::is_constructible<
                      MachineSettingsPage,
                      Ui::MainWindow *,
                      SettingsApplicationService *,
                      SettingsEditState *,
                      QString *,
                      bool *,
                      bool *,
                      const MachineSettingsPage::Callbacks &,
                      QObject *>::value,
                  "MachineSettingsPage must remain startup-constructible.");
    QVERIFY(true);
}

void UiArchitectureTest::pagesAreNonCopyable()
{
    static_assert(!std::is_copy_constructible<InspectionPage>::value,
                  "InspectionPage must not duplicate UI state.");
    static_assert(!std::is_copy_constructible<MachineSettingsPage>::value,
                  "MachineSettingsPage must not duplicate settings state.");
    static_assert(!std::is_copy_constructible<TemplateEditorPage>::value,
                  "TemplateEditorPage must not duplicate recipe state.");
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(UiArchitectureTest)

#include "ui_architecture_test.moc"
