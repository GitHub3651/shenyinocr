#include <QtTest>

#include "recipes/product_recipe.h"
#include "system_support/settings/machine_settings_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUuid>

namespace {

QString uuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void addAsset(ProductRecipe *recipe,
              RecipeProfile *profile,
              const QString &prefix,
              const QString &role,
              const QString &relativePath)
{
    const QString key = prefix + QLatin1Char('.') + role;
    recipe->assets.insert(key, relativePath);
    profile->assetKeys.insert(role, key);
}

RecipeProfile createProfile(ProductRecipe *recipe,
                            const QString &name,
                            int index)
{
    RecipeProfile profile;
    profile.name = name;
    profile.targetText = QStringLiteral("1");
    profile.trackingRoi = QRectF(10, 10, 20, 20);
    const QString prefix = QStringLiteral("profile%1").arg(index);
    const QString root = QStringLiteral("assets/profiles/%1/").arg(index);
    addAsset(recipe, &profile, prefix, QStringLiteral("rawImage"),
             root + QStringLiteral("template_raw.png"));
    addAsset(recipe, &profile, prefix, QStringLiteral("trackingTemplate"),
             root + QStringLiteral("tracking_template.bmp"));
    addAsset(recipe, &profile, prefix, QStringLiteral("calibration"),
             root + QStringLiteral("calibrate_config.yaml"));

    const DetectionMode mode = recipe->detectionMode;
    if (mode == DetectionMode::Stamp) {
        addAsset(recipe, &profile, prefix, QStringLiteral("stampRing"),
                 root + QStringLiteral("template_ring.bmp"));
    }
    if (mode == DetectionMode::Stamp
            || mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord) {
        profile.imageThresholdPercent = 70;
        profile.characterSourceSize = QSize(30, 20);
        RecipeCharacterBox box;
        box.name = QStringLiteral("1");
        box.rect = QRect(1, 1, 6, 8);
        profile.characterBoxes.append(box);
        addAsset(recipe, &profile, prefix,
                 QStringLiteral("character/1.png"),
                 root + QStringLiteral("character_templates/1.png"));
    }
    return profile;
}

ProductRecipe createRecipe(DetectionMode mode, int profileCount = 1)
{
    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("产品配方"), mode);
    if (mode == DetectionMode::Tissue) {
        recipe.tissueParameters.roughnessThreshold = 6.25;
        return recipe;
    }
    for (int i = 0; i < profileCount; ++i) {
        recipe.profiles.append(createProfile(
            &recipe, QStringLiteral("Profile-%1").arg(i + 1), i));
    }
    return recipe;
}

} // namespace

class ProductRecipeTest : public QObject
{
    Q_OBJECT

private slots:
    void machineSettingsFirstStartSaveReload();
    void machineSettingsRestoreDefaultsAndClear();
    void machineSettingsCorruptionAndLegacyAreRejected();
    void fiveDetectionModesRoundTrip();
    void singleAndMultipleProfilesRoundTrip();
    void tissueRecipeHasNoTemplateAssets();
    void oldRecipeShapeIsRejected();
};

void ProductRecipeTest::machineSettingsFirstStartSaveReload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MachineSettingsStore store(directory.path());

    MachineSettings loaded;
    MachineSettingsLoadStatus status =
            MachineSettingsLoadStatus::Loaded;
    MachineSettingsStoreError error;
    QVERIFY2(store.load(&loaded, &status, &error),
             qPrintable(error.userMessage));
    QVERIFY(status == MachineSettingsLoadStatus::FirstRun);
    QVERIFY(loaded == MachineSettings::defaults());
    QVERIFY(!QFile::exists(store.settingsFilePath()));

    MachineSettings saved = loaded;
    saved.cameraExposure = 1234;
    saved.cameraGain = 7;
    saved.detectModeId = QStringLiteral("barcode_word_detection");
    saved.imageSaveModeId = QStringLiteral("save_all");
    saved.imageSavePath = QDir(directory.path())
            .filePath(QStringLiteral("images"));
    saved.publishedRecipeIdsByMode.insert(
                saved.detectModeId, uuid());
    QVERIFY2(store.save(saved, &error),
             qPrintable(error.userMessage));
    QFile settingsJson(store.settingsFilePath());
    QVERIFY(settingsJson.open(QIODevice::ReadOnly));
    const QByteArray persistedJson = settingsJson.readAll();
    QVERIFY(persistedJson.contains("\"barcodeWord\""));
    QVERIFY(persistedJson.contains("\"all\""));
    QVERIFY(!persistedJson.contains("barcode_word_detection"));
    QVERIFY(!persistedJson.contains("save_all"));
    settingsJson.close();

    MachineSettings reloaded;
    QVERIFY2(store.load(&reloaded, &status, &error),
             qPrintable(error.userMessage));
    QVERIFY(status == MachineSettingsLoadStatus::Loaded);
    QVERIFY(reloaded == saved);
}

