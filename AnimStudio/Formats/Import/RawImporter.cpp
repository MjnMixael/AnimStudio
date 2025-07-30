#include "RawImporter.h"
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"
#include "Formats/ImageLoader.h"
#include <QDir>
#include <QImageReader>
#include <QRegularExpression>
#include <algorithm>

void RawImporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

QVector<AnimationFrame> RawImporter::loadImageSequence(const QStringList& filePaths, QStringList& warnings, std::function<void(float)> progressCallback)
{
    QVector<AnimationFrame> result;
    QSize refSize;
    bool refSizeSet = false;

    // Use a 1x1 transparent temp image for placeholder initially
    QImage tinyBlank(1, 1, QImage::Format_RGBA8888);
    tinyBlank.fill(Qt::transparent);
    QVector<int> blankIndices;

    for (int i = 0; i < filePaths.size(); ++i) {
        const QString& path = filePaths[i];
        QString fileName = QFileInfo(path).fileName();
        QImage img = ImageLoader::load(path);
        if (img.isNull()) {
            warnings.append(QString("Missing or unreadable frame: %1").arg(fileName));
            result.append(AnimationFrame{ tinyBlank, i, fileName });
            blankIndices.append(i);
        } else {
            if (!refSizeSet) {
                refSize = img.size();
                refSizeSet = true;
            } else if (img.size() != refSize) {
                warnings << QString("Size mismatch at frame %1: %2 (%3x%4), expected %5x%6")
                    .arg(i)
                    .arg(fileName)
                    .arg(img.width())
                    .arg(img.height())
                    .arg(refSize.width())
                    .arg(refSize.height());
            }
            result.append(AnimationFrame{ img, i, fileName });
        }

        if (progressCallback)
            progressCallback(static_cast<float>(i + 1) / filePaths.size());
    }

    // Fix placeholder frames if refSize is known
    if (refSizeSet) {
        QImage properBlank(refSize, QImage::Format_RGBA8888);
        properBlank.fill(Qt::transparent);
        for (int index : blankIndices) {
            result[index].image = properBlank;
        }
    }

    return result;
}

AnimationData RawImporter::importBlocking(const QString& dir) {
    AnimationData data;
    QStringList filters = availableFilters();
    QDir directory(dir);
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        data.importWarnings << "No files found in directory.";
        return data;
    }

    if (m_progressCallback) m_progressCallback(0.0f);

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

    int count = 0;
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

        // Emit progress 0 - 25
        if (m_progressCallback) {
            float frac = float(++count) / float(files.size());
            m_progressCallback(frac * 0.25f);
        }
    }

    QStringList filePaths;
    for (int i = 0; i <= maxIndex; ++i) {
        QString fullPath = directory.filePath(frameMap[i]);
        filePaths.append(fullPath);
    }
    data.frames = loadImageSequence(filePaths, data.importWarnings, m_progressCallback);

    if (m_progressCallback) { m_progressCallback(1); }

    data.frameCount = data.frames.size();
    return data;
}