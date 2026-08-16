#include <QtTest>

#include "recipes/prepared_recipe.h"
#include "recipes/recipe_editor_session.h"
#include "recipes/recipe_store.h"
#include "application/inspection_start_preflight.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

#include <type_traits>
#include <memory>

#include <opencv2/imgcodecs.hpp>

namespace {

QString uuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool writeImage(const QString &path,
                const cv::Mat &image,
                const char *extension)
{
    std::vector<uchar> bytes;
    if (!cv::imencode(extension, image, bytes)) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(
                reinterpret_cast<const char *>(bytes.data()),
                static_cast<qint64>(bytes.size()))
               == static_cast<qint64>(bytes.size());
}

bool writeCalibration(const QString &path,
                      bool includeStamp,
                      bool includeBarcode)
{
    std::vector<cv::Point2f> stamp;
    if (includeStamp) {
        stamp.push_back(cv::Point2f(-3, -3));
        stamp.push_back(cv::Point2f(3, -3));
        stamp.push_back(cv::Point2f(0, 3));
    }
    std::vector<cv::Point2f> date;
    date.push_back(cv::Point2f(-5, -2));
    date.push_back(cv::Point2f(5, -2));
    date.push_back(cv::Point2f(0, 4));
    std::vector<cv::Point2f> barcode;
    if (includeBarcode) {
        barcode.push_back(cv::Point2f(-6, -6));
        barcode.push_back(cv::Point2f(6, -6));
        barcode.push_back(cv::Point2f(6, 6));
        barcode.push_back(cv::Point2f(-6, 6));
    }
    cv::FileStorage storage(
                "calibrate_config.yaml",
                cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    storage << "stamp_poly" << stamp;
    storage << "date_poly" << date;
    storage << "barcode_poly" << barcode;
    const std::string yaml = storage.releaseAndGetString();
    QFile file(path);
    return !yaml.empty()
            && file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(yaml.data(), static_cast<qint64>(yaml.size()))
               == static_cast<qint64>(yaml.size());
}

void addAsset(ProductRecipe *recipe,
              RecipeProfile *profile,
              QMap<QString, QString> *sources,
              const QString &key,
              const QString &role,
              const QString &relativePath,
              const QString &sourcePath)
{
    recipe->assets.insert(key, relativePath);
    profile->assetKeys.insert(role, key);
    sources->insert(key, sourcePath);
}

bool addProfile(ProductRecipe *recipe,
                int index,
                const QString &sourceRoot,
                QMap<QString, QString> *sources)
{
    const QString profileRoot = QDir(sourceRoot)
            .filePath(QStringLiteral("profile-%1").arg(index));
    if (!QDir().mkpath(profileRoot)) {
        return false;
    }
    const QString prefix = QStringLiteral("profile%1").arg(index);
    const QString relativeRoot =
            QStringLiteral("assets/profiles/%1/").arg(index);
    const QString rawPath = QDir(profileRoot)
            .filePath(QStringLiteral("raw.png"));
    const QString trackingPath = QDir(profileRoot)
            .filePath(QStringLiteral("tracking.bmp"));
    const QString calibrationPath = QDir(profileRoot)
            .filePath(QStringLiteral("calibrate_config.yaml"));
    if (!writeImage(rawPath,
                    cv::Mat(80, 80, CV_8UC3, cv::Scalar(10, 20, 30)),
                    ".png")
            || !writeImage(trackingPath,
                           cv::Mat(20, 20, CV_8UC3,
                                   cv::Scalar(40, 50, 60)),
                           ".bmp")
            || !writeCalibration(
                calibrationPath,
                recipe->detectionMode == DetectionMode::Stamp,
                recipe->detectionMode == DetectionMode::BarcodeWord)) {
        return false;
    }

    RecipeProfile profile;
    profile.name = QStringLiteral("Profile-%1").arg(index + 1);
    profile.targetText = QStringLiteral("1");
    profile.trackingRoi = QRectF(10, 10, 20, 20);
    addAsset(recipe, &profile, sources,
             prefix + QStringLiteral(".rawImage"),
             QStringLiteral("rawImage"),
             relativeRoot + QStringLiteral("template_raw.png"),
             rawPath);
    addAsset(recipe, &profile, sources,
             prefix + QStringLiteral(".trackingTemplate"),
             QStringLiteral("trackingTemplate"),
             relativeRoot + QStringLiteral("tracking_template.bmp"),
             trackingPath);
    addAsset(recipe, &profile, sources,
             prefix + QStringLiteral(".calibration"),
             QStringLiteral("calibration"),
             relativeRoot + QStringLiteral("calibrate_config.yaml"),
             calibrationPath);

    if (recipe->detectionMode == DetectionMode::Stamp) {
        const QString ringPath = QDir(profileRoot)
                .filePath(QStringLiteral("ring.bmp"));
        if (!writeImage(ringPath,
                        cv::Mat(16, 16, CV_8UC1, cv::Scalar(80)),
                        ".bmp")) {
            return false;
        }
        addAsset(recipe, &profile, sources,
                 prefix + QStringLiteral(".stampRing"),
                 QStringLiteral("stampRing"),
                 relativeRoot + QStringLiteral("template_ring.bmp"),
                 ringPath);
    }

    if (recipe->detectionMode == DetectionMode::Stamp
            || recipe->detectionMode == DetectionMode::Word
            || recipe->detectionMode == DetectionMode::BarcodeWord) {
        profile.imageThresholdPercent =
                RecipeProfile::DefaultImageThresholdPercent;
        profile.characterSourceSize = QSize(11, 7);
        RecipeCharacterBox box;
        box.name = QStringLiteral("1");
        box.rect = QRect(1, 1, 6, 5);
        profile.characterBoxes.append(box);
        const QString characterPath = QDir(profileRoot)
                .filePath(QStringLiteral("1.png"));
        if (!writeImage(characterPath,
                        cv::Mat(5, 6, CV_8UC1, cv::Scalar(120)),
                        ".png")) {
            return false;
        }
        addAsset(recipe, &profile, sources,
                 prefix + QStringLiteral(".character0"),
                 QStringLiteral("character/1.png"),
                 relativeRoot
                 + QStringLiteral("character_templates/1.png"),
                 characterPath);
    }
    recipe->profiles.append(profile);
    return true;
}

ProductRecipe makeRecipe(DetectionMode mode,
                         int profileCount,
                         const QString &sourceRoot,
                         QMap<QString, QString> *sources)
{
    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("Recipe-%1")
                .arg(detectionModeId(mode)),
                mode);
    if (mode == DetectionMode::Tissue) {
        recipe.tissueParameters.roughnessThreshold = 7.5;
        return recipe;
    }
    for (int i = 0; i < profileCount; ++i) {
        if (!addProfile(&recipe, i, sourceRoot, sources)) {
            return ProductRecipe();
        }
    }
    return recipe;
}

InspectionStartResourceInput acceptedPreflightInput(
        DetectionMode mode)
{
    InspectionStartResourceInput input;
    input.preparedRecipeReady = true;
    if (mode == DetectionMode::Tissue) {
        input.modeKind = InspectionStartModeKind::Tissue;
        return input;
    }
    if (mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord) {
        input.modeKind = mode == DetectionMode::Word
                ? InspectionStartModeKind::WordProfiles
                : InspectionStartModeKind::BarcodeWordProfiles;
        InspectionStartProfileReadiness profile;
        profile.displayName = QStringLiteral("Profile");
        profile.trackingTemplateReady = true;
        profile.calibrationReady = true;
        profile.barcodeRegionReady = true;
        profile.dateRegionReady = true;
        profile.targetTextReady = true;
        profile.characterTemplatesReady = true;
        input.profiles.append(profile);
        return input;
    }
    input.modeKind = InspectionStartModeKind::SingleTemplate;
    input.trackingTemplateReady = true;
    input.dateRegionReady = true;
    input.targetTextRequired = true;
    input.targetTextReady = true;
    input.characterTemplatesRequired =
            mode == DetectionMode::Stamp;
    input.characterTemplatesReady = true;
    return input;
}

} // namespace

class RecipeStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void fiveModesSaveLoadAndPrepare();
    void multipleProfilesArePrepared();
    void editorSessionPublishesReadOnlySnapshot();
    void failedUpdatePreservesPreviousRecipe();
    void missingCorruptAndIllegalAssetsAreRejected();
    void oldTemplateDirectoriesAreNotRecognized();
    void fiveModesPassStartupResourcePreflight();
};

void RecipeStoreTest::fiveModesSaveLoadAndPrepare()
{
    static_assert(
        std::is_const<
            PreparedRecipeSnapshot::element_type>::value,
        "PreparedRecipeSnapshot must be read-only");

    const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    for (DetectionMode mode : modes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString recipesRoot =
                QDir(directory.path()).filePath(QStringLiteral("recipes"));
        const QString sourcesRoot =
                QDir(directory.path()).filePath(QStringLiteral("sources"));
        QVERIFY(QDir().mkpath(sourcesRoot));
        QMap<QString, QString> sources;
        const ProductRecipe recipe =
                makeRecipe(mode, 1, sourcesRoot, &sources);
        QVERIFY(!recipe.recipeId.isEmpty());

        RecipeStore store(recipesRoot);
        QString error;
        QVERIFY2(store.saveRecipe(recipe, sources, &error),
                 qPrintable(error));
        QVERIFY(QFile::exists(
                    QDir(store.recipeDirectoryPath(recipe.recipeId))
                    .filePath(QStringLiteral("recipe.json"))));

        PreparedRecipeSnapshot prepared;
        QVERIFY2(store.loadPreparedRecipe(
                    recipe.recipeId, &prepared, &error),
                 qPrintable(error));
        QVERIFY(prepared);
        QVERIFY(prepared->recipe);
        QVERIFY(prepared->recipe->detectionMode == mode);
        if (mode == DetectionMode::Tissue) {
            QVERIFY(prepared->profiles.isEmpty());
            QCOMPARE(prepared->tissue.roughnessThreshold, 7.5);
        } else {
            QCOMPARE(prepared->profiles.size(), 1);
            QVERIFY(!prepared->profiles.first().rawImage.empty());
            QVERIFY(!prepared->profiles.first()
                    .trackingTemplate.empty());
        }
    }

    QTemporaryDir stagedDirectory;
    QVERIFY(stagedDirectory.isValid());
    QMap<QString, QString> stagedSources;
    ProductRecipe staged = makeRecipe(
                DetectionMode::Word,
                1,
                QDir(stagedDirectory.path())
                .filePath(QStringLiteral("sources")),
                &stagedSources);
    RecipeStore stagedStore(
                QDir(stagedDirectory.path())
                .filePath(QStringLiteral("recipes")));
    QString stagedError;

    ProductRecipe geometryOnly = staged;
    QMap<QString, QString> geometrySources = stagedSources;
    RecipeProfile &geometryProfile = geometryOnly.profiles[0];
    geometryProfile.targetText.clear();
    geometryProfile.characterSourceSize = QSize(0, 0);
    geometryProfile.characterBoxes.clear();
    const QString characterRole = QStringLiteral("character/1.png");
    const QString characterKey =
            geometryProfile.assetKeys.take(characterRole);
    geometryOnly.assets.remove(characterKey);
    geometrySources.remove(characterKey);
    QVERIFY2(stagedStore.saveRecipe(
                geometryOnly, geometrySources, &stagedError),
             qPrintable(stagedError));
    PreparedRecipeSnapshot stagedPrepared;
    QVERIFY2(stagedStore.loadPreparedRecipe(
                staged.recipeId, &stagedPrepared, &stagedError),
             qPrintable(stagedError));
    QVERIFY(stagedPrepared);
    QVERIFY(stagedPrepared->profiles.first()
            .characterAssets.empty());
    QVERIFY(stagedPrepared->profiles.first()
            .characterTemplates.empty());

    staged.profiles[0].targetText.clear();
    QVERIFY2(stagedStore.saveRecipe(
                staged, stagedSources, &stagedError),
             qPrintable(stagedError));
    QVERIFY2(stagedStore.loadPreparedRecipe(
                staged.recipeId, &stagedPrepared, &stagedError),
             qPrintable(stagedError));
    QVERIFY(stagedPrepared);
    QCOMPARE(static_cast<int>(stagedPrepared->profiles.first()
             .characterAssets.size()), 1);
    QVERIFY(stagedPrepared->profiles.first()
            .characterTemplates.empty());

    InspectionStartResourceInput incomplete;
    incomplete.modeKind = InspectionStartModeKind::WordProfiles;
    incomplete.preparedRecipeReady = true;
    InspectionStartProfileReadiness readiness;
    readiness.displayName = staged.profiles.first().name;
    readiness.trackingTemplateReady = true;
    readiness.calibrationReady = true;
    readiness.dateRegionReady = true;
    incomplete.profiles.append(readiness);
    QVERIFY(InspectionStartPreflight::evaluateResources(incomplete).issue
            == InspectionStartIssue::WordProfilesIncomplete);

    staged.profiles[0].targetText = QStringLiteral("1");
    QVERIFY2(stagedStore.saveRecipe(
                staged, stagedSources, &stagedError),
             qPrintable(stagedError));
    QVERIFY2(stagedStore.loadPreparedRecipe(
                staged.recipeId, &stagedPrepared, &stagedError),
             qPrintable(stagedError));
    QCOMPARE(static_cast<int>(stagedPrepared->profiles.first()
             .characterTemplates.size()), 1);
    QCOMPARE(stagedPrepared->profiles.first()
             .characterTemplateTargetIndexes.front(), 0);
}

