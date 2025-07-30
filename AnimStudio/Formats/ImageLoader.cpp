#include "ImageLoader.h"
#include "Custom Handlers/PcxHandler.h"
#include "Custom Handlers/TgaHandler.h"

#include <QFile>
#include <QImageReader>
#include <QDebug>

QImage ImageLoader::load(const QString& path) {
    if (path.endsWith(".pcx", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "PCX file could not be opened:" << path;
            return QImage();
        }

        PcxHandler handler;
        handler.setDevice(&file);
        QImage img;
        if (!handler.read(&img)) {
            qWarning() << "Failed to load PCX image:" << path;
            return QImage();
        }
        return img;
    }

    if (path.endsWith(".tga", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "TGA file could not be opened:" << path;
            return QImage();
        }

        TgaHandler handler;
        handler.setDevice(&file);
        QImage img;
        if (!handler.read(&img)) {
            qWarning() << "Failed to load TGA image:" << path;
            return QImage();
        }
        return img;
    }

    return QImage(path); // fallback to Qt-native
}
