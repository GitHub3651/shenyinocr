#include <QtTest>

#include "application/template_application_service.h"
#include "devices/barcode/barcode_decoder.h"
#include "recipes/recipe_store.h"

#include <QDir>
#include <QTemporaryDir>

#include <cstddef>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

class FakeBarcodeDecoder : public IBarcodeDecoder
{
public:
    bool ensureLoaded() override { return true; }
    QString lastError() const override { return QString(); }

    BarcodeReadResult decode(
            const cv::Mat &grayRoi,
            const BarcodeDecodeOptions &,
            int,
            unsigned int,
            int *,
            unsigned int *) override
    {
        BarcodeReadResult result;
        result.status = grayRoi.empty()
                ? BarcodeReadStatus::InvalidRoi
                : BarcodeReadStatus::Success;
        result.readable = !grayRoi.empty();
        result.text = result.readable
                ? QStringLiteral("DM-OK") : QString();
        return result;
    }
};

std::shared_ptr<TemplateApplicationService> makeService(
        const QString &root,
        const std::shared_ptr<RecipeStore> &store)
{
    return std::shared_ptr<TemplateApplicationService>(
                new TemplateApplicationService(
                    store,
                    std::shared_ptr<IBarcodeDecoder>(
                        new FakeBarcodeDecoder),
                    QDir(root).filePath(QStringLiteral("editor"))));
}

bool stageProfiles(
        TemplateApplicationService *service,
        ProductRecipe *recipe,
        int profileCount,
        QString *errorMessage)
{
    QMap<QString, QString> sources;
    for (int index = 0; index < profileCount; ++index) {
        RecipeProfile profile;
        profile.name = QStringLiteral("Profile-%1").arg(index + 1);
        profile.targetText = QStringLiteral("AB");
        profile.trackingRoi = QRectF(10, 12, 30, 24);

        InitialRecipeProfileAssets assets;
        assets.profileIndex = index;
        assets.rawImage = cv::Mat(
                    90, 120, CV_8UC3,
                    cv::Scalar(20 + index, 30, 40));
        assets.trackingImageRect = cv::Rect(10, 12, 30, 24);
        assets.datePolygon = {
            cv::Point2f(-8.0f, -4.0f),
            cv::Point2f(8.0f, -4.0f),
            cv::Point2f(8.0f, 4.0f),
            cv::Point2f(-8.0f, 4.0f)
        };
        if (recipe->detectionMode == DetectionMode::BarcodeWord) {
            assets.barcodePolygon = {
                cv::Point2f(-10.0f, -10.0f),
                cv::Point2f(10.0f, -10.0f),
                cv::Point2f(10.0f, 10.0f),
                cv::Point2f(-10.0f, 10.0f)
            };
        }
        if (recipe->detectionMode == DetectionMode::Stamp) {
            assets.stampPolygon = {
                cv::Point2f(-5.0f, -5.0f),
                cv::Point2f(5.0f, -5.0f),
                cv::Point2f(0.0f, 5.0f)
            };
            assets.stampRing = cv::Mat(
                        20, 20, CV_8UC1, cv::Scalar(80));
        }
        if (!service->stageInitialProfileAssets(
                assets, recipe, &profile, &sources, errorMessage)) {
            return false;
        }
        recipe->profiles.append(profile);
    }
    return service->replaceDraft(*recipe, sources, errorMessage);
}

PreparedRecipeSnapshot publishTemplate(
        TemplateApplicationService *service,
        DetectionMode mode,
        int profileCount,
        QString *errorMessage)
{
    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("Recipe"), mode);
    if (!service->beginNew(recipe, errorMessage)
            || !stageProfiles(
                service, &recipe, profileCount, errorMessage)) {
        return PreparedRecipeSnapshot();
    }
    PreparedRecipeSnapshot prepared;
    if (!service->publish(&prepared, errorMessage)) {
        return PreparedRecipeSnapshot();
    }
    return prepared;
}

} // namespace

class TemplateApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void fourTemplateModesPublishPreparedAssets();
    void geometryMapsSingleAndMultiProfileRois();
    void characterAssetsRepublishSameUuid();
    void failedRepublishPreservesFormalRecipe();
    void crossModeSwitchAndTissueNeedNoTemplate();
    void profileAndModeStateChangeOnlyThroughCommands();
};

