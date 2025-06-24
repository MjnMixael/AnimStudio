// RawExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"

class RawExporter {
public:
    // Supported output image formats; add new ones here in future
    enum class Format {
        Png,
        Jpg,
        Tga,
        Pcx
    };

    // Export a single frame by index to the given file path.
    // Returns true on success.
    static bool exportCurrentFrame(
        const AnimationData& data,
        int frameIndex,
        const QString& outputPath,
        Format format);

    // Export all frames in 'data' to 'outputDir', naming each
    // baseName_frame0.png / .jpg / etc. Returns true only if ALL succeed.
    static bool exportAllFrames(
        const AnimationData& data,
        const QString& outputDir,
        Format format);

private:
    // Helpers
    static QByteArray formatToQtString(Format fmt);
    static QString extensionForFormat(Format fmt);
};
