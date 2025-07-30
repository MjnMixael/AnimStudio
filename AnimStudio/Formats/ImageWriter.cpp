#include "ImageWriter.h"
#include "Custom Handlers/PcxHandler.h"
#include "Custom Handlers/TgaHandler.h"

#include <QFile>
#include <QImageWriter>
#include <QDebug>

bool ImageWriter::write(const QImage& image, const QString& path, const QString& format) {
    if (format.compare("pcx", Qt::CaseInsensitive) == 0 || path.endsWith(".pcx", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for PCX writing:" << path;
            return false;
        }

        PcxHandler handler;
        handler.setDevice(&file);
        return handler.write(image);
    }

    if (format.compare("tga", Qt::CaseInsensitive) == 0 || path.endsWith(".tga", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for TGA writing:" << path;
            return false;
        }

        TgaHandler handler;
        handler.setDevice(&file);
        return handler.write(image);
    }

    QImageWriter writer(path);
    writer.setFormat(format.toUtf8());
    return writer.write(image);
}