void RecipeStoreTest::multipleProfilesArePrepared()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QMap<QString, QString> sources;
    const ProductRecipe recipe = makeRecipe(
                DetectionMode::Word,
                2,
                QDir(directory.path()).filePath(QStringLiteral("sources")),
                &sources);
    RecipeStore store(
                QDir(directory.path()).filePath(QStringLiteral("recipes")));
    QString error;
    QVERIFY2(store.saveRecipe(recipe, sources, &error),
             qPrintable(error));
    PreparedRecipeSnapshot prepared;
    QVERIFY2(store.loadPreparedRecipe(
                recipe.recipeId, &prepared, &error),
             qPrintable(error));
    QCOMPARE(prepared->profiles.size(), 2);
}

void RecipeStoreTest::editorSessionPublishesReadOnlySnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QMap<QString, QString> sources;
    ProductRecipe recipe = makeRecipe(
                DetectionMode::Word,
                1,
                QDir(directory.path()).filePath(QStringLiteral("sources")),
                &sources);
    RecipeStore store(
                QDir(directory.path()).filePath(QStringLiteral("recipes")));
    QString error;
    QVERIFY(store.saveRecipe(recipe, sources, &error));

    RecipeEditorSession session(
                QDir(directory.path())
                .filePath(QStringLiteral("editor")));
    QVERIFY2(session.beginEdit(store, recipe.recipeId, &error),
             qPrintable(error));
    const QString workspace = session.workspacePath();
    QVERIFY(QFileInfo(workspace).isDir());
    RecipeProfile profile = session.recipe().profiles.first();
    profile.imageThresholdPercent = 83;
    QVERIFY2(session.updateProfile(0, profile, &error),
             qPrintable(error));

    PreparedRecipeSnapshot prepared;
    QVERIFY2(session.publish(store, &prepared, &error),
             qPrintable(error));
    QVERIFY(prepared);
    QCOMPARE(prepared->recipe->profiles.first()
             .imageThresholdPercent, 83);
    session.reset();
    QVERIFY(!QFileInfo::exists(workspace));
}

