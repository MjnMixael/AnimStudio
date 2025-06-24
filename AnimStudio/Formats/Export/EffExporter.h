// EffExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"

class EffExporter {
public:
    // Export the animation frames and write an .eff metadata file to the directory
    // outputDir must exist; returns true only if all images and the .eff file succeed
    static bool exportAnimation(const AnimationData& data,
        const QString& outputDir,
        ImageFormat fmt);

private:
    // Write the .eff file describing the sequence
    static bool writeEffFile(const AnimationData& data,
        const QString& effPath,
        ImageFormat fmt);
};