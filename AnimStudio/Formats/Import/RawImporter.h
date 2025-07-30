#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include <optional>

class RawImporter {
public:
    // Import a blocking of frames
    QVector<AnimationFrame> loadImageSequence(const QStringList& filePaths, QStringList& warnings, std::function<void(float)> progressCallback = nullptr);

    AnimationData importBlocking(const QString& dir);

    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};
