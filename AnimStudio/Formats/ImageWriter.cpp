#include "ImageWriter.h"
#include "Custom Handlers/DdsHandler.h"
#include "Custom Handlers/PcxHandler.h"
#include "Custom Handlers/TgaHandler.h"

#include <QFile>
#include <QImageWriter>
#include <QDebug>

bool ImageWriter::write(const QImage& image, const QString& path, ImageFormat fmt, CompressionFormat cFormat) {
    if (fmt == ImageFormat::Pcx || path.endsWith(".pcx", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for PCX writing:" << path;
            return false;
        }

        PcxHandler handler;
        handler.setDevice(&file);
        return handler.write(image);
    }

    if (fmt == ImageFormat::Tga || path.endsWith(".tga", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for TGA writing:" << path;
            return false;
        }

        TgaHandler handler;
        handler.setDevice(&file);
        return handler.write(image);
    }

    if (fmt == ImageFormat::Dds || path.endsWith(".dds", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for DDS writing:" << path;
            return false;
        }

        DdsHandler handler;
        handler.setDevice(path);
        handler.setCompression(cFormat);
        return handler.write(image);
    }

    QString ext = extensionForFormat(fmt);

    QImageWriter writer(path);
    writer.setFormat(ext.toUtf8());
    return writer.write(image);
}
