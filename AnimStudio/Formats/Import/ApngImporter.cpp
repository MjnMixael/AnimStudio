#include "ApngImporter.h"
#include "Animation/AnimationData.h"
#include "apng_dis.h"
#include <QFileInfo>
#include <QImage>
#include <QDebug>

std::optional<AnimationData> ApngImporter::importFromFile(const QString& path) {
    try {
        std::vector<Image> frames;
        if (!path.isEmpty()) {
            // Convert QString to a wide?string buffer
            std::wstring wpath = path.toStdWString();
            // load_apng will expect a mutable wchar_t*, so we take &wpath[0]
            int result = load_apng(&wpath[0], frames);
            if (result < 0) {
                throw std::runtime_error("Failed to load APNG file: " + path.toStdString());
            }
        }
        
        // 2) Build AnimationData
        AnimationData out;
        QFileInfo fi(path);
        out.baseName = fi.completeBaseName();
        out.type = std::nullopt;
        out.frameCount = int(frames.size());
        out.frames.reserve(out.frameCount);
        out.animationType = AnimationType::Apng;

        // 1) Fix any zero denominators
        for (auto& f : frames) {
            if (f.delay_den == 0) f.delay_den = 100;
            if (f.delay_num == 0) f.delay_num = 1;
        }

        // 2) Compute common denominator = LCM of all frame delay_den
        int baseDen = frames.empty() ? 100 : frames[0].delay_den;
        auto lcm = [&](int a, int b) {
            return a / std::gcd(a, b) * b;
            };
        for (size_t i = 1; i < frames.size(); ++i) {
            baseDen = lcm(baseDen, int(frames[i].delay_den));
        }

        // 3) fps = baseDen (i.e. tick = 1/baseDen seconds)
        out.fps = baseDen;

        // 4) Expand frames into discrete ticks
        out.frames.clear();
        int globalIndex = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            auto& f = frames[i];
            // how many ticks this frame should occupy
            int repeatCount = int(f.delay_num) * (baseDen / int(f.delay_den));

            // wrap raw RGBA into a QImage once
            QImage img(
                reinterpret_cast<const uchar*>(f.p),
                int(f.w), int(f.h),
                int(f.w * f.bpp),
                QImage::Format_ARGB32
            );
            img = img.rgbSwapped();
            QImage copy = img.copy();

            // duplicate
            for (int t = 0; t < repeatCount; ++t) {
                AnimationFrame af;
                af.image = copy;
                af.index = globalIndex++;              // sequential tick index
                af.filename = QStringLiteral("%1_frame%2")
                    .arg(out.baseName)
                    .arg(int(i));
                out.frames.append(std::move(af));
            }

            // free the loader’s buffers
            f.free();
        }

        // 5) Update frameCount
        out.frameCount = out.frames.size();

        return out;
    }
    catch (...) {
        return std::nullopt;
    }
}