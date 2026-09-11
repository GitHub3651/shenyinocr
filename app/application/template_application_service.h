#pragma once

#include "application/template_editor_contract.h"
#include "application/template_geometry_service.h"
#include "templates/template_store.h"

#include <QImage>
#include <QMap>
#include <QString>

#include <functional>
#include <memory>

class IBarcodeDecoder;

class TemplateApplicationService
{
public:
    TemplateApplicationService(
        const std::shared_ptr<TemplateStore> &store,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder);

    bool beginNew(DetectionMode mode,
                  QString *errorMessage = nullptr);
    bool beginEdit(const QString &directoryPath,
                   DetectionMode expectedMode,
                   QString *errorMessage = nullptr);
    void cancel();
    bool isActive() const;
    const EditableTemplate &draft() const;
    bool replaceDraft(const EditableTemplate &value,
                      QString *errorMessage = nullptr);
    bool save(const QString &directoryPath,
              bool preserveExistingContents,
              PreparedTemplateSnapshot *preparedTemplate,
              QString *errorMessage = nullptr);

    TemplateSummary readSummary(
        const QString &directoryPath,
        DetectionMode expectedMode,
        TemplateStoreError *error = nullptr) const;
    bool loadPreparedTemplate(
        const QString &directoryPath,
        DetectionMode expectedMode,
        PreparedTemplateSnapshot *preparedTemplate,
        QString *errorMessage = nullptr) const;
    bool updateTemplates(
        const QStringList &directoryPaths,
        DetectionMode expectedMode,
        const std::function<void(TemplateSettings *)> &update,
        QString *resultMessage = nullptr);

    bool stageInitialAssets(
        const InitialTemplateAssets &assets,
        TemplateSettings *settings,
        EditableTemplate *value,
        QString *errorMessage = nullptr) const;
    bool stageCharacterAssets(
        const QMap<QString, QImage> &characterImages,
        EditableTemplate *value,
        QString *errorMessage = nullptr) const;

    QRect mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const;
    TemplateGeometryResult buildGeometry(
        const TemplateDrawingInput &input,
        const TemplateDisplayGeometry &geometry) const;
    bool validateBarcodeTemplate(
        const cv::Mat &sourceImage,
        const QRect &sourceRect,
        const TemplateBarcodeValidationOptions &options,
        QString *failureReason) const;

    QString currentDirectoryPath() const;
    void setActivePreparedTemplate(
        const PreparedTemplateSnapshot &preparedTemplate);

private:
    static QString storeErrorMessage(const TemplateStoreError &error);

    std::shared_ptr<TemplateStore> m_store;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
    TemplateGeometryService m_geometryService;
    QString m_currentDirectoryPath;
    EditableTemplate m_draft;
    PreparedTemplateSnapshot m_activePreparedTemplate;
    bool m_active = false;
};
