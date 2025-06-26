#pragma once

#include "Animation/AnimationData.h"
#include <QString>
#include <QFuture>
#include <optional>

class AniImporter {
public:
    std::optional<AnimationData> importFromFile(const QString& aniPath);
    
    // Call with values from 0.0 to 1.0 (progress %)
    void setProgressCallback(std::function<void(float)> cb);

private:
    std::function<void(float)> m_progressCallback;
};
