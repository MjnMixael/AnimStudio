#include "AniExporter.h"
#include <QFile>
#include <QDataStream>
#include <QImage>
#include <QColor>
#include <QVector>
#include <QDebug>   // For qWarning, qInfo
#include <QDir>     // For constructing file paths

// Define constants from FreeSpace code
#define PACKER_CODE                 0xEE    // The escape byte for Hoffoss RLE (used in header and RLE)
#define PACKING_METHOD_RLE          0       // The byte at the start of the frame noting a non-keyframe
#define PACKING_METHOD_RLE_KEY      1       // The byte at the start of the frame noting a keyframe
#define FRAME_HOLDOVER_COLOR_INDEX  254     // As per ani documentation, 254 is holdover from last frame

void AniExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

// Helper function to write a short (2 bytes) to the QDataStream in little-endian format.
// FreeSpace ANI files use little-endian byte order.
void writeShort(QDataStream& stream, short value) {
    stream.writeRawData(reinterpret_cast<const char*>(&value), sizeof(short));
}

// Helper function to write an int (4 bytes) to the DataStream in little-endian format.
// FreeSpace ANI files use little-endian byte order.
void writeInt(QDataStream& stream, int value) {
    stream.writeRawData(reinterpret_cast<const char*>(&value), sizeof(int));
}

/**
 * @brief Compresses a single scanline of 8-bit indexed pixel data using Hoffoss's RLE method.
 *
 * This function processes pixels in a scanline and applies run-length encoding
 * based on how the FreeSpace `packunpack.cpp` unpacker interprets the compressed stream.
 *
 * Unpacker rules (from `packunpack.cpp` for Hoffoss RLE, specifically the `else` block handling `PACKING_METHOD_RLE`):
 *
 * `cf` is the current pointer in the compressed data. `value` is the pixel to unpack, `count` is run length.
 *
 * ```cpp
 * if ( *cf != packer_code ) { // Case: Current byte is NOT the PACKER_CODE
 * value = *cf;            // The byte itself is the pixel value
 * cf++;
 * count = 1;              // Unpack 1 pixel
 * } else { // Case: Current byte IS the PACKER_CODE (0xEE)
 * cf++;                   // Consume the PACKER_CODE byte
 * count = *cf;            // Read the next byte as the 'count'
 * cf++;                   // Consume the 'count' byte
 * if (count < 2) {        // Sub-case: count is 0 or 1 -> this means a literal PACKER_CODE (0xEE) pixel
 * value = packer_code; // The pixel to unpack is *always* PACKER_CODE (0xEE) in this sub-case
 * count = 1;          // Unpack 1 pixel
 * } else {                // Sub-case: count is 2 or more -> this means a run of 'count' pixels
 * value = *cf;        // The pixel value is the byte *after* the count
 * cf++;               // Consume the pixel value byte
 * }
 * }
 * // After this block, 'value' holds the pixel, and 'count' holds the number of times to unpack it.
 * // The transparent pixel logic (if value == 254 then *p = *p2) is applied *after* this unpacking.
 * ```
 *
 * Encoding rules derived from this precise unpacker behavior:
 *
 * For a `currentPixel` and its `runLength`:
 *
 * 1. **Run of `N` identical pixels `P` (where `N >= 3`):**
 * - Unpacker needs: `PACKER_CODE (0xEE)`, then `N` (as `count`), then `P` (as `value`).
 * - **Encoder Output:** `PACKER_CODE`, `N` (actual run length), `P` (pixel value).
 * *(Note: The current code writes `runLength - 1`. If this works without distortion, it implies the unpacker might
 * internally add 1. Given your feedback, the original `runLength - 1` is preserved here as it seems to be compatible.)*
 *
 * 2. **Single literal `PACKER_CODE (0xEE)` pixel (`P = 0xEE`, `N = 1`):**
 * - Unpacker needs: `PACKER_CODE (0xEE)`, then `0` (or `1`) as `count`. The unpacker will then set `value = 0xEE`.
 * - **Encoder Output:** `PACKER_CODE`, `0` (using 0 for the count is standard for literal 0xEE).
 * *Crucially, the pixel value `0xEE` is **NOT** emitted here; the unpacker infers it.*
 *
 * 3. **Single literal pixel `P` (where `P != PACKER_CODE`, `N = 1` or `N = 2`):**
 * - Unpacker needs: `P` (as `value`). `count` will implicitly be 1.
 * - **Encoder Output:** `P` (just the pixel value itself, for `N=1` and `N=2` by iterating).
 *
 * The `transparentIndex` (254) is simply a pixel value during compression. Its special meaning
 * ("use pixel from last frame") is handled solely by the FreeSpace decoder *after* unpacking.
 *
 * @param scanline Pointer to the current scanline's 8-bit indexed pixel data.
 * @param width The width of the image (number of pixels in the scanline).
 * @return A QByteArray containing the compressed data for the scanline.
 */
