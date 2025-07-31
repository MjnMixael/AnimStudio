#pragma once

#include "Formats/ImageFormats.h"
#include <QImage>
#include <QString>

namespace ImageWriter {
    bool write(const QImage& image, const QString& path, ImageFormat fmt, CompressionFormat cFormat);
}
