#pragma once

#include "Animation/AnimationData.h"
#include <optional>

class ApngImporter {
public:
    std::optional<AnimationData> importFromFile(const QString& path);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::vector<std::vector<unsigned char>> infoChunks;  // all non-frame, non-IHDR chunks
    std::vector<unsigned char> ihdrChunk;                // the raw IHDR chunk (length/type/data/CRC)

    std::function<void(float)> m_progressCallback;
};
