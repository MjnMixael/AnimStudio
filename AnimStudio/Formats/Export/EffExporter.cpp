// EffExporter.cpp
#include "EffExporter.h"
#include "RawExporter.h"  // to export PNG/JPG frames
#include <QFile>
#include <QTextStream>
#include <QDir>

bool EffExporter::exportAnimation(const AnimationData& data,
    const QString& outputDir,
    ImageFormat fmt)
{
    // 1) Create subfolder named after baseName
    QDir parentDir(outputDir);
    QString subName = data.baseName;
    if (!parentDir.exists(subName)) {
        if (!parentDir.mkpath(subName)) {
            return false;
        }
    }
    QString targetDir = parentDir.filePath(subName);

    // 2) Export all frames with 4-digit zero-padding
    const int padDigits = 4;
    bool ok = true;
    int frameCount = data.frames.size();
    for (int i = 0; i < frameCount; ++i) {
        QString fileName = QString("%1_%2%3")
            .arg(data.baseName)
            .arg(i, padDigits, 10, QChar('0'))
            .arg(extensionForFormat(fmt));
        QString fullPath = QDir(targetDir).filePath(fileName);
        if (!RawExporter::exportCurrentFrame(data, i, fullPath, fmt)) {
            ok = false;
        }
    }

    // 3) Write the .eff metadata file inside the same subfolder
    QString effName = data.baseName + ".eff";
    QString effPath = QDir(targetDir).filePath(effName);
    ok = ok && writeEffFile(data, effPath, fmt);

    return ok;
}

bool EffExporter::writeEffFile(const AnimationData& data,
    const QString& effPath,
    ImageFormat fmt)
{
    QFile file(effPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    // Write type without leading dot, e.g. "png" or "jpg"
    QString typeStr = extensionForFormat(fmt).mid(1).toLower();
    out << "$Type: " << typeStr << "\n";
    out << "$Frames: " << data.frames.size() << "\n";
    out << "$FPS: " << data.fps << "\n";
    if (data.loopPoint > 0) {
        out << "$Keyframe: " << data.loopPoint << "\n";
    }

    file.close();
    return true;
}