void RecipeStoreTest::failedUpdatePreservesPreviousRecipe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recipesRoot =
            QDir(directory.path()).filePath(QStringLiteral("recipes"));
    QMap<QString, QString> sources;
    ProductRecipe recipe = makeRecipe(
                DetectionMode::Ocr,
                1,
                QDir(directory.path()).filePath(QStringLiteral("sources")),
                &sources);
    recipe.displayName = QStringLiteral("original");
    RecipeStore store(recipesRoot);
    QString error;
    QVERIFY2(store.saveRecipe(recipe, sources, &error),
             qPrintable(error));

    ProductRecipe update = recipe;
    update.displayName = QStringLiteral("updated");
    const std::shared_ptr<int> renameCalls(new int(0));
    RecipeStore failingStore(
                recipesRoot,
                [renameCalls](const QString &source,
                              const QString &destination) {
        ++(*renameCalls);
        if (*renameCalls == 2) {
            return false;
        }
        return QDir().rename(source, destination);
    });
    QVERIFY(!failingStore.saveRecipe(update, sources, &error));
    QVERIFY(error.startsWith(
                QStringLiteral("RECIPE_COMMIT_FAILED")));

    ProductRecipe restored;
    QVERIFY2(store.loadRecipe(recipe.recipeId, &restored, &error),
             qPrintable(error));
    QCOMPARE(restored.displayName, QStringLiteral("original"));
}

