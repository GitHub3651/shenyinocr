#include <QtTest/QtTest>

#include "recipe_selection.h"
#include "recipe_store.h"
#include "template_profile_load_plan.h"
#include "template_recipe_assembler.h"
#include "template_recipe_draft_session.h"
#include "template_recipe_publisher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

class RecipeStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void saveAndLoadCopiesRecipeJsonAndAssets();
    void successfulOverwriteReplacesWholeDirectory();
    void missingAssetSourcePreservesPreviousRecipe();
    void validationAndCommitFailuresPreservePreviousRecipe();
    void catalogListsValidatedRecipesAndReportsInvalidDirectories();
    void selectionResolvesOrderedProfileAssets();
    void selectionFailurePreservesPreviousOutput();
    void selectionBatchPreservesOrderAndReportsRejectedRecipes();
    void selectionBatchAllRejectedPreservesPreviousOutput();
    void profileLoadPlanOrdersCharacterVariantsByTarget();
    void profileLoadPlanReportsPendingTargetsWithoutPartialAssets();
    void recipeLoadPlanPreservesProfileOrderAndTargetUnits();
    void recipeLoadPlanFailurePreservesPreviousOutput();
    void selectedRecipeAssemblyCanBeSavedBackFromInternalAssets();
    void selectedRecipeAssemblyFailurePreservesPreviousOutput();
    void publishingCommitsASelectableRecipe();
    void publishingFailurePreservesPreviousRecipeAndOutput();
    void republishingSameHeaderKeepsIdentityAndReplacesAssets();
    void draftSessionRejectsChangedSourceAndKeepsIdentity();
};

namespace {

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

ProductRecipe recipeWithAsset(const QString &displayName,
                              const QString &assetKey,
                              const QString &relativeAssetPath)
{
    ProductRecipe recipe = createProductRecipe(displayName, DetectionMode::Word);
    recipe.assets.insert(assetKey, relativeAssetPath);
    RecipeProfile profile;
    profile.name = QStringLiteral("profile");
    profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profile.hasValidBoxes = true;
    profile.assetKeys.insert(QStringLiteral("testAsset"), assetKey);
    recipe.profiles.append(profile);
    return recipe;
}

} // namespace

void RecipeStoreTest::saveAndLoadCopiesRecipeJsonAndAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("source.bmp"));
    QVERIFY(writeBytes(sourcePath, QByteArray("image-bytes")));

    const QString recipesRoot = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("recipes"));
    const RecipeStore store(recipesRoot);
    ProductRecipe recipe = recipeWithAsset(
                QStringLiteral("word-product"),
                QStringLiteral("trackingTemplate"),
                QStringLiteral("assets/tracking_template.bmp"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("trackingTemplate"), sourcePath);

    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));

    ProductRecipe loaded;
    QVERIFY2(store.loadRecipe(recipe.recipeId, &loaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loaded.recipeId, recipe.recipeId);
    QCOMPARE(loaded.displayName, recipe.displayName);
    QVERIFY(loaded.assets == recipe.assets);

    const QString recipeDirectory = store.recipeDirectoryPath(recipe.recipeId);
    QVERIFY(QFileInfo::exists(QDir(recipeDirectory).filePath(
                                  QStringLiteral("recipe.json"))));
    QCOMPARE(readBytes(QDir(recipeDirectory).filePath(
                               QStringLiteral("assets/tracking_template.bmp"))),
             QByteArray("image-bytes"));

    ProductRecipe tissueRecipe = createProductRecipe(
                QStringLiteral("tissue-product"), DetectionMode::Tissue);
    QVERIFY2(store.saveRecipe(tissueRecipe,
                              QMap<QString, QString>(),
                              &errorMessage),
             qPrintable(errorMessage));
    ProductRecipe loadedTissueRecipe;
    QVERIFY2(store.loadRecipe(tissueRecipe.recipeId,
                              &loadedTissueRecipe,
                              &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loadedTissueRecipe.tissueParameters.roughnessThreshold, 6.0);
    QVERIFY(QDir(QDir(store.recipeDirectoryPath(tissueRecipe.recipeId))
                 .filePath(QStringLiteral("assets"))).exists());

    ProductRecipe unchanged = loadedTissueRecipe;
    QVERIFY(!store.loadRecipe(QStringLiteral("not-a-recipe-id"),
                              &unchanged,
                              &errorMessage));
    QCOMPARE(unchanged.recipeId, loadedTissueRecipe.recipeId);
    QCOMPARE(unchanged.displayName, loadedTissueRecipe.displayName);
}

void RecipeStoreTest::successfulOverwriteReplacesWholeDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    const QString firstSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first.bin"));
    const QString secondSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second.bin"));
    QVERIFY(writeBytes(firstSource, QByteArray("first")));
    QVERIFY(writeBytes(secondSource, QByteArray("second")));

    ProductRecipe recipe = recipeWithAsset(
                QStringLiteral("before"),
                QStringLiteral("oldAsset"),
                QStringLiteral("assets/old.bin"));
    QMap<QString, QString> firstSources;
    firstSources.insert(QStringLiteral("oldAsset"), firstSource);
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, firstSources, &errorMessage),
             qPrintable(errorMessage));

    recipe.displayName = QStringLiteral("after");
    recipe.assets.clear();
    recipe.assets.insert(QStringLiteral("newAsset"),
                         QStringLiteral("assets/new.bin"));
    recipe.profiles[0].assetKeys.clear();
    recipe.profiles[0].assetKeys.insert(QStringLiteral("testAsset"),
                                        QStringLiteral("newAsset"));
    QMap<QString, QString> secondSources;
    secondSources.insert(QStringLiteral("newAsset"), secondSource);
    QVERIFY2(store.saveRecipe(recipe, secondSources, &errorMessage),
             qPrintable(errorMessage));

    ProductRecipe loaded;
    QVERIFY2(store.loadRecipe(recipe.recipeId, &loaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loaded.displayName, QStringLiteral("after"));
    const QDir recipeDirectory(store.recipeDirectoryPath(recipe.recipeId));
    QVERIFY(!QFileInfo::exists(recipeDirectory.filePath(
                                   QStringLiteral("assets/old.bin"))));
    QCOMPARE(readBytes(recipeDirectory.filePath(QStringLiteral("assets/new.bin"))),
             QByteArray("second"));
}

