#include "ApngExporter.h"
#include <QFile>
#include <QImage>
#include <QDebug>
#include <QDir>
#include <vector>
#include <QtEndian> // Required for qToBigEndian

// libpng and zlib includes
#include <png.h>
#include <zlib.h>

// APNG Disposal Operations (from APNG Specification)
#define PNG_DISPOSE_OP_NONE         0   // Do not dispose, blend on top
#define PNG_DISPOSE_OP_BACKGROUND   1   // Clear to background color
#define PNG_DISPOSE_OP_PREVIOUS     2   // Restore to previous frame

// APNG Blend Operations (from APNG Specification)
#define PNG_BLEND_OP_SOURCE         0   // Replace
#define PNG_BLEND_OP_OVER           1   // Alpha blend

// Forward declarations for helper functions
// These functions are internal to the ApngExporter implementation.
namespace {
    // Writes a PNG chunk to the file.
    // type: 4-byte chunk type string (e.g., "IHDR")
    // data: Pointer to the chunk data
    // len: Length of the chunk data
    bool writePngChunk(QFile& file, const char* type, const unsigned char* data, size_t len);

    // Helper function to calculate CRC32 for PNG chunks.
    quint32 calculateCrc(const char* type, const unsigned char* data, size_t len);

    // Writes a PNG header (IHDR) chunk.
    bool writeIHDRChunk(QFile& file, int width, int height, int bitDepth, int colorType);

    // Writes an ACtL (Animation Control) chunk for APNG.
    bool writeAcTLChunk(QFile& file, int numFrames, int numPlays);

    // Writes an fcTL (Frame Control) chunk for each frame.
    bool writeFcTLChunk(QFile& file, quint32 sequenceNumber, int width, int height, int xOffset, int yOffset, quint16 delayNum, quint16 delayDen, quint8 disposeOp, quint8 blendOp);

    // Writes an IDAT chunk for image pixel data.
    bool writeIDATChunk(QFile& file, const QByteArray& pixelData);

    // Writes an fdAT (Frame Data) chunk for animated frames.
    bool writeFdATChunk(QFile& file, quint32 sequenceNumber, const QByteArray& pixelData);

    // Writes an IEND chunk.
    bool writeIENDChunk(QFile& file);

    // Compresses raw pixel data using zlib.
    QByteArray compressImageData(const QImage& image);
}

void ApngExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