void RecipeStoreTest::missingCorruptAndIllegalAssetsAreRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString recipesRoot =
            QDir(directory.path()).filePath(QStringLiteral("recipes"));
    RecipeStore store(recipesRoot);
    QString error;

    QMap<QString, QString> sources;
    ProductRecipe illegal = makeRecipe(
                DetectionMode::Ocr, 1,
                QDir(directory.path()).filePath(QStringLiteral("illegal")),
                &sources);
    const QString rawKey =
            illegal.profiles.first().assetKeys.value(
                QStringLiteral("rawImage"));
    illegal.assets[rawKey] = QStringLiteral("../outside.png");
    QVERIFY(!store.saveRecipe(illegal, sources, &error));

    sources.clear();
    ProductRecipe missing = makeRecipe(
                DetectionMode::Ocr, 1,
                QDir(directory.path()).filePath(QStringLiteral("missing")),
                &sources);
    sources.remove(
                missing.profiles.first().assetKeys.value(
                    QStringLiteral("trackingTemplate")));
    QVERIFY(!store.saveRecipe(missing, sources, &error));

    sources.clear();
    ProductRecipe corruptImage = makeRecipe(
                DetectionMode::Word, 1,
                QDir(directory.path()).filePath(QStringLiteral("corrupt-image")),
                &sources);
    QFile badImage(sources.value(
        QStringLiteral("profile0.character0")));
    QVERIFY(badImage.open(QIODevice::WriteOnly | QIODevice::Truncate));
    badImage.write("not-an-image");
    badImage.close();
    QVERIFY(!store.saveRecipe(corruptImage, sources, &error));

    sources.clear();
    ProductRecipe corruptYaml = makeRecipe(
                DetectionMode::Ocr, 1,
                QDir(directory.path()).filePath(QStringLiteral("corrupt-yaml")),
                &sources);
    QFile badYaml(sources.value(
        QStringLiteral("profile0.calibration")));
    QVERIFY(badYaml.open(QIODevice::WriteOnly | QIODevice::Truncate));
    badYaml.write("not: [valid");
    badYaml.close();
    QVERIFY(!store.saveRecipe(corruptYaml, sources, &error));

    sources.clear();
    ProductRecipe removed = makeRecipe(
                DetectionMode::Ocr, 1,
                QDir(directory.path()).filePath(QStringLiteral("removed")),
                &sources);
    QVERIFY2(store.saveRecipe(removed, sources, &error),
             qPrintable(error));
    const QString trackingKey =
            removed.profiles.first().assetKeys.value(
                QStringLiteral("trackingTemplate"));
    const QString formalTracking =
            QDir(store.recipeDirectoryPath(removed.recipeId))
            .filePath(removed.assets.value(trackingKey));
    QVERIFY(QFile::remove(formalTracking));
    PreparedRecipeSnapshot prepared;
    QVERIFY(!store.loadPreparedRecipe(
                removed.recipeId, &prepared, &error));
}