void RecipeStoreTest::missingAssetSourcePreservesPreviousRecipe()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    const QString originalSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("original.bin"));
    QVERIFY(writeBytes(originalSource, QByteArray("original")));

    ProductRecipe recipe = recipeWithAsset(
                QStringLiteral("original-name"),
                QStringLiteral("asset"),
                QStringLiteral("assets/data.bin"));
    QMap<QString, QString> originalSources;
    originalSources.insert(QStringLiteral("asset"), originalSource);
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, originalSources, &errorMessage),
             qPrintable(errorMessage));

    recipe.displayName = QStringLiteral("replacement-name");
    QMap<QString, QString> missingSources;
    missingSources.insert(
                QStringLiteral("asset"),
                QDir(temporaryDirectory.path()).filePath(
                    QStringLiteral("missing.bin")));
    QVERIFY(!store.saveRecipe(recipe, missingSources, &errorMessage));

    ProductRecipe loaded;
    QVERIFY2(store.loadRecipe(recipe.recipeId, &loaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loaded.displayName, QStringLiteral("original-name"));
    QCOMPARE(readBytes(QDir(store.recipeDirectoryPath(recipe.recipeId)).filePath(
                           QStringLiteral("assets/data.bin"))),
             QByteArray("original"));
}

void RecipeStoreTest::validationAndCommitFailuresPreservePreviousRecipe()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString recipesRoot = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("recipes"));
    const RecipeStore initialStore(recipesRoot);
    const QString originalSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("valid.bin"));
    const QString replacementSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("rejected.bin"));
    QVERIFY(writeBytes(originalSource, QByteArray("valid")));
    QVERIFY(writeBytes(replacementSource, QByteArray("rejected")));

    ProductRecipe recipe = recipeWithAsset(
                QStringLiteral("valid-recipe"),
                QStringLiteral("asset"),
                QStringLiteral("assets/data.bin"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("asset"), originalSource);
    QString errorMessage;
    QVERIFY2(initialStore.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));

    const RecipeStore rejectingStore(
                recipesRoot,
                [](const ProductRecipe &,
                   const QString &,
                   const QString &,
                   QString *validatorError) {
        if (validatorError) {
            *validatorError = QStringLiteral("asset rejected by validator");
        }
        return false;
    });
    recipe.displayName = QStringLiteral("rejected-recipe");
    sources[QStringLiteral("asset")] = replacementSource;
    QVERIFY(!rejectingStore.saveRecipe(recipe, sources, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("rejected")));

    ProductRecipe loaded;
    QVERIFY2(initialStore.loadRecipe(recipe.recipeId, &loaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loaded.displayName, QStringLiteral("valid-recipe"));
    QCOMPARE(readBytes(QDir(initialStore.recipeDirectoryPath(recipe.recipeId))
                       .filePath(QStringLiteral("assets/data.bin"))),
             QByteArray("valid"));

    int renameCallCount = 0;
    const RecipeStore failingCommitStore(
                recipesRoot,
                RecipeStore::AssetValidator(),
                [&renameCallCount](const QString &sourceDirectoryPath,
                                   const QString &destinationDirectoryPath) {
        ++renameCallCount;
        if (renameCallCount == 2) {
            return false;
        }
        return QDir().rename(sourceDirectoryPath, destinationDirectoryPath);
    });
    recipe.displayName = QStringLiteral("commit-failure-recipe");
    QVERIFY(!failingCommitStore.saveRecipe(recipe, sources, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("restored")));
    QCOMPARE(renameCallCount, 3);

    QVERIFY2(initialStore.loadRecipe(recipe.recipeId, &loaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loaded.displayName, QStringLiteral("valid-recipe"));
    QCOMPARE(readBytes(QDir(initialStore.recipeDirectoryPath(recipe.recipeId))
                       .filePath(QStringLiteral("assets/data.bin"))),
             QByteArray("valid"));

    const QStringList transactionDirectories = QDir(recipesRoot).entryList(
                QStringList()
                << recipe.recipeId + QStringLiteral(".tmp.*")
                << recipe.recipeId + QStringLiteral(".bak.*"),
                QDir::Dirs | QDir::NoDotAndDotDot);
    QVERIFY(transactionDirectories.isEmpty());
}

void RecipeStoreTest::catalogListsValidatedRecipesAndReportsInvalidDirectories()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString recipesRoot = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("recipes"));
    const RecipeStore store(recipesRoot);
    RecipeCatalog catalog;
    QString errorMessage;
    QVERIFY2(store.listRecipes(&catalog, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(catalog.recipes.isEmpty());
    QVERIFY(catalog.invalidRecipes.isEmpty());
    QVERIFY(!QFileInfo::exists(recipesRoot));

    ProductRecipe wordRecipe = recipeWithAsset(
                QStringLiteral("zeta-product"),
                QStringLiteral("trackingTemplate"),
                QStringLiteral("assets/tracking_template.bmp"));
    const QString assetSourcePath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("source.bmp"));
    QVERIFY(writeBytes(assetSourcePath, QByteArray("image-bytes")));
    QMap<QString, QString> wordSources;
    wordSources.insert(QStringLiteral("trackingTemplate"), assetSourcePath);
    QVERIFY2(store.saveRecipe(wordRecipe, wordSources, &errorMessage),
             qPrintable(errorMessage));

    ProductRecipe tissueRecipe = createProductRecipe(
                QStringLiteral("Alpha-product"), DetectionMode::Tissue);
    QVERIFY2(store.saveRecipe(tissueRecipe,
                              QMap<QString, QString>(),
                              &errorMessage),
             qPrintable(errorMessage));

    const QString invalidRecipeId =
            QStringLiteral("11111111-2222-3333-4444-555555555555");
    const QString invalidDirectory = QDir(recipesRoot).filePath(
                invalidRecipeId);
    QVERIFY(QDir().mkpath(invalidDirectory));
    QVERIFY(writeBytes(QDir(invalidDirectory).filePath(
                           QStringLiteral("recipe.json")),
                       QByteArray("not-json")));
    QVERIFY(QDir().mkpath(QDir(recipesRoot).filePath(
                              wordRecipe.recipeId
                              + QStringLiteral(".tmp.ignored"))));
    QVERIFY(QDir().mkpath(QDir(recipesRoot).filePath(
                              QStringLiteral("not-a-recipe"))));
    QVERIFY(QDir().mkpath(QDir(recipesRoot).filePath(
                              QStringLiteral(
                                  "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"))));

    QVERIFY2(store.listRecipes(&catalog, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(catalog.recipes.size(), 2);
    QCOMPARE(catalog.recipes.at(0).recipeId, tissueRecipe.recipeId);
    QCOMPARE(catalog.recipes.at(0).displayName,
             QStringLiteral("Alpha-product"));
    QCOMPARE(detectionModeId(catalog.recipes.at(0).detectionMode),
             QStringLiteral("tissue_detection"));
    QCOMPARE(catalog.recipes.at(0).profileCount, 0);
    QCOMPARE(catalog.recipes.at(1).recipeId, wordRecipe.recipeId);
    QCOMPARE(catalog.recipes.at(1).displayName,
             QStringLiteral("zeta-product"));
    QCOMPARE(detectionModeId(catalog.recipes.at(1).detectionMode),
             QStringLiteral("word_detection"));
    QCOMPARE(catalog.recipes.at(1).profileCount, 1);
    QCOMPARE(catalog.invalidRecipes.size(), 1);
    QCOMPARE(catalog.invalidRecipes.first().directoryName,
             invalidRecipeId);
    QVERIFY(catalog.invalidRecipes.first().message.contains(
                QStringLiteral("recipe.json")));

    const QString invalidRootPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("recipes-file"));
    QVERIFY(writeBytes(invalidRootPath, QByteArray("not-a-directory")));
    const RecipeStore invalidRootStore(invalidRootPath);
    const RecipeCatalog unchangedCatalog = catalog;
    QVERIFY(!invalidRootStore.listRecipes(&catalog, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Recipe root")));
    QCOMPARE(catalog.recipes.size(), unchangedCatalog.recipes.size());
    QCOMPARE(catalog.invalidRecipes.size(),
             unchangedCatalog.invalidRecipes.size());
}

void RecipeStoreTest::selectionResolvesOrderedProfileAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString firstTracking = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-tracking.bmp"));
    const QString firstCalibration = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-calibration.yaml"));
    const QString secondTracking = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-tracking.bmp"));
    const QString secondCalibration = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-calibration.yaml"));
    QVERIFY(writeBytes(firstTracking, QByteArray("first-tracking")));
    QVERIFY(writeBytes(firstCalibration, QByteArray("first-calibration")));
    QVERIFY(writeBytes(secondTracking, QByteArray("second-tracking")));
    QVERIFY(writeBytes(secondCalibration, QByteArray("second-calibration")));

    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("multi-profile"), DetectionMode::Word);
    RecipeProfile firstProfile;
    firstProfile.name = QStringLiteral("first");
    firstProfile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    firstProfile.hasValidBoxes = true;
    firstProfile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                                  QStringLiteral("firstTracking"));
    firstProfile.assetKeys.insert(QStringLiteral("calibration"),
                                  QStringLiteral("firstCalibration"));
    RecipeProfile secondProfile = firstProfile;
    secondProfile.name = QStringLiteral("second");
    secondProfile.assetKeys[QStringLiteral("trackingTemplate")] =
            QStringLiteral("secondTracking");
    secondProfile.assetKeys[QStringLiteral("calibration")] =
            QStringLiteral("secondCalibration");
    recipe.profiles.append(firstProfile);
    recipe.profiles.append(secondProfile);
    recipe.assets.insert(QStringLiteral("firstTracking"),
                         QStringLiteral("assets/profiles/0/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("firstCalibration"),
                         QStringLiteral("assets/profiles/0/calibration.yaml"));
    recipe.assets.insert(QStringLiteral("secondTracking"),
                         QStringLiteral("assets/profiles/1/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("secondCalibration"),
                         QStringLiteral("assets/profiles/1/calibration.yaml"));

    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("firstTracking"), firstTracking);
    sources.insert(QStringLiteral("firstCalibration"), firstCalibration);
    sources.insert(QStringLiteral("secondTracking"), secondTracking);
    sources.insert(QStringLiteral("secondCalibration"), secondCalibration);
    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));

    RecipeSelection selection;
    QVERIFY2(loadRecipeSelection(store,
                                 recipe.recipeId,
                                 DetectionMode::Word,
                                 &selection,
                                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(selection.recipe);
    QCOMPARE(selection.recipe->recipeId, recipe.recipeId);
    QCOMPARE(selection.recipeDirectoryPath,
             store.recipeDirectoryPath(recipe.recipeId));
    QCOMPARE(selection.profiles.size(), 2);
    QCOMPARE(selection.profiles.at(0).profile.name,
             QStringLiteral("first"));
    QCOMPARE(selection.profiles.at(1).profile.name,
             QStringLiteral("second"));
    QCOMPARE(readBytes(selection.profiles.at(0).assetPathsByRole.value(
                           QStringLiteral("trackingTemplate"))),
             QByteArray("first-tracking"));
    QCOMPARE(readBytes(selection.profiles.at(1).assetPathsByRole.value(
                           QStringLiteral("calibration"))),
             QByteArray("second-calibration"));
}

void RecipeStoreTest::selectionFailurePreservesPreviousOutput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe validRecipe = createProductRecipe(
                QStringLiteral("valid"), DetectionMode::Word);
    RecipeProfile validProfile;
    validProfile.name = QStringLiteral("profile");
    validProfile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    validProfile.hasValidBoxes = true;
    validProfile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                                  QStringLiteral("tracking"));
    validProfile.assetKeys.insert(QStringLiteral("calibration"),
                                  QStringLiteral("calibration"));
    validRecipe.profiles.append(validProfile);
    validRecipe.assets.insert(QStringLiteral("tracking"),
                              QStringLiteral("assets/tracking.bmp"));
    validRecipe.assets.insert(QStringLiteral("calibration"),
                              QStringLiteral("assets/calibration.yaml"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("tracking"), trackingSource);
    sources.insert(QStringLiteral("calibration"), calibrationSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(validRecipe, sources, &errorMessage),
             qPrintable(errorMessage));
    RecipeSelection selection;
    QVERIFY2(loadRecipeSelection(store,
                                 validRecipe.recipeId,
                                 DetectionMode::Word,
                                 &selection,
                                 &errorMessage),
             qPrintable(errorMessage));
    const QString originalRecipeId = selection.recipe->recipeId;
    const int originalProfileCount = selection.profiles.size();

    QVERIFY(!loadRecipeSelection(store,
                                 validRecipe.recipeId,
                                 DetectionMode::BarcodeWord,
                                 &selection,
                                 &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("mode")));
    QCOMPARE(selection.recipe->recipeId, originalRecipeId);
    QCOMPARE(selection.profiles.size(), originalProfileCount);

    ProductRecipe incompleteRecipe = createProductRecipe(
                QStringLiteral("incomplete"), DetectionMode::Word);
    RecipeProfile incompleteProfile = validProfile;
    incompleteProfile.name = QStringLiteral("incomplete-profile");
    incompleteProfile.assetKeys.clear();
    incompleteRecipe.profiles.append(incompleteProfile);
    QVERIFY2(store.saveRecipe(incompleteRecipe,
                              QMap<QString, QString>(),
                              &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(!loadRecipeSelection(store,
                                 incompleteRecipe.recipeId,
                                 DetectionMode::Word,
                                 &selection,
                                 &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("required runtime assets")));
    QCOMPARE(selection.recipe->recipeId, originalRecipeId);
    QCOMPARE(selection.profiles.size(), originalProfileCount);
}

void RecipeStoreTest::selectionBatchPreservesOrderAndReportsRejectedRecipes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    auto selectableRecipe = [](const QString &displayName,
                               const QString &profileName) {
        ProductRecipe recipe = createProductRecipe(displayName,
                                                   DetectionMode::Word);
        RecipeProfile profile;
        profile.name = profileName;
        profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
        profile.hasValidBoxes = true;
        profile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                                 QStringLiteral("tracking"));
        profile.assetKeys.insert(QStringLiteral("calibration"),
                                 QStringLiteral("calibration"));
        recipe.profiles.append(profile);
        recipe.assets.insert(QStringLiteral("tracking"),
                             QStringLiteral("assets/tracking.bmp"));
        recipe.assets.insert(QStringLiteral("calibration"),
                             QStringLiteral("assets/calibration.yaml"));
        return recipe;
    };

    ProductRecipe firstRecipe = selectableRecipe(QStringLiteral("first"),
                                                 QStringLiteral("first-profile"));
    ProductRecipe secondRecipe = selectableRecipe(QStringLiteral("second"),
                                                  QStringLiteral("second-profile"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("tracking"), trackingSource);
    sources.insert(QStringLiteral("calibration"), calibrationSource);
    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(firstRecipe, sources, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(store.saveRecipe(secondRecipe, sources, &errorMessage),
             qPrintable(errorMessage));
    const QString missingRecipeId = createProductRecipe(
                QStringLiteral("missing"), DetectionMode::Word).recipeId;

    RecipeSelectionBatch batch;
    QVERIFY2(loadRecipeSelectionBatch(
                 store,
                 QStringList()
                 << secondRecipe.recipeId
                 << missingRecipeId
                 << firstRecipe.recipeId
                 << secondRecipe.recipeId.toUpper(),
                 DetectionMode::Word,
                 &batch,
                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(batch.selections.size(), 2);
    QCOMPARE(batch.selections.at(0).recipe->recipeId,
             secondRecipe.recipeId);
    QCOMPARE(batch.selections.at(0).profiles.first().profile.name,
             QStringLiteral("second-profile"));
    QCOMPARE(batch.selections.at(1).recipe->recipeId,
             firstRecipe.recipeId);
    QCOMPARE(batch.selections.at(1).profiles.first().profile.name,
             QStringLiteral("first-profile"));
    QCOMPARE(batch.rejectedSelections.size(), 1);
    QCOMPARE(batch.rejectedSelections.first().recipeId, missingRecipeId);
    QVERIFY(!batch.rejectedSelections.first().message.isEmpty());
}

void RecipeStoreTest::selectionBatchAllRejectedPreservesPreviousOutput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe recipe = createProductRecipe(QStringLiteral("word"),
                                               DetectionMode::Word);
    RecipeProfile profile;
    profile.name = QStringLiteral("profile");
    profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profile.hasValidBoxes = true;
    profile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                             QStringLiteral("tracking"));
    profile.assetKeys.insert(QStringLiteral("calibration"),
                             QStringLiteral("calibration"));
    recipe.profiles.append(profile);
    recipe.assets.insert(QStringLiteral("tracking"),
                         QStringLiteral("assets/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("calibration"),
                         QStringLiteral("assets/calibration.yaml"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("tracking"), trackingSource);
    sources.insert(QStringLiteral("calibration"), calibrationSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));

    RecipeSelectionBatch batch;
    QVERIFY2(loadRecipeSelectionBatch(store,
                                      QStringList() << recipe.recipeId,
                                      DetectionMode::Word,
                                      &batch,
                                      &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(batch.selections.size(), 1);
    const QString originalRecipeId =
            batch.selections.first().recipe->recipeId;

    const QString missingRecipeId = createProductRecipe(
                QStringLiteral("missing"), DetectionMode::BarcodeWord)
            .recipeId;
    QVERIFY(!loadRecipeSelectionBatch(
                store,
                QStringList() << recipe.recipeId << missingRecipeId,
                DetectionMode::BarcodeWord,
                &batch,
                &errorMessage));
    QVERIFY(errorMessage.contains(
                QStringLiteral("No selected recipe could be loaded")));
    QVERIFY(errorMessage.contains(QStringLiteral("mode")));
    QCOMPARE(batch.selections.size(), 1);
    QCOMPARE(batch.selections.first().recipe->recipeId,
             originalRecipeId);
    QVERIFY(batch.rejectedSelections.isEmpty());
}

void RecipeStoreTest::profileLoadPlanOrdersCharacterVariantsByTarget()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    const QString rawImagePath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("raw.png"));
    const QString aExactPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("A.png"));
    const QString aParenthesizedPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("A(1).png"));
    const QString aUnderscorePath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("A_2.png"));
    const QString bExactPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("B.png"));
    const QStringList assetPaths = QStringList()
            << trackingPath
            << calibrationPath
            << rawImagePath
            << aExactPath
            << aParenthesizedPath
            << aUnderscorePath
            << bExactPath;
    for (const QString &path : assetPaths) {
        QVERIFY(writeBytes(path, path.toUtf8()));
    }

    ResolvedRecipeProfile resolvedProfile;
    resolvedProfile.profile.name = QStringLiteral("ordered-profile");
    resolvedProfile.profile.targetText = QStringLiteral("AB");
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), trackingPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("calibration"), calibrationPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("rawImage"), rawImagePath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("character/B.png"), bExactPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("character/A_2.png"), aUnderscorePath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("character/A.png"), aExactPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("character/A(1).png"),
                aParenthesizedPath);

    TemplateProfileLoadPlan loadPlan;
    QString errorMessage;
    QVERIFY2(buildTemplateProfileLoadPlan(
                 resolvedProfile,
                 QStringList() << QStringLiteral("a") << QStringLiteral("b"),
                 &loadPlan,
                 &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loadPlan.profile.name, QStringLiteral("ordered-profile"));
    QCOMPARE(loadPlan.rawImagePath, QFileInfo(rawImagePath).absoluteFilePath());
    QVERIFY(loadPlan.pendingTargetMessage.isEmpty());
    QCOMPARE(loadPlan.characterTemplates.size(), 4);
    QCOMPARE(loadPlan.characterTemplates.at(0).fileName,
             QStringLiteral("A.png"));
    QCOMPARE(loadPlan.characterTemplates.at(1).fileName,
             QStringLiteral("A(1).png"));
    QCOMPARE(loadPlan.characterTemplates.at(2).fileName,
             QStringLiteral("A_2.png"));
    QCOMPARE(loadPlan.characterTemplates.at(3).fileName,
             QStringLiteral("B.png"));
    QCOMPARE(loadPlan.characterTemplates.at(0).targetIndex, 0);
    QCOMPARE(loadPlan.characterTemplates.at(1).targetIndex, 0);
    QCOMPARE(loadPlan.characterTemplates.at(2).targetIndex, 0);
    QCOMPARE(loadPlan.characterTemplates.at(3).targetIndex, 1);
}

