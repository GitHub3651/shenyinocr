#include <QtTest/QtTest>

#include "appsettingsmanager.h"
#include "product_recipe.h"
#include "template_profile_assets.h"
#include "template_profile_mapper.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

#include <type_traits>

static_assert(std::is_const<ProductRecipeSnapshot::element_type>::value,
              "Runtime product recipes must be immutable.");

class ProductRecipeTest : public QObject
{
    Q_OBJECT

private slots:
    void newTissueRecipeUsesCanonicalIdentityAndSixPointZeroDefault();
    void jsonRoundTripRetainsModeParametersAndAssets();
    void invalidFieldsAndEscapingAssetAreRejected();
    void runtimeSnapshotIsIndependentFromEditableRecipe();
    void templatePrivateSettingsMappingRetainsProfileFields();
    void templateProfileAssetManifestPreservesVariantsAndNamespaces();
};

namespace {

RecipeProfile validProfile(const QString &name)
{
    RecipeProfile profile;
    profile.name = name;
    profile.targetText = QStringLiteral("A1");
    profile.imageThreshold = 70.0;
    profile.trackingBox = QRectF(10.0, 20.0, 120.0, 80.0);
    profile.hasValidBoxes = true;
    return profile;
}

} // namespace

void ProductRecipeTest::newTissueRecipeUsesCanonicalIdentityAndSixPointZeroDefault()
{
    const ProductRecipe recipe =
            createProductRecipe(QStringLiteral("  \u7eb8\u5dfe\u4ea7\u54c1  "),
                                DetectionMode::Tissue);

    QCOMPARE(recipe.schemaVersion, ProductRecipe::CurrentSchemaVersion);
    QCOMPARE(recipe.displayName, QStringLiteral("\u7eb8\u5dfe\u4ea7\u54c1"));
    QCOMPARE(detectionModeId(DetectionMode::Stamp), QStringLiteral("stamp_detection"));
    QCOMPARE(detectionModeId(DetectionMode::Word), QStringLiteral("word_detection"));
    QCOMPARE(detectionModeId(DetectionMode::Ocr), QStringLiteral("ocr_detection"));
    QCOMPARE(detectionModeId(recipe.detectionMode), QStringLiteral("tissue_detection"));
    QCOMPARE(detectionModeId(DetectionMode::BarcodeWord),
             QStringLiteral("barcode_word_detection"));
    QCOMPARE(recipe.tissueParameters.roughnessThreshold, 6.0);
    QCOMPARE(QUuid(recipe.recipeId).toString(QUuid::WithoutBraces), recipe.recipeId);

    QString errorMessage;
    QVERIFY2(validateProductRecipe(recipe, &errorMessage), qPrintable(errorMessage));
}

