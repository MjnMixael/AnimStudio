#pragma once

#include <QImage>
#include <QString>

namespace ImageWriter {
    bool write(const QImage& image, const QString& path, const QString& format);
}