void RecipeStoreTest::profileLoadPlanReportsPendingTargetsWithoutPartialAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    const QString aExactPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("A.png"));
    QVERIFY(writeBytes(trackingPath, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationPath, QByteArray("calibration")));
    QVERIFY(writeBytes(aExactPath, QByteArray("a")));

    ResolvedRecipeProfile resolvedProfile;
    resolvedProfile.profile.name = QStringLiteral("pending-profile");
    resolvedProfile.profile.targetText = QStringLiteral("AB");
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), trackingPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("calibration"), calibrationPath);
    resolvedProfile.assetPathsByRole.insert(
                QStringLiteral("character/A.png"), aExactPath);

    TemplateProfileLoadPlan loadPlan;
    QString errorMessage;
    QVERIFY2(buildTemplateProfileLoadPlan(
                 resolvedProfile,
                 QStringList() << QStringLiteral("a") << QStringLiteral("b"),
                 &loadPlan,
                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(loadPlan.pendingTargetMessage.contains(QStringLiteral("b")));
    QVERIFY(loadPlan.characterTemplates.isEmpty());

    TemplateProfileLoadPlan unchangedPlan = loadPlan;
    unchangedPlan.profile.name = QStringLiteral("sentinel");
    resolvedProfile.assetPathsByRole.remove(QStringLiteral("calibration"));
    QVERIFY(!buildTemplateProfileLoadPlan(
                resolvedProfile,
                QStringList() << QStringLiteral("a"),
                &unchangedPlan,
                &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("calibration")));
    QCOMPARE(unchangedPlan.profile.name, QStringLiteral("sentinel"));
}

void RecipeStoreTest::recipeLoadPlanPreservesProfileOrderAndTargetUnits()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString firstTrackingPath =
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-tracking.bmp"));
    const QString firstCalibrationPath =
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-calibration.yaml"));
    const QString firstAPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("A(2).png"));
    const QString firstBPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("B.png"));
    const QString secondTrackingPath =
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-tracking.bmp"));
    const QString secondCalibrationPath =
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-calibration.yaml"));
    const QString secondCharacterPath =
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("3.png"));
    const QStringList assetPaths = QStringList()
            << firstTrackingPath
            << firstCalibrationPath
            << firstAPath
            << firstBPath
            << secondTrackingPath
            << secondCalibrationPath
            << secondCharacterPath;
    for (const QString &path : assetPaths) {
        QVERIFY(writeBytes(path, path.toUtf8()));
    }

    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("ordered-recipe"), DetectionMode::Word);
    RecipeProfile firstProfile;
    firstProfile.name = QStringLiteral("first");
    firstProfile.targetText = QStringLiteral("A(2)B");
    RecipeProfile secondProfile;
    secondProfile.name = QStringLiteral("second");
    secondProfile.targetText = QStringLiteral("3");
    recipe.profiles.append(firstProfile);
    recipe.profiles.append(secondProfile);

    ResolvedRecipeProfile firstResolved;
    firstResolved.profile = firstProfile;
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), firstTrackingPath);
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("calibration"), firstCalibrationPath);
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("character/A(2).png"), firstAPath);
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("character/B.png"), firstBPath);
    ResolvedRecipeProfile secondResolved;
    secondResolved.profile = secondProfile;
    secondResolved.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), secondTrackingPath);
    secondResolved.assetPathsByRole.insert(
                QStringLiteral("calibration"), secondCalibrationPath);
    secondResolved.assetPathsByRole.insert(
                QStringLiteral("character/3.png"), secondCharacterPath);

    RecipeSelection selection;
    selection.recipe = ProductRecipeSnapshot(new ProductRecipe(recipe));
    selection.recipeDirectoryPath = temporaryDirectory.path();
    selection.profiles.append(firstResolved);
    selection.profiles.append(secondResolved);

    TemplateRecipeLoadPlan loadPlan;
    QString errorMessage;
    QVERIFY2(buildTemplateRecipeLoadPlan(selection,
                                         &loadPlan,
                                         &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(loadPlan.recipe->recipeId, recipe.recipeId);
    QCOMPARE(loadPlan.recipeDirectoryPath, temporaryDirectory.path());
    QCOMPARE(loadPlan.profiles.size(), 2);
    QCOMPARE(loadPlan.profiles.at(0).profile.name,
             QStringLiteral("first"));
    QCOMPARE(loadPlan.profiles.at(0).targetUnits.join(QStringLiteral("|")),
             QStringLiteral("a(2)|b"));
    QCOMPARE(loadPlan.profiles.at(0).characterTemplates.size(), 2);
    QCOMPARE(loadPlan.profiles.at(0).characterTemplates.at(0).targetIndex, 0);
    QCOMPARE(loadPlan.profiles.at(0).characterTemplates.at(1).targetIndex, 1);
    QCOMPARE(loadPlan.profiles.at(1).profile.name,
             QStringLiteral("second"));
    QCOMPARE(loadPlan.profiles.at(1).targetUnits,
             QStringList() << QStringLiteral("3"));
    QCOMPARE(parseTemplateTargetUnits(QStringLiteral("---")).size(), 0);
}

void RecipeStoreTest::recipeLoadPlanFailurePreservesPreviousOutput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationPath = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingPath, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationPath, QByteArray("calibration")));

    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("invalid-second"), DetectionMode::Word);
    RecipeProfile firstProfile;
    firstProfile.name = QStringLiteral("first");
    RecipeProfile secondProfile;
    secondProfile.name = QStringLiteral("second");
    recipe.profiles.append(firstProfile);
    recipe.profiles.append(secondProfile);

    ResolvedRecipeProfile firstResolved;
    firstResolved.profile = firstProfile;
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), trackingPath);
    firstResolved.assetPathsByRole.insert(
                QStringLiteral("calibration"), calibrationPath);
    ResolvedRecipeProfile secondResolved;
    secondResolved.profile = secondProfile;
    secondResolved.assetPathsByRole.insert(
                QStringLiteral("trackingTemplate"), trackingPath);

    RecipeSelection selection;
    selection.recipe = ProductRecipeSnapshot(new ProductRecipe(recipe));
    selection.profiles.append(firstResolved);
    selection.profiles.append(secondResolved);

    ProductRecipe sentinelRecipe = createProductRecipe(
                QStringLiteral("sentinel"), DetectionMode::Word);
    TemplateRecipeLoadPlan unchangedPlan;
    unchangedPlan.recipe =
            ProductRecipeSnapshot(new ProductRecipe(sentinelRecipe));
    TemplateProfileLoadPlan sentinelProfile;
    sentinelProfile.profile.name = QStringLiteral("sentinel-profile");
    unchangedPlan.profiles.append(sentinelProfile);

    QString errorMessage;
    QVERIFY(!buildTemplateRecipeLoadPlan(selection,
                                         &unchangedPlan,
                                         &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("second")));
    QVERIFY(errorMessage.contains(QStringLiteral("calibration")));
    QCOMPARE(unchangedPlan.recipe->recipeId, sentinelRecipe.recipeId);
    QCOMPARE(unchangedPlan.profiles.size(), 1);
    QCOMPARE(unchangedPlan.profiles.first().profile.name,
             QStringLiteral("sentinel-profile"));
}

