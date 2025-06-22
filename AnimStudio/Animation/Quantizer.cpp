// quantizer.cpp
#include "Quantizer.h"
#include <QImage>
#include <QByteArray>
#include <QDebug>

std::optional<QuantResult> Quantizer::quantize(const QVector<AnimationFrame>& src) {
    if (src.isEmpty()) {
        qDebug() << "Quantize: no source frames provided";
        return std::nullopt;
    }

    // 1) Initialize attributes and histogram for global palette
    liq_attr* attr = liq_attr_create();
    if (!attr) {
        qDebug() << "Quantize: liq_attr_create failed";
        return std::nullopt;
    }
    liq_set_last_index_transparent(attr, 1); // For FSO's ANI transparency index
    liq_set_max_colors(attr, 256);
    liq_set_quality(attr, 0, 100);

    liq_histogram* hist = liq_histogram_create(attr);
    if (!hist) {
        qDebug() << "Quantize: liq_histogram_create failed";
        liq_attr_destroy(attr);
        return std::nullopt;
    }

    // Prepare to keep liq_image pointers for remapping
    QVector<liq_image*> liqImages;
    liqImages.reserve(src.size());
    int w = src[0].image.width();
    int h = src[0].image.height();

    // 2) Add each frame to histogram
    for (const AnimationFrame& frame : src) {
        QImage img = frame.image;
        if (img.format() != QImage::Format_RGBA8888) {
            img = img.convertToFormat(QImage::Format_RGBA8888);
        }
        if (img.width() != w || img.height() != h) {
            qDebug() << "Quantize: frame sizes differ, cannot global-quantize";
            // cleanup
            for (liq_image* liqimg : liqImages) liq_image_destroy(liqimg);
            liq_histogram_destroy(hist);
            liq_attr_destroy(attr);
            return std::nullopt;
        }
        // create and add
        liq_image* liqimg = liq_image_create_rgba(attr,
            img.bits(), w, h, 0.0f);
        if (!liqimg) {
            qDebug() << "Quantize: liq_image_create_rgba failed";
            for (liq_image* liqimg2 : liqImages) liq_image_destroy(liqimg2);
            liq_histogram_destroy(hist);
            liq_attr_destroy(attr);
            return std::nullopt;
        }
        liq_histogram_add_image(hist, attr, liqimg);
        liqImages.push_back(liqimg);
    }

    // 3) Generate global palette from histogram
    liq_result* resultPal = nullptr;
    liq_set_dithering_level(resultPal, static_cast<float>(0.8));
    if (LIQ_OK != liq_histogram_quantize(hist, attr, &resultPal) || !resultPal) {
        qDebug() << "Quantize: liq_histogram_quantize failed";
        for (liq_image* liqimg : liqImages) liq_image_destroy(liqimg);
        liq_histogram_destroy(hist);
        liq_attr_destroy(attr);
        return std::nullopt;
    }
    // histogram no longer needed
    liq_histogram_destroy(hist);

    // 4) Prepare output structure
    QuantResult out;
    out.frames.reserve(src.size());

    // Extract palette for QImage
    const liq_palette* pal = liq_get_palette(resultPal);
    QVector<QRgb> table;
    table.reserve(pal->count);
    for (int i = 0; i < static_cast<int>(pal->count); ++i) {
        const liq_color& c = pal->entries[i];
        table.append(qRgba(c.r, c.g, c.b, c.a));
    }

    // 5) Remap each frame with the same palette
    size_t bufSize = static_cast<size_t>(w) * static_cast<size_t>(h);
    QByteArray buffer(static_cast<int>(bufSize), 0);
    for (liq_image* liqimg : liqImages) {
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
    }

    // 6) Clean up quantization result & attributes
    liq_result_destroy(resultPal);
    liq_attr_destroy(attr);

    return out;
}
