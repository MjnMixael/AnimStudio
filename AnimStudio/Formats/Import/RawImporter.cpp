#include "RawImporter.h"
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"
#include <QDir>
#include <QImageReader>
#include <QRegularExpression>
#include <algorithm>

AnimationData RawImporter::importBlocking(const QString& dir) {
    AnimationData data;
    QStringList filters = availableFilters();
    QDir directory(dir);
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);

    if (!files.isEmpty()) {
        // Use the first file to seed baseName and type
        QFileInfo firstFi(directory.filePath(files.first()));
        // Given the “complete” base name (everything before the last dot):
        QString rawBase = firstFi.completeBaseName();  // e.g. "walk_0001"

        // Remove a trailing “_1234” or “-1234” or just “1234”
        QString strippedBase = rawBase;
        strippedBase.replace(
            QRegularExpression("([_\\-]?\\d+)$"),
            QString()     // replace with empty
        );
        data.baseName = strippedBase; // e.g. "walk"
        data.type = formatFromExtension(firstFi.suffix());       // e.g. "png"
    } else {
        // No files: fall back to directory name
        data.baseName = QString();
        data.type = std::nullopt;
    }

    data.frameCount = files.size();
    data.fps = 15; // TODO Default FPS, can be adjusted later

    for (const QString& file : files) {
        QImage img(dir + "/" + file);
        if (!img.isNull()) {
            AnimationFrame frame;
            frame.image = img;
            data.frames.append(frame);
        }
    }

    data.animationType = AnimationType::Raw;

    return data;
}