#pragma once

#include "AnimationData.h"
#include <optional>

class ApngImporter {
public:
    static std::optional<AnimationData> importFromFile(const QString& path);

private:
    std::vector<std::vector<unsigned char>> infoChunks;  // all non-frame, non-IHDR chunks
    std::vector<unsigned char> ihdrChunk;                // the raw IHDR chunk (length/type/data/CRC)
};