void TemplateApplicationServiceTest::fourTemplateModesPublishPreparedAssets()
{
    const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::BarcodeWord
    };
    for (DetectionMode mode : modes) {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const std::shared_ptr<RecipeStore> store(
                    new RecipeStore(QDir(root.path()).filePath("recipes")));
        const std::shared_ptr<TemplateApplicationService> service =
                makeService(root.path(), store);
        QString error;
        const int profileCount = mode == DetectionMode::Word
                || mode == DetectionMode::BarcodeWord ? 2 : 1;
        const PreparedRecipeSnapshot prepared = publishTemplate(
                    service.get(), mode, profileCount, &error);
        QVERIFY2(prepared, qPrintable(error));
        QCOMPARE(prepared->profiles.size(), profileCount);
        QCOMPARE(static_cast<int>(prepared->recipe->detectionMode),
                 static_cast<int>(mode));
        for (const PreparedRecipeProfile &profile : prepared->profiles) {
            QVERIFY(!profile.rawImage.empty());
            QVERIFY(!profile.trackingTemplate.empty());
            QVERIFY(profile.datePolygon.size() >= 3u);
        }
    }
}

void TemplateApplicationServiceTest::geometryMapsSingleAndMultiProfileRois()
{
    QTemporaryDir root;
    const std::shared_ptr<RecipeStore> store(
                new RecipeStore(QDir(root.path()).filePath("recipes")));
    const std::shared_ptr<TemplateApplicationService> service =
            makeService(root.path(), store);
    TemplateDisplayGeometry display;
    display.viewSize = QSize(200, 160);
    display.displayedImageSize = QSize(160, 120);
    display.sourceImageSize = QSize(320, 240);
    QCOMPARE(service->mapDisplayRectToImage(
                 QRect(20, 20, 40, 30), display),
             QRect(0, 0, 80, 60));
    const TemplateProfileGeometry profile =
            service->buildProfileGeometry(
                QRect(40, 40, 50, 30),
                QRect(100, 60, 30, 30),
                QPolygon() << QPoint(80, 50)
                           << QPoint(120, 50)
                           << QPoint(120, 80)
                           << QPoint(80, 80),
                true,
                display);
    QVERIFY2(profile.valid, qPrintable(profile.errorMessage));
    QCOMPARE(profile.trackingImageRect.x, 40);
    QCOMPARE(profile.trackingImageRect.y, 40);
    QCOMPARE(profile.trackingImageRect.width, 98);
    QCOMPARE(profile.trackingImageRect.height, 58);
    QCOMPARE(profile.datePolygon.size(), std::size_t(4));
    QCOMPARE(profile.barcodePolygon.size(), std::size_t(4));

    BarcodeReadResult barcode;
    QString failureReason;
    QVERIFY(service->validateBarcodeTemplate(
                cv::Mat(80, 100, CV_8UC3, cv::Scalar(20, 30, 40)),
                QRect(10, 12, 30, 24),
                BarcodeDecodeOptions(),
                &barcode,
                &failureReason));
    QVERIFY(barcode.readable);
    QCOMPARE(barcode.text, QStringLiteral("DM-OK"));
    QVERIFY(failureReason.isEmpty());
}

void TemplateApplicationServiceTest::characterAssetsRepublishSameUuid()
{
    QTemporaryDir root;
    const std::shared_ptr<RecipeStore> store(
                new RecipeStore(QDir(root.path()).filePath("recipes")));
    const std::shared_ptr<TemplateApplicationService> service =
            makeService(root.path(), store);
    QString error;
    PreparedRecipeSnapshot prepared = publishTemplate(
                service.get(), DetectionMode::Word, 1, &error);
    QVERIFY2(prepared, qPrintable(error));
    const QString recipeId = prepared->recipe->recipeId;

    ProductRecipe recipe = service->draft();
    RecipeProfile profile = recipe.profiles.first();
    const PreparedRecipeProfile &preparedProfile =
            prepared->profiles.first();
    const cv::Point2f trackingCenter(
                static_cast<float>(
                    preparedProfile.definition.trackingRoi.center().x()),
                static_cast<float>(
                    preparedProfile.definition.trackingRoi.center().y()));
    std::vector<cv::Point> absoluteDatePolygon;
    for (const cv::Point2f &point : preparedProfile.datePolygon) {
        absoluteDatePolygon.emplace_back(
                    cvRound(trackingCenter.x + point.x),
                    cvRound(trackingCenter.y + point.y));
    }
    const cv::Rect characterSourceBounds =
            cv::boundingRect(absoluteDatePolygon)
            & cv::Rect(0, 0,
                       preparedProfile.rawImage.cols,
                       preparedProfile.rawImage.rows);
    QVERIFY(characterSourceBounds.width > 0);
    QVERIFY(characterSourceBounds.height > 0);
    const QSize characterSourceSize(
                characterSourceBounds.width,
                characterSourceBounds.height);
    profile.characterSourceSize = characterSourceSize;
    RecipeCharacterBox box;
    box.name = QStringLiteral("A");
    box.rect = QRect(QPoint(0, 0), characterSourceSize);
    profile.characterBoxes.append(box);
    QMap<QString, QImage> images;
    QImage characterImage(
                characterSourceSize,
                QImage::Format_Grayscale8);
    characterImage.fill(0);
    images.insert(QStringLiteral("A.png"), characterImage);
    QMap<QString, QString> sources = service->assetSourcePaths();
    QVERIFY2(service->stageCharacterAssets(
                 0, images, &recipe, &profile,
                 &sources, &error), qPrintable(error));
    recipe.profiles[0] = profile;
    QVERIFY2(service->replaceDraft(recipe, sources, &error),
             qPrintable(error));
    QVERIFY2(service->publish(&prepared, &error), qPrintable(error));
    QCOMPARE(prepared->recipe->recipeId, recipeId);
    QCOMPARE(prepared->profiles.first().characterTemplates.size(),
             std::size_t(1));
}

