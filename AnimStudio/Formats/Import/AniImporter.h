#pragma once

#include "AnimationData.h"
#include <QString>
#include <QFuture>
#include <optional>

class AniImporter {
public:
    static std::optional<AnimationData> importFromFile(const QString& aniPath);
    //static QFuture<std::optional<AnimationData>> importFromFileAsync(const QString& aniPath);
};