void RecipeStoreTest::oldTemplateDirectoriesAreNotRecognized()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RecipeStore store(
                QDir(directory.path()).filePath(QStringLiteral("recipes")));
    const QString legacyName = QStringLiteral("legacy-template");
    const QString legacyNamedDirectory =
            QDir(store.recipesRootPath()).filePath(legacyName);
    QVERIFY(QDir().mkpath(legacyNamedDirectory));
    QFile namedIni(QDir(legacyNamedDirectory)
                   .filePath(QStringLiteral("settings.ini")));
    QVERIFY(namedIni.open(QIODevice::WriteOnly));
    namedIni.write("[Template]\nthreshold=70\n");
    namedIni.close();
    ProductRecipe recipe;
    QString error;
    QVERIFY(!store.loadRecipe(
                legacyName, &recipe, &error));
    QVERIFY(error.startsWith(
                QStringLiteral("RECIPE_LEGACY_FORMAT_REJECTED")));

    const QString legacyId = uuid();
    const QString legacyDirectory =
            store.recipeDirectoryPath(legacyId);
    QVERIFY(QDir().mkpath(legacyDirectory));
    QFile ini(QDir(legacyDirectory)
              .filePath(QStringLiteral("settings.ini")));
    QVERIFY(ini.open(QIODevice::WriteOnly));
    ini.write("[Template]\nthreshold=70\n");
    ini.close();
    QVERIFY(!store.loadRecipe(legacyId, &recipe, &error));

    RecipeStore invalidRoot(QStringLiteral(""));
    QVERIFY(!invalidRoot.loadRecipe(legacyId, &recipe, &error));
    QVERIFY(error.startsWith(
                QStringLiteral("RECIPE_ROOT_UNAVAILABLE")));

    RecipeCatalog catalog;
    QVERIFY(store.listRecipes(&catalog, &error));
    QVERIFY(catalog.recipes.isEmpty());
    QCOMPARE(catalog.invalidRecipes.size(), 1);
}

void RecipeStoreTest::fiveModesPassStartupResourcePreflight()
{
    const DetectionMode modes[] = {
        DetectionMode::Stamp,
        DetectionMode::Word,
        DetectionMode::Ocr,
        DetectionMode::Tissue,
        DetectionMode::BarcodeWord
    };
    for (DetectionMode mode : modes) {
        const InspectionStartResourceInput input =
                acceptedPreflightInput(mode);
        const InspectionStartPreflightResult result =
                InspectionStartPreflight::evaluateResources(input);
        QVERIFY2(result.isAccepted(),
                 qPrintable(result.details.join(
                                QLatin1Char('\n'))));
    }

    InspectionStartResourceInput missing;
    missing.modeKind = InspectionStartModeKind::Tissue;
    const InspectionStartPreflightResult rejected =
            InspectionStartPreflight::evaluateResources(missing);
    QVERIFY(rejected.issue
            == InspectionStartIssue::PreparedRecipeMissing);
}

QTEST_GUILESS_MAIN(RecipeStoreTest)
#include "recipe_store_test.moc"
