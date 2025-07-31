// ApngExporter.cpp
#include "ApngExporter.h"
#include <QDir>

// bring in the pared-down APNGASM
#include "apngasm.h"     // declares apngasm::APNGAsm
#include "apngframe.h"   // declares apngasm::rgba

#include <numeric>       // for std::gcd

void ApngExporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

ExportResult ApngExporter::exportAnimation(const AnimationData& data, const QString& path, QString name)
{
    if (m_progressCallback)
        m_progressCallback(0.0f);
    
    if (data.frames.isEmpty()) {
        return ExportResult::fail("No frames to export.");
    }

    // Validate frame format and size consistency
    const QSize expectedSize = data.frames.first().image.size();
    for (int i = 0; i < data.frames.size(); ++i) {
        const QImage& img = data.frames[i].image;
        if (img.size() != expectedSize) {
            return ExportResult::fail(
                QString("Frame %1 has mismatched size (%2x%3 vs %4x%5).")
                .arg(i)
                .arg(img.width())
                .arg(img.height())
                .arg(expectedSize.width())
                .arg(expectedSize.height()));
        }
    }
    
    // build output filename
    QString fullFile = QDir(path).filePath(name + ".png");

    // init the assembler for infinite looping
    apngasm::APNGAsm builder;
    builder.setLoops(0);       // 0 == infinite
    builder.setSkipFirst(false);

    // compute uniform per-frame delay = 1/data.fps
    unsigned num = 1;
    unsigned den = static_cast<unsigned>(data.fps);
    unsigned g = std::gcd(num, den);
    num /= g; den /= g;

    // add each frame’s RGBA buffer
    int count = 0;
    for (const auto& frame : data.frames) {
        QImage img = frame.image;
        if (img.format() != QImage::Format_RGBA8888)
            img = img.convertToFormat(QImage::Format_RGBA8888);

        // fully qualify rgba from the apngasm namespace
        apngasm::rgba* pixels =
            reinterpret_cast<apngasm::rgba*>(img.bits());

        // addFrame(rgba*, width, height, delayNum, delayDen) :contentReference[oaicite:0]{index=0}
        builder.addFrame(
            pixels,
            static_cast<unsigned>(img.width()),
            static_cast<unsigned>(img.height()),
            num,
            den
        );

        // Emit progress mapped into [0.0, 0.05]
        if (m_progressCallback) {
            float frac = float(++count) / float(data.frames.size());
            float pct = frac * 0.05f;
            m_progressCallback(pct);
        }

        count++;
    }

    if (m_progressCallback) {
        builder.setProgressCallback(m_progressCallback);
    }

    // assemble() writes the file at fullFile and returns true/false :contentReference[oaicite:1]{index=1}
    if (!builder.assemble(fullFile.toStdString())) {
        return ExportResult::fail(QString("APNG export failed while writing to '%1'.").arg(QFileInfo(fullFile).fileName()));
    }

    if (m_progressCallback) {
        m_progressCallback(1.0);
    }

    return ExportResult::ok();
}