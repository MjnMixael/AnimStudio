// EffExporter.cpp
#include "EffExporter.h"
#include "RawExporter.h"  // to export PNG/JPG frames
#include <QFile>
#include <QTextStream>
#include <QDir>

void EffExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

bool writeEffFile(const AnimationData& data,
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

bool EffExporter::exportAnimation(const AnimationData& data, const QString& outputDir, ImageFormat fmt, QString name)
{
    // 1) Create subfolder named after name
    QDir parentDir(outputDir);
    QString subName = name;
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
            .arg(name)
            .arg(i, padDigits, 10, QChar('0'))
            .arg(extensionForFormat(fmt));
        QString fullPath = QDir(targetDir).filePath(fileName);
        RawExporter exporter;
        if (!exporter.exportCurrentFrame(data, i, fullPath, fmt)) {
            ok = false;
        }

        // Emit progress (frame-wise granularity)
        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.frames.size());
            m_progressCallback(progress);
        }
    }

    // 3) Write the .eff metadata file inside the same subfolder
    QString effName = name + ".eff";
    QString effPath = QDir(targetDir).filePath(effName);
    ok = ok && writeEffFile(data, effPath, fmt);

    if (m_progressCallback)
        m_progressCallback(1.0f);

    return ok;
}
