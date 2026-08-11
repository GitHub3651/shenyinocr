#include <QtTest/QtTest>

#include "product_recipe.h"

#include <QJsonObject>
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
};

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

QTEST_APPLESS_MAIN(ProductRecipeTest)

#include "product_recipe_test.moc"
