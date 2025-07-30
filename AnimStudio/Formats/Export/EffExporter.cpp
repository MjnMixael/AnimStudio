// EffExporter.cpp
#include "EffExporter.h"
#include "RawExporter.h"
#include <QFile>
#include <QTextStream>
#include <QDir>

void EffExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

ExportResult writeEffFile(const AnimationData& data,
    const QString& effPath,
    ImageFormat fmt)
{
    QFile file(effPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return ExportResult::fail(QString("Failed to write .eff file: could not open '%1'").arg(effPath));
    }

    QTextStream out(&file);
    // Write type without leading dot, e.g. "png" or "jpg"
    QString typeStr = extensionForFormat(fmt).mid(1).toLower();
    out << "$Type: " << typeStr << "\n";
    out << "$Frames: " << data.frames.size() << "\n";
    out << "$FPS: " << data.fps << "\n";
    if (data.hasLoopPoint && data.loopPoint > 0) {
        out << "$Keyframe: " << data.loopPoint << "\n";
    }

    file.close();
    return ExportResult::ok();
}

ExportResult EffExporter::exportAnimation(const AnimationData& data, const QString& outputDir, ImageFormat fmt, QString name)
{
    // Create subfolder named after name
    QDir parentDir(outputDir);
    QString subName = name;
    if (!parentDir.exists(subName)) {
        if (!parentDir.mkpath(subName)) {
            return ExportResult::fail(QString("Failed to create export subfolder '%1' in '%2'").arg(subName, outputDir));
        }
    }
    QString targetDir = parentDir.filePath(subName);

    // Export all frames with 4-digit zero-padding
    const int padDigits = 4;
    QStringList errors;
    for (int i = 0; i < data.frames.size(); ++i) {
        QString fileName = QString("%1_%2%3")
            .arg(name)
            .arg(i, padDigits, 10, QChar('0'))
            .arg(extensionForFormat(fmt));
        QString fullPath = QDir(targetDir).filePath(fileName);
        RawExporter exporter;
        ExportResult result = exporter.exportCurrentFrame(data, i, fullPath, fmt, false);
        if (!result.success) {
            errors << result.errorMessage;
        }

        // Emit progress (frame-wise granularity)
        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.frames.size());
            m_progressCallback(progress);
        }
    }

    if (!errors.isEmpty()) {
        return ExportResult::fail("One or more frames failed to export:\n" + errors.join("\n"));
    }

    // Write the .eff metadata file inside the same subfolder
    QString effName = name + ".eff";
    QString effPath = QDir(targetDir).filePath(effName);
    ExportResult effResult = writeEffFile(data, effPath, fmt);
    if (!effResult.success) {
        return effResult;
    }

    if (m_progressCallback)
        m_progressCallback(1.0f);

    return ExportResult::ok();
}
