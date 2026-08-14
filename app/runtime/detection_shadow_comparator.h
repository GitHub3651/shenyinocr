#pragma once

#include "TrackingTypes.h"

#include <QStringList>

struct DetectionShadowComparisonOptions
{
    int coordinateTolerance = 0;
    double scoreTolerance = 0.0;
    bool compareDiagnostic = false;
};

struct DetectionShadowComparison
{
    QStringList differences;

    bool isEquivalent() const
    {
        return differences.isEmpty();
    }
};

class DetectionShadowComparator
{
public:
    static DetectionShadowComparison compare(
        const DetectionResult &primary,
        const DetectionResult &shadow,
        const DetectionShadowComparisonOptions &options =
            DetectionShadowComparisonOptions());
};