bool ApngExporter::exportAnimation(const AnimationData& data, const QString& path) {
    // Construct the full file path.
    QString fullApngFilePath = QDir(path).filePath(data.baseName + ".png"); // Use PNG because that's what FSO expects

    QFile file(fullApngFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("ApngExporter: Could not open file for writing: %s", qPrintable(fullApngFilePath));
        return false;
    }

    if (data.frames.isEmpty()) {
        qWarning("ApngExporter: No frames provided in AnimationData.");
        file.close();
        return false;
    }

    // --- PNG Signature ---
    const uchar pngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    file.write(reinterpret_cast<const char*>(pngSignature), 8);

    // --- IHDR Chunk (Image Header) ---
    // This defines the overall image dimensions, color type, etc.
    // For APNG, this IHDR and the first IDAT/IEND block represent the "default" image
    // shown by non-APNG-aware viewers. It's usually the first frame.
    int width = data.originalSize.width();
    int height = data.originalSize.height();
    // Assuming 32-bit RGBA for simplicity, adjust if your AnimationData handles other formats.
    // QImage::Format_ARGB32 (alpha, red, green, blue) or Format_RGBA8888 are common for full color.
    // If your frames are indexed, you'd use PNG_COLOR_TYPE_PALETTE and provide a PLTE chunk.
    int bitDepth = 8; // 8 bits per channel
    int colorType = PNG_COLOR_TYPE_RGB_ALPHA; // RGBA

    if (!writeIHDRChunk(file, width, height, bitDepth, colorType)) {
        file.close();
        return false;
    }

    // --- acTL Chunk (Animation Control) ---
    // This chunk identifies the file as an APNG and specifies animation properties.
    if (!writeAcTLChunk(file, data.frames.size(), 0)) { // 0 for infinite loop
        file.close();
        return false;
    }

    // --- Process Frames ---
    quint32 sequenceNumber = 0; // Sequence number for fcTL and fdAT chunks

    for (int i = 0; i < data.frames.size(); ++i) {
        const AnimationFrame& frame = data.frames[i];
        const QImage& image = frame.image;

        // Ensure image format is suitable for direct pixel access or convert.
        // Convert to ARGB32_Premultiplied for consistent internal representation
        // before converting to RGBA for PNG compression.
        QImage processedImage = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        // Compress image data for the current frame
        QByteArray compressedFrameData = compressImageData(processedImage);
        if (compressedFrameData.isEmpty()) {
            qWarning("ApngExporter: Failed to compress image data for frame %d.", i);
            file.close();
            return false;
        }

        // --- fcTL Chunk (Frame Control) ---
        // For the first frame, it acts as the default image, so its fcTL precedes its IDAT.
        // For subsequent frames, fcTL precedes fdAT.
        quint16 delayNum = 1; // Numerator of frame delay (e.g., 1)
        quint16 delayDen = data.fps; // Denominator of frame delay (e.g., 15 for 15 FPS)
        // Ensure delayDen is not zero to avoid division by zero.
        if (delayDen == 0) delayDen = 1;

        // Dispose operation: PNG_DISPOSE_OP_BACKGROUND (0) means clear to background color,
        // PNG_DISPOSE_OP_NONE (1) means don't dispose, just blend on top.
        // We'll use NONE as a common default unless specific blending is desired.
        quint8 disposeOp = PNG_DISPOSE_OP_NONE;
        // Blend operation: PNG_BLEND_OP_SOURCE (0) means replace, PNG_BLEND_OP_OVER (1) means alpha blend.
        quint8 blendOp = PNG_BLEND_OP_OVER; // Alpha blend is generally desired for animations.


        if (!writeFcTLChunk(file, sequenceNumber++, width, height,
            0, 0, // x/y offset (full frame)
            delayNum, delayDen, disposeOp, blendOp))
        {
            file.close();
            return false;
        }

        // --- Frame Data (IDAT for first frame, fdAT for subsequent frames) ---
        if (i == 0) {
            // First frame: standard IDAT chunk
            if (!writeIDATChunk(file, compressedFrameData)) {
                file.close();
                return false;
            }
        } else {
            // Subsequent frames: fdAT chunk
            if (!writeFdATChunk(file, sequenceNumber++, compressedFrameData)) {
                file.close();
                return false;
            }
        }

        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.frames.size());
            m_progressCallback(progress);
        }
    }

    // --- IEND Chunk (Image End) ---
    if (!writeIENDChunk(file)) {
        file.close();
        return false;
    }

    file.close();
    qInfo("ApngExporter: Successfully exported animation to %s", qPrintable(fullApngFilePath));

    if (m_progressCallback)
        m_progressCallback(1.0f);

    return true;
}


namespace { // Anonymous namespace for helper functions

