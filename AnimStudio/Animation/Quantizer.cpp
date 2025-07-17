// quantizer.cpp
#include "Quantizer.h"
#include <QImage>
#include <QByteArray>
#include <QDebug>

// C-callback shim for libimagequant progress callback
static int liqProgressShim(float fraction, void* userInfo) {
    auto* fn = static_cast<ProgressFn*>(userInfo);
    return (*fn)(fraction) ? 1 : 0;
}

Quantizer::Quantizer() = default;

Quantizer& Quantizer::setQualityRange(int min, int max) {
    qualityMin_ = min;
    qualityMax_ = max;
    return *this;
}

Quantizer& Quantizer::setDitheringLevel(float dither) {
    ditheringLevel_ = dither;
    return *this;
}

Quantizer& Quantizer::setMaxColors(int maxColors) {
    maxColors_ = maxColors;
    return *this;
}

Quantizer& Quantizer::setEnforcedTransparency(bool enforceTransparency) {
    enforceTransparency_ = enforceTransparency;
    return *this;
}

Quantizer& Quantizer::setCustomPalette(const QVector<QRgb>& palette) {
    customPalette_ = palette;
    return *this;
}

void Quantizer::reset() {
    cancelRequested_.store(false);
    qualityMin_ = 0;
    qualityMax_ = 100;
    ditheringLevel_ = 0.0f;
    maxColors_ = 256;
    customPalette_.clear();
}

void Quantizer::cancel() {
    cancelRequested_.store(true);
}