QByteArray compressScanlineHoffossRLE(const uchar* scanline, int width) {
    QByteArray compressedData;
    int x = 0;
    int pixelCount = 0; // Counts reconstructed pixels for sanity check

    while (x < width) {
        uchar currentPixel = scanline[x];
        int runLength = 1;
        while (x + runLength < width
            && scanline[x + runLength] == currentPixel
            && runLength < 255) { // Max run length for a single byte
            runLength++;
        }

        if (runLength > 2) {
            // 3-byte RLE for runs of 3 or more identical pixels
            // Format: PACKER_CODE, (runLength - 1), pixel_value
            // When read, the PACKER_CODE notes a rune then the length of the run is expected as
            // "repeat this many times not including current pixel" and then the pixel
            compressedData.append(static_cast<char>(PACKER_CODE));
            compressedData.append(static_cast<char>(runLength - 1)); // Store N-1, as unpacker increments runcount
            compressedData.append(static_cast<char>(currentPixel));
            pixelCount += runLength;
        } else {
            // For runs of 1 or 2 pixels, or single literal 0xEE pixels
            for (int i = 0; i < runLength; i++) {
                if (currentPixel == PACKER_CODE) {
                    // Single 0xEE literal (for a pixel that IS our packer code)
                    // Format: PACKER_CODE, 0
                    // The unpacker for `count < 2` infers the pixel is PACKER_CODE (0xEE) itself.
                    compressedData.append(static_cast<char>(PACKER_CODE));
                    compressedData.append(static_cast<char>(0)); // Special count 0 for literal 0xEE
                } else {
                    // 1-byte literal for all other colors (non-PACKER_CODE pixels)
                    // Format: pixel_value
                    compressedData.append(static_cast<char>(currentPixel));
                }
            }
            pixelCount += runLength; // Add the number of pixels processed in this literal loop
        }

        x += runLength;
    }

    // Sanity check: The total number of pixels encoded should exactly match the scanline width
    if (pixelCount != width) {
        qWarning() << "RLE encoder error: encoded" << pixelCount << "pixels but width is" << width;
    }
    Q_ASSERT(pixelCount == width); // Assert for debugging in development builds

    return compressedData;
}

/**
 * @brief Exports animation data to a FreeSpace-compatible ANI file.
 *
 * This method takes `AnimationData` containing a series of 8-bit indexed images
 * and compresses them into the ANI format, including header information,
 * palette, keyframes, and RLE compressed pixel data.
 *
 * @param data The AnimationData struct containing frames, FPS, dimensions, and keyframe info.
 * Assumes `data.frames[i].image` are `QImage::Format_Indexed8` and `data.quantizedPalette` is valid.
 * @param aniPath The full path (including filename and .ani extension) where the file should be saved.
 * @return True if the export was successful, false otherwise.
 */
