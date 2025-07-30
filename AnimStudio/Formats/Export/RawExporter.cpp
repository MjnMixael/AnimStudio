// RawExporter.cpp
#include "RawExporter.h"
#include "Formats/ImageWriter.h"
#include <QDir>
#include <QImageWriter>

void RawExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

ExportResult RawExporter::exportCurrentFrame(
    const AnimationData& data,
    int frameIndex,
    const QString& outputPath,
    ImageFormat format,
    bool updateProgress)
{
    if (frameIndex < 0 || frameIndex >= data.frames.size())
        return ExportResult::fail(QString("Invalid frame index: %1. Total frames: %2.")
            .arg(frameIndex)
            .arg(data.frames.size()));

    if (m_progressCallback && updateProgress)
        m_progressCallback(0.0f);

    const QImage& frame = [&]() -> const QImage& {
        if (format == ImageFormat::Pcx &&
            frameIndex < data.quantizedFrames.size() &&
            !data.quantizedFrames[frameIndex].image.isNull())
        {
            return data.quantizedFrames[frameIndex].image;
        }
        return data.frames[frameIndex].image;
    }();

    QString ext = formatToQtString(format);
    QImageWriter writer(outputPath);
    if (!ImageWriter::write(frame, outputPath, ext)) {
        return ExportResult::fail(QString("Failed to write frame %1 to '%2'")
            .arg(frameIndex)
            .arg(QFileInfo(outputPath).fileName()));
    }

    if (m_progressCallback && updateProgress)
        m_progressCallback(1.0f);

    return ExportResult::ok();
}

ExportResult RawExporter::exportAllFrames(
    const AnimationData& data,
    const QString& outputDir,
    ImageFormat format)
{
    QDir dir(outputDir);
    if (!dir.exists()) {
        // fail immediately if user-picked folder doesn’t exist
        return ExportResult::fail(QString("Output directory does not exist: '%1'").arg(outputDir));
    }

    if (data.frameCount == 0) {
        return ExportResult::fail(QString("No frames available to export!"));
    }

    // figure out how many digits we need: last index is data.frameCount-1
    int maxIndex = data.frameCount - 1;
    int digits = QString::number(maxIndex).length();
    QString ext = extensionForFormat(format);  // includes the leading “.”

    QStringList errors;
    for (int i = 0; i < data.frameCount; ++i) {
        // zero-pad the frame number to 'digits' width
        QString fileName = QString("%1_%2%3")
            .arg(data.baseName)
            .arg(i, digits, 10, QChar('0'))
            .arg(ext);

        QString fullPath = dir.filePath(fileName);
        ExportResult result = exportCurrentFrame(data, i, fullPath, format, false);

        if (!result.success) {
            errors << result.errorMessage;
        }

        // Emit progress (frame-wise granularity)
        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.frames.size());
            m_progressCallback(progress);
        }
    }

    if (m_progressCallback)
        m_progressCallback(1.0f);

    if (!errors.isEmpty()) {
        return ExportResult::fail("One or more frames failed to export:\n" + errors.join("\n"));
    }

    return ExportResult::ok();
}
