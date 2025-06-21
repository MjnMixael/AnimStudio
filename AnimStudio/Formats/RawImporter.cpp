#include "RawImporter.h"
#include "AnimationData.h"
#include <QDir>
#include <QImageReader>
#include <QRegularExpression>
#include <algorithm>

AnimationData RawImporter::importBlocking(const QString& dir) {
    AnimationData data;
    QStringList filters = { "*.png", "*.bmp", "*.jpg", "*.tga" };
    QDir directory(dir);
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);

    data.frameCount = files.size();
    data.fps = 15; // TODO Default FPS, can be adjusted later
    data.baseName = QFileInfo(dir).fileName(); // TODO Use first image file name as base name

    for (const QString& file : files) {
        QImage img(dir + "/" + file);
        if (!img.isNull()) {
            AnimationFrame frame;
            frame.image = img;
            data.frames.append(frame);
        }
    }

    return data;
}