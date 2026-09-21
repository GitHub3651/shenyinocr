#pragma once

#include "engines/ocr/ocr_engine.h"

class DeepOcrEngine final : public IOcrEngine
{
public:
    explicit DeepOcrEngine(const QString &configPath)
        : IOcrEngine(configPath)
    {
    }

    using IOcrEngine::recognize;
};
