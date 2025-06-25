// RawExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"

class RawExporter {
public:
    // Export a single frame by index to the given file path.
    // Returns true on success.
    bool exportCurrentFrame(const AnimationData& data, int frameIndex, const QString& outputPath, ImageFormat format, bool updateProgress = false);

    // Export all frames in 'data' to 'outputDir', naming each
    // baseName_frame0.png / .jpg / etc. Returns true only if ALL succeed.
    bool exportAllFrames( const AnimationData& data, const QString& outputDir, ImageFormat format);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};
