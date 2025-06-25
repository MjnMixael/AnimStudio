#include "AnimationData.h"

QString getTypeString(AnimationType type) {
    QString typeStr;

    switch (type) {
        case AnimationType::Ani:  typeStr = "ANI"; break;
        case AnimationType::Eff:  typeStr = "EFF"; break;
        case AnimationType::Apng: typeStr = "APNG"; break;
        case AnimationType::Raw:  typeStr = "Sequence"; break;
        default:                  typeStr = "Unknown"; break;
    }

    return typeStr;
}