void ProductRecipeTest::machineSettingsRestoreDefaultsAndClear()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MachineSettingsStore store(directory.path());
    MachineSettings settings = MachineSettings::defaults();
    settings.cameraExposure = 2345;
    MachineSettingsStoreError error;
    QVERIFY(store.save(settings, &error));

    MachineSettings restored;
    QVERIFY2(store.restoreDefaults(&restored, &error),
             qPrintable(error.userMessage));
    QVERIFY(restored == MachineSettings::defaults());

    MachineSettingsLoadStatus status;
    MachineSettings loaded;
    QVERIFY(store.load(&loaded, &status, &error));
    QVERIFY(loaded == MachineSettings::defaults());
    QVERIFY(status == MachineSettingsLoadStatus::Loaded);

    QVERIFY2(store.clear(&error), qPrintable(error.userMessage));
    QVERIFY(store.load(&loaded, &status, &error));
    QVERIFY(status == MachineSettingsLoadStatus::FirstRun);
    QVERIFY(loaded == MachineSettings::defaults());
}

void ProductRecipeTest::machineSettingsCorruptionAndLegacyAreRejected()
{
    QTemporaryDir legacyDirectory;
    QVERIFY(legacyDirectory.isValid());
    QFile legacy(QDir(legacyDirectory.path())
                 .filePath(QStringLiteral("settings.ini")));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    legacy.write("[Global]\nexposure=800\n");
    legacy.close();

    MachineSettingsStore legacyStore(legacyDirectory.path());
    MachineSettings settings;
    MachineSettingsLoadStatus status;
    MachineSettingsStoreError error;
    QVERIFY(legacyStore.load(&settings, &status, &error));
    QVERIFY(status == MachineSettingsLoadStatus::FirstRun);

    QTemporaryDir corruptDirectory;
    QVERIFY(corruptDirectory.isValid());
    MachineSettingsStore corruptStore(corruptDirectory.path());
    QVERIFY(QDir().mkpath(
                QFileInfo(corruptStore.settingsFilePath())
                .absolutePath()));
    QFile corrupt(corruptStore.settingsFilePath());
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("[Global]\nexposure=800\n");
    corrupt.close();
    QVERIFY(!corruptStore.load(&settings, &status, &error));
    QVERIFY(!error.code.isEmpty());

    QTemporaryDir oldIdDirectory;
    QVERIFY(oldIdDirectory.isValid());
    MachineSettingsStore oldIdStore(oldIdDirectory.path());
    QVERIFY(oldIdStore.save(MachineSettings::defaults(), &error));
    QFile oldIdJson(oldIdStore.settingsFilePath());
    QVERIFY(oldIdJson.open(QIODevice::ReadOnly));
    QByteArray oldIdBytes = oldIdJson.readAll();
    oldIdJson.close();
    oldIdBytes.replace("\"word\"", "\"word_detection\"");
    QVERIFY(oldIdJson.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(oldIdJson.write(oldIdBytes), oldIdBytes.size());
    oldIdJson.close();
    QVERIFY(!oldIdStore.load(&settings, &status, &error));
    QCOMPARE(error.code,
             QStringLiteral("SETTINGS_FIELD_RANGE_INVALID"));

    MachineSettingsStore invalidRoot(QStringLiteral(""));
    QVERIFY(!invalidRoot.load(&settings, &status, &error));
    QCOMPARE(error.code, QStringLiteral("DATA_ROOT_UNAVAILABLE"));
}

void ProductRecipeTest::fiveDetectionModesRoundTrip()
{
    const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    for (DetectionMode mode : modes) {
        const ProductRecipe source = createRecipe(mode);
        QString validationError;
        QVERIFY2(validateProductRecipe(source, &validationError),
                 qPrintable(validationError));
        ProductRecipe restored;
        QVERIFY2(productRecipeFromJson(
                    productRecipeToJson(source),
                    &restored,
                    &validationError),
                 qPrintable(validationError));
        QCOMPARE(productRecipeToJson(restored),
                 productRecipeToJson(source));
        QVERIFY(!detectionModeUiId(mode).isEmpty());
    }
}

void ProductRecipeTest::singleAndMultipleProfilesRoundTrip()
{
    ProductRecipe single = createRecipe(DetectionMode::Stamp);
    QCOMPARE(single.profiles.size(), 1);
    QVERIFY(validateProductRecipe(single));

    ProductRecipe multiple = createRecipe(DetectionMode::Word, 2);
    QCOMPARE(multiple.profiles.size(), 2);
    QVERIFY(validateProductRecipe(multiple));
    ProductRecipe restored;
    QString error;
    QVERIFY2(productRecipeFromJson(
                productRecipeToJson(multiple),
                &restored,
                &error), qPrintable(error));
    QCOMPARE(restored.profiles.size(), 2);

    ProductRecipe geometryOnly = createRecipe(DetectionMode::Word);
    RecipeProfile &profile = geometryOnly.profiles[0];
    profile.targetText.clear();
    profile.characterSourceSize = QSize(0, 0);
    profile.characterBoxes.clear();
    const QStringList roles = profile.assetKeys.keys();
    for (const QString &role : roles) {
        if (role.startsWith(QLatin1String("character/"))) {
            geometryOnly.assets.remove(profile.assetKeys.take(role));
        }
    }
    QVERIFY2(validateProductRecipe(geometryOnly, &error),
             qPrintable(error));
    QVERIFY2(productRecipeFromJson(
                productRecipeToJson(geometryOnly),
                &restored,
                &error), qPrintable(error));
    QVERIFY(restored.profiles.first().targetText.isEmpty());
    QVERIFY(restored.profiles.first().characterBoxes.isEmpty());
}

void ProductRecipeTest::tissueRecipeHasNoTemplateAssets()
{
    const ProductRecipe tissue =
            createRecipe(DetectionMode::Tissue);
    QVERIFY(tissue.profiles.isEmpty());
    QVERIFY(tissue.assets.isEmpty());
    QVERIFY(validateProductRecipe(tissue));
    QCOMPARE(tissue.tissueParameters.roughnessThreshold, 6.25);
}

void ProductRecipeTest::oldRecipeShapeIsRejected()
{
    ProductRecipe source = createRecipe(DetectionMode::Word);
    QJsonObject json = productRecipeToJson(source);
    json.insert(QStringLiteral("detectionMode"),
                QStringLiteral("word_detection"));
    ProductRecipe restored;
    QString error;
    QVERIFY(!productRecipeFromJson(json, &restored, &error));

    json = productRecipeToJson(source);
    json.insert(QStringLiteral("TemplatePrivateSettings"),
                QJsonObject());
    QVERIFY(!productRecipeFromJson(json, &restored, &error));

    ProductRecipe duplicateAsset = source;
    RecipeProfile &profile = duplicateAsset.profiles[0];
    profile.assetKeys[QStringLiteral("rawImage")] =
            profile.assetKeys.value(QStringLiteral("trackingTemplate"));
    QVERIFY(!validateProductRecipe(duplicateAsset, &error));
}

QTEST_GUILESS_MAIN(ProductRecipeTest)
#include "product_recipe_test.moc"
