// EffExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"

class EffExporter {
public:
    // Export the animation frames and write an .eff metadata file to the directory
    // outputDir must exist; returns true only if all images and the .eff file succeed
    bool exportAnimation(const AnimationData& data, const QString& outputDir, ImageFormat fmt);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};