std::optional<QuantResult> Quantizer::quantize(const QVector<AnimationFrame>& src, ProgressFn progressCb) {
    if (src.isEmpty()) {
        qDebug() << "Quantize: no source frames provided";
        return std::nullopt;
    }

    running_ = true;

    // Resource handles
    liq_attr* attr = nullptr;
    liq_histogram* hist = nullptr;
    liq_result* resultPal = nullptr;
    QVector<liq_image*> liqImages;
    liqImages.reserve(src.size());
    int w = src[0].image.width();
    int h = src[0].image.height();
    const size_t total = src.size();

    // If caller wants progress, register it on the attr
    std::unique_ptr<ProgressFn> cbPtr = progressCb ? std::make_unique<ProgressFn>(std::move(progressCb)) : nullptr;

    // Cleanup & early-return helper
    auto quit = [&](const char* message) -> std::optional<QuantResult> {
        qDebug() << message;
        if (resultPal)    liq_result_destroy(resultPal);
        for (auto* img : liqImages) liq_image_destroy(img);
        if (hist)         liq_histogram_destroy(hist);
        if (attr)         liq_attr_destroy(attr);
        running_ = false;
        return std::nullopt;
    };

    bool usingCustomPalette = !customPalette_.isEmpty();

    // Initialize attributes
    attr = liq_attr_create();
    if (!attr) return quit("Quantize: liq_attr_create failed");
    liq_set_last_index_transparent(attr, 1); // For FSO's ANI transparency index
    liq_set_quality(attr, qualityMin_, qualityMax_);
    //TODO liq_image_set_background
    //TODO liq_set_output_gamma

    hist = liq_histogram_create(attr);
    if (!hist) return quit("Quantize: liq_histogram_create failed");

    // Create the histogram from palette if set
    if (usingCustomPalette) {
        int numColors = fmin(256, static_cast<int>(customPalette_.size()));
        liq_set_max_colors(attr, numColors);

        // Build an array of histogram entries from your palette
        std::vector<liq_histogram_entry> entries;
        entries.reserve(customPalette_.size());

        int colorCount = 0;
        for (QRgb qc : customPalette_) {
            if (cancelRequested_.load()) return quit("Quantize: cancelled by user");

            if (colorCount >= numColors) {
                qWarning() << "Quantize: custom palette exceeds max colors, truncating";
                break; // Limit to max colors
            }

            liq_color col = {
                static_cast<unsigned char>(qRed(qc)),
                static_cast<unsigned char>(qGreen(qc)),
                static_cast<unsigned char>(qBlue(qc)),
                static_cast<unsigned char>(qAlpha(qc))
            };

            if (colorCount == 255 && enforceTransparency_) {
                col.a = 0;

                // Also adjust our source palette just to be sure
                customPalette_[colorCount] = qRgba(qRed(qc), qGreen(qc), qBlue(qc), 0);
            }
            
            entries.push_back({ col, 1u }); // count=1.0f

            // Feed them into the histogram
            if (LIQ_OK != liq_histogram_add_fixed_color(
                hist,
                col,
                /*gamma=*/0.0
            ))
            {
                qWarning() << "Quantize: liq_histogram_add_colors failed";
            }

            colorCount++;
        }

        
    } else {
        liq_set_max_colors(attr, maxColors_); // Default max colors if no custom palette
    }

    // Process each frame to add to histogram for palette generation
    for (int i = 0; i < total; ++i) {
        if (cancelRequested_.load()) return quit("Quantize: cancelled by user");

        const auto& frame = src[i];
        QImage img = frame.image;
        if (img.format() != QImage::Format_RGBA8888) {
            img = img.convertToFormat(QImage::Format_RGBA8888);
        }
        if (img.width() != w || img.height() != h) {
            return quit("Quantize: frame sizes differ, cannot global-quantize");
        }
        liq_image* liqimg = liq_image_create_rgba(attr,
            img.bits(), w, h, 0.0f);
        if (!liqimg) {
            return quit("Quantize: liq_image_create_rgba failed");
        }
        liq_histogram_add_image(hist, attr, liqimg);
        liqImages.push_back(liqimg); // Still need these for remapping later

        // Track progress adding each frame to the histogram. 0% -> 20%
        if (cbPtr) {
            float pct = 0.0f + float(i + 1) / float(total) * 20.0f;
            (*cbPtr)(pct);
        }
    }

    // Generate global palette from histogram
    if (LIQ_OK != liq_histogram_quantize(hist, attr, &resultPal) || !resultPal) {
        return quit("Quantize: liq_histogram_quantize failed");
    }

    // histogram no longer needed
    liq_histogram_destroy(hist);

    // If the user requested cancellation, we can stop here
    if (cancelRequested_.load()) return quit("Quantize: cancelled by user");

    // Prepare output structure
    QuantResult out;
    out.frames.reserve(src.size());

    // Extract palette for QImage
    const liq_palette* pal = liq_get_palette(resultPal);
    liq_set_dithering_level(resultPal, ditheringLevel_);
    QVector<QRgb> table;
    table.reserve(pal->count);
    for (int i = 0; i < static_cast<int>(pal->count); ++i) {
        if (cancelRequested_.load()) return quit("Quantize: cancelled by user");
        const liq_color& c = pal->entries[i];
        table.append(qRgba(c.r, c.g, c.b, c.a));
    }

    out.palette = table;

    // Remap each frame with the same palette
    size_t bufSize = static_cast<size_t>(w) * static_cast<size_t>(h);
    QByteArray buffer(static_cast<int>(bufSize), 0);
    for (int i = 0; i < liqImages.size(); i++) {
        if (cancelRequested_.load()) return quit("Quantize: cancelled by user");

        liq_image* liqimg = liqImages[i];

        QImage outImg(w, h, QImage::Format_Indexed8);
        outImg.setColorTable(table);

        if (LIQ_OK != liq_write_remapped_image(resultPal, liqimg,
            reinterpret_cast<unsigned char*>(buffer.data()),
            bufSize)) {
            qDebug() << "Quantize: remapping frame failed";
        }
        // copy buffer
        for (int y = 0; y < h; ++y) memcpy(
            outImg.scanLine(y),
            buffer.constData() + y * w, w);

        AnimationFrame qf;
        qf.image = std::move(outImg);
        out.frames.push_back(std::move(qf));

        liq_image_destroy(liqimg);

        // update progress for each frame 20% -> 100%
        if (cbPtr) {
            float pct = 20.0f + float(i + 1) / float(total) * 80.0f;
            (*cbPtr)(pct);
        }
    }

    // Clean up quantization result & attributes
    liq_result_destroy(resultPal);
    liq_attr_destroy(attr);

    // This sucks but now that the quantization is done, we need
    // to make sure the palette order is the same as the input if provided...
    // So we gotta do a little remapping again here. But we can hash map it and
    // be faster about it.
    if (usingCustomPalette) {
        // Create a hash map of the original palette for fast lookup
        QHash<QRgb, int> colorMap;
        for (int i = 0; i < customPalette_.size(); ++i) {
            colorMap[customPalette_[i]] = i;
        }
        // Now create a mapping from the quantized palette back to the original indices
        QVector<int> remap(table.size(), -1);
        for (int i = 0; i < table.size(); ++i) {
            auto it = colorMap.find(table[i]);
            if (it != colorMap.end()) {
                remap[i] = it.value(); // original index
            } else {
                qDebug() << "Quantize: color not found in custom palette, using fallback for index" << i;
                remap[i] = i; // fallback to self if no match found
            }
        }

        // Now remap each frame's image data to use the custom palette
        for (AnimationFrame& frame : out.frames) {
            QImage& img = frame.image;
            if (img.format() != QImage::Format_Indexed8)
                continue;

            frame.image.setColorTable(customPalette_);

            uchar* bits = img.bits();
            int size = img.width() * img.height();

            for (int i = 0; i < size; ++i) {
                bits[i] = remap[bits[i]];
            }
        }

        out.palette = customPalette_;
    }

    // Double check transparency handling
    for (AnimationFrame& frame : out.frames) {
        QImage& img = frame.image;
        if (img.format() != QImage::Format_Indexed8)
            continue;

        uchar* bits = img.bits();
        int size = img.width() * img.height();

        for (int i = 0; i < size; ++i) {
            QRgb originalColor = out.palette[bits[i]];
            if (qAlpha(originalColor) == 0) {
                bits[i] = 255; // Force use of index 255 for transparent pixels
            }
        }
    }

    running_ = false;

    return out;
}
