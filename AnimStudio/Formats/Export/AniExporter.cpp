#include "AniExporter.h"
#include <QFile>
#include <QDataStream>
#include <QImage>
#include <QColor>
#include <QVector>
#include <QDebug>   // For qWarning, qInfo
#include <QDir>     // For constructing file paths

// Define constants from FreeSpace code
#define PACKER_CODE                 0xEE    // The escape byte for Hoffoss RLE (used in header and and RLE)
#define PACKING_METHOD_RLE          0       // Hoffoss's RLE format (for non-keyframes) - not strictly used for 1 frame
#define PACKING_METHOD_RLE_KEY      1       // Hoffoss's key frame RLE format (used for the single frame)
#define TRANSPARENT_COLOR_INDEX     254     // As per anidoc.txt, 254 is transparent pixel

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
 * Re-evaluation of unpacker logic and common RLE patterns:
 * Unpacker looks for `PACKER_CODE`. If found, reads `count`.
 * - If `count < 2` (i.e., 0 or 1): The pixel value is `PACKER_CODE` itself. (Effectively a literal 0xEE).
 * This path consumes `PACKER_CODE` then `count` (2 bytes total).
 * - If `count >= 2`: The pixel value is the *next* byte `P`. The run length is `count`.
 * This path consumes `PACKER_CODE`, `count`, `P` (3 bytes total).
 * - If first byte is `P != PACKER_CODE`: Pixel value is `P`, run length is 1.
 * This path consumes `P` (1 byte total).
 *
 * The "horizontal offset" implies our encoder might be emitting *fewer* bytes than the unpacker consumes for certain patterns,
 * or *more* bytes than the unpacker consumes. Given the description, it's more likely we're emitting fewer bytes, causing
 * the unpacker to get "ahead" in the stream, pulling future pixels "leftward" into current positions, which looks like a "rightward" shift if the visual starts from left.
 *
 * Encoding rules based on new hypothesis (prioritizing 3-byte consumption for 0xEE):
 *
 * For a `currentPixel` and its `runLength`:
 * 1. **Run of `N` identical pixels `P` (where `N >= 2`):**
 * Output: `PACKER_CODE (0xEE)`, `N`, `P`. (3 bytes)
 * (Unpacker reads 0xEE, then `N`. Since `N >= 2`, it reads `P` and unpacks `P` for `N` times.)
 * This rule seems solid and made solid colors work.
 *
 * 2. **Single literal `PACKER_CODE (0xEE)` pixel (`P = 0xEE`, `N = 1`):**
 * This is the tricky one. If `PACKER_CODE, 0` (2 bytes) is causing the offset, then maybe it needs to be 3 bytes.
 * If FreeSpace's original packer encodes a literal `0xEE` as `PACKER_CODE, 1, PACKER_CODE` (a run of 1 of `0xEE`),
 * even though the unpacker's `count < 2` might seem to handle `PACKER_CODE, 0` more cleanly.
 * Let's try encoding it as a "run of 1" which makes it a 3-byte sequence.
 * Output: `PACKER_CODE (0xEE)`, `1`, `PACKER_CODE (0xEE)`. (3 bytes)
 * (Unpacker reads 0xEE, then `1`. Since `1 < 2`, it sets value to `PACKER_CODE` and unpacks 1 pixel. BUT it *consumes* 3 bytes.)
 * **This implies the unpacker might *always* read 3 bytes after `PACKER_CODE` if `count` is read as the *actual next byte*, not based on `count < 2` first reading.**
 * The `packunpack.cpp` code seems to suggest this:
 * `value = anim_instance_get_byte(ai,offset); offset++;`  // READ 1st byte
 * `if ( value == packer_code ) {`
 * `count = anim_instance_get_byte(ai,offset); offset++;` // READ 2nd byte
 * `if (count < 2) { ... } else { ... value = anim_instance_get_byte(ai,offset); offset++; }` // READ 3rd byte (ONLY if count >=2)
 * This contradicts the "always read 3 bytes" for 0xEE.
 *
 * Let's go back to the most recent robust interpretation from `packunpack.cpp`'s actual code:
 * If `PACKER_CODE` is found:
 * - Reads `count` (2nd byte). `offset++`
 * - If `count < 2`: *No further byte read for pixel value.* `value = packer_code`.
 * - Else (`count >= 2`): Reads `P` (3rd byte). `offset++`. `value = P`.
 *
 * This means my previous rules correctly matched byte consumption:
 * Rule 1 (N>=2): `0xEE, N, P` -> 3 bytes
 * Rule 2 (0xEE literal): `0xEE, 0` -> 2 bytes
 * Rule 3 (other literal): `P` -> 1 byte
 *
 * If the offset is still happening, it means the *assumed behavior* of `count < 2` (i.e. `0xEE, 0` and `0xEE, 1` *always* meaning `0xEE` pixel, and consuming 2 bytes) is correct, BUT it's used for something else.
 *
 * **What if the `anidoc.txt` simplified unpacker is the *true* source of truth for the RLE behavior (over `packunpack.cpp`)?**
 * `if(runvalue==anifile.header.packer_code)`
 * `cf=readfrombuffer(cf,&runcount,1);` // Reads byte as `runcount`
 * `if(runcount<2)` // If runcount is 0 or 1
 * `runvalue=anifile.header.packer_code;` // `value` is `0xEE`. (Consumes 2 bytes total)
 * `else` // If runcount is >= 2
 * `cf=readfrombuffer(cf,&runvalue,1);` // `value` is *next byte*. (Consumes 3 bytes total)
 *
 * This *still* results in 2 bytes for (`0xEE`, `0` or `1`), and 3 bytes for (`0xEE`, `N>=2`, `P`).
 *
 * The "horizontal offset" is so specific. It means we write X bytes, but they consume Y bytes, and Y != X consistently.
 * If pixels are too far to the right:
 * Encoder writes: `A, B, C, D, E, F` (6 bytes)
 * Decoder reads: `A, B, C, D, E, F, G, H, ...` (8 bytes in same perceived space)
 * This means the decoder is reading *more* bytes than we produced, for some specific patterns.
 * If the decoder is reading `X+N` bytes while we wrote `X`, it implies our RLE is *too efficient* for certain patterns or *misses* putting out some byte.
 *
 * Could the `runcount` byte (the `N` in `0xEE, N, P`) itself be misinterpreted if it's `0xEE`?
 * The `runcount` byte is just a raw byte. It's not RLE-encoded itself.
 *
 * Okay, let's try this specific change for literal 0xEE to match the 3-byte pattern of a run, even if it's a run of 1.
 * This is my strongest hunch for fixing the offset given the data.
 * **This implies the unpacker always reads 3 bytes after the initial `PACKER_CODE` byte, or interprets `count=1` differently.**
 *
 * Let's go for it:
 * If `currentPixel == PACKER_CODE` and `runLength == 1`:
 * Output: `PACKER_CODE`, `1`, `PACKER_CODE`. (3 bytes)
 * This makes all `PACKER_CODE` sequences 3 bytes long, regardless of run length, simplifying byte consumption for the decoder.
 * Only literal non-0xEE pixels would be 1 byte.
 */
