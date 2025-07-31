// ImageFormats.h
#pragma once

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <unordered_map>

// Define an enum for supported image formats
enum class ImageFormat {
    Png,
    Jpg,
    Bmp,
    Tga,
    Pcx,
    Dds
};

// Define an enum for dds compression types
enum class CompressionFormat {
    BC1,
    BC3,
    BC7,
};

// Provide a hash function so ImageFormat can be used as a key in unordered_map
namespace std {
    template<>
    struct hash<ImageFormat> {
        size_t operator()(ImageFormat fmt) const noexcept {
            return static_cast<size_t>(fmt);
        }
    };
}

// Mapping from ImageFormat to its file extension (including the leading dot)
extern const std::unordered_map<ImageFormat, QString> imageFormatExtensions;

// Mapping from CompressionFormat to its description string
extern const std::unordered_map<CompressionFormat, QString> compressionFormatDescriptions;

// Convert an ImageFormat into the Qt image format string (for QImageWriter)
QByteArray formatToQtString(ImageFormat fmt);

// Get the file extension (including leading dot) for an ImageFormat
QString extensionForFormat(ImageFormat fmt);

// Get the ImageFormat enum value for the given extension (with or without dot)
ImageFormat formatFromExtension(const QString& ext);

// Check if the given extension is a valid supported format
bool isValidExtension(const QString& ext);

// Get the CompressionFormat enum value for the given description string
CompressionFormat getCompressionFormatFromDescription(const QString& desc);

// Check if the given compression format is valid
bool isValidCompressionFormat(const QString& desc);

// Get a list of file patterns (e.g. "*.png") for all supported formats
QStringList availableFilters();

// Get a list of supported extension strings (e.g. "png", "jpg") without the dot
QStringList availableExtensions();

// Get a list of supported compression formats
QStringList availableCompressionFormats();

// Check if a given file extension is supported
bool isSupportedFormat(const QString& ext);