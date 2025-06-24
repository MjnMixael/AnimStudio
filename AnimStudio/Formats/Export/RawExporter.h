// RawExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"

class RawExporter {
public:
    // Export a single frame by index to the given file path.
    // Returns true on success.
    static bool exportCurrentFrame(
        const AnimationData& data,
        int frameIndex,
        const QString& outputPath,
        ImageFormat format);

    // Export all frames in 'data' to 'outputDir', naming each
    // baseName_frame0.png / .jpg / etc. Returns true only if ALL succeed.
    static bool exportAllFrames(
        const AnimationData& data,
        const QString& outputDir,
        ImageFormat format);
};
