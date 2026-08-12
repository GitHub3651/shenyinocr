#include <QtTest/QtTest>

#include "recipe_store.h"

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

QTEST_APPLESS_MAIN(RecipeStoreTest)

#include "recipe_store_test.moc"
