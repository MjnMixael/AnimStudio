#include "AnimationData.h"

QVector<AnimationTypeData> AnimationTypes = {
        { AnimationType::Ani,  true,  "Ani"  },
        { AnimationType::Eff,  true,  "Eff"  },
        { AnimationType::Apng, true,  "Apng" },
        { AnimationType::Raw,  false, "Sequence"  },
};

QString getTypeString(AnimationType type) {
    for (const auto& t : AnimationTypes) {
        if (t.type == type)
            return t.name;
    }
    return "Unknown";
}

QVector<AnimationType> getExportableTypes() {
    QVector<AnimationType> types;
    for (const auto& t : AnimationTypes) {
        if (t.exportable)
            types.append(t.type);
    }
    return types;
}