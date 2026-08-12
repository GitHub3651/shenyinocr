#include "template_profile_mapper.h"

#include "../appsettingsmanager.h"

RecipeProfile recipeProfileFromTemplatePrivateSettings(
        const QString &profileName,
        const TemplatePrivateSettings &settings,
        const QMap<QString, QString> &assetKeys)
{
    RecipeProfile profile;
    profile.name = profileName;
    profile.targetText = settings.targetText;
    profile.imageThreshold = settings.imageThreshold;
    profile.trackingBox = QRectF(settings.trackingBox.x,
                                 settings.trackingBox.y,
                                 settings.trackingBox.width,
                                 settings.trackingBox.height);
    profile.hasValidBoxes = settings.hasValidBoxes;
    profile.characterSourceImageSize = settings.characterSourceImageSize;
    profile.characterBoxes.reserve(settings.characterBoxes.size());
    for (const CharacterTemplateBox &sourceBox : settings.characterBoxes) {
        RecipeCharacterBox targetBox;
        targetBox.name = sourceBox.name;
        targetBox.rect = sourceBox.rect;
        profile.characterBoxes.append(targetBox);
    }
    profile.barcodeParameters.formatMask = settings.barcodeOptions.formatMask;
    profile.barcodeParameters.roiPaddingPercent =
            settings.barcodeOptions.roiPaddingPercent;
    profile.barcodeParameters.maxDecodeTimeMs =
            settings.barcodeOptions.maxDecodeTimeMs;
    profile.barcodeParameters.enableFallback =
            settings.barcodeOptions.enableFallback;
    profile.assetKeys = assetKeys;
    return profile;
}

TemplatePrivateSettings templatePrivateSettingsFromRecipeProfile(
        const RecipeProfile &profile)
{
    TemplatePrivateSettings settings;
    settings.targetText = profile.targetText;
    settings.imageThreshold = profile.imageThreshold;
    settings.trackingBox = cv::Rect2d(profile.trackingBox.x(),
                                      profile.trackingBox.y(),
                                      profile.trackingBox.width(),
                                      profile.trackingBox.height());
    settings.hasValidBoxes = profile.hasValidBoxes;
    settings.characterSourceImageSize = profile.characterSourceImageSize;
    settings.characterBoxes.reserve(profile.characterBoxes.size());
    for (const RecipeCharacterBox &sourceBox : profile.characterBoxes) {
        CharacterTemplateBox targetBox;
        targetBox.name = sourceBox.name;
        targetBox.rect = sourceBox.rect;
        settings.characterBoxes.append(targetBox);
    }
    settings.barcodeOptions.formatMask = profile.barcodeParameters.formatMask;
    settings.barcodeOptions.roiPaddingPercent =
            profile.barcodeParameters.roiPaddingPercent;
    settings.barcodeOptions.maxDecodeTimeMs =
            profile.barcodeParameters.maxDecodeTimeMs;
    settings.barcodeOptions.enableFallback =
            profile.barcodeParameters.enableFallback;
    return settings;
}
