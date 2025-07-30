// ImageFormats.cpp
#include "ImageFormats.h"

#include <QStringList>

const std::unordered_map<ImageFormat, QString> imageFormatExtensions = {
    { ImageFormat::Png, ".png" },
    { ImageFormat::Jpg, ".jpg" },
    { ImageFormat::Bmp, ".bmp" },
    { ImageFormat::Tga, ".tga" },
    { ImageFormat::Pcx, ".pcx" },
    //{ ImageFormat::Dds, ".dds" }
};

QByteArray formatToQtString(ImageFormat fmt) {
    auto it = imageFormatExtensions.find(fmt);
    if (it != imageFormatExtensions.end()) {
        // derive Qt format from extension, e.g. ".png" -> "PNG"
        QString ext = it->second;
        QString qt = ext.mid(1).toUpper();
        return qt.toUtf8();
    }
    // fallback to PNG
    return QByteArray("png");
}

QString extensionForFormat(ImageFormat fmt) {
    // Use the map to allow easy future extension
    auto it = imageFormatExtensions.find(fmt);
    if (it != imageFormatExtensions.end()) {
        return it->second;
    }
    return ".png"; // fallback
}

ImageFormat formatFromExtension(const QString& ext) {
    QString norm = ext;
    if (!norm.startsWith('.')) {
        norm.prepend('.');
    }
    norm = norm.toLower();

    for (const auto& kv : imageFormatExtensions) {
        if (QString::compare(kv.second, norm, Qt::CaseInsensitive) == 0) {
            return kv.first;
        }
    }
    // fallback to PNG
    return ImageFormat::Png;
}

QStringList availableFilters() {
    QStringList filters;
    filters.reserve(static_cast<int>(imageFormatExtensions.size()));
    for (const auto& kv : imageFormatExtensions) {
        filters.append("*" + kv.second);
    }
    return filters;
}

QStringList availableExtensions() {
    QStringList exts;
    exts.reserve(static_cast<int>(imageFormatExtensions.size()));
    for (const auto& kv : imageFormatExtensions) {
        // strip leading dot
        exts.append(kv.second.mid(1));
    }
    return exts;
}

bool isSupportedFormat(const QString& ext) {
    // Normalize: ensure leading dot and lowercase
    QString norm = ext;
    if (!norm.startsWith('.')) {
        norm.prepend('.');
    }
    norm = norm.toLower();

    // Check against our supported extensions
    for (const auto& kv : imageFormatExtensions) {
        if (QString::compare(kv.second, norm, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}