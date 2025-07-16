// ApngExporter.h
#pragma once

#include <QString>
#include "Animation/AnimationData.h"

class ApngExporter {
public:
    // Export the animation as an animated PNG
    // path is the full path including .apng extension
    ExportResult exportAnimation(const AnimationData& data, const QString& path, QString name);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};