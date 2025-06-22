// AniImporter.cpp
#include "AniImporter.h"
#include "Animation/AnimationData.h"
#include <QFile>
#include <QDataStream>
#include <QImage>
#include <QFileInfo>
#include <optional>

// helper to align each scanline to a 4-byte boundary
static int paddedRowSize(int width) {
    int pad = (4 - (width % 4)) % 4;
    return width + pad;
}

std::optional<AnimationData> AniImporter::importFromFile(const QString& aniPath) {
    QFile f(aniPath);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    // 1) Read fixed-size header (DDN spec) :contentReference[oaicite:0]{index=0}
    quint16 shouldBeZero, version, fps;
    quint8  rgbR, rgbG, rgbB;
    quint16 w, h, nframes;
    quint8  packerCode;
    ds >> shouldBeZero >> version >> fps;
    ds >> rgbR >> rgbG >> rgbB;
    ds >> w >> h >> nframes;
    ds >> packerCode;

    if (shouldBeZero != 0 || version < 2 || nframes == 0)
        return std::nullopt;  // invalid ANI

    // 2) Read 256-entry palette :contentReference[oaicite:1]{index=1}
    QVector<QRgb> qtPalette;
    qtPalette.reserve(256);
    for (int i = 0; i < 256; ++i) {
        quint8 pr, pg, pb;
        ds >> pr >> pg >> pb;
        qtPalette.append(qRgb(pr, pg, pb));
    }

    // 3) Read keyframe info
    quint16 numKeys;
    ds >> numKeys;
    QVector<quint16> keys;
    keys.reserve(numKeys);
    for (int i = 0; i < numKeys; ++i) {
        quint16 twobyte;
        quint32 startcount;
        ds >> twobyte >> startcount;
        keys.append(twobyte);
    }
    // skip endcount
    quint32 endcount;
    ds >> endcount;

    // Prepare buffers
    int rowStride = paddedRowSize(w);
    int bufSize = rowStride * h;
    auto lastFrame = std::make_unique<quint8[]>(bufSize);
    auto curFrame = std::make_unique<quint8[]>(bufSize);

    // initialize lastFrame to the “background” RGB (rarely used)
    quint32 bg = rgbR + (rgbG << 8) + (rgbB << 16);
    memset(lastFrame.get(), bg, bufSize);

    // Build our output structure
    AnimationData out;
    QFileInfo fi(aniPath);
    out.baseName = fi.completeBaseName();
    out.type = "";
    out.frameCount = int(nframes);
    out.fps = int(fps);
    for (quint16 k : keys) {
        // ANI stores keyframe numbers 1..N but our frames are 0..N-1
        int idx = (k > 0) ? int(k) - 1 : 0;
        out.keyframeIndices.append(idx);
    }
    out.frames.reserve(nframes);
    out.animationType = AnimationType::Ani;

    // 4) Decompress each frame :contentReference[oaicite:2]{index=2}
    for (int i = 0; i < nframes; ++i) {
        quint8 flagByte;
        ds >> flagByte;            // often unused

        int  x = 0, y = 0, cur = 0;
        quint8 runValue = 0, runCount = 0;
        quint8* p = curFrame.get();
        quint8* p2 = (i > 0 ? lastFrame.get() : curFrame.get());

        // pixel-by-pixel decode
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                if (runCount > 0) {
                    --runCount;
                } else {
                    ds >> runValue;
                    if (runValue == packerCode) {
                        ds >> runCount;
                        if (runCount < 2) {
                            // literal packerCode
                            runValue = packerCode;
                        } else {
                            ds >> runValue;
                        }
                    }
                }

                // transparent index = 254
                *p = (runValue == 254 ? *p2 : runValue);
                ++p; ++p2; ++cur;
            }
            // skip padding
            int pad = rowStride - w;
            p += pad;
            p2 += pad;
            cur += pad;
        }

        // swap buffers for next iteration
        memcpy(lastFrame.get(), curFrame.get(), bufSize);

        // 5) Convert to QImage and append
        QImage img(w, h, QImage::Format_Indexed8);
        img.setColorTable(qtPalette);
        // copy each scanline
        for (int yy = 0; yy < h; ++yy) {
            memcpy(img.scanLine(yy),
                curFrame.get() + yy * rowStride,
                w);
        }

        AnimationFrame af;
        af.image = img;
        af.index = i;
        af.filename = QStringLiteral("%1_frame%2").arg(out.baseName).arg(i);
        out.frames.append(std::move(af));
    }

    return out;
}