ExportResult AniExporter::exportAnimation(const AnimationData& data, const QString& aniPath, QString name) {

    if (m_progressCallback)
        m_progressCallback(0.0f);

    // Construct the full file path: aniPath (directory) + data.baseName + ".ani"
    QString fullAniFilePath = QDir(aniPath).filePath(name + ".ani");

    QFile file(fullAniFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return ExportResult::fail(QString("Could not open file for writing: %1").arg(fullAniFilePath));
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian); // FreeSpace uses little-endian byte order

    // --- Validate Input Data ---
    if (data.quantizedFrames.isEmpty()) {
        file.close();
        return ExportResult::fail(QString("No quantized frames found. You must reduce colors before exporting as ANI."));
    }

    // All frames must have the same size and be 8-bit indexed
    const QImage& firstImage = data.quantizedFrames.first().image;
    if (firstImage.format() != QImage::Format_Indexed8) {
        file.close();
        return ExportResult::fail(QString("Input must be Format_Indexed8. Please ensure images are correctly quantized."));
    }

    // Validate the quantizedPalette from AnimationData
    if (data.quantizedPalette.size() != 256) {
        file.close();
        return ExportResult::fail(QString("Quantized palette size is not 256. It must be exactly 256 colors for ANI export."));
    }

    // --- Prepare Palette and Transparency ---
    QColor transparentRgb(0, 255, 0); // FreeSpace traditionally uses RGB(0, 255, 0) for transparency

    QVector<QRgb> palette = data.quantizedPalette;

    // Placeholder for keyframe data: stores pairs of (frame_num, offset_in_compressed_data)
    QVector<QPair<short, int>> keyframes;

    // Calculate dimensions from the provided AnimationData
    int frameWidth = data.originalSize.width();
    int frameHeight = data.originalSize.height();

    // Prepare a QByteArray to store all compressed image data
    QByteArray compressedImageData;
    // `lastFramePixels` stores the raw pixel data of the previously encoded (and logically 'decoded') frame.
    // This is crucial for delta compression of subsequent frames.
    QByteArray lastFramePixels;

    // If we have transparency then make it bright green
    if (qAlpha(palette[255]) == 0) {
        palette[255] = transparentRgb.rgb();
    }

    // Convert our loop point to the proper keyframe for ANI
    QVector<int> keyframeIndices = data.keyframeIndices;
    if (data.hasLoopPoint && data.loopPoint > 0) {
        keyframeIndices.clear();
        keyframeIndices.append(data.loopPoint - 1);
        keyframeIndices.append(0); // Always include the first frame as a keyframe
    }

    // --- Iterate through ALL frames to compress and build keyframe info ---
    for (int i = 0; i < data.quantizedFrames.size(); ++i) {
        const AnimationFrame& currentAnimationFrame = data.quantizedFrames[i];
        const QImage& currentImage = currentAnimationFrame.image;

        // Basic validation for current frame dimensions
        if (currentImage.width() != frameWidth || currentImage.height() != frameHeight) {
            file.close();
            return ExportResult::fail(QString("Frame %1 has incorrect dimensions (%2x%3 instead of %4x%5).")
                .arg(i)
                .arg(currentImage.width())
                .arg(currentImage.height())
                .arg(frameWidth)
                .arg(frameHeight));
        }

        // Determine if the current frame should be a keyframe.
        // The very first frame (index 0) is always a keyframe.
        // Other keyframes are determined by the `keyframeIndices`.
        bool isKeyFrame = (i == 0) || keyframeIndices.contains(i);

        // If it's a keyframe, record its position (offset) in the compressed data.
        // Frame numbers in ANI are 1-based.
        if (isKeyFrame) {
            keyframes.append({ static_cast<short>(i + 1), compressedImageData.size() });
        }

        // This will hold the compressed data for the current frame
        QByteArray frameCompressedData;

        // The first byte of each frame's compressed data indicates the packing method.
        // For keyframes, use PACKING_METHOD_RLE_KEY (1).
        // For non-keyframes, use PACKING_METHOD_RLE (0).
        if (isKeyFrame) {
            frameCompressedData.append(static_cast<char>(PACKING_METHOD_RLE_KEY));
        } else {
            frameCompressedData.append(static_cast<char>(PACKING_METHOD_RLE));
        }

        // Create a modifiable copy of the current frame's pixel data for processing
        QByteArray currentFrameModifiablePixels(
            reinterpret_cast<const char*>(currentImage.constBits()),
            currentImage.sizeInBytes()
        );
        uchar* modifiableImagePixels = reinterpret_cast<uchar*>(currentFrameModifiablePixels.data());


        // Iterate through each scanline (row) of the image for compression
        int stride = currentImage.bytesPerLine();
        for (int y = 0; y < frameHeight; ++y) {
            uchar* currentScanline = modifiableImagePixels + (y * stride);

            if (isKeyFrame) {
                // For keyframes, we sanitize transparent pixels by replacing FRAME_HOLDOVER_COLOR_INDEX (254) with 0.
                // This ensures a "clean" base frame for the decoder, as per ANIVIEW32's behavior.
                for (int x = 0; x < frameWidth; ++x) {
                    if (currentScanline[x] == FRAME_HOLDOVER_COLOR_INDEX) {
                        currentScanline[x] = 0; // Replace with the first color in the palette (usually black)
                    }
                }
                frameCompressedData.append(compressScanlineHoffossRLE(currentScanline, frameWidth));
            } else {
                // For non-keyframes, apply delta compression:
                // If a pixel is identical to the corresponding pixel in the last frame,
                // replace it with FRAME_HOLDOVER_COLOR_INDEX (254).
                // This will create runs of 254s, which RLE will compress efficiently.
                if (!lastFramePixels.isEmpty() && lastFramePixels.size() == currentImage.sizeInBytes()) {
                    const uchar* lastScanline = reinterpret_cast<const uchar*>(lastFramePixels.constData()) + (y * stride);

                    for (int x = 0; x < frameWidth; ++x) {
                        if (currentScanline[x] == lastScanline[x]) {
                            currentScanline[x] = FRAME_HOLDOVER_COLOR_INDEX;
                        }
                    }
                    frameCompressedData.append(compressScanlineHoffossRLE(currentScanline, frameWidth));
                } else {
                    // Fallback: If lastFramePixels is not valid (e.g., first frame is not a keyframe, though it should be),
                    // compress the frame without delta optimization.
                    qWarning() << "AniExporter: lastFramePixels not available for non-keyframe " << i << ", scanline " << y << ". Compressing as full frame.";
                    frameCompressedData.append(compressScanlineHoffossRLE(currentImage.constScanLine(y), frameWidth));
                }
            }
        }

        // Append the compressed data for the current frame to the total compressed data buffer
        compressedImageData.append(frameCompressedData);

        // Update `lastFramePixels` with the *original* (or fully reconstructed) pixel data of the current frame.
        // This is crucial because the *next* frame's delta compression will compare against the actual image data
        // of *this* frame, not the delta-compressed version.
        lastFramePixels.clear();
        lastFramePixels.append(reinterpret_cast<const char*>(currentImage.bits()), currentImage.sizeInBytes());

        // Emit progress (frame-wise granularity)
        if (m_progressCallback) {
            float progress = float(i + 1) / float(data.quantizedFrames.size());
            m_progressCallback(progress);
        }
    }

    // --- Write ANI Header to File ---

    // 1. should_be_zero (short) - always 0
    writeShort(stream, 0);

    // 2. version (short) - >= 2 for custom transparency
    writeShort(stream, 2);

    // 3. fps (short)
    writeShort(stream, data.fps);

    // 4. transparent RGB color (3 bytes: red, green, blue)
    uchar tR = static_cast<uchar>(transparentRgb.red());
    uchar tG = static_cast<uchar>(transparentRgb.green());
    uchar tB = static_cast<uchar>(transparentRgb.blue());
    stream.writeRawData(reinterpret_cast<const char*>(&tR), 1);
    stream.writeRawData(reinterpret_cast<const char*>(&tG), 1);
    stream.writeRawData(reinterpret_cast<const char*>(&tB), 1);

    // 5. width (short)
    writeShort(stream, frameWidth);

    // 6. height (short)
    writeShort(stream, frameHeight);

    // 7. nframes (short) - total number of frames
    writeShort(stream, data.frameCount); // Use actual frame count

    // 8. packer_code (char) - used for compressed (repeated) bytes
    char packerCodeVal = static_cast<char>(PACKER_CODE);
    stream.writeRawData(&packerCodeVal, 1);

    // 9. palette[256] (256 * 3 bytes) - RGB triplets
    for (const QRgb& color : palette) {
        QColor qColor(color);
        uchar r = static_cast<uchar>(qColor.red());
        uchar g = static_cast<uchar>(qColor.green());
        uchar b = static_cast<uchar>(qColor.blue());
        stream.writeRawData(reinterpret_cast<const char*>(&r), 1);
        stream.writeRawData(reinterpret_cast<const char*>(&g), 1);
        stream.writeRawData(reinterpret_cast<const char*>(&b), 1);
    }

    // 10. num_keys (short)
    writeShort(stream, keyframes.size());

    // 11. keyframe definitions (variable number)
    // Each keyframe has: short twobyte (frame_num), int startcount (offset)
    for (const auto& keyframe : keyframes) {
        writeShort(stream, keyframe.first);  // frame_num (1-based)
        writeInt(stream, keyframe.second);   // offset in the compressed data block
    }

    // 12. Compressed data length (int)
    writeInt(stream, compressedImageData.size());

    // --- Write Compressed Image Data to File ---
    stream.writeRawData(compressedImageData.constData(), compressedImageData.size());

    file.close();

    if (m_progressCallback)
        m_progressCallback(1.0f);

    return ExportResult::ok();
}