// AniExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"

class AniExporter {
public:
    // Export the animation as a FreeSpace-compatible ANI file
    // aniPath is the full path including .ani extension
    ExportResult exportAnimation(const AnimationData& data, const QString& aniPath, QString name);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};