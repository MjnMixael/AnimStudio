#pragma once

#include <QString>
#include "Animation/AnimationData.h"
#include <optional>

class RawImporter {
public:
    static AnimationData importBlocking(const QString& dir);
};
