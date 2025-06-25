// RawExporter.cpp
#include "RawExporter.h"
#include <QDir>
#include <QImageWriter>

void RawExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

bool RawExporter::exportCurrentFrame(
    const AnimationData& data,
    int frameIndex,
    const QString& outputPath,
    ImageFormat format,
    bool updateProgress)
{
    if (frameIndex < 0 || frameIndex >= data.frames.size())
        return false;

    if (m_progressCallback && updateProgress)
        m_progressCallback(0.0f);

    const auto& frame = data.frames[frameIndex].image;
    QImageWriter writer(outputPath);
    writer.setFormat(formatToQtString(format));
    // you can tweak quality/compression here via writer.setQuality(...)

    if (m_progressCallback && updateProgress)
        m_progressCallback(1.0f);

    return writer.write(frame);
}

bool RawExporter::exportAllFrames(
    const AnimationData& data,
    const QString& outputDir,
    ImageFormat format)
{
    QDir dir(outputDir);
    if (!dir.exists()) {
        // fail immediately if user-picked folder doesn’t exist
        return false;
    }

    if (data.frameCount == 0) {
        return true;
    }

    // figure out how many digits we need: last index is data.frameCount-1
    int maxIndex = data.frameCount - 1;
    int digits = QString::number(maxIndex).length();
    QString ext = extensionForFormat(format);  // includes the leading “.”

    bool allOk = true;
    for (int i = 0; i < data.frameCount; ++i) {
        // zero-pad the frame number to 'digits' width
        QString fileName = QString("%1_%2%3")
            .arg(data.baseName)
            .arg(i, digits, 10, QChar('0'))
            .arg(ext);

        QString fullPath = dir.filePath(fileName);
        if (!exportCurrentFrame(data, i, fullPath, format)) {
            allOk = false;  // continue, but note the failure
        }

        // Emit progress (frame-wise granularity)
        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.frames.size());
            m_progressCallback(progress);
        }
    }

    if (m_progressCallback)
        m_progressCallback(1.0f);
    return allOk;
}
