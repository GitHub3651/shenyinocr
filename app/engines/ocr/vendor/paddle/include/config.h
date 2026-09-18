#pragma once

#include <string>

namespace PaddleOCR {

class OCRConfig
{
public:
    explicit OCRConfig(const std::string &configFile);

    int cpuMathLibraryNumThreads = 4;
    bool useMkldnn = true;

    int maxSideLen = 960;
    float detDbThresh = 0.2f;
    float detDbBoxThresh = 0.4f;
    float detDbUnclipRatio = 1.4f;

    std::string detModelDir;
    std::string recModelDir;
    std::string charListFile;
};

} // namespace PaddleOCR
