#include "ApngImporter.h"
#include "Animation/AnimationData.h"
#include "apng_dis.h"
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QDebug>

#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Parse the DATA field of an iTXt chunk (no length/type/crc) for the FSO loop
// keyframe. Mirrors FreeSpaceOpen's parse_fso_keyframe_itxt: keyword must be
// "FSO.Keyframe" (case-insensitive), the value must be uncompressed, and the
// text must be exactly a decimal integer with no trailing bytes.
// iTXt layout: keyword \0 comp_flag comp_method lang \0 translated \0 text
bool parseFsoKeyframeItxt(const QByteArray& data, int& outKeyframe) {
    const char* begin = data.constData();
    const char* end = begin + data.size();

    const char* keywordEnd = static_cast<const char*>(memchr(begin, '\0', end - begin));
    if (keywordEnd == nullptr) return false;

    if (QByteArray(begin, int(keywordEnd - begin)).compare("FSO.Keyframe", Qt::CaseInsensitive) != 0)
        return false;

    const char* p = keywordEnd + 1;
    if (p + 2 > end) return false;
    const unsigned char compressionFlag = static_cast<unsigned char>(*p++);
    ++p;  // compression method, ignored
    if (compressionFlag != 0) return false;  // only uncompressed values supported

    const char* languageEnd = static_cast<const char*>(memchr(p, '\0', end - p));
    if (languageEnd == nullptr) return false;
    p = languageEnd + 1;

    const char* translatedEnd = static_cast<const char*>(memchr(p, '\0', end - p));
    if (translatedEnd == nullptr) return false;
    p = translatedEnd + 1;

    if (p >= end) return false;

    std::string text(p, end);
    char* parseEnd = nullptr;
    long parsed = strtol(text.c_str(), &parseEnd, 10);
    if (parseEnd != text.c_str() + text.size()) return false;

    outKeyframe = static_cast<int>(parsed);
    return true;
}

// Scan a PNG/APNG file for the FSO.Keyframe iTXt chunk. Returns the raw APNG
// frame index the animation should loop back to, or -1 if none is present.
int extractApngKeyframe(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const QByteArray bytes = file.readAll();
    file.close();

    static const unsigned char signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (bytes.size() < 8 || memcmp(bytes.constData(), signature, 8) != 0) return -1;

    qint64 pos = 8;
    while (pos + 8 <= bytes.size()) {
        const unsigned char* u = reinterpret_cast<const unsigned char*>(bytes.constData() + pos);
        const qint64 length = (qint64(u[0]) << 24) | (qint64(u[1]) << 16) | (qint64(u[2]) << 8) | qint64(u[3]);
        const char* type = bytes.constData() + pos + 4;
        const qint64 dataPos = pos + 8;
        if (length < 0 || dataPos + length + 4 > bytes.size()) break;  // truncated/corrupt

        if (memcmp(type, "iTXt", 4) == 0) {
            int keyframe = -1;
            if (parseFsoKeyframeItxt(bytes.mid(int(dataPos), int(length)), keyframe))
                return keyframe;
        }
        if (memcmp(type, "IEND", 4) == 0) break;

        pos = dataPos + length + 4;  // advance past data + crc
    }
    return -1;
}

}  // namespace

void ApngImporter::setProgressCallback(std::function<void(float)> cb) {
    m_progressCallback = std::move(cb);
}

std::optional<AnimationData> ApngImporter::importFromFile(const QString& path) {
    if (m_progressCallback) m_progressCallback(0.0f);
    try {
        std::vector<Image> frames;
        if (!path.isEmpty()) {
            // Convert QString to a wide?string buffer
            std::wstring wpath = path.toStdWString();
            // load_apng will expect a mutable wchar_t*, so we take &wpath[0]
            if (m_progressCallback) m_progressCallback(0.05f);
            int result = load_apng(&wpath[0], frames);
            if (m_progressCallback) m_progressCallback(0.35f);
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

        if (m_progressCallback) m_progressCallback(0.4f);

        // 3) fps = baseDen (i.e. tick = 1/baseDen seconds)
        out.fps = baseDen;

        // 4) Expand frames into discrete ticks
        out.frames.clear();
        // Record the tick index at which each source APNG frame begins, so an
        // FSO.Keyframe (expressed as an APNG frame index) can be mapped into our
        // expanded tick space below.
        std::vector<int> frameStartTicks;
        frameStartTicks.reserve(frames.size());
        int globalIndex = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            auto& f = frames[i];
            frameStartTicks.push_back(globalIndex);
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

            // report 40 -> 100% over the load loop
            if (m_progressCallback) {
                float frac = float(i + 1) / float(frames.size());
                float p = 0.4f + frac * 1.0f;
                m_progressCallback(p);
            }
        }

        // 5) Update frameCount
        out.frameCount = out.frames.size();

        // 6) Apply the FSO loop keyframe, if present. The chunk stores an APNG
        // frame index; map it into our expanded tick space. A keyframe of 0 (or
        // an out-of-range value) just means "loop to the start".
        int apngKeyframe = extractApngKeyframe(path);
        if (apngKeyframe > 0 && apngKeyframe < int(frameStartTicks.size())) {
            out.loopPoint = frameStartTicks[apngKeyframe];
            out.hasLoopPoint = out.loopPoint > 0;
        } else if (apngKeyframe > 0) {
            out.importWarnings.append(
                QStringLiteral("Ignoring invalid APNG loop keyframe %1 (%2 frames).")
                    .arg(apngKeyframe).arg(int(frameStartTicks.size())));
        }

        if (m_progressCallback) m_progressCallback(1.0f);

        return out;
    }
    catch (...) {
        return std::nullopt;
    }
}