void ProductRecipeTest::jsonRoundTripRetainsModeParametersAndAssets()
{
    ProductRecipe original =
            createProductRecipe(QStringLiteral("\u4e8c\u7ef4\u7801\u4ea7\u54c1"),
                                DetectionMode::BarcodeWord);
    original.assets.insert(QStringLiteral("trackingTemplate"),
                           QStringLiteral("assets/tracking_template.bmp"));
    original.assets.insert(QStringLiteral("calibration"),
                           QStringLiteral("assets/calibrate_config.yaml"));
    original.assets.insert(QStringLiteral("characterA"),
                           QStringLiteral("assets/character_templates/A.bmp"));
    RecipeProfile profile = validProfile(QStringLiteral("profile-1"));
    profile.characterSourceImageSize = QSize(200, 100);
    RecipeCharacterBox characterBox;
    characterBox.name = QStringLiteral("A");
    characterBox.rect = QRect(5, 6, 20, 30);
    profile.characterBoxes.append(characterBox);
    profile.barcodeParameters.formatMask = 3u;
    profile.barcodeParameters.roiPaddingPercent = 12;
    profile.barcodeParameters.maxDecodeTimeMs = 75;
    profile.barcodeParameters.enableFallback = false;
    profile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                             QStringLiteral("trackingTemplate"));
    profile.assetKeys.insert(QStringLiteral("calibration"),
                             QStringLiteral("calibration"));
    profile.assetKeys.insert(QStringLiteral("character/A"),
                             QStringLiteral("characterA"));
    original.profiles.append(profile);

    ProductRecipe loaded;
    QString errorMessage;
    QVERIFY2(productRecipeFromJson(productRecipeToJson(original),
                                   &loaded,
                                   &errorMessage),
             qPrintable(errorMessage));

    QCOMPARE(loaded.schemaVersion, original.schemaVersion);
    QCOMPARE(loaded.recipeId, original.recipeId);
    QCOMPARE(loaded.displayName, original.displayName);
    QCOMPARE(detectionModeId(loaded.detectionMode),
             QStringLiteral("barcode_word_detection"));
    QVERIFY(loaded.assets == original.assets);
    QCOMPARE(loaded.profiles.size(), 1);
    const RecipeProfile loadedProfile = loaded.profiles.first();
    QCOMPARE(loadedProfile.name, profile.name);
    QCOMPARE(loadedProfile.targetText, profile.targetText);
    QCOMPARE(loadedProfile.imageThreshold, profile.imageThreshold);
    QCOMPARE(loadedProfile.trackingBox, profile.trackingBox);
    QCOMPARE(loadedProfile.hasValidBoxes, true);
    QCOMPARE(loadedProfile.characterSourceImageSize,
             profile.characterSourceImageSize);
    QCOMPARE(loadedProfile.characterBoxes.size(), 1);
    QCOMPARE(loadedProfile.characterBoxes.first().name,
             characterBox.name);
    QCOMPARE(loadedProfile.characterBoxes.first().rect,
             characterBox.rect);
    QCOMPARE(loadedProfile.barcodeParameters.formatMask, 3u);
    QCOMPARE(loadedProfile.barcodeParameters.roiPaddingPercent, 12);
    QCOMPARE(loadedProfile.barcodeParameters.maxDecodeTimeMs, 75);
    QCOMPARE(loadedProfile.barcodeParameters.enableFallback, false);
    QVERIFY(loadedProfile.assetKeys == profile.assetKeys);
}

void ProductRecipeTest::invalidFieldsAndEscapingAssetAreRejected()
{
    QString errorMessage;
    ProductRecipe invalid =
            createProductRecipe(QStringLiteral("\u975e\u6cd5\u7eb8\u5dfe\u4ea7\u54c1"),
                                DetectionMode::Tissue);
    invalid.tissueParameters.roughnessThreshold = 0.0;
    QVERIFY(!validateProductRecipe(invalid, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("roughnessThreshold")));

    invalid.tissueParameters.roughnessThreshold = 6.0;
    invalid.assets.insert(QStringLiteral("trackingTemplate"),
                          QStringLiteral("../outside.bmp"));
    QVERIFY(!validateProductRecipe(invalid, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("assets/")));

    ProductRecipe invalidProfileRecipe =
            createProductRecipe(QStringLiteral("invalid-profile"),
                                DetectionMode::Word);
    invalidProfileRecipe.assets.insert(
                QStringLiteral("trackingTemplate"),
                QStringLiteral("assets/tracking_template.bmp"));
    RecipeProfile invalidProfile = validProfile(QStringLiteral("profile"));
    QCOMPARE(invalidProfile.characterSourceImageSize, QSize(0, 0));
    invalidProfile.assetKeys.insert(QStringLiteral("calibration"),
                                    QStringLiteral("missingAsset"));
    invalidProfileRecipe.profiles.append(invalidProfile);
    QVERIFY(!validateProductRecipe(invalidProfileRecipe, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("asset reference")));

    invalidProfileRecipe.profiles[0].assetKeys.clear();
    invalidProfileRecipe.profiles[0].hasValidBoxes = false;
    QVERIFY(!validateProductRecipe(invalidProfileRecipe, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("trackingBox")));

    ProductRecipe unchanged =
            createProductRecipe(QStringLiteral("\u4fdd\u7559\u5bf9\u8c61"),
                                DetectionMode::Stamp);
    const QString unchangedId = unchanged.recipeId;
    QJsonObject unknownMode = productRecipeToJson(unchanged);
    unknownMode.insert(QStringLiteral("detectionMode"), QStringLiteral("unknown"));
    QVERIFY(!productRecipeFromJson(unknownMode, &unchanged, &errorMessage));
    QCOMPARE(unchanged.recipeId, unchangedId);
}

