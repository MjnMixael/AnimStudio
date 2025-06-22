// EffImporter.h
#pragma once

#include "Animation/AnimationData.h"
#include <QString>
#include <QFuture>
#include <optional>

class EffImporter {
public:
    static std::optional<AnimationData> importFromFile(const QString& effPath);
    static QFuture<std::optional<AnimationData>> importFromFileAsync(const QString& effPath); // NEW
};