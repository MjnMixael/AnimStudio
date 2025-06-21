#pragma once

#include <QString>
#include "AnimationData.h"
#include <optional>

class RawImporter {
public:
    static AnimationData importBlocking(const QString& dir);
};
