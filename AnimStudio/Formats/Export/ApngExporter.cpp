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

bool ApngExporter::exportAnimation(const AnimationData& data,
    const QString& path,
    QString name)
{
    // 1) build output filename
    QString fullFile = QDir(path).filePath(name + ".png");

    // 2) init the assembler for infinite looping
    apngasm::APNGAsm builder;
    builder.setLoops(0);       // 0 == infinite
    builder.setSkipFirst(false);

    // 3) compute uniform per-frame delay = 1/data.fps
    unsigned num = 1;
    unsigned den = static_cast<unsigned>(data.fps);
    unsigned g = std::gcd(num, den);
    num /= g; den /= g;

    // 4) add each frame’s RGBA buffer
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

    // 5) assemble() writes the file at fullFile and returns true/false :contentReference[oaicite:1]{index=1}
    return builder.assemble(fullFile.toStdString());

    if (m_progressCallback) {
        m_progressCallback(1.0);
    }
}