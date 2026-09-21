#pragma once

#include "engines/ocr/ocr_engine.h"

class CharacterOcrEngine final : public IOcrEngine
{
public:
    explicit CharacterOcrEngine(const QString &configPath)
        : IOcrEngine(configPath)
    {
    }

    using IOcrEngine::recognizeCharacter;
    using IOcrEngine::segmentCharacters;
};
