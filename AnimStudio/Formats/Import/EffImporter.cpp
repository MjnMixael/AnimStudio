// EffImporter.cpp
#include "EffImporter.h"
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QImageReader>
#include <QRegularExpression>
#include <QtConcurrent>

static std::optional<AnimationData> parseEff(const QString& effPath) {
    QFile file(effPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    QTextStream in(&file);
    AnimationData data;
    data.baseName = QFileInfo(effPath).completeBaseName();

    QString type;

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith("$Type:", Qt::CaseInsensitive))
            type = line.section(':', 1).trimmed();
        else if (line.startsWith("$Frames:", Qt::CaseInsensitive))
            data.frameCount = line.section(':', 1).trimmed().toInt();
        else if (line.startsWith("$FPS:", Qt::CaseInsensitive))
            data.fps = line.section(':', 1).trimmed().toInt();
        else if (line.startsWith("$Keyframe:", Qt::CaseInsensitive))
            data.keyframeIndices.append(line.section(':', 1).trimmed().toInt());
    }

    if (!isSupportedFormat(type)) {
        return std::nullopt; // unsupported type
    } else {
        data.type = formatFromExtension(type);
    }

    QDir dir = QFileInfo(effPath).absoluteDir();
    QString suffix = extensionForFormat(data.type.value());

    for (int i = 0; i < data.frameCount; ++i) {
        QString frameName = QString("%1_%2%3").arg(data.baseName).arg(i, 4, 10, QChar('0')).arg(suffix);
        QString fullPath = dir.filePath(frameName);
        QImage frame(fullPath);
        if (frame.isNull()) return std::nullopt;

        data.frames.append(AnimationFrame{ frame, i, frameName });
    }

    data.animationType = AnimationType::Eff;

    return data;
}

std::optional<AnimationData> EffImporter::importFromFile(const QString& effPath) {
    return parseEff(effPath);
}

QFuture<std::optional<AnimationData>> EffImporter::importFromFileAsync(const QString& effPath) {
    return QtConcurrent::run(parseEff, effPath);
}