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

    if (files.isEmpty()) {
        data.importWarnings << "No files found in directory.";
        return data;
    }

    QFileInfo firstFi(directory.filePath(files.first()));
    QString base = firstFi.completeBaseName();
    QString strippedBase = base;
    strippedBase.replace(QRegularExpression("([_\\-]?\\d+)$"), QString());
    QString expectedExt = firstFi.suffix().toLower();

    data.baseName = strippedBase;
    data.type = formatFromExtension(expectedExt);
    data.fps = 15;
    data.animationType = AnimationType::Raw;

    QSet<QString> loadedNames;
    QMap<int, QString> frameMap;
    int maxIndex = -1;

    for (const QString& file : files) {
        QFileInfo fi(file);
        QString name = fi.completeBaseName();
        QString ext = fi.suffix().toLower();

        if (ext != expectedExt) {
            data.importWarnings << "Extension mismatch: " + file;
        }

        QString temp = name;
        temp.replace(QRegularExpression("([_\\-]?\\d+)$"), QString());
        if (temp != strippedBase) {
            data.importWarnings << "Skipping file with unmatched base: " + file;
            continue;
        }

        QRegularExpression re("([_\\-]?)(\\d+)$");
        auto match = re.match(name);
        if (!match.hasMatch()) {
            data.importWarnings << "Skipping file with no frame index: " + file;
            continue;
        }

        QString key = match.captured(2);

        if (loadedNames.contains(key)) {
            data.importWarnings << QString("Skipped duplicate frame #%1 (file \"%2\").").arg(key, file);
            continue;
        }

        int frameNum = match.captured(2).toInt();
        frameMap[frameNum] = file;
        maxIndex = std::max(maxIndex, frameNum);
        loadedNames.insert(key);
    }

    QSize refSize;
    for (int i = 0; i <= maxIndex; ++i) {
        if (!frameMap.contains(i)) {
            QImage blank(refSize.isValid() ? refSize : QSize(1, 1), QImage::Format_RGBA8888);
            blank.fill(Qt::transparent);
            data.frames.append({ blank });
            data.importWarnings << QString("Missing frame #%1 - filled with transparent.").arg(i);
            continue;
        }

        QString fullPath = directory.filePath(frameMap[i]);
        QImage img(fullPath);
        if (img.isNull()) {
            data.importWarnings << "Could not load: " + fullPath;
            data.frames.append({ QImage() });
            continue;
        }

        if (!refSize.isValid()) {
            refSize = img.size();
        } else if (img.size() != refSize) {
            data.importWarnings << QString("Resizing frame #%1 from %2x%3 to %4x%5.")
                .arg(i)
                .arg(img.width()).arg(img.height())
                .arg(refSize.width()).arg(refSize.height());
            img = img.scaled(refSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        data.frames.append({ img });
    }

    data.frameCount = data.frames.size();
    return data;
}