    // Helper function to calculate CRC32
    quint32 calculateCrc(const char* type, const unsigned char* data, size_t len) {
        quint32 crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data), len);
        return crc;
    }

    bool writePngChunk(QFile& file, const char* type, const unsigned char* data, size_t len) {
        // Write chunk length (4 bytes, network byte order)
        quint32 length = static_cast<quint32>(len);
        // PNG expects big-endian byte order for multi-byte values.
        quint32 length_be = qToBigEndian(length);
        file.write(reinterpret_cast<const char*>(&length_be), 4);

        file.write(type, 4); // Chunk type (4 bytes)
        if (len > 0) {
            file.write(reinterpret_cast<const char*>(data), len); // Chunk data
        }

        // Calculate and write CRC (4 bytes)
        quint32 crc = calculateCrc(type, data, len);
        quint32 crc_be = qToBigEndian(crc); // Convert CRC to big-endian
        file.write(reinterpret_cast<const char*>(&crc_be), 4);
        return true;
    }

    bool writeIHDRChunk(QFile& file, int width, int height, int bitDepth, int colorType) {
        unsigned char ihdrData[13]; // 13 bytes for IHDR data

        // Width (4 bytes, big-endian)
        quint32 width_be = qToBigEndian(static_cast<quint32>(width));
        memcpy(&ihdrData[0], &width_be, 4);

        // Height (4 bytes, big-endian)
        quint32 height_be = qToBigEndian(static_cast<quint32>(height));
        memcpy(&ihdrData[4], &height_be, 4);

        // Bit depth (1 byte)
        ihdrData[8] = static_cast<uchar>(bitDepth);

        // Color type (1 byte)
        ihdrData[9] = static_cast<uchar>(colorType);

        // Compression method (1 byte, 0 for deflate)
        ihdrData[10] = 0;

        // Filter method (1 byte, 0 for adaptive filtering)
        ihdrData[11] = 0;

        // Interlace method (1 byte, 0 for no interlace)
        ihdrData[12] = 0;

        return writePngChunk(file, "IHDR", ihdrData, sizeof(ihdrData));
    }

    bool writeAcTLChunk(QFile& file, int numFrames, int numPlays) {
        unsigned char actlData[8]; // 8 bytes for acTL data

        // Number of frames (4 bytes, big-endian)
        quint32 numFrames_be = qToBigEndian(static_cast<quint32>(numFrames));
        memcpy(&actlData[0], &numFrames_be, 4);

        // Number of plays (4 bytes, big-endian, 0 for infinite)
        quint32 numPlays_be = qToBigEndian(static_cast<quint32>(numPlays));
        memcpy(&actlData[4], &numPlays_be, 4);

        return writePngChunk(file, "acTL", actlData, sizeof(actlData));
    }

    bool writeFcTLChunk(QFile& file, quint32 sequenceNumber, int width, int height, int xOffset, int yOffset, quint16 delayNum, quint16 delayDen, quint8 disposeOp, quint8 blendOp) {
        unsigned char fctlData[26]; // 26 bytes for fcTL data

        // Sequence number (4 bytes, big-endian)
        quint32 sequenceNumber_be = qToBigEndian(sequenceNumber);
        memcpy(&fctlData[0], &sequenceNumber_be, 4);

        // Width (4 bytes, big-endian)
        quint32 width_be = qToBigEndian(static_cast<quint32>(width));
        memcpy(&fctlData[4], &width_be, 4);

        // Height (4 bytes, big-endian)
        quint32 height_be = qToBigEndian(static_cast<quint32>(height));
        memcpy(&fctlData[8], &height_be, 4);

        // X offset (4 bytes, big-endian)
        quint32 xOffset_be = qToBigEndian(static_cast<quint32>(xOffset));
        memcpy(&fctlData[12], &xOffset_be, 4);

        // Y offset (4 bytes, big-endian)
        quint32 yOffset_be = qToBigEndian(static_cast<quint32>(yOffset));
        memcpy(&fctlData[16], &yOffset_be, 4);

        // Delay numerator (2 bytes, big-endian)
        quint16 delayNum_be = qToBigEndian(delayNum);
        memcpy(&fctlData[20], &delayNum_be, 2);

        // Delay denominator (2 bytes, big-endian)
        quint16 delayDen_be = qToBigEndian(delayDen);
        memcpy(&fctlData[22], &delayDen_be, 2);

        // Dispose operation (1 byte)
        fctlData[24] = disposeOp;

        // Blend operation (1 byte)
        fctlData[25] = blendOp;

        return writePngChunk(file, "fcTL", fctlData, sizeof(fctlData));
    }

    bool writeIDATChunk(QFile& file, const QByteArray& pixelData) {
        return writePngChunk(file, "IDAT", reinterpret_cast<const unsigned char*>(pixelData.constData()), pixelData.size());
    }

    bool writeFdATChunk(QFile& file, quint32 sequenceNumber, const QByteArray& pixelData) {
        // fdAT data is sequence number + compressed pixel data
        QByteArray fdatBuffer;
        fdatBuffer.resize(4 + pixelData.size());

        // Sequence number (4 bytes, big-endian)
        quint32 sequenceNumber_be = qToBigEndian(sequenceNumber);
        memcpy(fdatBuffer.data(), &sequenceNumber_be, 4);

        // Copy the compressed pixel data after the sequence number
        memcpy(fdatBuffer.data() + 4, pixelData.constData(), pixelData.size());

        return writePngChunk(file, "fdAT", reinterpret_cast<const unsigned char*>(fdatBuffer.constData()), fdatBuffer.size());
    }

    bool writeIENDChunk(QFile& file) {
        return writePngChunk(file, "IEND", nullptr, 0); // IEND has no data
    }

    QByteArray compressImageData(const QImage& image) {
        QByteArray compressedData;

        int width = image.width();
        int height = image.height();
        int scanlineLength = width * 4; // 4 bytes per pixel (RGBA)

        // Allocate buffer for raw image data including filter bytes
        QByteArray rawImageData(height * (scanlineLength + 1), Qt::Uninitialized);
        char* rawPtr = rawImageData.data();

        for (int y = 0; y < height; ++y) {
            *rawPtr++ = 0; // Filter method byte (None) - always prepend filter byte

            const QRgb* scanline = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < width; ++x) {
                QRgb pixel = scanline[x];
                // QImage::Format_ARGB32_Premultiplied stores 0xAARRGGBB on little-endian systems.
                // PNG_COLOR_TYPE_RGB_ALPHA expects R, G, B, A.
                // We need to extract and write in R, G, B, A order.
                *rawPtr++ = qRed(pixel);
                *rawPtr++ = qGreen(pixel);
                *rawPtr++ = qBlue(pixel);
                *rawPtr++ = qAlpha(pixel);
            }
        }

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        // Initialize deflate, with default compression level.
        // Z_DEFAULT_COMPRESSION (or 9 for maximum, 1 for fastest)
        // 15 is windowBits, default for deflate.
        // Z_DEFAULT_STRATEGY (or Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE)
        if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
            qWarning("ApngExporter: zlib deflateInit failed.");
            return QByteArray();
        }

        strm.avail_in = rawImageData.size();
        strm.next_in = reinterpret_cast<Bytef*>(rawImageData.data());

        // Output buffer size. Start with something reasonable, grow if needed.
        // A common heuristic is 1.001 * input_size + 12 (for zlib overhead).
        size_t bufferSize = rawImageData.size() + (rawImageData.size() / 1000) + 12 + 1;
        compressedData.resize(bufferSize);

        int ret;
        do {
            strm.avail_out = compressedData.size() - strm.total_out;
            strm.next_out = reinterpret_cast<Bytef*>(compressedData.data() + strm.total_out);

            // Resize output buffer if it's full
            if (strm.avail_out == 0) {
                bufferSize *= 2;
                compressedData.resize(bufferSize);
                strm.avail_out = compressedData.size() - strm.total_out;
                strm.next_out = reinterpret_cast<Bytef*>(compressedData.data() + strm.total_out);
            }

            ret = deflate(&strm, Z_FINISH); // Z_FINISH tells zlib to flush all pending output and close.
            // For single block, Z_FINISH is used.
        } while (ret == Z_OK || ret == Z_BUF_ERROR); // Continue until finished or an error occurs

        deflateEnd(&strm);

        if (ret != Z_STREAM_END) {
            qWarning("ApngExporter: zlib compression failed: %d", ret);
            return QByteArray();
        }

        compressedData.resize(strm.total_out); // Resize to actual compressed data size
        return compressedData;
    }

} // end anonymous namespace
