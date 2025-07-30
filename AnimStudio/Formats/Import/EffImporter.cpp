// EffImporter.cpp
#include "EffImporter.h"
#include "RawImporter.h"
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"
#include "Formats/ImageLoader.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QImageReader>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>

void EffImporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

std::optional<AnimationData> EffImporter::parseEff(const QString& effPath) {
    QFile file(effPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    if (m_progressCallback) m_progressCallback(0.0f);

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
    
    QStringList filePaths;
    for (int i = 0; i < data.frameCount; ++i) {
        QString frameName = QString("%1_%2%3").arg(data.baseName).arg(i, 4, 10, QChar('0')).arg(suffix);
        QString fullPath = dir.filePath(frameName);
        filePaths.append(fullPath);
    }

    RawImporter importer;
    data.frames = importer.loadImageSequence(filePaths, data.importWarnings, m_progressCallback);

    if (m_progressCallback) m_progressCallback(1.0f);

    data.animationType = AnimationType::Eff;

    return data;
}

std::optional<AnimationData> EffImporter::importFromFile(const QString& effPath) {
    return parseEff(effPath);
}