// AnimationData.h
#pragma once

#include <QString>
#include <QImage>
#include <QVector>

enum class AnimationType {
    Eff,
    Ani,
    Apng,
    Raw
};

struct AnimationFrame {
    QImage image;
    int index;
    QString filename;
};

struct AnimationData {
    QString baseName;
    QString type; // DDS, PNG, etc.
    AnimationType animationType = AnimationType::Raw;
    int frameCount = 0;
    int fps = 15;
    QVector<int> keyframeIndices;

    QVector<AnimationFrame> frames;
};
