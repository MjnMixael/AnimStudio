// AnimationData.h
#pragma once

#include <QString>
#include <QImage>
#include <QVector>

struct AnimationFrame {
    QImage image;
    int index;
    QString filename;
};

struct AnimationData {
    QString baseName;
    QString type; // DDS, PNG, etc.
    int frameCount = 0;
    int fps = 15;
    int keyframe = -1;

    QVector<AnimationFrame> frames;
};
