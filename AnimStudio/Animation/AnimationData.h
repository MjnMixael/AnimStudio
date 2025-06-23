// AnimationData.h
#pragma once

#include <QString>
#include <QImage>
#include <QVector>
#include <QSize>

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
    QSize originalSize; // size of the original frames
    int frameCount = 0;
    int fps = 15;
    QVector<int> keyframeIndices;
    int loopPoint = 0; // frame index to loop back to

    QVector<AnimationFrame> frames;
    QVector<AnimationFrame> quantizedFrames;
    bool quantized = false;
};