void RecipeStoreTest::selectedRecipeAssemblyCanBeSavedBackFromInternalAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString firstTracking = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-tracking.bmp"));
    const QString firstCalibration = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("first-calibration.yaml"));
    const QString secondTracking = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-tracking.bmp"));
    const QString secondCalibration = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("second-calibration.yaml"));
    QVERIFY(writeBytes(firstTracking, QByteArray("first-tracking")));
    QVERIFY(writeBytes(firstCalibration, QByteArray("first-calibration")));
    QVERIFY(writeBytes(secondTracking, QByteArray("second-tracking")));
    QVERIFY(writeBytes(secondCalibration, QByteArray("second-calibration")));

    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("before-edit"), DetectionMode::Word);
    RecipeProfile firstProfile;
    firstProfile.name = QStringLiteral("first");
    firstProfile.targetText = QStringLiteral("AB");
    firstProfile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    firstProfile.hasValidBoxes = true;
    firstProfile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                                  QStringLiteral("firstTracking"));
    firstProfile.assetKeys.insert(QStringLiteral("calibration"),
                                  QStringLiteral("firstCalibration"));
    RecipeProfile secondProfile = firstProfile;
    secondProfile.name = QStringLiteral("second");
    secondProfile.targetText = QStringLiteral("CD");
    secondProfile.assetKeys[QStringLiteral("trackingTemplate")] =
            QStringLiteral("secondTracking");
    secondProfile.assetKeys[QStringLiteral("calibration")] =
            QStringLiteral("secondCalibration");
    recipe.profiles.append(firstProfile);
    recipe.profiles.append(secondProfile);
    recipe.assets.insert(QStringLiteral("firstTracking"),
                         QStringLiteral("assets/profiles/0/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("firstCalibration"),
                         QStringLiteral("assets/profiles/0/calibration.yaml"));
    recipe.assets.insert(QStringLiteral("secondTracking"),
                         QStringLiteral("assets/profiles/1/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("secondCalibration"),
                         QStringLiteral("assets/profiles/1/calibration.yaml"));

    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("firstTracking"), firstTracking);
    sources.insert(QStringLiteral("firstCalibration"), firstCalibration);
    sources.insert(QStringLiteral("secondTracking"), secondTracking);
    sources.insert(QStringLiteral("secondCalibration"), secondCalibration);
    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));

    RecipeSelection selection;
    QVERIFY2(loadRecipeSelection(store,
                                 recipe.recipeId,
                                 DetectionMode::Word,
                                 &selection,
                                 &errorMessage),
             qPrintable(errorMessage));
    TemplateRecipeAssembly assembly;
    QVERIFY2(assembleSelectedTemplateRecipe(selection,
                                            &assembly,
                                            &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(assembly.recipe.recipeId, recipe.recipeId);
    QCOMPARE(assembly.recipe.profiles.size(), 2);
    QCOMPARE(assembly.recipe.profiles.at(0).name, QStringLiteral("first"));
    QCOMPARE(assembly.recipe.profiles.at(1).name, QStringLiteral("second"));
    QCOMPARE(assembly.assetSourcePaths.size(), recipe.assets.size());
    for (auto it = assembly.assetSourcePaths.constBegin();
         it != assembly.assetSourcePaths.constEnd();
         ++it) {
        QVERIFY(QFileInfo(it.value()).absoluteFilePath().startsWith(
                    QFileInfo(selection.recipeDirectoryPath)
                    .absoluteFilePath()));
    }

    assembly.recipe.displayName = QStringLiteral("after-edit");
    QVERIFY2(store.saveRecipe(assembly.recipe,
                              assembly.assetSourcePaths,
                              &errorMessage),
             qPrintable(errorMessage));

    ProductRecipe reloaded;
    QVERIFY2(store.loadRecipe(recipe.recipeId, &reloaded, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(reloaded.displayName, QStringLiteral("after-edit"));
    QCOMPARE(reloaded.profiles.size(), 2);
    QCOMPARE(reloaded.profiles.at(0).name, QStringLiteral("first"));
    QCOMPARE(reloaded.profiles.at(0).targetText, QStringLiteral("AB"));
    QCOMPARE(reloaded.profiles.at(1).name, QStringLiteral("second"));
    QCOMPARE(reloaded.profiles.at(1).targetText, QStringLiteral("CD"));
    QVERIFY(reloaded.assets == recipe.assets);
    const QDir recipeDirectory(store.recipeDirectoryPath(recipe.recipeId));
    QCOMPARE(readBytes(recipeDirectory.filePath(
                           QStringLiteral("assets/profiles/0/tracking.bmp"))),
             QByteArray("first-tracking"));
    QCOMPARE(readBytes(recipeDirectory.filePath(
                           QStringLiteral("assets/profiles/1/calibration.yaml"))),
             QByteArray("second-calibration"));
}

void RecipeStoreTest::selectedRecipeAssemblyFailurePreservesPreviousOutput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString tracking = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibration = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(tracking, QByteArray("tracking")));
    QVERIFY(writeBytes(calibration, QByteArray("calibration")));

    ProductRecipe recipe = createProductRecipe(
                QStringLiteral("editable"), DetectionMode::Word);
    RecipeProfile profile;
    profile.name = QStringLiteral("profile");
    profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profile.hasValidBoxes = true;
    profile.assetKeys.insert(QStringLiteral("trackingTemplate"),
                             QStringLiteral("tracking"));
    profile.assetKeys.insert(QStringLiteral("calibration"),
                             QStringLiteral("calibration"));
    recipe.profiles.append(profile);
    recipe.assets.insert(QStringLiteral("tracking"),
                         QStringLiteral("assets/tracking.bmp"));
    recipe.assets.insert(QStringLiteral("calibration"),
                         QStringLiteral("assets/calibration.yaml"));
    QMap<QString, QString> sources;
    sources.insert(QStringLiteral("tracking"), tracking);
    sources.insert(QStringLiteral("calibration"), calibration);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QString errorMessage;
    QVERIFY2(store.saveRecipe(recipe, sources, &errorMessage),
             qPrintable(errorMessage));
    RecipeSelection selection;
    QVERIFY2(loadRecipeSelection(store,
                                 recipe.recipeId,
                                 DetectionMode::Word,
                                 &selection,
                                 &errorMessage),
             qPrintable(errorMessage));
    selection.profiles[0].assetPathsByRole.remove(
                QStringLiteral("calibration"));

    TemplateRecipeAssembly unchangedAssembly;
    unchangedAssembly.recipe = createProductRecipe(
                QStringLiteral("sentinel"), DetectionMode::Word);
    unchangedAssembly.assetSourcePaths.insert(QStringLiteral("sentinel"),
                                              QStringLiteral("sentinel-path"));
    const QString sentinelRecipeId = unchangedAssembly.recipe.recipeId;

    QVERIFY(!assembleSelectedTemplateRecipe(selection,
                                             &unchangedAssembly,
                                             &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("profile")));
    QVERIFY(errorMessage.contains(QStringLiteral("calibration")));
    QCOMPARE(unchangedAssembly.recipe.recipeId, sentinelRecipeId);
    QCOMPARE(unchangedAssembly.recipe.displayName, QStringLiteral("sentinel"));
    QCOMPARE(unchangedAssembly.assetSourcePaths.size(), 1);
    QCOMPARE(unchangedAssembly.assetSourcePaths.value(
                 QStringLiteral("sentinel")),
             QStringLiteral("sentinel-path"));
}

void RecipeStoreTest::publishingCommitsASelectableRecipe()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe recipeHeader = createProductRecipe(
                QStringLiteral("published"), DetectionMode::Word);
    TemplateRecipeProfileSource profileSource;
    profileSource.profile.name = QStringLiteral("profile");
    profileSource.profile.targetText = QStringLiteral("AB");
    profileSource.profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profileSource.profile.hasValidBoxes = true;
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("trackingTemplate"),
                QStringLiteral("profile0_tracking"));
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("calibration"),
                QStringLiteral("profile0_calibration"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("profile0_tracking"),
                QStringLiteral("assets/profiles/0/tracking.bmp"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("profile0_calibration"),
                QStringLiteral("assets/profiles/0/calibration.yaml"));
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("profile0_tracking"), trackingSource);
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("profile0_calibration"), calibrationSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);
    RecipeSelection publishedSelection;
    QString errorMessage;
    QVERIFY2(publishTemplateRecipe(
                 store,
                 recipeHeader,
                 profileSources,
                 &publishedSelection,
                 &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(publishedSelection.recipe);
    QCOMPARE(publishedSelection.recipe->recipeId, recipeHeader.recipeId);
    QCOMPARE(publishedSelection.recipe->displayName,
             QStringLiteral("published"));
    QCOMPARE(publishedSelection.profiles.size(), 1);
    QCOMPARE(publishedSelection.profiles.first().profile.targetText,
             QStringLiteral("AB"));
    QCOMPARE(readBytes(publishedSelection.profiles.first()
                       .assetPathsByRole.value(
                           QStringLiteral("trackingTemplate"))),
             QByteArray("tracking"));
}

void RecipeStoreTest::publishingFailurePreservesPreviousRecipeAndOutput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe recipeHeader = createProductRecipe(
                QStringLiteral("original"), DetectionMode::BarcodeWord);
    TemplateRecipeProfileSource profileSource;
    profileSource.profile.name = QStringLiteral("profile");
    profileSource.profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profileSource.profile.hasValidBoxes = true;
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("trackingTemplate"),
                QStringLiteral("tracking"));
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("calibration"),
                QStringLiteral("calibration"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("tracking"),
                QStringLiteral("assets/tracking.bmp"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("calibration"),
                QStringLiteral("assets/calibration.yaml"));
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("tracking"), trackingSource);
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("calibration"), calibrationSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);
    RecipeSelection selection;
    QString errorMessage;
    QVERIFY2(publishTemplateRecipe(
                 store,
                 recipeHeader,
                 profileSources,
                 &selection,
                 &errorMessage),
             qPrintable(errorMessage));
    const QString originalRecipeId = selection.recipe->recipeId;
    const QString originalDisplayName = selection.recipe->displayName;

    recipeHeader.displayName = QStringLiteral("replacement");
    profileSource.assetManifest.assetSourcePaths.remove(
                QStringLiteral("calibration"));
    profileSources[0] = profileSource;
    QVERIFY(!publishTemplateRecipe(
                store,
                recipeHeader,
                profileSources,
                &selection,
                &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("maps do not match")));
    QCOMPARE(selection.recipe->recipeId, originalRecipeId);
    QCOMPARE(selection.recipe->displayName, originalDisplayName);

    ProductRecipe reloaded;
    QVERIFY2(store.loadRecipe(recipeHeader.recipeId,
                              &reloaded,
                              &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(reloaded.displayName, QStringLiteral("original"));
}

void RecipeStoreTest::republishingSameHeaderKeepsIdentityAndReplacesAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString trackingSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking-v1")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe recipeHeader = createProductRecipe(
                QStringLiteral("draft"), DetectionMode::Word);
    const QString stableRecipeId = recipeHeader.recipeId;
    TemplateRecipeProfileSource profileSource;
    profileSource.profile.name = QStringLiteral("profile");
    profileSource.profile.targetText = QStringLiteral("AB");
    profileSource.profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profileSource.profile.hasValidBoxes = true;
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("trackingTemplate"),
                QStringLiteral("tracking"));
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("calibration"),
                QStringLiteral("calibration"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("tracking"),
                QStringLiteral("assets/tracking.bmp"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("calibration"),
                QStringLiteral("assets/calibration.yaml"));
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("tracking"), trackingSource);
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("calibration"), calibrationSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);
    RecipeSelection selection;
    QString errorMessage;
    QVERIFY2(publishTemplateRecipe(store,
                                   recipeHeader,
                                   profileSources,
                                   &selection,
                                   &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(selection.recipe->recipeId, stableRecipeId);
    QCOMPARE(readBytes(selection.profiles.first().assetPathsByRole.value(
                           QStringLiteral("trackingTemplate"))),
             QByteArray("tracking-v1"));

    QVERIFY(writeBytes(trackingSource, QByteArray("tracking-v2")));
    recipeHeader = *selection.recipe;
    recipeHeader.displayName = QStringLiteral("published");
    profileSource.profile.targetText = QStringLiteral("CD");
    profileSources[0] = profileSource;
    QVERIFY2(publishTemplateRecipe(store,
                                   recipeHeader,
                                   profileSources,
                                   &selection,
                                   &errorMessage),
             qPrintable(errorMessage));

    QCOMPARE(selection.recipe->recipeId, stableRecipeId);
    QCOMPARE(selection.recipe->displayName, QStringLiteral("published"));
    QCOMPARE(selection.profiles.first().profile.targetText,
             QStringLiteral("CD"));
    QCOMPARE(readBytes(selection.profiles.first().assetPathsByRole.value(
                           QStringLiteral("trackingTemplate"))),
             QByteArray("tracking-v2"));

    RecipeCatalog catalog;
    QVERIFY2(store.listRecipes(&catalog, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(catalog.recipes.size(), 1);
    QCOMPARE(catalog.recipes.first().recipeId, stableRecipeId);
}

void RecipeStoreTest::draftSessionRejectsChangedSourceAndKeepsIdentity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString sourceDirectory = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("source"));
    const QString otherDirectory = QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("other"));
    QVERIFY(QDir().mkpath(sourceDirectory));
    QVERIFY(QDir().mkpath(otherDirectory));
    const QString trackingSource = QDir(sourceDirectory).filePath(
                QStringLiteral("tracking.bmp"));
    const QString calibrationSource = QDir(sourceDirectory).filePath(
                QStringLiteral("calibration.yaml"));
    QVERIFY(writeBytes(trackingSource, QByteArray("tracking-v1")));
    QVERIFY(writeBytes(calibrationSource, QByteArray("calibration")));

    ProductRecipe recipeHeader = createProductRecipe(
                QStringLiteral("draft-session"), DetectionMode::BarcodeWord);
    const QString stableRecipeId = recipeHeader.recipeId;
    TemplateRecipeDraftSession session;
    QString errorMessage;
    QVERIFY2(session.begin(recipeHeader,
                           sourceDirectory,
                           &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(session.isActive());
    QCOMPARE(session.recipeHeader().recipeId, stableRecipeId);

    ProductRecipe invalidHeader = recipeHeader;
    invalidHeader.detectionMode = DetectionMode::Tissue;
    QVERIFY(!session.begin(invalidHeader,
                           otherDirectory,
                           &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("word family")));
    QVERIFY(session.isActive());
    QCOMPARE(session.recipeHeader().recipeId, stableRecipeId);
    QCOMPARE(QFileInfo(session.sourceDirectoryPath()).canonicalFilePath(),
             QFileInfo(sourceDirectory).canonicalFilePath());

    TemplateRecipeProfileSource profileSource;
    profileSource.profile.name = QStringLiteral("profile");
    profileSource.profile.targetText = QStringLiteral("A");
    profileSource.profile.trackingBox = QRectF(1.0, 2.0, 30.0, 40.0);
    profileSource.profile.hasValidBoxes = true;
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("trackingTemplate"),
                QStringLiteral("tracking"));
    profileSource.assetManifest.profileAssetKeys.insert(
                QStringLiteral("calibration"),
                QStringLiteral("calibration"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("tracking"),
                QStringLiteral("assets/tracking.bmp"));
    profileSource.assetManifest.recipeAssets.insert(
                QStringLiteral("calibration"),
                QStringLiteral("assets/calibration.yaml"));
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("tracking"), trackingSource);
    profileSource.assetManifest.assetSourcePaths.insert(
                QStringLiteral("calibration"), calibrationSource);
    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.append(profileSource);

    const RecipeStore store(QDir(temporaryDirectory.path()).filePath(
                                QStringLiteral("recipes")));
    RecipeSelection selection;
    selection.recipeDirectoryPath = QStringLiteral("sentinel");
    QVERIFY(!session.publish(store,
                             otherDirectory,
                             profileSources,
                             &selection,
                             &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("source directory changed")));
    QCOMPARE(selection.recipeDirectoryPath, QStringLiteral("sentinel"));

    QVERIFY2(session.publish(store,
                             sourceDirectory,
                             profileSources,
                             &selection,
                             &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(selection.recipe->recipeId, stableRecipeId);
    QCOMPARE(session.recipeHeader().recipeId, stableRecipeId);
    QCOMPARE(selection.profiles.first().profile.targetText,
             QStringLiteral("A"));

    QVERIFY(writeBytes(trackingSource, QByteArray("tracking-v2")));
    profileSource.profile.targetText = QStringLiteral("B");
    profileSources[0] = profileSource;
    QVERIFY2(session.publish(store,
                             sourceDirectory,
                             profileSources,
                             &selection,
                             &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(selection.recipe->recipeId, stableRecipeId);
    QCOMPARE(selection.profiles.first().profile.targetText,
             QStringLiteral("B"));
    QCOMPARE(readBytes(selection.profiles.first().assetPathsByRole.value(
                           QStringLiteral("trackingTemplate"))),
             QByteArray("tracking-v2"));

    RecipeCatalog catalog;
    QVERIFY2(store.listRecipes(&catalog, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(catalog.recipes.size(), 1);
    QCOMPARE(catalog.recipes.first().recipeId, stableRecipeId);

    session.reset();
    QVERIFY(!session.isActive());
}

QTEST_APPLESS_MAIN(RecipeStoreTest)

#include "recipe_store_test.moc"