void ProductRecipeTest::runtimeSnapshotIsIndependentFromEditableRecipe()
{
    ProductRecipe editable =
            createProductRecipe(QStringLiteral("\u7eb8\u5dfeA"), DetectionMode::Tissue);
    QString errorMessage;
    const ProductRecipeSnapshot snapshot =
            makeProductRecipeSnapshot(editable, &errorMessage);
    QVERIFY2(static_cast<bool>(snapshot), qPrintable(errorMessage));

    editable.displayName = QStringLiteral("\u7eb8\u5dfeB");
    editable.tissueParameters.roughnessThreshold = 9.0;

    QCOMPARE(snapshot->displayName, QStringLiteral("\u7eb8\u5dfeA"));
    QCOMPARE(snapshot->tissueParameters.roughnessThreshold, 6.0);
}

void ProductRecipeTest::templatePrivateSettingsMappingRetainsProfileFields()
{
    TemplatePrivateSettings source;
    source.targetText = QStringLiteral("A1");
    source.imageThreshold = 63.0;
    source.trackingBox = cv::Rect2d(10.5, 20.25, 120.0, 80.0);
    source.hasValidBoxes = true;
    source.characterSourceImageSize = QSize(200, 100);
    CharacterTemplateBox characterBox;
    characterBox.name = QStringLiteral("A");
    characterBox.rect = QRect(5, 6, 20, 30);
    source.characterBoxes.append(characterBox);
    source.barcodeOptions.formatMask = 3u;
    source.barcodeOptions.roiPaddingPercent = 12;
    source.barcodeOptions.maxDecodeTimeMs = 75;
    source.barcodeOptions.enableFallback = false;

    QMap<QString, QString> assetKeys;
    assetKeys.insert(QStringLiteral("trackingTemplate"),
                     QStringLiteral("profileTracking"));
    assetKeys.insert(QStringLiteral("calibration"),
                     QStringLiteral("profileCalibration"));
    const RecipeProfile profile =
            recipeProfileFromTemplatePrivateSettings(
                QStringLiteral("profile-1"), source, assetKeys);

    QCOMPARE(profile.name, QStringLiteral("profile-1"));
    QCOMPARE(profile.targetText, source.targetText);
    QCOMPARE(profile.imageThreshold, source.imageThreshold);
    QCOMPARE(profile.trackingBox,
             QRectF(source.trackingBox.x,
                    source.trackingBox.y,
                    source.trackingBox.width,
                    source.trackingBox.height));
    QCOMPARE(profile.hasValidBoxes, source.hasValidBoxes);
    QCOMPARE(profile.characterSourceImageSize,
             source.characterSourceImageSize);
    QCOMPARE(profile.characterBoxes.size(), 1);
    QCOMPARE(profile.characterBoxes.first().name, characterBox.name);
    QCOMPARE(profile.characterBoxes.first().rect, characterBox.rect);
    QCOMPARE(profile.barcodeParameters.formatMask, 3u);
    QCOMPARE(profile.barcodeParameters.roiPaddingPercent, 12);
    QCOMPARE(profile.barcodeParameters.maxDecodeTimeMs, 75);
    QCOMPARE(profile.barcodeParameters.enableFallback, false);
    QVERIFY(profile.assetKeys == assetKeys);

    const TemplatePrivateSettings restored =
            templatePrivateSettingsFromRecipeProfile(profile);
    QCOMPARE(restored.configVersion, source.configVersion);
    QCOMPARE(restored.targetText, source.targetText);
    QCOMPARE(restored.imageThreshold, source.imageThreshold);
    QCOMPARE(restored.trackingBox.x, source.trackingBox.x);
    QCOMPARE(restored.trackingBox.y, source.trackingBox.y);
    QCOMPARE(restored.trackingBox.width, source.trackingBox.width);
    QCOMPARE(restored.trackingBox.height, source.trackingBox.height);
    QCOMPARE(restored.hasValidBoxes, source.hasValidBoxes);
    QCOMPARE(restored.characterSourceImageSize,
             source.characterSourceImageSize);
    QCOMPARE(restored.characterBoxes.size(), 1);
    QCOMPARE(restored.characterBoxes.first().name, characterBox.name);
    QCOMPARE(restored.characterBoxes.first().rect, characterBox.rect);
    QCOMPARE(restored.barcodeOptions.formatMask,
             source.barcodeOptions.formatMask);
    QCOMPARE(restored.barcodeOptions.roiPaddingPercent,
             source.barcodeOptions.roiPaddingPercent);
    QCOMPARE(restored.barcodeOptions.maxDecodeTimeMs,
             source.barcodeOptions.maxDecodeTimeMs);
    QCOMPARE(restored.barcodeOptions.enableFallback,
             source.barcodeOptions.enableFallback);
}

