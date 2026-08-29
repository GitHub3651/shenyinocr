// 文件作用：集中定义外部模板文件夹的数据和唯一磁盘访问接口。
#pragma once

#include "contracts/barcode_parameter_defaults.h"
#include "contracts/detection_mode.h"

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

#include <opencv2/core.hpp>

struct TemplateCharacterBox
{
    QString name;
    QRect rect;
};

struct TemplateBarcodeParameters
{
    unsigned int formatMask = BarcodeParameterDefaults::FormatMask;
    int roiPaddingPercent = BarcodeParameterDefaults::RoiPaddingPercent;
    int maxDecodeTimeMs = BarcodeParameterDefaults::MaxDecodeTimeMs;
    bool enableFallback = BarcodeParameterDefaults::EnableFallback;
};

struct TemplateSettings
{
    static const int CurrentSchemaVersion = 1;
    static const int DefaultImageThresholdPercent = 70;

    int schemaVersion = CurrentSchemaVersion;
    DetectionMode detectionMode = DetectionMode::Stamp;
    QString targetText;
    int imageThresholdPercent = DefaultImageThresholdPercent;
    QRectF trackingRoi;
    QVector<QPointF> datePolygon;
    QVector<QPointF> barcodePolygon;
    QVector<QPointF> stampPolygon;
    QSize characterSourceSize = QSize(0, 0);
    QVector<TemplateCharacterBox> characterBoxes;
    TemplateBarcodeParameters barcodeParameters;
};

struct TemplateCharacterAsset
{
    QString fileName;
    QString storageStem;
    cv::Mat image;
};

struct EditableTemplate
{
    TemplateSettings settings;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    cv::Mat stampRingTemplate;
    QVector<TemplateCharacterAsset> characterAssets;
    bool replaceCharacterAssets = false;
};

struct PreparedTemplate
{
    QString directoryPath;
    QString displayName;
    TemplateSettings settings;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    cv::Mat stampRingTemplate;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    std::vector<cv::Point2f> stampPolygon;
    QStringList targetUnits;
    std::vector<TemplateCharacterAsset> characterAssets;
    std::vector<cv::Mat> characterTemplates;
    std::vector<int> characterTemplateTargetIndexes;
};

typedef std::shared_ptr<const PreparedTemplate> PreparedTemplateSnapshot;

struct TemplateSummary
{
    QString directoryPath;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    bool valid = false;
    bool complete = false;
    QString message;
};

struct TemplateStoreError
{
    QString code;
    QString userMessage;
    QString diagnostic;
    QString path;

    bool isEmpty() const { return code.isEmpty(); }
};

class TemplateStore
{
public:
    static QStringList templateTargetUnits(const QString &targetText);

    TemplateSummary readSummary(
        const QString &directoryPath,
        DetectionMode expectedMode,
        TemplateStoreError *error = nullptr) const;

    bool loadEditable(const QString &directoryPath,
                      DetectionMode expectedMode,
                      EditableTemplate *value,
                      TemplateStoreError *error = nullptr) const;

    bool loadPrepared(const QString &directoryPath,
                      DetectionMode expectedMode,
                      PreparedTemplateSnapshot *value,
                      TemplateStoreError *error = nullptr) const;

    bool save(const QString &directoryPath,
              const EditableTemplate &value,
              bool preserveExistingContents,
              TemplateStoreError *error = nullptr) const;
};

QString characterStorageStem(const QString &unit);
bool templateCharacterAssetMatchesTarget(
    const QString &storageStem,
    const QString &target);
QString missingTemplateTargetUnit(
    DetectionMode mode,
    const QStringList &targetUnits,
    const QVector<TemplateCharacterAsset> &characterAssets);
