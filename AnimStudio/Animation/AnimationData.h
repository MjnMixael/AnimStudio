// AnimationData.h
#pragma once

#include "Formats/ImageFormats.h"

#include <QString>
#include <QStringList>
#include <QImage>
#include <QVector>
#include <QSize>
#include <QRgb>

#include <optional>

#define TRANSPARENT_COLOR_INDEX     255     // As per ani documentation, 255 is transparent pixel

enum class AnimationType {
    Eff,
    Ani,
    Apng,
    Raw
};

struct AnimationTypeData {
    AnimationType type;
    bool exportable;
    QString name;
};

extern QVector<AnimationTypeData> animationTypes;

struct AnimationFrame {
    QImage image;
    int index;
    QString filename;
};

struct AnimationData {
    QString baseName;
    std::optional<ImageFormat> type;
    AnimationType animationType = AnimationType::Raw;
    QSize originalSize; // size of the original frames
    int frameCount = 0;
    int fps = 15;
    float totalLength = 0;
    QVector<int> keyframeIndices;
    int loopPoint = 0; // frame index to loop back to
    bool usingLoopPoint = false;

    QVector<AnimationFrame> frames;
    QVector<AnimationFrame> quantizedFrames;
    bool quantized = false;
    QVector<QRgb> quantizedPalette; // if quantized, this holds the palette used

    QStringList importWarnings;
};

QString getTypeString(AnimationType type);

QVector<AnimationType> getExportableTypes();
