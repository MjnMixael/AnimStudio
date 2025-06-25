// AniExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"

class AniExporter {
public:
    // Export the animation as a FreeSpace-compatible ANI file
    // aniPath is the full path including .ani extension
    static bool exportAnimation(const AnimationData& data,
        const QString& aniPath);
};