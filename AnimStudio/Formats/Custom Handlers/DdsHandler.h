#pragma once

#include "Formats/ImageFormats.h"
#include <QImage>
#include <QIODevice>

class DdsHandler {
public:
    void setDevice(const QString& device) { m_device = device; }
    void setCompression(CompressionFormat format) { m_compressionFormat = format; }

    bool read(QImage* image);
    bool write(const QImage& image);

private:
    QString m_device;
    CompressionFormat m_compressionFormat = CompressionFormat::BC7;
};
