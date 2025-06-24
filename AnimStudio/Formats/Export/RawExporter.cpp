// RawExporter.cpp
#include "RawExporter.h"
#include <QDir>
#include <QImageWriter>

QByteArray RawExporter::formatToQtString(Format fmt) {
    switch (fmt) {
    case Format::Png:  return "PNG";
    case Format::Jpg:  return "JPG";
    case Format::Tga:  return "TGA";
    case Format::Pcx:  return "PCX";  // Requires external plugin if Qt doesn't support PCX out of the box
    }
    return "PNG";
}

QString RawExporter::extensionForFormat(Format fmt) {
    switch (fmt) {
    case Format::Png:  return ".png";
    case Format::Jpg:  return ".jpg";
    case Format::Tga:  return ".tga";
    case Format::Pcx:  return ".pcx";
    }
    return ".png";
}

bool RawExporter::exportCurrentFrame(
    const AnimationData& data,
    int frameIndex,
    const QString& outputPath,
    Format format)
{
    if (frameIndex < 0 || frameIndex >= data.frames.size())
        return false;

    const auto& frame = data.frames[frameIndex].image;
    QImageWriter writer(outputPath);
    writer.setFormat(formatToQtString(format));
    // you can tweak quality/compression here via writer.setQuality(...)
    return writer.write(frame);
}

bool RawExporter::exportAllFrames(
    const AnimationData& data,
    const QString& outputDir,
    Format format)
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
    }
    return allOk;
}