void ProductRecipeTest::templateProfileAssetManifestPreservesVariantsAndNamespaces()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    auto writeAsset = [&temporaryDirectory](const QString &fileName) {
        QFile file(temporaryDirectory.filePath(fileName));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write("asset") == 5;
    };
    QVERIFY(writeAsset(QStringLiteral("tracking_template.bmp")));
    QVERIFY(writeAsset(QStringLiteral("calibrate_config.yaml")));
    QVERIFY(writeAsset(QStringLiteral("template_raw.png")));
    QVERIFY(writeAsset(QStringLiteral("template_ring.bmp")));
    QVERIFY(writeAsset(QStringLiteral("A.png")));
    QVERIFY(writeAsset(QStringLiteral("A(1).png")));
    QVERIFY(writeAsset(QStringLiteral("A_2.JPG")));
    QVERIFY(writeAsset(QStringLiteral("app_settings.appset")));

    const TemplateProfileAssetManifest first =
            buildTemplateProfileAssetManifest(temporaryDirectory.path(), 2);
    QCOMPARE(first.recipeAssets.size(), 6);
    QCOMPARE(first.assetSourcePaths.size(), first.recipeAssets.size());
    QCOMPARE(first.profileAssetKeys.size(), first.recipeAssets.size());
    QCOMPARE(first.profileAssetKeys.value(QStringLiteral("trackingTemplate")),
             QStringLiteral("profile2.trackingTemplate"));
    QCOMPARE(first.profileAssetKeys.value(QStringLiteral("calibration")),
             QStringLiteral("profile2.calibration"));
    QCOMPARE(first.profileAssetKeys.value(QStringLiteral("rawImage")),
             QStringLiteral("profile2.rawImage"));

    const QStringList characterFileNames = {
        QStringLiteral("A.png"),
        QStringLiteral("A(1).png"),
        QStringLiteral("A_2.JPG")
    };
    for (const QString &fileName : characterFileNames) {
        const QString role = QStringLiteral("character/") + fileName;
        const QString assetKey = first.profileAssetKeys.value(role);
        QVERIFY(!assetKey.isEmpty());
        QCOMPARE(first.recipeAssets.value(assetKey),
                 QStringLiteral("assets/profiles/2/character_templates/")
                 + fileName);
        QCOMPARE(first.assetSourcePaths.value(assetKey),
                 QFileInfo(temporaryDirectory.filePath(fileName))
                 .absoluteFilePath());
    }
    QVERIFY(!first.assetSourcePaths.values().contains(
                QFileInfo(temporaryDirectory.filePath(
                              QStringLiteral("template_ring.bmp")))
                .absoluteFilePath()));

    const TemplateProfileAssetManifest second =
            buildTemplateProfileAssetManifest(temporaryDirectory.path(), 3);
    for (auto it = first.recipeAssets.constBegin();
         it != first.recipeAssets.constEnd();
         ++it) {
        QVERIFY(!second.recipeAssets.contains(it.key()));
        QVERIFY(!second.recipeAssets.values().contains(it.value()));
    }
}

QTEST_APPLESS_MAIN(ProductRecipeTest)

#include "product_recipe_test.moc"