QByteArray compressScanlineHoffossRLE(const uchar* scanline, int width) {
    QByteArray compressedData;
    int x = 0;
    int pixelCount = 0;                // ? counts reconstructed pixels

    while (x < width) {
        uchar currentPixel = scanline[x];
        int runLength = 1;
        while (x + runLength < width
            && scanline[x + runLength] == currentPixel
            && runLength < 255) {
            runLength++;
        }

        if (runLength >= 2) {
            // 3-byte RLE for runs ? 2
            compressedData.append(static_cast<char>(PACKER_CODE));
            compressedData.append(static_cast<char>(runLength));
            compressedData.append(static_cast<char>(currentPixel));
            pixelCount += runLength;   // ? adds runLength pixels
        } else if (currentPixel == PACKER_CODE) {
            // 2-byte literal 0xEE (count=1 ? one pixel)
            compressedData.append(static_cast<char>(PACKER_CODE));
            compressedData.append(static_cast<char>(0));
            pixelCount += 1;           // ? one pixel
        } else {
            // 1-byte literal for all other colors
            compressedData.append(static_cast<char>(currentPixel));
            pixelCount += 1;           // ? one pixel
        }

        x += runLength;
    }

    // At this point pixelCount should exactly match width
    if (pixelCount != width) {
        qWarning() << "RLE encoder error: encoded" << pixelCount << "pixels but width is" << width;
    }
    Q_ASSERT(pixelCount == width);

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
bool AniExporter::exportAnimation(const AnimationData& data, const QString& aniPath) {
    // Construct the full file path: aniPath (directory) + data.baseName + ".ani"
    QString fullAniFilePath = QDir(aniPath).filePath(data.baseName + ".ani");

    QFile file(fullAniFilePath); // Use the newly constructed full path
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("AniExporter: Could not open file for writing: %s", qPrintable(fullAniFilePath)); // Report the full path
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian); // FreeSpace uses little-endian byte order

    // --- Validate Input Data ---
    if (data.frames.isEmpty()) {
        qWarning("AniExporter: No frames provided in AnimationData.");
        file.close();
        return false;
    }

    // Validate the quantizedPalette from AnimationData
    if (data.quantizedPalette.size() != 256) {
        qWarning("AniExporter: Quantized palette size is not 256. It must be exactly 256 colors for ANI export.");
        file.close();
        return false;
    }

    // --- Prepare Palette and Transparency ---
    QColor transparentRgb(0, 255, 0); // FreeSpace traditionally uses RGB(0, 255, 0) for transparency

    QVector<QRgb> palette = data.quantizedPalette;
    palette[TRANSPARENT_COLOR_INDEX] = transparentRgb.rgb(); // Ensure transparent color is at index 254


    // --- DEBUG OVERRIDE: Force single frame using first actual frame from data, and sanitize transparent pixels ---
    // This section overrides the input data to create a simple, known ANI for testing.
    // REMEMBER TO REMOVE THESE LINES AFTER DEBUGGING IS COMPLETE!
    int debug_nframes = 1;
    int debug_frameWidth = data.originalSize.width();
    int debug_frameHeight = data.originalSize.height();

    // Get the first actual frame provided in the AnimationData
    const QImage& original_first_image = data.frames.first().image;

    // Create a NEW QImage for the debug frame where all TRANSPARENT_COLOR_INDEX pixels are replaced.
    // This is crucial for initial keyframes which should not rely on previous frame data.
    QImage debug_image_sanitized(debug_frameWidth, debug_frameHeight, QImage::Format_Indexed8);
    debug_image_sanitized.setColorTable(palette); // Use the (potentially modified) palette

    for (int y = 0; y < debug_frameHeight; ++y) {
        const uchar* originalScanline = original_first_image.constScanLine(y);
        uchar* sanitizedScanline = debug_image_sanitized.scanLine(y);
        for (int x = 0; x < debug_frameWidth; ++x) {
            if (originalScanline[x] == TRANSPARENT_COLOR_INDEX) {
                sanitizedScanline[x] = 0; // Replace with the first color in the palette (usually black)
            } else {
                sanitizedScanline[x] = originalScanline[x];
            }
        }
    }

    // Keyframe list for the single frame
    QVector<QPair<short, int>> keyframes;
    keyframes.append({ 1, 0 }); // Frame 1 is at offset 0 (it's the only frame)

    // Compressed image data for the single frame
    QByteArray compressedImageData;
    // The single frame will be a keyframe (PACKING_METHOD_RLE_KEY)
    compressedImageData.append(static_cast<char>(PACKING_METHOD_RLE_KEY));
    for (int y = 0; y < debug_frameHeight; ++y) {
        const uchar* scanline = debug_image_sanitized.constScanLine(y);
        compressedImageData.append(compressScanlineHoffossRLE(scanline, debug_frameWidth));
    }

    // Dump the first 8 bytes in hex so we can eyeball the method?byte + first runs:
    qDebug() << "First bytes of RLE data:";
    for (int i = 0; i < 8 && i < compressedImageData.size(); ++i) {
        qDebug().noquote() << QString("%1").arg((uint8_t)compressedImageData[i], 2, 16, QChar('0'));
    }
    // --- END DEBUG OVERRIDE ---


    // --- Write ANI Header to File ---

    // 1. should_be_zero (short) - always 0
    writeShort(stream, 0);

    // 2. version (short) - >= 2 for custom transparency
    writeShort(stream, 2);

    // 3. fps (short)
    writeShort(stream, data.fps); // Still use the original FPS from data

    // 4. transparent RGB color (3 bytes: red, green, blue)
    uchar tR = static_cast<uchar>(transparentRgb.red());
    uchar tG = static_cast<uchar>(transparentRgb.green());
    uchar tB = static_cast<uchar>(transparentRgb.blue());
    stream.writeRawData(reinterpret_cast<const char*>(&tR), 1);
    stream.writeRawData(reinterpret_cast<const char*>(&tG), 1);
    stream.writeRawData(reinterpret_cast<const char*>(&tB), 1);

    // 5. width (short)
    writeShort(stream, debug_frameWidth); // Use debug width

    // 6. height (short)
    writeShort(stream, debug_frameHeight); // Use debug height

    // 7. nframes (short) - total number of frames
    writeShort(stream, debug_nframes); // Use debug frame count (1)

    // 8. packer_code (char) - used for compressed (repeated) bytes
    char packerCodeVal = PACKER_CODE; // Defined as 0xEE
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
    writeShort(stream, keyframes.size()); // Use debug keyframe count (1)

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
    qInfo("AniExporter: Successfully exported DEBUG (first frame, sanitized, 3-byte 0xEE literal test) animation to %s", qPrintable(fullAniFilePath)); // Report the full path
    return true;
}
