#include "Palette.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace Palette {

    static const QList<PaletteFormatInfo> supportedFormats = {
        { PaletteFormat::JascPal, "JASC Palette", {"pal"} },
        { PaletteFormat::RiffPal, "RIFF Palette", {"pal"} }, // fallback
        { PaletteFormat::GimpGpl, "GIMP Palette", {"gpl"} },
        { PaletteFormat::AdobeAct, "Adobe ACT Palette", {"act"} },
    };

    const QList<PaletteFormatInfo>& getSupportedFormats() {
        return supportedFormats;
    }

    static PaletteFormat detectFormat(const QString& fileName) {
        const QString ext = QFileInfo(fileName).suffix().toLower();

        for (const auto& info : supportedFormats) {
            if (info.extensions.contains(ext))
                return info.format;
        }

        return PaletteFormat::Unknown;
    }

    // --- JASC-PAL format ---
    bool loadJascPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "loadJascPal: failed to open" << fileName;
            return false;
        }

        QTextStream in(&f);
        if (in.readLine().trimmed() != QLatin1String("JASC-PAL")) {
            qWarning() << "loadJascPal: not a JASC-PAL file:" << fileName;
            return false;
        }

        in.readLine(); // skip version
        bool ok = false;
        const int count = in.readLine().trimmed().toInt(&ok);
        if (!ok || count <= 0) {
            qWarning() << "loadJascPal: invalid entry count in" << fileName;
            return false;
        }

        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int r, g, b;
            in >> r >> g >> b;
            if (in.status() != QTextStream::Ok) {
                qWarning() << "loadJascPal: malformed color at index" << i;
                return false;
            }
            out.append(qRgba(r, g, b, 255));
        }

        return true;
    }

    // --- GIMP .gpl format ---
    bool loadGimpPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "loadGimpPal: failed to open" << fileName;
            return false;
        }

        QTextStream in(&f);
        out.clear();

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            if (line.startsWith('#') || line.isEmpty() || line.startsWith("GIMP Palette"))
                continue;

            int r, g, b;
            QTextStream ts(&line);
            ts >> r >> g >> b;

            if (ts.status() == QTextStream::Ok)
                out.append(qRgba(r, g, b, 255));
        }

        return !out.isEmpty();
    }

    // --- Adobe .act format ---
    bool loadAdobeAct(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "loadAdobeAct: failed to open" << fileName;
            return false;
        }

        QByteArray data = f.readAll();
        if (data.size() != 768 && data.size() != 772) {
            qWarning() << "loadAdobeAct: invalid size:" << data.size();
            return false;
        }

        int count = data.size() / 3;
        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int r = static_cast<uchar>(data[i * 3 + 0]);
            int g = static_cast<uchar>(data[i * 3 + 1]);
            int b = static_cast<uchar>(data[i * 3 + 2]);
            out.append(qRgba(r, g, b, 255));
        }

        return true;
    }

    // --- Binary .pal format ---
    bool loadRiffPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "loadRiffPal: failed to open" << fileName;
            return false;
        }

        QByteArray data = f.readAll();
        if (!data.startsWith("RIFF") || !data.contains("PAL data")) {
            qWarning() << "loadRiffPal: not a valid RIFF .pal file:" << fileName;
            return false;
        }

        // Look for "data" chunk
        int index = data.indexOf("data");
        if (index < 0 || index + 4 >= data.size()) {
            qWarning() << "loadRiffPal: missing data chunk";
            return false;
        }

        int dataSize = *reinterpret_cast<const quint16*>(data.constData() + index + 4);
        int count = dataSize / 4;
        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int base = index + 6 + i * 4;
            if (base + 3 >= data.size()) break;
            uchar r = data[base];
            uchar g = data[base + 1];
            uchar b = data[base + 2];
            out.append(qRgba(r, g, b, 255));
        }

        return !out.isEmpty();
    }

    // --- Pad to 256 entries ---
    void padTo256(QVector<QRgb>& palette, QRgb filler)
    {
        while (palette.size() < 256)
            palette.append(filler);
        if (palette.size() > 256)
            palette.resize(256);
    }

    // --- Auto-detect format based on extension ---
    bool loadPaletteAuto(const QString& fileName, QVector<QRgb>& out) {
        switch (detectFormat(fileName)) {
        case PaletteFormat::JascPal:
            if (loadJascPal(fileName, out)) return true;
            return loadRiffPal(fileName, out); // fallback if not a real JASC
        case PaletteFormat::RiffPal:
            return loadRiffPal(fileName, out);
        case PaletteFormat::GimpGpl:
            return loadGimpPal(fileName, out);
        case PaletteFormat::AdobeAct:
            return loadAdobeAct(fileName, out);
        case PaletteFormat::Unknown:
        default:
            qWarning() << "loadPaletteAuto: unsupported format:" << fileName;
            return false;
        }
    }


} // namespace Palette