void TemplateApplicationServiceTest::failedRepublishPreservesFormalRecipe()
{
    QTemporaryDir root;
    const std::shared_ptr<RecipeStore> store(
                new RecipeStore(QDir(root.path()).filePath("recipes")));
    const std::shared_ptr<TemplateApplicationService> service =
            makeService(root.path(), store);
    QString error;
    const PreparedRecipeSnapshot original = publishTemplate(
                service.get(), DetectionMode::Ocr, 1, &error);
    QVERIFY2(original, qPrintable(error));

    ProductRecipe changed = service->draft();
    changed.displayName = QStringLiteral("Changed");
    QMap<QString, QString> brokenSources = service->assetSourcePaths();
    brokenSources[brokenSources.firstKey()] =
            QDir(root.path()).filePath(QStringLiteral("missing.bin"));
    QVERIFY(service->replaceDraft(changed, brokenSources, &error));
    PreparedRecipeSnapshot rejected;
    QVERIFY(!service->publish(&rejected, &error));

    PreparedRecipeSnapshot reloaded;
    QVERIFY2(store->loadPreparedRecipe(
                 original->recipe->recipeId, &reloaded, &error),
             qPrintable(error));
    QCOMPARE(reloaded->recipe->displayName,
             original->recipe->displayName);
}

void TemplateApplicationServiceTest::crossModeSwitchAndTissueNeedNoTemplate()
{
    QTemporaryDir root;
    const std::shared_ptr<RecipeStore> store(
                new RecipeStore(QDir(root.path()).filePath("recipes")));
    const std::shared_ptr<TemplateApplicationService> service =
            makeService(root.path(), store);
    QString error;
    QVERIFY(publishTemplate(
                service.get(), DetectionMode::Stamp, 1, &error));
    QVERIFY(publishTemplate(
                service.get(), DetectionMode::BarcodeWord, 2, &error));
    QCOMPARE(static_cast<int>(
                 service->activePreparedRecipe()->recipe->detectionMode),
             static_cast<int>(DetectionMode::BarcodeWord));

    ProductRecipe tissue = createProductRecipe(
                QStringLiteral("Tissue"), DetectionMode::Tissue);
    tissue.tissueParameters.roughnessThreshold = 7.5;
    QVERIFY(service->beginNew(tissue, &error));
    QVERIFY(service->replaceDraft(
                tissue, QMap<QString, QString>(), &error));
    PreparedRecipeSnapshot prepared;
    QVERIFY2(service->publish(&prepared, &error), qPrintable(error));
    QVERIFY(prepared->profiles.isEmpty());
    QCOMPARE(prepared->tissue.roughnessThreshold, 7.5);
}

void TemplateApplicationServiceTest::profileAndModeStateChangeOnlyThroughCommands()
{
    QTemporaryDir root;
    const std::shared_ptr<RecipeStore> store(
                new RecipeStore(QDir(root.path()).filePath("recipes")));
    const std::shared_ptr<TemplateApplicationService> service =
            makeService(root.path(), store);

    WordTemplateProfile first;
    first.name = QStringLiteral("Profile-A");
    WordTemplateProfile second;
    second.name = QStringLiteral("Profile-B");
    service->replaceWordProfiles({first, second});
    QCOMPARE(service->wordProfiles().size(), std::size_t(2));

    WordTemplateProfile updated = service->wordProfiles().at(1);
    updated.settings.imageThresholdPercent = 82;
    QVERIFY(service->replaceWordProfile(1, updated));
    QCOMPARE(service->wordProfiles().at(1)
             .settings.imageThresholdPercent, 82);

    const QString wordMode = detectionModeUiId(DetectionMode::Word);
    service->rememberPublishedRecipe(
                wordMode,
                QStringLiteral("00000000-0000-0000-0000-000000000001"));
    QVERIFY(service->modeMemory().publishedRecipeIdsByMode()
            .contains(wordMode));
    service->forgetPublishedRecipe(wordMode);
    QVERIFY(!service->modeMemory().publishedRecipeIdsByMode()
            .contains(wordMode));

    service->clearWordProfiles();
    QVERIFY(service->wordProfiles().empty());
}

QTEST_APPLESS_MAIN(TemplateApplicationServiceTest)
#include "template_application_service_test.moc"
